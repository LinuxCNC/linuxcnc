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

#include <locale.h>
#include <algorithm>
#include "linuxcnc.h"
#include <limits.h>
#include <stdio.h>
#include <set>
#include <map>
#include <bitset>
#include "canon.hh"
#include "emcpos.h"
#include "libintl.h"
#include <boost/python/object_fwd.hpp>
#include <cmath>
#include <rtapi_string.h>	// rtapi_strlcpy()
#include "interp_parameter_def.hh"
#include "interp_fwd.hh"
#include "interp_base.hh"
#include "tooldata.hh"


#define _(s) gettext(s)

/**********************/
/*   COMPILER MACROS  */
/**********************/

template<class T>
T R2D(T r) { return r * (180. / M_PI); }
template<class T>
T D2R(T r) { return r * (M_PI / 180.); }
template<class T>
T SQ(T a) { return a*a; }

template<class T>
inline int round_to_int(T x) {
    return (int)std::nearbyint(x);
}

/* nested remap: a remapped code is found in the body of a subroutine
 * which is executing on behalf of another remapped code
 * example: a user G-code command executes a tool change
 */
#define MAX_NESTED_REMAPS 10

/* numerical constants */

/*****************************************************************************
The default tolerance (if none tighter is specified in the INI file) should be:
2 * 0.001 * sqrt(2) for inch, and 2 * 0.01 * sqrt(2) for mm.
This would mean that any valid arc where the endpoints and/or centerpoint
got rounded or truncated to 0.001 inch or 0.01 mm precision would be accepted.

Tighter tolerance down to a minimum of 1 micron +- also accepted.
******************************************************************************/

#define CENTER_ARC_RADIUS_TOLERANCE_INCH (2 * 0.001 * M_SQRT2)
#define MIN_CENTER_ARC_RADIUS_TOLERANCE_INCH 0.00004

// Note: started from original tolerance and divided by 10 here (since that was originally done inside the interpreter)
#define RADIUS_TOLERANCE_INCH 0.00005

/* Equivalent metric constants */

#define CENTER_ARC_RADIUS_TOLERANCE_MM (2 * 0.01 * M_SQRT2)
#define MIN_CENTER_ARC_RADIUS_TOLERANCE_MM 0.001

#define RADIUS_TOLERANCE_MM (RADIUS_TOLERANCE_INCH * MM_PER_INCH)

// Modest relative error
#define SPIRAL_RELATIVE_TOLERANCE 0.001

/* angle threshold for concavity for cutter compensation, in radians */
#define TOLERANCE_CONCAVE_CORNER 0.05
#define TOLERANCE_EQUAL 1e-6 /* two numbers compare EQ if the
				  difference is less than this */

static inline bool equal(double a, double b)
{
    return (fabs(a - b) < TOLERANCE_EQUAL);
}

#define TINY 1e-12              /* for arc_data_r */

// max number of m codes on one line
#define MAX_EMS  4

// feed_mode
enum class FEED_MODE {
    UNITS_PER_MINUTE=0,
    INVERSE_TIME=1,
    UNITS_PER_REVOLUTION=2
};

// cutter radius compensation mode, 0 or false means none
// not using CANON_SIDE since interpreter handles cutter radius comp
enum class CUTTER_COMP {
    OFF = 0,
    RIGHT = 1,
    LEFT = 2,
};

// spindle control modes
enum class SPINDLE_MODE {
    CONSTANT_RPM,
    CONSTANT_SURFACE
};

// unary operations
// These are not enums because the "&" operator is used in
// reading the operation names and is illegal with an enum

enum UnaryOperations
{
    ABS = 1,
    ACOS = 2,
    ASIN = 3,
    ATAN = 4,
    COS = 5,
    EXP = 6,
    FIX = 7,
    FUP = 8,
    LN = 9,
    ROUND = 10,
    SIN = 11,
    SQRT = 12,
    TAN = 13,
    EXISTS = 14,
};


