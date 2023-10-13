from PyQt5.QtGui import *
from PyQt5.QtCore import *
from PyQt5.QtWidgets import *

class DetachTabWidget(QTabWidget):
    def __init__(self, parent=None):
        super(DetachTabWidget, self).__init__(parent)

        self.tabBar = self.TabBar(self)
        self.tabBar.onDetachTabSignal.connect(self.detachTab)
        self.tabBar.onMoveTabSignal.connect(self.moveTab)

        self.setTabBar(self.tabBar)


    ##
    #  The default movable functionality of QTabWidget must remain disabled
    #  so as not to conflict with the added features
    def setMovable(self, movable):
        pass

    ##
    #  Move a tab from one position (index) to another
    #
    #  @param    fromIndex    the original index location of the tab
    #  @param    toIndex      the new index location of the tab
    @pyqtSlot(int, int)
    def moveTab(self, fromIndex, toIndex):
        widget = self.widget(fromIndex)
        icon = self.tabIcon(fromIndex)
        text = self.tabText(fromIndex)

        self.removeTab(fromIndex)
        self.insertTab(toIndex, widget, icon, text)
        self.setCurrentIndex(toIndex)


    ##
    #  Detach the tab by removing it's contents and placing them in
    #  a DetachedTab dialog
    #
    #  @param    index    the index location of the tab to be detached
    #  @param    point    the screen position for creating the new DetachedTab dialog
    @pyqtSlot(int, QPoint)
    def detachTab(self, index, point):

        # Get the tab content
        name = self.tabText(index)
        icon = self.tabIcon(index)        
        if icon.isNull():
            icon = self.window().windowIcon()              
        contentWidget = self.widget(index)
        contentWidgetRect = contentWidget.frameGeometry()

        # Create a new detached tab window
        detachedTab = self.DetachedTab(contentWidget, self.parentWidget())
        detachedTab.setWindowModality(Qt.NonModal)
        detachedTab.setWindowTitle(name + '  - Close to reattach')
        detachedTab.setWindowIcon(icon)
        detachedTab.setObjectName(name)
        detachedTab.setGeometry(contentWidgetRect)
        detachedTab.onCloseSignal.connect(self.attachTab)
        detachedTab.move(point)
        detachedTab.show()

    def attachTab1(self, contentWidget, name, icon):

        # Make the content widget a child of this widget
        contentWidget.setParent(self)


        # Create an image from the given icon
        if not icon.isNull():
            tabIconPixmap = icon.pixmap(icon.availableSizes()[0])
            tabIconImage = tabIconPixmap.toImage()
        else:
            tabIconImage = None


        # Create an image of the main window icon
        if not icon.isNull():
            windowIconPixmap = self.window().windowIcon().pixmap(icon.availableSizes()[0])
            windowIconImage = windowIconPixmap.toImage()
        else:
            windowIconImage = None


        # Determine if the given image and the main window icon are the same.
        # If they are, then do not add the icon to the tab
        if name  == 'Model Selection':
            index = 0
        elif name  == "Model' Parameters":
            index = 1
        elif name  == 'Stim settings':
            index = 2
        elif name  == 'Parameter evolution settings':
            index = 3
        elif name  == 'LambdaE':
            index = 4
        elif name  == 'Simulation settings':
            index = 5
        elif name  == 'LFP + PPS + Pulse Results':
            index = 6
        if tabIconImage == windowIconImage:
            index = self.insertTab(index,contentWidget, name)
            # index = self.addTab(contentWidget, name)
        else:
            # index = self.addTab(contentWidget, icon, name)
            index = self.insertTab(index,contentWidget, icon, name)



        # Make this tab the current tab
        if index > -1:
            self.setCurrentIndex(index)


    ##
    #  Re-attach the tab by removing the content from the DetachedTab dialog,
    #  closing it, and placing the content back into the DetachTabWidget
    #
    #  @param    contentWidget    the content widget from the DetachedTab dialog
    #  @param    name             the name of the detached tab
    #  @param    icon             the window icon for the detached tab
    @pyqtSlot(QWidget, type(''), QIcon)
    def attachTab(self, contentWidget, name, icon):

        # Make the content widget a child of this widget
        contentWidget.setParent(self)


        # Create an image from the given icon
        if not icon.isNull():
            tabIconPixmap = icon.pixmap(icon.availableSizes()[0])
            tabIconImage = tabIconPixmap.toImage()
        else:
            tabIconImage = None


        # Create an image of the main window icon
        if not icon.isNull():
            windowIconPixmap = self.window().windowIcon().pixmap(icon.availableSizes()[0])
            windowIconImage = windowIconPixmap.toImage()
        else:
            windowIconImage = None


        # Determine if the given image and the main window icon are the same.
        # If they are, then do not add the icon to the tab
        if tabIconImage == windowIconImage:
            index = self.addTab(contentWidget, name)
        else:
            index = self.addTab(contentWidget, icon, name)


        # Make this tab the current tab
        if index > -1:
            self.setCurrentIndex(index)


    ##
    #  When a tab is detached, the contents are placed into this QDialog.  The tab
    #  can be re-attached by closing the dialog or by double clicking on its
    #  window frame.
    class DetachedTab(QDialog):
        onCloseSignal = pyqtSignal(QWidget,type(''), QIcon)

        def __init__(self, contentWidget, parent=None):
            QDialog.__init__(self, parent)

            layout = QVBoxLayout(self)            
            self.contentWidget = contentWidget            
            layout.addWidget(self.contentWidget)
            self.contentWidget.show()
            self.setWindowFlags(Qt.Window)


        ##
        #  Capture a double click event on the dialog's window frame
        #
        #  @param    event    an event
        #
        #  @return            true if the event was recognized
        def event(self, event):

            # If the event type is QEvent.NonClientAreaMouseButtonDblClick then
            # close the dialog
            if event.type() == 176:
                event.accept()
                self.close()

            return QDialog.event(self, event)


        ##
        #  If the dialog is closed, emit the onCloseSignal and give the
        #  content widget back to the DetachTabWidget
        #
        #  @param    event    a close event
        def closeEvent(self, event):
            self.onCloseSignal.emit(self.contentWidget, self.objectName(), self.windowIcon())


    ##
    #  The TabBar class re-implements some of the functionality of the QTabBar widget
    class TabBar(QTabBar):
        onDetachTabSignal = pyqtSignal(int, QPoint)
        onMoveTabSignal = pyqtSignal(int, int)

        def __init__(self, parent=None):
            QTabBar.__init__(self, parent)

            self.setAcceptDrops(True)
            self.setElideMode(Qt.ElideRight)
            self.setSelectionBehaviorOnRemove(QTabBar.SelectLeftTab)

            self.dragStartPos = QPoint()
            self.dragDropedPos = QPoint()
            self.mouseCursor = QCursor()
            self.dragInitiated = False


        ##
        #  Send the onDetachTabSignal when a tab is double clicked
        #
        #  @param    event    a mouse double click event
        def mouseDoubleClickEvent(self, event):
            event.accept()
            self.onDetachTabSignal.emit(self.tabAt(event.pos()), self.mouseCursor.pos())


        ##
        #  Set the starting position for a drag event when the mouse button is pressed
        #
        #  @param    event    a mouse press event
        def mousePressEvent(self, event):
            if event.button() == Qt.LeftButton:
                self.dragStartPos = event.pos()

            self.dragDropedPos.setX(0)
            self.dragDropedPos.setY(0)

            self.dragInitiated = False

            QTabBar.mousePressEvent(self, event)


        ##
        #  Determine if the current movement is a drag.  If it is, convert it into a QDrag.  If the
        #  drag ends inside the tab bar, emit an onMoveTabSignal.  If the drag ends outside the tab
        #  bar, emit an onDetachTabSignal.
        #
        #  @param    event    a mouse move event
        def mouseMoveEvent(self, event):

            # Determine if the current movement is detected as a drag
            if not self.dragStartPos.isNull() and ((event.pos() - self.dragStartPos).manhattanLength() > QApplication.startDragDistance()):
                self.dragInitiated = True

            # If the current movement is a drag initiated by the left button
            if (((event.buttons() & Qt.LeftButton)) and self.dragInitiated):

                # Stop the move event
                finishMoveEvent = QMouseEvent(QEvent.MouseMove, event.pos(), Qt.NoButton, Qt.NoButton, Qt.NoModifier)
                QTabBar.mouseMoveEvent(self, finishMoveEvent)

                # Convert the move event into a drag
                drag = QDrag(self)
                mimeData = QMimeData()
                mimeData.setData('action', b'application/tab-detach')
                drag.setMimeData(mimeData)

                #Create the appearance of dragging the tab content
                pixmap = self.parentWidget().grab()
                targetPixmap = QPixmap(pixmap.size())
                targetPixmap.fill(Qt.transparent)
                painter = QPainter(targetPixmap)
                painter.setOpacity(0.85)
                painter.drawPixmap(0, 0, pixmap)
                painter.end()
                drag.setPixmap(targetPixmap)

                # Initiate the drag
                dropAction = drag.exec_(Qt.MoveAction | Qt.CopyAction)

                # If the drag completed outside of the tab bar, detach the tab and move
                # the content to the current cursor position
                if dropAction == Qt.IgnoreAction:
                    event.accept()
                    self.onDetachTabSignal.emit(self.tabAt(self.dragStartPos), self.mouseCursor.pos())

                # Else if the drag completed inside the tab bar, move the selected tab to the new position
                elif dropAction == Qt.MoveAction:
                    if not self.dragDropedPos.isNull():
                        event.accept()
                        self.onMoveTabSignal.emit(self.tabAt(self.dragStartPos), self.tabAt(self.dragDropedPos))
            else:
                QTabBar.mouseMoveEvent(self, event)


        ##
        #  Determine if the drag has entered a tab position from another tab position
        #
        #  @param    event    a drag enter event
        def dragEnterEvent(self, event):
            mimeData = event.mimeData()
            formats = mimeData.formats()

            if 'action' in formats and mimeData.data('action') == 'application/tab-detach':
                event.acceptProposedAction()

            QTabBar.dragMoveEvent(self, event)


        ##
        #  Get the position of the end of the drag
        #
        #  @param    event    a drop event
        def dropEvent(self, event):
            self.dragDropedPos = event.pos()
            QTabBar.dropEvent(self, event)

