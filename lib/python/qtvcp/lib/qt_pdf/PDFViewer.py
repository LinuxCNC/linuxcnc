import sys
import os
from qtpy import QtGui, QtWidgets
from qtpy.QtCore import Qt

# Set up logging
from qtvcp import logger
LOG = logger.getLogger(__name__)

USE_QTPDF = False
USE_POPPLER = False

try:
    from qtpy.QtPdf import QPdfDocument, QPdfDocumentRenderOptions
    _QTPDF_RENDER_OPTS = QPdfDocumentRenderOptions()
    _QTPDF_RENDER_OPTS.setRenderFlags(
        QPdfDocumentRenderOptions.RenderFlag.Annotations
    )
    USE_QTPDF = True
except Exception:
    try:
        import popplerqt5
        USE_POPPLER = True
    except Exception:
        LOG.warning('PDFViewer - Neither QtPdf nor python3-poppler-qt5 is available.')

LIB_BAD = not USE_QTPDF and not USE_POPPLER

class PDFView(QtWidgets.QScrollArea):
    def __init__(self, parent=None):
        super(PDFView, self).__init__(parent)
        self.setWidgetResizable(True)

        self.widget = QtWidgets.QWidget()
        self.setWidget(self.widget)
        self.vbox = QtWidgets.QVBoxLayout()
        self.widget.setLayout(self.vbox)
        self._zoom = 1.0
        self._doc = None

        if LIB_BAD:
            label = QtWidgets.QLabel('<b>Missing PDF backend: install python3-poppler-qt5 or use Qt6 with QtPdf</b>')
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
            print('No path:', filename)
            return

        if LIB_BAD:
            return

        if USE_QTPDF:
            if self._doc is None:
                self._doc = QPdfDocument(self)
            self._doc.load(filename)
        else:
            self._doc = popplerqt5.Poppler.Document.load(filename)
            self._doc.setRenderHint(popplerqt5.Poppler.Document.Antialiasing)
            self._doc.setRenderHint(popplerqt5.Poppler.Document.TextAntialiasing)
        self.refreshPages()

    def refreshPages(self):
        if self._doc is None:
            return
        # clear layout of pages
        for i in reversed(range(self.vbox.count())):
            self.vbox.itemAt(i).widget().setParent(None)

        # convert pages to images in a label
        if USE_QTPDF:
            for i in range(self._doc.pageCount()):
                label = QtWidgets.QLabel()
                label.setScaledContents(True)
                page_size = self._doc.pagePointSize(i)
                render_size = (page_size * self._zoom).toSize()
                image = self._doc.render(i, render_size, _QTPDF_RENDER_OPTS)
                label.setPixmap(QtGui.QPixmap.fromImage(image))
                self.vbox.addWidget(label)
        else:
            for i in range(self._doc.numPages()):
                label = QtWidgets.QLabel()
                label.setScaledContents(True)
                page = self._doc.page(i)
                image = page.renderToImage(72.0 * self._zoom, 72.0 * self._zoom)
                label.setPixmap(QtGui.QPixmap.fromImage(image))
                self.vbox.addWidget(label)

    def zoomFactor(self):
        return self._zoom

    def setZoomFactor(self, factor):
        if factor > 3: factor = 3.0
        if factor < .5: factor = .5
        self._zoom = factor
        self.refreshPages()

def pdf_view(filename):
    """Return a Scrollarea showing the pages of the specified PDF file."""
    filename = os.path.expanduser(filename)
    if not os.path.exists(filename):
        print('No path:', filename)

    area = QtWidgets.QScrollArea()
    area.setWidgetResizable(True)
    widget = QtWidgets.QWidget()
    vbox = QtWidgets.QVBoxLayout()

    if USE_QTPDF:
        doc = QPdfDocument()
        doc.load(filename)
        for i in range(doc.pageCount()):
            label = QtWidgets.QLabel()
            label.setScaledContents(True)
            page_size = doc.pagePointSize(i)
            image = doc.render(i, page_size.toSize(), _QTPDF_RENDER_OPTS)
            label.setPixmap(QtGui.QPixmap.fromImage(image))
            vbox.addWidget(label)
    else:
        doc = popplerqt5.Poppler.Document.load(filename)
        doc.setRenderHint(popplerqt5.Poppler.Document.Antialiasing)
        doc.setRenderHint(popplerqt5.Poppler.Document.TextAntialiasing)
        for i in range(doc.numPages()):
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
