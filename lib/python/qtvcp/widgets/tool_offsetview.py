#!/usr/bin/env python3
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
import operator

from PyQt5.QtCore import Qt, QAbstractTableModel, QVariant, pyqtProperty, QSize, pyqtSlot
from PyQt5.QtGui import QColor, QIcon
from PyQt5.QtWidgets import (QTableView, QAbstractItemView, QCheckBox,
QItemEditorFactory,QDoubleSpinBox,QSpinBox,QStyledItemDelegate)
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

# Force the log level for this module
#LOG.setLevel(logger.DEBUG) # One of DEBUG, INFO, WARNING, ERROR, CRITICAL

ICONPATH = os.path.join(INFO.LIB_PATH, 'images/widgets/tool_offsetview')

# custom spinbox controls for editing
class ItemEditorFactory(QItemEditorFactory):
    def __init__(self):
        super(ItemEditorFactory,self).__init__()

    def createEditor(self, userType, parent):
        if userType == QVariant.Double:
            doubleSpinBox = QDoubleSpinBox(parent)
            doubleSpinBox.setDecimals(4)
            doubleSpinBox.setMaximum(99999)
            doubleSpinBox.setMinimum(-99999)
            return doubleSpinBox
        elif userType == QVariant.Int:
            spinBox = QSpinBox(parent)
            spinBox.setMaximum(20000)
            spinBox.setMinimum(1)
            return spinBox
        else:
            return super(ItemEditorFactory,self).createEditor(userType, parent)

