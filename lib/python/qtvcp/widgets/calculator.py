#!/usr/bin/python3

import math
import sys

from PyQt5.QtCore import Qt
from PyQt5.QtWidgets import (QApplication, QGridLayout, QLayout, QLineEdit,
        QSizePolicy, QToolButton, QDialog, QDialogButtonBox, QMenu, QAction,
        QHBoxLayout)
from PyQt5.QtGui import QIcon

from qtvcp.core import Status, Info
STATUS = Status()
INFO = Info()

class Button(QToolButton):
    def __init__(self, text, parent=None):
        super(Button, self).__init__(parent)

        self.setSizePolicy(QSizePolicy.Expanding, QSizePolicy.Preferred)
        self.setText(text)

    def sizeHint(self):
        size = super(Button, self).sizeHint()
        size.setHeight(size.height() + 20)
        size.setWidth(max(size.width(), size.height()))
        return size


class Calculator(QDialog):
    NumDigitButtons = 10
    
    def __init__(self, parent=None):
        super(Calculator, self).__init__(parent)

        self.pendingAdditiveOperator = ''
        self.pendingMultiplicativeOperator = ''

        self.sumInMemory = 0.0
        self.sumSoFar = 0.0
        self.factorSoFar = 0.0
        self.waitingForOperand = True

        self.display = QLineEdit('0')
        self.display.setReadOnly(True)
        self.display.setAlignment(Qt.AlignRight)
        self.display.setMaxLength(15)

        font = self.display.font()
        font.setPointSize(font.pointSize() + 8)
        self.display.setFont(font)

        self.digitButtons = []
        
        for i in range(Calculator.NumDigitButtons):
            self.digitButtons.append(self.createButton(str(i),
                    self.digitClicked))

        self.pointButton = self.createButton(".", self.pointClicked)
        if sys.version_info.major > 2:
            text = str("\N{PLUS-MINUS SIGN}")
        else:
            text = u"\N{PLUS-MINUS SIGN}".encode('utf-8')
        self.changeSignButton = self.createButton(text,
                self.changeSignClicked)

        self.axisButton = self.createAxisButton("Axis X",
                self.axisClicked)
        self.backspaceButton = self.createButton("Backspace",
                self.backspaceClicked)
        self.clearButton = self.createButton("Clear", self.clear)
        self.clearAllButton = self.createButton("Clear All", self.clearAll)

        self.clearMemoryButton = self.createButton("MC", self.clearMemory)
        self.readMemoryButton = self.createButton("MR", self.readMemory)
        self.setMemoryButton = self.createButton("MS", self.setMemory)
        self.addToMemoryButton = self.createButton("M+", self.addToMemory)

        if sys.version_info.major > 2:
            text = str("\N{DIVISION SIGN}")
        else:
            text = u"\N{DIVISION SIGN}".encode('utf-8')
        self.divisionButton = self.createButton(text,
                self.multiplicativeOperatorClicked)
        if sys.version_info.major > 2:
            text = str("\N{MULTIPLICATION SIGN}")
        else:
            text = u"\N{MULTIPLICATION SIGN}".encode('utf-8')
        self.timesButton = self.createButton(text,
                self.multiplicativeOperatorClicked)
        self.minusButton = self.createButton("-", self.additiveOperatorClicked)
        self.plusButton = self.createButton("+", self.additiveOperatorClicked)

        self.squareRootButton = self.createButton("Sqrt",
                self.unaryOperatorClicked)
        if sys.version_info.major > 2:
            text = str("\N{SUPERSCRIPT TWO}")
        else:
            text = u"\N{SUPERSCRIPT TWO}".encode('utf-8')
        self.powerButton = self.createButton(text,
                self.unaryOperatorClicked)
        self.reciprocalButton = self.createButton("1/x",
                self.unaryOperatorClicked)
        self.equalButton = self.createButton("=", self.equalClicked)

        mainLayout = QGridLayout()
        mainLayout.setSizeConstraint(QLayout.SetFixedSize)

        mainLayout.addWidget(self.display, 0, 0, 1, 6)
        mainLayout.addWidget(self.backspaceButton, 1, 0, 1, 1)
        mainLayout.addWidget(self.axisButton, 1, 1, 1, 2)
        mainLayout.addWidget(self.clearButton, 1, 3, 1, 1)
        mainLayout.addWidget(self.clearAllButton, 1, 4, 1, 2)

        mainLayout.addWidget(self.clearMemoryButton, 2, 0)
        mainLayout.addWidget(self.readMemoryButton, 3, 0)
        mainLayout.addWidget(self.setMemoryButton, 4, 0)
        mainLayout.addWidget(self.addToMemoryButton, 5, 0)

        for i in range(1, Calculator.NumDigitButtons):
            row = ((9 - i) / 3) + 2
            column = ((i - 1) % 3) + 1
            mainLayout.addWidget(self.digitButtons[i], row, column)

        mainLayout.addWidget(self.digitButtons[0], 5, 1)
        mainLayout.addWidget(self.pointButton, 5, 2)
        mainLayout.addWidget(self.changeSignButton, 5, 3)

        mainLayout.addWidget(self.divisionButton, 2, 4)
        mainLayout.addWidget(self.timesButton, 3, 4)
        mainLayout.addWidget(self.minusButton, 4, 4)
        mainLayout.addWidget(self.plusButton, 5, 4)

        mainLayout.addWidget(self.squareRootButton, 2, 5)
        mainLayout.addWidget(self.powerButton, 3, 5)
        mainLayout.addWidget(self.reciprocalButton, 4, 5)
        mainLayout.addWidget(self.equalButton, 5, 5)

        self.cBox = QHBoxLayout()
        self.cBox.addWidget(self.createButton('-> mm',
                    self.convertClicked, 'Convert inch to mm'))
        self.cBox.addWidget(self.createButton('-> inch',
                    self.convertClicked, 'Convert mm to inch'))
        self.cBox.addWidget(self.createButton('25.4/x',
                    self.convertClicked, 'Convert metric pitch to TPI or TPI to metric pitch'))
        mainLayout.addLayout(self.cBox,6,0,1,2)

        self.bBox = QDialogButtonBox()
        self.bBox.addButton('Apply', QDialogButtonBox.AcceptRole)
        self.bBox.addButton('Cancel', QDialogButtonBox.RejectRole)
        self.bBox.rejected.connect( self.reject)
        self.bBox.accepted.connect(self.accept)
        mainLayout.addWidget(self.bBox,7,0)

        self.setLayout(mainLayout)

        self.setWindowTitle("Calculator")
        if not INFO.LINUXCNC_IS_RUNNING:
            self.axisButton.setEnabled(False)
        STATUS.connect('all-homed', lambda w: self.axisButton.setEnabled(True))
        STATUS.connect('not-all-homed', lambda w, data: self.axisButton.setEnabled(False))

    def digitClicked(self):
        clickedButton = self.sender()
        digitValue = int(clickedButton.text())

        if self.display.text() == '0' and digitValue == 0.0:
            return

        if self.waitingForOperand:
            self.display.clear()
            self.waitingForOperand = False

        self.display.setText(self.display.text() + str(digitValue))

    def unaryOperatorClicked(self):
        clickedButton = self.sender()
        clickedOperator = clickedButton.text()
        operand = float(self.display.text())

        if clickedOperator == "Sqrt":
            if operand < 0.0:
                self.abortOperation()
                return

            result = math.sqrt(operand)
        elif clickedOperator == "x\N{SUPERSCRIPT TWO}":
            result = math.pow(operand, 2.0)
        elif clickedOperator == "1/x":
            if operand == 0.0:
                self.abortOperation()
                return

            result = 1.0 / operand

        self.display.setText(str(result))
        self.waitingForOperand = True

    def additiveOperatorClicked(self):
        clickedButton = self.sender()
        clickedOperator = clickedButton.text()
        operand = float(self.display.text())

        if self.pendingMultiplicativeOperator:
            if not self.calculate(operand, self.pendingMultiplicativeOperator):
                self.abortOperation()
                return

            self.display.setText(str(self.factorSoFar))
            operand = self.factorSoFar
            self.factorSoFar = 0.0
            self.pendingMultiplicativeOperator = ''

        if self.pendingAdditiveOperator:
            if not self.calculate(operand, self.pendingAdditiveOperator):
                self.abortOperation()
                return

            self.display.setText(str(self.sumSoFar))
        else:
            self.sumSoFar = operand

        self.pendingAdditiveOperator = clickedOperator
        self.waitingForOperand = True

    def multiplicativeOperatorClicked(self):
        clickedButton = self.sender()
        clickedOperator = clickedButton.text()
        operand = float(self.display.text())

        if self.pendingMultiplicativeOperator:
            if not self.calculate(operand, self.pendingMultiplicativeOperator):
                self.abortOperation()
                return

            self.display.setText(str(self.factorSoFar))
        else:
            self.factorSoFar = operand

        self.pendingMultiplicativeOperator = clickedOperator
        self.waitingForOperand = True

    def equalClicked(self):
        operand = float(self.display.text())

        if self.pendingMultiplicativeOperator:
            if not self.calculate(operand, self.pendingMultiplicativeOperator):
                self.abortOperation()
                return

            operand = self.factorSoFar
            self.factorSoFar = 0.0
            self.pendingMultiplicativeOperator = ''

        if self.pendingAdditiveOperator:
            if not self.calculate(operand, self.pendingAdditiveOperator):
                self.abortOperation()
                return

            self.pendingAdditiveOperator = ''
        else:
            self.sumSoFar = operand

        self.display.setText(str(self.sumSoFar))
        self.sumSoFar = 0.0
        self.waitingForOperand = True

    def pointClicked(self):
        if self.waitingForOperand:
            self.display.setText('0')

        if "." not in self.display.text():
            self.display.setText(self.display.text() + ".")

        self.waitingForOperand = False

    def changeSignClicked(self):
        text = self.display.text()
        value = float(text)

        if value > 0.0:
            text = "-" + text
        elif value < 0.0:
            text = text[1:]

        self.display.setText(text)

    def axisClicked(self):
        conversion = {'X':0, 'Y':1, "Z":2, 'A':3, "B":4, "C":5, 'U':6, 'V':7, 'W':8}
        digitValue = 0.0
        try:
            p,relp,dtg = STATUS.get_position()
            text = self.axisButton.text()
            for let in INFO.AVAILABLE_AXES:
                if let in text:
                    digitValue =  round(relp[conversion[let]],5)
                    text = text.replace('%s'%let,'%s'%digitValue)
        except Exception as e:
            print(e)
            return

        if self.display.text() == '0' and digitValue == 0.0:
            return

        if self.waitingForOperand:
            self.display.clear()
            self.waitingForOperand = False

        self.display.setText(self.display.text() + str(digitValue))

    def axisTriggered(self, data):
        self.axisButton.setText('Axis {}'.format(data))

    def backspaceClicked(self):
        if self.waitingForOperand:
            return

        text = self.display.text()[:-1]
        if not text:
            text = '0'
            self.waitingForOperand = True

        self.display.setText(text)

    def convertClicked(self):
        clickedButton = self.sender()
        clickedOperator = clickedButton.text()
        operand = float(self.display.text())

        if clickedOperator == "-> mm":
            result = operand * 25.4
        elif clickedOperator == "-> inch":
            if operand == 0:
                result = 0
            else:
                result = operand / 25.4
        elif clickedOperator == "25.4/x":
            if operand == 0:
                result = 0
            else:
                result = 25.4 / operand
        else:
            return
        self.display.setText(str(result))
        self.waitingForOperand = True

    def clear(self):
        if self.waitingForOperand:
            return

        self.display.setText('0')
        self.waitingForOperand = True

    def clearAll(self):
        self.sumSoFar = 0.0
        self.factorSoFar = 0.0
        self.pendingAdditiveOperator = ''
        self.pendingMultiplicativeOperator = ''
        self.display.setText('0')
        self.waitingForOperand = True

    def clearMemory(self):
        self.sumInMemory = 0.0

    def readMemory(self):
        self.display.setText(str(self.sumInMemory))
        self.waitingForOperand = True

    def setMemory(self):
        self.equalClicked()
        self.sumInMemory = float(self.display.text())

    def addToMemory(self):
        self.equalClicked()
        self.sumInMemory += float(self.display.text())

    def createButton(self, text, member, tip=None):
        button = Button(text)
        button.clicked.connect(member)
        if tip is not None:
            button.setToolTip(tip)
        return button

    def createAxisButton(self, text, member):
        button = Button(text)
        button.clicked.connect(member)
        SettingMenu = QMenu()
        button.settingMenu = SettingMenu
        for i in INFO.AVAILABLE_AXES:
            axisButton = QAction(QIcon('exit24.png'), i, self)
            # weird lambda i=i to work around 'function closure'
            axisButton.triggered.connect(lambda state, i=i: self.axisTriggered(i))
            SettingMenu.addAction(axisButton)
        button.setMenu(SettingMenu)
        return button

    def abortOperation(self):
        self.clearAll()
        self.display.setText("####")

    def getDisplay(self):
        try:
            print(self.display.text())
            a = int(self.display.text())
        except Exception as e:
            self.display.setText('0')
            print(e)
        print(self.display.text())
        return self.display.text()

    def calculate(self, rightOperand, pendingOperator):
        if pendingOperator == "+":
            self.sumSoFar += rightOperand
        elif pendingOperator == "-":
            self.sumSoFar -= rightOperand
        elif pendingOperator == "\N{MULTIPLICATION SIGN}":
            self.factorSoFar *= rightOperand
        elif pendingOperator == "\N{DIVISION SIGN}":
            if rightOperand == 0.0:
                return False

            self.factorSoFar /= rightOperand

        return True


if __name__ == '__main__':

    import sys

    app = QApplication(sys.argv)
    calc = Calculator()
    calc.show()
    sys.exit(app.exec_())