// binary operations
enum BinaryOperations
{
    NO_OPERATION = 0,
    DIVIDED_BY = 1,
    MODULO = 2,
    POWER = 3,
    TIMES = 4,
    AND2 = 5,
    EXCLUSIVE_OR = 6,
    MINUS = 7,
    NON_EXCLUSIVE_OR = 8,
    PLUS = 9,
    RIGHT_BRACKET = 10,
    /* relational operators (are binary operators)*/
    LT = 11,
    EQ = 12,
    NE = 13,
    LE = 14,
    GE = 15,
    GT = 16,
    RELATIONAL_OP_FIRST = 11,
    RELATIONAL_OP_LAST = 16,
};

// O code
enum OCodes
{
    O_none = 0,
    O_sub = 1,
    O_endsub = 2,
    O_call = 3,
    O_do = 4,
    O_while = 5,
    O_if = 6,
    O_elseif = 7,
    O_else = 8,
    O_endif = 9,
    O_break = 10,
    O_continue = 11,
    O_endwhile = 12,
    O_return = 13,
    O_repeat = 14,
    O_endrepeat = 15,
    M_98 = 16,
    M_99 = 17,
    O_ = 18,
};

// G-codes are symbolic to be dialect-independent in source code
enum GCodes
{
    G_0 = 0,
    G_1 = 10,
    G_2 = 20,
    G_3 = 30,
    G_4 = 40,
    G_5 = 50,
    G_5_1 = 51,
    G_5_2 = 52,
    G_5_3 = 53,
    G_6	= 60,
    G_6_1 = 61,
    G_6_2 = 62,
    G_6_3 = 63,
    G_7 = 70,
    G_8 = 80,
    G_10 = 100,
    G_17 = 170,
    G_17_1 = 171,
    G_18 = 180,
    G_18_1 = 181,
    G_19 = 190,
    G_19_1 = 191,
    G_20 = 200,
    G_21 = 210,
    G_28 = 280,
    G_28_1 = 281,
    G_30 = 300,
    G_30_1 = 301,
    G_33 = 330,
    G_33_1 = 331,
    G_38_2 = 382,
    G_38_3 = 383,
    G_38_4 = 384,
    G_38_5 = 385,
    G_40 = 400,
    G_41 = 410,
    G_41_1 = 411,
    G_42 = 420,
    G_42_1 = 421,
    G_43 = 430,
    G_43_1 = 431,
    G_43_2 = 432,
    G_49 = 490,
    G_50 = 500,
    G_51 = 510,
    G_52 = 520,
    G_53 = 530,
    G_54 = 540,
    G_55 = 550,
    G_56 = 560,
    G_57 = 570,
    G_58 = 580,
    G_59 = 590,
    G_59_1 = 591,
    G_59_2 = 592,
    G_59_3 = 593,
    G_61 = 610,
    G_61_1 = 611,
    G_64 = 640,
    G_70 = 700,
    G_71 = 710,
    G_71_1 = 711,
    G_71_2 = 712,
    G_72 = 720,
    G_72_1 = 721,
    G_72_2 = 722,
    G_73 = 730,
    G_74 = 740,
    G_76 = 760,
    G_80 = 800,
    G_81 = 810,
    G_82 = 820,
    G_83 = 830,
    G_84 = 840,
    G_85 = 850,
    G_86 = 860,
    G_87 = 870,
    G_88 = 880,
    G_89 = 890,
    G_90 = 900,
    G_90_1 = 901,
    G_91 = 910,
    G_91_1 = 911,
    G_92 = 920,
    G_92_1 = 921,
    G_92_2 = 922,
    G_92_3 = 923,
    G_93 = 930,
    G_94 = 940,
    G_95 = 950,
    G_96 = 960,
    G_97 = 970,
    G_98 = 980,
    G_99 = 990,
};

std::string toString(GCodes g);

// name of parameter file for saving/restoring interpreter variables
#define RS274NGC_PARAMETER_FILE_NAME_DEFAULT "rs274ngc.var"
#define RS274NGC_PARAMETER_FILE_BACKUP_SUFFIX ".bak"

