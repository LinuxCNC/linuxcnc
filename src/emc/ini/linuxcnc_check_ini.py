#!/usr/bin/env python3
#
# Check the INI-file configuration
# Copyright (C) 2026 B. Stultiens
#
# Based on check_config.tcl
#
# This program is free software; you can redistribute it and/or modify it under
# the terms of the GNU General Public License as published by the Free Software
# Foundation; either version 2 of the License, or (at your option) any later
# version.
#
# This program is distributed in the hope that it will be useful, but WITHOUT
# ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
# FOR A PARTICULAR PURPOSE. See the GNU General Public License for more
# details.
#
# You should have received a copy of the GNU General Public License along with
# this program; if not, write to the Free Software Foundation, Inc., 51
# Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
#
import sys
import os
import getopt
import linuxcnc

inifilename = ""
error_on_warning = False

# See: emc/motion/emcmotcfg.h
EMCMOT_MAX_JOINTS   = 16
EMCMOT_MAX_AXIS     = 9 # XYZABCUVW
EMCMOT_MAX_SPINDLES = 8

# See: emc/nml_intf/emccfg.h
DEFAULT_AXIS_MAX_VELOCITY      = 1.0
DEFAULT_AXIS_MAX_ACCELERATION  = 1.0
DEFAULT_AXIS_MAX_JERK          = 0.0
DEFAULT_AXIS_MIN_LIMIT         = -1e99
DEFAULT_AXIS_MAX_LIMIT         = +1e99
DEFAULT_JOINT_MAX_VELOCITY     = 1.0
DEFAULT_JOINT_MAX_ACCELERATION = 1.0
DEFAULT_JOINT_MAX_JERK         = 0.0
DEFAULT_JOINT_MIN_LIMIT        = -1e99
DEFAULT_JOINT_MAX_LIMIT        = +1e99


def usage():
    print("""Check LinuxCNC INI-file configuration.
Usage:
  linuxcnc_check_ini [-e] [-h] inifile.ini

Options:
  -e|--error    Treat warnings as errors
  -h|--help     This message
""")
    sys.exit(2)

imessages = []
wmessages = []
emessages = []
#
# Collect messages
#
def perr(msg):
    global emessages
    emessages.append("{}: error: {}".format(inifilename, msg))

def pwarn(msg):
    global wmessages
    wmessages.append("{}: warning: {}".format(inifilename, msg))

def pmsg(msg):
    global imessages
    imessages.append("{}: info: {}".format(inifilename, msg))

#
# Flush the messages to stderr. Returns zero only when no error messages are
# present and no warnings are present when treated as errors.
#
def flush_messages():
    for m in imessages:
        print(m, file=sys.stderr);
    for m in wmessages:
        print(m, file=sys.stderr);
    for m in emessages:
        print(m, file=sys.stderr);
    if len(emessages) > 0 or (error_on_warning and len(wmessages) > 0):
        return 1
    else:
        return 0

#
# Check the ini-file for mandatory settings and the type of those settings
#
def check_mandatory_items():
    # These must be present in the INI-file
    MANDATORY_ITEMS = [
        # Use None for Mini and/or Maxi if not used
        # Section, Variable, Type(s,r,i,u,b), Mini, Maxi
        ("KINS", "KINEMATICS", 's', None, None),
        ("KINS", "JOINTS",     'i', 0,    EMCMOT_MAX_JOINTS)
    ]

    rv = True
    for s, v, t, mini, maxi in MANDATORY_ITEMS:
        if not ini.hasvariable(s, v):
            perr("[{}]{}: Missing entry".format(s, v))
            rv = False
        else:
            if 's' == t:
                if not ini.getstring(s, v):    # There must be string content
                    perr("[{}]{}: Expected a string value".format(s, v))
                    rv = False
            elif 'i' == t:
                val = ini.getsint(s, v)
                if None == val:
                    perr("[{}]{}: Expected an integer value".format(s, v))
                    rv = False
                if (None != mini and val < mini) or (None != maxi and val > maxi):
                    perr("[{}]{}: Integer value '{}' out of range [{},{}]".format(s, v, val, mini, maxi))
                    rv = False
            elif 'u' == t:
                val = ini.getuint(s, v)
                if None == val:
                    perr("[{}]{}: Expected an unsigned value".format(s, v))
                    rv = False
                if (None != mini and val < mini) or (None != maxi and val > maxi):
                    perr("[{}]{}: Unsigned value '{}' out of range [{},{}]".format(s, v, val, mini, maxi))
                    rv = False
            elif 'b' == t:
                if None == ini.getbool(s, v):
                    perr("[{}]{}: Expected an boolean value".format(s, v))
                    rv = False
            elif 'r' == t:
                val = ini.getreal(s, v)
                if None == val:
                    perr("[{}]{}: Expected a real value".format(s, v))
                    rv = False
                if (None != mini and val < mini) or (None != maxi and val > maxi):
                    perr("[{}]{}: Real value '{}' out of range [{},{}]".format(s, v, val, mini, maxi))
                    rv = False
            else:
                perr("Internal error: check_mandatory_items(): invalid type '{}' for check".format(t))
                return False
    return rv

