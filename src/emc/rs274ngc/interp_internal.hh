/********************************************************************
* Description: interp_internal.hh
*
*   Derived from a work by Thomas Kramer
*
* Author:
* License: GPL Version 2
* System: Linux
*    
* Copyright (c) 2004 All rights reserved.
*
********************************************************************/
#ifndef INTERP_INTERNAL_HH
#define INTERP_INTERNAL_HH

#include "config.h"
#include <limits.h>
#include <stdio.h>
#include "canon.hh"
#include "emcpos.h"
#include "libintl.h"
#define _(s) gettext(s)

/**********************/
/*   COMPILER MACROS  */
/**********************/

#ifndef R2D
#define R2D(r) ((r)*180.0/M_PI)
#endif
#ifndef D2R
#define D2R(r) ((r)*M_PI/180.0)
#endif
#ifndef SQ
#define SQ(a) ((a)*(a))
#endif

#define MAX(x, y)        ((x) > (y) ? (x) : (y))

#define round_to_int(x) ((int) ((x) < 0.0 ? ((x) - 0.5) : ((x) + 0.5)))
/* how far above hole bottom for rapid return, in inches */
#define G83_RAPID_DELTA 0.010


/* numerical constants */
#define TOLERANCE_INCH 0.0005
#define TOLERANCE_MM 0.005
/* angle threshold for concavity for cutter compensation, in radians */
#define TOLERANCE_CONCAVE_CORNER 0.05  
#define TOLERANCE_EQUAL 0.0001 /* two numbers compare EQ if the
				  difference is less than this */

#define TINY 1e-12              /* for arc_data_r */

// max number of m codes on one line
#define MAX_EMS  4

// English - Metric conversion (long number keeps error buildup down)
#define MM_PER_INCH 25.4
//#define INCH_PER_MM 0.039370078740157477

// on-off switch settings
#define OFF 0
#define ON 1

// feed_mode
enum feed_mode { UNITS_PER_MINUTE=0, INVERSE_TIME=1, UNITS_PER_REVOLUTION=2 };

// cutter radius compensation mode, OFF already defined to 0
// not using CANON_SIDE since interpreter handles cutter radius comp
#define RIGHT 1
#define LEFT 2

// spindle control modes
enum SPINDLE_MODE { CONSTANT_RPM, CONSTANT_SURFACE };

// unary operations
// These are not enums because the "&" operator is used in
// reading the operation names and is illegal with an enum

#define ABS 1
#define ACOS 2
#define ASIN 3
#define ATAN 4
#define COS 5
#define EXP 6
#define FIX 7
#define FUP 8
#define LN 9
#define ROUND 10
#define SIN 11
#define SQRT 12
#define TAN 13


// binary operations
#define NO_OPERATION 0
#define DIVIDED_BY 1
#define MODULO 2
#define POWER 3
#define TIMES 4
#define AND2 5
#define EXCLUSIVE_OR 6
#define MINUS 7
#define NON_EXCLUSIVE_OR 8
#define PLUS 9
#define RIGHT_BRACKET 10

/* relational operators (are binary operators)*/
#define LT 11
#define EQ 12
#define NE 13
#define LE 14
#define GE 15
#define GT 16
#define RELATIONAL_OP_FIRST 11
#define RELATIONAL_OP_LAST  16

// O code
#define O_none      0
#define O_sub       1
#define O_endsub    2
#define O_call      3
#define O_do        4
#define O_while     5
#define O_if        6
#define O_elseif    7
#define O_else      8
#define O_endif     9
#define O_break    10
#define O_continue 11
#define O_endwhile 12
#define O_return   13
#define O_repeat   14
#define O_endrepeat 15