class SurfViewer(QMainWindow):
    def __init__(self, parent=None):
        super(SurfViewer, self).__init__()
        self.parent = parent
        self.centralTabs= DetachTabWidget()
        self.setCentralWidget(self.centralTabs)
        self.setFixedWidth(200)
        self.setFixedHeight(200)

        #tab 1
        self.tab_1 = QWidget()
        self.centralTabs.addTab(self.tab_1,"Label 1")
        vbox = QVBoxLayout()
        Label = QLabel('Tab1')
        Label.setFixedWidth(180)
        LineEdit = QLineEdit('Tab1')
        LineEdit.setFixedWidth(180)
        vbox.addWidget(Label)
        vbox.addWidget(LineEdit)
        vbox.setAlignment(Qt.AlignTop)
        self.tab_1.setLayout(vbox)

        #tab 2
        self.tab_2 = QWidget()
        self.centralTabs.addTab(self.tab_2,"Label 2")
        vbox = QVBoxLayout()
        Label = QLabel('Tab2')
        Label.setFixedWidth(180)
        LineEdit = QLineEdit('Tab2')
        LineEdit.setFixedWidth(180)
        vbox.addWidget(Label)
        vbox.addWidget(LineEdit)
        vbox.setAlignment(Qt.AlignTop)
        self.tab_2.setLayout(vbox)

if __name__ == '__main__':
    import sys
    app = QApplication(sys.argv)
    ex = SurfViewer(app)
    ex.setWindowTitle('window')
    ex.show()
    sys.exit(app.exec_( ))

