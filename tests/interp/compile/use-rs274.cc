//    Copyright © 2016 Jeff Epler <jepler@unpythonic.net>
//
//    This program is free software; you can redistribute it and/or modify
//    it under the terms of the GNU General Public License as published by
//    the Free Software Foundation; either version 2 of the License, or
//    (at your option) any later version.
//
//    This program is distributed in the hope that it will be useful,
//    but WITHOUT ANY WARRANTY; without even the implied warranty of
//    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//    GNU General Public License for more details.
//
//    You should have received a copy of the GNU General Public License
//    along with this program; if not, write to the Free Software
//    Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.

#include <Python.h> // must be first header
#include "rs274ngc.hh"

static void read_execute(InterpBase *b, const char *line) {
    fprintf(stderr, "> %s\n", line);
    int r = b->read(line);
    r = b->execute();
}

int main() {
    InterpBase *b = makeInterp();
    b->init();
    read_execute(b, "(this is a comment)");
    read_execute(b, "G0X0Y0");
    read_execute(b, "F100");
    read_execute(b, "G5.2 X3.53   Y-1.50   P2");
    read_execute(b, "     X5.33   Y-11.01  P1");
    read_execute(b, "     X3.52   Y-24.00  P1");
    read_execute(b, "     X0.0    Y-29.56  P1");
    read_execute(b, "G5.3");
    read_execute(b, "(second comment)");
    read_execute(b, "M2");
    return 0;
}

// this is junk that you have to define in exactly this way because of how mah
// implemented the python "remap" functionality of the interpreter
// (and it needs Python.h for the definition of struct inttab)
int _task = 0;
char _parameter_file_name[LINELEN];
extern "C" void initinterpreter();
extern "C" void initemccanon();
extern "C" struct _inittab builtin_modules[];
struct _inittab builtin_modules[] = {
    { (char *) "interpreter", initinterpreter },
    { (char *) "emccanon", initemccanon },
    { NULL, NULL }
};

// everything below here is stuff that needs a real implementation, not a dummy
// one
void INIT_CANON() {}
void SET_G5X_OFFSET(int origin,
                           double x, double y, double z,
                           double a, double b, double c,
                           double u, double v, double w) {}
void SET_G92_OFFSET(double x, double y, double z,
                           double a, double b, double c,
                           double u, double v, double w) {}
void SET_XY_ROTATION(double t) {}
void CANON_UPDATE_END_POINT(double x, double y, double z, 
				   double a, double b, double c,
				   double u, double v, double w) {}
void USE_LENGTH_UNITS(CANON_UNITS u) {}
void SELECT_PLANE(CANON_PLANE pl) {}
void SET_TRAVERSE_RATE(double rate) {}
void STRAIGHT_TRAVERSE(int lineno,
                              double x, double y, double z,
			      double a, double b, double c,
                              double u, double v, double w) {}
void SET_FEED_RATE(double rate) {}
void SET_FEED_REFERENCE(CANON_FEED_REFERENCE reference) {}
void SET_FEED_MODE(int spindle, int mode) {}
void SET_MOTION_CONTROL_MODE(CANON_MOTION_MODE mode, double tolerance) {}
void SET_NAIVECAM_TOLERANCE(double tolerance) {}
void SET_CUTTER_RADIUS_COMPENSATION(double radius) {}
void START_CUTTER_RADIUS_COMPENSATION(int direction) {}
void STOP_CUTTER_RADIUS_COMPENSATION() {}
void START_SPEED_FEED_SYNCH(int spindle, double feed_per_revolution, bool velocity_mode) {}
void STOP_SPEED_FEED_SYNCH() {}
void ARC_FEED(int lineno,
                     double first_end, double second_end,
		     double first_axis, double second_axis, int rotation,
		     double axis_end_point, 
                     double a, double b, double c,
                     double u, double v, double w) {}
void STRAIGHT_FEED(int lineno,
                          double x, double y, double z,
                          double a, double b, double c,
                          double u, double v, double w) {
    printf("-> %.1f %.1f\n", x, y);
}
void NURBS_FEED(int lineno, std::vector<CONTROL_POINT> nurbs_control_points, unsigned int k) {
    double u = 0.0;
    unsigned int n = nurbs_control_points.size() - 1;
    double umax = n - k + 2;
    unsigned int div = nurbs_control_points.size()*3;
    std::vector<unsigned int> knot_vector = knot_vector_creator(n, k);
    PLANE_POINT P1;
    while (u+umax/div < umax) {
        PLANE_POINT P1 = nurbs_point(u+umax/div,k,nurbs_control_points,knot_vector);
        STRAIGHT_FEED(lineno, P1.X,P1.Y, 0., 0.,0.,0.,  0.,0.,0.);
        u = u + umax/div;
    }
    P1.X = nurbs_control_points[n].X;
    P1.Y = nurbs_control_points[n].Y;
    STRAIGHT_FEED(lineno, P1.X,P1.Y, 0., 0.,0.,0.,  0.,0.,0.);
    knot_vector.clear();
}
void RIGID_TAP(int lineno,
                      double x, double y, double z, double scale) {}
