#!/usr/bin/env python3

'''
    This class is used to get information from a config.ini file,
    It will return cleared information, so the checks for valid values 
    is away from the GUI code

    Copyright 2014 Norbert Schechner
    nieson@web.de

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.

'''

from linuxcnc import ini
import os
import operator

# Set up logging
from common import logger

LOG = logger.getLogger(__name__)
# Force the log level for this module
# LOG.setLevel(logger.DEBUG) # One of DEBUG, INFO, WARNING, ERROR, CRITICAL

CONFIGPATH = os.environ['CONFIG_DIR']

class GetIniInfo:

    def __init__(self):
        inipath = os.environ["INI_FILE_NAME"]
        self.inifile = ini(inipath)
        if not self.inifile:
            LOG.critical("Error, no INI File given !!")
            sys.exit()

    def get_cycle_time(self):
        temp = self.inifile.find("DISPLAY", "CYCLE_TIME")
        try:
            return int(temp)
        except:
            message = ("Wrong entry [DISPLAY] CYCLE_TIME in INI File! ")
            message += ("Will use gmoccapy default 150")
            LOG.warning(message)
            return 150

    def get_postgui_halfile(self):
        postgui_halfile = self.inifile.findall("HAL", "POSTGUI_HALFILE") or None
        return postgui_halfile

    def get_postgui_halcmds(self):
        postgui_halcmds = self.inifile.findall("HAL", "POSTGUI_HALCMD") or None
        return postgui_halcmds

    def get_preference_file_path(self):
        # we get the preference file, if there is none given in the INI
        # we use gmoccapy.pref in the config dir
        temp = self.inifile.find("DISPLAY", "PREFERENCE_FILE_PATH")
        if not temp:
            machinename = self.inifile.find("EMC", "MACHINE")
            if not machinename:
                temp = os.path.join(CONFIGPATH, "gmoccapy.pref")
            else:
                machinename = machinename.replace(" ", "_")
                temp = os.path.join(CONFIGPATH, "%s.pref" % machinename)
        LOG.info("Preference file path: %s" % temp)
        return temp

    def get_coordinates(self):
        temp = self.inifile.find("TRAJ", "COORDINATES")
        # get rid of the spaces, if there are some
        temp = temp.replace(' ','')

        if not temp:
            LOG.warning("No coordinates entry found in [TRAJ] of INI file, will use XYZ as default")
            temp = "xyz"
        return temp.lower()

    def get_joints(self):
        temp = self.inifile.find("KINS", "JOINTS")
        if not temp:
            LOG.warning("No JOINTS entry found in [KINS] of INI file, will use 3 as default")
            return (3)
        return int(temp)

    def get_axis_list(self):
        axis_list = []
        coordinates = self.get_coordinates()
        for joint, axisletter in enumerate(coordinates):
            if axisletter in axis_list:
                continue
            axis_list.append(axisletter)

        # to much axes given, can only handle 9
        if len(axis_list) > 9:
            message = _("**** gmoccapy can only handle 9 axis, but you have given {0} through your INI file. ").format(len(axis_list))
            message += _("gmoccapy will not start ****\n")
            LOG.critical(message)
            #dialogs.warning_dialog(self, _("Very critical situation"), message, sound = False)
            sys.exit()

        return axis_list

    def get_joint_axis_relation(self):
        # we will find out the relation between joint and axis.
        temp = self.inifile.find("KINS", "KINEMATICS").split()

        # follow the order given in $ man trivkins
        # Joint numbers are assigned sequentially according to  the  axis  letters
        # specified with the coordinates parameter.
        #
        # If the coordinates parameter is omitted, joint numbers are assigned
        # sequentially to every known axis letter ("xyzabcuvw").

        joint_axis_dic = {}
        coordinates = None
        for entry in temp:
            LOG.debug("Entry = {0}".format(entry))
            if "coordinates" in entry.lower():
                coordinates = entry.split("=")[1].lower()
                LOG.debug("found the following coordinates {0}".format(coordinates))

        if not coordinates:
            LOG.warning("No coordinates found in [KINS] KINEMATICS, we will use order from [TRAJ] COORDINATES.")
            coordinates = self.get_coordinates()

        # at this point we should have the coordinates of the config, we will check if the amount of
        # coordinates does match the [KINS] JOINTS part
        LOG.debug("Number of joints = {0}".format(self.get_joints()))
        LOG.debug("{0} COORDINATES found = {1}".format(len(coordinates), coordinates))

        # let us check if there are double letters, as that would be a gantry machine
        double_axis_letter = []
        for axisletter in ["x", "y", "z", "a", "b", "c", "u", "v", "w"]:
            if coordinates.count(axisletter) > 1:
                # OK we have a special case here, we need to take care off
                # i.e. a Gantry XYYZ config
                double_axis_letter.append(axisletter)
                LOG.debug("Found double letter {0}".format(double_axis_letter))

        if self.get_joints() == len(coordinates):
            prev_double_axis_leter = ""
            for joint, axisletter in enumerate(coordinates):
                if axisletter in double_axis_letter:
                    if axisletter != prev_double_axis_leter:
                        count = 0
                        prev_double_axis_leter = axisletter
                    axisletter = axisletter + str(count)
                    count += 1
                joint_axis_dic[joint] = axisletter
                LOG.debug("joint {0} = axis {1}".format(joint, joint_axis_dic[joint]))
        else:
            LOG.warning("Amount of joints from [KINS]JOINTS= is not identical with axisletters "
            "given in [TRAJ]COORDINATES or [KINS]KINEMATICS.\n"
            "Will use the old style used prior to joint axis branch merge, see man trivkins for details.\n"
            "It is strongly recommended to update your config.\n"
            "For all unused joints an entry like [JOINT_3]HOME_SEQUENCE = 0 in your "
            "INI File is needed to get the <<all homed>> signal and be able "
            "to switch to MDI or AUTO Mode.")
            for joint, axisletter in enumerate(["x", "y", "z", "a", "b", "c", "u", "v", "w"]):
                if axisletter in coordinates:
                    joint_axis_dic[joint] = axisletter
        LOG.debug(joint_axis_dic)
        #return sorted(joint_axis_dic, key=joint_axis_dic.get, reverse=False)
        return joint_axis_dic, double_axis_letter

    def get_trivial_kinematics(self):
        temp = self.inifile.find("KINS", "KINEMATICS").split()
        LOG.debug("[KINS] KINESTYPE is {0}".format(temp[0]))

        if temp[0].lower() == "trivkins":
            for element in temp:
                if "BOTH" in element.upper():
                    LOG.warning("Found kinstype=BOTH but using trivkins. "
                    "It is not recommended to do so! "
                    "Will use mode to switch between Joints and World mode, "
                    "hopefully supported by the used <<{0}>> module.".format(temp[0]))
                    return False
            return True
        else:
            LOG.debug("Will use mode to switch between Joints and World mode")
            LOG.debug("hopefully supported by the used <<{0}>> module\n".format(temp[0]))
            # I.e.
            # pumakins = 6 axis XYZABC
            # scarakins = 4 axis XYZA
            # genhexkins = 6 axis XYZABC
            return False

    def get_no_force_homing(self):
        temp = self.inifile.find("TRAJ", "NO_FORCE_HOMING")
        if not temp or temp == "0":
            return False
        return True

    def get_position_feedback_actual(self):
        temp = self.inifile.find("DISPLAY", "POSITION_FEEDBACK")
        if not temp or temp == "0":
            return True
        if temp.lower() == "actual":
            return True
        else:
            return False

    def get_lathe(self):
        temp = self.inifile.find("DISPLAY", "LATHE")
        if not temp or temp == "0":
            return False
        return True

    def get_backtool_lathe(self):
        temp = self.inifile.find("DISPLAY", "BACK_TOOL_LATHE")
        if not temp or temp == "0":
            return False
        return True

    def get_lathe_wear_offsets(self):
        temp = self.inifile.find("DISPLAY", "LATHE_WEAR_OFFSETS")
        if not temp or temp == "0":
            return False
        return True

    def get_jog_vel(self):
        # get default jog velocity
        # must convert from INI's units per second to gmoccapy's units per minute
        temp = self.inifile.find("TRAJ", "DEFAULT_LINEAR_VELOCITY")
        if not temp:
            temp = self.inifile.find("TRAJ", "MAX_LINEAR_VELOCITY" )
            if temp:
                temp = float(temp) / 2
                LOG.warning("No DEFAULT_LINEAR_VELOCITY entry found in [TRAJ] of INI file. Using half on MAX_LINEAR_VELOCITY.")
            else:
                temp = 3.0
                LOG.warning("No DEFAULT_LINEAR_VELOCITY entry found in [TRAJ] of INI file. Using default value of 180 units / min.")
        return float(temp) * 60

    def get_max_jog_vel(self):
        # get max jog velocity
        # must convert from INI's units per second to gmoccapy's units per minute
        temp = self.inifile.find("TRAJ", "MAX_LINEAR_VELOCITY")
        if not temp:
            temp = 10.0
            LOG.warning("No MAX_LINEAR_VELOCITY entry found in [TRAJ] of INI file. Using default value of 600 units / min.")
        return float(temp) * 60

    def get_default_ang_jog_vel(self):
        # get default angular jog velocity
        temp = self.inifile.find("DISPLAY", "DEFAULT_ANGULAR_VELOCITY")
        if not temp:
            temp = 360.0
            LOG.warning("No DEFAULT_ANGULAR_VELOCITY entry found in [DISPLAY] of INI file. Using default value of 360 degree / min.")
        return float(temp)

    def get_max_ang_jog_vel(self):
        # get max angular velocity
        temp = self.inifile.find("DISPLAY", "MAX_ANGULAR_VELOCITY")
        if not temp:
            temp = 3600.0
            LOG.warning("No MAX_ANGULAR_VELOCITY entry found in [DISPLAY] of INI file. Using default value of 3600 degree / min.")
        return float(temp)

    def get_min_ang_jog_vel(self):
        # get min angular velocity
        temp = self.inifile.find("DISPLAY", "MIN_ANGULAR_VELOCITY")
        if not temp:
            temp = 0.1
            LOG.warning("No MIN_ANGULAR_VELOCITY entry found in [DISPLAY] of INI file. Using default value of 0.1 degree / min.")
        return float(temp)

    def get_default_spindle_speed(self):
        # check for default spindle speed settings
        temp = self.inifile.find("DISPLAY", "DEFAULT_SPINDLE_SPEED")
        if not temp:
            temp = 300
            LOG.warning("No DEFAULT_SPINDLE_SPEED entry found in [DISPLAY] of INI file")
        return float(temp)

    def get_max_spindle_override(self):
        # check for override settings
        temp = self.inifile.find("DISPLAY", "MAX_SPINDLE_OVERRIDE")
        if not temp:
            temp = 1.0
            LOG.warning("No MAX_SPINDLE_OVERRIDE entry found in [DISPLAY] of INI file")
        return float(temp)

    def get_min_spindle_override(self):
        temp = self.inifile.find("DISPLAY", "MIN_SPINDLE_OVERRIDE")
        if not temp:
            temp = 0.1
            LOG.warning("No MIN_SPINDLE_OVERRIDE entry found in [DISPLAY] of INI file")
        return float(temp)

    def get_max_feed_override(self):
        temp = self.inifile.find("DISPLAY", "MAX_FEED_OVERRIDE")
        if not temp:
            temp = 1.0
            LOG.warning("No MAX_FEED_OVERRIDE entry found in [DISPLAY] of INI file")
        return float(temp)

    def get_embedded_tabs(self):
        # Check INI file for embed commands
        # NAME is used as the tab label if a notebook is used
        # LOCATION is the widgets name from the gladefile.
        # COMMAND is the actual program command
        # if no location is specified the main notebook is used

        tab_names = self.inifile.findall("DISPLAY", "EMBED_TAB_NAME")
        tab_location = self.inifile.findall("DISPLAY", "EMBED_TAB_LOCATION")
        tab_cmd = self.inifile.findall("DISPLAY", "EMBED_TAB_COMMAND")

        if len(tab_names) != len(tab_cmd):
            return False, False, False
        if len(tab_location) != len(tab_names):
            for num, i in enumerate(tab_names):
                try:
                    if tab_location[num]:
                        continue
                except:
                    tab_location.append("notebook_mode")
        return tab_names, tab_location, tab_cmd

    def get_parameter_file(self):
        temp = self.inifile.find("RS274NGC", "PARAMETER_FILE")
        if not temp:
            return False
        return temp

    def get_program_prefix(self):
        # and we want to set the default path
        default_path = self.inifile.find("DISPLAY", "PROGRAM_PREFIX")
        if not default_path:
            LOG.warning("Path {0} from DISPLAY , PROGRAM_PREFIX does not exist, ".format(default_path)\
            + "trying default path...")
            default_path = "~/linuxcnc/nc_files/"
            if not os.path.exists(os.path.expanduser(default_path)):
                LOG.warning("Default path to ~/linuxcnc/nc_files does not exist, setting now home as path.")
                default_path = os.path.expanduser("~/")
        return default_path

    def get_file_ext(self):
        file_ext = self.inifile.findall("FILTER", "PROGRAM_EXTENSION")
        if file_ext:
            ext_list = ["*.ngc"]
            for data in file_ext:
                raw_ext = data.split(",")
                for extension in raw_ext:
                    ext = extension.split()
                    ext_list.append(ext[0].replace(".", "*."))
        else:
            LOG.warning("Error converting the file extensions from INI file [FILTER]PROGRAM_PREFIX, "
            "using as default '*.ngc'")
            ext_list = ["*.ngc"]
        return ext_list

    def get_increments(self):
        jog_increments = []
        increments = self.inifile.find("DISPLAY", "INCREMENTS")
        if increments:
            if "," in increments:
                for i in increments.split(","):
                    jog_increments.append(i.strip())
            else:
                jog_increments = increments.split()
            jog_increments.insert(0, 0)
        else:
            jog_increments = [0, "1.000", "0.100", "0.010", "0.001"]
            LOG.warning("No default jog increments entry found in [DISPLAY] of INI file. Using default values.")
        return jog_increments

    def get_toolfile(self):
        temp = self.inifile.find("EMCIO", "TOOL_TABLE")
        if not temp:
            return False
        return temp

    def get_tool_sensor_data(self):
        xpos = self.inifile.find("TOOLSENSOR", "X")
        ypos = self.inifile.find("TOOLSENSOR", "Y")
        zpos = self.inifile.find("TOOLSENSOR", "Z")
        maxprobe = self.inifile.find("TOOLSENSOR", "MAXPROBE")
        return xpos, ypos, zpos, maxprobe

    def get_macros(self):
        # lets look in the INI file, if there are any entries
        macros = self.inifile.findall("MACROS", "MACRO")
        # If there are no entries we will return False
        if not macros:
            return False

        # we need the subroutine paths to check where to search for the macro files
        subroutine_paths = self.get_subroutine_paths()
        if not subroutine_paths:
            return False

        # we do check, if the corresponding files to the macros do exist
        checked_macros =[]
        for macro in macros:
            found = False
            for path in subroutine_paths.split(":"):
                file = path + "/" + macro.split()[0] + ".ngc"
                if os.path.isfile( file ):
                    checked_macros.append(macro)
                    found = True
                    break
            if not found: # report error!
                message = ("File %s of the macro %s could not be found. " %((str(macro.split()[0]) + ".ngc"),[macro]) )
                message += ("We searched in subdirectories: %s" %subroutine_paths.split(":"))
                LOG.info(message)

        return checked_macros

    def get_subroutine_paths(self):
        subroutines_paths = self.inifile.find("RS274NGC", "SUBROUTINE_PATH")
        if not subroutines_paths:
            message = _("No subroutine folder or program prefix is given in the INI file!")
            LOG.warning(message)
            subroutines_paths = self.get_program_prefix()
        if not subroutines_paths:
            return False
        return subroutines_paths

    def get_axis_2_min_limit(self):
        # needed to calculate the offset for automated tool measurement
        temp = self.inifile.find("AXIS_2", "MIN_LIMIT")
        if not temp:
            return False
        return float(temp)

    def get_RS274_start_code(self):
        temp = self.inifile.find("RS274NGC", "RS274NGC_STARTUP_CODE")
        if not temp:
            temp = ""
        return  temp

    def get_user_messages(self):
        message_text = self.inifile.findall("DISPLAY", "MESSAGE_TEXT")
        message_type = self.inifile.findall("DISPLAY", "MESSAGE_TYPE")
        message_pinname = self.inifile.findall("DISPLAY", "MESSAGE_PINNAME")
        if len(message_text) != len(message_type) or len(message_text) != len(message_pinname):
            LOG.warning("ERROR in user message setup")
            return None
        else:
            for element in message_pinname:
                if " " in element:
                    LOG.warning("ERROR in user message setup. Pin name should not contain spaces.")
                    return None
            messages = list(zip(message_text, message_type, message_pinname))
            return messages

    def get_machine_units(self):
        units = self.inifile.find("TRAJ", "LINEAR_UNITS")
        if units == "mm" or units == "cm" or units == "inch":
            return units
        else:
            LOG.warning("ERROR getting machine units. "
            "Please check [TRAJ] LINEAR_UNITS for a valid entry, found {0}.".format(units))
            return None

    def get_user_command_file(self):
        temp = self.inifile.find("DISPLAY", "USER_COMMAND_FILE")
        if temp:
            LOG.info("USER_COMMAND_FILE = " + temp)
        return temp

    def get_user_css_file(self):
        temp = self.inifile.find("DISPLAY", "USER_CSS_FILE")
        if temp:
            LOG.info("USER_CSS_FILE = " + temp)
        return temp