// Subroutine parameters
#define INTERP_SUB_PARAMS 30
#define INTERP_SUB_ROUTINE_LEVELS 10
#define INTERP_FIRST_SUBROUTINE_PARAM 1

// max number of local variables saved (?)
#define MAX_NAMED_PARAMETERS 50

/**********************/
/*      TYPEDEFS      */
/**********************/

/* distance_mode */
enum class DISTANCE_MODE
{
    ABSOLUTE,
    INCREMENTAL,
};

/* retract_mode for cycles */
enum class RETRACT_MODE
{
    R_PLANE,
    OLD_Z,
};

// string table - to get rid of strdup/free
const char *strstore(const char *s);


// Block execution phases in execution order
// very carefully check code for sequencing when
// adding phases!

// used to record execution trail in breadcrumbs
enum phases  {
    NO_REMAPPED_STEPS,
    STEP_COMMENT,
    STEP_SPINDLE_MODE,
    STEP_FEED_MODE,
    STEP_SET_FEED_RATE,
    STEP_SET_SPINDLE_SPEED,
    STEP_PREPARE,
    STEP_M_5,
    STEP_M_6,
    STEP_RETAIN_G43,
    STEP_M_7,
    STEP_M_8,
    STEP_M_9,
    STEP_M_10,
    STEP_DWELL,
    STEP_SET_PLANE,
    STEP_LENGTH_UNITS,
    STEP_LATHE_DIAMETER_MODE,
    STEP_CUTTER_COMP,
    STEP_TOOL_LENGTH_OFFSET,
    STEP_COORD_SYSTEM,
    STEP_CONTROL_MODE,
    STEP_DISTANCE_MODE,
    STEP_IJK_DISTANCE_MODE,
    STEP_RETRACT_MODE,
    STEP_MODAL_0,
    STEP_G92_IS_APPLIED,
    STEP_MOTION,
    STEP_MGROUP4,
    MAX_STEPS
};


// Modal groups
// also indices into g_modes
// unused: 9,11
enum ModalGroups
{
    GM_MODAL_0 = 0,
    GM_MOTION = 1,
    GM_SET_PLANE = 2,
    GM_DISTANCE_MODE = 3,
    GM_IJK_DISTANCE_MODE = 4,
    GM_FEED_MODE = 5,
    GM_LENGTH_UNITS = 6,
    GM_CUTTER_COMP = 7,
    GM_TOOL_LENGTH_OFFSET = 8,
    // 9 unused
    GM_RETRACT_MODE = 10,
    // 11 unused
    GM_COORD_SYSTEM = 12,
    GM_CONTROL_MODE = 13,
    GM_SPINDLE_MODE = 14,
    GM_LATHE_DIAMETER_MODE = 15,
    GM_G92_IS_APPLIED = 16,
    GM_MAX_MODAL_GROUPS
};

// the remap configuration descriptor
struct remap_struct {
    const char *name;
    const char *argspec;
    // if no modalgroup= was given in the REMAP= line, use these defaults
#define MCODE_DEFAULT_MODAL_GROUP 10
#define GCODE_DEFAULT_MODAL_GROUP 1
    int modal_group;
    int motion_code; // only for g's - to identify cycles
    const char *prolog_func; // Py function or null
    const char *remap_py;    // Py function maybe  null, OR
    const char *remap_ngc;   // NGC file, maybe  null
    const char *epilog_func; // Py function or null
};


// case insensitive compare for std::map etc
struct nocase_cmp
{
    bool operator()(const char* s1, const char* s2) const
    {
        return strcasecmp(s1, s2) < 0;
    }
};

typedef std::map<const char *,remap,nocase_cmp> remap_map;
typedef remap_map::iterator remap_iterator;

typedef std::map<int, remap_pointer> int_remap_map;
typedef int_remap_map::iterator int_remap_iterator;

