#!/usr/bin/env python2.7
#
# Qtvcp widget
# Copyright (c) 2017 Chris Morley
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
###############################################################################

import sys
import os
import locale

from PyQt5.QtCore import Qt, QAbstractTableModel, QVariant
from PyQt5.QtWidgets import QTableView, QAbstractItemView
import linuxcnc

from qtvcp.widgets.widget_baseclass import _HalWidgetBase
from qtvcp.core import Status, Action, Info
from qtvcp import logger

#BASE = os.path.abspath(os.path.join(os.path.dirname(sys.argv[0]), ".."))
#LOCALEDIR = os.path.join(BASE, "share", "locale")
#locale.setlocale(locale.LC_ALL, '')

# Instiniate the libraries with global reference
# STATUS gives us status messages from linuxcnc
# INI holds ini details
# ACTION gives commands to linuxcnc
# LOG is for running code logging
STATUS = Status()
ACTION = Action()
INFO = Info()
LOG = logger.getLogger(__name__)

# Set the log level for this module
# LOG.setLevel(logger.DEBUG) # One of DEBUG, INFO, WARNING, ERROR, CRITICAL


class OriginOffsetView(QTableView, _HalWidgetBase):
    def __init__(self, parent=None):
        super(OriginOffsetView, self).__init__(parent)
        self.setAlternatingRowColors(True)

        self.filename = INFO.PARAMETER_FILE
        self.axisletters = ["x", "y", "z", "a", "b", "c", "u", "v", "w"]
        self.linuxcnc = linuxcnc
        self.status = linuxcnc.stat()
        self.IS_RUNNING = False
        self.current_system = None
        self.current_tool = 0
        self.metric_display = False
        self.mm_text_template = '%10.3f'
        self.imperial_text_template = '%9.4f'
        self.setEnabled(False)
        self.table = self.createTable()

    def _hal_init(self):
        self.delay = 0
        STATUS.connect('all-homed', lambda w: self.setEnabled(True))
        STATUS.connect('periodic', self.periodic_check)
        STATUS.connect('metric-mode-changed', lambda w, data: self.metricMode(data))
        STATUS.connect('tool-in-spindle-changed', lambda w, data: self.currentTool(data))
        STATUS.connect('user-system-changed', self._convert_system)
        conversion = {0:"X", 1:"Y", 2:"Z", 3:"A", 4:"B", 5:"C", 6:"U", 7:"V", 8:"W"}
        for num, let in conversion.iteritems():
            if let in (INFO.AVAILABLE_AXES):
                continue
            self.hideColumn(num)

    def _convert_system(self, w, data):
        convert = ("None", "G54", "G55", "G56", "G57", "G58", "G59", "G59.1", "G59.2", "G59.3")
        self.current_system = convert[int(data)]

    def currentTool(self, data):
        self.current_tool = data
    def metricMode(self, state):
        self.metric_display = state

    def createTable(self):
        # create blank taple array
        self.tabledata = [[0, 0, 1, 0, 0, 0, 0, 0, 0, 'Absolute Position'],
                          [None, None, 2, None, None, None, None, None, None, 'Rotational Offsets'],
                          [0, 0, 3, 0, 0, 0, 0, 0, 0, 'G92 Offsets'],
                          [0, 0, 0, 0, 0, 0, 0, 0, 0, 'Current Tool'],
                          [0, 0, 4, 0, 0, 0, 0, 0, 0, 'System 1'],
                          [0, 0, 5, 0, 0, 0, 0, 0, 0, 'System 2'],
                          [0, 0, 6, 0, 0, 0, 0, 0, 0, 'System 3'],
                          [0, 0, 7, 0, 0, 0, 0, 0, 0, 'System 4'],
                          [0, 0, 8, 0, 0, 0, 0, 0, 0, 'System 5'],
                          [0, 0, 9, 0, 0, 0, 0, 0, 0, 'System 6'],
                          [0, 0, 10, 0, 0, 0, 0, 0, 0, 'System 7'],
                          [0, 0, 11, 0, 0, 0, 0, 0, 0, 'System 8'],
                          [0, 0, 12, 0, 0, 0, 0, 0, 0, 'System 9']]

        # create the view
        self.setSelectionMode(QAbstractItemView.SingleSelection)

        # set the table model
        header = ['X', 'Y', 'Z', 'A', 'B', 'C', 'U', 'V', 'W', 'Name']
        vheader = ['ABS', 'Rot', 'G92', 'Tool', 'G54', 'G55', 'G56', 'G57', 'G58', 'G59', 'G59.1', 'G59.2', 'G59.3']
        self.tablemodel = MyTableModel(self.tabledata, header, vheader, self)
        self.setModel(self.tablemodel)
        self.clicked.connect(self.showSelection)
        #self.dataChanged.connect(self.selection_changed)

        # set the minimum size
        self.setMinimumSize(100, 100)

        # set horizontal header properties
        hh = self.horizontalHeader()
        hh.setStretchLastSection(True)

        # set column width to fit contents
        self.resizeColumnsToContents()

        # set row height
        self.resizeRowsToContents()

        # enable sorting
        self.setSortingEnabled(False)

    def showSelection(self, item):
        cellContent = item.data()
        #text = cellContent.toPyObject()  # test
        text = cellContent
        LOG.debug('Text: {}, Row: {}, Column: {}'.format(text, item.row(), item.column()))
        sf = "You clicked on {}".format(text)
        # display in title bar for convenience
        self.setWindowTitle(sf)

    #############################################################

    # Reload the offsets into display
    def reload_offsets(self):
        g54, g55, g56, g57, g58, g59, g59_1, g59_2, g59_3 = self.read_file()
        if g54 is None: return
        # Get the offsets arrays and convert the units if the display
        # is not in machine native units
        try:
            self.status.poll()
            self.IS_RUNNING = True
        except:
            self.current_system = "G54"
            self.IS_RUNNING = False

        ap = self.status.actual_position
        tool = self.status.tool_offset
        g92 = self.status.g92_offset
        rot = self.status.rotation_xy

        if self.metric_display != INFO.MACHINE_IS_METRIC:
            ap = INFO.convert_units_9(ap)
            tool = INFO.convert_units_9(tool)
            g92 = INFO.convert_units_9(g92)
            g54 = INFO.convert_units_9(g54)
            g55 = INFO.convert_units_9(g55)
            g56 = INFO.convert_units_9(g56)
            g57 = INFO.convert_units_9(g57)
            g58 = INFO.convert_units_9(g58)
            g59 = INFO.convert_units_9(g59)
            g59_1 = INFO.convert_units_9(g59_1)
            g59_2 = INFO.convert_units_9(g59_2)
            g59_3 = INFO.convert_units_9(g59_3)

        # set the text style based on unit type
        if self.metric_display:
            tmpl = self.mm_text_template
        else:
            tmpl = self.imperial_text_template

        degree_tmpl = "%11.2f"

        # fill each row of the liststore fron the offsets arrays
        for row, i in enumerate([ap, rot, g92, tool, g54, g55, g56, g57, g58, g59, g59_1, g59_2, g59_3]):
            for column in range(0, 9):
                if row == 1:
                    if column == 2:
                        self.tabledata[row][column] = locale.format(degree_tmpl, rot)
                    else:
                        self.tabledata[row][column] = " "
                else:
                    self.tabledata[row][column] = locale.format(tmpl, i[column])
        self.tablemodel.layoutChanged.emit()
        self.resizeColumnsToContents()

    # We read the var file directly
    # and pull out the info we need
    # if anything goes wrong we set all the info to 0
    def read_file(self):
        try:
            g54 = [0, 0, 0, 0, 0, 0, 0, 0, 0]
            g55 = [0, 0, 0, 0, 0, 0, 0, 0, 0]
            g56 = [0, 0, 0, 0, 0, 0, 0, 0, 0]
            g57 = [0, 0, 0, 0, 0, 0, 0, 0, 0]
            g58 = [0, 0, 0, 0, 0, 0, 0, 0, 0]
            g59 = [0, 0, 0, 0, 0, 0, 0, 0, 0]
            g59_1 = [0, 0, 0, 0, 0, 0, 0, 0, 0]
            g59_2 = [0, 0, 0, 0, 0, 0, 0, 0, 0]
            g59_3 = [0, 0, 0, 0, 0, 0, 0, 0, 0]
            if self.filename is None:
                return g54, g55, g56, g57, g58, g59, g59_1, g59_2, g59_3
            if not os.path.exists(self.filename):
                LOG.error('File does not exist: yellow<{}>'.format(self.filename))
                return g54, g55, g56, g57, g58, g59, g59_1, g59_2, g59_3
            logfile = open(self.filename, "r").readlines()
            for line in logfile:
                temp = line.split()
                param = int(temp[0])
                data = float(temp[1])

                if 5229 >= param >= 5221:
                    g54[param - 5221] = data
                elif 5249 >= param >= 5241:
                    g55[param - 5241] = data
                elif 5269 >= param >= 5261:
                    g56[param - 5261] = data
                elif 5289 >= param >= 5281:
                    g57[param - 5281] = data
                elif 5309 >= param >= 5301:
                    g58[param - 5301] = data
                elif 5329 >= param >= 5321:
                    g59[param - 5321] = data
                elif 5349 >= param >= 5341:
                    g59_1[param - 5341] = data
                elif 5369 >= param >= 5361:
                    g59_2[param - 5361] = data
                elif 5389 >= param >= 5381:
                    g59_3[param - 5381] = data
            return g54, g55, g56, g57, g58, g59, g59_1, g59_2, g59_3
        except:
            return None, None, None, None, None, None, None, None, None

    def dataChanged(self, new, old, x):
        row = new.row()
        col = new.column()
        data = self.tabledata[row][col]

        # Hack to not edit any rotational offset but Z axis
        if row == 1 and not col == 2: return

        # set the text style based on unit type
        if self.metric_display:
            tmpl = lambda s: self.mm_text_template % s
        else:
            tmpl = lambda s: self.imperial_text_template % s

        # make sure we switch to correct units for machine and rotational, row 2, does not get converted
        try:
                qualified = float(data)
                #qualified = float(locale.atof(data))
        except Exception as e:
            LOG.exception(e)
        # now update linuxcnc to the change
        try:
            if self.IS_RUNNING:
                ACTION.RECORD_CURRENT_MODE()
                if row == 0:  # current Origin
                    ACTION.CALL_MDI("G10 L2 P0 %s %10.4f" % (self.axisletters[col], qualified))
                elif row == 1:  # rotational
                    if col == 2:  # Z axis only
                        ACTION.CALL_MDI("G10 L2 P0 R %10.4f" % (qualified))
                elif row == 2:  # G92 offset
                    ACTION.CALL_MDI("G92 %s %10.4f" % (self.axisletters[col], qualified))
                elif row == 3:  # Tool
                    if not self.current_tool == 0:
                        ACTION.CALL_MDI("G10 L1 P%d %s %10.4f" % (self.current_tool, self.axisletters[col], qualified))
                        ACTION.CALL_MDI('g43')
                else:
                        ACTION.CALL_MDI("G10 L2 P%d %s %10.4f" % (row-3, self.axisletters[col], qualified))

                ACTION.UPDATE_VAR_FILE()
                ACTION.RESTORE_RECORDED_MODE()
                STATUS.emit('reload-display')
                self.reload_offsets()
        except Exception as e:
            LOG.exception("offsetpage widget error: MDI call error", exc_info=e)
            self.reload_offsets()

    # only update every 10th time periodic calls
    def periodic_check(self, w):
        if self.delay < 99:
            self.delay += 1
            return
        else:
            self.delay = 0
        if self.filename:
            self.reload_offsets()
        return True


