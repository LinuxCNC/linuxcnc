/*
 * Copyright: 2013-4
 * Author: Jeff Epler <jepler@unpythonic.net>
 * Author: Michael Haberler <git@mah.priv.at>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */
#include <string.h>
#include "rs274ngc_interp.hh"

setup_struct::setup_struct() :
    AA_axis_offset(0.0),
    AA_current(0.0),
    AA_origin_offset(0.0),
    BB_axis_offset(0.0),
    BB_current(0.0),
    BB_origin_offset(0.0),
    CC_axis_offset(0.0),
    CC_current(0.0),
    CC_origin_offset(0.0),

    u_axis_offset(0.0),
    u_current(0.0),
    u_origin_offset(0.0),

    v_axis_offset(0.0),
    v_current(0.0),
    v_origin_offset(0.0),

    w_axis_offset(0.0),
    w_current(0.0),
    w_origin_offset(0.0),

    arc_not_allowed(0),

    axis_offset_x(0.0),
    axis_offset_y(0.0),
    axis_offset_z(0.0),

    /* blocks(), */
    remap_level(0),
    control_mode(0),
    current_pocket(0),

    current_x (0.0),
    current_y (0.0),
    current_z (0.0),
    cutter_comp_radius (0.0),
    cutter_comp_orientation(0),
    cutter_comp_side(0),
    cycle_cc (0.0),
    cycle_i (0.0),
    cycle_j (0.0),
    cycle_k (0.0),
    cycle_l(0),
    cycle_p (0.0),
    cycle_q (0.0),
    cycle_r (0.0),
    cycle_il (0.0),
    cycle_il_flag(0),
    distance_mode(MODE_ABSOLUTE),

    ijk_distance_mode(MODE_ABSOLUTE),
    feed_mode(0),
    feed_override(0),
    feed_rate (0.0),
    file_pointer(NULL),
    flood(0),
    length_units(0),
    line_length(0),
    mist(0),
    motion_mode(0),
    origin_index(0),
    origin_offset_x (0.0),
    origin_offset_y (0.0),
    origin_offset_z (0.0),
    rotation_xy (0.0),

    parameter_occurrence(0),
    named_parameter_occurrence(0),
    percent_flag(0),
    plane(0),
    probe_flag(0),
    input_flag(0),
    toolchange_flag(0),
    input_index(0),
    input_digital(0),
    cutter_comp_firstmove(0),
    program_x (0.0),
    program_y (0.0),
    program_z (0.0),
    retract_mode(R_PLANE),
    random_toolchanger(0),
    selected_pocket(0),
    selected_tool(0),
    sequence_number(0),
    speed (0.0),
    spindle_mode(CONSTANT_RPM),
    speed_feed_mode(0),
    speed_override(0),
    spindle_turning(0),
    stack_index(0),
    pockets_max(0),
    traverse_rate (0.0),
    orient_offset (0.0),

    defining_sub(0),
    sub_name(NULL),
    doing_continue(0),
    doing_break(0),
    executed_if(0),

    skipping_o(NULL),
    skipping_to_sub(NULL),
    skipping_start(0),
    test_value (0.0),
    return_value (0.0),
    value_returned(0),
    call_level(0),
    call_state(0),
    adaptive_feed(0),
    feed_hold(0),
    loggingLevel(0),
    debugmask(0),
    use_lazy_close(0),
    lazy_closing(0),
    tool_change_at_g30(0),
    tool_change_quill_up(0),
    tool_change_with_spindle_on(0),
    a_axis_wrapped(0),
    b_axis_wrapped(0),
    c_axis_wrapped(0),

    a_indexer(0),
    b_indexer(0),
    c_indexer(0),

    lathe_diameter_mode(0),
    mdi_interrupt(0),
    feature_set(0),
    disable_g92_persistence(0),
    on_abort_command(NULL),
    init_once(0)
{
    memset(active_g_codes, 0, sizeof(active_g_codes));
    memset(active_m_codes, 0, sizeof(active_m_codes));
    memset(active_settings, 0, sizeof(active_settings));
    memset(blocktext, 0, sizeof(blocktext));
    memset(filename, 0, sizeof(filename));
    memset(linetext, 0, sizeof(linetext));
    memset(parameters, 0, sizeof(parameters));
    memset(named_parameters, 0, sizeof(named_parameters));
    memset(named_parameter_values, 0, sizeof(named_parameter_values));
    memset(stack, 0, sizeof(stack));
    memset(subroutines, 0, sizeof(subroutines));
    memset(log_file, 0, sizeof(log_file));
    memset(program_prefix, 0, sizeof(program_prefix));
    memset(wizard_root, 0, sizeof(wizard_root));
    memset(tool_table, 0, sizeof(tool_table));
    ZERO_EMC_POSE(tool_offset);

    // not sure about sub_context:
    // a plain array of a struct with non-POD data?

    // these are non-POD, I assume these are default-constructed:
    // offset_map
    // pythis
    // g_remapped,m_remapped
    // remaps
}
