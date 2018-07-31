#!/usr/bin/env python

'''
    This class is used to get information from a config.ini file,
    It will return cleared informations, so the checks for valid values 
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

CONFIGPATH = os.environ['CONFIG_DIR']

class GetIniInfo:

    def __init__(self):
        inipath = os.environ["INI_FILE_NAME"]
        self.inifile = ini(inipath)
        if not self.inifile:
            print("**** GMOCCAPY GETINIINFO **** \nError, no INI File given !!")
            sys.exit()

    def get_cycle_time(self):
        temp = self.inifile.find("DISPLAY", "CYCLE_TIME")
        try:
            return int(temp)
        except:
            message = ("**** GMOCCAPY GETINIINFO **** \n")
            message += ("Wrong entry [DISPLAY] CYCLE_TIME in INI File!\n")            
            message += ("Will use gmoccapy default 150")
            print(message)
            return 150

    def get_postgui_halfile(self):
        postgui_halfile = self.inifile.find("HAL", "POSTGUI_HALFILE")
        if not postgui_halfile:
            postgui_halfile = None
        return postgui_halfile

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
        print("**** GMOCCAPY GETINIINFO **** \nPreference file path: %s" % temp)
        return temp

    def get_coordinates(self):
        temp = self.inifile.find("TRAJ", "COORDINATES")
        # get rid of the spaces, if there are some
        temp = temp.replace(' ','')

        if not temp:
            print("**** GMOCCAPY GETINIINFO **** \nNo coordinates entry found in [TRAJ] of INI file, will use XYZ as default")
            temp = "xyz"
        return temp.lower()

    def get_joints(self):
        temp = self.inifile.find("KINS", "JOINTS")
        if not temp:
            print("**** GMOCCAPY GETINIINFO **** \nNo JOINTS entry found in [KINS] of INI file, will use 3 as default")
            return (3)
        return int(temp)

    def get_axis_list(self):
        axis_list = []
        coordinates = self.get_coordinates()
        for joint, axisletter in enumerate(coordinates):
            if axisletter in axis_list:
                continue
            axis_list.append(axisletter)
        return axis_list

    def get_joint_axis_relation(self):
        # we will find out the relation between joint and axis.
        # first we look if the kinematics module will be loaded with the coordinates parameter
        temp = self.inifile.find("KINS", "KINEMATICS").split()
        print("found kinematics module", temp)

        if temp[0].lower() != "trivkins":
            print("\n**** GMOCCAPY GETINIINFO **** \n[KINS] KINEMATICS is not trivkins")
            print("Will use mode to switch between Joints and World mode")
            print("hopefully supported by the used <<%s>> module\n"%temp[0])
            return None

        # follow the order given in $ man trivkins
        # Joint numbers are assigned sequentialy according to  the  axis  letters
        # specified with the coordinates= parameter.
        #
        # If the coordinates= parameter is omitted, joint numbers are assigned
        # sequentially to every known axis letter ("xyzabcuvw").

        joint_axis_dic = {}
        coordinates = None
        for entry in temp:
            print("Entry =", entry )
            if "coordinates" in entry.lower():
                coordinates = entry.split("=")[1].lower()
                print("found the following coordinates", coordinates )
            if "kinstype" in entry.lower():
                print ("found kinstype", entry.split("=")[1])
                # we will not take care of this one, because linuxcnc will take
                # care about the differences between KINEMATICS_IDENTITY and others
                # a additional check is done on some places within the gmoccapy code

        if not coordinates:
            print("no coordinates found in [KINS] KINEMATICS, we will use order from")
            print("[TRAJ] COORDINATES")
            coordinates = self.get_coordinates()

        # at this point we should have the coordinates of the config, we will check if the amount of
        # coordinates does match the [KINS] JOINTS part
        print("Number of joints = ", self.get_joints())
        print("%s COORDINATES found = %s" %(len(coordinates), coordinates))

        # let us check if there are double letters, as that would be a gantry machine
        double_axis_letter = []
        for axisletter in ["x", "y", "z", "a", "b", "c", "u", "v", "w"]:
            if coordinates.count(axisletter) > 1:
                # OK we have a special case here, we need to take care off
                # i.e. a Gantry XYYZ config
                double_axis_letter.append(axisletter)
                print("Fount double letter ", double_axis_letter)

        if self.get_joints() == len(coordinates):
            count = 0
            for joint, axisletter in enumerate(coordinates):
                if axisletter in double_axis_letter:
                    axisletter = axisletter + str(count)
                    count += 1
                joint_axis_dic[axisletter] = joint
                print("axis %s = joint %s" %(axisletter, joint_axis_dic[axisletter]))
        else:
            print("\n**** GMOCCAPY GETINIINFO **** ")
            print("Amount of joints from [KINS]JOINTS= is not identical with axisletters")
            print("given in [TRAJ]COORDINATES or [KINS]KINEMATICS")
            print("will use the old style used prior to joint axis branch merge,")
            print("see man trivkins for details")
            print("It is strongly recommended to update your config\n")
            print("\nFor all unused joints an entry like [JOINT_3]HOME_SEQUENCE = 0 in your")
            print("INI File is needed to get the <<all homed>> signal and be able")
            print("to switch to MDI or AUTO Mode\n")
            for joint, axisletter in enumerate(["x", "y", "z", "a", "b", "c", "u", "v", "w"]):
                if axisletter in coordinates:
                    joint_axis_dic[axisletter] = joint

        return joint_axis_dic

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
        # must convert from INI's units per second to gscreen's units per minute
        temp = self.inifile.find("TRAJ", "DEFAULT_LINEAR_VELOCITY")
        if not temp:
            temp = self.inifile.find("TRAJ", "MAX_LINEAR_VELOCITY" )
            if temp:
                temp = float(temp) / 2
                print("**** GMOCCAPY GETINIINFO **** \nNo DEFAULT_LINEAR_VELOCITY entry found in [TRAJ] of INI file\nUsing half on MAX_LINEAR_VELOCITY")
            else:
                temp = 3.0
                print("**** GMOCCAPY GETINIINFO **** \nNo DEFAULT_LINEAR_VELOCITY entry found in [TRAJ] of INI file\nUsing default value of 180 units / min")
        return float(temp) * 60

    def get_max_jog_vel(self):
        # get max jog velocity
        # must convert from INI's units per second to gscreen's units per minute
        temp = self.inifile.find("TRAJ", "MAX_LINEAR_VELOCITY")
        if not temp:
            temp = 10.0
            print("**** GMOCCAPY GETINIINFO **** \nNo MAX_LINEAR_VELOCITY entry found in [TRAJ] of INI file\nUsing default value of 600 units / min")
        return float(temp) * 60

    def get_default_spindle_speed(self):
        # check for default spindle speed settings
        temp = self.inifile.find("DISPLAY", "DEFAULT_SPINDLE_SPEED")
        if not temp:
            temp = 300
            print("**** GMOCCAPY GETINIINFO **** \n No DEFAULT_SPINDLE_SPEED entry found in [DISPLAY] of INI file")
        return float(temp)

    def get_max_spindle_override(self):
        # check for override settings
        temp = self.inifile.find("DISPLAY", "MAX_SPINDLE_OVERRIDE")
        if not temp:
            temp = 1.0
            print("**** GMOCCAPY GETINIINFO **** \nNo MAX_SPINDLE_OVERRIDE entry found in [DISPLAY] of INI file")
        return float(temp)

    def get_min_spindle_override(self):
        temp = self.inifile.find("DISPLAY", "MIN_SPINDLE_OVERRIDE")
        if not temp:
            temp = 0.1
            print("**** GMOCCAPY GETINIINFO **** \nNo MIN_SPINDLE_OVERRIDE entry found in [DISPLAY] of INI file")
        return float(temp)

    def get_max_feed_override(self):
        temp = self.inifile.find("DISPLAY", "MAX_FEED_OVERRIDE")
        if not temp:
            temp = 1.0
            print("**** GMOCCAPY GETINIINFO **** \nNo MAX_FEED_OVERRIDE entry found in [DISPLAY] of INI file")
        return float(temp)

    def get_max_rapid_override(self):
        temp = self.inifile.find("DISPLAY", "MAX_RAPID_OVERRIDE")
        if not temp:
            temp = 1.0
            print("**** GMOCCAPY GETINIINFO **** \nNo MAX_RAPID_OVERRIDE entry found in [DISPLAY] of INI file \n Default settings 100 % applied!")
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
            print("**** GMOCCAPY GETINIINFO **** \nPath %s from DISPLAY , PROGRAM_PREFIX does not exist" % default_path)
            print("**** GMOCCAPY GETINIINFO **** \nTrying default path...")
            default_path = "~/linuxcnc/nc_files/"
            if not os.path.exists(os.path.expanduser(default_path)):
                print("**** GMOCCAPY GETINIINFO **** \nDefault path to ~/linuxcnc/nc_files does not exist")
                print("**** GMOCCAPY GETINIINFO **** \nsetting now home as path")
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
            print("**** GMOCCAPY GETINIINFO **** \nError converting the file extensions from INI File 'FILTER','PROGRAMM_PREFIX")
            print("**** GMOCCAPY GETINIINFO **** \nusing as default '*.ngc'")
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
            print("**** GMOCCAPY GETINIINFO **** \nNo default jog increments entry found in [DISPLAY] of INI file\nUsing default values")
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
        # lets look in the ini File, if there are any entries
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
                message = ( "\n**** GMOCCAPY INFO ****\n" )
                message += ( "File %s of the macro %s could not be found ****\n" %((str(macro.split()[0]) + ".ngc"),[macro])  )
                message += ("we searched in subdirectories: %s" %subroutine_paths.split(":"))
                print (message)

        return checked_macros

    def get_subroutine_paths(self):
        subroutines_paths = self.inifile.find("RS274NGC", "SUBROUTINE_PATH")
        if not subroutines_paths:
            message = _( "**** GMOCCAPY GETINIINFO ****\n" )
            message += _( "**** No subroutine folder or program prefix is given in the ini file **** \n" )
            print( message )
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
            return False
        return  temp

    def get_user_messages(self):
        message_text = self.inifile.findall("DISPLAY", "MESSAGE_TEXT")
        message_type = self.inifile.findall("DISPLAY", "MESSAGE_TYPE")
        message_pinname = self.inifile.findall("DISPLAY", "MESSAGE_PINNAME")
        if len(message_text) != len(message_type) or len(message_text) != len(message_pinname):
            print("**** GMOCCAPY GETINIINFO **** \nERROR in user message setup")
            return None
        else:
            for element in message_pinname:
                if " " in element:
                    print("**** GMOCCAPY GETINIINFO **** \nERROR in user message setup \nPinname should not contain spaces")
                    return None
            messages = zip(message_text, message_type, message_pinname)
            return messages
