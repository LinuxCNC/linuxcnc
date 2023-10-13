from PyQt5.QtCore import pyqtProperty, pyqtSignal, QSize, QObject, Qt
from PyQt5.QtGui import QDoubleValidator, QIntValidator
from PyQt5.QtWidgets import QWidget, QPushButton,QLineEdit, QHBoxLayout

import decimal
from decimal import Decimal


class LineEdit(QLineEdit):

    clicked = pyqtSignal(bool)

    def __init__(self, parent=None):
        super().__init__(parent)
        #self.clicked.connect(lambda :self.entryClicked())

    def mousePressEvent(self,event):
        self.clicked.emit(True)

class TouchSpinBox(QWidget):
    def __init__(self, parent=None):
        super().__init__(parent)
        self._value = 0
        self._multiplierIndex = 0

        self.setMinimumSize(QSize(200, 25))    
        self.setMaximumSize(QSize(300, 50))    

        lay = QHBoxLayout()
        lay.setContentsMargins(0,0,0,0)
        lay.setSpacing(0)
        self.setLayout(lay)

        self.entry = LineEdit()
        self.entry.setMaximumHeight(100)
        self.entry.setMinimumWidth(100)

        self.entry.clicked.connect(lambda :self.callDialog(self))
        lay.addWidget(self.entry)

        up = QPushButton('Up')
        up.setMaximumHeight(100)
        up.setMaximumWidth(40)
        up.pressed.connect(lambda :self.updateValue(True))
        lay.addWidget(up)

        self.select = QPushButton(str('<-'))
        self.select.setMaximumHeight(100)
        self.select.setMaximumWidth(40)
        self.select.pressed.connect(lambda :self.moveLeft())
        lay.addWidget(self.select)

        self.selectR = QPushButton(str('->'))
        self.selectR.setMaximumHeight(100)
        self.selectR.setMaximumWidth(40)
        self.selectR.pressed.connect(lambda :self.moveRight())
        lay.addWidget(self.selectR)

        self.down = QPushButton('Dwn')
        self.down.setMaximumHeight(100)
        self.down.setMaximumWidth(40)
        self.down.pressed.connect(lambda :self.updateValue(False))
        lay.addWidget(self.down)

        self.init_settings()

    def init_settings(self):
        self.entry.setText(str('00'))
        self.validInt = QIntValidator(-99999, 99999)
        self.entry.setValidator(self.validInt)

    def updateValue(self, state):
        mult = pow(10,self._multiplierIndex)
        print('update mult:',mult)
        decimal.getcontext().prec = 8
        x = Decimal(self._value) + Decimal( mult)
        print('Dec:',x)
        if state:
            self.setValue(Decimal(self._value) + Decimal( mult))
        else:
            self.setValue(Decimal(self._value) - Decimal( mult))

    def moveLeft(self):
        mult = self._multiplierIndex + 1
        t = len(self.entry.text())
        print('mult',mult, 't',t)
        if t < 0:t=0
        if mult >= t:
            mult = 0

        pwr = pow(10,mult)
        print('pow',pwr)

        self._multiplierIndex = mult
        #self.select.setText(str(pow(10,mult)))
        print('left',t-self._multiplierIndex-1,mult)
        self.entry.setSelection (t-self._multiplierIndex-1,1)

    def moveRight(self):
        mult = self._multiplierIndex - 1
        t = len(self.entry.text())
        print('mult',mult, 't',t)
        if t < 0:t=0
        if mult < 0:
            mult = t-1

        pwr = pow(10,mult)
        print('pow',pwr)

        self._multiplierIndex = mult
        #self.select.setText(str(pow(10,mult)))
        print('right',t-self._multiplierIndex-1,mult)
        self.entry.setSelection (t-self._multiplierIndex-1,1)

    def text(self):
        return self.entry.text()

    def value(self):
        return float(self.entry.text())

    def setValue(self, value):
        self._value = value
        self.entry.setText(str(value))

    # class patch or should we call a dialog from here too?
    def callDialog(self,widget):
        pass

