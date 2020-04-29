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
import operator

from PyQt5.QtCore import Qt, QAbstractTableModel, QVariant
from PyQt5.QtWidgets import QTableView, QAbstractItemView, QCheckBox

from qtvcp.widgets.widget_baseclass import _HalWidgetBase
from qtvcp.core import Status, Action, Info, Tool
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
TOOL = Tool()
LOG = logger.getLogger(__name__)

# Set the log level for this module
LOG.setLevel(logger.INFO) # One of DEBUG, INFO, WARNING, ERROR, CRITICAL


class ToolOffsetView(QTableView, _HalWidgetBase):
    def __init__(self, parent=None):
        super(ToolOffsetView, self).__init__(parent)
        self.setAlternatingRowColors(True)

        self.filename = INFO.PARAMETER_FILE
        self.axisletters = ["x", "y", "z", "a", "b", "c", "u", "v", "w"]
        self.editing_flag = False
        self.current_system = None
        self.current_tool = 0
        self.metric_display = False
        self.mm_text_template = '%10.3f'
        self.imperial_text_template = '%9.4f'
        self.setEnabled(False)

        # create table
        self.createAllView()

    def _hal_init(self):
        self.delay = 0
        STATUS.connect('all-homed', lambda w: self.setEnabled(True))
        STATUS.connect('interp-idle', lambda w: self.setEnabled(STATUS.machine_is_on()
                                                    and STATUS.is_all_homed()))
        STATUS.connect('interp-run', lambda w: self.setEnabled(False))
        STATUS.connect('metric-mode-changed', lambda w, data: self.metricMode(data))
        STATUS.connect('tool-in-spindle-changed', lambda w, data: self.currentTool(data))
        conversion = {5:"Y", 6:'Y', 7:"Z", 8:'Z', 9:"A", 10:"B", 11:"C", 12:"U", 13:"V", 14:"W"}
        for num, let in conversion.iteritems():
            if let in (INFO.AVAILABLE_AXES):
                continue
            self.hideColumn(num)
        if not INFO.MACHINE_IS_LATHE:
            for i in (4,6,8,16,17,18):
                self.hideColumn(i)
        else:
            self.hideColumn(15)

    def currentTool(self, data):
        self.current_tool = data
    def metricMode(self, state):
        self.metric_display = state

    def createAllView(self):
        # create the view
        self.setSelectionMode(QAbstractItemView.SingleSelection)
        #self.setSelectionBehavior(QAbstractItemView.SelectRows)

        # set the table model
        self.tablemodel = MyTableModel(self)
        self.setModel(self.tablemodel)
        self.clicked.connect(self.showSelection)
        #self.dataChanged.connect(self.selection_changed)

        # set the minimum size
        self.setMinimumSize(100, 100)

        # set horizontal header properties
        hh = self.horizontalHeader()
        hh.setMinimumSectionSize(75)
        hh.setStretchLastSection(True)
        hh.setSortIndicator(1,Qt.AscendingOrder)

        # set column width to fit contents
        self.resizeColumnsToContents()

        # set row height
        self.resizeRowsToContents()

        # enable sorting
        self.setSortingEnabled(True)

    def showSelection(self, item):
        cellContent = item.data()
        #text = cellContent.toPyObject()  # test
        text = cellContent
        LOG.debug('Text: {}, Row: {}, Column: {}'.format(text, item.row(), item.column()))
        sf = "You clicked on {}".format(text)
        # display in title bar for convenience
        self.setWindowTitle(sf)

    #############################################################

    def dataChanged(self, new, old, roles):
        self.editing_flag = True
        row = new.row()
        col = new.column()
        data = self.tablemodel.data(new)
        print 'Entered data:', data, row,col
        # now update linuxcnc to the change
        try:
            if STATUS.is_status_valid():
                #for i in self.tablemodel.arraydata:
                #    LOG.debug("2>>> = {}".format(i))
                error = TOOL.SAVE_TOOLFILE(TOOL.CONVERT_TO_STANDARD_TYPE(self.tablemodel.arraydata))
                if error:
                    raise
                ACTION.RECORD_CURRENT_MODE()
                ACTION.CALL_MDI('g43')
                ACTION.RESTORE_RECORDED_MODE()
                STATUS.emit('reload-display')
                #self.tablemodel.update()
                #self.resizeColumnsToContents()
        except Exception as e:
            LOG.exception("offsetpage widget error: MDI call error", exc_info=e)
        self.editing_flag = False

    def add_tool(self):
        if not STATUS.is_auto_running():
            LOG.debug('add tool request')
            TOOL.ADD_TOOL()

    def delete_tools(self):
        if not STATUS.is_auto_running():
            LOG.debug('delete tools request')
            dtools = self.tablemodel.listCheckedTools()
            if dtools:
                error = TOOL.DELETE_TOOLS(dtools)

    def get_checked_list(self):
        return self.tablemodel.listCheckedTools()

