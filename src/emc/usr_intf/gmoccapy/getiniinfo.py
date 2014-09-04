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
    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA

'''

from linuxcnc import ini
import os

CONFIGPATH = os.environ['CONFIG_DIR']

class GetIniInfo:

    def __init__(self):
        inipath = os.environ["INI_FILE_NAME"]
        self.inifile = ini(inipath)
        if not self.inifile:
            print("**** GMOCCAPY GETINIINFO **** \n Error, no INI File given !!")
            sys.exit()

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
        print("**** GMOCCAPY GETINIINFO **** \n Preference file path: %s" % temp)
        return temp

    def get_coordinates(self):
        temp = self.inifile.find("TRAJ", "COORDINATES")
        if not temp:
            print("**** GMOCCAPY GETINIINFO **** \n No coordinates entry found in [TRAJ] of INI file")
            return ("XYZ")
        return temp

    def get_no_force_homing(self):
        temp = self.inifile.find("TRAJ", "NO_FORCE_HOMING")
        if not temp or temp == "0":
            return False
        return True

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


    def get_jog_vel(self):
        # get default jog velocity
        # must convert from INI's units per second to gscreen's units per minute
        temp = self.inifile.find("DISPLAY", "DEFAULT_LINEAR_VELOCITY")
        if not temp:
            temp = 3.0
            # self.add_alarm_entry(_("No DEFAULT_LINEAR_VELOCITY entry found in [DISPLAY] of INI file: using internal default of %s"%temp))
        return float(temp) * 60

    def get_max_jog_vel(self):
        # get max jog velocity
        # must convert from INI's units per second to gscreen's units per minute
        temp = self.inifile.find("DISPLAY", "MAX_LINEAR_VELOCITY")
        if not temp:
            temp = 10.0
            # self.add_alarm_entry(_("No MAX_LINEAR_VELOCITY entry found in [DISPLAY] of INI file: using internal default of %s"%temp))
        return float(temp) * 60

# ToDo : This may not be needed, as it could be recieved from linuxcnc.stat
    def get_max_velocity(self):
        # max velocity settings: more then one place to check
        # This is the maximum velocity of the machine
        temp = self.inifile.find("TRAJ", "MAX_VELOCITY")
        if  temp == None:
            print("**** GMOCCAPY GETINIINFO **** \n No MAX_VELOCITY found in [TRAJ] of the INI file")
            temp = 15.0
        return float(temp) * 60

    def get_max_spindle_override(self):
        # check for override settings
        temp = self.inifile.find("DISPLAY", "MAX_SPINDLE_OVERRIDE")
        if not temp:
            temp = 1.0
            print("**** GMOCCAPY GETINIINFO **** \n No MAX_SPINDLE_OVERRIDE entry found in [DISPLAY] of INI file")
        return float(temp)

    def get_min_spindle_override(self):
        temp = self.inifile.find("DISPLAY", "MIN_SPINDLE_OVERRIDE")
        if not temp:
            temp = 0.1
            print("**** GMOCCAPY GETINIINFO **** \n No MIN_SPINDLE_OVERRIDE entry found in [DISPLAY] of INI file")
        return float(temp)

    def get_max_feed_override(self):
        temp = self.inifile.find("DISPLAY", "MAX_FEED_OVERRIDE")
        if not temp:
            temp = 1.0
            print("**** GMOCCAPY GETINIINFO **** \n No MAX_FEED_OVERRIDE entry found in [DISPLAY] of INI file")
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
            print("**** GMOCCAPY GETINIINFO **** \n Path %s from DISPLAY , PROGRAM_PREFIX does not exist" % default_path)
            print("**** GMOCCAPY GETINIINFO **** \n Trying default path...")
            default_path = "~/linuxcnc/nc_files/"
            if not os.path.exists(os.path.expanduser(default_path)):
                print("**** GMOCCAPY GETINIINFO **** \n Default path to ~/linuxcnc/nc_files does not exist")
                print("**** GMOCCAPY GETINIINFO **** \n setting now home as path")
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
            print("**** GMOCCAPY GETINIINFO **** \n Error converting the file extensions from INI File 'FILTER','PROGRAMM_PREFIX")
            print("**** GMOCCAPY GETINIINFO **** \n using as default '*.ngc'")
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
            jog_increments = [0, "1,000", "0,100", "0,010", "0,001"]
            print("**** GMOCCAPY GETINIINFO **** \n No default jog increments entry found in [DISPLAY] of INI file")
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
        return self.inifile.findall("MACROS", "MACRO")

    def get_subroutine_path(self):
        subroutines_path = self.inifile.find("RS274NGC", "SUBROUTINE_PATH")
        if not subroutines_path:
            subroutines_path = self.get_program_prefix()
        if not subroutines_path:
            return False
        return subroutines_path

    def get_axis_2_min_limit(self):
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
            print("**** GMOCCAPY GETINIINFO **** \n ERROR in user message setup")
            return None
        else:
            for element in message_pinname:
                if " " in element:
                    print("**** GMOCCAPY GETINIINFO **** \n ERROR in user message setup \n Pinname should not contain spaces")
                    return None
            messages = zip(message_text, message_type, message_pinname)
            return messages