class ToolOffsetView(QTableView, _HalWidgetBase):
    def __init__(self, parent=None):
        super(ToolOffsetView, self).__init__(parent)
        self.setAlternatingRowColors(True)

        self.filename = INFO.PARAMETER_FILE
        self.axisletters = ["x", "y", "z", "a", "b", "c", "u", "v", "w"]
        self.editing_flag = False
        self.current_system = None
        self.current_tool = 0
        self.setEnabled(False)
        self.dialog_code = 'CALCULATOR'
        self.text_dialog_code = 'KEYBOARD'
        self.setIconSize(QSize(32,32))
        self._last = 0

        # create table
        self.createAllView()

    def _hal_init(self):
        self.delay = 0
        STATUS.connect('all-homed', lambda w: self.setEnabled(True))
        STATUS.connect('not-all-homed', lambda w, axis: self.setEnabled(False))
        STATUS.connect('interp-idle', lambda w: self.setEnabled(STATUS.machine_is_on()
                                                    and (STATUS.is_all_homed()
                                                        or INFO.NO_HOME_REQUIRED)))
        STATUS.connect('interp-run', lambda w: self.setEnabled(False))
        STATUS.connect('metric-mode-changed', lambda w, data: self.metricMode(data))
        STATUS.connect('diameter-mode', lambda w, data: self.diameterMode(data))
        STATUS.connect('tool-in-spindle-changed', lambda w, data: self.currentTool(data))
        STATUS.connect('general',self.return_value)

        conversion = {5:"Y", 6:'Y', 7:"Z", 8:'Z', 9:"A", 10:"B", 11:"C", 12:"U", 13:"V", 14:"W"}
        for num, let in conversion.items():
            if let in (INFO.AVAILABLE_AXES):
                continue
            self.hideColumn(num)
        if not INFO.MACHINE_IS_LATHE:
            for i in (4,6,8,16,17,18):
                self.hideColumn(i)

    def currentTool(self, data):
        self.current_tool = data

    def metricMode(self, state):
        self.tablemodel.metricDisplay(state)
        self.update()

    def diameterMode(self, state):
        self.tablemodel.diameterDisplay(state)
        self.update()

    def createAllView(self):
        styledItemDelegate=QStyledItemDelegate()
        styledItemDelegate.setItemEditorFactory(ItemEditorFactory())
        self.setItemDelegate(styledItemDelegate)

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
        # auto adjust to contents
        hh.setSectionResizeMode(3)

        hh.setStretchLastSection(True)
        hh.setSortIndicator(1,Qt.AscendingOrder)

        vh = self.verticalHeader()
        vh.setVisible(False)

        # set column width to fit contents
        self.resizeColumnsToContents()

        # set row height
        self.resizeRowsToContents()

        # enable sorting
        self.setSortingEnabled(True)

    def showSelection(self, item):
        cellContent = item.data()
        text = cellContent
        LOG.debug('Text: {}, Row: {}, Column: {}'.format(text, item.row(), item.column()))
        sf = "You clicked on {}".format(text)
        # display in title bar for convenience
        self.setWindowTitle(sf)
        # row 0 is not editable (checkbox position)
        # column 19 is the descritive text column
        if item.column() == 19:
            self.callTextDialog(text,item)
        elif item.column() <19 and item.column() > 0:
            self.callDialog(text,item)

    # alphanumerical
    def callTextDialog(self, text,item):
        text = self.tablemodel.arraydata[item.row()][19]
        tool = self.tablemodel.arraydata[item.row()][1]
        mess = {'NAME':self.text_dialog_code,'ID':'%s__' % self.objectName(),
                'PRELOAD':text, 'TITLE':'Tool {} Description Entry'.format(tool),
                'ITEM':item}
        LOG.debug('message sent:{}'.format (mess))
        STATUS.emit('dialog-request', mess)

    # numerical only
    def callDialog(self, text,item):
        axis = self.tablemodel.headerdata[item.column()]
        tool = self.tablemodel.arraydata[item.row()][1]
        mess = {'NAME':self.dialog_code,'ID':'%s__' % self.objectName(),
                'PRELOAD':float(text), 'TITLE':'Tool {} Offset of {},{}'.format(tool, axis,text),
                'ITEM':item}
        LOG.debug('message sent:{}'.format (mess))
        STATUS.emit('dialog-request', mess)


    # process the STATUS return message
    def return_value(self, w, message):
        LOG.debug('message returned:{}'.format (message))
        num = message['RETURN']
        code = bool(message.get('ID') == '%s__'% self.objectName())
        name = bool(message.get('NAME') == self.dialog_code)
        name2 = bool(message.get('NAME') == self.text_dialog_code)
        item = message.get('ITEM')
        if code and name and num is not None:
            self.tablemodel.setData(item, num, None)
        elif code and name2 and num is not None:
            self.tablemodel.setData(item, num, None)

    #############################################################

    def dataChanged(self, new, old, roles):
        self.editing_flag = True
        row = new.row()
        col = new.column()
        data = self.tablemodel.data(new)
        #print('Entered data:', data, row,col)
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

    def set_all_unchecked(self):
        self.tablemodel.uncheckAllTools()

    # This function uses the color name (string); setProperty
    # expects a QColor object
    def highlight(self, color):
        self.setProperty('styleColorHighlight', QColor(color))

    # This function uses the color name (string); calling setProperty
    # expects a QColor object
    def selected(self, color):
        self.setProperty('styleColorSelection', QColor(color))

    # external controls
    @pyqtSlot(float)
    @pyqtSlot(int)
    def scroll(self, data):
        if data > self._last:
            self.up()
        elif data < self._last:
            self.down()
        self._last = data

    # moves the selection up
    @pyqtSlot()
    def up(self):
        cr = self.currentIndex().row()
        cc = 0#self.currentIndex().column()
        row = cr-1
        if row < 0:
            row = 0
        z = self.model().createIndex(row,cc)
        self.setCurrentIndex(z)

    # moves the selection down
    @pyqtSlot()
    def down(self):
        cr = self.currentIndex().row()
        cc = 0#self.currentIndex().column()
        row = cr+1
        if row > self.model().rowCount(None)-1:
            row = cr
        z = self.model().createIndex(row, cc)
        self.setCurrentIndex(z)

    def toggleCurrent(self):
        self.model().toggle(self.currentIndex().row())

    #########################################################################
    # This is how designer can interact with our widget properties.
    # designer will show the pyqtProperty properties in the editor
    # it will use the get set and reset calls to do those actions
    #
    ########################################################################

    def set_dialog_code(self, data):
        self.dialog_code = data
    def get_dialog_code(self):
        return self.dialog_code
    def reset_dialog_code(self):
        self.dialog_code = 'CALCULATOR'
    dialog_code_string = pyqtProperty(str, get_dialog_code, set_dialog_code, reset_dialog_code)

    def set_keyboard_code(self, data):
        self.text_dialog_code = data
    def get_keyboard_code(self):
        return self.text_dialog_code
    def reset_keyboard_code(self):
        self.text_dialog_code = 'KEYBOARD'
    text_dialog_code_string = pyqtProperty(str, get_keyboard_code, set_keyboard_code, reset_keyboard_code)

    def setmetrictemplate(self, data):
        self.tablemodel.metric_text_template = data
    def getmetrictemplate(self):
        return self.tablemodel.metric_text_template
    def resetmetrictemplate(self):
        self.tablemodel.metric_text_template =  '%10.3f'
    metric_template = pyqtProperty(str, getmetrictemplate, setmetrictemplate, resetmetrictemplate)

    def setimperialtexttemplate(self, data):
        self.tablemodel.imperial_text_template = data
    def getimperialtexttemplate(self):
        return self.tablemodel.imperial_text_template
    def resetimperialtexttemplate(self):
        self.tablemodel.imperial_text_template =  '%9.4f'
    imperial_template = pyqtProperty(str, getimperialtexttemplate, setimperialtexttemplate, resetimperialtexttemplate)

    def getColorHighlight(self):
        return QColor(self.tablemodel._highlightcolor)
    def setColorHighlight(self, value):
        self.tablemodel._highlightcolor = value.name()
        #self.tablemodel.layoutChanged.emit()
    styleColorHighlight = pyqtProperty(QColor, getColorHighlight, setColorHighlight)

    def getColorSelection(self):
        return QColor(self.tablemodel._selectedcolor)
    def setColorSelection(self, value):
        self.tablemodel._selectedcolor = value.name()
        #self.tablemodel.layoutChanged.emit()
    styleColorSelection = pyqtProperty(QColor, getColorSelection, setColorSelection)

