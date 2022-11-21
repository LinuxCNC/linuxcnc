import sys
import os
from PyQt5 import QtGui, QtWidgets
from PyQt5.QtCore import Qt

# Set up logging
from qtvcp import logger
LOG = logger.getLogger(__name__)

try:
    import popplerqt5
    LIB_BAD = False
except:
    LIB_BAD = True
    LOG.warning('PDFViwer - Is python3-poppler-qt5 installed?')

class PDFView(QtWidgets.QScrollArea):
    def __init__(self, parent=None):
        super(PDFView, self).__init__(parent)
        self.setWidgetResizable(True)

        self.widget = QtWidgets.QWidget()
        self.setWidget(self.widget)
        self.vbox = QtWidgets.QVBoxLayout()
        self.widget.setLayout(self.vbox)
        if LIB_BAD:
            label = QtWidgets.QLabel('<b>Missing python3-poppler-qt5 module</b>')
            self.vbox.addWidget(label)

    def loadSample(self, name):
        n = os.path.join(os.path.dirname(__file__),name+'.pdf')
        if os.path.exists(n):
            self.loadView(n)
        else:
            LOG.warning('Sample {}, does not exists at: {}'.format(name, n))

    def loadView(self, path):
        filename = os.path.expanduser(path)
        if not os.path.exists(filename):
            print('No path:',filename)

        if LIB_BAD:
            return

        doc = popplerqt5.Poppler.Document.load(filename)
        doc.setRenderHint(popplerqt5.Poppler.Document.Antialiasing)
        doc.setRenderHint(popplerqt5.Poppler.Document.TextAntialiasing)

        # clear layout of pages
        for i in reversed(range(self.vbox.count())): 
            self.vbox.itemAt(i).widget().setParent(None)

        # convert pages to images in a label
        for i in range(0,doc.numPages()):
            label = QtWidgets.QLabel()
            label.setScaledContents(True)

            page = doc.page(i)
            image = page.renderToImage()

            label.setPixmap(QtGui.QPixmap.fromImage(image))
            self.vbox.addWidget(label)

def pdf_view(filename):
    """Return a Scrollarea showing the pages of the specified PDF file."""
    filename = os.path.expanduser(filename)
    if not os.path.exists(filename):
        print('No path:',filename)

    doc = popplerqt5.Poppler.Document.load(filename)
    doc.setRenderHint(popplerqt5.Poppler.Document.Antialiasing)
    doc.setRenderHint(popplerqt5.Poppler.Document.TextAntialiasing)

    area = QtWidgets.QScrollArea()
    area.setWidgetResizable(True)
    widget = QtWidgets.QWidget()
    vbox = QtWidgets.QVBoxLayout()

    for i in range(0,doc.numPages()):
        label = QtWidgets.QLabel()
        label.setScaledContents(True)

        page = doc.page(i)
        image = page.renderToImage()

        label.setPixmap(QtGui.QPixmap.fromImage(image))
        vbox.addWidget(label)
    widget.setLayout(vbox)
    area.setWidget(widget)
    return area


def main():
    app = QtWidgets.QApplication(sys.argv)
    argv = QtWidgets.QApplication.arguments()
    if len(argv) < 2:
        filename = '~/emc/nc_files/3D_Chips.pdf'
    else:
        filename = argv[-1]
    view = pdf_view(filename)
    view.show()
    sys.exit(app.exec_())

if __name__ == '__main__':
    main()
