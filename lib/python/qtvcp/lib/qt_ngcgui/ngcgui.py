#!/usr/bin/env python3
# NGCGUI for Qtvcp
#
# Copyright (c) 2021  Jim Sloot <persei802@gmail.com>
# Based on pyngcgui.py written by Dewey Garrett <dgarrett@panix.com>
#
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 2 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#

import sys
import os
import re
import hashlib
import datetime
import shutil
from PyQt5 import QtGui, QtWidgets, QtCore, uic
from PyQt5.QtCore import QRegExp
from PyQt5.QtWidgets import QFileDialog, QHBoxLayout, QVBoxLayout, QLabel, QWidget, QLineEdit, QMessageBox
from qtvcp.core import Action, Info, Path
from qtvcp import logger

ACTION = Action()
INFO = Info()
PATH = Path()
LOG = logger.getLogger(__name__)
#LOG.setLevel(logger.DEBUG) # One of DEBUG, INFO, WARNING, ERROR, CRITICAL

HERE = os.path.dirname(os.path.abspath(__file__))
INTERP_SUB_PARAMS = 30
PROG_NAME  = os.path.splitext(os.path.basename(__file__))[0]
LABEL_ID = 0


class OnePg(QWidget):
    def __init__(self, page, pre_file, sub_file, pst_file):
        super(OnePg, self).__init__()
        self.page = page
        self.pre_file = pre_file
        self.sub_file = sub_file
        self.pst_file = pst_file
        self.feature_ct = 0
        self.savesec = []
        self.fset = None
        self.entry_list = list()
        self.name = "Page_" + str(page.tabWidget.currentIndex() + 1)

    def update_onepage(self, ftype, fname):
        if ftype == 'pre':
            self.pre_file = fname
            self.fset.pre_data = PreFile(fname)
        elif ftype == 'sub':
            if self.sub_file != '':
                # SubFile.re_read
                # fill parm fields with new data from file
                return
            self.sub_file = fname
            try:
                self.make_fileset()
                self.create_parms()
            except Exception as detail:
                LOG.exception(detail)
                return False
        elif ftype == 'pst':
            self.pst_file = fname
            self.fset.pst_data = PstFile(fname)
        else:
            print("update_onepage: unexpected ftype {}".format(ftype))

    def make_fileset(self):
        try:
            self.fset = FileSet(self.pre_file, self.sub_file, self.pst_file)
        except OSError as detail:
            print("{}: make_fileset: {}".format(PROG_NAME, detail))

    def create_parms(self):
        num_parms = int(self.fset.sub_data.pdict['lastparm'])
        main_layout = QVBoxLayout()
        parm_layout = QHBoxLayout()
        self.col = list()
        for i in range(3):
            self.col.append(QVBoxLayout())
            parm_layout.addLayout(self.col[i])
        # fill in parameter values
        for key, val in list(self.fset.sub_data.ndict.items()):
            w = self.new_label(30, str(key))
            v = self.new_lineedit(key, val[1])
            c = self.new_label(100, val[2])
            if key <= 10:
                hbox = QHBoxLayout()
                hbox.addWidget(w)
                hbox.addWidget(v)
                hbox.addWidget(c)
                self.col[0].addLayout(hbox)
            elif 10 < key <= 20:
                hbox = QHBoxLayout()
                hbox.addWidget(w)
                hbox.addWidget(v)
                hbox.addWidget(c)
                self.col[1].addLayout(hbox)
            elif 20 < key <= 30:
                hbox = QHBoxLayout()
                hbox.addWidget(w)
                hbox.addWidget(v)
                hbox.addWidget(c)
                self.col[2].addLayout(hbox)
        # fill remainder with empty labels
        if 20 <= num_parms < 30:
            self.col[2].addWidget(self.empty_label())
        elif 10 <= num_parms < 20:
            self.col[2].addWidget(self.empty_label())
            self.col[1].addWidget(self.empty_label())
        elif num_parms < 10:
            self.col[2].addWidget(self.empty_label())
            self.col[1].addWidget(self.empty_label())
            self.col[0].addWidget(self.empty_label())
            
        info_label = QLineEdit(self.fset.sub_data.pdict['info'])
        info_label.setReadOnly(True)
        info_label.setFixedHeight(30)
        main_layout.addWidget(info_label)
        main_layout.addLayout(parm_layout)
        self.setLayout(main_layout)
        
    def fill_parm_fields(self):
        i = 0
        for key, val in list(self.fset.sub_data.ndict.items()):
            self.entry_list[i].setText(str(val[1]))
            if val[1] is None or val[1] == '':
                self.entry_list[i].setStyleSheet("border: 2px solid red;")
            i += 1

    def empty_label(self):
        lbl = QLabel()
        lbl.setSizePolicy(QtWidgets.QSizePolicy(QtWidgets.QSizePolicy.Expanding,QtWidgets.QSizePolicy.Expanding))
        return lbl

    def new_label(self, width, text):
        lbl = QLabel(text)
        if width is not None:
            lbl.setFixedWidth(width)
        lbl.setFixedHeight(30)
        return lbl

    def new_lineedit(self, key, data):
        # QRegExp('^[+-]?((\d+(\.\d{,4})?)|(\.\d{,4}))$' allows max 4 digits after the decimal
        valid = QtGui.QRegExpValidator(QRegExp('^[+-]?((\d+(\.\d{,4})?)|(\.\d{,4}))$'))
        lineedit = QLineEdit(str(data))
        lineedit.parm_no = key
        lineedit.setMaxLength(10)
        lineedit.setFixedWidth(70)
        lineedit.setFixedHeight(30)
        lineedit.setValidator(valid)
        if data is None or data == '':
            lineedit.setStyleSheet("border: 2px solid red;")
        lineedit.editingFinished.connect(self.parm_edited)
        self.entry_list.append(lineedit)
        return lineedit

    def parm_edited(self):
        line = self.sender()
        line.setStyleSheet('')
        parm = getattr(line, 'parm_no')
        val = line.text()
        name, value, comment = self.fset.sub_data.ndict[parm]
        if '.' in val: value = float(val)
        elif 'e' in val: value = float(val)
        else: value = int(val)
        self.fset.sub_data.ndict[parm] = (name, value, comment)