#########################################
# custom model
#########################################
class MyTableModel(QAbstractTableModel):
    def __init__(self, parent=None):
        """
        Args:
            datain: a list of lists\n
            headerdata: a list of strings
        """
        super(MyTableModel, self).__init__(parent)
        self.text_template = '%.4f'
        self.metric_text_template = '%10.3f'
        self.zero_text_template = '%10.1f'
        self.imperial_text_template = '%9.4f'
        self.degree_text_template = '%10.1f'
        self.metric_display = False
        self.diameter_display = False
        self._highlightcolor = '#00ffff'
        self._selectedcolor = '#00ff00'
        self.headerdata = ['','tool','pocket','X','X Wear', 'Y', 'Y Wear', 'Z', 'Z Wear', 'A', 'B', 'C', 'U', 'V', 'W', 'Diameter', 'Front Angle', 'Back Angle','Orient','Comment']
        if INFO.MACHINE_IS_LATHE:
            self.headerdata[2] = 'Stn'
        self.vheaderdata = []
        self.arraydata = [[0, 0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0, 0,'No Tool']]
        STATUS.connect('toolfile-stale',lambda o, d: self.update(d))
        self.update(None)

    def metricDisplay(self, state):
        self.metric_display = state
        self.layoutChanged.emit()

    def diameterDisplay(self, state):
        self.diameter_display = state
        self.layoutChanged.emit()

    # make a list of all the checked tools
    def listCheckedTools(self):
        checkedlist = []
        for row in self.arraydata:
            if row[0].isChecked():
                checkedlist.append(row[1])
        return checkedlist

    def uncheckAllTools(self):
        for row in self.arraydata:
            if row[0].isChecked():
                row[0].setChecked(False)

    def toggle(self, row):
        item = self.arraydata[row][0]
        newState = (not item.isChecked())
        self.uncheckAllTools()
        item.setChecked(newState)
        self.layoutChanged.emit()

    # update the internal array from STATUS's toolfile read array
    # we make sure the first array is switched to a QCheckbox widget
    def update(self, models):
        data = TOOL.CONVERT_TO_WEAR_TYPE(models)
        if data in (None, []):
            data = [[QCheckBox(),0, 0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0, 0,'No Tool']]
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

        elif role == Qt.DecorationRole and index.column() == 18:
            value = self.arraydata[index.row()][index.column()]
            return QIcon(os.path.join(ICONPATH, "tool_pos_{}.png".format(value)))

        elif role == Qt.DisplayRole:
            value = self.arraydata[index.row()][index.column()]
            col = index.column()
            if isinstance(value, float):
                if value == 0.0:
                    tmpl = lambda s: self.zero_text_template % s
                    return tmpl(value)
                elif col in(16,17):
                    tmpl = lambda s: self.degree_text_template % s
                    return tmpl(value)
                elif self.metric_display:
                    tmpl = lambda s: self.metric_text_template % s
                else:
                    tmpl = lambda s: self.imperial_text_template % s
                if self.metric_display != INFO.MACHINE_IS_METRIC:
                    value = INFO.convert_units(value)
                if col in(3,4):
                    if self.diameter_display:
                        value *=2
                        self.headerdata[3] = 'X D'
                        self.headerdata[4] = 'X Wear D'
                    else:
                        self.headerdata[3] = 'X R'
                        self.headerdata[4] = 'X Wear R'
                return tmpl(value)

            if isinstance(value, str):
                return '%s' % value
            # Default (anything not captured above: e.g. int)
            return value

        elif role == Qt.BackgroundRole:
            value = self.arraydata[index.row()][index.column()]
            if (isinstance(value, int) or isinstance(value, float) or
                  isinstance(value, str), isinstance(value, QCheckBox)):
                if self.arraydata[index.row()][1] == self.parent().current_tool:
                    return QColor(self._highlightcolor)
                elif self.arraydata[index.row()][0].isChecked():
                    return QColor(self._selectedcolor)
                else:
                    return QVariant()

        elif role == Qt.CheckStateRole:
            if index.column() == 0:
                # print(">>> data() row,col = %d, %d" % (index.row(), index.column()))
                if self.arraydata[index.row()][index.column()].isChecked():
                    return Qt.Checked
                else:
                    return Qt.Unchecked

        elif role == Qt.ForegroundRole:
            value = self.arraydata[index.row()][index.column()]

            if ((isinstance(value, int) or isinstance(value, float))
                and value < 0 ):
                return QColor('red')

        return QVariant()


    # Returns the item flags for the given index.
    def flags(self, index):
        if not index.isValid():
            return None
        if index.column() == 0:
            return Qt.ItemIsEnabled | Qt.ItemIsUserCheckable | Qt.ItemIsSelectable
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
                self.uncheckAllTools()
                self.arraydata[index.row()][index.column()].setChecked(True)
                #self.arraydata[index.row()][index.column()].setText("Delete")
                #print 'selected',self.arraydata[index.row()][1]
            else:
                self.arraydata[index.row()][index.column()].setChecked(False)
                #self.arraydata[index.row()][index.column()].setText("Un")
            # don't emit dataChanged - return right away
            self.parent().reset()
            return True


        try:
            if col in (1,2,18): # tool, pocket, orientation
                v = int(value)
            elif col == 19:
                v = str(value) # comment
            else:
                v = float(value)
                if self.metric_display:
                    v = INFO.convert_metric_to_machine(value)
                else:
                    v = INFO.convert_imperial_to_machine(value)
                if col in(3,4) and self.diameter_display:
                    v /=2
            self.arraydata[index.row()][col] = v
        except:
            LOG.error("Invalid data type in row {} column:{} ".format(index.row(), col))
            return False
        LOG.debug(">>> setData() value = {} ".format(value))
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
        # don't sort checkbox column
        if Ncol != 0:
            self.layoutAboutToBeChanged.emit()
            self.arraydata = sorted(self.arraydata, key=operator.itemgetter(Ncol))
            if order == Qt.DescendingOrder:
                self.arraydata.reverse()
            self.layoutChanged.emit()

if __name__ == "__main__":
    from PyQt5.QtWidgets import QApplication
    app = QApplication(sys.argv)
    w = ToolOffsetView()
    w.setEnabled(True)
    w._hal_init()
    w.highlight('lightblue')
    #w.setProperty('styleColorHighlight',QColor('purple'))
    w.show()
    sys.exit(app.exec_())