// G Codes are symbolic to be dialect-independent in source code
#define G_0      0
#define G_1     10
#define G_2     20
#define G_3     30
#define G_4     40
#define G_5     50
#define G_5_1   51
#define G_5_2   52
#define G_5_3   53
#define G_7     70
#define G_8     80
#define G_10   100
#define G_17   170
#define G_17_1 171
#define G_18   180
#define G_18_1 181
#define G_19   190
#define G_19_1 191
#define G_20   200
#define G_21   210
#define G_28   280
#define G_28_1 281
#define G_30   300
#define G_30_1 301
#define G_33   330
#define G_33_1 331
#define G_38_2 382
#define G_38_3 383
#define G_38_4 384
#define G_38_5 385
#define G_40   400
#define G_41   410
#define G_41_1 411
#define G_42   420
#define G_42_1 421
#define G_43   430
#define G_43_1 431
#define G_49   490
#define G_50   500
#define G_51   510
#define G_53   530
#define G_54   540
#define G_55   550
#define G_56   560
#define G_57   570
#define G_58   580
#define G_59   590
#define G_59_1 591
#define G_59_2 592
#define G_59_3 593
#define G_61   610
#define G_61_1 611
#define G_64   640
#define G_73   730
#define G_76   760
#define G_80   800
#define G_81   810
#define G_82   820
#define G_83   830
#define G_84   840
#define G_85   850
#define G_86   860
#define G_87   870
#define G_88   880
#define G_89   890
#define G_90   900
#define G_90_1 901
#define G_91   910
#define G_91_1 911
#define G_92   920
#define G_92_1 921
#define G_92_2 922
#define G_92_3 923
#define G_93   930
#define G_94   940
#define G_95   950
#define G_96   960
#define G_97   970
#define G_98   980
#define G_99   990

// name of parameter file for saving/restoring interpreter variables
#define RS274NGC_PARAMETER_FILE_NAME_DEFAULT "rs274ngc.var"
#define RS274NGC_PARAMETER_FILE_BACKUP_SUFFIX ".bak"

// number of parameters in parameter table
#define RS274NGC_MAX_PARAMETERS 5414

// Subroutine parameters
#define INTERP_SUB_PARAMS 30
#define INTERP_OWORD_LABELS 1000
#define INTERP_SUB_ROUTINE_LEVELS 10
#define INTERP_FIRST_SUBROUTINE_PARAM 1

/**********************/
/*      TYPEDEFS      */
/**********************/

/* distance_mode */
typedef enum
{ MODE_ABSOLUTE, MODE_INCREMENTAL }
DISTANCE_MODE;

/* retract_mode for cycles */
typedef enum
{ R_PLANE, OLD_Z }
RETRACT_MODE;

typedef struct block_struct
{
  bool a_flag;
  double a_number;
  bool b_flag;
  double b_number;
  bool c_flag;
  double c_number;
  char comment[256];
  double d_number_float;
  bool d_flag;
  bool e_flag;
  double e_number;
  bool f_flag;
  double f_number;
  int g_modes[16];
  bool h_flag;
  int h_number;
  bool i_flag;
  double i_number;
  bool j_flag;
  double j_number;
  bool k_flag;
  double k_number;
  int l_number;
  bool l_flag;
  int line_number;
  int n_number;
  int motion_to_be;
  int m_count;
  int m_modes[11];
  int user_m;
  double p_number;
  bool p_flag;
  double q_number;
  bool q_flag;
  bool r_flag;
  double r_number;
  bool s_flag;
  double s_number;
  bool t_flag;
  int t_number;
  bool u_flag;
  double u_number;
  bool v_flag;
  double v_number;
  bool w_flag;
  double w_number;
  bool x_flag;
  double x_number;
  bool y_flag;
  double y_number;
  bool z_flag;
  double z_number;

  int radius_flag;
  double radius;
  int theta_flag;
  double theta;

  // control (o-word) stuff
  long     offset;   // start of line in file
  int      o_type;
  int      o_number;
  char    *o_name;   // !!!KL be sure to free this
  double   params[INTERP_SUB_PARAMS];
}
block;

typedef block *block_pointer;

#define NAMED_PARAMETERS_ALLOC_UNIT 20
struct named_parameters_struct {
  int named_parameter_alloc_size;
  int named_parameter_used_size;
  char **named_parameters;
  double *named_param_values;
  };

typedef struct context_struct {
  long position;       // location (ftell) in file
  int sequence_number; // location (line number) in file
  char *filename;      // name of file for this context
  char *subName;       // name of the subroutine (oword)
  double saved_params[INTERP_SUB_PARAMS];
  struct named_parameters_struct named_parameters;
}context;


