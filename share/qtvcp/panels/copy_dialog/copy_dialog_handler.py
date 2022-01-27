############################
# **** IMPORT SECTION **** #
############################
import sys
import os
import shutil
from PyQt5.QtCore import *
from PyQt5.QtGui import *
from PyQt5.QtWidgets import *
from qtvcp.widgets.file_manager import FileManager as FM
###########################################
# **** instantiate libraries section **** #
###########################################

###################################
# **** HANDLER CLASS SECTION **** #
###################################

class HandlerClass:

    ########################
    # **** INITIALIZE **** #
    ########################
    # widgets allows access to  widgets from the qtvcp files
    # at this point the widgets and hal pins are not instantiated
    def __init__(self, halcomp,widgets,paths):
        self.w = widgets
        self.paths = paths

    ##########################################
    # Special Functions called from QTVCP
    ##########################################

    # at this point:
    # the widgets are instantiated.
    # the HAL pins are built but HAL is not set ready
    def initialized__(self):
        self.w.setWindowTitle(' QtVCP Builtin Screen Code Copier')
        self.w.filemanager.showList(True)
        self.w.filemanager.model.setNameFilters([''])
        self.w.filemanager.cb.hide()
        self.w.filemanager.button3.hide()
        self.w.filemanager.user_path = os.path.expanduser('~/linuxcnc/configs')
        if os.path.exists(self.paths.RIPCONFIGDIR):
            self.w.filemanager._jumpList.update([('RIP',self.paths.RIPCONFIGDIR)])
            self.w.filemanager.addAction('RIP')
        self.w.filemanager.updateDirectoryView(self.w.filemanager.user_path)
        for i in sorted(self.paths.find_screen_dirs()):
            self.w.comboBox.addItem(i)
        self.w.comboBox.currentIndexChanged.connect(self.selectionchange)
        self.w.lineEdit.setText(self.w.comboBox.currentText())

    ########################
    # callbacks            #
    ########################
    def selectionchange(self,i):
        self.w.lineEdit.setText(self.w.comboBox.currentText())

    #######################
    # callbacks from form #
    #######################
    def applyClicked(self):
        basename = self.w.lineEdit.text()
        if basename == '': basename = self.w.comboBox.currentText()
        path = os.path.join(self.paths.SCREENDIR, self.w.comboBox.currentText())
        self.copyFilesWithCheck(path, self.w.filemanager.textLine.text(), basename, self.w.comboBox.currentText() )

    #####################
    # general functions #
    #####################

    def makedirs(self, dest):
        if not os.path.exists(dest):
            os.makedirs(dest)

    def copyFilesWithCheck(self, src, dest, base, origbase):
            dest = os.path.join(dest, base)

            if os.path.exists(dest):
                info = "Will Overwrite Existing Directory!"
            else:
                info = None
            rtn = self.w.messageDialog_.showdialog(
                    'Copy {} Screen Code?'.format(base),
                    more_info=info,
                    details=' To Folder:\n {}'.format(dest),
                    display_type='YESNO',
                    icon=QMessageBox.Information, pinname=None,
                    focus_text=None,
                    focus_color=None,
                    play_alert=None,
                    nblock=False)
            if not rtn:
                #print('Cancelled')
                return
            else:
                self.makedirs(dest)

            # walk the folder and copy everything
            for path, dirs, filenames in os.walk(src):
                for directory in dirs:
                    destDir = path.replace(src,dest)
                    self.makedirs(os.path.join(destDir, directory))
                for sfile in filenames:
                    if sfile == 'resources.py': continue
                    print (os.path.splitext(sfile)[0], origbase)
                    if origbase in os.path.splitext(sfile)[0]:
                        dfile = sfile.replace(origbase, base)
                    else:
                        dfile = sfile
                    print (dfile)
                    srcFile = os.path.join(path, sfile)
                    destFile = os.path.join(path.replace(src, dest), dfile)
                    shutil.copy(srcFile, destFile)

    #####################
    # KEY BINDING CALLS #
    #####################

    ###########################
    # **** closing event **** #
    ###########################

    ##############################
    # required class boiler code #
    ##############################

    def __getitem__(self, item):
        return getattr(self, item)
    def __setitem__(self, item, value):
        return setattr(self, item, value)


################################
# required handler boiler code #
################################

def get_handlers(halcomp,widgets,paths):
     return [HandlerClass(halcomp,widgets,paths)]
