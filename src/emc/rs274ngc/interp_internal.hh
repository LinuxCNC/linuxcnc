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
* Last change:
* $Revision$
* $Author$
* $Date$
********************************************************************/
#ifndef INTERP_INTERNAL_HH
#define INTERP_INTERNAL_HH

#include "config.h"
#include <stdio.h>
#include "canon.hh"

/**********************/
/*   COMPILER MACROS  */
/**********************/

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
#define UNKNOWN 1e-20

// max number of m codes on one line
#define MAX_EMS  4

// English - Metric conversion (long number keeps error buildup down)
#define MM_PER_INCH 25.4
//#define INCH_PER_MM 0.039370078740157477

// on-off switch settings
#define OFF 0
#define ON 1

// feed_mode
#define UNITS_PER_MINUTE 0
#define INVERSE_TIME 1

// cutter radius compensation mode, OFF already defined to 0
// not using CANON_SIDE since interpreter handles cutter radius comp
#define RIGHT 1
#define LEFT 2

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

// G Codes are symbolic to be dialect-independent in source code
#define G_0      0
#define G_1     10
#define G_2     20
#define G_3     30
#define G_4     40
#define G_10   100
#define G_17   170
#define G_18   180
#define G_19   190
#define G_20   200
#define G_21   210
#define G_28   280
#define G_30   300
#define G_33   330
#define G_38_2 382
#define G_40   400
#define G_41   410
#define G_42   420
#define G_43   430
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
#define G_91   910
#define G_92   920
#define G_92_1 921
#define G_92_2 922
#define G_92_3 923
#define G_93   930
#define G_94   940
#define G_98   980
#define G_99   990

// name of parameter file for saving/restoring interpreter variables
#define RS274NGC_PARAMETER_FILE_NAME_DEFAULT "rs274ngc.var"
#define RS274NGC_PARAMETER_FILE_BACKUP_SUFFIX ".bak"

// number of parameters in parameter table
#define RS274NGC_MAX_PARAMETERS 5400

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

typedef bool ON_OFF;

typedef struct block_struct
{
  ON_OFF a_flag;
  double a_number;
  ON_OFF b_flag;
  double b_number;
  ON_OFF c_flag;
  double c_number;
  char comment[256];
  int d_number;
  double f_number;
  int g_modes[15];
  int h_number;
  ON_OFF i_flag;
  double i_number;
  ON_OFF j_flag;
  double j_number;
  ON_OFF k_flag;
  double k_number;
  int l_number;
  int line_number;
  int motion_to_be;
  int m_count;
  int m_modes[11];
  int user_m;
  double p_number;
  double q_number;
  ON_OFF r_flag;
  double r_number;
  double s_number;
  int t_number;
  ON_OFF x_flag;
  double x_number;
  ON_OFF y_flag;
  double y_number;
  ON_OFF z_flag;
  double z_number;

  // control (o-word) stuff
  long     offset;   // start of line in file
  int      o_type;
  int      o_number;
  double   params[INTERP_SUB_PARAMS];
}
block;

typedef block *block_pointer;

typedef struct context_struct {
  long position;       // location (ftell) in file
  int sequence_number; // location (line number) in file
  double saved_params[INTERP_SUB_PARAMS];
}context;

typedef struct offset_struct {
  int o_word;
  int type;
  long offset;
  int sequence_number;
}offset;

