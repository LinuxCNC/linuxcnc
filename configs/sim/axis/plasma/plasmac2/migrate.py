#!/usr/bin/python3

'''
migrate.py
'''

import sys
import os
from shutil import *
from tkinter import *
from tkinter import filedialog
import configparser
import gettext

if os.path.dirname(__file__).startswith('/usr/share'):
    localeDir = '/usr/share/locale'
    INSTYPE = ['pkg', 'linuxcnc']
    HALLIB = '/usr/share/linuxcnc/hallib'
else:
    path = os.path.dirname(__file__).split('configs')[0]
    localeDir = os.path.join(path, 'share/locale')
    INSTYPE = ['rip', os.path.join(path, 'scripts/linuxcnc')]
    HALLIB = os.path.join(path, 'lib/hallib')

iconPath = os.path.join(os.path.dirname(__file__), 'lib/images/chips_plasma.png')

gettext.install("linuxcnc", localedir=localeDir)
_ = gettext.gettext

class Dialog(Toplevel):
    def __init__(self, parent, title, text, buttons=1, insType=False, entry=None):
        super().__init__()
#        self.transient(parent)
        self.grab_set()
        self.attributes('-type', 'popup_menu')
        self.overrideredirect(True)
        self.resizable(False, False)
        self.reply = False
        self.value = StringVar()
        self.entry = entry
        self.parent = parent
        self.focus_set()
        justify = 'left' if 'Migration Log' in title else 'center'
        f = Frame(self, relief='ridge', borderwidth=4)
        f.grid(row=0, column=1, padx=1, pady=1)
        t = Label(f, text=title)
        t.grid(row=0, column=0, columnspan=3, sticky='ew')
        l = Label(f, text=f"{text}\n", justify=justify)
        l.grid(row=1, column=0, columnspan=3, padx=8, pady=(8,0))
        if entry:
            e = Entry(f, textvariable=self.value, justify='center')
            e.grid(row=2, column=0, columnspan=3, sticky='ew', padx=8, pady=(0,8))
            e.insert(END, entry)
            e.icursor(END)
            e.focus_set()
        if insType:
            text = 'Copy Command'
            key = '<c>'
        else:
            text = 'Cancel'
            key = '<Key-Escape>'
        b1 = Button(f, text='OK', width=12, command=lambda:self.on_button(1))
        self.bb1 = self.bind('<Key-Return>', lambda e:self.on_button(1))
        if buttons == 3:
            b2 = Button(f, text='View Log', width=12, command=lambda:self.on_button(2))
            self.bb2 = self.bind('<v>', lambda e:self.on_button(2))
            b2.grid(row=3, column=1, pady=(0,8))
        if buttons > 1:
            b0 = Button(f, text=text, width=12, command=lambda:self.on_button(0))
            self.bb0 = self.bind(key, lambda e:self.on_button(0))
            b1.grid(row=3, column=0, sticky='w', padx=(8,0), pady=(0,8))
            b0.grid(row=3, column=2, sticky='e', padx=(0,8), pady=(0,8))
        else:
            b1.grid(row=3, column=1, padx=(0,8), pady=(0,8))
        parent.eval(f"tk::PlaceWindow {self} center")

    def on_button(self, button):
        if button == 2:
            title = 'plasmac2 Migration Log'
            text = ''
            with open(self.parent.logFile, 'r') as inFile:
                for line in inFile:
                    text += line
            Dialog(self.parent, title, text).show()
            self.reply = 'log'
        elif button and self.entry:
            self.reply = self.value.get()
        else:
            self.reply = button
        self.parent.focus_set()
        self.destroy()

    def show(self):
        self.wm_deiconify()
        self.wait_window()
        return self.reply