#########################################
# custom model
#########################################
class MyTableModel(QAbstractTableModel):
    def __init__(self, datain, headerdata, vheaderdata, parent=None):
        """
        Args:
            datain: a list of lists\n
            headerdata: a list of strings
        """
        super(MyTableModel, self).__init__(parent)
        self.arraydata = datain
        self.headerdata = headerdata
        self.Vheaderdata = vheaderdata

    def rowCount(self, parent):
        return len(self.arraydata)

    def columnCount(self, parent):
        if len(self.arraydata) > 0:
            return len(self.arraydata[0])
        return 0

    def data(self, index, role):
        if role == Qt.EditRole:
            return self.arraydata[index.row()][index.column()]
        if role == Qt.DisplayRole:
            return QVariant(self.arraydata[index.row()][index.column()])
        return QVariant()


    def flags(self, index):
        if not index.isValid():
            return None
        # print(">>> flags() index.column() = ", index.column())
        if index.column() == 9 and index.row() in(0, 1, 2, 3):
            return Qt.ItemIsEnabled
        elif index.row() == 1 and not index.column() == 2:
            return Qt.NoItemFlags
        else:
            return Qt.ItemIsEditable | Qt.ItemIsEnabled | Qt.ItemIsSelectable

    def setData(self, index, value, role):
        if not index.isValid():
            return False
        LOG.debug(self.arraydata[index.row()][index.column()])
        LOG.debug(">>> setData() role = {}".format(role))
        LOG.debug(">>> setData() index.column() = {}".format(index.column()))
        try:
            if index.column() == 9:
                v = str(value)
            else:
                v = float(value)
        except:
            return False
        LOG.debug(">>> setData() value = {}".format(value))
        LOG.debug(">>> setData() qualified value = {}".format(v))
        # self.emit(SIGNAL("dataChanged(QModelIndex,QModelIndex)"), index, index)
        LOG.debug(">>> setData() index.row = {}".format(index.row()))
        LOG.debug(">>> setData() index.column = {}".format(index.column()))
        self.arraydata[index.row()][index.column()] = v
        self.dataChanged.emit(index, index)
        return True

    def headerData(self, col, orientation, role):
        if orientation == Qt.Horizontal and role == Qt.DisplayRole:
            return QVariant(self.headerdata[col])
        if orientation != Qt.Horizontal and role == Qt.DisplayRole:
            return QVariant(self.Vheaderdata[col])
        return QVariant()

    def sort(self, Ncol, order):
        """
        Sort table by given column number.
        """
        self.emit(SIGNAL("layoutAboutToBeChanged()"))
        self.arraydata = sorted(self.arraydata, key=operator.itemgetter(Ncol))
        if order == Qt.DescendingOrder:
            self.arraydata.reverse()
        self.emit(SIGNAL("layoutChanged()"))

if __name__ == "__main__":
    from PyQt5.QtWidgets import QApplication
    app = QApplication([])
    w = OriginOffsetView()
    w._hal_init()
    w.show()
    sys.exit(app.exec_())
