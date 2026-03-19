import sys
import os
from PyQt6 import QtGui, QtWidgets
from PyQt6.QtCore import Qt, QSize
from PyQt6.QtPdf import QPdfDocument, QPdfDocumentRenderOptions

# Set up logging
from qtvcp import logger
LOG = logger.getLogger(__name__)

class PDFView(QtWidgets.QScrollArea):
    def __init__(self, parent=None):
        super(PDFView, self).__init__(parent)
        self.setWidgetResizable(True)

        self.widget = QtWidgets.QWidget()
        self.setWidget(self.widget)
        self.vbox = QtWidgets.QVBoxLayout()
        self.widget.setLayout(self.vbox)
        self._zoom = 1.0
        self.doc = None
        self._render_opts = QPdfDocumentRenderOptions()
        self._render_opts.setRenderFlags(
            QPdfDocumentRenderOptions.RenderFlag.Antialiasing |
            QPdfDocumentRenderOptions.RenderFlag.TextAntialiasing
        )

    def loadSample(self, name):
        n = os.path.join(os.path.dirname(__file__),name+'.pdf')
        if os.path.exists(n):
            self.loadView(n)
        else:
            LOG.warning('Sample {}, does not exists at: {}'.format(name, n))

    def loadView(self, path):
        filename = os.path.expanduser(path)
        if not os.path.exists(filename):
            print('No path:', filename)
            return

        self.doc = QPdfDocument(self)
        self.doc.load(filename)
        self.refreshPages()

    def refreshPages(self):
        if self.doc is None:
            return

        # clear layout of pages
        for i in reversed(range(self.vbox.count())):
            self.vbox.itemAt(i).widget().setParent(None)

        # convert pages to images in a label
        for i in range(self.doc.pageCount()):
            label = QtWidgets.QLabel()
            label.setScaledContents(True)

            page_size = self.doc.pagePointSize(i)
            image_size = QSize(
                int(page_size.width() * self._zoom),
                int(page_size.height() * self._zoom)
            )
            image = self.doc.render(i, image_size, self._render_opts)

            label.setPixmap(QtGui.QPixmap.fromImage(image))
            self.vbox.addWidget(label)

    def zoomFactor(self):
        return self._zoom

    def setZoomFactor(self, factor):
        if factor > 3: factor = 3.0
        if factor < .5: factor =.5
        self._zoom = factor
        self.refreshPages()

def pdf_view(filename):
    """Return a Scrollarea showing the pages of the specified PDF file."""
    filename = os.path.expanduser(filename)
    if not os.path.exists(filename):
        print('No path:', filename)
        return None

    doc = QPdfDocument()
    doc.load(filename)

    area = QtWidgets.QScrollArea()
    area.setWidgetResizable(True)
    widget = QtWidgets.QWidget()
    vbox = QtWidgets.QVBoxLayout()

    for i in range(doc.pageCount()):
        label = QtWidgets.QLabel()
        label.setScaledContents(True)

        page_size = doc.pagePointSize(i)
        image_size = QSize(int(page_size.width()), int(page_size.height()))
        render_opts = QPdfDocumentRenderOptions()
        render_opts.setRenderFlags(
            QPdfDocumentRenderOptions.RenderFlag.Antialiasing |
            QPdfDocumentRenderOptions.RenderFlag.TextAntialiasing
        )
        image = doc.render(i, image_size, render_opts)

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
    sys.exit(app.exec())

if __name__ == '__main__':
    main()