// Declare class so that we can use it in the typedef.
class Interp;
typedef int (Interp::*read_function_pointer) (char *, int *, block_pointer, double *);

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
  int active_g_codes[ACTIVE_G_CODES];  // array of active G codes
  int active_m_codes[ACTIVE_M_CODES];  // array of active M codes
  double active_settings[ACTIVE_SETTINGS];     // array of feed, speed, etc.
  double axis_offset_x;         // X-axis g92 offset
  double axis_offset_y;         // Y-axis g92 offset
  double axis_offset_z;         // Z-axis g92 offset
  block block1;                 // parsed next block
  char blocktext[LINELEN];   // linetext downcased, white space gone
  CANON_MOTION_MODE control_mode;       // exact path or cutting mode
  int current_slot;             // carousel slot number of current tool
  double current_x;             // current X-axis position
  double current_y;             // current Y-axis position
  double current_z;             // current Z-axis position
  double cutter_comp_radius;    // current cutter compensation radius
  int cutter_comp_side;         // current cutter compensation side
  double cycle_cc;              // cc-value (normal) for canned cycles
  double cycle_i;               // i-value for canned cycles
  double cycle_j;               // j-value for canned cycles
  double cycle_k;               // k-value for canned cycles
  int cycle_l;                  // l-value for canned cycles
  double cycle_p;               // p-value (dwell) for canned cycles
  double cycle_q;               // q-value for canned cycles
  double cycle_r;               // r-value for canned cycles
  DISTANCE_MODE distance_mode;  // absolute or incremental
  int feed_mode;                // G_93 (inverse time) or G_94 units/min
  ON_OFF feed_override;         // whether feed override is enabled
  double feed_rate;             // feed rate in current units/min
  char filename[LINELEN];    // name of currently open NC code file
  FILE *file_pointer;           // file pointer for open NC code file
  ON_OFF flood;                 // whether flood coolant is on
  int tool_offset_index;        // for use with tool length offsets
  CANON_UNITS length_units;     // millimeters or inches
  int line_length;              // length of line last read
  char linetext[LINELEN];    // text of most recent line read
  ON_OFF mist;                  // whether mist coolant is on
  int motion_mode;              // active G-code for motion
  int origin_index;             // active origin (1=G54 to 9=G59.3)
  double origin_offset_x;       // origin offset x
  double origin_offset_y;       // origin offset y
  double origin_offset_z;       // origin offset z
  double parameters[RS274NGC_MAX_PARAMETERS];   // system parameters
  int parameter_occurrence;     // parameter buffer index
  int parameter_numbers[50];    // parameter number buffer
  double parameter_values[50];  // parameter value buffer
  ON_OFF percent_flag;          // ON means first line was percent sign
  CANON_PLANE plane;            // active plane, XY-, YZ-, or XZ-plane
  ON_OFF probe_flag;            // flag indicating probing done
  double program_x;             // program x, used when cutter comp on
  double program_y;             // program y, used when cutter comp on
  double program_z;             // program y, used when cutter comp on
  RETRACT_MODE retract_mode;    // for cycles, old_z or r_plane
  int selected_tool_slot;       // tool slot selected but not active
  int sequence_number;          // sequence number of line last read
  double speed;                 // current spindle speed in rpm
  CANON_SPEED_FEED_MODE speed_feed_mode;        // independent or synched
  ON_OFF speed_override;        // whether speed override is enabled
  CANON_DIRECTION spindle_turning;      // direction spindle is turning
  char stack[50][80];           // stack of calls for error reporting
  int stack_index;              // index into the stack
  double tool_zoffset;          // current tool Z offset (AKA tool length offset)
  double tool_xoffset;          // current tool X offset
  int tool_max;                 // highest number tool slot in carousel
  CANON_TOOL_TABLE tool_table[CANON_TOOL_MAX + 1];      // index is slot number
  int tool_table_index;         // tool index used with cutter comp
  double traverse_rate;         // rate for traverse motions

  /* stuff for subroutines and control structures */
  int defining_sub;                  // true if in a subroutine defn
  int doing_continue;                // true if doing a continue
  //int doing_break;                 // true if doing a break
  int executed_if;                   // true if executed in current if
  int skipping_o;                    // o_number we are skipping for (or zero)
  double test_value;                 // value for "if", "while", "elseif"
  int call_level;                    // current subroutine level
  context sub_context[INTERP_SUB_ROUTINE_LEVELS];
  int oword_labels;
  offset oword_offset[INTERP_OWORD_LABELS];
  ON_OFF adaptive_feed;              // adaptive feed is enabled
  ON_OFF feed_hold;                  // feed hold is enabled
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

#define ERM(error_code) if (1) {                    \
  _setup.stack_index = 0;                      \
  strcpy(_setup.stack[_setup.stack_index++], name); \
  _setup.stack[_setup.stack_index][0] = 0;     \
  return error_code;                                \
  } else

#define ERP(error_code) if (_setup.stack_index < 49) { \
  strcpy(_setup.stack[_setup.stack_index++], name);    \
  _setup.stack[_setup.stack_index][0] = 0;        \
  return error_code;                                   \
  } else return error_code

#define CHK(bad, error_code) if (bad) {             \
  _setup.stack_index = 0;                      \
  strcpy(_setup.stack[_setup.stack_index++], name); \
  _setup.stack[_setup.stack_index][0] = 0;     \
  return error_code;                                \
  } else

#define CHP(try_this)                                      \
  if ((status = (try_this)) != INTERP_OK) {       \
     if (_setup.stack_index < 49)                          \
        {strcpy(_setup.stack[_setup.stack_index++], name); \
         _setup.stack[_setup.stack_index][0] = 0;     \
         return status;}                                   \
     else {return status;}                                 \
  } else


#define CYCLE_MACRO(call) for (repeat = block->l_number; \
                               repeat > 0;                    \
                               repeat--)                      \
     {                                                        \
       aa = (aa + aa_increment);                         \
       bb = (bb + bb_increment);                         \
       cycle_traverse(plane, aa, bb, old_cc);                 \
       if (old_cc != r)                                     \
         cycle_traverse(plane, aa, bb, r);                    \
       CHP(call);                                             \
       old_cc = clear_cc;                                \
     }


#endif // INTERP_INTERNAL_HH
