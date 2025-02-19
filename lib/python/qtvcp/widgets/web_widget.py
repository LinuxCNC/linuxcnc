import os

from PyQt5.QtCore import QUrl, QFile, QUrl
from PyQt5.QtWidgets import (QWidget,QVBoxLayout,QLabel)
from qtvcp.core import Path

# Set up logging
from qtvcp import logger
LOG = logger.getLogger(__name__)
PATH = Path()

good = True
try:
    from PyQt5.QtWebEngineWidgets import QWebEngineView as WebBase
except:
    try:
        from PyQt5.QtWebKitWidgets import QWebView as WebBase
    except:
        LOG.warning('WebWidget - Is python3-pyqt-QtWebEngine installed?')
        # fail safe - mostly for designer
        # PyQt5.QtWebEngineWidgets must be loaded before QApplication
        # which doesn't happen in designer. Also screen won't immediately 
        # crash if both libraries are not missing
        WebBase = QWidget
        good = False

class WebWidget(WebBase):
    def __init__(self, parent=None):
        super(WebWidget, self).__init__(parent)

        # bad imports - give a clue
        if not good:
            vbox = QVBoxLayout(self)
            vbox.addStretch(1)
            mess = QLabel('WebWidget Import failed')
            vbox.addWidget(mess)

    # load a HTML file, but try to fix the image path
    def loadFile(self, path):
        file = QFile(path)
        file.open(QFile.ReadOnly)
        txt = file.readAll()
        txt = str(txt, encoding='utf8')
        # fix sample html image path
        if 'IMAGEDIR/' in txt:
            txt = txt.replace('IMAGEDIR/','')
        super().setHtml(txt,QUrl.fromLocalFile(str(PATH.IMAGEDIR+'/')))