// !!!KL ???use the index to this as a surrogate for the o-word text
typedef struct offset_struct {
  int o_word;
  char *o_word_name; // or zero
  int type;
  char *filename;  // the name of the file
  long offset;     // the offset in the file
  int sequence_number;
  int repeat_count;
}offset;

/*

The current_x, current_y, and current_z are the location of the tool
in the current coordinate system. current_x and current_y differ from
program_x and program_y when cutter radius compensation is on.
current_z is the position of the tool tip in program coordinates when
tool length compensation is using the actual tool length; it is the
position of the spindle when tool length is zero.

In a setup, the axis_offset values are set by g92 and the origin_offset
values are set by g54 - g59.3. The net origin offset uses both values
and is not represented here

*/
#define STACK_LEN 50
#define STACK_ENTRY_LEN 80
#define MAX_SUB_DIRS 10

typedef struct setup_struct
{
  double AA_axis_offset;        // A-axis g92 offset
  double AA_current;            // current A-axis position
  double AA_origin_offset;      // A-axis origin offset
  double BB_axis_offset;        // B-axis g92offset
  double BB_current;            // current B-axis position
  double BB_origin_offset;      // B-axis origin offset
  double CC_axis_offset;        // C-axis g92offset
  double CC_current;            // current C-axis position
  double CC_origin_offset;      // C-axis origin offset

  double u_axis_offset, u_current, u_origin_offset;
  double v_axis_offset, v_current, v_origin_offset;
  double w_axis_offset, w_current, w_origin_offset;

  int active_g_codes[ACTIVE_G_CODES];  // array of active G codes
  int active_m_codes[ACTIVE_M_CODES];  // array of active M codes
  double active_settings[ACTIVE_SETTINGS];     // array of feed, speed, etc.
  bool arc_not_allowed;       // we just exited cutter compensation, so we error if the next move isn't straight
  double axis_offset_x;         // X-axis g92 offset
  double axis_offset_y;         // Y-axis g92 offset
  double axis_offset_z;         // Z-axis g92 offset
  block block1;                 // parsed next block
  char blocktext[LINELEN];   // linetext downcased, white space gone
  CANON_MOTION_MODE control_mode;       // exact path or cutting mode
  int current_pocket;             // carousel slot number of current tool
  double current_x;             // current X-axis position
  double current_y;             // current Y-axis position
  double current_z;             // current Z-axis position
  double cutter_comp_radius;    // current cutter compensation radius
  int cutter_comp_orientation;  // current cutter compensation tool orientation
  int cutter_comp_side;         // current cutter compensation side
  double cycle_cc;              // cc-value (normal) for canned cycles
  double cycle_i;               // i-value for canned cycles
  double cycle_j;               // j-value for canned cycles
  double cycle_k;               // k-value for canned cycles
  int cycle_l;                  // l-value for canned cycles
  double cycle_p;               // p-value (dwell) for canned cycles
  double cycle_q;               // q-value for canned cycles
  double cycle_r;               // r-value for canned cycles
  double cycle_il;              // "initial level" height when switching from non-cycle into cycle, for g98 retract
  int cycle_il_flag;            // il is currently valid because we're in a series of cycles
  DISTANCE_MODE distance_mode;  // absolute or incremental
  DISTANCE_MODE ijk_distance_mode;  // absolute or incremental for IJK in arcs
  int feed_mode;                // G_93 (inverse time) or G_94 units/min
  bool feed_override;         // whether feed override is enabled
  double feed_rate;             // feed rate in current units/min
  char filename[PATH_MAX];      // name of currently open NC code file
  FILE *file_pointer;           // file pointer for open NC code file
  bool flood;                 // whether flood coolant is on
  int tool_offset_index;        // for use with tool length offsets
  CANON_UNITS length_units;     // millimeters or inches
  int line_length;              // length of line last read
  char linetext[LINELEN];       // text of most recent line read
  bool mist;                  // whether mist coolant is on
  int motion_mode;              // active G-code for motion
  int origin_index;             // active origin (1=G54 to 9=G59.3)
  double origin_offset_x;       // g5x offset x
  double origin_offset_y;       // g5x offset y
  double origin_offset_z;       // g5x offset z
  double rotation_xy;         // rotation of coordinate system around Z, in degrees
  double parameters[RS274NGC_MAX_PARAMETERS];   // system parameters
  int parameter_occurrence;     // parameter buffer index
  int parameter_numbers[50];    // parameter number buffer
  double parameter_values[50];  // parameter value buffer
  int named_parameter_occurrence;
  char *named_parameters[50];
  double named_parameter_values[50];
  bool percent_flag;          // ON means first line was percent sign
  CANON_PLANE plane;            // active plane, XY-, YZ-, or XZ-plane
  bool probe_flag;            // flag indicating probing done
  bool input_flag;            // flag indicating waiting for input done
  bool toolchange_flag;       // flag indicating we just had a tool change
  int input_index;		// channel queried
  bool input_digital;		// input queried was digital (OFF=analog)
  bool cutter_comp_firstmove; // this is the first comp move
  double program_x;             // program x, used when cutter comp on
  double program_y;             // program y, used when cutter comp on
  double program_z;             // program y, used when cutter comp on
  RETRACT_MODE retract_mode;    // for cycles, old_z or r_plane
  int random_toolchanger;       // tool changer swaps pockets, and pocket 0 is the spindle instead of "no tool"
  int selected_pocket;          // tool slot selected but not active
  int sequence_number;          // sequence number of line last read
  double speed;                 // current spindle speed in rpm or SxM
  SPINDLE_MODE spindle_mode;    // CONSTANT_RPM or CONSTANT_SURFACE
  CANON_SPEED_FEED_MODE speed_feed_mode;        // independent or synched
  bool speed_override;        // whether speed override is enabled
  CANON_DIRECTION spindle_turning;      // direction spindle is turning
  char stack[STACK_LEN][STACK_ENTRY_LEN];      // stack of calls for error reporting
  int stack_index;              // index into the stack
  EmcPose tool_offset;          // tool length offset
  int pockets_max;                 // number of pockets in carousel (including pocket 0, the spindle)
  CANON_TOOL_TABLE tool_table[CANON_POCKETS_MAX];      // index is pocket number
  double traverse_rate;         // rate for traverse motions

  /* stuff for subroutines and control structures */
  int defining_sub;                  // true if in a subroutine defn
  char *sub_name;                    // name of sub we are defining (free this)
  int doing_continue;                // true if doing a continue
  //int doing_break;                 // true if doing a break
  int executed_if;                   // true if executed in current if
  char *skipping_o;                  // o_name we are skipping for (or zero)
  char *skipping_to_sub;             // o_name of sub skipping to (or zero)
  int skipping_start;                // start of skipping (sequence)
  double test_value;                 // value for "if", "while", "elseif"
  int call_level;                    // current subroutine level
  context sub_context[INTERP_SUB_ROUTINE_LEVELS];
  int oword_labels;
  offset oword_offset[INTERP_OWORD_LABELS];
  bool adaptive_feed;              // adaptive feed is enabled
  bool feed_hold;                  // feed hold is enabled
  int loggingLevel;                  // 0 means logging is off
  char log_file[PATH_MAX];
  char program_prefix[PATH_MAX];            // program directory
  char subroutines[MAX_SUB_DIRS][PATH_MAX]; // subroutines directories
  int use_lazy_close;                // wait until next open before closing
                                     // the input file
  int lazy_closing;                  // close has been called
  char wizard_root[PATH_MAX];
  int tool_change_at_g30;
  int tool_change_quill_up;
  int tool_change_with_spindle_on;
  int a_axis_wrapped;
  int b_axis_wrapped;
  int c_axis_wrapped;

  int a_indexer;
  int b_indexer;
  int c_indexer;

  bool lathe_diameter_mode;       //Lathe diameter mode (g07/G08)
}
setup;