#
# Get and split a value of a variable in the specified section according to:
# [SECTION]VARIABLE = base [opt=val [opt=val [...]]]
#
def split_opts(section, variable):
    parts = ini.getstring(section, variable, fallback="").split()
    opts = {}
    if len(parts) < 1:
        perr("[{}]{}: Missing content".format(section, variable))
        return (None, None)
    for opt in parts[1:]:
        if '=' in opt:
            o, v = opt.split("=")
            if o in opts:
                perr("[{}}{}: Duplicate option key '{}'".format(section, variable, o))
                return (None, None)
            opts[o] = v
        else:
            perr("[{}]{}: Option '{}' is missing a '=' and value".format(section, variable, opt))
            return (None, None)
    return (parts[0], opts)

#
# Checks the enumerated values of:
# - [TRAJ]LINEAR_UNITS
# - [TRAJ]ANGULAR_UNITS
# - [AXIS_*]TYPE
# - [JOINT_*]TYPE
# - [DISPLAY]POSITION_OFFSET
# - [DISPLAY]POSITION_FEEDBACK
# - [EMC]RCS_DEBUG_DEST
#
def check_enums():
    # Linear and angular units enums
    if not ini.hasvariable("TRAJ", "LINEAR_UNITS"):
        perr("[TRAJ]LINEAR_UNITS: Missing")
    else:
        if None == ini.getlinearunits("TRAJ", "LINEAR_UNITS"):
            perr("[TRAJ]LINEAR_UNITS: Must be one of [mm,metric,in,inch,imperial]")

    if not ini.hasvariable("TRAJ", "ANGULAR_UNITS"):
        perr("[TRAJ]ANGULAR_UNITS: Missing")
    else:
        if None == ini.getangularunits("TRAJ", "ANGULAR_UNITS"):
            perr("[TRAJ]ANGULAR_UNITS: Must be one of [deg,degree,grad,gon,rad,radian]")

    # Get all JOINT_* and AXIS_* section names for the type
    sects = list(filter(lambda s: (s.startswith("JOINT_") or s.startswith("AXIS_")), ini.getsections()))
    for sec in sects:
        # If it is undefined, it will have a default
        # Joints map to the axis default if not set
        if not ini.hasvariable(sec, "TYPE"):
            continue
        # But if set, it must be the correct enum
        if None == ini.getjointtype(sec, "TYPE"):
            perr("[{}]TYPE: Must be one of [LINEAR,ANGULAR]".format(sec))

    if ini.hasvariable("DISPLAY", "POSITION_OFFSET"):
        if ini.getstring("DISPLAY", "POSITION_OFFSET").upper() not in ["RELATIVE", "MACHINE"]:
            perr("[DISPLAY]POSITION_OFFSET: Must be one of [RELATIVE,MACHINE]")
    if ini.hasvariable("DISPLAY", "POSITION_FEEDBACK"):
        if ini.getstring("DISPLAY", "POSITION_FEEDBACK").upper() not in ["COMMANDED", "ACTUAL"]:
            perr("[DISPLAY]POSITION_FEEDBACK: Must be one of [COMMANDED,ACTUAL]")

    if ini.hasvariable("EMC", "RCS_DEBUG_DEST"):
        if ini.getstring("EMC", "RCS_DEBUG_DEST").upper() not in ["NULL", "STDOUT", "STDERR", "FILE", "LOGGER", "MSGBOX"]:
            perr("[EMC]RCS_DEBUG_DEST: Must be one of [NULL,STDOUT,STDERR,FILE,LOGGER,MSGBOX]")