#define REMAP_FUNC(r) (r->remap_ngc ? r->remap_ngc: \
		       (r->remap_py ? r->remap_py : "BUG-no-remap-func"))

struct block_struct
{
  char comment[256]{};
  double a_number{};
  double b_number{};
  double c_number{};
  double d_number_float{};
  double e_number{};
  double f_number{};
  int h_number{};
  double i_number{};
  double j_number{};
  double k_number{};
  int l_number{};
  int n_number{};
  double p_number{};
  double q_number{};
  double r_number{};
  double s_number{};
  int t_number{};
  double u_number{};
  double v_number{};
  double w_number{};
  double x_number{};
  double y_number{};
  double z_number{};

  int line_number{};
  int saved_line_number{};  // value of sequence_number when a remap was encountered
  int motion_to_be{};
  int m_count{};
  int m_modes[11]{};
  int user_m{};
  int dollar_number{};
  int g_modes[GM_MAX_MODAL_GROUPS]{};

  bool a_flag{};
  bool b_flag{};
  bool c_flag{};
  bool d_flag{};
  bool e_flag{};
  bool f_flag{};
  bool h_flag{};
  bool i_flag{};
  bool j_flag{};
  bool k_flag{};
  bool l_flag{};
  bool p_flag{};
  bool q_flag{};
  bool r_flag{};
  bool s_flag{};
  bool t_flag{};
  bool u_flag{};
  bool v_flag{};
  bool w_flag{};
  bool x_flag{};
  bool y_flag{};
  bool z_flag{};

  bool dollar_flag{};

  double radius{};
  double theta{};
  int radius_flag{};
  int theta_flag{};

  // control (o-word) stuff
  long     offset{};   // start of line in file
  int      o_type{};
  int      call_type{}; // oword-sub, python oword-sub, remap
  const char    *o_name{};   // !!!KL be sure to free this
  double   params[INTERP_SUB_PARAMS]{};
  int param_cnt{};

  // bitmap of phases already executed
  // we have some 31 or so different steps in a block. We must remember
  // which one is done when we reexecute a block after a remap.
  std::bitset<MAX_STEPS>  breadcrumbs{};

#define TICKOFF(step) block->breadcrumbs[step] = 1
#define TODO(step) (block->breadcrumbs[step] == 0)
#define ONCE(step) (TODO(step) ? TICKOFF(step),1 : 0)
#define ONCE_M(step) (TODO(STEP_M_ ## step) ? TICKOFF(STEP_M_ ## step),1 : 0)


    // there might be several remapped items in a block, but at any point
    // in time there's only one executing
    // conceptually blocks[1..n] are also the 'remap frames'
    remap_pointer executing_remap{}; // refers to config descriptor
    std::set<int> remappings{}; // all remappings in this block (enum phases)
    int phase{}; // current remap execution phase

    // the strategy to get the builtin behaviour of a code in a remap procedure is as follows:
    // if recursion is detected in find_remappings() (called by parse_line()), that *step* 
    // (roughly the modal group) is NOT added to the set of remapped steps in a block (block->remappings)
    // in the convert_* procedures we test if the step is remapped with the macro below, and whether
    // it is the current code which is remapped (IS_USER_MCODE, IS_USER_GCODE etc). If both
    // are true, we execute the remap procedure; if not, use the builtin code.
#define STEP_REMAPPED_IN_BLOCK(bp, step) (bp->remappings.find(step) != bp->remappings.end())

    // true if in a remap procedure the code being remapped was
    // referenced, which caused execution of the builtin semantics
    // reason for recording the fact: this permits an epilog to do the
    // right thing depending on whether the builtin was used or not.
    bool builtin_used{};
};

// indicates which type of Python handler yielded, and needs reexecution
// post sync/read_inputs
enum call_states {
    CS_NORMAL,
    CS_REEXEC_PROLOG,
    CS_REEXEC_PYBODY,
    CS_REEXEC_EPILOG,
    CS_REEXEC_PYOSUB,
};

