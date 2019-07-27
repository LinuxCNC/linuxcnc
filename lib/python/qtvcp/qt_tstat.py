#!/usr/bin/env python
# Qtvcp
#
# Copyright (c) 2017  Chris Morley <chrisinnanaimo@hotmail.com>
#
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 2 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
# 
###############################################################################

import os
import linuxcnc
import hashlib

from qtvcp.core import Info
# Set up logging
import logger

INFO = Info()
LOG = logger.getLogger(__name__)
# Set the log level for this module
LOG.setLevel(logger.DEBUG) # One of DEBUG, INFO, WARNING, ERROR, CRITICAL

KEYWORDS = ['T', 'P', 'X', 'Y', 'Z', 'A', 'B', 'C', 'U', 'V', 'W', 'D', 'I', 'J', 'Q', ';']


class _TStat(object):

    def __init__(self):
        # only initialize once for all instances
        if self.__class__._instanceNum >=1:
            return
        self.__class__._instanceNum += 1

        self.NUM = 0
        self.POCKET = 1
        self.X = 2
        self.Y = 3
        self.Z = 4
        self.A = 5
        self.B = 6
        self.C = 7
        self.U = 8
        self.V = 9
        self.W = 10
        self.DIAMETER = 11
        self.FRONTANGLE = 12
        self.BACKANGLE = 13
        self.ORIENTATION = 14
        self.COMMENTS = 15
        self.hash_check = None 
        self.toolfile = INFO.TOOL_FILE_PATH
        self.tool_wear_info = None
        self.current_tool_num = -1
        self.model = []
        self.wear_model = []
        self.toolinfo = None

    def GET_TOOL_INFO(self, toolnum):
        self.current_tool_num = int(toolnum)
        self._reload()
        return self.toolinfo

    def GET_TOOL_FILE(self):
        self._reload()
        return self.model, self.wear_model

    def SAVE_TOOLFILE(self, array):
        self._save(array)

    def ADD_TOOL(self, tool_array = [0,0,'0','0','0','0','0','0','0','0','0','0','0','0','0','No Tool']):
        pass

    # [0] = tool number
    # [1] = pocket number
    # [2] = X offset
    # [3] = Y offset
    # [4] = Z offset
    # [5] = A offset
    # [6] = B offset
    # [7] = C offset
    # [8] = U offset
    # [9] = V offset
    # [10] = W offset
    # [11] = tool diameter
    # [12] = frontangle
    # [13] = backangle
    # [14] = tool orientation
    # [15] = tool comments
    # Reload the tool file into the array model and update tool_info
    def _reload(self):
        if self.toolfile == None:
            return None
        if not os.path.exists(self.toolfile):
            print "Toolfile does not exist"
            return None
        self.hash_code = self.md5sum(self.toolfile)
        # clear the current liststore, search the tool file, and add each tool
        self.model = []
        self.wear_model = []
        logfile = open(self.toolfile, "r").readlines()
        self.toolinfo = None
        toolinfo_flag = False
        for rawline in logfile:
            # strip the comments from line and add directly to array
            # if index = -1 the delimiter ; is missing - clear comments
            index = rawline.find(";")
            comment =''
            if not index == -1:
                comment = (rawline[index+1:])
                comment = comment.rstrip("\n")
                line = rawline.rstrip(comment)
            else:
                line = rawline
            array = [0,0,'0','0','0','0','0','0','0','0','0','0','0','0','0',comment]
            wear_flag = False
            # search beginning of each word for keyword letters
            # if i = ';' that is the comment and we have already added it
            # offset 0 and 1 are integers the rest floats
            # we strip leading and following spaces from the comments
            for offset,i in enumerate(KEYWORDS):
                if i == ';': continue
                for word in line.split():
                    if word.startswith(';'): break
                    if word.startswith(i):
                        if offset == 0:
                            if int(word.lstrip(i)) == self.current_tool_num:
                                toolinfo_flag = True
                                # This array's tool num is the current tool num
                                # remember it for later
                                temp = array
                            # check if tool is greater then 10000 -then it's a wear tool
                            if int(word.lstrip(i)) > 10000:
                                wear_flag = True
                        if offset in(0,1):
                            try:
                                array[offset]= int(word.lstrip(i))
                            except:
                                 LOG.error("toolfile integer access: {}".format(self.toolfile))
                        else:
                            try:
                                if float(word.lstrip(i)) < 0.000001:
                                    array[offset]= ("0")
                                else:
                                    array[offset]= ("%10.4f" % float(word.lstrip(i)))
                            except:
                                LOG.error("toolfile float access: {}".format(self.toolfile))
                        break

            # add array line to model array
            if wear_flag:
                self.wear_model.append(array)
            else:
                self.model.append(array)
        if toolinfo_flag:
            self.toolinfo = temp
        else:
            self.toolinfo = [0,0,'0','0','0','0','0','0','0','0','0','0','0','0','0','No Tool']

    def CONVERT_TO_WEAR_TYPE(self, data):
        if not INFO.MACHINE_IS_LATHE:
            maintool = data[0] + data[1]
            weartool = []
        else:
            maintool = data[0]
            weartool = data[1]
        tool_num_list = {}
        full_tool_list = []
        for rnum, row in enumerate(maintool):
            new_line = [0,0,'0','0','0','0','0','0','0','0','0','0','0','0','0','0','0','0','No Tool']
            values = [ value for value in row ]
            for cnum,i in enumerate(values):
                if cnum == 0:
                    tool_num_list[i] = rnum
                if cnum in(0,1,2,):
                    new_line[cnum] = i
                elif cnum == 3:
                    new_line[4] = i
                elif cnum == 4:
                    new_line[6] = i
                elif cnum in(5,6,7,8,9,10,11,12,13,14,15):
                    new_line[cnum+3] = i
            #print new_line
            full_tool_list.append(new_line)

        # any tool number over 10000 is a wear offset
        # add it's value to the parent tool
        # eg 10001 goes to tool 1
        for rnum, row in enumerate(weartool):
            values = [ value for value in row ]
            parent_tool = tool_num_list[( values[0]-10000)]
            full_tool_list[parent_tool][3] = values[2]
            full_tool_list[parent_tool][5] = values[3]
            full_tool_list[parent_tool][7] = values[4]
        return full_tool_list

    def CONVERT_TO_STANDARD(self, data):
        tool_wear_list = []           
        full_tool_list = []
        for rnum, row in enumerate(data):
            new_line = [0,0,'0','0','0','0','0','0','0','0','0','0','0','0','0','']
            new_wear_line = [0,0,'0','0','0','0','0','0','0','0','0','0','0','0','0','Wear Offset']
            wear_flag = False
            values = [ value for value in row ]
            for cnum,i in enumerate(values):
                if cnum in(0,1):
                    new_line[cnum] = int(i)
                elif cnum == 2:
                    new_line[cnum] = float(i)
                elif cnum == 3 and i !='0':
                    wear_flag = True
                    new_wear_line[2] = float(i)
                elif cnum == 5 and i !='0':
                    wear_flag = True
                    new_wear_line[3] = float(i)
                elif cnum == 7 and i !='0':
                    wear_flag = True
                    new_wear_line[4] = float(i)
                elif cnum in(8,9,10,11,12,13,14,15,16,17):
                    new_line[cnum-3] = float(i)
                elif cnum == 18:
                    new_line[cnum-3] = str(i)
            if wear_flag:
                new_wear_line[0] = int(values[0]+10000)
                new_wear_line[15] = 'Wear Offset Tool %d'% values[0]
                tool_wear_list.append(new_wear_line)
            # add tool line to tool list
            full_tool_list.append(new_line)
        # add wear list to full tool list
        full_tool_list = full_tool_list + tool_wear_list
        return full_tool_list

    # TODO check for linnuxcnc ON and IDLE which is the only safe time to edit/SAVE the tool file.
    def _save(self, new_model):
        if self.toolfile == None:return
        file = open(self.toolfile, "w")
        for row in new_model:
            values = [ value for value in row ]
            #print values
            line = ""
            for num,i in enumerate(values):
                #print i
                if num in (0,1): # tool# pocket#
                    line = line + "%s%d "%(KEYWORDS[num], i)
                elif num == 15: # comments
                    test = i.strip()
                    line = line + "%s%s "%(KEYWORDS[num],test)
                else:
                    test = str(i).lstrip()  # floats
                    line = line + "%s%s "%(KEYWORDS[num], test)
            LOG.debug("Save line: {}".format(line))
            print >>file,line
        # Theses lines are required to make sure the OS doesn't cache the data
        # That would make linuxcnc and the widget to be out of synch leading to odd errors
        file.flush()
        os.fsync(file.fileno())
        # tell linuxcnc we changed the tool table entries
        try:
            linuxcnc.command().load_tool_table()
        except:
            LOG.error("reloading of tool table into linuxcnc: {}".format(self.toolfile))


        # create a hash code
    def md5sum(self,filename):
        try:
            f = open(filename, "rb")
        except IOError:
            return None
        else:
            return hashlib.md5(f.read()).hexdigest()

        # check the hash code on the toolfile against
        # the saved hash code when last reloaded.
    def file_current_check(self):
        m = self.hash_code
        m1 = self.md5sum(self.toolfile)
        if m1 and m != m1:
            self._reload()