class SubFile():
    def __init__(self, fname):
        self.sub_file = fname
        self.pre_file = ''
        self.pst_file = ''
        self.image = None
        self.fset = None
        self.min_num = sys.maxsize
        self.max_num = 0
        self.g_max_parm = INTERP_SUB_PARAMS
        self.g_strict = False
        self.pdict = {} # named items:   pdict[keyword] = value
        self.ndict = {} # ordinal items: ndict[idx] = (name,dvalue,comment)
        self.ldict = {} # label items:   ldict[lno] = thelabel
        self.pdict['info'] = ''
        self.pdict['lastparm'] = 0
        self.pdict['subname'] = ''
        self.inputlines = []
        self.errlist=[]
        self.md5 = None
        self.mtime = None
        if self.sub_file == '': return
        self.mtime = os.path.getmtime(self.sub_file)
        self.md5 = md5sum(self.sub_file)
        if os.path.splitext(self.sub_file)[-1] in ['.ngc','.NGC','.nc','.NC']:
            img_file = find_image(fname)
            if img_file is None:
                img_file = os.path.join(HERE, 'images/silver_dragon.png')
                self.flag_error("No image found - using default")
            self.image = img_file
            self.read_ngc()
        elif os.path.splitext(self.sub_file)[-1] in ['.gcmc','.GCMC']:
            self.read_gcmc()
        else:
            LOG.error("Unknown file suffix for {}".format(self.sub_file))
            return

    def clear(self):
        self.sub_file = ''
        self.pdict = {}
        self.ndict = {}
        self.ldict = {}
        self.inputlines = []

    def re_read(self):
        if 'isgcmc' in self.pdict:
            self.read_gcmc()
        else:
            self.read_ngc()

    def read_gcmc(self):
        pass

    def read_ngc(self):
        bname = os.path.splitext(os.path.basename(self.sub_file))[0]
        _file = open(self.sub_file, 'r')
        for line in _file.readlines():
            self.specialcomments_ngc(line) # for compat, check on unaltered line
            self.inputlines.append(line)
        idx = 1 # 1 based for labels ldict
        nextparm = 0
        subname = None
        endsubname = None
        info_found = False
        for orig_line in self.inputlines:
            line = orig_line.translate(' \t').lower()
            if info_found is False:
                info = get_info_item(line) # check on unaltered line
                if info is not None:
                    self.pdict['info'] = info
            lineiscomment = is_comment(line)
            sname = check_sub_start(line)
            if subname is not None and sname is not None:
                self.flag_error("Multiple subroutines in file not allowed")
            if subname is None and sname is not None:
                subname = sname
                if subname is not None and subname != bname:
                    self.flag_error("Sub label {} does not match subroutine file name".format(bname))
            if endsubname is not None:
                if lineiscomment or (line.strip() == ''):
                    pass
                elif  line.find('m2') >= 0:
                    # linuxcnc ignores m2 after endsub in single-file subroutines
                    # mark as ignored here for use with expandsub option
                    self.inputlines[-1] = (';' + bname + ' ignoring: ' +  self.inputlines[-1])
                    pass
                else:
                    self.flag_error("File contains lines after subend: {}".format(line))
            ename = check_sub_end(line)
            if subname is None and ename is not None:
                self.flag_error("endsub before sub {}".format(ename))
            if subname is not None and ename is not None:
               endsubname = ename
               if endsubname != subname:
                   self.flag_error("endsubname different from subname")
            label = check_for_label(line)
            if label: self.ldict[idx] = label
            if subname is not None and endsubname is None and lineiscomment is False:
                pparm, min, max = check_positional_parm_range(line, self.min_num, self.max_num)
                if pparm is not None:
                    if pparm > self.g_max_parm:
                        self.flag_error("Parm {} exceeds maximum limit of {}".format(pparm, self.g_max_parm))
                    self.min_num = min
                    self.max_num = max
                # blanks required for this, use original line not translated line
                name, pnum, dvalue, comment = find_positional_parms(orig_line)
                if name:
                    self.ndict[pnum] = (name, dvalue, comment)
                    # require parms in sequence to minimize user errors
                    nextparm = nextparm + 1
                    if self.g_strict:
                        if pnum != nextparm:
                            self.flag_error("Out of sequence positional parameter {} expected {}".format(pnum, nextparm))
                    while pnum > nextparm:
                        makename = "#" + str(nextparm)
                        self.ndict[nextparm] = (makename, "", makename)
                        nextparm = nextparm + 1
                    self.pdict['lastparm'] = pnum
            idx += 1
        if    subname is None: self.flag_error('No sub found in file')
        if endsubname is None: self.flag_error('No endsub found in file')

        if self.g_strict:
            if nextparm == 0: self.lbl_status.setText('No subroutine parms found')

        self.pdict['subname'] = subname
        if self.pdict['info'] == '':
            self.pdict['info'] = 'sub: '+ str(subname)

    def flag_error(self, e):
        # accumulate errors from read() so entire file can be processed
        self.errlist.append(e)

    def specialcomments_ngc(self, line):
        if line.find(' FEATURE ') >= 0:
            self.flag_error("Disallowed use of ngcgui generated file as Subfile")
        if line.find('not_a_subfile') >= 0:
            self.flag_error("Not intended for use as a subfile")


