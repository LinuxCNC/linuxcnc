import sys
from PyQt5.QtWidgets import (QApplication, QDialog, QDialogButtonBox,
                            QVBoxLayout,QDialogButtonBox)
from PyQt5.QtCore import QTimer, Qt

class CustomDialog(QDialog):

    def __init__(self, *args, **kwargs):
        super(CustomDialog, self).__init__(*args, **kwargs)

        self.setWindowFlags(self.windowFlags() | Qt.WindowStaysOnTopHint)
        self.setWindowTitle("Filter-with-GUI Test")

        QBtn = QDialogButtonBox.Cancel

        self.buttonBox = QDialogButtonBox(QBtn)
        self.buttonBox.rejected.connect(self.reject)

        self.layout = QVBoxLayout()
        self.layout.addWidget(self.buttonBox)
        self.setLayout(self.layout)

        self._percentDone = 0

        self._timer = QTimer()
        self._timer.timeout.connect(self.process)
        self._timer.start(100)

    def reject(self):
        # This provides an error message
        print >>sys.stderr, 'You asked to cancel before finished.'
        raise SystemExit(1)

    def process(self):
        try:
            # output a line of gcode
            print >>sys.stdout, '(MSG, made line of code : {})'.format(self._percentDone)

            # keep track of progress
            self._percentDone +=1

            # update progress
            print >>sys.stderr, 'FILTER_PROGRESS={}'.format(self._percentDone)

            # if done end with no error/error message
            if self._percentDone == 100:
                raise SystemExit(0)

        except Exception as e:
            # This provides an error message
            print >>sys.stderr, 'Something bad happened:',e
            # this signals the error message should be shown
            raise SystemExit(1)



if __name__ == "__main__":

    app = QApplication(sys.argv)
    w = CustomDialog()
    w.show()
    sys.exit( app.exec_() )

