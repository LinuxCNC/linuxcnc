#!/usr/bin/env python3
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
import hashlib

from qtvcp.core import Status, Info, Action
# Set up logging
from . import logger

STATUS = Status()
INFO = Info()
ACTION = Action()
LOG = logger.getLogger(__name__)
# Force the log level for this module
# LOG.setLevel(logger.INFO) # One of DEBUG, INFO, WARNING, ERROR, CRITICAL

KEYWORDS = ['T', 'P', 'X', 'Y', 'Z', 'A', 'B', 'C', 'U', 'V', 'W', 'D', 'I', 'J', 'Q', ';']


class _TStat(object):

    def __init__(self):
        # only initialize once for all instances
        if self.__class__._instanceNum >= 1:
            return
        self.__class__._instanceNum += 1
        self._delay = 0
        self._hash_code = None
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
        self.toolinfo = None
        STATUS.connect('periodic', self.periodic_check)
        STATUS.connect('forced-update', lambda o: self.emit_update())

    def GET_TOOL_INFO(self, toolnum):
        self.current_tool_num = int(toolnum)
        self._reload()
        return self.toolinfo

    def GET_TOOL_ARRAY(self):
        info = self.GET_TOOL_MODELS()
        return info[0] + info[1]

    def GET_TOOL_MODELS(self):
        return self._reload()

    def SAVE_TOOLFILE(self, array):
        return self._save(array)

    def ADD_TOOL(self, newtool=[-99, 0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0, 'New Tool']):
        info = self.GET_TOOL_MODELS()
        info[0].insert(0, newtool)
        return self._save(info[0] + info[1])

    def DELETE_TOOLS(self, tools):
        ta = self.GET_TOOL_ARRAY()
        if type(tools) == int:
            tools = [tools]
        return self._save(ta, tools)

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
        if self.toolfile is None or not os.path.exists(self.toolfile):
            LOG.debug("Toolfile does not exist' {}".format(self.toolfile))
            return None
        # print 'file',self.toolfile
        # clear the current liststore, search the tool file, and add each tool
        tool_model = []
        wear_model = []
        logfile = open(self.toolfile, "r").readlines()
        self.toolinfo = None
        toolinfo_flag = False
        for rawline in logfile:
            # ignore blank lines
            if rawline == '':
                continue
            # print 'raw:',rawline
            # strip the comments from line and add directly to array
            # if index = -1 the delimiter ; is missing - clear comments
            index = rawline.find(";")
            comment = ''
            if not index == -1:
                comment = (rawline[index + 1:])
                comment = comment.rstrip("\n")
                line = rawline.rstrip(comment)
            else:
                line = rawline
            array = [0, 0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0, comment]
            wear_flag = False
            # search beginning of each word for keyword letters
            # if i = ';' that is the comment and we have already added it
            # offset 0 and 1 are integers the rest floats
            # we strip leading and following spaces from the comments
            for offset, i in enumerate(KEYWORDS):
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
                        if offset in (0, 1, 14):
                            try:
                                array[offset] = int(word.lstrip(i))
                            except ValueError as e:
                                try:
                                    array[offset] = int(float(word.lstrip(i)))
                                except Exception as e:
                                    LOG.error("toolfile integer access: {} : {}".format(word.lstrip(i), e))
                        else:
                            try:
                                # we will call this range zero:
                                if float(word.lstrip(i)) < 0.000001 and float(word.lstrip(i)) > -0.000001:
                                    array[offset] = 0.0
                                else:
                                    array[offset] = float(word.lstrip(i))
                            except:
                                LOG.error("toolfile float access: {}".format(self.toolfile))
                        break

            # add array line to model array
            if wear_flag:
                wear_model.append(array)
            else:
                tool_model.append(array)
        if toolinfo_flag:
            self.toolinfo = temp
        else:
            self.toolinfo = [0, 0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0, 'No Tool']
        # print 'load'
        # for i in tool_model:
        #    print i
        # print wear_model
        return (tool_model, wear_model)

    # converts from linuxcnc toolfile array to toolwear array
    # linuxcnc handles toolwear by having tool wear as extra tools with tool numbers above 10000 (fanuc style)
    # qtvcp just adds the extra tool wear positions (x and z) to the original array 
    def CONVERT_TO_WEAR_TYPE(self, data):
        if data is None:
            data = ([], [])
        if not INFO.MACHINE_IS_LATHE:
            maintool = data[0] + data[1]
            weartool = []
        else:
            maintool = data[0]
            weartool = data[1]
        # print 'main',data
        tool_num_list = {}
        full_tool_list = []
        for rnum, row in enumerate(maintool):
            new_line = [False, 0, 0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0,
                        'No Tool']
            valuesInRow = [value for value in row]
            for cnum, i in enumerate(valuesInRow):
                if cnum == 0:
                    # keep a dict of actual tools numbers vrs row index
                    tool_num_list[i] = rnum
                if cnum in (0, 1, 2):
                    # transfer these positions directly to new line (offset by 1 for checkbox)
                    new_line[cnum + 1] = i
                elif cnum == 3:
                    # move Y past x wear position
                    new_line[5] = i
                elif cnum == 4:
                    # move z past y wear position
                    new_line[7] = i
                elif cnum in (5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15):
                    # a;; the rest past z wear position
                    new_line[cnum + 4] = i
            full_tool_list.append(new_line)
            # print 'row',row
            # print 'new row',new_line
        # any tool number over 10000 is a wear offset
        # It's already been separated in the weartool variable.
        # now we pull the values we need out and put it in our
        # full tool list's  tool variable's parent tool row
        # eg 10001 goes to tool 1, 10002 goes to tool 2 etc
        # for now only if in lathe mode
        if INFO.MACHINE_IS_LATHE:
            for rnum, row in enumerate(weartool):
                values = [value for value in row]
                try:
                    parent_tool = tool_num_list[(values[0] - 10000)]
                except KeyError:
                    LOG.error("tool wear number has no parent Tool: {}".format(values))
                    continue
                else:
                    full_tool_list[parent_tool][4] = values[2]
                    full_tool_list[parent_tool][6] = values[3]
                    full_tool_list[parent_tool][8] = values[4]
        return full_tool_list

    # converts from toolwear array to linuxcnc toolfile array
    # linuxcnc handles toolwear by having tool wear as extra tools with tool numbers above 10000 (fanuc style)
    # qtvcp just adds the extra tool wear positions (x and z) to the original array 
    def CONVERT_TO_STANDARD_TYPE(self, data):
        # for i in data:
        #    print i
        if data is None:
            data = ([])
        tool_wear_list = []
        full_tool_list = []
        for rnum, row in enumerate(data):
            new_line = [0, 0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0, '']
            new_wear_line = [0, 0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0, 'Wear Offset']
            wear_flag = False
            values = [value for value in row]
            for cnum, i in enumerate(values):
                # print cnum, i, type(i)
                if cnum in (1, 2):
                    new_line[cnum - 1] = int(i)
                elif cnum == 3:
                    new_line[cnum - 1] = float(i)
                elif cnum == 4 and i != '0':
                    wear_flag = True
                    new_wear_line[2] = float(i)
                elif cnum == 5 and i != '0':
                    new_line[cnum - 2] = float(i)
                elif cnum == 6 and i != '0':
                    wear_flag = True
                    new_wear_line[3] = float(i)
                elif cnum == 7 and i != '0':
                    new_line[cnum - 3] = float(i)
                elif cnum == 8 and i != '0':
                    wear_flag = True
                    new_wear_line[4] = float(i)
                elif cnum in (9, 10, 11, 12, 13, 14, 15, 16, 17):
                    new_line[cnum - 4] = float(i)
                elif cnum == 18:
                    new_line[cnum - 4] = int(i)
                elif cnum == 19:
                    new_line[cnum - 4] = str(i)
            if wear_flag:
                new_wear_line[0] = int(values[1] + 10000)
                new_wear_line[15] = 'Wear Offset %d' % values[1]
                tool_wear_list.append(new_wear_line)
            # add tool line to tool list
            full_tool_list.append(new_line)
            LOG.debug("converted line: {}".format(new_line))
        # add wear list to full tool list if in lathe mode
        if INFO.MACHINE_IS_LATHE:
            full_tool_list = full_tool_list + tool_wear_list
        return full_tool_list

    # TODO check for linnuxcnc ON and IDLE which is the only safe time to edit/SAVE the tool file.

    def _save(self, new_model, delete=()):
        if self.toolfile == None:
            return True
        file = open(self.toolfile, "w")
        for row in new_model:
            values = [value for value in row]
            line = ""
            skip = False
            for num, i in enumerate(values):
                # print KEYWORDS[num], i, #type(i), int(i)
                if num == 0 and i in delete:
                    LOG.debug("delete tool ' {}".format(i))
                    skip = True
                if num in (0, 1, 14):  # tool# pocket# orientation
                    line = line + "%s%d " % (KEYWORDS[num], i)
                elif num == 15:  # comments
                    test = i.strip()
                    line = line + "%s%s " % (KEYWORDS[num], test)
                else:
                    test = float(str(i).lstrip())  # floats
                    if test == 0.0:
                        line = line + "%s%d " % (KEYWORDS[num], test)
                    elif num in (12, 13):
                        line = line + "%s%3.1f " % (KEYWORDS[num], test)
                    else:
                        line = line + "%s%.5f " % (KEYWORDS[num], test)
            LOG.debug("Save line: {}".format(line))
            if not skip:
                print(line, file=file)
        # These lines are required to make sure the OS doesn't cache the data
        # That would make linuxcnc and the widget to be out of synch leading to odd errors
        file.flush()
        os.fsync(file.fileno())
        # tell linuxcnc we changed the tool table entries
        try:
            ACTION.RELOAD_TOOLTABLE()
        except:
            LOG.error("reloading of tool table into linuxcnc: {}".format(self.toolfile))
            return True

        # create a hash code

    def md5sum(self, filename):
        try:
            f = open(filename, "rb")
        except:
            return None
        else:
            return hashlib.md5(f.read()).hexdigest()

    # push the update to whoever using STATUS
    def emit_update(self):
        data = self.GET_TOOL_MODELS()
        if data is not None:
            STATUS.emit('toolfile-stale', data)

    # check the hash code on the toolfile against
    # the saved hash code when last reloaded.
    def periodic_check(self, w):
        if self._delay < 9:
            self._delay += 1
            return
        if STATUS.is_status_valid() == False:
            return
        self._delay = 0
        m1 = self.md5sum(self.toolfile)
        if m1 and self._hash_code != m1:
            self._hash_code = m1
            self.emit_update()