class FileSet():
    def __init__(self, pre_file, sub_file, pst_file):
        self.pre_data = PreFile(pre_file)
        self.sub_data = SubFile(sub_file)
        self.pst_data = PstFile(pst_file)


class PreFile():
    def __init__(self, thefile):
        self.pre_file = thefile
        self.md5 = ''
        self.read()

    def clear(self):
        self.pre_file = ''
        self.inputlines=[]

    def read(self):
        self.mtime = None
        self.inputlines = []
        if self.pre_file == "": return

        self.mtime = os.path.getmtime(self.pre_file)
        f = open(self.pre_file)
        for l in f.readlines():
            # dont include not_a_subfile lines
            if (l.find('not_a_subfile') < 0) and (l.strip() != ''):
                self.inputlines.append(l)
        f.close()
        self.md5 = md5sum(self.pre_file)


class PstFile():
    def __init__(self, thefile):
        self.pst_file = thefile
        self.md5 = ''
        self.read()

    def clear(self):
        self.pst_file = ''
        self.inputlines = []

    def read(self):
        self.mtime = None
        self.inputlines = []
        if self.pst_file == "": return
        self.mtime = os.path.getmtime(self.pst_file)
        f = open(self.pst_file)
        for l in f.readlines():
            # dont include not_a_subfile lines
            if (l.find('not_a_subfile') < 0) and (l.strip() != ''):
                self.inputlines.append(l)
        f.close()
        self.md5 = md5sum(self.pst_file)