#########################################
# custom model
#########################################
class MyTableModel(QAbstractTableModel):
    def __init__(self, datain, parent=None):
        """
        Args:
            datain: a list of lists\n
            headerdata: a list of strings
        """
        super(MyTableModel, self).__init__(parent)
        self.text_template = '%.4f'
        self.headerdata = ['Select','tool','pocket','X','X Wear', 'Y', 'Y Wear', 'Z', 'Z Wear', 'A', 'B', 'C', 'U', 'V', 'W', 'Diameter', 'Front Angle', 'Back Angle','Orientation','Comment']
        self.vheaderdata = []
        self.arraydata = [[0, 0,'0','0','0','0','0','0','0','0','0','0','0','0','0','0', 0,'No Tool']]
        STATUS.connect('toolfile-stale',lambda o, d: self.update(d))
        self.update(None)

    # make a list of all the checked tools
    def listCheckedTools(self):
        checkedlist = []
        for row in self.arraydata:
            if row[0].isChecked():
                checkedlist.append(row[1])
        return checkedlist

    # update the internal array from STATUS's toolfile read array
    # we make sure the first array is switched to a QCheckbox widget
    def update(self, models):
        data = TOOL.CONVERT_TO_WEAR_TYPE(models)
        if data in (None, []):
            data = [[QCheckBox(),0, 0,'0','0','0','0','0','0','0','0','0','0','0','0','0','0', 0,'No Tool']]
        for line in data:
                if line[0] != QCheckBox:
                    line[0] = QCheckBox()
        self.arraydata = data
        self.layoutChanged.emit()

    # Returns the number of rows under the given parent.
    # When the parent is valid it means that rowCount is
    # returning the number of children of parent.
    #
    # Note: When implementing a table based model, rowCount() 
    # should return 0 when the parent is valid.
    def rowCount(self, parent):
        return len(self.arraydata)

    # Returns the number of columns for the children of the given parent.
    # Note: When implementing a table based model, columnCount() should 
    # return 0 when the parent is valid.
    def columnCount(self, parent):
        if len(self.arraydata) > 0:
            return len(self.arraydata[0])
        return 0

    # Returns the data stored under the given role for the item referred to by the index.
    def data(self, index, role=Qt.DisplayRole):
        if role == Qt.EditRole:
            return self.arraydata[index.row()][index.column()]
        elif role == Qt.DisplayRole:
            return self.arraydata[index.row()][index.column()]
        elif role == Qt.CheckStateRole:
            if index.column() == 0:
                # print(">>> data() row,col = %d, %d" % (index.row(), index.column()))
                if self.arraydata[index.row()][index.column()].isChecked():
                    return Qt.Checked
                else:
                    return Qt.Unchecked
        return QVariant()


    # Returns the item flags for the given index.
    def flags(self, index):
        if not index.isValid():
            return None
        if index.column() == 0:
            return Qt.ItemIsEnabled | Qt.ItemIsUserCheckable
        else:
            return Qt.ItemIsEditable | Qt.ItemIsEnabled | Qt.ItemIsSelectable

    # Sets the role data for the item at index to value.
    # Returns true if successful; otherwise returns false.
    # The dataChanged() signal should be emitted if the data was successfully set.
    # these column numbers correspond to our included wear columns
    # it will be converted when saved
    def setData(self, index, value, role):
        col = index.column()
        if not index.isValid():
            LOG.error(">>> index not valid {}".format(index))
            return False
        LOG.debug("original value:{}".format(self.arraydata[index.row()][col]))
        LOG.debug(">>> setData() role = {}".format(role))
        LOG.debug(">>> setData() column() = {}".format(col))
        if role == Qt.CheckStateRole and index.column() == 0:
            #print(">>> setData() role = ", role)
            #print(">>> setData() index.column() = ", index.column())
            if value == Qt.Checked:
                self.arraydata[index.row()][index.column()].setChecked(True)
                #self.arraydata[index.row()][index.column()].setText("Delete")
                #print 'selected',self.arraydata[index.row()][1]
            else:
                self.arraydata[index.row()][index.column()].setChecked(False)
                #self.arraydata[index.row()][index.column()].setText("Un")
            # don't emit dataChanged - return right away
            return True

        # TODO make valuse actually change in metric/impeial mode
        # currently it displays always in machine units.
        # there needs to be conversion added to this code
        # and this class needs access to templates and units mode.
        # don't convert tool,pocket,A, B, C, front angle, back angle, orintation or comments
        tmpl = lambda s: self.text_template % s
        try:
            if col in (1,2,18): # tool, pocket, orientation
                v = int(value)
            elif col == 19:
                v = str(value) # comment
            else:
                v = float(value)
            self.arraydata[index.row()][col] = v
        except:
            LOG.error("Invaliad data type in row {} column:{} ".format(index.row(), col))
            return False
        LOG.debug(">>> setData() value = {} ".format(value))
        LOG.debug(">>> setData() qualified value = {}".format(v))
        LOG.debug(">>> setData() index.row = {}".format(index.row()))
        LOG.debug(">>> setData() index.column = {}".format(col))

        LOG.debug(">>> = {}".format(self.arraydata[index.row()][col]))
        for i in self.arraydata:
            LOG.debug(">>> = {}".format(i))

        self.dataChanged.emit(index, index)
        return True

    # Returns the data for the given role and section in the header with the specified orientation.
    # For horizontal headers, the section number corresponds to the column number. 
    # Similarly, for vertical headers, the section number corresponds to the row number.
    def headerData(self, col, orientation, role):
        if orientation == Qt.Horizontal and role == Qt.DisplayRole:
            return QVariant(self.headerdata[col])
        if orientation != Qt.Horizontal and role == Qt.DisplayRole:
            return QVariant('')
        return QVariant()

    # Sorts the model by column in the given order.
    def sort(self, Ncol, order):
        """
        Sort table by given column number.
        """
        self.layoutAboutToBeChanged.emit()
        self.arraydata = sorted(self.arraydata, key=operator.itemgetter(Ncol))
        if order == Qt.DescendingOrder:
            self.arraydata.reverse()
        self.layoutChanged.emit()

if __name__ == "__main__":
    from PyQt5.QtWidgets import QApplication
    app = QApplication(sys.argv)
    w = ToolOffsetView()
    w._hal_init()
    w.show()
    sys.exit(app.exec_())
