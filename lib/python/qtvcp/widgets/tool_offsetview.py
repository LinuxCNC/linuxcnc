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
LOG.setLevel(logger.DEBUG) # One of DEBUG, INFO, WARNING, ERROR, CRITICAL


class ToolOffsetView(QTableView, _HalWidgetBase):
    def __init__(self, parent=None):
        super(ToolOffsetView, self).__init__(parent)
        self.setAlternatingRowColors(True)

        self.filename = INFO.PARAMETER_FILE
        self.axisletters = ["x", "y", "z", "a", "b", "c", "u", "v", "w"]
        self.IS_RUNNING = False
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
        STATUS.connect('periodic', self.periodic_check)
        STATUS.connect('metric-mode-changed', lambda w, data: self.metricMode(data))
        STATUS.connect('tool-in-spindle-changed', lambda w, data: self.currentTool(data))
        conversion = {0:"X", 1:"Y", 2:"Z", 3:"A", 4:"B", 5:"C", 6:"U", 7:"V", 8:"W"}
        for num, let in conversion.iteritems():
            if let in (INFO.AVAILABLE_AXES):
                continue
            self.hideColumn(num+2)

    # only update every 100th time periodic calls
    # if editing don't update
    #
    def periodic_check(self, w):
        try:
            STATUS.stat.poll()
            self.IS_RUNNING = True
        except:
            self.IS_RUNNING = False
            return
        if self.delay < 999:
            self.delay += 1
            return
        if self.editing_flag: return
        self.delay = 0
        self.tablemodel.update(TOOL.GET_TOOL_FILE())
        self.resizeColumnsToContents()
        return True

    def currentTool(self, data):
        self.current_tool = data
    def metricMode(self, state):
        self.metric_display = state

    def createAllView(self):
        # create the view
        self.setSelectionMode(QAbstractItemView.SingleSelection)

        # set the table model
        header = ['tool','pocket','X', 'Y', 'Z', 'A', 'B', 'C', 'U', 'V', 'W', 'Diameter', 'Front Angle', 'Back Angle','Orientation','Comment']
        vheader = []
        self.tablemodel = MyTableModel(TOOL.GET_TOOL_FILE(), header, vheader, self)
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

    def dataChanged(self, new, old, roles):
        self.editing_flag = True
        row = new.row()
        col = new.column()
        data = self.tablemodel.data(new)
        print 'Entered data:', data, row,col
        if col is not 15:
            # set the text style based on unit type
            if self.metric_display:
                tmpl = lambda s: self.mm_text_template % s
            else:
                tmpl = lambda s: self.imperial_text_template % s

            # #TODO make sure we switch to correct units for machine when saving file
            try:
                    qualified = float(data)
                    #qualified = float(locale.atof(data))
            except Exception as e:
                LOG.exception(e)
                qualified = None

            print ' QUALIFIED:', qualified
        # now update linuxcnc to the change
        try:
            if self.IS_RUNNING:
                TOOL.SAVE_TOOLFILE(self.tablemodel.arraydata)
                ACTION.RECORD_CURRENT_MODE()
                ACTION.CALL_MDI('g43')
                ACTION.RESTORE_RECORDED_MODE()
                STATUS.emit('reload-display')
                self.tablemodel.update(TOOL.GET_TOOL_FILE())
                self.resizeColumnsToContents()
        except Exception as e:
            LOG.exception("offsetpage widget error: MDI call error", exc_info=e)
        self.editing_flag = False




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

    def update(self, data):
        #print 'update'
        if data is None:
            data = [[0,0,'0','0','0','0','0','0','0','0','0','0','0','0','0','No Tool']]
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
        if role == Qt.DisplayRole:
            return self.arraydata[index.row()][index.column()]
        return QVariant()


    # Returns the item flags for the given index.
    def flags(self, index):
        if not index.isValid():
            return None
        return Qt.ItemIsEditable | Qt.ItemIsEnabled | Qt.ItemIsSelectable

    # Sets the role data for the item at index to value.
    # Returns true if successful; otherwise returns false.
    # The dataChanged() signal should be emitted if the data was successfully set.
    def setData(self, index, value, role):
        if not index.isValid():
            return False
        LOG.debug("original value:{}".format(self.arraydata[index.row()][index.column()]))
        LOG.debug(">>> setData() role = {}".format(role))
        LOG.debug(">>> setData() index.column() = {}".format(index.column()))
        try:
            if index.column() in (0,1):
                v = int(value)
            elif index.column() == 15:
                v = str(value)
            else:
                v = float(value)
        except:
            return False
        LOG.debug(">>> setData() value = {} ".format(value))
        LOG.debug(">>> setData() qualified value = {}".format(v))
        LOG.debug(">>> setData() index.row = {}".format(index.row()))
        LOG.debug(">>> setData() index.column = {}".format(index.column()))
        self.arraydata[index.row()][index.column()] = v
        print self.arraydata[index.row()][index.column()]
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
        self.emit(SIGNAL("layoutAboutToBeChanged()"))
        self.arraydata = sorted(self.arraydata, key=operator.itemgetter(Ncol))
        if order == Qt.DescendingOrder:
            self.arraydata.reverse()
        self.emit(SIGNAL("layoutChanged()"))

if __name__ == "__main__":
    from PyQt5.QtWidgets import QApplication
    app = QApplication(sys.argv)
    w = ToolOffsetView()
    w._hal_init()
    w.show()
    sys.exit(app.exec_())
