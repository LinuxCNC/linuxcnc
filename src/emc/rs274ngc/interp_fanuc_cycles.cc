/********************************************************************
* Description: interp_fanuc_cycles.cc
*
*   Fanuc lathe G-code system A canned cycles and G50:
*     G90  outer/inner diameter turning cycle (straight or taper)
*     G92  threading cycle, one pass per block, repeat with new X
*     G94  end face turning cycle
*     G50  spindle speed clamp (S) and coordinate system setting (X/Z)
*
*   Enabled with [RS274NGC]FANUC_LATHE.  In this mode G90/G94 with axis
*   words live in the motion group (promoted in enhance_block), G92 is
*   the threading cycle, G98/G99 select the feed mode and G50 takes the
*   role of the coordinate system setting.
*
* License: GPL Version 2
* System: Linux
*
********************************************************************/
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <math.h>
#include "rs274ngc.hh"
#include "rs274ngc_return.hh"
#include "rs274ngc_interp.hh"
#include "interp_internal.hh"
#include "interp_queue.hh"

namespace {

/* Emit a straight move and keep the interpreter position in step. */
void fanuc_move(setup_pointer settings, int line, bool rapid,
                double x, double y, double z)
{
    if (rapid)
        STRAIGHT_TRAVERSE(line, x, y, z,
                          settings->AA_current, settings->BB_current,
                          settings->CC_current, settings->u_current,
                          settings->v_current, settings->w_current);
    else
        STRAIGHT_FEED(line, x, y, z,
                      settings->AA_current, settings->BB_current,
                      settings->CC_current, settings->u_current,
                      settings->v_current, settings->w_current);
    settings->current_x = x;
    settings->current_y = y;
    settings->current_z = z;
}

} // namespace

/*! convert_fanuc_g50

Returned Value: int
   If convert_axis_offsets returns an error code, this returns that code.
   Otherwise, this returns INTERP_OK.

Side effects:
   G50 S- writes the spindle speed clamp used by constant surface speed
   control (the field behind G96 D-).  G50 with coordinate words sets the
   current coordinate system, exactly like the G92 offset.

Called by: convert_modal_0

*/

int Interp::convert_fanuc_g50(block_pointer block, setup_pointer settings)
{
    if (block->s_flag) {
        for (int s = 0; s < settings->num_spindles; s++) {
            settings->css_maximum[s] = fabs(block->s_number);
            if (settings->spindle_mode[s] == SPINDLE_MODE::CONSTANT_SURFACE) {
                enqueue_SET_SPINDLE_MODE(s, fabs(block->s_number));
            }
        }
    }
    if (block->x_flag || block->y_flag || block->z_flag ||
        block->a_flag || block->b_flag || block->c_flag ||
        block->u_flag || block->v_flag || block->w_flag) {
        CHP(convert_axis_offsets(G_92, block, settings));
    } else {
        CHKS((!block->s_flag),
             _("G50 requires S (spindle speed clamp) or coordinate words"));
    }
    return INTERP_OK;
}

/*! convert_fanuc_cycle

Returned Value: int
   If any of the following errors occur, this returns the error shown.
   Otherwise, this returns INTERP_OK.
   1. Cutter radius compensation is on:
      "Cannot use Fanuc lathe cycles with cutter radius compensation on"
   2. Inverse time feed mode is on:
      "Cannot use Fanuc lathe cycles in inverse time feed mode"
   3. G92 without an F word and no previous feed:
      "F word missing or zero with G92 threading cycle"
   4. G92 with the spindle stopped:
      "Spindle not turning in G92"

Side effects:
   Executes the cycle as a sequence of rapid and feed moves and leaves
   the tool at the cycle start point, so that an axis-only block repeats
   the cycle (Fanuc modal cycles).  The programmed X/Z words are kept as
   the cycle modal values in settings->fanuc_cycle_x/z, independently of
   the position the tool returns to.

Called by: convert_motion

The cycles always work in the XZ plane.  U/W are incremental X/Z and
have already been folded into X/Z by convert_motion; U is a diameter
when G7 lathe diameter mode is on.

G90 (turning):          G94 (facing):
  1 rapid to X first      1 rapid to Z
  2 feed to Z             2 feed to X
  3 feed back to X start  3 feed back to Z start
  4 rapid to Z start      4 rapid to X start

G92 (threading): one spindle synchronized pass per block
  1 rapid in X to the thread depth
  2 synchronized feed to Z
  3 rapid retract in X
  4 rapid return to the cycle start point

With G90 and R the cut is a taper: it starts at X - 2R and ends at X,
so R is the signed radius difference between the start and the end of
the cut, as on the control.  G94 has no taper word.

G92 on the 0i-TF is straight threading only (format X Z F Q); the
optional Q thread chamfering word is not supported yet.

*/