class SaveSection():
    def __init__(self, mypg, pre_info, sub_info, pst_info, force_expand=False):
        self.label_id = LABEL_ID
        self.label_id += 1
        self.parmlist = []
        self.sdata=[]
        self.sdata.append("({}: FEATURE {})\n".format(PROG_NAME, dt()))
        self.sdata.append("({}: PRE_file: {})\n".format(PROG_NAME, pre_info.pre_file))
        self.sdata.append("({}: SUB_file: {})\n".format(PROG_NAME, sub_info.sub_file))
        self.sdata.append("({}: PST_file: {})\n".format(PROG_NAME, pst_info.pst_file))
        # note: this line will be replaced on file output with a count that can span multiple pages
        self.sdata.append("#<_feature:> = 0\n")
        if pre_info.inputlines:
            self.sdata.append("({}: Preamble file: {})\n".format(PROG_NAME, pre_info.pre_file))
            self.sdata.extend(pre_info.inputlines)
        calltxt = "o<{}> call ".format(sub_info.pdict['subname'])
        tmpsdata = []
        for key, val in list(sub_info.ndict.items()):
            name, value, comment = val
            # TODO entries of 'None' are converted to 0 to avoid error
            # is this an ok thing to do?
            value = 0 if value is None else value
            try:
                v = float(value)
            except Exception as detail:
                LOG.exception(detail)
                mypg.textEdit_status.append("Entry for parm {} is not a number <{}>".format(key, value))
            if value is None: value = 0
            self.parmlist.append(str(value))
            if 'isgcmc' in sub_info.pdict:
                pass
            else:
                calltxt += "[{}]".format(value)
            # these appear only for not-expandsub
            tmpsdata.append("({:11s} = {:12s} = {:12s})\n".format(str(key), str(name), str(value)))
        calltxt = calltxt + '\n'
        if (mypg.chk_expand.isChecked() and 'isgcmc' in sub_info.pdict):
            print("Expand not honored for gcmc file: {}".format(os.path.basename(sub_info.sub_file)))
        if (not mypg.chk_expand.isChecked()) and (not force_expand):
            self.sdata.append("({}: Subroutine file: {})\n".format(PROG_NAME, sub_info.sub_file))
            self.sdata.append("({}: Positional parameters)\n".format(PROG_NAME))
            self.sdata.extend(tmpsdata)
            self.sdata.append(calltxt) # call the subroutine
        else:
            # expand the subroutine in place with unique labels
            self.sdata.append("(Positional parameters for {})\n".format(mypg.sub_file))
            for i in range(0, len(self.parmlist)):
                self.sdata.append("        #{} = {}\n".format(i+1, self.parmlist[i]))
            self.sdata.append("(expanded file: {})\n".format(mypg.sub_file))
            idx = 0
            for line in sub_info.inputlines:
                idx += 1
                if line.strip() == '':
                    continue
                if idx in sub_info.ldict:
                    modlabel = sub_info.ldict[idx]
                    if modlabel == 'ignoreme':
                        continue
                    modlabel = 'o<{}{}>'.format(self.label_id, modlabel)
                    r = re.search(r'^o<(.*?)>(.*)',line.strip())
                    if r:
                        modline = r.group(2) + '\n'
                    else:
                        print('SaveSection__init__:unexpected:',line)
                    self.sdata.append("{:11s} {}".format(modlabel, modline))
                else:
                    theline = "{:11s} {}".format('', line)
                    if len(theline) >= 252:
                        theline = line
                    self.sdata.append(theline)
        if pst_info.inputlines:
            self.sdata.append("({}: Postamble file: {})\n".format(PROG_NAME, pst_info.pst_file))
            self.sdata.extend(pst_info.inputlines)