typedef setup *setup_pointer;

/*

The _setup model includes a stack array for the names of function
calls. This stack is written into if an error occurs. Just before each
function returns an error code, it writes its name in the next
available string, initializes the following string, and increments
the array index. The following four macros do the work.

The size of the stack array is 50. An error in the middle of a very
complex expression would cause the ERP and CHP macros to write past the
bounds of the array if a check were not provided. No real program
would contain such a thing, but the check is included to make the
macros totally crash-proof. If the function call stack is deeper than
49, the top of the stack will be missing.

*/

// Set an error string using printf-style formats and return
#define ERS(fmt, ...)                                      \
    do {                                                   \
        setError (fmt, ## __VA_ARGS__);                    \
        _setup.stack_index = 0;                            \
        strncpy(_setup.stack[_setup.stack_index], __PRETTY_FUNCTION__, STACK_ENTRY_LEN); \
        _setup.stack[_setup.stack_index][STACK_ENTRY_LEN-1] = 0; \
        _setup.stack_index++; \
        _setup.stack[_setup.stack_index][0] = 0;           \
        return INTERP_ERROR;                               \
    } while(0)

// Return one of the very few numeric errors
#define ERN(error_code)                                    \
    do {                                                   \
        _setup.stack_index = 0;                            \
        strncpy(_setup.stack[_setup.stack_index], __PRETTY_FUNCTION__, STACK_ENTRY_LEN); \
        _setup.stack[_setup.stack_index][STACK_ENTRY_LEN-1] = 0; \
        _setup.stack_index++; \
        _setup.stack[_setup.stack_index][0] = 0;           \
        return error_code;                                 \
    } while(0)


// Propagate an error up the stack
#define ERP(error_code)                                        \
    do {                                                       \
        if (_setup.stack_index < STACK_LEN - 1) {                         \
            strncpy(_setup.stack[_setup.stack_index], __PRETTY_FUNCTION__, STACK_ENTRY_LEN); \
            _setup.stack[_setup.stack_index][STACK_ENTRY_LEN-1] = 0;    \
            _setup.stack_index++;                                       \
            _setup.stack[_setup.stack_index][0] = 0;           \
        }                                                      \
        return error_code;                                     \
    } while(0)


// If the condition is true, set an error string as with ERS
#define CHKS(bad, fmt, ...)                                    \
    do {                                                       \
        if (bad) {                                             \
	    ERS(fmt, ## __VA_ARGS__);                          \
        }                                                      \
    } while(0)

// If the condition is true, return one of the few numeric errors
#define CHKN(bad, error_code)                                  \
    do {                                                       \
        if (bad) {                                             \
	    ERN(error_code);                                   \
        }                                                      \
    } while(0)


// Propagate an error up the stack as with ERP if the result of 'call' is not
// INTERP_OK
#define CHP(call)                                                  \
    do {                                                           \
	int CHP__status = (call);                                  \
        if (CHP__status != INTERP_OK) {                            \
	    ERP(CHP__status);                                      \
        }                                                          \
    } while(0)


#define CYCLE_MACRO(call) for (repeat = block->l_number; \
                               repeat > 0; \
                               repeat--) \
     { \
       aa = (aa + aa_increment); \
       bb = (bb + bb_increment); \
       if(radius_increment) { \
           double radius, theta; \
           CHKS((bb == 0 && aa == 0), _("Incremental motion with polar coordinates is indeterminate when at the origin")); \
           theta = atan2(bb, aa); \
           radius = hypot(bb, aa) + radius_increment; \
           aa = radius * cos(theta); \
           bb = radius * sin(theta); \
       } \
       if(theta_increment) { \
           double radius, theta; \
           CHKS((bb == 0 && aa == 0), _("Incremental motion with polar coordinates is indeterminate when at the origin")); \
           theta = atan2(bb, aa) + theta_increment; \
           radius = hypot(bb, aa); \
           aa = radius * cos(theta); \
           bb = radius * sin(theta); \
       } \
       cycle_traverse(block, plane, aa, bb, current_cc); \
       if (old_cc != r) \
         cycle_traverse(block, plane, aa, bb, r); \
       CHP(call); \
     }


#endif // INTERP_INTERNAL_HH