void STRAIGHT_PROBE(int lineno,
                           double x, double y, double z,
                           double a, double b, double c,
                           double u, double v, double w, unsigned char probe_type) {}
void STOP() {}
void DWELL(double seconds) {}
void SET_SPINDLE_MODE(int spindle, double r) {}
void SPINDLE_RETRACT_TRAVERSE() {}
void START_SPINDLE_CLOCKWISE(int spindle, int dir) {}
void START_SPINDLE_COUNTERCLOCKWISE(int spindle, int dir) {}
void SET_SPINDLE_SPEED(int spindle, double r) {}
void STOP_SPINDLE_TURNING(int spindle) {}
void SPINDLE_RETRACT() {}
void ORIENT_SPINDLE(int spindle, double orientation, int mode) {}
void WAIT_SPINDLE_ORIENT_COMPLETE(int spindle, double timeout) {}
void LOCK_SPINDLE_Z() {}
void USE_SPINDLE_FORCE() {}
void USE_NO_SPINDLE_FORCE() {}
void SET_TOOL_TABLE_ENTRY(int pocket, int toolno, EmcPose offset, double diameter,
                                 double frontangle, double backangle, int orientation) {}
void USE_TOOL_LENGTH_OFFSET(EmcPose offset) {}
void CHANGE_TOOL(int slot) {}	
void SELECT_POCKET(int pocket, int tool) {}	
void CHANGE_TOOL_NUMBER(int number) {}
void START_CHANGE(void) {}
void CLAMP_AXIS(CANON_AXIS axis) {}
void COMMENT(const char *s) { puts(s); }
void DISABLE_ADAPTIVE_FEED() {}
void ENABLE_ADAPTIVE_FEED() {}
void DISABLE_FEED_OVERRIDE() {}
void ENABLE_FEED_OVERRIDE() {}
void DISABLE_SPEED_OVERRIDE(int spindle) {}
void ENABLE_SPEED_OVERRIDE(int spindle) {}
void DISABLE_FEED_HOLD() {}
void ENABLE_FEED_HOLD() {}
void FLOOD_OFF() {}
void FLOOD_ON() {}
void MESSAGE(char *s) {}
void LOG(char *s) {}
void LOGOPEN(char *s) {}
void LOGAPPEND(char *s) {}
void LOGCLOSE() {}
void MIST_OFF() {}
void MIST_ON() {}
void PALLET_SHUTTLE() {}
void TURN_PROBE_OFF() {}
void TURN_PROBE_ON() {}
void UNCLAMP_AXIS(CANON_AXIS axis) {}
void NURB_KNOT_VECTOR() {}	
void NURB_CONTROL_POINT(int i, double x, double y, double z,
			       double w) {}
void NURB_FEED(double sStart, double sEnd) {}
void SET_BLOCK_DELETE(bool enabled) {}
bool GET_BLOCK_DELETE(void) {}
void OPTIONAL_PROGRAM_STOP() {}
void SET_OPTIONAL_PROGRAM_STOP(bool state) {}
bool GET_OPTIONAL_PROGRAM_STOP() {}
void PROGRAM_END() {}
void PROGRAM_STOP() {}
void SET_MOTION_OUTPUT_BIT(int index) {}
void CLEAR_MOTION_OUTPUT_BIT(int index) {}
void SET_AUX_OUTPUT_BIT(int index) {}
void CLEAR_AUX_OUTPUT_BIT(int index) {}
void SET_MOTION_OUTPUT_VALUE(int index, double value) {}
void SET_AUX_OUTPUT_VALUE(int index, double value) {}
int WAIT(int index, 
		int input_type, 
	        int wait_type, 
                double timeout) {} 