// detail for O_call; tags the frame
enum call_types {
    CT_NONE,             // not in a call
    CT_NGC_OWORD_SUB,    // no restartable Python code involved
    CT_NGC_M98_SUB,      // like above; Fanuc-style, pass in params #1..#30
    CT_PYTHON_OWORD_SUB, // restartable Python code may be involved
    CT_REMAP,            // restartable Python code may be involved
};


enum retopts { RET_NONE, RET_DOUBLE, RET_INT, RET_YIELD, RET_STOPITERATION, RET_ERRORMSG };

// parameters will go to a std::map<const char *,parameter_value_pointer>
struct parameter_value_struct {
    double value;
    unsigned attr;
};

typedef std::map<const char *, parameter_value, nocase_cmp> parameter_map;
typedef parameter_map::iterator parameter_map_iterator;

#define PA_READONLY	1
#define PA_GLOBAL	2
#define PA_UNSET	4
#define PA_USE_LOOKUP	8   // use lookup_named_param() to retrieve value
#define PA_FROM_INI	16  // a variable of the form '_[section]value' was retrieved from the INI file
#define PA_PYTHON	32  // call namedparams.<varname>() to retrieve the value

// optional 3rd arg to store_named_param()
// flag initialization of r/o parameter
#define OVERRIDE_READONLY 1

#define MAX_REMAPOPTS 20
// current implementation limits - legal modal groups
// for M- and G-codes
#define M_MODE_OK(m) ((m > 3) && (m < 11))
#define G_MODE_OK(m) (m == 1)

struct pycontext_impl;
struct pycontext {
    pycontext();
    pycontext(const struct pycontext &);
    pycontext &operator=(const struct pycontext &);
    ~pycontext();
    pycontext_impl *impl;
};

struct context_struct {
    context_struct();
    void clear();

    long position;       // location (ftell) in file
    int sequence_number; // location (line number) in file
    const char *filename;      // name of file for this context
    const char *subName;       // name of the subroutine (oword)
    int m98_loop_counter;      // loop counter for Fanuc-style sub calls
    double saved_params[INTERP_SUB_PARAMS];
    parameter_map named_params;
    unsigned char context_status;		// see CONTEXT_ defines below
    int saved_g_codes[ACTIVE_G_CODES];  // array of active G-codes
    int saved_m_codes[ACTIVE_M_CODES];  // array of active M-codes
    double saved_settings[ACTIVE_SETTINGS];     // array of feed, speed, etc.
    int call_type; // enum call_types
    pycontext pystuff;
    // Python-related stuff
};

// context.context_status
#define CONTEXT_VALID   1 // this was stored by M7*
#define CONTEXT_RESTORE_ON_RETURN 2 // automatically execute M71 on sub return
#define REMAP_FRAME   4 // a remap call frame

struct offset_struct {
  int type;
  const char *filename;  // the name of the file
  long offset;     // the offset in the file
  int sequence_number;
  int repeat_count;
};

typedef std::map<const char *, offset, nocase_cmp> offset_map_type;
typedef std::map<const char *, offset, nocase_cmp>::iterator offset_map_iterator;

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
#define STACK_ENTRY_LEN 256
#define MAX_SUB_DIRS 10

struct setup
{
  setup();
  ~setup();

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

  int active_g_codes[ACTIVE_G_CODES];  // array of active G-codes
  int active_m_codes[ACTIVE_M_CODES];  // array of active M-codes
  double active_settings[ACTIVE_SETTINGS];     // array of feed, speed, etc.
  StateTag state_tag;

  bool arc_not_allowed;       // we just exited cutter compensation, so we error if the next move isn't straight
  double axis_offset_x;         // X-axis g92 offset
  double axis_offset_y;         // Y-axis g92 offset
  double axis_offset_z;         // Z-axis g92 offset
  // block block1;                 // parsed next block
  // stack of controlling blocks for remap execution
  block blocks[MAX_NESTED_REMAPS];
  // index into blocks, points to currently controlling block
  int remap_level;

#define CONTROLLING_BLOCK(s) ((s).blocks[(s).remap_level])
#define EXECUTING_BLOCK(s)   ((s).blocks[0])