#
# Check a set of variables for having the right type if they are present
#
def check_bool(s, v):
    if ini.hasvariable(s, v):
        if None == ini.getbool(s, v):
            perr("[{}]{}: Invalid boolean value".format(s, v))

def check_bools():
    BOOLVARS = [("TRAJ", "NO_FORCE_HOMING"),
                ("TRAJ", "ARC_BLEND_ENABLE"),
                ("TRAJ", "ARC_BLEND_FALLBACK_ENABLE"),
                ("EMCIO", "TOOL_CHANGE_WITH_SPINDLE_ON"),
                ("EMCIO", "TOOL_CHANGE_QUILL_UP"),
                ("EMCIO", "TOOL_CHANGE_AT_G30"),
                ("EMCIO", "RANDOM_TOOLCHANGER"),
                ("RS274NGC", "INI_VARS"),
                ("RS274NGC", "HAL_PIN_VARS"),
                ("RS274NGC", "RETAIN_G43"),
                ("RS274NGC", "OWORD_NARGS"),
                ("RS274NGC", "NO_DOWNCASE_OWORD"),
                ("RS274NGC", "OWORD_WARN_ONLY"),
                ("RS274NGC", "DISABLE_G92_PERSISTENCE"),
                ("RS274NGC", "DISABLE_AUTO_G54"),
                ("RS274NGC", "DISABLE_FANUC_STYLE_SUB"),
                ("DISPLAY", "DISABLE_CONE_SCALING"),
                ("DISPLAY", "HOMING_PROMPT"),
                ("DISPLAY", "LATHE"),
                ("DISPLAY", "BACK_TOOL_LATHE"),
                ("DISPLAY", "FOAM")
        ]
    # Variables in [AXIS_*]
    BOOLAVARS = ["WRAPPED_ROTARY"]
    # Variables in [JOINT_*]
    BOOLJVARS = ["LOCKING_INDEXER",
                 "HOME_USE_INDEX",
                 "HOME_INDEX_NO_ENCODER_RESET",
                 "HOME_IGNORE_LIMITS",
                 "HOME_IS_SHARED",
                 "VOLATILE_HOME"
        ]
    for s, v in BOOLVARS:
        check_bool(s, v)
    for asect in list(filter(lambda s: s.startswith("AXIS_"), ini.getsections())):
        for v in BOOLAVARS:
            check_bool(asect, v)
    for jsect in list(filter(lambda s: s.startswith("JOINT_"), ini.getsections())):
        for v in BOOLJVARS:
            check_bool(jsect, v)

#
# Integer checks also include a range check
#
def check_int(s, v, mini, maxi):
    if ini.hasvariable(s, v):
        val = ini.getsint(s, v)
        if None == val:
            perr("[{}]{}: Invalid integer value".format(s, v))
        if (None != mini and val < mini) or (None != maxi and val > maxi):
            if None == mini: mini = ""
            if None == maxi: maxi = ""
            perr("[{}]{}: Integer value '{}' out of range [{},{}]".format(s, v, val, mini, maxi))

def check_ints():
    INTVARS = [
        # section, variable, minimum, maximum
        # Use None for min/max if not used
        ("EMCMOT", "BASE_PERIOD",  0, None),
        ("EMCMOT", "SERVO_PERIOD", 0, None),
        ("EMCMOT", "TRAJ_PERIOD",  0, None),
        ("TRAJ",   "SPINDLES",     0, EMCMOT_MAX_SPINDLES),
        ("TRAJ",   "PLANNER_TYPE", 0, 1)
    ]
    # Variables in [JOINT_*]
    INTJVARS = [
        ("COMP_FILE_TYPE",        0, 1),
        ("HOME_ABSOLUTE_ENCODER", 0, 2)
    ]
    for s, v, mi, ma in INTVARS:
        check_int(s, v, mi, ma)
    for jsect in list(filter(lambda s: s.startswith("JOINT_"), ini.getsections())):
        for v, mi, ma in INTJVARS:
            check_int(jsect, v, mi, ma)