int UNLOCK_ROTARY(int line_no, int axis) {}
int LOCK_ROTARY(int line_no, int axis) {}
double GET_EXTERNAL_FEED_RATE() {}
int GET_EXTERNAL_FLOOD() {}
CANON_UNITS GET_EXTERNAL_LENGTH_UNIT_TYPE() {}
double GET_EXTERNAL_LENGTH_UNITS() {}
double GET_EXTERNAL_ANGLE_UNITS() {}
int GET_EXTERNAL_MIST() {}
CANON_MOTION_MODE GET_EXTERNAL_MOTION_CONTROL_MODE() {}
double GET_EXTERNAL_MOTION_CONTROL_TOLERANCE() {}
void GET_EXTERNAL_PARAMETER_FILE_NAME(char *filename, int max_size) {
    snprintf(filename, max_size, "%s", "rs274ngc.var");
}
CANON_PLANE GET_EXTERNAL_PLANE() {}
double GET_EXTERNAL_POSITION_A() {}
double GET_EXTERNAL_POSITION_B() {}
double GET_EXTERNAL_POSITION_C() {}
double GET_EXTERNAL_POSITION_X() {}
double GET_EXTERNAL_POSITION_Y() {}
double GET_EXTERNAL_POSITION_Z() {}
double GET_EXTERNAL_POSITION_U() {}
double GET_EXTERNAL_POSITION_V() {}
double GET_EXTERNAL_POSITION_W() {}
double GET_EXTERNAL_PROBE_POSITION_A() {}
double GET_EXTERNAL_PROBE_POSITION_B() {}
double GET_EXTERNAL_PROBE_POSITION_C() {}
double GET_EXTERNAL_PROBE_POSITION_X() {}
double GET_EXTERNAL_PROBE_POSITION_Y() {}
double GET_EXTERNAL_PROBE_POSITION_Z() {}
double GET_EXTERNAL_PROBE_POSITION_U() {}
double GET_EXTERNAL_PROBE_POSITION_V() {}
double GET_EXTERNAL_PROBE_POSITION_W() {}
double GET_EXTERNAL_PROBE_VALUE() {}
int GET_EXTERNAL_PROBE_TRIPPED_VALUE() {}
int GET_EXTERNAL_QUEUE_EMPTY() {}
double GET_EXTERNAL_SPEED(int spindle) {}
CANON_DIRECTION GET_EXTERNAL_SPINDLE(int spindle) {}
double GET_EXTERNAL_TOOL_LENGTH_XOFFSET() {}
double GET_EXTERNAL_TOOL_LENGTH_YOFFSET() {}
double GET_EXTERNAL_TOOL_LENGTH_ZOFFSET() {}
double GET_EXTERNAL_TOOL_LENGTH_AOFFSET() {}
double GET_EXTERNAL_TOOL_LENGTH_BOFFSET() {}
double GET_EXTERNAL_TOOL_LENGTH_COFFSET() {}
double GET_EXTERNAL_TOOL_LENGTH_UOFFSET() {}
double GET_EXTERNAL_TOOL_LENGTH_VOFFSET() {}
double GET_EXTERNAL_TOOL_LENGTH_WOFFSET() {}
int GET_EXTERNAL_POCKETS_MAX() {}
int GET_EXTERNAL_TOOL_SLOT() {}
int GET_EXTERNAL_SELECTED_TOOL_SLOT() {}
CANON_TOOL_TABLE GET_EXTERNAL_TOOL_TABLE(int pocket) {}
int GET_EXTERNAL_TC_FAULT() {}
int GET_EXTERNAL_TC_REASON() {}
double GET_EXTERNAL_TRAVERSE_RATE() {}
int GET_EXTERNAL_FEED_OVERRIDE_ENABLE() {}
int GET_EXTERNAL_SPINDLE_OVERRIDE_ENABLE(int spindle) {}
int GET_EXTERNAL_ADAPTIVE_FEED_ENABLE() {}
int GET_EXTERNAL_FEED_HOLD_ENABLE() {}
int GET_EXTERNAL_DIGITAL_INPUT(int index, int def) {}
double GET_EXTERNAL_ANALOG_INPUT(int index, double def) {}
int GET_EXTERNAL_AXIS_MASK() { return 7; }
void FINISH(void) {}
void CANON_ERROR(const char *fmt, ...) {}
void PLUGIN_CALL(int len, const char *call) {}
void IO_PLUGIN_CALL(int len, const char *call) {}
USER_DEFINED_FUNCTION_TYPE
    USER_DEFINED_FUNCTION[USER_DEFINED_FUNCTION_NUM];
int GET_EXTERNAL_OFFSET_APPLIED() {};
EmcPose GET_EXTERNAL_OFFSETS(){};
