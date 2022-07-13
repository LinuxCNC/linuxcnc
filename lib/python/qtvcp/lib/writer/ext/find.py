


from PyQt5 import QtWidgets
#PYQT5 QTextEdit, QDialog, QPushButton, QRadioButton, QGridLayout


from PyQt5 import QtGui, QtCore
from PyQt5.QtCore import Qt

import re

class Find(QtWidgets.QDialog):
    def __init__(self, parent = None):
        
        QtWidgets.QDialog.__init__(self, parent)

        self.parent = parent

        self.lastStart = 0

        self.initUI()
 
    def initUI(self):

        # Button to search the document for something
        findButton = QtWidgets.QPushButton("Find",self)
        findButton.clicked.connect(self.find)

        # Button to replace the last finding
        replaceButton = QtWidgets.QPushButton("Replace",self)
        replaceButton.clicked.connect(self.replace)

        # Button to remove all findings
        allButton = QtWidgets.QPushButton("Replace all",self)
        allButton.clicked.connect(self.replaceAll)

        # Normal mode - radio button
        self.normalRadio = QtWidgets.QRadioButton("Normal",self)

        # Regular Expression Mode - radio button
        regexRadio = QtWidgets.QRadioButton("RegEx",self)

        # The field into which to type the query
        self.findField = QtWidgets.QTextEdit(self)
        self.findField.resize(250,50)

        # The field into which to type the text to replace the
        # queried text
        self.replaceField = QtWidgets.QTextEdit(self)
        self.replaceField.resize(250,50)
        
        layout = QtWidgets.QGridLayout()

        layout.addWidget(self.findField,1,0,1,4)
        layout.addWidget(self.normalRadio,2,2)
        layout.addWidget(regexRadio,2,3)
        layout.addWidget(findButton,2,0,1,2)
        
        layout.addWidget(self.replaceField,3,0,1,4)
        layout.addWidget(replaceButton,4,0,1,2)
        layout.addWidget(allButton,4,2,1,2)

        self.setGeometry(300,300,360,250)
        self.setWindowTitle("Find and Replace")
        self.setLayout(layout)

        # By default the normal mode is activated
        self.normalRadio.setChecked(True)

    def find(self):

        # Grab the parent's text
        text = self.parent.text.toPlainText()

        # And the text to find
        query = self.findField.toPlainText()

        if self.normalRadio.isChecked():

            # Use normal string search to find the query from the
            # last starting position
            self.lastStart = text.find(query,self.lastStart + 1)

            # If the find() method didn't return -1 (not found)
            if self.lastStart >= 0:

                end = self.lastStart + len(query)
                
                self.moveCursor(self.lastStart,end)

            else:

                # Make the next search start from the beginning again
                self.lastStart = 0
                
                self.parent.text.moveCursor(QtGui.QTextCursor.End)

        else:

            # Compile the pattern
            pattern = re.compile(query)

            # The actual search
            match = pattern.search(text,self.lastStart + 1)

            if match:

                self.lastStart = match.start()
                
                self.moveCursor(self.lastStart,match.end())

            else:

                self.lastStart = 0
                
                # We set the cursor to the end if the search was unsuccessful
                self.parent.text.moveCursor(QtGui.QTextCursor.End)

    def replace(self):

        # Grab the text cursor
        cursor = self.parent.text.textCursor()

        # Security
        if cursor.hasSelection():

            # We insert the new text, which will override the selected
            # text
            cursor.insertText(self.replaceField.toPlainText())

            # And set the new cursor
            self.parent.text.setTextCursor(cursor)

    def replaceAll(self):

        self.lastStart = 0

        self.find()

        # Replace and find until self.lastStart is 0 again
        while self.lastStart:
            self.replace()
            self.find()

    def moveCursor(self,start,end):

        # We retrieve the QTextCursor object from the parent's QTextEdit
        cursor = self.parent.text.textCursor()

        # Then we set the position to the beginning of the last match
        cursor.setPosition(start)

        # Next we move the Cursor by over the match and pass the KeepAnchor parameter
        # which will make the cursor select the the match's text
        cursor.movePosition(QtGui.QTextCursor.Right,QtGui.QTextCursor.KeepAnchor,end - start)

        # And finally we set this new cursor as the parent's 
        self.parent.text.setTextCursor(cursor)
