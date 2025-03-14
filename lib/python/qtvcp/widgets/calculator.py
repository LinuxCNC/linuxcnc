#!/usr/bin/env python3
import math
import sys

from PyQt5.QtCore import Qt
from PyQt5.QtWidgets import (QApplication, QGridLayout, QLayout, QLineEdit,
        QSizePolicy, QPushButton, QDialog, QDialogButtonBox, QMenu, QAction,
        QVBoxLayout, QToolButton, QLabel)
from PyQt5.QtGui import QDoubleValidator
from PyQt5.QtGui import QIcon
from PyQt5 import QtCore
from qtvcp.core import Status, Info
STATUS = Status()
INFO = Info()

# TODO: this needs to handle internationalization since much of the world uses
# commas instead of periods for decimal points.


class CalculatorLineEdit(QLineEdit):
    operatorKeyPressed = QtCore.pyqtSignal(str)
    digitKeyPressed = QtCore.pyqtSignal(str)
    fieldKeyPressed = QtCore.pyqtSignal(str)
    cancelKeyPressed = QtCore.pyqtSignal()

    def __init__(self, parent=None):
        super(CalculatorLineEdit, self).__init__(parent)
        self.setMaxLength(15)
        self.setAlignment(Qt.AlignRight)

        # set up a validator so the input text can only be a number
        validator = QDoubleValidator()
        # StandardNotation disables scientific notation like 10e-5, which could be left incompleted
        # before submitting (e.g. "10e" would be invalid and cause an error)
        validator.setNotation(QDoubleValidator.StandardNotation)
        self.setValidator(validator)

    def keyPressEvent(self, event):
        # The digit and decimal keys emit a digitKeyPressed signal

        if (event.key() >= Qt.Key_0 and event.key() <= Qt.Key_9) or event.key() == Qt.Key_Period:
            self.digitKeyPressed.emit(str(event.text()))

        # these keys are absorbed and not sent to QLineEdit but a signal is emitted
        # special keys:
        # ALT+Left - move to previous field
        # ALT+Right - move to next field
        # ALT+Backspace - cancel
        key_map = {
            Qt.Key_Minus: lambda: self.operatorKeyPressed.emit('-'),
            Qt.Key_Plus: lambda: self.operatorKeyPressed.emit('+'),
            Qt.Key_Asterisk: lambda: self.operatorKeyPressed.emit('*'),
            Qt.Key_Slash: lambda: self.operatorKeyPressed.emit('/'),
            Qt.Key_Equal: lambda: self.operatorKeyPressed.emit('='),
            (Qt.Key_Right, Qt.AltModifier): lambda: self.fieldKeyPressed.emit('next'),
            (Qt.Key_Left, Qt.AltModifier): lambda: self.fieldKeyPressed.emit('previous'),
            (Qt.Key_Backspace, Qt.AltModifier): self.cancelKeyPressed.emit
        }

        key = event.key()
        modifiers = event.modifiers()

        if (key, modifiers) in key_map:
            key_map[(key, modifiers)]()
        elif key in key_map:
            key_map[key]()
        else:
            # the fallthrough sends everything else to QLineEdit
            return super(CalculatorLineEdit, self).keyPressEvent(event)