  char blocktext[LINELEN];   // linetext downcased, white space gone
  CANON_MOTION_MODE control_mode;       // exact path or cutting mode
    double tolerance;           // G64 blending tolerance
    double naivecam_tolerance;  // G64 naive cam tolerance
    double tolerance_default;   // G64 P Default value, -1 to disable
    double naivecam_tolerance_default; // G64 Q Default Value, -1 to disable 
  int current_pocket;             // carousel slot (index) number of current tool
  double current_x;             // current X-axis position
  double current_y;             // current Y-axis position
  double current_z;             // current Z-axis position
  double cutter_comp_radius;    // current cutter compensation radius
  int cutter_comp_orientation;  // current cutter compensation tool orientation
  CUTTER_COMP cutter_comp_side;         // current cutter compensation side
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
  FEED_MODE feed_mode;                // G_93 (inverse time) or G_94 units/min
  bool feed_override;         // whether feed override is enabled
  double feed_rate;             // feed rate in current units/min
  char filename[PATH_MAX];      // name of currently open NC code file
  FILE *file_pointer;           // file pointer for open NC code file
  bool flood;                 // whether flood coolant is on
  CANON_UNITS length_units;     // millimeters or inches
  double center_arc_radius_tolerance_inch; // modify with INI setting
  double center_arc_radius_tolerance_mm;   // modify with INI setting
  int line_length;              // length of line last read
  char linetext[LINELEN];       // text of most recent line read
  bool mist;                  // whether mist coolant is on
  int motion_mode;              // active G-code for motion
  int origin_index;             // active origin (1=G54 to 9=G59.3)
  double origin_offset_x;       // g5x offset x
  double origin_offset_y;       // g5x offset y
  double origin_offset_z;       // g5x offset z
  double rotation_xy;         // rotation of coordinate system around Z, in degrees
  double parameters[interp_param_global::RS274NGC_MAX_PARAMETERS];   // system parameters
  int parameter_occurrence;     // parameter buffer index
  int parameter_numbers[MAX_NAMED_PARAMETERS];    // parameter number buffer
  double parameter_values[MAX_NAMED_PARAMETERS];  // parameter value buffer
  int named_parameter_occurrence;
  const char *named_parameters[MAX_NAMED_PARAMETERS];
  double named_parameter_values[MAX_NAMED_PARAMETERS];
  bool percent_flag;          // true means first line was percent sign
  CANON_PLANE plane;            // active plane, XY-, YZ-, or XZ-plane
  bool probe_flag;            // flag indicating probing done
  bool input_flag;            // flag indicating waiting for input done
  bool toolchange_flag;       // flag indicating we just had a tool change
  int input_index;		// channel queried
  bool input_digital;		// input queried was digital (false=analog)
  bool cutter_comp_firstmove; // this is the first comp move
  double program_x;             // program x, used when cutter comp on
  double program_y;             // program y, used when cutter comp on
  double program_z;             // program y, used when cutter comp on
  RETRACT_MODE retract_mode;    // for cycles, old_z or r_plane
  int random_toolchanger;       // tool changer swaps pockets, and pocket 0 is the spindle instead of "no tool"
  int selected_pocket;          // tool slot (index) selected but not active
    int selected_tool;          // start switchover to pocket-agnostic interp
  int sequence_number;          // sequence number of line last read
  int num_spindles;				// number of spindles available
  int active_spindle;			// the spindle currently used for CSS, FPR etc.
  double speed[EMCMOT_MAX_SPINDLES];// array of spindle speeds
  SPINDLE_MODE spindle_mode[EMCMOT_MAX_SPINDLES];// SPINDLE_MODE::CONSTANT_RPM or SPINDLE_MODE::CONSTANT_SURFACE
  CANON_SPEED_FEED_MODE speed_feed_mode;        // independent or synched
  bool speed_override[EMCMOT_MAX_SPINDLES];        // whether speed override is enabled
  CANON_DIRECTION spindle_turning[EMCMOT_MAX_SPINDLES];  // direction spindle is turning
  char stack[STACK_LEN][STACK_ENTRY_LEN];      // stack of calls for error reporting
  int stack_index;              // index into the stack
  EmcPose tool_offset;          // tool length offset
  CANON_TOOL_TABLE tool_table[CANON_POCKETS_MAX];      // index is pocket number
  double traverse_rate;         // rate for traverse motions
  double orient_offset;         // added to M19 R word, from [RS274NGC]ORIENT_OFFSET
  bool g43_with_zero_offset;    // added to allow active G43 with tool offset values all zero