class NgcGui(QtWidgets.QWidget):
    def __init__(self, parent=None):
        super(NgcGui, self).__init__(parent)
        self.auto_file = ''
        self.pre_file = ''
        self.sub_file = ''
        self.pst_file = ''
        self.last_file = None
        self.temp = None
        self.tab_index = 0
        self.feature_total = 0
        self.subroutine_path = INFO.SUB_PATH
        # load the widgets ui file
        self.ui_file = os.path.join(HERE, 'ngcgui.ui')
        LOG.debug("UI Filename: {}".format(self.ui_file))
        try:
            self.instance = uic.loadUi(self.ui_file, self)
        except AttributeError as e:
            LOG.critical(e)
        # button connections
        self.btn_add_tab.pressed.connect(self.add_page)
        self.btn_select_pre.pressed.connect(lambda: self.file_choose('pre'))
        self.btn_select_sub.pressed.connect(lambda: self.file_choose('sub'))
        self.btn_select_pst.pressed.connect(lambda: self.file_choose('pst'))
        self.btn_reread.pressed.connect(self.reread_files)
        self.btn_create.pressed.connect(self.create_feature)
        self.btn_restart.pressed.connect(self.restart_features)
        self.btn_finalize.pressed.connect(self.finalize_features)
        self.tabWidget.currentChanged.connect(lambda index: self.tab_changed(index))
        self.tabWidget.tabCloseRequested.connect(lambda index: self.close_tab(index))
        self.add_configd_tabs()

    ###################################################################
    #Function to automatically add preconfigured NGCGUI files form the Linuxcnc INI as tabs in NGCGUI for QTVCP
    #The function is called by the Ngcgui class constructor and relies on the following INI entries
    #NGCGUI_SUBFILE : name of the NGCGUI file (including extension) to be automatically loaded
    #NGCGUI_SUBFILE_PATH : path of the directory where the files can be found, relative to the root of the launched Linuxcnc INI
    ###################################################################
        
    def add_configd_tabs(self):
        if INFO.NGC_SUB_PATH is None:
            LOG.debug("UI Filename: {} Didn't find a path NGCGUI files".format(self.ui_file))
            return
        elif INFO.NGC_SUB is None:
            LOG.debug("Didn't find any configured NGCGUI files")
            return
        
        abs_ngc_sub_path = os.path.abspath(INFO.NGC_SUB_PATH)
        LOG.debug("Path to NGCGUI files:{}".format(abs_ngc_sub_path ))
        LOG.debug("Found the following ngc_sub: {}".format(INFO.NGC_SUB))

        # sort through sub list and add the pages.
        for curr_ngcfile in INFO.NGC_SUB:
           curr_fname = os.path.join(abs_ngc_sub_path,curr_ngcfile)
           LOG.debug("Adding NGCGUI:{}".format(curr_fname))
           self.add_page()
           mpage = self.tabWidget.currentWidget()
           mindex = self.tabWidget.currentIndex()
           mpage.update_onepage('sub', curr_fname)
           self.tabWidget.setTabText(mindex, curr_ngcfile)    
        index = self.tabWidget.currentIndex()
        self.tab_changed(index)

    def add_page(self):
        page = OnePg(self, '', '', '') # create new blank page
        page.make_fileset()
        idx = self.tabWidget.addTab(page, 'New Tab')
        self.tabWidget.setCurrentIndex(idx)

    def file_choose(self, ftype):
        page = self.tabWidget.currentWidget()
        index = self.tabWidget.currentIndex()
        if index < 0:
            self.textEdit_status.append("File choose error: no tabs added")
            return
        if ftype == 'pre':
            title = "Select preamble program"
            error = "File choose - no preamble selected"
        elif ftype == 'sub':
            if page.sub_file == '':
                title = "Select subroutine program"
                error = "File choose - no subroutine selected"
            else:
                self.textEdit_status.append("Cannot re-use existing layout")
                return
        elif ftype == 'pst':
            title = "Select postamble program"
            error = "File choose - no postamble selected"
        else:
            self.textEdit_status.append("Unknown file type")
            return

        fname = get_file_open(title)
        if fname == '':
            self.textEdit_status.append(error)
            return

        # make sure linuxcnc it's self can find the path
        rtn = self.check_linuxcnc_paths(fname)
        if rtn == '':
            self.textEdit_status.append("Path is not in linuxcnc's designated search folders")
            return
        else:
            fname = rtn

        if ftype == 'pre':
            self.pre_file = fname
            self.lineEdit_preamble.setText(os.path.basename(fname))
            page.update_onepage('pre', fname)
            self.textEdit_status.append("Updated preamble file")
        elif ftype == 'sub':
            self.tabWidget.setTabText(index, os.path.splitext(os.path.basename(fname))[0])
            self.sub_file = fname
            page.update_onepage('sub', fname)
            try:
                error_list = page.fset.sub_data.errlist
                if len(error_list) > 0:
                    for i in range(len(error_list)):
                        self.textEdit_status.append(error_list[i])
                    page.fset.sub_data.errlist = []
            except:
                pass
            try:
                img = QtGui.QPixmap(page.fset.sub_data.image)
                self.lbl_image.setPixmap(img)
            except:
                self.lbl_image.clear()
            self.textEdit_status.append("Updated subroutine file")
        elif ftype == 'pst':
            self.pst_file = fname
            self.lineEdit_postamble.setText(os.path.basename(fname))
            page.update_onepage('pst',fname)
            self.textEdit_status.append("Updated postamble file")
        else:
            raise ValueError("File choose: unexpected filetype {}".format(ftype))
        return

    def reread_files(self):
        if self.tabWidget.count() == 0:
            self.textEdit_status.append("Reread files error: No tabs added")
            return
        # user can edit file and use button to reread it
        page = self.tabWidget.currentWidget()
        if self.sub_file == '':
            self.textEdit_status.append("No subroutine file to read")
            return False
        page.fset.sub_data.re_read() # handle ngc or gcmc
        page.fill_parm_fields()
        self.textEdit_status.append('Reread files')
        return True # success

    def close_tab(self, index):
        page = self.tabWidget.currentWidget()
        self.feature_total -= page.feature_ct
        self.lbl_features_total.setText(str(self.feature_total))
        self.tabWidget.removeTab(index)
        index = self.tabWidget.currentIndex()
        self.tab_changed(index)

    def tab_changed(self, index):
        if index < 0:
            self.textEdit_status.clear()
            self.lbl_image.clear()
            self.lineEdit_preamble.clear()
            self.lineEdit_postamble.clear()
            self.lbl_features.setText('0')
            self.lbl_features_total.setText('0')
            return
        page = self.tabWidget.currentWidget()
        try:
            img = QtGui.QPixmap(page.fset.sub_data.image)
            self.lbl_image.setPixmap(img)
        except:
            self.lbl_image.clear()
        self.lbl_features.setText(str(page.feature_ct))
        self.lineEdit_preamble.setText(os.path.basename(page.pre_file))
        self.lineEdit_postamble.setText(os.path.basename(page.pst_file))

    def create_feature(self):
        if self.tabWidget.count() == 0:
            self.textEdit_status.append("Create error: No tabs added")
            return
        page = self.tabWidget.currentWidget()
        fset = page.fset
        if fset.sub_data.pdict['subname'] == '':
            self.textEdit_status.append("No subroutine file specified")
            return
        if 'isgcmc' in fset.sub_data.pdict:
            stat = self.savesection_gcmc()
        else:
            stat = self.savesection_ngc()
        if stat:
            page.feature_ct += 1
            self.feature_total += 1
            self.lbl_features.setText(str(page.feature_ct))
            self.lbl_features_total.setText(str(self.feature_total))
            self.textEdit_status.append("Created Feature #{}".format(page.feature_ct))
        else:
            pass

    def finalize_features(self):
        if self.tabWidget.count() == 0:
            self.textEdit_status.append("Finalize error: No tabs added")
            return
        g_auto_file_ct = 1
        LABEL_ID = 0
        page = self.tabWidget.currentWidget()
        if page.feature_ct <= 0:
            self.textEdit_status.append("No features specified on this page")
            return
        if len(page.savesec) == 0:
            self.textEdit_status.append("finalize_features: Unexpected: No features")
            return
        txt = ''
        plist = []
        sequence = ""
        for pno in range(self.tabWidget.count()):
            npage = self.tabWidget.widget(pno)
            ltxt = self.tabWidget.tabText(pno)
            howmany = len(npage.savesec)
            if howmany > 0:
                plist.append(npage)
                sequence = sequence + " " + ltxt
                txt = txt + "{} has {} features\n".format(ltxt, howmany)
        print(txt)

        if len(plist) > 1:
            msgBox = QMessageBox()
            msgBox.setWindowTitle('Finalize Features')
            msgBox.setIcon(QMessageBox.Question)
            msgBox.setStandardButtons(QMessageBox.No | QMessageBox.Yes | QMessageBox.Cancel)
            msgBox.setIcon(QMessageBox.Information)
            msgBox.setText('Finalize all Tabs?\n\n'
                        'No:     Current page only\n'
                        'Yes:    All pages\n'
                        'Cancel: Nevermind\n\n'
                        'Order: left to right')
            returnValue = msgBox.exec()
            if returnValue == QMessageBox.Yes:
                pass # use plist for all pages
            elif returnValue == QMessageBox.No:
                pno = self.tabWidget.currentIndex()
                npage = self.tabWidget.widget(pno)
                plist = [nset.pg_for_npage[npage]]
            elif returnValue == QMessageBox.Cancel:
                return # do nothing
            else:
                self.textEdit_status.append('finalize_features: unknown return value')
        # make a unique filename
        # (avoids problems with gremlin ignoring new file with same name)
        if not self.chk_save.isChecked() and INFO.LINUXCNC_IS_RUNNING:
            autoname= "tmp"
        else:
            autoname = get_file_save("Select auto filename")
        if autoname is None:
            self.textEdit_status.append("Finalize features aborted")
            return
        self.auto_file = autoname
        dirname = os.path.realpath(os.path.dirname(autoname))
        basename = str(g_auto_file_ct) + "_" + os.path.basename(autoname)
        tmpname  = os.path.join(dirname, basename)
        if os.path.exists(tmpname):
            os.remove(tmpname)
        # hack: alternate names (0,1) to force gremlin file loading
        #       and touchy filechooser updates
        g_auto_file_ct = (g_auto_file_ct + 1)%2
        basename = str(g_auto_file_ct) + "_" + os.path.basename(autoname)
        tmpname  = os.path.join(dirname, basename)
        self.last_file = tmpname
        savename = None
        f = open(tmpname,'w')
        nopts = []
        if not self.chk_add_m2.isChecked():
            f.write("%\n")
            f.write("({}: no add_m2 option)\n".format(PROG_NAME))
        featurect = 0; features_total=0
        for pg in plist:
            features_total = features_total + len(pg.savesec)
        for pg in plist:
            ct = self.write_to_file(f, pg, featurect, features_total)
            featurect += ct
            pg.feature_ct = 0
            pg.savesec = []
        if self.chk_add_m2.isChecked():
            f.write("({}: m2 line added) m2 (g54 activated)\n".format(PROG_NAME))
        else:
            f.write("%\n")
        f.close()
        if self.chk_autosend.isChecked() and INFO.LINUXCNC_IS_RUNNING:
            self.textEdit_status.append('Finalize: Sending file to linuxcnc')
            ACTION.OPEN_PROGRAM(tmpname)
            save_a_copy(tmpname)
        else:
            self.textEdit_status.append("Linuxcnc is not running")
            if self.last_file is None:
                fname = os.path.abspath(self.auto_file)
            else:
                fname = self.last_file
            shutil.move(tmpname, fname)
            save_a_copy(fname)
        for pg in plist:
            pg.feature_ct = 0
            pg.savesec = []
        self.feature_total = 0
        self.lbl_features.setText('0')
        self.lbl_features_total.setText('0')
        self.textEdit_status.append('Restarted all features')
        LABEL_ID = 0
        return

    def restart_features(self):
        if self.tabWidget.count() == 0:
            self.textEdit_status.append("Restart error: No tabs added")
            return
        page = self.tabWidget.currentWidget()
        self.feature_total -= page.feature_ct
        page.feature_ct = 0
        page.savesec = []
        self.lbl_features.setText('0')
        self.lbl_features_total.setText(str(self.feature_total))
        self.textEdit_status.append("Features restarted for {}".format(page.name))

    def savesection_ngc(self):
        page = self.tabWidget.currentWidget()
        force_expand = False
        try:
            save = SaveSection(self, page.fset.pre_data, page.fset.sub_data, page.fset.pst_data, force_expand)
            page.savesec.append(save)
        except ValueError as detail:
            print('SAVESECTION_ngc: failed {}'.format(detail))
        return True # success

    def write_to_file(self, fname, pg, featurect, features_total):
        ct = 0
        for i in range(0, len(pg.savesec)):
            ct += 1
            for line in pg.savesec[i].sdata:
                if line.find("#<_feature:>") == 0:
                    fname.write("({}: feature line added) #<_feature:> = {}\n".format(PROG_NAME, featurect))
                    featurect += 1
                    fname.write("({}: remaining_features line added) #<_remaining_features:> = {}\n".format(PROG_NAME, features_total - featurect))
                else:
                    fname.write(line)
        return(ct)

    def warp_info_frame(self, newLayout):
        """ Convenience function to move the info frame to another layout"""
        newLayout.addWidget(self.frame_info)
        # is this enough?
        self.frame_info.setMaximumWidth(500)

    def warp_tabs_frame(self, newLayout):
        """ Convenience function to move the tabs frame to another layout"""
        newLayout.addWidget(self.frame_tabs)

    # This could be overridden to actually fix the path
    def check_linuxcnc_paths(self, fname):
        if INFO.is_in_known_paths(fname):
            return self.check_linuxcnc_paths_pass(fname)
        else:
            return self.check_linuxcnc_paths_fail(fname)

    # can be overridden
    # return fname to continue
    def check_linuxcnc_paths_pass(self, fname):
        return fname

    # can be overridden
    # return '' to fail
    def check_linuxcnc_paths_fail(self, fname):
        return ''

