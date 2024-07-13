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
        self.w.filemanager.addButton.hide()
        self.w.filemanager.delButton.hide()
        self.w.filemanager.loadButton.hide()
        self.w.filemanager.copy_control.hide()
        self.w.filemanager.user_path = os.path.expanduser('~/linuxcnc/configs')
        if os.path.exists(self.paths.RIPCONFIGDIR):
            self.w.filemanager._jumpList.update([('RIP',self.paths.RIPCONFIGDIR)])
            self.w.filemanager.addAction('RIP')
        self.w.filemanager._jumpList.update([('Desktop',os.path.expanduser('~/Desktop'))])
        self.w.filemanager.addAction('Desktop')

        self.w.filemanager.updateDirectoryView(self.w.filemanager.user_path)
        self.list_screen_files()
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

        # VCP panel
        if self.w.radiobutton_vcp.isChecked():
            path = os.path.join(self.paths.PANELDIR, self.w.comboBox.currentText())

        # vismach 
        elif self.w.radiobutton_vismach.isChecked():
            self.copyVismachFiles(self.paths.VISMACHDIR, self.w.filemanager.textLine.text(),
                                 basename, self.w.comboBox.currentText() )

            # check for vismach object directory
            objname = self.w.comboBox.currentText().strip('.py')
            objname = objname + '_obj'
            objpath = os.path.join(self.paths.VISMACHDIR, objname)
            objbasename = basename.strip('.py') + '_obj'
            if os.path.exists(objpath):
                self.copyVismachFiles(self.paths.VISMACHDIR, self.w.filemanager.textLine.text(),
                                 objbasename, objname )
            return

        # screen
        else:
            path = os.path.join(self.paths.SCREENDIR, self.w.comboBox.currentText())

        self.copyFilesWithCheck(path, self.w.filemanager.textLine.text(),
                                 basename, self.w.comboBox.currentText() )

    def updateCombo(self, value):
        rbtn = self.w.sender()
        if rbtn.isChecked() == True:
            if rbtn == self.w.radiobutton_vcp:
               self.list_vcp_files()
            elif rbtn == self.w.radiobutton_vismach:
                self.list_vismach_files()
            else:
                self.list_screen_files()
    #####################
    # general functions #
    #####################

    def list_screen_files(self):
        self.w.comboBox.clear()
        for i in sorted(self.paths.find_screen_dirs()):
            self.w.comboBox.addItem(i)

    def list_vcp_files(self):
        self.w.comboBox.clear()
        for i in sorted(self.paths.find_panel_dirs()):
            self.w.comboBox.addItem(i)

    def list_vismach_files(self):
        self.w.comboBox.clear()
        for i in sorted(self.paths.find_vismach_files()):
            if '.py' in i:
                self.w.comboBox.addItem(i)

    def makedirs(self, dest):
        if not os.path.exists(dest):
            os.makedirs(dest)

    def copyVismachFiles(self, src, dest, baseName, fileName):
        rtn = self.confirmDialog(dest, baseName)
        if not rtn: return

        fromPath = os.path.join(src, fileName)
        toPath = os.path.join(dest, baseName)
        print(fromPath, toPath)
        # make any needed directory
        if not os.path.exists(os.path.dirname(toPath)):
            self.makedirs(os.path.dirname(toPath))

        if not os.path.exists(toPath):
            if os.path.isfile(fromPath):
                shutil.copy(fromPath, toPath)
            # probably vismach object folder
            elif os.path.isdir(fromPath):
                shutil.copytree(fromPath, toPath)

    def copyFilesWithCheck(self, src, dest, baseName, srcDirName):

        # make a copy of the user pick destination path
        ndest = os.path.join(dest)

        # check for lack of standard folders in path
        folder = ''
        if self.w.radiobutton_vcp.isChecked():
            folder = 'panels' 
        elif self.w.radiobutton_screen.isChecked():
            folder = 'screens' 
        if not folder in dest:
            r = self.confirmPathDialog('qtvcp/' + folder)
            if r:
                if not 'qtvcp' in dest:
                    dest = os.path.join(dest, 'qtvcp')
                if not folder in dest:
                    dest = os.path.join(dest, folder)
                # overwrite with the new path
                ndest = dest
                if os.path.exists(dest):
                    self.w.filemanager.updateDirectoryView(dest)

        # confirm user wants to copy
        rtn = self.confirmDialog(ndest, baseName)
        if not rtn: return

        # add in the basename
        ndest = os.path.join(ndest, baseName)

        # make any needed initial directory
        if not os.path.exists(os.path.dirname(ndest)):
            self.makedirs(os.path.dirname(ndest))

        # update the file manager path
        if os.path.exists(dest):
            self.w.filemanager.updateDirectoryView(dest)

        # walk the folder and copy everything
        for path, dirs, filenames in os.walk(src):
            if dirs == []:
                destDir = path.replace(src,ndest)
                self.makedirs(destDir)
            else:
                for directory in dirs:
                    destDir = path.replace(src,ndest)
                    self.makedirs(os.path.join(destDir, directory))

            for sfile in filenames:
                if sfile == 'resources.py': continue

                # if source directory name is in file name, rename it.
                if srcDirName in os.path.splitext(sfile)[0]:
                    dfile = sfile.replace(srcDirName, baseName)
                else:
                    dfile = sfile

                srcFile = os.path.join(path, sfile)
                destFile = os.path.join(path.replace(src, ndest), dfile)

                shutil.copy(srcFile, destFile)

    # last confirm before copying -check for file overwriting
    def confirmDialog(self, srcPath, name):
        dest = os.path.join(srcPath, name)
        info = 'Copy {} Code?'.format(name)
        if os.path.exists(dest):
            title = "Confirm: Will Overwrite Existing Files!"
        else:
            title = 'Please Confirm:'
        rtn = self.w.messageDialog_.showdialog(
                    title,
                    more_info=info,
                    details=' To Folder:\n {}'.format(dest),
                    display_type='YESNO',
                    icon=QMessageBox.Information, pinname=None,
                    focus_text=None,
                    focus_color=None,
                    play_alert=None,
                    nblock=False,
                    use_exec =True)
        if not rtn:
            return False
        return True

    # confirm that qyvc/screens or panels is not needed in path
    def confirmPathDialog(self, srcPath):
        dest = os.path.join(srcPath)
        info = "Destination does not have {} folders in path. This is the usual way for qtvcp to find them. Do you wish to add the folders".format(srcPath)
        title = 'Add {} to Path?'.format(srcPath)
        rtn = self.w.messageDialog_.showdialog(
                    title,
                    more_info=info,
                    details=' To Folder:\n {}'.format(dest),
                    display_type='YESNO',
                    icon=QMessageBox.Information, pinname=None,
                    focus_text=None,
                    focus_color=None,
                    play_alert=None,
                    nblock=False,
                    use_exec =True)
        if not rtn:
            return False
        return True

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