#
# Kinematics is specified as:
# [KINS]KINEMATICS = module [coordinates=<coords>] [kinstype=<type>] [...]
#
def parse_kinematics():
    kins, opts = split_opts("KINS", "KINEMATICS")
    if None == kins:
        return (None, None)
    # Check the axis->joint mapping
    if "coordinates" in opts:
        for c in opts["coordinates"].upper():
            if c not in "XYZABCUVW":
                perr("Invalid axis '{}' in kinematics coordinates option".format(c))
                return (None, None)
    return (kins, opts)

#
# Motion controller is specified like:
# [EMCMOT]EMCMOT = module [num_extrajoints=N] [unlock_joints_mask=N] [...]
#
def check_extrajoints():
    if not ini.hasvariable("EMCMOT", "EMCMOT"):
        return  # Will default in the linuxcnc script
    motmod, motopts = split_opts("EMCMOT", "EMCMOT")
    if None == motmod:
        return  # Message was emitted in split_opts()

    if "num_extrajoints" in motopts:
        pmsg("Extra joints specified={}; [KINS]JOINTS={} must accommodate kinematic joints *plus* extra joints"
                .format(motopts["num_extrajoints"], ini.find("KINS", "JOINTS")))

#
# Traverse all JOINT_* and AXIS_* sections and see if there are any duplicate
# variables in them. It could be a serious problem if duplicates are detected.
#
def warn_for_multiple_ini_values():
    # Select all JOINT_* and AXIS_* sections
    # We could be more specific with a regex, but that is optional...
    sects = list(filter(lambda s: (s.startswith("JOINT_") or s.startswith("AXIS_")), ini.getsections()))
    for sec in sects:
        # Get all variable names from the section
        # ini.getvariables returns list of tuples (name,value)
        varnames = [n for n,v in ini.getvariables(sec)]
        # Get the set of duplicates
        dups = [x for x in set(varnames) if varnames.count(x) > 1]
        # Warn all duplicates
        for d in dups:
            v = ini.findall(sec, d) # Get all values to show the values
            pwarn("[{}]{}: Duplicate entry found, values: {}".format(sec, d, v))

#
# Check maximum joint and axis velocity/acceleration
# Check consistency of axis/joint min/max limits
#
def validate_identity_kins_limits(coords, kinematics):
    planjerk = ini.getuint("TRAJ", "PLANNER_TYPE", fallback=0) > 0
    # Check joints velocity and acceleration
    for joint in range(0, ini.getsint("KINS", "JOINTS")):
        jsec = "JOINT_{}".format(joint)
        if not ini.hasvariable(jsec, "MAX_VELOCITY"):
            pwarn("[{}]MAX_VELOCITY: Unspecified, default used: {}".format(jsec, DEFAULT_JOINT_MAX_VELOCITY))
        if not ini.hasvariable(jsec, "MAX_ACCELERATION"):
            pwarn("[{}]MAX_ACCELERATION: Unspecified, default used: {}".format(jsec, DEFAULT_JOINT_MAX_ACCELERATION))
        if  planjerk and not ini.hasvariable(jsec, "MAX_JERK"):
            pwarn("[{}]MAX_JERK: Unspecified, default used: {}".format(jsec, DEFAULT_JOINT_MAX_JERK))

    # Check axis velocity and acceleration
    for a in list(set(coords)): # Make axis letters unique
        asec = "AXIS_{}".format(a)
        if not ini.hassection(asec):
            continue
        if not ini.hasvariable(asec, "MAX_VELOCITY"):
            pwarn("[{}]MAX_VELOCITY: Unspecified, default used: {}".format(asec, DEFAULT_AXIS_MAX_VELOCITY))
        if not ini.hasvariable(asec, "MAX_ACCELERATION"):
            pwarn("[{}]MAX_ACCELERATION: Unspecified, default used: {}".format(asec, DEFAULT_AXIS_MAX_ACCELERATION))
        if  planjerk and not ini.hasvariable(asec, "MAX_JERK"):
            pwarn("[{}]MAX_JERK: Unspecified, default used: {}".format(asec, DEFAULT_AXIS_MAX_JERK))

        # Check limits
        # Iterate all joints of axes of the same name (f.ex. XYYZ for Y gives [1, 2])
        for joint in [u for u,v in filter(lambda x: x[1] == a, enumerate(coords))]:
            jsec = "JOINT_{}".format(joint)
            jlim = ini.getreal(jsec, "MIN_LIMIT", fallback=DEFAULT_JOINT_MIN_LIMIT)
            alim = ini.getreal(asec, "MIN_LIMIT", fallback=DEFAULT_AXIS_MIN_LIMIT)
            if jlim > alim:
                if not ini.hasvariable(asec, "MIN_LIMIT"):
                    pwarn("[{}]MIN_LIMIT: Unspecified, default used: {}".format(asec, alim))
                perr("[{}]MIN_LIMIT > [{}]MIN_LIMIT ({} > {})".format(jsec, asec, jlim, alim))
            jlim = ini.getreal(jsec, "MAX_LIMIT", fallback=DEFAULT_JOINT_MAX_LIMIT)
            alim = ini.getreal(asec, "MAX_LIMIT", fallback=DEFAULT_AXIS_MAX_LIMIT)
            if jlim < alim:
                if not ini.hasvariable(asec, "MAX_LIMIT"):
                    pwarn("[{}]MAX_LIMIT: Unspecified, default used: {}".format(asec, alim))
                perr("[{}]MAX_LIMIT < [{}]MAX_LIMIT ({} < {})".format(jsec, asec, jlim, alim))