  /* stuff for subroutines and control structures */
  int defining_sub;                  // true if in a subroutine defn
  const char *sub_name;                    // name of sub we are defining (free this)
  int doing_continue;                // true if doing a continue
  int doing_break;                   // true if doing a break
  int executed_if;                   // true if executed in current if
  const char *skipping_o;                  // o_name we are skipping for (or zero)
  const char *skipping_to_sub;             // o_name of sub skipping to (or zero)
  int skipping_start;                // start of skipping (sequence)
  double test_value;                 // value for "if", "while", "elseif"
  double return_value;               // optional return value for "return", "endsub"
  int value_returned;                // the last NGC procedure did/did not return a value
  int call_level;                    // current subroutine level
  context sub_context[INTERP_SUB_ROUTINE_LEVELS];
  int call_state;                  //  enum call_states - indicate Py handler reexecution
  offset_map_type offset_map;      // store label x name, file, line

  bool adaptive_feed;              // adaptive feed is enabled
  bool feed_hold;                  // feed hold is enabled
  int loggingLevel;                  // 0 means logging is off
  int debugmask;                     // from INI EMC/DEBUG
  char log_file[PATH_MAX];
  char program_prefix[PATH_MAX];            // program directory
  const char *subroutines[MAX_SUB_DIRS];  // subroutines directories
  int use_lazy_close;                // wait until next open before closing
                                     // the input file
  int lazy_closing;                  // close has been called
  char wizard_root[PATH_MAX];
  int tool_change_at_g30;
  int tool_change_quill_up;
  int tool_change_with_spindle_on;
  double parameter_g73_peck_clearance;
  double parameter_g83_peck_clearance;
  int a_axis_wrapped;
  int b_axis_wrapped;
  int c_axis_wrapped;

  int a_indexer_jnum;
  int b_indexer_jnum;
  int c_indexer_jnum;

  bool lathe_diameter_mode;       //Lathe diameter mode (g07/G08)
  bool mdi_interrupt;
  int feature_set;

    int disable_fanuc_style_sub;
    // M99 in main is treated as program end by default; this causes
    // control to skip to beginning of file
    bool loop_on_main_m99;

  int disable_g92_persistence;

#define FEATURE(x) (_setup.feature_set & FEATURE_ ## x)
#define FEATURE_RETAIN_G43           0x00000001
#define FEATURE_OWORD_N_ARGS         0x00000002
#define FEATURE_INI_VARS             0x00000004
#define FEATURE_HAL_PIN_VARS         0x00000008
    // do not lowercase named params inside comments - for #<_hal[PinName]>
#define FEATURE_NO_DOWNCASE_OWORD    0x00000010
#define FEATURE_OWORD_WARNONLY       0x00000020

    boost::python::object *pythis;  // boost::cref to 'this'
    const char *on_abort_command;
    int_remap_map  g_remapped,m_remapped;
    remap_map remaps;
#define INIT_FUNC  "__init__"
#define DELETE_FUNC  "__delete__"

    // task calls upon interp.init() repeatedly
    // protect init() operations which are not idempotent
    int init_once;  
};


// the externally visible singleton instance

extern class PythonPlugin *python_plugin;
#define PYUSABLE (((python_plugin) != NULL) && (python_plugin->usable()))

