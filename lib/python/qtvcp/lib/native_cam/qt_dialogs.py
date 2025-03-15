from PyQt5.QtWidgets import ( QMessageBox, QFileDialog, QTextEdit, QMenu)
from PyQt5.QtCore import QUrl, Qt

QUESTION = QMessageBox.Question
CRITICAL = QMessageBox.Critical
WARNING = QMessageBox.Warning
INFORMATION = QMessageBox.Information
NOICON = QMessageBox.NoIcon

#############
# dialogs
#############

def helpDialog(**kward):

        lic = kward.get('licence') or ''
        ver = kward.get('version') or ''

        mess_dlg(title =  kward.get('title') or 'Help',
                      mess = lic, info = ver, winTitle='About')

def openDialog(**kward):

        dlg = QFileDialog()
        dlg.setFileMode(QFileDialog.ExistingFile)
        dlg.setAcceptMode(QFileDialog.AcceptOpen)
        dlg.setWindowTitle(kward.get('extensions', 'Open Dialog'))
        dlg.setNameFilter(kward.get('extfilter',"All Files (*);;Text Files (*.txt)"))

        if not kward.get('extensions') is None:
            dlg.setNameFilter(kward.get('extensions'))
        if not kward.get('directory') is None:
            dlg.setDirectory(kward.get('directory'))

        # sidebar links
        urls = []
        urls.append(QUrl.fromLocalFile(os.path.expanduser('~')))
        local = os.path.join(os.path.expanduser('~'),'linuxcnc/nc_files')
        if os.path.exists(local):
            urls.append(QUrl.fromLocalFile(local))
        dlg.setSidebarUrls(urls)

        filename = None
        if dlg.exec_():
           filename = dlg.selectedFiles()[0]
           path = dlg.directory().absolutePath()

        return filename

def saveDialog(**kward):
        dlg = QFileDialog()
        dlg.setFileMode(QFileDialog.AnyFile)
        dlg.setAcceptMode(QFileDialog.AcceptSave)
        dlg.setWindowTitle(kward.get('extensions', 'Save Project Dialog'))
        dlg.setNameFilter(kward.get('extfilter',"All Files (*);;Text Files (*.txt)"))

        if not kward.get('extensions') is None:
            dlg.setNameFilter(kward.get('extensions'))
        if not kward.get('directory') is None:
            dlg.setDirectory(kward.get('directory'))
        if not kward.get('filename') is None:
            dlg.selectFile(kward.get('filename'))
        # sidebar links
        urls = []
        urls.append(QUrl.fromLocalFile(os.path.expanduser('~')))
        local = os.path.join(os.path.expanduser('~'),'linuxcnc/nc_files')
        if os.path.exists(local):
            urls.append(QUrl.fromLocalFile(local))
        dlg.setSidebarUrls(urls)

        filename = None
        if dlg.exec_():
           filename = dlg.selectedFiles()[0]
           path = dlg.directory().absolutePath()

        return filename

def action_save_ngc(*arg) :
        filechooserdialog = gtk.FileChooserDialog(_("Save as ngc..."), None,
            gtk.FILE_CHOOSER_ACTION_SAVE,
            (gtk.STOCK_CANCEL, gtk.RESPONSE_CANCEL, gtk.STOCK_OK, gtk.RESPONSE_OK))
        try :
            filt = gtk.FileFilter()
            filt.set_name("NGC")
            filt.add_mime_type("text/ngc")
            filt.add_pattern("*.ngc")
            filechooserdialog.add_filter(filt)
            filechooserdialog.set_current_folder(NGC_DIR)
            filechooserdialog.set_keep_above(True)
            filechooserdialog.set_transient_for(self.get_toplevel())

            if filechooserdialog.run() == gtk.RESPONSE_OK:
                gcode = self.to_gcode()
                filename = filechooserdialog.get_filename()
                if filename[-4] != ".ngc" not in filename :
                    filename += ".ngc"
                with open(filename, "wb") as f:
                    f.write(self.to_gcode())
                f.close()
        finally :
            filechooserdialog.destroy()


def mess_dlg( mess, winTitle = 'NativeCAM', title="NativeCAM", info='', icon=QMessageBox.Critical):
        def forceDetailsOpen(dlg):
          try:
            # force the details box open on first time display
            for i in dlg.buttons():
                if dlg.buttonRole(i) == QMessageBox.ActionRole:
                    for j in dlg.children():
                        for k in j.children():
                            if isinstance( k, QTextEdit):
                                #i.hide()
                                if not k.isVisible():
                                    i.click()
          except Exception as e:
            print(e)
            pass

        dlg = QMessageBox()
        dlg.setWindowTitle(winTitle)
        dlg.setTextFormat(Qt.RichText)
        dlg.setText('<b>{}</b>'.format(title))
        dlg.setInformativeText(info)
        dlg.setStandardButtons(QMessageBox.Ok | QMessageBox.Cancel)
        dlg.setIcon(icon)
        dlg.setDetailedText(mess)
        dlg.show()
        forceDetailsOpen(dlg)

        retval = dlg.exec()

def mess_yesno(mess, title = "Nativecam"):
        def forceDetailsOpen(dlg):
          try:
            # force the details box open on first time display
            for i in dlg.buttons():
                if dlg.buttonRole(i) == QMessageBox.ActionRole:
                    for j in dlg.children():
                        for k in j.children():
                            if isinstance( k, QTextEdit):
                                #i.hide()
                                if not k.isVisible():
                                    i.click()
          except:
            pass

        dlg = QMessageBox()
        dlg.setTextFormat(Qt.RichText)
        dlg.setText('<b>{}</b>'.format(title))
        dlg.setInformativeText(mess)
        dlg.setStandardButtons(QMessageBox.Yes | QMessageBox.No)
        dlg.setIcon(QMessageBox.Question)
        dlg.show()
        forceDetailsOpen(dlg)

        button = dlg.exec()

        if button == QMessageBox.Yes:
            return True
        else:
            return False

def mes_update_sys(mess, title = "Nativecam"):
        NO, YES, CANCEL, REFRESH = list(range(4))
        def forceDetailsOpen(dlg):
          try:
            # force the details box open on first time display
            for i in dlg.buttons():
                if dlg.buttonRole(i) == QMessageBox.ActionRole:
                    for j in dlg.children():
                        for k in j.children():
                            if isinstance( k, QTextEdit):
                                #i.hide()
                                if not k.isVisible():
                                    i.click()
          except:
            pass
        dlg = QMessageBox()
        dlg.setTextFormat(Qt.RichText)
        dlg.setText('<b>{}</b>'.format(title))
        #dlg.setText(mess)
        dlg.setInformativeText(mess)
        dlg.setStandardButtons(QMessageBox.Yes | QMessageBox.No | QMessageBox.Cancel | QMessageBox.RestoreDefaults)
        dlg.setIcon(QMessageBox.Question)
        dlg.show()
        forceDetailsOpen(dlg)

        button = dlg.exec()

        if button == QMessageBox.No:
            return NO
        elif button == QMessageBox.Yes:
            return YES
        elif button == QMessageBox.Cancel:
            return CANCEL
        elif button == QMessageBox.Apply:
            return REFRESH