class TouchDoubleSpinBox(TouchSpinBox):
    def __init__(self, parent=None):
        super(TouchDoubleSpinBox, self).__init__(parent)

    def init_settings(self):
        self._value = 10.001
        self.validDouble = QDoubleValidator(-999.999, 999.999, 3)
        self.entry.setText(str('10.001'))
        self.entry.setValidator(self.validDouble)
        self.entry.returnPressed.connect(lambda :self.entryUpdate())

    def updateValue(self, state):
        def convert(x):
            return ['0','1','2','3','4','5','6','7','8','9','-',' ','end'][x]
        def indexof(x):
            return ['0','1','2','3','4','5','6','7','8','9','-',' ','end'].index(x)
        if self.entry.text() == '':
            self.entry.setText('0')
        pos = self.getPosition(self._multiplierIndex)
        x = self.entry.text()[pos]
        print('update txt:',x,'pos',pos)
        if state:
            next = convert(indexof(x)+1)
            if next in( '-',' ','end'):
                if pos == 0:
                    if next != 'end':
                        pass
                    else:
                        next = 0
                else:
                    next = 0
        else:
            next = convert(indexof(x)-1)
            if next in( '-',' ','end'):
                if pos == 0:
                    if next != 'end':
                        pass
                    else:
                        next = 0
                else:
                    next = 0
        self.entry.setText( self.replace_at_index(self.entry.text(),pos,str(next)))
        self.entry.setSelection (pos,1)

    def replace_at_index(self,text,index=0,replacement=''):
        return '%s%s%s'%(text[:index],replacement,text[index+1:])

    def getPosition(self, index):
        t = len(self.entry.text())-1
        print('T:',t,'Index:',index)
        if t < 0:t=0
        if index > t:
            self.entry.setText('0'+ self.entry.text())
            t = len(self.entry.text())-1
            #index = 0

        if self.entry.text()[t-index] in ('.',','):
            index+=1
        pos = t-index
        self._multiplierIndex = index
        return pos

    def moveLeft(self):
        index = self._multiplierIndex +1
        t = len(self.entry.text())-1
        print('T:',t,'Index:',index)
        if t < 0:t=0
        if index == -1:
            if '.' in self.entry.text():
                self.entry.setText(self.entry.text()+'0')
                t = len(self.entry.text())-1
                index = 0
            else:
                self.entry.setText(self.entry.text()+'.0')
                t = len(self.entry.text())-1
                index = 0


        if self.entry.text()[t-index] in ('.',','):
            index-=1
        pos = t-index
        self._multiplierIndex = index

        try:
            print('text;',self.entry.text()[pos])
        except:
            pass
        #self.select.setText(str(pos))
        self.entry.setSelection (pos,1)

    def moveRight(self):
        index = self._multiplierIndex -1
        t = len(self.entry.text())-1
        print('T:',t,'Index:',index)
        if t < 0:t=0
        if index == -1:
            if '.' in self.entry.text():
                self.entry.setText(self.entry.text()+'0')
                t = len(self.entry.text())-1
                index = 0
            else:
                self.entry.setText(self.entry.text()+'.0')
                t = len(self.entry.text())-1
                index = 0


        if self.entry.text()[t-index] in ('.',','):
            index-=1
        pos = t-index
        self._multiplierIndex = index

        try:
            print('text;',self.entry.text()[pos])
        except:
            pass
        #self.select.setText(str(pos))
        self.entry.setSelection (pos,1)

    def moveLeft(self):
        pos = self.getPosition(self._multiplierIndex +1)

        try:
            print('text;',self.entry.text()[pos])
        except:
            pass

        #self.select.setText(str(pos))
        self.entry.setSelection (pos,1)

    def setValue(self, value):
        self.entry.setText('{:4.3f}'.format(value))

    def entryUpdate(self):
        print('Update:',self.entry.text())
        if self.entry.text() == '':
            self.setValue(0)

# for direct testing
if __name__ == "__main__":
    from PyQt5.QtWidgets import *
    from PyQt5.QtCore import *
    from PyQt5.QtGui import *
    import sys

    app = QApplication(sys.argv)
    w = TouchSpinBox()
    w.show()
    w = TouchDoubleSpinBox()
    w.show()
    sys.exit( app.exec_() )
