#ifndef RS274NGC_HH
#define RS274NGC_HH

/*
  rs274ngc.hh

  Declarations for the rs274abc translator.

  Modification history:

  22-Mar-2004  FMP increased number of m_modes[] to 200, to support
  user-defined M codes M100-M199
*/

/**********************/
/* INCLUDE DIRECTIVES */
/**********************/

#include <stdio.h>
#include "canon.hh"

/**********************/
/*   COMPILER MACROS  */
/**********************/

#define AND              &&
#define IS               ==
#define ISNT             !=
#define MAX(x, y)        ((x) > (y) ? (x) : (y))
#define NOT              !
#define OR               ||
#define SET_TO           =

#ifndef TRUE
#define TRUE             1
#endif

#ifndef FALSE
#define FALSE            0
#endif

#define RS274NGC_TEXT_SIZE 256

/* numerical constants */
#define TOLERANCE_INCH 0.0005
#define TOLERANCE_MM 0.005
#define TOLERANCE_CONCAVE_CORNER 0.05 /* angle threshold for concavity for
                                         cutter compensation, in radians */
#define TINY 1e-12 /* for arc_data_r */
#define UNKNOWN 1e-20
#define TWO_PI  6.2831853071795864

#ifndef PI
#define PI      3.1415926535897932
#endif

#ifndef PI2
#define PI2     1.5707963267948966
#endif

// array sizes
#define RS274NGC_ACTIVE_G_CODES 12
#define RS274NGC_ACTIVE_M_CODES 7
#define RS274NGC_ACTIVE_SETTINGS 3

// name of parameter file for saving/restoring interpreter variables
#define RS274NGC_PARAMETER_FILE_NAME_DEFAULT "rs274ngc.var"
#define RS274NGC_PARAMETER_FILE_BACKUP_SUFFIX ".bak"

// max number of m codes on one line
#define MAX_EMS  4

// English - Metric conversion (long number keeps error buildup down)
#define MM_PER_INCH 25.4
#define INCH_PER_MM 0.039370078740157477

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

// number of parameters in parameter table
#define RS274NGC_MAX_PARAMETERS 5400

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
#define G_38_2 382
#define G_40   400
#define G_41   410
#define G_42   420
#define G_43   430
#define G_49   490
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

/**********************/
/*      TYPEDEFS      */
/**********************/

/* distance_mode */
typedef enum {MODE_ABSOLUTE, MODE_INCREMENTAL} DISTANCE_MODE;

/* retract_mode for cycles */
typedef enum {R_PLANE, OLD_Z} RETRACT_MODE;

typedef int ON_OFF;

typedef struct block_struct {
#ifdef AA
  ON_OFF   a_flag;
  double   a_number;
#endif
#ifdef BB
  ON_OFF   b_flag;
  double   b_number;
#endif
#ifdef CC
  ON_OFF   c_flag;
  double   c_number;
#endif
  char     comment[256];
  int      d_number;
  double   f_number;
  int      g_modes[14];
  int      h_number;
  ON_OFF   i_flag;
  double   i_number;
  ON_OFF   j_flag;
  double   j_number;
  ON_OFF   k_flag;
  double   k_number;
  int      l_number;
  int      line_number;
  int      motion_to_be;
  int      m_count;
  int      m_modes[200];
  int      user_m;
  double   p_number;
  double   q_number;
  ON_OFF   r_flag;
  double   r_number;
  double   s_number;
  int      t_number;
  ON_OFF   x_flag;
  double   x_number;
  ON_OFF   y_flag;
  double   y_number;
  ON_OFF   z_flag;
  double   z_number;
} block;

typedef block * block_pointer;

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