class Calculator(QDialog):
    NumDigitButtons = 10
    def __init__(self, parent=None):
        super(Calculator, self).__init__(parent)

        try:
            self.PREFS_ = self.QTVCP_INSTANCE_.PREFS_
            self.PREF_SECTION = 'CALCULATOR'
        except:
            self.PREFS_ = None

        self.pendingAdditiveOperator = ''
        self.pendingMultiplicativeOperator = ''

        self.sumInMemory = 0.0
        self.sumSoFar = 0.0
        self.factorSoFar = 0.0
        self.waitingForOperand = True

        self.intResult = None
        self.floatResult = None

        self.display = CalculatorLineEdit('0')
        self.display.setMinimumHeight(30)
        self.display.setAlignment(Qt.AlignRight)
        self.display.setMaxLength(15)

        self.display.textEdited.connect(self.displayTextEdited)
        self.display.textChanged.connect(self.displayTextChanged)

        self.pendingLabel = QLabel()
        self.pendingLabel.setAlignment(Qt.AlignRight)

        self.memoryLabel = QLabel()
        self.memoryLabel.setAlignment(Qt.AlignRight)
        self.updateMemLabel()

        self.display.digitKeyPressed.connect(self.physDigitPressed)
        self.display.operatorKeyPressed.connect(self.physOperatorPressed)
        self.display.fieldKeyPressed.connect(self.physFieldKeyPressed)
        self.display.cancelKeyPressed.connect(self.physCancelKeyPressed)
        self.digitButtons = []
        for i in range(Calculator.NumDigitButtons):
            self.digitButtons.append(self.createButton(str(i),
                    self.digitClicked))

        self.pointButton = self.createButton(".", self.pointClicked)
        self.changeSignButton = self.createButton(str("\N{PLUS-MINUS SIGN}"), self.changeSignClicked)
        self.axisButton = self.createAxisButton("AXIS X", self.axisClicked)
        self.backspaceButton = self.createButton("BKSP",self.backspaceClicked, 'Backspace')
        self.clearButton = self.createButton("CLR", self.clear)
        self.clearAllButton = self.createButton("CLR ALL", self.clearAll)
        self.clearMemoryButton = self.createButton("MC", self.clearMemory, 'Clear memory')
        self.readMemoryButton = self.createButton("MR", self.readMemory, 'Read memory')
        self.setMemoryButton = self.createButton("MS", self.setMemory, 'Set memory')
        self.addToMemoryButton = self.createButton("M+", self.addToMemory, 'Add to memory')

        self.divisionButton = self.createButton(str("\N{DIVISION SIGN}"),
                self.multiplicativeOperatorClicked)
        self.timesButton = self.createButton(str("\N{MULTIPLICATION SIGN}"), self.multiplicativeOperatorClicked)
        self.minusButton = self.createButton("-", self.additiveOperatorClicked)
        self.plusButton = self.createButton("+", self.additiveOperatorClicked)

        self.squareRootButton = self.createButton("SQRT", self.unaryOperatorClicked)
        self.powerButton = self.createButton(str("x\N{SUPERSCRIPT TWO}"), self.unaryOperatorClicked)
        self.reciprocalButton = self.createButton("1/x", self.unaryOperatorClicked)
        self.equalButton = self.createButton("=", self.equalClicked)

        self.to_mm_btn = self.createButton('-> MM', self.convertClicked, 'Convert inch to mm')
        self.to_mm_btn.setProperty('convert', 'to_mm')
        self.to_inch_btn = self.createButton('-> INCH', self.convertClicked, 'Convert mm to inch')
        self.to_inch_btn.setProperty('convert', 'to_inch')
        self.tpi_btn = self.createButton('25.4/X', self.convertClicked, 'Convert metric pitch to TPI or TPI to metric pitch')
        self.tpi_btn.setProperty('convert', '25.4/X')

        mainLayout = QGridLayout()
        mainLayout.setSizeConstraint(QLayout.SetFixedSize)

        mainLayout.addWidget(self.backspaceButton, 2, 0, 1, 1)
        mainLayout.addWidget(self.axisButton, 2, 1, 1, 2)
        mainLayout.addWidget(self.clearButton, 2, 3, 1, 1)
        mainLayout.addWidget(self.clearAllButton, 2, 4, 1, 2)
        mainLayout.addWidget(self.clearMemoryButton, 3, 0)
        mainLayout.addWidget(self.readMemoryButton, 4, 0)
        mainLayout.addWidget(self.setMemoryButton, 5, 0)
        mainLayout.addWidget(self.addToMemoryButton, 6, 0)

        for i in range(1, Calculator.NumDigitButtons):
            row = ((9 - i) // 3) + 2
            column = ((i - 1) % 3) + 1
            mainLayout.addWidget(self.digitButtons[i], row + 1, column)

        mainLayout.addWidget(self.digitButtons[0], 6, 1)
        mainLayout.addWidget(self.pointButton, 6, 2)
        mainLayout.addWidget(self.changeSignButton, 6, 3)

        mainLayout.addWidget(self.divisionButton, 3, 4)
        mainLayout.addWidget(self.timesButton, 4, 4)
        mainLayout.addWidget(self.minusButton, 5, 4)
        mainLayout.addWidget(self.plusButton, 6, 4)

        mainLayout.addWidget(self.squareRootButton, 3, 5)
        mainLayout.addWidget(self.powerButton, 4, 5)
        mainLayout.addWidget(self.reciprocalButton, 5, 5)
        mainLayout.addWidget(self.equalButton, 6, 5)

        mainLayout.addWidget(self.to_mm_btn, 7, 0)
        mainLayout.addWidget(self.to_inch_btn, 7, 1)
        mainLayout.addWidget(self.tpi_btn, 7, 2)
        if self.PREFS_:
            constValues = self.PREFS_.getpref('constValuesList', 'None', str, self.PREF_SECTION)
            if constValues != 'None':
                self.constButtons = []
                constValues = ''.join(constValues.split())
                for value in constValues.split(',')[:6]:
                    constButton = QPushButton(value)
                    constButton.setSizePolicy(QSizePolicy.Preferred, QSizePolicy.Preferred)
                    constButton.setFocusPolicy(Qt.NoFocus)
                    constButton.clicked.connect(self.constClicked)
                    mainLayout.addWidget(constButton, len(self.constButtons) + 2, 6)
                    self.constButtons.append(constButton)

        mainLayout.addWidget(self.display, 0, 1, 1, mainLayout.columnCount())
        mainLayout.addWidget(self.pendingLabel, 0, 0)
        mainLayout.addWidget(self.memoryLabel, 1, 1, 1, mainLayout.columnCount()-1)

        self.backButton = QPushButton('Back')
        self.backButton.setSizePolicy(QSizePolicy.Preferred, QSizePolicy.Preferred)
        self.backButton.setFocusPolicy(Qt.NoFocus)
        self.backButton.clicked.connect(self.backActionWrapper)

        self.nextButton = QPushButton('Next')
        self.nextButton.setSizePolicy(QSizePolicy.Preferred, QSizePolicy.Preferred)
        self.nextButton.setFocusPolicy(Qt.NoFocus)
        self.nextButton.clicked.connect(self.nextActionWrapper)
        self.applyNextButton = QPushButton('Apply\nNext')
        self.applyNextButton.setSizePolicy(QSizePolicy.Preferred, QSizePolicy.Preferred)
        self.applyNextButton.clicked.connect(self.applyActionWrapper)
        self.applyNextButton.setVisible(False)

        self.bBox = QDialogButtonBox()

        cancelButton = self.bBox.addButton(QDialogButtonBox.StandardButton.Cancel)
        cancelButton.setFocusPolicy(Qt.NoFocus)
        applyButton = self.bBox.addButton(QDialogButtonBox.StandardButton.Apply)
        applyButton.setFocusPolicy(Qt.NoFocus)
        self.bBox.addButton(self.backButton, QDialogButtonBox.ActionRole)
        self.bBox.addButton(self.nextButton, QDialogButtonBox.ActionRole)
        self.bBox.addButton(self.applyNextButton, QDialogButtonBox.ActionRole)
        self.bBox.rejected.connect(self.reject)
        self.bBox.accepted.connect(self.accept)

        applyButton.clicked.connect(self.accept)

        self.display.returnPressed.connect(self.physReturnPressed)
        calc_layout = QVBoxLayout()
        calc_layout.addLayout(mainLayout)
        calc_layout.addWidget(self.bBox)
        self.setLayout(calc_layout)

        for button in self.bBox.buttons():
            if button.text() == 'Cancel' or button.text() == 'Apply':
                button.setSizePolicy(QSizePolicy.Preferred, QSizePolicy.Preferred)

        self.setWindowTitle("Calculator")
        if not INFO.LINUXCNC_IS_RUNNING:
            self.axisButton.setEnabled(False)
        STATUS.connect('all-homed', lambda w: self.axisButton.setEnabled(True))
        STATUS.connect('not-all-homed', lambda w, data: self.axisButton.setEnabled(False))

        if self.PREFS_:
            self.behaviorOnShow = self.PREFS_.getpref('onShowBehavior', 'None', str, self.PREF_SECTION)
        else:
            self.behaviorOnShow = 'None'

    def showEvent(self, event):
        if self.behaviorOnShow != 'None':
            if 'CLEAR_ALL' in self.behaviorOnShow.upper():
                self.clearAll()
            if 'FORCE_FOCUS' in self.behaviorOnShow.upper():
                self.display.setFocus()

    def displayTextEdited(self):
        # this only triggers on user changes
        if self.display.text() == '':
            self.display.setText('0')

    def displayTextChanged(self):
        # this triggers on both user and programmatic changes
        self.display.setStyleSheet("QLineEdit { }")

    def physCancelKeyPressed(self):
        self.reject()

    def physFieldKeyPressed(self, field):
        if field == 'next' and self.nextButton.isVisible():
            self.nextButton.animateClick()
        elif field == 'previous' and self.backButton.isVisible():
            self.backButton.animateClick()

    def physReturnPressed(self):
        if not self.waitingForOperand:
            self.equalClicked()
        elif self.PREFS_ and self.PREFS_.getpref('acceptOnReturnKey', False, bool, self.PREF_SECTION):
            if self.applyNextButton.isVisible():
                self.applyNextButton.animateClick()
            else:
                self.accept()

    def physDigitPressed(self, digit):
        if self.display.text() == '0' and digit != '0':
            self.display.clear()

        if self.waitingForOperand:
            self.display.clear()
            self.waitingForOperand = False


    def physOperatorPressed(self, operator):
        if operator == '+':
            self.plusButton.animateClick()
        elif operator == '-':
            if self.pendingAdditiveOperator != '-':
                self.minusButton.animateClick()
            else:
                self.changeSignButton.animateClick()
                self.pendingAdditiveOperator = ''
                self.waitingForOperand = True
                self.pendingLabel.clear()
        elif operator == '*':
            self.timesButton.animateClick()
        elif operator == '/':
            self.divisionButton.animateClick()
        elif operator == '=':
            self.equalButton.animateClick()


    def digitClicked(self):
        clickedButton = self.sender()
        digitValue = int(clickedButton.text())

        if self.display.text() == '0' and digitValue == 0.0:
            return

        if self.waitingForOperand:
            self.display.clear()
            self.waitingForOperand = False

        self.display.setText(self.display.text() + str(digitValue))
        self.display.setFocus()

    def unaryOperatorClicked(self):
        clickedButton = self.sender()
        clickedOperator = clickedButton.text()
        operand = float(self.display.text())
        if clickedOperator == "SQRT":
            if operand < 0.0:
                self.abortOperation()
                return
            result = math.sqrt(operand)
        elif clickedOperator == str("x\N{SUPERSCRIPT TWO}"):
            result = math.pow(operand, 2.0)
        elif clickedOperator == "1/x":
            if operand == 0.0:
                self.abortOperation()
                return

            result = 1.0 / operand

        self.display.setText(str(result))
        self.display.setFocus()
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

        if self.pendingAdditiveOperator and not self.waitingForOperand:
            if not self.calculate(operand, self.pendingAdditiveOperator):
                self.abortOperation()
                return

            self.display.setText(str(self.sumSoFar))
        else:
            self.sumSoFar = operand

        self.pendingAdditiveOperator = clickedOperator
        self.waitingForOperand = True
        self.pendingLabel.setText(clickedOperator)
        self.display.setFocus()

    def multiplicativeOperatorClicked(self):
        clickedButton = self.sender()
        clickedOperator = clickedButton.text()
        operand = float(self.display.text())

        if self.pendingMultiplicativeOperator and not self.waitingForOperand:
            if not self.calculate(operand, self.pendingMultiplicativeOperator):
                self.abortOperation()
                return

            self.display.setText(str(self.factorSoFar))
        else:
            self.factorSoFar = operand

        self.pendingMultiplicativeOperator = clickedOperator
        self.waitingForOperand = True
        self.pendingLabel.setText(clickedOperator)
        self.display.setFocus()

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
        self.pendingLabel.clear()
        self.display.setFocus()

    def pointClicked(self):
        if self.waitingForOperand:
            self.display.setText('0')

        if "." not in self.display.text():
            self.display.setText(self.display.text() + ".")

        self.waitingForOperand = False
        self.display.setFocus()

    def changeSignClicked(self):
        text = self.display.text()
        value = float(text)

        if value > 0.0:
            text = "-" + text
        elif value < 0.0:
            text = text[1:]

        self.display.setText(text)
        self.display.setFocus()

    def axisClicked(self):
        conversion = {'X':0, 'Y':1, "Z":2, 'A':3, "B":4, "C":5, 'U':6, 'V':7, 'W':8}
        digitValue = 0.0
        try:
            p,relp,dtg = STATUS.get_position()
            text = self.axisButton.text()
            for let in INFO.AVAILABLE_AXES:
                if let == text[-1]:
                    digitValue =  round(relp[conversion[let]],5)
                    text = text.replace('%s'%let,'%s'%digitValue)
                    break
        except Exception as e:
            print(e)
            return

        if self.display.text() == '0' and digitValue == 0.0:
            return

        if self.waitingForOperand:
            self.display.clear()
            self.waitingForOperand = False
        self.display.setText(str(digitValue))
        self.display.setFocus()

    def axisTriggered(self, data):
        self.axisButton.setText('Axis {}'.format(data))
        self.display.setFocus()

    def backspaceClicked(self):
        if self.waitingForOperand:
            return

        text = self.display.text()[:-1]
        if not text:
            text = '0'
            self.waitingForOperand = True

        self.display.setFocus()
        self.display.setText(text)

    def convertClicked(self):
        clickedOperator = self.sender().property('convert')
        operand = float(self.display.text())

        if clickedOperator == "to_mm":
            result = operand * 25.4
        elif clickedOperator == "to_inch":
            if operand == 0:
                result = 0
            else:
                result = operand / 25.4
        elif clickedOperator == "25.4/X":
            if operand == 0:
                result = 0
            else:
                result = 25.4 / operand
        else:
            return
        self.display.setText(str(result))
        self.waitingForOperand = True
        self.display.setFocus()

    def constClicked(self):
        clickedButton = self.sender()
        constValue = float(clickedButton.text())

        if self.waitingForOperand:
            self.display.clear()
            self.waitingForOperand = False

        self.display.setText(str(constValue))
        self.display.setFocus()


    def clear(self):
        if self.waitingForOperand:
            return

        self.display.setText('0')
        self.waitingForOperand = True
        self.pendingLabel.clear()
        self.display.setFocus()

    def clearAll(self):
        self.sumSoFar = 0.0
        self.factorSoFar = 0.0
        self.pendingAdditiveOperator = ''
        self.pendingMultiplicativeOperator = ''
        self.display.setText('0')
        self.display.setFocus()
        self.pendingLabel.clear()
        self.waitingForOperand = True

    def updateMemLabel(self):
        self.memoryLabel.setText(f"MEM {self.sumInMemory}")
    def clearMemory(self):
        self.sumInMemory = 0.0
        self.updateMemLabel()
        self.display.setFocus()

    def readMemory(self):
        self.display.setText(str(self.sumInMemory))
        self.waitingForOperand = True
        self.display.setFocus()

    def setMemory(self):
        self.equalClicked()
        self.sumInMemory = float(self.display.text())
        self.updateMemLabel()
        self.display.setFocus()

    def addToMemory(self):
        self.equalClicked()
        self.sumInMemory += float(self.display.text())
        self.updateMemLabel()
        self.display.setFocus()

    def createButton(self, text, member, tip=None):
        button = QPushButton(text)
        button.setSizePolicy(QSizePolicy.Preferred, QSizePolicy.Preferred)
        button.setFocusPolicy(Qt.NoFocus)
        button.setMinimumSize(70, 40)
        button.clicked.connect(member)
        if tip is not None:
            button.setToolTip(tip)
        return button

    def createAxisButton(self, text, member):
        button = QToolButton()
        button.setText(text)
        button.setSizePolicy(QSizePolicy.Preferred, QSizePolicy.Preferred)
        button.setMinimumSize(70, 40)
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
        # this should really be dictated by the stylesheet in use
        self.display.setStyleSheet("QLineEdit { background-color: #ff7777; }")
        self.display.setFocus()

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
        if pendingOperator == str("\N{MULTIPLICATION SIGN}"):
            self.factorSoFar *= rightOperand
        elif pendingOperator == str("\N{DIVISION SIGN}"):
            if rightOperand == 0.0:
                return False
            self.factorSoFar /= rightOperand
        return True

    # these wrappers ensure focus is properly managed
    def backActionWrapper(self):
        self.backAction()
        self.display.setFocus()

    def nextActionWrapper(self):
        self.nextAction()
        self.display.setFocus()

    def applyActionWrapper(self):
        self.applyAction()
        self.display.setFocus()

    def accept(self):
        self.intResult = int(float(self.display.text()))
        self.floatResult = float(self.display.text())
        super(Calculator, self).accept()

    def reject(self):
        self.intResult = None
        self.floatResult = None
        super(Calculator, self).reject()

    # Subclass can redefine
    def backAction(self):
        pass
    def nextAction(self):
        pass
    def applyAction(self):
        pass

if __name__ == '__main__':

    import sys

    app = QApplication(sys.argv)
    calc = Calculator()
    calc.show()
    sys.exit(app.exec_())
