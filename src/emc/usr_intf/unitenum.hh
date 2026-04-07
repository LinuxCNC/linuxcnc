#ifndef __LINUXCNC_USR_INTF_UNITENUM_HH
#define __LINUXCNC_USR_INTF_UNITENUM_HH

// Removed definition from shcom.hh and moved to here so that mapini.{cc,hh}
// can use them without pulling in other stuff.
// Removed a local redefinition copy from halui.cc that now can use this
// include instead.

enum LINEAR_UNIT_CONVERSION : int {
    LINEAR_UNITS_CUSTOM = 1,
    LINEAR_UNITS_AUTO,
    LINEAR_UNITS_MM,
    LINEAR_UNITS_INCH,
    LINEAR_UNITS_CM
};

enum ANGULAR_UNIT_CONVERSION : int {
    ANGULAR_UNITS_CUSTOM = 1,
    ANGULAR_UNITS_AUTO,
    ANGULAR_UNITS_DEG,
    ANGULAR_UNITS_RAD,
    ANGULAR_UNITS_GRAD
};

#endif
