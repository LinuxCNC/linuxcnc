import sys

from PyQt5 import QtWidgets
#PYQT5 QSpinBox, QMessageBox, QDialog, QPushButton, QGridLayout, QLabel



from PyQt5 import QtGui, QtCore
from PyQt5.QtCore import Qt

class Table(QtWidgets.QDialog):
    def __init__(self,parent = None):
        QtWidgets.QDialog.__init__(self, parent)

        self.parent = parent
         
        self.initUI()
 
    def initUI(self):

        # Rows
        rowsLabel = QtWidgets.QLabel("Rows: ",self)
        
        self.rows = QtWidgets.QSpinBox(self)

        # Columns
        colsLabel = QtWidgets.QLabel("Columns",self)
        
        self.cols = QtWidgets.QSpinBox(self)

        # Cell spacing (distance between cells)
        spaceLabel = QtWidgets.QLabel("Cell spacing",self)
        
        self.space = QtWidgets.QSpinBox(self)

        # Cell padding (distance between cell and inner text)
        padLabel = QtWidgets.QLabel("Cell padding",self)

        self.pad = QtWidgets.QSpinBox(self)
        
        self.pad.setValue(10)

        # Button
        insertButton = QtWidgets.QPushButton("Insert",self)
        insertButton.clicked.connect(self.insert)

        # Layout
        layout = QtWidgets.QGridLayout()

        layout.addWidget(rowsLabel,0,0)
        layout.addWidget(self.rows,0,1)

        layout.addWidget(colsLabel,1,0)
        layout.addWidget(self.cols,1,1)

        layout.addWidget(padLabel,2,0)
        layout.addWidget(self.pad,2,1)
        
        layout.addWidget(spaceLabel,3,0)
        layout.addWidget(self.space,3,1)

        layout.addWidget(insertButton,4,0,1,2)

        self.setWindowTitle("Insert Table")
        self.setGeometry(300,300,200,100)
        self.setLayout(layout)

    def insert(self):

        cursor = self.parent.text.textCursor()

        # Get the configurations
        rows = self.rows.value()

        cols = self.cols.value()

        if not rows or not cols:

            popup = QtWidgets.QMessageBox(QtWidgets.QMessageBox.Warning,
                                      "Parameter error",
                                      "Row and column numbers may not be zero!",
                                      QtWidgets.QMessageBox.Ok,
                                      self)
            popup.show()

        else:

            padding = self.pad.value()

            space = self.space.value()

            # Set the padding and spacing
            fmt = QtGui.QTextTableFormat()
            
            fmt.setCellPadding(padding)

            fmt.setCellSpacing(space)

            # Inser the new table
            cursor.insertTable(rows,cols,fmt)

            self.close()
