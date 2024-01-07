import os

from PyQt5 import QtGui, QtCore
from PyQt5.QtWidgets import QApplication, QWidget, QListWidget, QLabel, QVBoxLayout, QToolButton, QWidget, QListWidgetItem
from PyQt5.QtGui import QIcon
import sys

current_dir =  os.path.dirname(__file__)
iconBasePath = os.path.join(current_dir, 'graphics')

class Window(QWidget):
    def __init__(self):
        super().__init__()
        self.title = "PyQt5 QListWidget"
        self.top = 200
        self.left = 500
        self.width = 400
        self.height = 300
        self.iconSize = QtCore.QSize(25,25)
        self.InitWindow()

    def InitWindow(self):
        self.setWindowIcon(QtGui.QIcon("icon.png"))
        self.setWindowTitle(self.title)
        self.setGeometry(self.left, self.top, self.width, self.height)
        vbox = QVBoxLayout()
        self.list = QListWidget()
        self.list.setViewMode(QListWidget.IconMode)
        self.list.setResizeMode(QListWidget.Adjust)
        self.list.setIconSize(QtCore.QSize(96,96))
        self.buildTopList()
        self.list.clicked.connect(self.listview_clicked)
        vbox.addWidget(self.list)
        self.label = QLabel()
        self.label.setFont(QtGui.QFont("Sanserif", 15))
        vbox.addWidget(self.label)
        self.setLayout(vbox)
        self.show()

    def buildItem(self,text,icon,tooltip):
        item = QListWidgetItem()
        item.setText(text)
        item.setIcon(QIcon( os.path.join(iconBasePath,icon) ))
        item.setSizeHint(self.iconSize)
        item.setToolTip(tooltip)
        item.setFlags(QtCore.Qt.ItemIsEnabled)
        return item

    def buildTopList(self):
        for num,i in enumerate(('action-add.gif','build.png','action-delete.gif')):
            item = self.buildItem(i,i,i)
            self.list.insertItem(num, item)

    def buildBackList(self):
            item = QListWidgetItem()
            item.setText('upper-level-icon.png')
            item.setIcon(QIcon( os.path.join(iconBasePath,'upper-level-icon.png') ))
            item.setSizeHint(self.iconSize)
            item.setFlags(QtCore.Qt.ItemIsEnabled)
            self.list.insertItem(0, item)


    def listview_clicked(self):
        item = self.list.currentItem()
        print(item)
        self.label.setText(str(item.text()))
        if item.text() == 'build.png':
            self.list.clear()
            self.buildBackList()
        elif item.text() == 'upper-level-icon.png':
            self.list.clear()
            self.buildTopList()

App = QApplication(sys.argv)
window = Window()
sys.exit(App.exec())