int Interp::convert_fanuc_cycle(int motion, block_pointer block,
                                setup_pointer settings)
{
    CHKS((settings->cutter_comp_side != CUTTER_COMP::OFF),
         _("Cannot use Fanuc lathe cycles with cutter radius compensation on"));
    CHKS((settings->feed_mode == FEED_MODE::INVERSE_TIME),
         _("Cannot use Fanuc lathe cycles in inverse time feed mode"));

    const double start_x = settings->current_x;
    const double start_y = settings->current_y;
    const double start_z = settings->current_z;

    double target_x, target_z;
    if (block->x_flag)
        target_x = block->x_number;
    else if (settings->fanuc_cycle_x_set)
        target_x = settings->fanuc_cycle_x;
    else
        target_x = start_x;

    if (block->z_flag)
        target_z = block->z_number;
    else if (settings->fanuc_cycle_z_set)
        target_z = settings->fanuc_cycle_z;
    else
        target_z = start_z;

    if (block->x_flag) {
        settings->fanuc_cycle_x = target_x;
        settings->fanuc_cycle_x_set = true;
    }
    if (block->z_flag) {
        settings->fanuc_cycle_z = target_z;
        settings->fanuc_cycle_z_set = true;
    }

    settings->motion_mode = motion;

    if (motion == G_92) {
        CHKS((block->r_flag),
             _("Taper threading is not supported with G92; use G76"));
        double pitch = block->f_flag ? block->f_number : settings->feed_rate;
        CHKS((pitch <= 0.0),
             _("F word missing or zero with G92 threading cycle"));
        CHKS(((settings->spindle_turning[settings->active_spindle] != CANON_CLOCKWISE) &&
              (settings->spindle_turning[settings->active_spindle] != CANON_COUNTERCLOCKWISE)),
             _("Spindle not turning in G92"));

        double delta[9] = {0};
        delta[0] = target_x - start_x;
        delta[2] = target_z - start_z;
        double min_radius = std::min(fabs(start_x), fabs(target_x));
        CHP(check_spindle_sync_feed(settings, pitch, "G92", delta, min_radius));

        fanuc_move(settings, block->line_number, true, target_x, start_y, start_z);
        DISABLE_FEED_OVERRIDE();
        DISABLE_SPEED_OVERRIDE(settings->active_spindle);
        START_SPEED_FEED_SYNCH(settings->active_spindle, pitch, 0);
        STRAIGHT_FEED(block->line_number, target_x, start_y, target_z,
                      settings->AA_current, settings->BB_current,
                      settings->CC_current, settings->u_current,
                      settings->v_current, settings->w_current);
        STOP_SPEED_FEED_SYNCH();
        ENABLE_FEED_OVERRIDE();
        if (settings->speed_override[settings->active_spindle])
            ENABLE_SPEED_OVERRIDE(settings->active_spindle);
        else
            DISABLE_SPEED_OVERRIDE(settings->active_spindle);
        settings->current_x = target_x;
        settings->current_z = target_z;
        fanuc_move(settings, block->line_number, true, start_x, start_y, target_z);
        fanuc_move(settings, block->line_number, true, start_x, start_y, start_z);
        return INTERP_OK;
    }

    double feed = block->f_flag ? block->f_number : settings->feed_rate;
    CHKS((feed <= 0.0), _("Feed rate missing or zero with %s"),
         (motion == G_90) ? "G90" : "G94");

    if (motion == G_90) {
        double r = block->r_flag ? block->r_number :
            (settings->fanuc_cycle_r_set ? settings->fanuc_cycle_r : 0.0);
        if (block->r_flag) {
            settings->fanuc_cycle_r = r;
            settings->fanuc_cycle_r_set = true;
        }
        double first_x = target_x - 2.0 * r;
        fanuc_move(settings, block->line_number, true, first_x, start_y, start_z);
        fanuc_move(settings, block->line_number, false, target_x, start_y, target_z);
        fanuc_move(settings, block->line_number, false, start_x, start_y, target_z);
        fanuc_move(settings, block->line_number, true, start_x, start_y, start_z);
    } else {
        fanuc_move(settings, block->line_number, true, start_x, start_y, target_z);
        fanuc_move(settings, block->line_number, false, target_x, start_y, target_z);
        fanuc_move(settings, block->line_number, false, target_x, start_y, start_z);
        fanuc_move(settings, block->line_number, true, start_x, start_y, start_z);
    }
    return INTERP_OK;
}