##################
# Global functions
##################

def check_positional_parm_range(s, min, max):
    r = re.search(r'#([0-9]+)', s)
    if r: pnum = int(r.group(1))
    # here the check is against system limit; g_max_parm applied elsewhere
    if r and (pnum <= INTERP_SUB_PARAMS):
        if pnum < min: min = pnum
        if pnum > max: max = pnum
        return pnum, min, max
    return None, None, None

def check_for_label(s):
    r = re.search(r'^o<(.*?)> *(sub|endsub).*',s)
    if r:
        return 'ignoreme' # do not include on expand

    r = re.search(r'^o<(.*?)> *(call).*',s)
    if r:
        return None # do not mod label on expand

    r = re.search(r'^o<(.*?)>.*',s)
    if r:
        return r.group(1) # make label unique on expand
    return None

def check_sub_end(s):
    r = re.search(r'^o<(.*)> *endsub.*',s)
    if r:
#        print('check_sub_end:g0:',r.group(0))
#        print('check_sub_end:g1:',r.group(1))
        return r.group(1)
    return None

def check_sub_start(s):
    r = re.search(r'^o<(.*)> *sub.*',s.strip())
    if r:
#        print('check_sub_start:g0:',r.group(0))
#        print('check_sub_start:g1:',r.group(1))
        return r.group(1)
    return None