inline bool is_a_cycle(int motion) {
    return ((motion > G_80) && (motion < G_90)) || (motion == G_73) || (motion == G_74);
}
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


// Just set an error string using printf-style formats, do NOT return
#define ERM(fmt, ...)                                      \
    do {                                                   \
        setError (fmt, ## __VA_ARGS__);                    \
        _setup.stack_index = 0;                            \
        (rtapi_strlcpy(_setup.stack[_setup.stack_index], __PRETTY_FUNCTION__, STACK_ENTRY_LEN)); \
        _setup.stack[_setup.stack_index][STACK_ENTRY_LEN-1] = 0; \
        _setup.stack_index++; \
        _setup.stack[_setup.stack_index][0] = 0;           \
    } while(0)

// Set an error string using printf-style formats and return
#define ERS(fmt, ...)                                      \
    do {                                                   \
        setError (fmt, ## __VA_ARGS__);                    \
        _setup.stack_index = 0;                            \
        (rtapi_strlcpy(_setup.stack[_setup.stack_index], __PRETTY_FUNCTION__, STACK_ENTRY_LEN)); \
        _setup.stack[_setup.stack_index][STACK_ENTRY_LEN-1] = 0; \
        _setup.stack_index++; \
        _setup.stack[_setup.stack_index][0] = 0;           \
        return INTERP_ERROR;                               \
    } while(0)

// Return one of the very few numeric errors
#define ERN(error_code)                                    \
    do {                                                   \
        _setup.stack_index = 0;                            \
        (rtapi_strlcpy(_setup.stack[_setup.stack_index], __PRETTY_FUNCTION__, STACK_ENTRY_LEN)); \
        _setup.stack[_setup.stack_index][STACK_ENTRY_LEN-1] = 0; \
        _setup.stack_index++; \
        _setup.stack[_setup.stack_index][0] = 0;           \
        return error_code;                                 \
    } while(0)


// Propagate an error up the stack
#define ERP(error_code)                                        \
    do {                                                       \
        if (_setup.stack_index < STACK_LEN - 1) {                         \
            (rtapi_strlcpy(_setup.stack[_setup.stack_index], __PRETTY_FUNCTION__, STACK_ENTRY_LEN)); \
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


// oword warnings 
#define OERR(fmt, ...)                                      \
    do {						    \
	if (FEATURE(OWORD_WARNONLY))			    \
	    fprintf(stderr,fmt, ## __VA_ARGS__);	    \
	else						    \
	    ERS(fmt, ## __VA_ARGS__);			    \
    } while(0)


//
// The traverse (in the active plane) to the location of the canned cycle
// is different on the first repeat vs on all the following repeats.
//
// The first traverse happens in the CURRENT_CC plane (which was raised to
// the R plane earlier, if needed), followed by a traverse down to the R
// plane.
//
// All later positioning moves happen in the CLEAR_CC plane, which is
// either the R plane or the OLD_CC plane depending on G98/G99.
//

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
       if ((repeat == block->l_number) && (current_cc > r)) { \
         cycle_traverse(block, plane, aa, bb, current_cc); \
         cycle_traverse(block, plane, aa, bb, r); \
       } else { \
         /* we must be at CLEAR_CC already */ \
         cycle_traverse(block, plane, aa, bb, clear_cc); \
         if (clear_cc > r) { \
           cycle_traverse(block, plane, aa, bb, r); \
         } \
       } \
       CHP(call); \
     }

;

struct scoped_locale {
    scoped_locale(int category_, const char *locale_) : category(category_), oldlocale(setlocale(category, NULL)) { setlocale(category, locale_); }
    ~scoped_locale() { setlocale(category, oldlocale.c_str()); }
    int category;
    std::string oldlocale;
};

#define FORCE_LC_NUMERIC_C scoped_locale force_lc_numeric_c(LC_NUMERIC, "C")
#endif // INTERP_INTERNAL_HH