typedef struct setup_struct {
#ifdef AA
  double AA_axis_offset;             // A-axis g92 offset
  double AA_current;                 // current A-axis position
  double AA_origin_offset;           // A-axis origin offset
#endif
#ifdef BB
  double BB_axis_offset;             // B-axis g92offset
  double BB_current;                 // current B-axis position
  double BB_origin_offset;           // B-axis origin offset
#endif
#ifdef CC
  double CC_axis_offset;             // C-axis g92offset
  double CC_current;                 // current C-axis position
  double CC_origin_offset;           // C-axis origin offset
#endif
  int active_g_codes
      [RS274NGC_ACTIVE_G_CODES];     // array of active G codes
  int active_m_codes
      [RS274NGC_ACTIVE_M_CODES];     // array of active M codes
  double active_settings
      [RS274NGC_ACTIVE_SETTINGS];    // array of feed, speed, etc.
  double axis_offset_x;              // X-axis g92 offset
  double axis_offset_y;              // Y-axis g92 offset
  double axis_offset_z;              // Z-axis g92 offset
  block block1;                      // parsed next block
  char blocktext[RS274NGC_TEXT_SIZE];// linetext downcased, white space gone
  CANON_MOTION_MODE control_mode;    // exact path or cutting mode
  int current_slot;                  // carousel slot number of current tool
  double current_x;                  // current X-axis position
  double current_y;                  // current Y-axis position
  double current_z;                  // current Z-axis position
  double cutter_comp_radius;         // current cutter compensation radius
  int cutter_comp_side;              // current cutter compensation side
  double cycle_cc;                   // cc-value (normal) for canned cycles
  double cycle_i;                    // i-value for canned cycles
  double cycle_j;                    // j-value for canned cycles
  double cycle_k;                    // k-value for canned cycles
  int cycle_l;                       // l-value for canned cycles
  double cycle_p;                    // p-value (dwell) for canned cycles
  double cycle_q;                    // q-value for canned cycles
  double cycle_r;                    // r-value for canned cycles
  DISTANCE_MODE distance_mode;       // absolute or incremental
  int feed_mode;                     // G_93 (inverse time) or G_94 units/min
  ON_OFF feed_override;              // whether feed override is enabled
  double feed_rate;                  // feed rate in current units/min
  char filename[RS274NGC_TEXT_SIZE]; // name of currently open NC code file
  FILE * file_pointer;               // file pointer for open NC code file
  ON_OFF flood;                      // whether flood coolant is on
  int length_offset_index;           // for use with tool length offsets
  CANON_UNITS length_units;          // millimeters or inches
  int line_length;                   // length of line last read
  char linetext[RS274NGC_TEXT_SIZE]; // text of most recent line read
  ON_OFF mist;                       // whether mist coolant is on
  int motion_mode;                   // active G-code for motion
  int origin_index;                  // active origin (1=G54 to 9=G59.3)
  double origin_offset_x;            // origin offset x
  double origin_offset_y;            // origin offset y
  double origin_offset_z;            // origin offset z
  double parameters
      [RS274NGC_MAX_PARAMETERS];     // system parameters
  int parameter_occurrence;          // parameter buffer index
  int parameter_numbers[50];         // parameter number buffer
  double parameter_values[50];       // parameter value buffer
  ON_OFF percent_flag;               // ON means first line was percent sign
  CANON_PLANE plane;                 // active plane, XY-, YZ-, or XZ-plane
  ON_OFF probe_flag;                 // flag indicating probing done
  double program_x;                  // program x, used when cutter comp on
  double program_y;                  // program y, used when cutter comp on
  RETRACT_MODE retract_mode;         // for cycles, old_z or r_plane
  int selected_tool_slot;            // tool slot selected but not active
  int sequence_number;               // sequence number of line last read
  double speed;                      // current spindle speed in rpm
  CANON_SPEED_FEED_MODE speed_feed_mode;   // independent or synched
  ON_OFF speed_override;             // whether speed override is enabled
  CANON_DIRECTION spindle_turning;   // direction spindle is turning
  char stack[50][80];                // stack of calls for error reporting
  int stack_index;                   // index into the stack
  double tool_length_offset;         // current tool length offset
  int tool_max;                      // highest number tool slot in carousel
  CANON_TOOL_TABLE tool_table
       [CANON_TOOL_MAX + 1];         // index is slot number
  int tool_table_index;              // tool index used with cutter comp
  double traverse_rate;              // rate for traverse motions
} setup;

typedef setup * setup_pointer;

// pointer to function that reads
typedef int (*read_function_pointer) (char *, int *, block_pointer, double *);


/*************************************************************************/
/*

Interface functions to call to tell the interpreter what to do.
Return values indicate status of execution.
These functions may change the state of the interpreter.

*/

// close the currently open NC code file
extern int rs274ngc_close();

// execute a line of NC code
#ifndef NOT_OLD_EMC_INTERP_COMPATIBLE
extern int rs274ngc_execute(const char *command=0);
#else
extern int rs274ngc_execute();
#endif

// stop running
extern int rs274ngc_exit();

// get ready to run
extern int rs274ngc_init();

// load a tool table
extern int rs274ngc_load_tool_table();

// open a file of NC code
extern int rs274ngc_open(const char *filename);

// read the mdi or the next line of the open NC code file
extern int rs274ngc_read(const char * mdi = 0);

// reset yourself
extern int rs274ngc_reset();

// restore interpreter variables from a file
extern int rs274ngc_restore_parameters(const char * filename);

// save interpreter variables to file
extern int rs274ngc_save_parameters(const char * filename,
                                    const double parameters[]);

// synchronize your internal model with the external world
extern int rs274ngc_synch();


/*************************************************************************/
/*

Interface functions to call to get information from the interpreter.
If a function has a return value, the return value contains the information.
If a function returns nothing, information is copied into one of the
arguments to the function. These functions do not change the state of
the interpreter.

*/

// copy active G codes into array [0]..[11]
extern void rs274ngc_active_g_codes(int * codes);

// copy active M codes into array [0]..[6]
extern void rs274ngc_active_m_codes(int * codes);

// copy active F, S settings into array [0]..[2]
extern void rs274ngc_active_settings(double * settings);

// copy the text of the error message whose number is error_code into the
// error_text array, but stop at max_size if the text is longer.
extern void rs274ngc_error_text(int error_code, char * error_text,
                                int max_size);

// copy the name of the currently open file into the file_name array,
// but stop at max_size if the name is longer
extern void rs274ngc_file_name(char * file_name, int max_size);

// return the length of the most recently read line
extern int rs274ngc_line_length();

// copy the text of the most recently read line into the line_text array,
// but stop at max_size if the text is longer
extern void rs274ngc_line_text(char * line_text, int max_size);

// return the current sequence number (how many lines read)
extern int rs274ngc_sequence_number();

// copy the function name from the stack_index'th position of the
// function call stack at the time of the most recent error into
// the function name string, but stop at max_size if the name is longer
extern void rs274ngc_stack_name(int stack_index, char * function_name,
                                int max_size);


#ifndef NOT_OLD_EMC_INTERP_COMPATABLE
// Get the parameter file name from the ini file.
extern int rs274ngc_ini_load(const char *filename);
static inline int rs274ngc_line() {return rs274ngc_sequence_number();}
static inline const char *rs274ngc_command() { static char buf[100]; rs274ngc_line_text(buf,100); return buf;}
static inline const char *rs274ngc_file() { static char buf[100]; rs274ngc_file_name(buf,100); return buf;}

#endif



#endif