def default_send(filename):
    import gladevcp.hal_filechooser
    try:
        s = linuxcnc.stat().poll()
    except:
        user_message(mtype=Gtk.MessageType.ERROR
            ,title=_('linuxcnc not running')
            ,msg = _('cannot send, linuxcnc not running'))
        return False
    try:
        fchooser = gladevcp.hal_filechooser.EMC_Action_Open()
        fchooser._hal_init()
        fchooser._load_file(filename)
        return True
    except:
        return False

def dt():
    return(datetime.datetime.now().strftime("%y%m%d:%H.%M.%S"))

def find_image(fname):
    found = False
    for suffix in ('png','gif','jpg','pgm'):
        name = os.path.splitext(os.path.basename(fname))[0]
        dir = os.path.dirname(fname)
        ifile = os.path.join(dir,name + '.' + suffix)
        if os.path.isfile(ifile):
            found = True
            break
    if not found: return None
    return ifile

def find_positional_parms(s):
# case1  #<parmname>=#n (=defaultvalue comment_text)
# case2  #<parmname>=#n (=defaultvalue)
# case3  #<parmname>=#n (comment_text)
# case4  #<parmname>=#n

    name    = None
    pnum    = None
    dvalue  = None
    comment = None
    s = s.expandtabs() # tabs to spaces
    r = re.search(r' *# *<([a-z0-9_-]+)> *= *#([0-9]+) *\(= *([0-9.+-]+[0-9.]*?) *(.*)\)', s, re.I)
    #case1   1name               2pnum          3dvalue             4comment
    if r is None: r=re.search(r' *# *<([a-z0-9_-]+)> *= *#([0-9]+) *\( *([0-9.+-]+)\)',s,re.I)
    #case2   1name               2pnum         3dvalue
    if r is None: r=re.search(r' *# *<([a-z0-9_-]+)> *= *#([0-9]+) *\((.*)\)',s,re.I)
    #case3   1name               2pnum       3comment
    if r is None: r=re.search(r' *# *<([a-z0-9_-]+)> *= *#([0-9]+) *$',s,re.I)
    #case4   1name               2pnum

    if r:
        n = len(r.groups())
    if r and n >= 2:
        name = comment = r.group(1) # use name as comment if not specified
        pnum = int(r.group(2))
    # here check is against system limit; g_max_parm applied elsewhere
        if pnum > INTERP_SUB_PARAMS:
            return None, None, None, None
        if n == 3:
            if r.group(3)[0] == '=': dvalue = r.group(3)[1:]
            else:                    comment = r.group(3)[:]
        if n == 4:
            dvalue = r.group(3)
            if dvalue.find('.') >= 0:
                dvalue = float(dvalue)
            else:
                dvalue = int(dvalue)
            if r.group(4): comment = r.group(4)
        if n > 4:
            print('find_positional_parameters unexpected n>4',s,)
            comment = r.group(4)
        if comment is None:
            print('find_positional_parameters:NOCOMMENT') # can't happen
            comment = ''
    return name, pnum, dvalue, comment

