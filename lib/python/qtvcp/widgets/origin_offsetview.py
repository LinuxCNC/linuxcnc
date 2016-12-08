import sys, os
from PyQt4.QtCore import *
from PyQt4.QtGui import *

# localization
import locale
print sys.argv
#BASE = os.path.abspath(os.path.join(os.path.dirname(sys.argv[0]), ".."))
#LOCALEDIR = os.path.join(BASE, "share", "locale")
#locale.setlocale(locale.LC_ALL, '')

import linuxcnc
from qtvcp.widgets.simple_widgets import _HalWidgetBase
from qtvcp.qt_glib import GStat, Lcnc_Action
from qtvcp.qt_istat import IStat
GSTAT = GStat()
ACTION = Lcnc_Action()
INI = IStat()

class Lcnc_OriginOffsetView(QTableView, _HalWidgetBase):
    def __init__(self, parent=None):
        super(Lcnc_OriginOffsetView, self).__init__(parent)

        self.filename = '../../../configs/sim/qtscreen/sim.var'
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
        # create table
        self.get_table_data()
        self.table = self.createTable()

    def _hal_init(self):
        self.delay = 0
        GSTAT.connect('all-homed', lambda w: self.setEnabled(True))
        GSTAT.connect('periodic', self.periodic_check)
        GSTAT.connect('metric-mode-changed', lambda w, data: self.metricMode(data))
        GSTAT.connect('tool-in-spindle-changed', lambda w, data: self.currentTool(data))
        GSTAT.connect('user-system-changed', self._convert_system)
        for num in range(0,9):
            if num in (INI.AVAILABLE_AXES_INT):
                continue
            self.hideColumn(num)

    def _convert_system(self, w, data):
        convert = ("None", "G54", "G55", "G56", "G57", "G58", "G59", "G59.1", "G59.2", "G59.3")
        self.current_system = convert[int(data)]

    def currentTool(self, data):
        self.current_tool = data
    def metricMode(self, state):
        self.metric_display = state

    def get_table_data(self):
        self.tabledata = [[0, 0, 1, 0, 0, 0, 0, 0, 0,'Current System'],
                          [None, None, 2, None, None, None, None, None, None,'Rotational Offsets'],
                          [0, 0, 3, 0, 0, 0, 0, 0, 0,'G92 Offsets'],
                          [0, 0, 0, 0, 0, 0, 0, 0, 0,'Current Tool'],
                          [0, 0, 4, 0, 0, 0, 0, 0, 0,'System 1'],
                          [0, 0, 5, 0, 0, 0, 0, 0, 0,'System 2'],
                          [0, 0, 6, 0, 0, 0, 0, 0, 0,'System 3'],
                          [0, 0, 7, 0, 0, 0, 0, 0, 0,'System 4'],
                          [0, 0, 8, 0, 0, 0, 0, 0, 0,'System 5'],
                          [0, 0, 9, 0, 0, 0, 0, 0, 0,'System 6'],
                          [0, 0, 10, 0, 0, 0, 0, 0, 0,'System 7'],
                          [0, 0, 11, 0, 0, 0, 0, 0, 0,'System 8'],
                          [0, 0, 12, 0, 0, 0, 0, 0, 0,'System 9']]
        self.reload_offsets()

    def createTable(self):
        # create the view
        self.setSelectionMode(QAbstractItemView.SingleSelection)

        # set the table model
        header = [ 'X', 'Y', 'Z', 'A', 'B', 'C', 'U', 'V', 'W', 'Name']
        vheader = ['G5x', 'Rot', 'G92', 'Tool', 'G54', 'G55', 'G56', 'G57', 'G58', 'G59', 'G59.1', 'G59.2', 'G59.3']
        tablemodel = MyTableModel(self.tabledata, header, vheader, self)
        self.setModel(tablemodel)
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
        text = cellContent.toPyObject()  # test
        print text, item.row(), item.column()
        sf = "You clicked on {}".format(text)
        # display in title bar for convenience
        self.setWindowTitle(sf)

    #############################################################

    # Reload the offsets into display
    def reload_offsets(self):
        g54, g55, g56, g57, g58, g59, g59_1, g59_2, g59_3 = self.read_file()
        if g54 == None: return
        # Get the offsets arrays and convert the units if the display
        # is not in machine native units
        try:
            self.status.poll()
            self.IS_RUNNING = True
        except:
            self.current_system = "G54"
            self.IS_RUNNING = False

        g5x = self.status.g5x_offset
        tool = self.status.tool_offset
        g92 = self.status.g92_offset
        rot = self.status.rotation_xy

        if self.metric_display != INI.MACHINE_IS_METRIC:
            g5x = INI.convert_units_9(g5x)
            tool = INI.convert_units_9(tool)
            g92 = INI.convert_units_9(g92)
            g54 = INI.convert_units_9(g54)
            g55 = INI.convert_units_9(g55)
            g56 = INI.convert_units_9(g56)
            g57 = INI.convert_units_9(g57)
            g58 = INI.convert_units_9(g58)
            g59 = INI.convert_units_9(g59)
            g59_1 = INI.convert_units_9(g59_1)
            g59_2 = INI.convert_units_9(g59_2)
            g59_3 = INI.convert_units_9(g59_3)

        # set the text style based on unit type
        if self.metric_display:
            tmpl = self.mm_text_template
        else:
            tmpl = self.imperial_text_template

        degree_tmpl = "%11.2f"

        # fill each row of the liststore fron the offsets arrays
        for row, i in enumerate([g5x, rot, g92, tool, g54, g55, g56, g57, g58, g59, g59_1, g59_2, g59_3]):
            for column in range(0, 9):
                if row == 1:
                    if column == 2:
                        self.tabledata[row][column]  = locale.format(degree_tmpl, rot)
                    else:
                        self.tabledata[row][column] = " "
                else:
                    self.tabledata[row][column] = locale.format(tmpl, i[column])

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
            if self.filename == None:
                return g54, g55, g56, g57, g58, g59, g59_1, g59_2, g59_3
            if not os.path.exists(self.filename):
                print 'no file', self.filename
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

    def dataChanged(self, new,old):
        row = new.row()
        col = new.column()
        data = self.tabledata[row][col]

        # Hack to not edit any rotational offset but Z axis
        if row == 2 and not col == 3: return

        # set the text style based on unit type
        if self.metric_display:
            tmpl = lambda s: self.mm_text_template % s
        else:
            tmpl = lambda s: self.imperial_text_template % s

        # make sure we switch to correct units for machine and rotational, row 2, does not get converted
        try:
                qualified = float(data)
                #qualified = float(locale.atof(data))
        except:
            print 'error'
        # now update linuxcnc to the change
        try:
            if self.IS_RUNNING:
                ACTION.RECORD_CURRENT_MODE()
                if row == 0: # current Origin
                    ACTION.CALL_MDI("G10 L2 P0 %s %10.4f" % (self.axisletters[col], qualified))
                elif row == 1: # rotational
                    if col == 2: # Z axis only
                        ACTION.CALL_MDI("G10 L2 P0 R %10.4f" % (qualified))
                elif row == 2: # G92 offset
                    ACTION.CALL_MDI("G92 %s %10.4f" % (self.axisletters[col], qualified))
                elif row == 3: # Tool
                    if not self.current_tool == 0:
                        ACTION.CALL_MDI("G10 L1 P%d %s %10.4f" % (self.current_tool, self.axisletters[col], qualified))
                        ACTION.CALL_MDI('g43')
                else:
                        ACTION.CALL_MDI("G10 L2 P%d %s %10.4f" % (row-3, self.axisletters[col], qualified))

                ACTION.UPDATE_VAR_FILE()
                ACTION.RESTORE_RECORDED_MODE()
                GSTAT.emit('reload-display')
                self.reload_offsets()
        except Exception, e:
            print e
            print "offsetpage widget error: MDI call error"
            self.reload_offsets()

    # only update every 10th time periodic calls
    def periodic_check(self,w):
        if self.delay <99:
            self.delay +=1
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
        QAbstractTableModel.__init__(self, parent)
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
        if not index.isValid():
            return QVariant()
        elif role != Qt.DisplayRole:
            return QVariant()
        return QVariant(self.arraydata[index.row()][index.column()])

    def flags(self, index):
        if not index.isValid():
            return None
        # print(">>> flags() index.column() = ", index.column())
        if index.column() == 9 and index.row() in(0,1,2,3) :
            return Qt.ItemIsEnabled
        elif index.row() == 1 and not index.column() == 2:
            return Qt.NoItemFlags
        else:
            return Qt.ItemIsEditable | Qt.ItemIsEnabled | Qt.ItemIsSelectable


    def setData(self, index, value, role):
        if not index.isValid():
            return False
        print self.arraydata[index.row()][index.column()]
        print(">>> setData() role = ", role)
        print(">>> setData() index.column() = ", index.column())
        if index.column() == 9:
            v=str(value.toPyObject())
        else:
            v=float(value.toPyObject())
        print(">>> setData() value = ", v)
        # self.emit(SIGNAL("dataChanged(QModelIndex,QModelIndex)"), index, index)
        print(">>> setData() index.row = ", index.row())
        print(">>> setData() index.column = ", index.column())
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
    app = QApplication(sys.argv)
    w = Lcnc_OriginOffsetView()
    w.show()
    sys.exit(app.exec_())