#
# Ensure coordinates are consistent
#
def consistent_coords_for_trivkins(coords):
    if "XYZABCUVW" == coords:
        return  # No coordinates were specified

    trajcoords = ini.getstring("TRAJ", "COORDINATES", fallback="").replace(" ", "").replace("\t", "").upper()
    if coords != trajcoords:
        pwarn("Inconsistent coordinates specifications: trivkins coordinates={} vs. [TRAJ]COORDINATES={}"
            .format(coords, trajcoords))

#
# Main program entry
#
def main():
    # Get the command-line options
    global progname
    progname = os.path.basename(sys.argv[0])

    try:
        opts, args = getopt.getopt(sys.argv[1:], "eh", ["error", "help"])
    except getopt.GetoptError as err:
        print(err, file=sys.stderr)  # Something like "option -a not recognized"
        sys.exit(2)

    for o, a in opts:
        if o in ("-e", "--error"):
            global error_on_warning
            error_on_warning = True
        if o in ("-h", "--help"):
            usage() # no return from here
        else:
            print("Unhandled option: '{}'".format(o), file=sys.stderr);
            sys.exit(2)

    if 1 != len(args):
        print("Must have exactly one argument specifying the INI-file path.", file=sys.stderr)
        sys.exit(2)

    # Open the INI-file
    global inifilename, ini
    inifilename = args[0]
    try:
        ini = linuxcnc.ini(inifilename)
    except linuxcnc.error as err:
        print(err, file=sys.stderr)
        sys.exit(2)

    # From here on we collect message using perr(), pwarn() and pmsg(). The
    # messages get flushed when we are done. The program's return value depends
    # on whether there are errors or not.

    # Check 1: Check mandatory items that must exist
    if not check_mandatory_items():
        return  # No point in continuing when this fails

    # Check 2: Check all JOINT_* and AXIS_* sections for duplicate variables
    warn_for_multiple_ini_values()  # we accept warnings

    # Check 3: Check variable types and value ranges
    check_enums()
    check_bools()
    check_ints()

    # Check 4: Warn when num_extrajoints is specified
    check_extrajoints() # An error message exists if parsing [EMCMOT]EMCMOT failed

    # Check 5: Get the [KINS]KINEMATICS
    kinematics, kinsopts = parse_kinematics()
    if None == kinematics:
        return  # There is no point in continuing if parsing [KINS]KINEMATICS failed

    # Only trivial kinematics are checked. Others need different checks
    if "trivkins" == kinematics:
        # Provide a full coordinate string if not defined in kinematics entry
        if "coordinates" in kinsopts:
            coords = kinsopts["coordinates"].upper()
        else:
            coords = "XYZABCUVW"

        # Check trivkins 1: Velocity, acceleration and min/max limits
        validate_identity_kins_limits(coords, kinematics)

        # Check trivkins 2: Ensure coordinate specification consistency
        consistent_coords_for_trivkins(coords)
    else:
        pmsg("[KINS]KINEMATICS={}: Unchecked".format(kinematics))

    return

if __name__ == "__main__":
    main()
    sys.exit(flush_messages())  # Exit value depends on presence of error messages