def get_file_open(caption):
    dialog = QFileDialog()
    options = QFileDialog.Options()
    options |= QFileDialog.DontUseNativeDialog
    _filter = "GCode Files (*.ngc *.nc)"
    _dir = os.path.abspath(INFO.NGC_SUB_PATH)
    fname, _ =  dialog.getOpenFileName(None, caption, _dir, _filter, options=options)
    return fname

def get_file_save(caption):
    dialog = QFileDialog()
    options = QFileDialog.Options()
    options |= QFileDialog.DontUseNativeDialog
    _filter = "GCode Files (*.ngc)"
    _dir = os.path.abspath(INFO.NGC_SUB_PATH)
    fname, _ =  dialog.getSaveFileName(None, caption, _dir, _filter, options=options)
    return fname

def get_info_item(line):
    l = line.translate(' \t').lower()
    r = re.search(r'^\(info:(.*)\)',l)
    if r:
        r = re.search(r'.*info:(.*)\)',line)
        if r: return r.group(1)
    return None

def is_comment(s):
    if s[0] == ';':      return True # ;xxx
    elif  s[0] == '(':
        if s[-2] == ')': return True # (yyy)
        else:            return True # (yyy)zzz  maybe bogus
    return False

def md5sum(fname):
    if not fname: return None
    return(hashlib.md5(open(fname, 'r').read().encode()).hexdigest())

def save_a_copy(fname, archive_dir='/tmp/old_ngc'):
    if fname is None:
        return
    try:
        if not os.path.exists(archive_dir):
            os.mkdir(archive_dir)
        shutil.copyfile(fname, os.path.join(archive_dir, dt() + '_' + os.path.basename(fname)))
    except IOError as msg:
        LOG.error("save_a_copy: IOError copying file to {}".format(archive_dir))
        LOG.exception(msg)
    except Exception as detail:
        LOG.exception("Save a copy Exception: {}".format(detail))
        sys.exit(1)

    #############################
    # Testing                   #
    #############################
if __name__ == "__main__":
    app = QtWidgets.QApplication(sys.argv)
    w = NgcGui()
    w.show()
    sys.exit( app.exec_() )