class Migrate(Tk):
    def __init__(self):
        super().__init__()
        icon = PhotoImage(file=iconPath)
        self.eval(f"wm iconphoto . {icon}")
        self.title('plasmac2 Migrate')
        text  = _('Migration utility for plasmac2')
        l = Label(self, text=text)
        self.label = Label(self, justify='left')
        self.migrate_text()
        b = Frame(self)
        bm = Button(b, text=_('Migrate'), width=10, command=self.migrate, padx=0)
        bq = Button(b, text=_('Quit'), width=10, command=self.shutdown, padx=0)
        bm.grid(row=0, column=0)
        bq.grid(row=0, column=2)
        l.grid(row=0, column=0, sticky='ew', padx=8, pady=(8,0))
        self.label.grid(row=1, column=0, sticky='ew', padx=8, pady=(8,0))
        b.grid(row=3, column=0, sticky='ew', padx=8, pady=(16,8))
        b.columnconfigure((0,1,2), weight=1)
        self.lcnc = os.path.expanduser('~/linuxcnc')
        if not os.path.isdir(self.lcnc):
            title = _('Directory Error')
            msg1 = _('The directory ~/linuxcnc does not exist')
            msg2 = _('It needs to have been created by LinuxCNC')
            msg3 = _('and have a working QtPlasmaC configuration')
            msg4 = _('to ensure that the structure is correct')
            Dialog(self, title, f"{msg1}.\n{msg2}\n{msg3}\n{msg4}.\n").show()
            raise SystemExit
        if 'root' in self.lcnc:
            title = _('User Error')
            msg = _('plasmac2 setup can not be run as a root user')
            Dialog(self, title, f"{msg}.\n").show()
            raise SystemExit
        self.configDir = os.path.join(self.lcnc, 'configs')
        self.currentDir = os.path.dirname(__file__)
        self.prefs = configparser.ConfigParser()
        self.prefs.optionxform=str
        self.eval(f"tk::PlaceWindow {self} center")
        self.bind('<m>', lambda e:self.migrate())
        self.bind('<q>', lambda e:self.shutdown())

    def migrate(self):
        self.migrate_text()
        ini = filedialog.askopenfilename(
            title='QtPlasmaC INI File',
            initialdir=self.configDir,
            filetypes=(('ini files', '*.ini'),))
        if not ini:
            return
        iniFile = os.path.basename(ini)
        self.oldDir = os.path.dirname(ini)
        path, base = self.oldDir.rsplit('/',1)
        newBase = f"{base}_plasmac2"
        title = _('Directory Name')
        msg1 = _('The new directory name will be')
        msg2 = _('If you change it to the same name as the QtPlasmaC config')
        msg3 = _('then the QtPlasmaC config directory wil be renamed to:')
        msg4 = _('Accept it or change it then accept')
        reply = Dialog(self, title, f"{msg1}: {newBase}\n\n{msg2}\n{msg3}\n{base}_qtplasmac\n\n{msg4}\n", buttons=2, entry=newBase).show()
        if reply == base:
            try:
                orgDir = self.oldDir
                self.oldDir = f"{self.oldDir}_qtplasmac"
                os.rename(orgDir, self.oldDir)
            except Exception as e:
                title = 'Directory Error'
                msg1 = _('Error while renaming')
                Dialog(self, title, f"{msg1}: {orgDir}\n\nto: {self.oldDir}\n\n{e}").show()
                return
        elif not reply:
            return
        self.newDir = os.path.join(path, reply)
        self.newIni = os.path.join(self.newDir, iniFile)
        self.logFile = os.path.join(self.newDir, 'migrate.log')
        self.log = []
        if os.path.isfile(self.logFile):
            os.remove(self.logFile)
        if os.path.exists(self.newDir):
            title = _('Directory Exists')
            msg1 = _('already exists')
            msg2 = _('Overwrite')
            reply = Dialog(self, title, f"{self.newDir} {msg1}\n\n{msg2}?\n", buttons=2).show()
            if not reply:
                return
            if os.path.isfile(self.newDir) or os.path.islink(self.newDir):
                os.remove(self.newDir)
            elif os.path.isdir(self.newDir):
                rmtree(self.newDir)
        try:
            ignores = ignore_patterns('qtplasmac','backups','machine_log*.txt', \
                                      '*.qss','*.py','*.bak','qtvcp.prefs','M190', \
                                      'qtplasmac.prefs')
            # copy required files
            p2Path = os.path.join(self.newDir, 'plasmac2')
            copytree(self.oldDir, self.newDir, ignore=ignores)
            # remove any existing plasmac2
            if os.path.islink(p2Path):
                os.remove(p2Path)
            elif os.path.exists(p2Path):
                os.remove(p2Path)
            # create a link to plasmac2
            os.symlink(self.currentDir, p2Path)
            # copy user custom files
            self.copy_custom_files(self.newDir)
            self.do_ini_file()
            self.do_hal_files()
            self.do_prefs_file()
            # all done...
            path = os.path.join(self.newDir, iniFile)
            title = _('Migration Complete')
            msg1 = _('The INI file for the plasmac2 config is')
            msg2 = _('Copy Command will copy the complete command required to run LinuxCNC using this config')
            msg  = f"{msg1}:\n\n{path}\n\n{msg2}.\n"
            clip = False
            btns = 2
            if self.log:
                with open(self.logFile, 'w') as outFile:
                    for line in self.log:
                        outFile.write(line)
                msg1 = '-------------------------------------------------'
                msg2 = _('Some hal files contain references to qtplasmac and they were commented out in the file')
                msg3 = _('There is a log of these files in')
                msg4 = _('in the new config directory')
                msg += f"\n{msg1}\n\n{msg2}.\n\n{msg3} {os.path.basename(self.logFile)} {msg4}.\n"
                btns = 3
            while 1:
                reply = Dialog(self, title, msg, buttons=btns, insType=INSTYPE[0]).show()
                if reply == 1:
                    if clip:
                        text1 = _('Warning')
                        text2 = _('The copied command will be removed from the')
                        text3 = _('system clipboard when this window is closed')
                        self.label['text'] = f"{text1}:\n{text2}\n{text3}.\n"
                    break
                elif reply == 0:
                    clip = True
                    self.clipboard_clear()
                    self.clipboard_append(f"{INSTYPE[1]} {path}")
                    self.update()
        except Exception as e:
            title = _('Migration error')
            line = sys.exc_info()[-1].tb_lineno
            msg1 = _('Migration was unsuccessful')
            msg2 = _('Error in line')
            Dialog(self, title, f"{msg1}.\n\n{msg2}: {line}\n{str(e)}").show()

    def do_ini_file(self):
        with open(self.newIni, 'r') as inFile:
            config = inFile.readlines()
        # [DISPLAY] section
        section = {}
        for lNum in range(config.index('[DISPLAY]\n') + 1, len(config)):
            if config[lNum].startswith('['):
                break
            section[lNum] = config[lNum]
        insert = lNum - 1 if config[lNum - 1] == '\n' else lNum
        done = []
        for lNum in section:
            if section[lNum].startswith('DISPLAY') and 'd' not in done:
                config[lNum] = 'DISPLAY = axis\n'
                done.append('d')
            elif section[lNum].startswith('OPEN_FILE') and 'of' not in done:
                config[lNum] = 'OPEN_FILE = ""\n'
                done.append('of')
            elif section[lNum].startswith('TOOL_EDITOR') and 'te' not in done:
                config[lNum] = 'TOOL_EDITOR = tooledit x y\n'
                done.append('te')
            elif section[lNum].startswith('CYCLE_TIME') and 'ct' not in done:
                config[lNum] = 'CYCLE_TIME = 100\n'
                done.append('ct')
            elif section[lNum].startswith('USER_COMMAND_FILE') and 'uc' not in done:
                config[lNum] = 'USER_COMMAND_FILE = ./plasmac2/plasmac2.py\n'
                done.append('uc')
            elif section[lNum].startswith('POSITION_OFFSET') and 'po' not in done:
                config[lNum] = 'POSITION_OFFSET = RELATIVE\n'
                done.append('po')
            elif section[lNum].startswith('POSITION_FEEDBACK') and 'pf' not in done:
                config[lNum] = 'POSITION_FEEDBACK = ACTUAL\n'
                done.append('pf')
        for option in ['d','of','te','ct','uc','po','pf']:
            if option == 'd' and option not in done:
                config.insert(insert,'DISPLAY = axis\n')
            elif option == 'of' and option not in done:
                config.insert(insert,'OPEN_FILE = ""\n')
            elif option == 'te' and option not in done:
                config.insert(insert,'TOOL_EDITOR = tooledit x y\n')
            elif option == 'ct' and option not in done:
                config.insert(insert,'CYCLE_TIME = 100\n')
            elif option == 'uc' and option not in done:
                config.insert(insert,'USER_COMMAND_FILE = ./plasmac2/plasmac2.py\n')
            elif option == 'po' and option not in done:
                config.insert(insert,'POSITION_OFFSET = RELATIVE\n')
            elif option == 'pf' and option not in done:
                config.insert(insert,'POSITION_FEEDBACK = ACTUAL\n')
        # [RS274NGC] section
        section = {}
        for lNum in range(config.index('[RS274NGC]\n') + 1, len(config)):
            if config[lNum].startswith('['):
                break
            section[lNum] = config[lNum]
        insert = lNum - 1 if config[lNum - 1] == '\n' else lNum
        done = []
        for lNum in section:
            if section[lNum].startswith('USER_M_PATH') and 'u' not in done:
                mPath = config[lNum].split('=')[1].strip()
                config[lNum] = f"USER_M_PATH = ./plasmac2:{mPath}\n"
                done.append('u')
        for option in ['u']:
            if option == 'u' and option not in done:
                config.insert(insert,'USER_M_PATH = ./plasmac2:./\n')
        # [EMC] section - to get machine name
        section = {}
        for lNum in range(config.index('[EMC]\n') + 1, len(config)):
            if config[lNum].startswith('['):
                break
            section[lNum] = config[lNum]
        insert = lNum - 1 if config[lNum - 1] == '\n' else lNum
        done = []
        for lNum in section:
            if section[lNum].startswith('MACHINE'):
                self.machineName = config[lNum].split('=')[1].strip()
        # [FILTER] section
        section = {}
        for lNum in range(config.index('[FILTER]\n') + 1, len(config)):
            if config[lNum].startswith('['):
                break
            section[lNum] = config[lNum]
        insert = lNum - 1 if config[lNum - 1] == '\n' else lNum
        done = []
        for lNum in section:
            if section[lNum].startswith('PROGRAM_EXTENSION') and 'p' not in done:
                config[lNum] = 'PROGRAM_EXTENSION = .ngc,.nc,.tap (filter gcode files)\n'
                done.append('p')
            if section[lNum].startswith('ngc') and 'g' not in done:
                config[lNum] = 'ngc = qtplasmac_gcode\n'
                done.append('g')
            if section[lNum].startswith('nc') and 'c' not in done:
                config[lNum] = 'nc = qtplasmac_gcode\n'
                done.append('c')
            if section[lNum].startswith('tap') and 'a' not in done:
                config[lNum] = 'tap = qtplasmac_gcode\n'
                done.append('a')
        for option in ['a','c','g','p']:
            if option == 'a' and option not in done:
                config.insert(insert,'tap = qtplasmac_gcode\n')
            elif option == 'c' and option not in done:
                config.insert(insert,'nc = qtplasmac_gcode\n')
            elif option == 'g' and option not in done:
                config.insert(insert,'ngc = qtplasmac_gcode\n')
            elif option == 'p' and option not in done:
                config.insert(insert,'PROGRAM_EXTENSION = .ngc,.nc,.tap (filter gcode files)\n')
        # [HAL] section
        section = {}
        self.halList = []
        sim = False
        self.qtplasmacHal = []
        for lNum in range(config.index('[HAL]\n') + 1, len(config)):
            if config[lNum].startswith('['):
                break
            section[lNum] = config[lNum]
        for lNum in section:
            if section[lNum].startswith('HALFILE') or \
               section[lNum].startswith('POSTGUI_HALFILE') or \
               section[lNum].startswith('SHUTDOWN'):
                halFileRaw = config[lNum].split('=')[1].strip()
                if os.path.isfile(os.path.join(HALLIB, halFileRaw)):
                    halFile = os.path.join(HALLIB, halFileRaw)
                else:
                    halFile = os.path.realpath(os.path.join(self.oldDir, halFileRaw))
                    self.halList.append(halFileRaw)
                # if existing config is a sim
                if 'sim_postgui' in halFile:
                    sim = True
                    config[lNum] = ''
                elif 'sim_no_stepgen.tcl' in halFile:
                    config[lNum] = 'HALFILE = plasmac2/lib/sim/sim_no_stepgen.tcl\n'
                    sim = True
                elif 'sim_stepgen.tcl' in halFile:
                    config[lNum] = 'HALFILE  = plasmac2/lib/sim/sim_stepgen.tcl\n'
                    sim = True
                elif 'sim_' in halFile:
                    sim = True
        # add sim panel if required
        if sim:
            app1 = '[APPLICATIONS]'
            app2 = 'DELAY = 2'
            app3 = 'APP = plasmac2/lib/sim/sim_panel.py'
            config.insert(0, f"{app1}\n{app2}\n{app3}\n\n")
        # write the ini file
        with open(self.newIni, 'w') as outFile:
            for line in config:
                outFile.write(line)

    def do_hal_files(self):
        validPins = ['out_0', 'out_1', 'out_2', 'abort','frame-job','pause', \
                     'power','probe','pulse','run','run-pause','touchoff']
        for file in self.halList:
            if not os.path.dirname(file):
                halFile = os.path.join(self.newDir, file)
            edit = False
            logged = False
            with open(halFile, 'r') as inFile:
                config = inFile.readlines()
                for lNum in range(len(config)):
                    if 'qtplasmac.ext_' in config[lNum]:
                        for pin in validPins:
                            if f"qtplasmac.ext_{pin}" in config[lNum]:
                                config[lNum] = config[lNum].replace(f"qtplasmac.ext_{pin}", f"axisui.ext.{pin}")
                                edit = True
                    if 'qtplasmac' in config[lNum] or 'sim-torch' in config[lNum]:
                        config[lNum] = f"# MIGRATION: {config[lNum]}"
                        edit = True
                        if not logged:
                            self.log.append(f"\n{file}\n")
                            logged = True
                        self.log.append(f"{lNum+1} - {config[lNum]}")
            # write the hal fil
            if edit:
                with open(halFile, 'w') as outFile:
                    for line in config:
                        outFile.write(line)

    def do_prefs_file(self):
        prefsFile = os.path.join(self.newDir, f"{self.machineName}.prefs")
        with open(prefsFile, 'r') as inFile:
            config = inFile.readlines()
        # [BUTTONS] section
        edit = False
        section = {}
        for lNum in range(config.index('[BUTTONS]\n') + 1, len(config)):
            if config[lNum].startswith('['):
                break
            section[lNum] = config[lNum]
        for lNum in section:
            if 'Name =' in section[lNum]:
                name, value = config[lNum].split('=')
                value = value.replace('\\',' ').replace('&&', '& ')
                config[lNum] = f"{name}={value}"
                edit = True
            if 'Code =' in section[lNum] and 'offsets-view' in section[lNum]:
                config[lNum-1] = f"{config[lNum-1].split('=')[0]}= \n"
                config[lNum] = f"{config[lNum].split('=')[0]}= \n"
                self.log.append(f"\n{self.machineName}.prefs\n")
                text1 = _('is not available in plasmac2')
                text2 = _('it has been removed from the prefs file')
                self.log.append(f"{lNum} & {lNum+1} - offsets-view {text1}, {text2}.\n")
                edit = True
            elif 'qtplasmac.ext_' in config[lNum]:
                config[lNum] = config[lNum].replace('qtplasmac.ext_','axisui.ext.')
                edit = True
        # write the prefs file
        if edit:
            with open(prefsFile, 'w') as outFile:
                for line in config:
                    outFile.write(line)

    def copy_custom_files(self, dir):
        for f in ['commands', 'hal', 'periodic']:
            src = os.path.join(self.currentDir, 'lib/custom', f"user_{f}.py")
            copy(src, os.path.join(dir, '.'))

    def migrate_text(self):
        text1 = _('Makes a copy of an existing working QtPlasmaC config')
        text2 = _('Then converts that copy to a working plasmac2 config')
        self.label['text'] = f"{text1}.\n\n{text2}.\n"

    def shutdown(self):
        raise SystemExit

if __name__ == "__main__":
    migrate = Migrate()
    migrate.mainloop()
