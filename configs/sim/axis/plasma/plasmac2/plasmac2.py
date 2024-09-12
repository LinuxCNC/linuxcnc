'''
plasmac2 is an add-on to the LinuxCNC Axis GUI for creating
a plasma cutting configuration using the plasmac component

This program is free software; you can redistribute it and/or modify it
under the terms of the GNU General Public License as published by the
Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful, but
WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License along
with this program; if not, write to the Free Software Foundation, Inc.,
51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
'''

VER = '20'

##############################################################################
# the next line suppresses undefined variable errors in VSCode               #
# pyright: reportUndefinedVariable = false
##############################################################################


##############################################################################
# UPDATES                                                                    #
##############################################################################
def update_check():
    ''' check for updates - the newest update must be added last here '''
    # v15 (2024 Jan 30) set user_m_path to include /nc_files/plasmac/m_files
    if 'nc_files/plasmac/m_files' not in inifile.find('RS274NGC', 'USER_M_PATH'):
        version = 'v15'
        try:
            if os.path.isfile(os.path.join(configPath, 'M190')):
                os.rename(os.path.join(configPath, 'M190'), os.path.join(configPath, 'M190.bak'))
            tmpFile = f'{s.ini_filename}~'
            COPY(s.ini_filename, tmpFile)
            with open(tmpFile, 'r') as inFile:
                with open(s.ini_filename, 'w') as outFile:
                    for line in inFile:
                        if line.startswith('USER_M_PATH'):
                            if '/usr' in BASE:
                                mPath = '/usr/share/doc/linuxcnc/examples/nc_files/plasmac/m_files'
                            else:
                                mPath = os.path.realpath(os.path.join(BASE, 'nc_files/plasmac/m_files'))
                            if line.strip().endswith(':./'):
                                mPath = f"./:{mPath}"
                                line = f"{line.strip()[:-3].replace('./plasmac2', mPath)}\n"
                            else:
                                line = line.replace('./plasmac2', mPath)
                        outFile.write(line)
            if os.path.isfile(tmpFile):
                os.remove(tmpFile)
        except Exception as e:
            title = 'error'
            msg0 = f"Update to {version} failed due to:\n\n{e}"
            notifications.add('error', f"{msg0}\n")
        else:
            title = 'info'
            msg0 = f"Updated successfully to {version}\nA restart is required"
        notifications.add(title, f"{msg0}\n")


##############################################################################
# NEW CLASSES                                                                #
##############################################################################
# class for preferences file
prefP = configparser.ConfigParser
class plasmacPreferences(prefP):
    optionxform = str
    types = {bool: prefP.getboolean,
             float: prefP.getfloat,
             int: prefP.getint,
             str: prefP.get,
             repr: lambda self, section, option: eval(prefP.get(self, section, option)),
            }

    def __init__(self):
        prefP.__init__(self, strict=False, interpolation=None)
        self.fn = os.path.join(configPath, f"{vars.machine.get()}.prefs")
        self.read(self.fn)

# class for materials file
matsP = configparser.ConfigParser
class plasmacMaterials(matsP):
    optionxform = str
    types = {bool: matsP.getboolean,
             float: matsP.getfloat,
             int: matsP.getint,
             str: matsP.get,
             repr: lambda self, section, option: eval(matsP.get(self, section, option)),
            }

    def __init__(self):
        matsP.__init__(self)
        self.fn = os.path.join(configPath, f"{vars.machine.get()}_material.cfg")
        self.read(self.fn)

# class for temp materials file
tempP = configparser.ConfigParser
class plasmacTempMaterial(tempP):
    optionxform = str
    types = {bool: tempP.getboolean,
             float: tempP.getfloat,
             int: tempP.getint,
             str: tempP.get,
             repr: lambda self,section, option: eval(tempP.get(self, section, option)),
            }

    def __init__(self):
        tempP.__init__(self)
        self.fn = os.path.join(tmpPath, f"{vars.machine.get()}_material.gcode")
        self.read(self.fn)

# class for popup dialogs
class plasmacPopUp(Tkinter.Toplevel):
    def __init__(self, func, title, msg, vkb=None, optional=None):
        super().__init__(root_window)
        self.attributes('-type', 'popup_menu')
        self.overrideredirect(True)
        if pVars.popLocation.get() == 'pointer':
            rE(f"tk::PlaceWindow {self} pointer")
        elif pVars.popLocation.get() == 'gui center':
            rE(f"tk::PlaceWindow {self} widget {root_window}")
        else:
            rE(f"tk::PlaceWindow {self} center")
        self.wait_visibility()
        if func != 'offsets':
            self.grab_set()
        self.title(title)
        self.protocol("WM_DELETE_WINDOW", lambda:self.popup_complete(False, False))
        self.frm = Tkinter.Frame(self, bg=colorBack, bd=2, relief='ridge')
        ttl = Tkinter.Label(self.frm, text=title, fg=colorBack, bg=colorFore)
        ttl.pack(fill='x')
        b1Text = b2Text = b3Text = b4Text = None
        if func == 'rfl_type':
            b1Text, b2Text = self.popup_rfl_type(func)
        elif func == 'rfl':
            b1Text, b2Text = self.popup_run_from_line(func)
        elif func == 'sc':
            b1Text, b2Text = self.popup_single_cut(func)
        elif func == 'offsets':
            b1Text, b2Text, b3Text, b4Text = self.popup_entry(func, msg, optional)
        else:
            b1Text, b2Text = self.popup_entry(func, msg, optional)
        bbox = Tkinter.Frame(self.frm, bg=colorBack)
        b1Value = True if func != 'offsets' else 'laser'
        b1 = Tkinter.Button(bbox, text=b1Text, command=lambda:self.popup_complete(b1Value, func), width=8)
        b1.configure(fg=colorFore, bg=colorBack, activebackground=colorBack, highlightthickness=0)
        b1.pack(side='left')
        b2Value = False if func != 'offsets' else 'scribe'
        if b2Text:
            b2 = Tkinter.Button(bbox, text=b2Text, command=lambda:self.popup_complete(b2Value, func), width=8)
            b2.configure(fg=colorFore, bg=colorBack, activebackground=colorBack, highlightthickness=0)
            b2.pack(side='left', padx=(8,0))
        if b3Text:
            b3 = Tkinter.Button(bbox, text=b3Text, command=lambda:self.popup_complete('probe', func), width=8)
            b3.configure(fg=colorFore, bg=colorBack, activebackground=colorBack, highlightthickness=0)
            b3.pack(side='left', padx=(8,0))
        if b4Text:
            b4 = Tkinter.Button(bbox, text=b4Text, command=lambda:self.popup_complete('cancel', func), width=8)
            b4.configure(fg=colorFore, bg=colorBack, activebackground=colorBack, highlightthickness=0)
            b4.pack(side='left', padx=(8,0))
        bbox.pack(padx=4, pady=4)
        self.frm.pack()
        if vkb:
            vkbData['required'] = True
            vkb_show(vkb)
        root_window.wait_window(self)

    def popup_rfl_type(self, func):
        self.rflType = Tkinter.StringVar(self, 'run')
        f1 = Tkinter.Frame(self.frm, bg=colorBack)
        run = Tkinter.Radiobutton(f1, text=_('Here to end'), variable=self.rflType, value='run', width=12, bg=colorBack, highlightthickness=0, anchor='w')
        cut = Tkinter.Radiobutton(f1, text=_('This cutpath'), variable=self.rflType, value='cut', width=12, bg=colorBack, highlightthickness=0, anchor='w')
        run.pack(anchor='w')
        cut.pack(anchor='w')
        f1.pack(padx=4, pady=4, anchor='w')
        return _('OK'), _('Cancel')

    def popup_run_from_line(self, func):
        self.leadIn = Tkinter.BooleanVar()
        self.leadLength = Tkinter.StringVar()
        self.leadAngle = Tkinter.StringVar()
        f1 = Tkinter.Frame(self.frm, bg=colorBack)
        lbl1 = Tkinter.Label(f1, text=_('Use Leadin:'), fg=colorFore, bg=colorBack, width=12, anchor='e')
        lbl1.pack(side='left')
        leadinDo = Tkinter.Checkbutton(f1, fg=colorFore, bg=colorBack, variable=self.leadIn, indicatoron=False, width=2, bd=1)
        leadinDo.configure(highlightthickness=0, activebackground=colorBack, selectcolor=colorActive, relief='raised', overrelief='raised')
        leadinDo.pack(side='left')
        f1.pack(padx=4, pady=4, anchor='w')
        f2 = Tkinter.Frame(self.frm, bg=colorBack)
        lbl2 = Tkinter.Label(f2, text=_('Leadin Length:'), fg=colorFore, bg=colorBack, width=12, anchor='e')
        lbl2.pack(side='left')
        leadinLength = Tkinter.Spinbox(f2, fg=colorFore, bg=colorBack, textvariable=self.leadLength, width=10)
        leadinLength.configure(highlightthickness=0)
        leadinLength.pack(side='left')
        f2.pack(padx=4, pady=4, anchor='w')
        f3 = Tkinter.Frame(self.frm, bg=colorBack)
        lbl3 = Tkinter.Label(f3, text=_('Leadin Angle:'), fg=colorFore, bg=colorBack, width=12, anchor='e')
        lbl3.pack(side='left')
        leadinAngle = Tkinter.Spinbox(f3, fg=colorFore, bg=colorBack, textvariable=self.leadAngle, width=10)
        leadinAngle.configure(highlightthickness=0)
        leadinAngle.pack(side='left')
        f3.pack(padx=4, pady=4, anchor='w')
        self.leadIn.set(False)
        if s.linear_units == 1:
            leadinLength.config(width=10, from_=1, to=25, increment=1, format='%0.0f', wrap=1)
            self.leadLength.set(5)
        else:
            leadinLength.config(width=10, from_=0.05, to=1, increment=0.05, format='%0.2f', wrap=1)
            self.leadLength.set(0.2)
        leadinAngle.config(width=10, from_=-359, to=359, increment=1, format='%0.0f', wrap=1)
        self.leadAngle.set(0)
        return _('Load'), _('Cancel')

    def popup_single_cut(self, func):
        self.xLength = Tkinter.StringVar()
        self.yLength = Tkinter.StringVar()
        f1 = Tkinter.Frame(self.frm, bg=colorBack)
        lbl1 = Tkinter.Label(f1, text=_('X Length:'), fg=colorFore, bg=colorBack, width=12, anchor='e')
        lbl1.pack(side='left')
        xLength = Tkinter.Entry(f1, fg=colorFore, bg=colorBack, textvariable=self.xLength, width=10)
        xLength.configure(highlightthickness=0)
        xLength.pack(side='left')
        f1.pack(padx=4, pady=4, anchor='w')
        f2 = Tkinter.Frame(self.frm, bg=colorBack)
        lbl2 = Tkinter.Label(f2, text=_('Y Length:'), fg=colorFore, bg=colorBack, width=12, anchor='e')
        lbl2.pack(side='left')
        yLength = Tkinter.Entry(f2, fg=colorFore, bg=colorBack, textvariable=self.yLength, width=10)
        yLength.configure(highlightthickness=0)
        yLength.pack(side='left')
        f2.pack(padx=4, pady=4, anchor='w')
        self.xLength.set(getPrefs(PREF, 'SINGLE_CUT', 'X length', 0, float))
        self.yLength.set(getPrefs(PREF, 'SINGLE_CUT', 'Y length', 0, float))
        return _('Load'), _('Cancel')

    def popup_entry(self, func, msg, optional):
        label = Tkinter.Label(self.frm, text=msg, fg=colorFore, bg=colorBack)
        if msg[:3] == '1. ':
            label.configure(justify='left')
        label.pack(padx=4, pady=4)
        if func in ['entry', 'touch']:
            self.entry = Tkinter.Entry(self.frm, justify='right', fg=colorFore, bg=colorBack)
            self.entry.configure(highlightthickness=0, selectforeground=colorBack, selectbackground=colorFore)
            if optional is not None:
                self.entry.insert('end', optional)
            self.entry.pack(padx=4, pady=4)
            self.entry.focus_set()
        if func == 'touch':
            self.entry.insert('end', '0.0')
            opl = Tkinter.Label(self.frm, text=_('Coordinate System'), fg=colorFore, bg=colorBack)
            opl.pack(padx=4, pady=4)
            self.c = c = StringVar(t)
            c.set(optional)
            self.opt = Tkinter.OptionMenu(self.frm, c, *all_systems[:])
            self.opt.configure(fg=colorFore, bg=colorBack, activebackground=colorBack, highlightthickness=0)
            self.opt.children['menu'].configure(fg=colorFore, bg=colorBack, activeforeground=colorBack, activebackground=colorFore)
            self.opt.pack(padx=4, pady=4)
        if func in ['info', 'error', 'warn']:
            return _('OK'), None
        elif func in ['yesno']:
            return _('Yes'), _('No')
        elif func in ['offsets']:
            return _('Laser'), _('Scribe'), _('Offset Probe'), _('Cancel')
        elif func in ['entry', 'touch']:
            return _('OK'), _('Cancel')

    def popup_complete(self, value, func):
        if func == 'rfl_type':
            self.reply = value, self.rflType.get()
        elif func == 'rfl':
            self.reply = value, self.leadIn.get(), float(self.leadLength.get()), float(self.leadAngle.get())
        elif func == 'sc':
            self.reply = value, self.xLength.get(), self.yLength.get()
        elif func in ['entry']:
            self.reply = value, self.entry.get()
        elif func in ['touch']:
             self.reply = value, self.entry.get(), self.c.get()
        else:
            self.reply = value
        self.destroy()


##############################################################################
# PREFERENCE FUNCTIONS                                                       #
##############################################################################
def getPrefs(prefs, section, option, default=False, typ=bool):
    m = prefs.types.get(typ)
    if prefs.has_section(section):
        if prefs.has_option(section, option):
            try:
                return m(prefs, section, option)
            except Exception as err:
                msg0 = f"[{section}]{option} is invalid, default of {typ(default)} applied"
                notifications.add('error', f"{msg0}\n")
                prefs.set(section, option, str(typ(default)))
                prefs.write(open(prefs.fn, 'w'))
                return typ(default)
        else:
            prefs.set(section, option, str(typ(default)))
            prefs.write(open(prefs.fn, 'w'))
            print(f"Preferences file, adding {option} to [{section}] section")
            return typ(default)
    else:
        prefs.add_section(section)
        prefs.set(section, option, str(typ(default)))
        prefs.write(open(prefs.fn, 'w'))
        print(f"Preferences file, adding [{section}] section")
        print(f"Preferences file, adding {option} to [{section}] section")
        return typ(default)

def putPrefs(prefs, section, option, value, typ=bool):
    if prefs.has_section(section):
        prefs.set(section, option, str(typ(value)))
        prefs.write(open(prefs.fn, 'w'))
    else:
        prefs.add_section(section)
        prefs.set(section.upper(), option, str(typ(value)))
        prefs.write(open(prefs.fn, 'w'))

def removePrefsSect(prefs, section):
    prefs.remove_section(section)
    prefs.write(open(prefs.fn, 'w'))

def sortPrefs(prefs):
    prefs._sections = OrderedDict(sorted(prefs._sections.items(), key=lambda t: int(t[0].rsplit('_',1)[1]) ))
    prefs.write(open(prefs.fn, 'w'))


##############################################################################
# WINDOW FUNCTIONS                                                           #
##############################################################################
def mode_changed():
    if pVars.plasmacMode.get() == 2:
        rE(f"grid forget {foverride}")
        rE(f"grid forget {fplasma}.arcvl")
        rE(f"grid forget {fplasma}.arc-voltage")
        rE(f"grid forget {fleds}.led-void-locked")
        rE(f"grid forget {fleds}.led-void-lockedL")
        rE(f"grid forget {fruns}.check.av")
        rE(f"grid forget {fruns}.check.avL")
        rE(f"grid forget {fparam}.c2.thc.thc-auto")
        rE(f"grid forget {fparam}.c2.thc.thc-autoL")
        rE(f"grid forget {fparam}.c2.thc.thc-delay")
        rE(f"grid forget {fparam}.c2.thc.thc-delayL")
        rE(f"grid forget {fparam}.c2.thc.thc-sample-counts")
        rE(f"grid forget {fparam}.c2.thc.thc-sample-countsL")
        rE(f"grid forget {fparam}.c2.thc.thc-sample-threshold")
        rE(f"grid forget {fparam}.c2.thc.thc-sample-thresholdL")
        rE(f"grid forget {fparam}.c2.thc.thc-threshold")
        rE(f"grid forget {fparam}.c2.thc.thc-thresholdL")
        rE(f"grid forget {fparam}.c2.thc.pid-i-gain")
        rE(f"grid forget {fparam}.c2.thc.pid-i-gainL")
        rE(f"grid forget {fparam}.c2.thc.pid-d-gain")
        rE(f"grid forget {fparam}.c2.thc.pid-d-gainL")
        rE(f"grid forget {fparam}.c2.thc.voidlock-slope")
        rE(f"grid forget {fparam}.c2.thc.voidlock-slopeL")
        rE(f"grid forget {fparam}.c3.arc.arc-voltage-scale")
        rE(f"grid forget {fparam}.c3.arc.arc-voltage-scaleL")
        rE(f"grid forget {fparam}.c3.arc.arc-voltage-offset")
        rE(f"grid forget {fparam}.c3.arc.arc-voltage-offsetL")
        rE(f"grid forget {fparam}.c3.arc.arc-ok-high")
        rE(f"grid forget {fparam}.c3.arc.arc-ok-highL")
        rE(f"grid forget {fparam}.c3.arc.arc-ok-low")
        rE(f"grid forget {fparam}.c3.arc.arc-ok-lowL")
    elif pVars.plasmacMode.get() == 1:
        rE(f"grid forget {fparam}.c3.arc.arc-ok-high")
        rE(f"grid forget {fparam}.c3.arc.arc-ok-highL")
        rE(f"grid forget {fparam}.c3.arc.arc-ok-low")
        rE(f"grid forget {fparam}.c3.arc.arc-ok-lowL")
    else:
        rE(f"grid {fparam}.c3.arc.arc-ok-highL -column 0 -row 5 -sticky e -padx {{4 0}} -pady {{4 0}}")
        rE(f"grid {fparam}.c3.arc.arc-ok-high -column 1 -row 5 -sticky e -padx {{0 4}} -pady {{4 0}}")
        rE(f"grid {fparam}.c3.arc.arc-ok-lowL -column 0 -row 6 -sticky e -padx {{4 0}} -pady {{4 0}}")
        rE(f"grid {fparam}.c3.arc.arc-ok-low -column 1 -row 6 -sticky e -padx {{0 4}} -pady {{4 0}}")
    if pVars.plasmacMode.get() < 2:
        rE(f"grid {foverride} -column 0 -row 3 -padx 2 -pady {{0 4}} -sticky w")
        rE(f"grid {fplasma}.arcvl -column 0 -row 0 -sticky nw")
        rE(f"grid {fplasma}.arc-voltage -column 0 -row 1 -sticky se -rowspan 2")
        rE(f"grid {fleds}.led-void-locked -column 4 -row 3 -padx {{4 0}} -pady {{4 0}}")
        rE(f"grid {fleds}.led-void-lockedL -column 5 -row 3 -padx {{0 0}} -pady {{4 0}} -sticky W")
        rE(f"grid {fruns}.check.av -column 0 -row 0 -sticky w")
        rE(f"grid {fruns}.check.avL -column 1 -row 0 -sticky w")
        rE(f"grid {fparam}.c2.thc.thc-autoL -column 0 -row 0 -sticky e")
        rE(f"grid {fparam}.c2.thc.thc-auto -column 1 -row 0 -sticky e -padx {{0 4}}")
        if pVars.thcAuto.get():
            rE(f"grid forget {fparam}.c2.thc.thc-delay")
            rE(f"grid forget {fparam}.c2.thc.thc-delayL")
            rE(f"grid {fparam}.c2.thc.thc-sample-countsL -column 0 -row 2 -sticky e -padx {{4 0}} -pady {{4 0}}")
            rE(f"grid {fparam}.c2.thc.thc-sample-counts -column 1 -row 2 -sticky e -padx {{0 4}} -pady {{4 0}}")
            rE(f"grid {fparam}.c2.thc.thc-sample-thresholdL -column 0 -row 3 -sticky e -padx {{4 0}} -pady {{4 0}}")
            rE(f"grid {fparam}.c2.thc.thc-sample-threshold -column 1 -row 3 -sticky e -padx {{0 4}} -pady {{4 0}}")
        else:
            rE(f"grid forget {fparam}.c2.thc.thc-sample-counts")
            rE(f"grid forget {fparam}.c2.thc.thc-sample-countsL")
            rE(f"grid forget {fparam}.c2.thc.thc-sample-threshold")
            rE(f"grid forget {fparam}.c2.thc.thc-sample-thresholdL")
            rE(f"grid {fparam}.c2.thc.thc-delayL -column 0 -row 1 -sticky e -padx {{4 0}} -pady {{4 0}}")
            rE(f"grid {fparam}.c2.thc.thc-delay -column 1 -row 1 -sticky e -padx {{0 4}} -pady {{4 0}}")
        rE(f"grid {fparam}.c2.thc.thc-thresholdL -column 0 -row 4 -sticky e -padx {{4 0}} -pady {{4 0}}")
        rE(f"grid {fparam}.c2.thc.thc-threshold -column 1 -row 4 -sticky e -padx {{0 4}} -pady {{4 0}}")
        rE(f"grid {fparam}.c2.thc.pid-i-gainL -column 0 -row 6 -sticky e -padx {{4 0}} -pady {{4 0}}")
        rE(f"grid {fparam}.c2.thc.pid-i-gain -column 1 -row 6 -sticky e -padx {{0 4}} -pady {{4 0}}")
        rE(f"grid {fparam}.c2.thc.pid-d-gainL -column 0 -row 7 -sticky e -padx {{4 0}} -pady {{4 0}}")
        rE(f"grid {fparam}.c2.thc.pid-d-gain -column 1 -row 7 -sticky e -padx {{0 4}} -pady {{4 0}}")
        rE(f"grid {fparam}.c2.thc.voidlock-slopeL -column 0 -row 9 -sticky e -padx {{4 0}} -pady {{4 0}}")
        rE(f"grid {fparam}.c2.thc.voidlock-slope -column 1 -row 9 -sticky e -padx {{0 4}} -pady {{4 0}}")
        rE(f"grid {fparam}.c3.arc.arc-voltage-scaleL -column 0 -row 3 -sticky e -padx {{4 0}} -pady {{4 0}}")
        rE(f"grid {fparam}.c3.arc.arc-voltage-scale -column 1 -row 3 -sticky e -padx {{0 4}} -pady {{4 0}}")
        rE(f"grid {fparam}.c3.arc.arc-voltage-offsetL -column 0 -row 4 -sticky e -padx {{4 0}} -pady {{4 0}}")
        rE(f"grid {fparam}.c3.arc.arc-voltage-offset -column 1 -row 4 -sticky e -padx {{0 4}} -pady {{4 0}}")
    hal.set_p('plasmac.mode', str(pVars.plasmacMode.get()))

def cone_size_changed():
    o.set_cone_basesize(float(rE(f"{fsetup}.l.gui.cone get")))

def font_changed(value=0):
    if value:
        populate_gcode_fonts()
        if 'mono' not in pVars.codeFont.get().lower():
            pVars.codeFont.set(pVars.guiFont.get())
    global lastFontSize
    rE(f"font configure TkDefaultFont -family {{{pVars.guiFont.get()}}} -size {pVars.fontSize.get()}")
    rE(f"font configure fontGui -family {{{pVars.guiFont.get()}}} -size {pVars.fontSize.get()}")
    rE(f"font configure fontArc -family {{{pVars.guiFont.get()}}} -size {int(pVars.fontSize.get())*3}")
    rE(f"font configure fontCode -family {{{pVars.codeFont.get()}}} -size {pVars.fontSize.get()}")
    rE(f".info.gcodef.gcodes configure -font fontGui")
    rE(f"{fplasma}.arc-voltage configure -font fontArc")
    rE(f".pane.bottom.t.text configure -font fontCode")
    if s.paused:
        txt = _('Cut Recovery')
        rE(f"{ftabs} itemconfigure cutrecs -text {txt}")
    else:
        rE(f"{ftabs} delete manual 0")
        rE(f"{ftabs} delete mdi 0")
        rE(f"{ftabs} insert end manual")
        rE(f"{ftabs} insert end mdi")
        rE(f"{ftabs} raise manual")
        txt = _('Manual')
        rE(f"{ftabs} itemconfigure manual -text {txt}")
        txt = _('MDI')
        rE(f"{ftabs} itemconfigure mdi -text {txt}")
    rE(f"{fright} delete preview 0")
    rE(f"{fright} delete numbers 0")
    rE(f"{fright} delete stats 0")
    rE(f"{fright} insert end preview")
    rE(f"{fright} insert end numbers")
    rE(f"{fright} insert end stats")
    rE(f"{fright} raise preview")
    txt = _('Preview')
    rE(f"{fright} itemconfigure preview -text {txt}")
    txt = _('DRO')
    rE(f"{fright} itemconfigure numbers -text {txt}")
    txt = _('Statistics')
    rE(f"{fright} itemconfigure stats -text {txt}")
    for nbook in [ftabs, fright]:
        pages = rE(f"{nbook} pages").split()
        for page in pages:
            rE(f"{nbook} itemconfigure {page} -foreground {colorFore} -background {colorBack}")
    rE(f"{fright}.fpreview configure -bd 2")
    rE(f"{fright}.fnumbers configure -bd 2")
    rE(f"{fright}.fstats configure -bd 2")
    for box in spinBoxes:
        rE(f"{box} configure -font fontGui")
    for box in rSpinBoxes:
        rE(f"{box} configure -font fontGui")
    ledFrame = int(pVars.fontSize.get()) * 2
    ledScale = int(pVars.fontSize.get()) / int(lastFontSize)
    lastFontSize = pVars.fontSize.get()
    for led in wLeds:
        rE(f"{led} configure -width {ledFrame} -height {ledFrame}")
        rE(f"{led} scale 1 1 1 {ledScale} {ledScale}")
    bSize = int(int(pVars.fontSize.get()) / 10 * 24) if int(pVars.fontSize.get()) > 10 else 24
    for w in toolButtons:
        rE(f".toolbar.{w} configure -width {bSize} -height {bSize}")
    for w in matButtons:
        rE(f"{toolmat}.{w} configure -width {bSize} -height {bSize}")
    get_coordinate_font(None)
    set_window_size()
    # set combobox list widths
    longest = max(fontsGui, key=len)
    l = Label(bd=0, padx=0, pady=0, highlightthickness=0, font='fontGui', text=longest)
    ll = int(rE(f"winfo reqwidth {l}")) + 20
    rE(f"{fsetup}.l.gui.fgui configure -listboxwidth {ll}")
    longest = max(fontsCode, key=len)
    l = Label(bd=0, padx=0, pady=0, highlightthickness=0, font='fontCode', text=longest)
    ll = int(rE(f"winfo reqwidth {l}")) + 20
    rE(f"{fsetup}.l.gui.fcode configure -listboxwidth {ll}")
    material_list_width()

def material_list_width():
    if rE(f"{fruns}.material.materials cget -values"):
        longest = max(tuple_from_tcl_string(matNames), key=len)
        l = Label(bd=0, padx=0, pady=0, highlightthickness=0, font='fontGui', text=longest)
        ll = int(rE(f"winfo reqwidth {l}")) + 20
        rE(f"{fruns}.material.materials configure -listboxwidth {ll}")

def populate_gcode_fonts():
    for font in sorted(fontsCode):
        rE(f"{fsetup}.l.gui.fcode insert end {{{font}}}")

def tuple_from_tcl_string(str):
    multi = False
    tmp = ''
    tup = ()
    for char in str:
        if char == '{':
            multi = True
        elif char == '}':
            if tmp and tmp not in tup:
                tup += (tmp,)
            tmp = ''
            multi = False
        elif multi:
            tmp += char
        elif char == ' ':
            if tmp and tmp not in tup:
                tup += (tmp,)
            tmp = ''
        else:
            tmp += char
    if tmp and tmp not in tup:
        tup += (tmp,)
    return(tup)

def close_window():
    rE(f"focus {fbuttons}.torch-enable")
    vkb_hide()
    if pVars.closeDialog.get():
        msgs = ''
        if pVars.closeText.get():
            customText = getPrefs(PREF, 'GUI_OPTIONS', 'Exit warning text', '', str).split('\\')
            if customText:
                for t in customText:
                    msgs += f"{t}\n\n"
        text2 = _('Do you really want to close LinuxCNC ?')
        msgs  += text2
        if not plasmacPopUp('yesno', _('CONFIRM CLOSE'), msgs).reply:
            return
    putPrefs(PREF, 'GUI_OPTIONS', 'Window last', rE(f"winfo geometry {root_window}").split('+', 1)[0], str)
    if hal.component_exists('pmx485'):
        hal.set_p('pmx485.enable', '0')
    root_window.destroy()

def set_window_size(winSize=False):
    global wSize, bPane
    winSize = rE(f"winfo geometry {root_window}").split('+', 1)[0] if winSize == 'combo' else winSize
    size = pVars.winSize.get()
    if size not in ['default', 'last', 'fullscreen', 'maximized']:
        msg0 = _('Invalid parameters in [GUI_OPTIONS]Window size in preferences file')
        notifications.add('error', f"{msg0}\n")
        return False
    else:
        if pVars.orient.get() == 'portrait':
            # window height, window height with pmx enabled, window width, bottom pane height
            wSize = { '7': [ 720,  756, 644,  92],  '8': [ 7588,  796, 644, 102],
                      '9': [ 814,  856, 662, 108], '10': [ 880,  924, 664, 116],
                     '11': [ 918,  966, 734, 124], '12': [ 962, 1012, 806, 132],
                     '13': [1058, 1114, 878, 140], '14': [1102, 1158, 950, 148],
                     '15': [1146, 1206,1024, 156], '16': [1214, 1278,1094, 164],
                     '17': [1270, 1336,1166, 172], '18': [1334, 1404,1166, 180],
                     '19': [1366, 1438,1240, 188], '20': [1442, 1514,1310, 196],
                    }
            offset = 1 if pmPort else 0
            height = wSize[pVars.fontSize.get()][offset]
            width  = wSize[pVars.fontSize.get()][2]
            bPane  = wSize[pVars.fontSize.get()][3]
        else:
            # window height, bottom pane height
            wSize = { '7': [456,  92],  '8': [482, 102],
                      '9': [516, 108], '10': [562, 116],
                     '11': [586, 124], '12': [612, 132],
                     '13': [686, 140], '14': [710, 148],
                     '15': [736, 156], '16': [784, 164],
                     '17': [811, 172], '18': [858, 180],
                     '19': [882, 188], '20': [932, 196],
                    }
            height = wSize[pVars.fontSize.get()][0]
            width = int(height * 1.75)
            bPane = wSize[pVars.fontSize.get()][1]
        rE(f".pane paneconfigure $pane_top -minsize {bPane}")
        rE(f".pane paneconfigure $pane_bottom -minsize {bPane}")
        rE(f".pane paneconfigure $pane_bottom -height {bPane}")
        if size == 'maximized':
            root_window.attributes('-fullscreen', False, '-zoomed', True)
        elif size == 'fullscreen':
            root_window.attributes('-fullscreen', True, '-zoomed', False)
        else:
            root_window.attributes('-fullscreen', False, '-zoomed', False)
            last = winSize if winSize else restoreSetup['winLast']
            if size == 'last' and last != 'none':
                width = int(last.split('x')[0])
                height = int(last.split('x')[1])
            xPos = int((root_window.maxsize()[0] - width) / 2)
            yPos = int((root_window.maxsize()[1] - height) / 2)
            root_window.geometry(f"{width}x{height}+{xPos}+{yPos}")
        rE(f"{fsetup}.r.ubuttons.canvas xview moveto 0.0")
        rE(f"{fsetup}.r.ubuttons.canvas yview moveto 0.0")
        root_window.update_idletasks()

def wcs_rotation(mode):
    global currentX, currentY, currentRot
    if mode == 'get':
        currentX = s.g5x_offset[0]
        currentY = s.g5x_offset[1]
        currentRot = s.rotation_xy
    elif mode == 'set':
        ensure_mode(linuxcnc.MODE_MDI)
        c.mdi(f"G10 L2 P0 X{currentX} Y{currentY} R{currentRot}")
        c.wait_complete()
        if currentRot != s.rotation_xy:
            reload_file()

def preview_toggle(conv=False):
    if pVars.previewLarge.get() or conv:
        rE('grid forget .pane.top.tabs')
        rE(f"grid forget {ftop}.feedoverride")
        rE(f"grid forget {ftop}.rapidoverride")
        rE(f"grid forget {ftop}.jogspeed")
        rE(f"grid forget {ftop}.ajogspeed")
        rE('.pane paneconfigure $pane_bottom -minsize 1')
        rE('.pane paneconfigure $pane_bottom -height 1')
        if pVars.orient.get() == 'portrait':
            rE(f"grid forget {fportrait}")
        else:
            rE(f"grid forget {fbuttons}")
            rE(f"grid forget {toolmat}")
            rE(f"grid forget {fruns}")
    else:
        rE('grid .pane.top.tabs -column 0 -row 1 -sticky nesw -padx 2 -pady 2')
        rE('grid .pane.top.feedoverride -column 0 -row 2 -sticky new')
        rE('grid .pane.top.rapidoverride -column 0 -row 3 -sticky new')
        rE('grid .pane.top.jogspeed -column 0 -row 5 -sticky new')
        # test if we need the angular jog slider (56 = 0x38 = 000111000 = ABC)
        if s.axis_mask & 56 == 0 and 'ANGULAR' in joint_type:
            rE('grid .pane.top.ajogspeed -column 0 -row 6 -sticky new')
        rE(f".pane paneconfigure $pane_top -minsize {bPane}")
        rE(f".pane paneconfigure $pane_bottom -minsize {bPane}")
        rE(f".pane paneconfigure $pane_bottom -height {bPane}")
        populate_run_panel()

def setup_toggle(state):
    if int( state):
        hide_default()
        enable_menus(False)
        rE('grid .toolsetup -column 0 -row 0 -columnspan 3 -sticky nesw')
        rE(f"grid {fsetup} -column 1 -row 1 -rowspan 2 -sticky nsew")
        keyboard_bindings(False)
        vkbData['required'] = True
        vkb_show('keyboard')
    else:
        rE('grid forget .toolsetup')
        rE(f"grid forget {fsetup}")
        show_default()

def param_toggle(state):
    if int(state):
        hide_default()
        enable_menus(False)
        rE('grid .toolparam -column 0 -row 0 -columnspan 3 -sticky nesw')
        rE(f"grid {fparam} -column 1 -row 1 -rowspan 2 -sticky nsew")
        keyboard_bindings(False)
        vkbData['required'] = True
        vkb_show('numpad')
    else:
        rE('grid forget .toolparam')
        rE(f"grid forget {fparam}")
        show_default()

def conv_toggle(state, convSent=False):
    global CONV, convFirstRun, preConvFile, loaded_file, lastViewType
    if int(state):
        lastViewType = get_view_type()
        preview_toggle(True)
        enable_menus(False)
        if pVars.orient.get() == 'portrait':
            rE('grid .toolconv -column 1 -row 2 -columnspan 1 -sticky nesw')
            rE('grid .fconv -column 1 -row 3 -rowspan 1 -sticky nsew')
        else:
            rE('grid .toolconv -column 0 -row 0 -columnspan 3 -sticky nesw')
            rE('grid .fconv -column 0 -row 1 -rowspan 2 -sticky nsew')
        rE(f"{fright} itemconfigure numbers -state disabled")
        for page in rE(f"{fright} pages").split():
            if page != 'preview':
                rE(f"{fright} itemconfigure {page} -state disabled")
        rE(f"{fright} raise preview")
        matIndex = rE(f"{fruns}.material.materials getvalue")
        if comp['development']:
            reload(conversational)
        if convFirstRun or comp['development']:
            CONV = conversational.Conv(convFirstRun, root_window, widgets.toolFrame, \
                   widgets.convFrame, bwidget.ComboBox, imagePath, tmpPath, pVars, \
                   unitsPerMm, comp, PREF, getPrefs, putPrefs, open_file_guts, \
                   wcs_rotation, conv_toggle, color_change, plasmacPopUp, o)
            convFirstRun = False
        # set_conv_preview()
        if loaded_file:
            COPY(loaded_file, preConvFile)
        CONV.start(materialFileDict, matIndex, vars.taskfile.get(), s.g5x_index, commands.set_view_z)
        set_conv_preview()
        keyboard_bindings(False)
        vkbData['required'] = True
        vkb_show('numpad')
    else:
        preview_toggle()
        rE('grid forget .toolconv')
        rE('grid forget .fconv')
        show_default()
        for page in rE(f"{fright} pages").split():
            rE(f"{fright} itemconfigure {page} -state normal")
        reset_conv_preview()
        if convSent:
            loaded_file = vars.taskfile.get()
        else:
            if loaded_file:
                if 'shape.ngc' in loaded_file:
                    COPY(preConvFile, loaded_file)
                open_file_guts(loaded_file, True, False)
                commands.set_view_z()
            else:
                clear_program()
        root_window.update_idletasks()
        if lastViewType == 'p':
            commands.set_view_p()
        elif lastViewType == 't':
            commands.set_view_t()
        else:
            commands.set_view_z()
        CONV.oldConvButton = 'line'

def clear_program():
    file = os.path.join(tmpPath, 'clear.ngc')
    with open(file, 'w') as outFile:
        outFile.write('m2')
    open_file_guts(file, True, False)
    t.configure(state='normal')
    t.delete(0.0, 'end')
    t.configure(state='disabled')

def set_conv_preview():
    ''' configure the preview for conversational '''
    global convViewOptions, joint_dro_format_old , dro_format_old
    convViewOptions['alpha'] = vars.program_alpha.get()
    convViewOptions['dtg'] = vars.show_distance_to_go.get()
    convViewOptions['extents'] = vars.show_extents.get()
    convViewOptions['limits'] = vars.show_machine_limits.get()
    convViewOptions['offsets'] = vars.show_offsets.get()
    convViewOptions['plot'] = vars.show_live_plot.get()
    convViewOptions['rapids'] = vars.show_rapids.get()
    convViewOptions['speed'] = vars.show_machine_speed.get()
    convViewOptions['tool'] = vars.show_tool.get()
    convViewOptions['cone'] = o.cone_basesize
    convViewOptions['origin'] = o.show_small_origin
    vars.program_alpha.set(False)
    vars.show_distance_to_go.set(False)
    vars.show_extents.set(True)
    vars.show_machine_limits.set(False)
    vars.show_offsets.set(False)
    vars.show_live_plot.set(False)
    vars.show_rapids.set(True)
    vars.show_machine_speed.set(False)
    vars.show_tool.set(False)
    o.cone_basesize = .025
    o.show_small_origin = False
    o.hide_icons = True
    o.set_view_z()
    joint_dro_format_old = o.joint_dro_format
    o.joint_dro_format = joint_dro_format_new
    dro_format_old = o.dro_format
    o.dro_format = dro_format_new


def reset_conv_preview():
    ''' configure the preview for gcode file '''
    global convViewOptions
    vars.program_alpha.set(convViewOptions['alpha'])
    vars.show_distance_to_go.set(convViewOptions['dtg'])
    vars.show_extents.set(convViewOptions['extents'])
    vars.show_machine_limits.set(convViewOptions['limits'])
    vars.show_offsets.set(convViewOptions['offsets'])
    vars.show_live_plot.set(convViewOptions['plot'])
    vars.show_rapids.set(convViewOptions['rapids'])
    vars.show_machine_speed.set(convViewOptions['speed'])
    vars.show_tool.set(convViewOptions['tool'])
    o.cone_basesize = convViewOptions['cone']
    o.show_small_origin = convViewOptions['origin']
    o.joint_dro_format = joint_dro_format_old
    o.dro_format = dro_format_old
    o.hide_icons = False

def joint_dro_format_new(s,spd,num_of_joints,limit, homed):
    ''' replace dro with selected material '''
    text = conv_material_text()
    return limit, homed, text, text

def dro_format_new(s,spd,dtg,limit,homed,positions,axisdtg,g5x_offset,g92_offset,tlo_offset):
    ''' replace dro with selected material '''
    text = conv_material_text()
    return limit, homed, text, text

def conv_material_text():
    ''' return a list of material parameters to display '''
    res = 3 if s.linear_units != 1 else 2
    text  = f"Cut Speed={materialFileDict[int(CONV.matCombo.get().split(':')[0])]['cut_speed']:.0f}"
    text += f"   Cut Height={materialFileDict[int(CONV.matCombo.get().split(':')[0])]['cut_height']:.{res}f}"
    text += f"   Kerf Width={materialFileDict[int(CONV.matCombo.get().split(':')[0])]['kerf_width']:.{res}f}"
    return [text]

def hide_default():
    rE('grid forget .pane')
    rE('grid forget .fconv')
    rE('grid forget .toolbar')
    if pVars.orient.get() == 'portrait':
        rE(f"grid forget {fportrait}")
    else:
        rE(f"grid forget {fbuttons}")
        rE(f"grid forget {toolmat}")
        rE(f"grid forget {fruns}")

def show_default():
    rE('grid .toolbar -column 0 -row 0 -columnspan 3 -sticky nesw')
    rE('grid .pane -column 1 -row 1 -rowspan 2 -sticky nsew')
    rE(f".pane paneconfigure $pane_bottom -height {bPane}")
    if not pVars.previewLarge.get():
        populate_run_panel()
    enable_menus(True)
    keyboard_bindings(pVars.kbShortcuts.get())
    vkbData['required'] = False
    rE('focus .')

def enable_menus(state):
    state = 'normal' if state else 'disabled'
    menus = ['File', 'Machine', 'View', 'Conversational', 'Parameters', 'Setup', 'Help']
    for menu in menus:
        rE(f".menu entryconfig {menu} -state {state}")


##############################################################################
# TABLE VIEW FUNCTIONS                                                       #
##############################################################################
def set_view_t(event=None, scale=None):
    # set button states
    widgets.view_z.configure(relief='link')
    widgets.view_z2.configure(relief='link')
    widgets.view_x.configure(relief='link')
    widgets.view_y.configure(relief='link')
    widgets.view_y2.configure(relief='link')
    widgets.view_t.configure(relief='sunken')
    widgets.view_p.configure(relief='link')
    vars.view_type.set(5)
    # set view
    mult = 1 if s.linear_units != 1 else 25.4
    zoomScale = scale * 2 if scale else float(rE(f"{fsetup}.l.gui.zoom get")) * 2
    xTableLength = machineBounds['xLen'] / mult / zoomScale
    yTableLength = machineBounds['yLen'] / mult / zoomScale
    xTableCenter = (machineBounds['X-'] + (machineBounds['xLen'] / 2)) / mult
    yTableCenter = (machineBounds['Y-'] + (machineBounds['yLen'] / 2)) / mult
    o.reset()
    glTranslatef(-xTableCenter, -yTableCenter, 0)
    o.set_eyepoint_from_extents(xTableLength, yTableLength)
    o.perspective = False
    o.lat = o.lon = 0
    glRotateScene(o, 1.0, xTableCenter, yTableCenter, 0, 0, 0, 0, 0)
    o._redraw()

def get_view_type():
    if str(widgets.view_x['relief']) == 'sunken':
        view = 'x'
    elif (str(widgets.view_y['relief']) == 'sunken' or
         str(widgets.view_y2['relief']) == 'sunken'):
        view = 'y'
    elif (str(widgets.view_z['relief']) == 'sunken' or
          str(widgets.view_z2['relief']) == 'sunken'):
        view = 'z'
    elif (str(widgets.view_t['relief']) == 'sunken'):
        view = 't'
    else:
        view = 'p'
    return view


##############################################################################
# STATISTICS FUNCTIONS                                                       #
##############################################################################
def secs_to_hms(s):
    m, s = divmod(int(s), 60)
    h, m = divmod(m, 60)
    return f"{h:02.0f}:{m:02.0f}:{s:02.0f}"

def hms_to_secs(hms):
    h, m, s = hms.split(':')
    return int(h)*3600 + int(m)*60 + int(s)

def save_total_stats():
    if hal.get_value('plasmac.torch-enable'):
        val = float(pVars.lengthT.get()[:len(pVars.lengthT.get()) - 1]) * statDivisor
        pVars.lengthS.set(val)
        putPrefs(PREF, 'STATISTICS', 'Cut length', val, float)
        val = pVars.pierceT.get()
        pVars.pierceS.set(val)
        putPrefs(PREF, 'STATISTICS', 'Pierce count', val, int)
        val = hms_to_secs(pVars.rapidT.get())
        pVars.rapidS.set(val)
        putPrefs(PREF, 'STATISTICS', 'Rapid time', val, int)
        val = hms_to_secs(pVars.probeT.get())
        pVars.probeS.set(val)
        putPrefs(PREF, 'STATISTICS', 'Probe time', val, int)
        val = hms_to_secs(pVars.torchT.get())
        pVars.torchS.set(val)
        putPrefs(PREF, 'STATISTICS', 'Torch on time', val, int)
        val = hms_to_secs(pVars.cutT.get())
        pVars.cutS.set(val)
        putPrefs(PREF, 'STATISTICS', 'Cut time', val, int)
        val = hms_to_secs(pVars.pausedT.get())
        pVars.pausedS.set(val)
        putPrefs(PREF, 'STATISTICS', 'Paused time', val, int)
        val = hms_to_secs(pVars.runT.get())
        pVars.runS.set(val)
        putPrefs(PREF, 'STATISTICS', 'Program run time', val, int)

def clear_job_stats():
    pVars.lengthJ.set(f"0.00{statSuffix}")
    pVars.pierceJ.set(0)
    pVars.rapidJ.set('00:00:00')
    pVars.probeJ.set('00:00:00')
    pVars.torchJ.set('00:00:00')
    pVars.cutJ.set('00:00:00')
    pVars.pausedJ.set('00:00:00')
    pVars.runJ.set('00:00:00')

def reset_all_stats(stat):
    if stat == 'length':
        pVars.lengthS.set(0)
        pVars.lengthJ.set(f"0.00{statSuffix}")
        pVars.lengthT.set(f"0.00{statSuffix}")
        putPrefs(PREF, 'STATISTICS', 'Cut length', 0, float)
    elif stat == 'pierce':
        pVars.pierceS.set(0)
        pVars.pierceJ.set('0')
        pVars.pierceT.set('0')
        putPrefs(PREF, 'STATISTICS', 'Pierce count', 0, int)
    elif stat == 'rapid':
        pVars.rapidS.set(0)
        pVars.rapidJ.set('00:00:00')
        pVars.rapidT.set('00:00:00')
        putPrefs(PREF, 'STATISTICS', 'Rapid time', 0, int)
    elif stat == 'probe':
        pVars.probeS.set(0)
        pVars.probeJ.set('00:00:00')
        pVars.probeT.set('00:00:00')
        putPrefs(PREF, 'STATISTICS', 'Probe time', 0, int)
    elif stat == 'torch':
        pVars.torchS.set(0)
        pVars.torchJ.set('00:00:00')
        pVars.torchT.set('00:00:00')
        putPrefs(PREF, 'STATISTICS', 'Torch on time', 0, int)
    elif stat == 'cut':
        pVars.cutS.set(0)
        pVars.cutJ.set('00:00:00')
        pVars.cutT.set('00:00:00')
        putPrefs(PREF, 'STATISTICS', 'Cut time', 0, int)
    elif stat == 'paused':
        pVars.pausedS.set(0)
        pVars.pausedJ.set('00:00:00')
        pVars.pausedT.set('00:00:00')
        putPrefs(PREF, 'STATISTICS', 'Paused time', 0, int)
    elif stat == 'run':
        pVars.runS.set(0)
        pVars.runJ.set('00:00:00')
        pVars.runT.set('00:00:00')
        putPrefs(PREF, 'STATISTICS', 'Program run time', 0, int)


##############################################################################
# PARAMETER FUNCTIONS                                                        #
##############################################################################
# set appropriate hal pin if parameter changed
def param_changed(widget, value):
    global widgetValues
    item = widget.rsplit('.',1)[1]
    if item in ['x-single-cut','y-single-cut']:
        return
    widgetValues[widget] = value
    if item == 'cut-amps':
        if pmx485['exists']:
            pmx485_current_changed(int(value))
    elif item == 'cut-mode':
        if pmx485['exists']:
            pmx485_mode_changed(int(value))
    elif item == 'gas-pressure':
        if pmx485['exists']:
            pmx485_pressure_changed(float(value))
    elif item == 'cut-speed':
        hal.set_p('plasmac.cut-feed-rate',f"{value}")
    elif item[1:] == '-pierce-offset':
        if extHalPins:
            comp[item] = value
    else:
        hal.set_p(f"plasmac.{item}",f"{value}")

def validate_spinbox(widget, spinType, resolution, value, original):
    dbPrint = True if (not firstRun and comp['development']) else False
    if dbPrint: print(f"\nvalidating {widget}")
    if value == '':
        if dbPrint: print('blank = still valid')
        return True
    minval = float(rE(f"{widget} cget -from")) if spinType == 'flt' else int(float(rE(f"{widget} cget -from")))
    maxval = float(rE(f"{widget} cget -to")) if spinType == 'flt' else int(float(rE(f"{widget} cget -to")))
    # a negative value if range min equals zero is invalid
    if value == '-' and minval >= 0:
        if dbPrint: print('cannot be less than zero = invalid')
        return False
    # a decimal point in an int is invalid
    if '.' in str(value) and spinType == 'int':
        if dbPrint: print('a decimal in an int = invalid')
        return False
    # more digits than the resolution of a float is invalid
    if '.' in str(value) and len(str(value).split('.')[1]) > int(resolution):
        # don't error on first run so we can import parameters from qtplasmac
        if dbPrint: print('float resolution too large = invalid')
        if not firstRun:
            return False
    # we cannot do math on "-", ".", or "-." but they are valid for a float
    if value == '.' or value == '-' or value == '-.':
        if dbPrint: print('illegal math = still valid')
        return True
    # if a float cannot be calculated from value then it is invalid
    # we won't change the hal pin yet
    try:
        v = float(value)
    except:
        if dbPrint: print('cannot calculate a float = invalid')
        return False
    # if value is too large it is invalid
    if v > maxval:
        if dbPrint: print('value out of bounds = invalid')
        return False
    # if value is too small it may be valid
    if v < minval or v > maxval:
        if dbPrint: print('value out of bounds = still valid')
        return True
    # must be a good number...
    if widget == f"{fsetup}.l.gui.zoom":
        commands.set_view_t(None, v)
    elif widget == f"{fsetup}.l.gui.jogspeed":
        jog_default_changed(value)
    elif widget == f"{fsetup}.l.gui.crspeed":
        cut_rec_default_changed(value)
    else:
        param_changed(widget,value)
    if dbPrint: print('valid entry')
    return True

def load_param_clicked():
    for widget in cpList:
        try:
            # bring in all parameters as floats to allow reading QtPlasmaC parameters
            value = getPrefs(PREF, 'PLASMA_PARAMETERS', str(widget[8]), widget[3], float)
        except:
            value = 0
            title = _('PARAMETER ERROR')
            msg0 = _('Invalid parameter for')
            plasmacPopUp('error', title, f"{msg0}: {widget[8]}")
            continue
        # convert to int here if required
        value = value if widget[2] > 0 else int(value)
        rE(f"{widget[0]}.{widget[1]} set {value}")
        widgetValues[f"{widget[0]}.{widget[1]}"] = value
    value = getPrefs(PREF, 'ENABLE_OPTIONS', 'THC auto', False, bool)
    pVars.thcAuto.set(value)
    mode_changed()

def save_param_clicked():
    for widget in cpList:
        value = rE(f"{widget[0]}.{widget[1]} get")
        if widget[2] > 0:
            putPrefs(PREF, 'PLASMA_PARAMETERS', str(widget[8]), value, float)
        else:
            putPrefs(PREF, 'PLASMA_PARAMETERS', str(widget[8]), value, int)
    value = pVars.thcAuto.get()
    putPrefs(PREF, 'ENABLE_OPTIONS', 'THC auto', value, bool)

def load_setup_clicked():
    fontChanged = False
    rE(f"{fsetup}.l.gui.jogspeed set {restoreSetup['jogSpeed']}")
    jog_default_changed(restoreSetup['jogSpeed'])
    if pVars.plasmacMode.get() != restoreSetup['plasmacMode']:
        pVars.plasmacMode.set(restoreSetup['plasmacMode'])
        mode_changed()
    if pVars.winSize.get() != restoreSetup['winSize'] or \
       rE(f"winfo geometry {root_window}").split('+', 1)[0] != restoreSetup['winLast']:
        pVars.winSize.set(restoreSetup['winSize'])
        set_window_size(restoreSetup['winLast'])
    if pVars.orient.get() != restoreSetup['orient']:
        pVars.orient.set(restoreSetup['orient'])
        set_orientation()
    if pVars.fontSize.get() != restoreSetup['fontSize']:
        pVars.fontSize.set(restoreSetup['fontSize'])
        fontChanged = True
    if pVars.guiFont.get() != restoreSetup['guiFont']:
        pVars.guiFont.set(restoreSetup['guiFont'])
        fontChanged = True
    if pVars.codeFont.get() != restoreSetup['codeFont']:
        pVars.codeFont.set(restoreSetup['codeFont'])
        fontChanged = True
    if fontChanged:
        font_changed()
    if pVars.coneSize.get() != restoreSetup['coneSize']:
        pVars.coneSize.set(restoreSetup['coneSize'])
        cone_size_changed()
    if pVars.popLocation.get() != restoreSetup['popLocation']:
        pVars.popLocation.set(restoreSetup['popLocation'])
    if pVars.matDefault.get() != restoreSetup['matDefault']:
        pVars.matDefault.set(restoreSetup['matDefault'])
        display_selected_material(restoreSetup['matDefault'])
    if pVars.useVirtKB.get() != restoreSetup['useVirtKB']:
        pVars.useVirtKB.set(restoreSetup['useVirtKB'])
        virtual_kb_changed()
    if pVars.useVirtKB.get():
        pVars.kbShortcuts.set(0)
        keyboard_bindings(0)
    else:
        if pVars.kbShortcuts.get() != restoreSetup['kbShortcuts']:
            pVars.kbShortcuts.set(restoreSetup['kbShortcuts'])
            keyboard_bindings(restoreSetup['kbShortcuts'])
    if int(rE(f"{fsetup}.l.gui.crspeed get")) == restoreSetup['crPercent']:
        rE(f"{fcrspeed}.display.cut-rec-speed set {restoreSetup['crPercent']}")
    else:
        rE(f"{fsetup}.l.gui.crspeed set {restoreSetup['crPercent']}")
    pVars.closeDialog.set(restoreSetup['closeDialog'])
    pVars.closeText.set(restoreSetup['closeText'])
    rE(f"{fsetup}.l.gui.zoom set {restoreSetup['tableZoom']}")
    if not (isPaused or isRunning):
        user_button_load()
    read_colors()
    color_change()

def save_setup_clicked():
    if int(rE(f"{fsetup}.l.gui.jogspeed get")) < minJogSpeed:
        rE(f"{fsetup}.l.gui.jogspeed set {minJogSpeed}")
    restoreSetup['jogSpeed'] = rE(f"{fsetup}.l.gui.jogspeed get")
    putPrefs(PREF, 'GUI_OPTIONS', 'Jog speed', restoreSetup['jogSpeed'], int)
    restoreSetup['plasmacMode'] = pVars.plasmacMode.get()
    putPrefs(PREF, 'GUI_OPTIONS', 'Mode', restoreSetup['plasmacMode'], int)
    restoreSetup['closeText'] = pVars.closeText.get()
    putPrefs(PREF, 'GUI_OPTIONS', 'Exit warning text', restoreSetup['closeText'], str)
    restoreSetup['closeDialog'] = pVars.closeDialog.get()
    putPrefs(PREF, 'GUI_OPTIONS', 'Exit warning', restoreSetup['closeDialog'], bool)
    restoreSetup['winSize'] = pVars.winSize.get()
    putPrefs(PREF, 'GUI_OPTIONS', 'Window size', restoreSetup['winSize'], str)
    restoreSetup['winLast'] = rE(f"winfo geometry {root_window}").split('+', 1)[0]
    putPrefs(PREF, 'GUI_OPTIONS', 'Window last', restoreSetup['winLast'], str)
    restoreSetup['orient'] = pVars.orient.get()
    putPrefs(PREF, 'GUI_OPTIONS', 'Orientation', restoreSetup['orient'], str)
    restoreSetup['fontSize'] = pVars.fontSize.get()
    putPrefs(PREF, 'GUI_OPTIONS', 'Font size', restoreSetup['fontSize'], str)
    restoreSetup['guiFont'] = pVars.guiFont.get()
    putPrefs(PREF, 'GUI_OPTIONS', 'GUI font', restoreSetup['guiFont'], str)
    restoreSetup['codeFont'] = pVars.codeFont.get()
    putPrefs(PREF, 'GUI_OPTIONS', 'Gcode font', restoreSetup['codeFont'], str)
    restoreSetup['coneSize'] = pVars.coneSize.get()
    putPrefs(PREF, 'GUI_OPTIONS', 'Preview cone size', restoreSetup['coneSize'], float)
    restoreSetup['popLocation'] = pVars.popLocation.get()
    putPrefs(PREF, 'GUI_OPTIONS', 'Popup location', restoreSetup['popLocation'], str)
    restoreSetup['tableZoom'] = float(rE(f"{fsetup}.l.gui.zoom get"))
    putPrefs(PREF, 'GUI_OPTIONS', 'Table zoom', restoreSetup['tableZoom'], float)
    restoreSetup['matDefault'] = pVars.matDefault.get()
    putPrefs(PREF, 'GUI_OPTIONS', 'Default material', restoreSetup['matDefault'], int)
    restoreSetup['crPercent'] = rE(f"{fsetup}.l.gui.crspeed get")
    putPrefs(PREF, 'GUI_OPTIONS', 'Cut recovery speed %', restoreSetup['crPercent'], int)
    rE(f"{fcrspeed}.display.cut-rec-speed set {restoreSetup['crPercent']}")
    restoreSetup['kbShortcuts'] = pVars.kbShortcuts.get()
    putPrefs(PREF, 'GUI_OPTIONS', 'Use keyboard shortcuts', restoreSetup['kbShortcuts'], bool)
    restoreSetup['useVirtKB'] = pVars.useVirtKB.get()
    putPrefs(PREF, 'GUI_OPTIONS', 'Use soft keyboard', restoreSetup['useVirtKB'], bool)
    if not (isPaused or isRunning):
        user_button_save()
    putPrefs(PREF, 'GUI_OPTIONS', 'Foreground color', colorFore, str)
    putPrefs(PREF, 'GUI_OPTIONS', 'Background color', colorBack, str)
    putPrefs(PREF, 'GUI_OPTIONS', 'Disabled color', colorDisable, str)
    putPrefs(PREF, 'GUI_OPTIONS', 'Active color', colorActive, str)
    putPrefs(PREF, 'GUI_OPTIONS', 'Warning color', colorWarn, str)
    putPrefs(PREF, 'GUI_OPTIONS', 'Voltage color', colorVolt, str)
    putPrefs(PREF, 'GUI_OPTIONS', 'Arc OK color', colorArc, str)
    putPrefs(PREF, 'GUI_OPTIONS', 'LED color', colorLed, str)
    putPrefs(PREF, 'GUI_OPTIONS', 'Trough color', colorTrough, str)
    for button in togglePins:
        set_toggle_pins(button)


##############################################################################
# GENERAL FUNCTIONS                                                          #
##############################################################################
def thc_enable_toggled():
    hal.set_p('plasmac.thc-enable', str(pVars.thcEnable.get()))
    putPrefs(PREF, 'ENABLE_OPTIONS','THC enable', pVars.thcEnable.get(), bool)

def corner_enable_toggled():
    hal.set_p('plasmac.cornerlock-enable', str(pVars.cornerEnable.get()))
    putPrefs(PREF, 'ENABLE_OPTIONS','Corner lock enable', pVars.cornerEnable.get(), bool)

def void_enable_toggled():
    hal.set_p('plasmac.voidlock-enable', str(pVars.voidEnable.get()))
    putPrefs(PREF, 'ENABLE_OPTIONS','Void lock enable', pVars.voidEnable.get(), bool)

def auto_volts_toggled():
    hal.set_p('plasmac.use-auto-volts', str(pVars.autoVolts.get()))
    putPrefs(PREF, 'ENABLE_OPTIONS', 'Use auto volts', pVars.autoVolts.get(), bool)

def ohmic_enable_toggled():
    hal.set_p('plasmac.ohmic-probe-enable', str(pVars.ohmicEnable.get()))
    putPrefs(PREF, 'ENABLE_OPTIONS', 'Ohmic probe enable', pVars.ohmicEnable.get(), bool)

def ignore_arc_ok_toggled():
    hal.set_p('plasmac.ignore-arc-ok-1', str(pVars.ignorArcOk.get()))

def laser_button_enable():
    if laserOffsets['X'] or laserOffsets['Y']:
        rE(f"grid {fjogf}.zerohome.laser -column 3 -row 0")
        rE(f"grid {fcrbuttons}.laser -column 0 -row 0")
    else:
        rE(f"grid forget {fjogf}.zerohome.laser")
        rE(f"grid forget {fcrbuttons}.laser")

def offset_probe_setup():
    hal.set_p('plasmac.offset-probe-x', f"{probeOffsets['X']}")
    hal.set_p('plasmac.offset-probe-y', f"{probeOffsets['Y']}")
    hal.set_p('plasmac.offset-probe-delay', f"{probeOffsets['Delay']}")
    if probeOffsets['X'] or probeOffsets['Y']:
        rE(f"grid {fparam}.c1.probe.offset-feed-rateL -column 0 -row 6 -sticky e -padx {{4 0}} -pady {{4 0}}")
        rE(f"grid {fparam}.c1.probe.offset-feed-rate -column 1 -row 6 -sticky e -padx {{0 4}} -pady {{4 0}}")
    else:
        rE(f"grid forget {fparam}.c1.probe.offset-feed-rateL")
        rE(f"grid forget {fparam}.c1.probe.offset-feed-rate")

def set_toggle_pins(button):
    togglePins[button]['state'] = hal.get_value(togglePins[button]['pin'])
    if togglePins[button]['state']:
        rE(f"{fbuttons}.button{button} configure -bg {colorActive}")
        rE(f"{fbuttons}.button{button} configure -text {{{togglePins[button]['ontext']}}}")
    else:
        if togglePins[button]['runcritical']:
            rE(f"{fbuttons}.button{button} configure -bg {colorWarn}")
        else:
            rE(f"{fbuttons}.button{button} configure -bg {colorBack}")
        rE(f"{fbuttons}.button{button} configure -text {{{togglePins[button]['offtext']}}}")

def jog_default_changed(value):
    set_jog_slider(int(value) / (vars.max_speed.get() * 60))

def set_jog_slider(value):
    # jog slider value is from 0 to 1
    if not manualCut['state']:
        rE(f"set jog_slider_val {value}")
        rE(f"update_jog_slider_vel {value}")

def tar_filter(tarinfo):
     if os.path.splitext(tarinfo.name)[1] in ['.pyc']:
          return None
     else:
          return tarinfo

def backup_clicked():
    n = time.localtime()
    d = f"{str(n.tm_year)[-2:]}{n.tm_mon:02d}{n.tm_mday:02d}"
    t = f"{n.tm_hour:02d}{n.tm_min:02d}{n.tm_sec:02d}"
    configDir = os.path.dirname(vars.emcini.get())
    outName = f"{vars.machine.get()}_v{VER}_{d}_{t}.tar.gz"
    outFile = os.path.join(os.path.expanduser('~'), outName)
    with tarfile.open(outFile, mode='w:gz', ) as archive:
        archive.add(configDir, filter=tar_filter)
    title = _('BACKUP')
    msg0 = _('A compressed backup of the machine configuration has been saved in your home directory')
    msg1 = _('The file name is')
    msg2 = _('This file may be attached to a post on the LinuxCNC forum to aid in problem solving')
    plasmacPopUp('info', title, f"{msg0}\n\n{msg1}: {outName}\n\n{msg2}\n")

def torch_enable():
    hal.set_p('plasmac.torch-enable', str(not hal.get_value('plasmac.torch-enable')))
    color_torch()

def update_preview(clear=False):
    c.wait_complete(.25)
    s.poll()
    while s.interp_state != linuxcnc.INTERP_IDLE:
        o.tkRedraw()
        o.redraw_dro()
        o.update_idletasks()
        root_window.update()
        s.poll()
    if clear:
       live_plotter.clear()

def user_manual():
    web = webbrowser.open_new_tab('https://phillc54.github.io/plasmac2-docs/plasmac2.html')
    if not web:
        msg0 = _('Cannot open user manual web page')
        notifications.add('error', f"{msg0}")


##############################################################################
# JOINTS/AXES FUNCTIONS                                                      #
##############################################################################
def ja_button_setup(widget, button, text):
    rE(f"radiobutton {widget} -value {button} -text {text} -anchor center \
       -variable ja_rbutton -command ja_button_activated -padx 10 -pady 6 \
       -indicatoron 0 -bd 2 -highlightthickness 0 -selectcolor {colorActive}")

def ja_button_activated():
    if vars.ja_rbutton.get() in 'xyzabcuvw':
        widget = getattr(widgets, f"axis_{vars.ja_rbutton.get()}")
        widget.focus()
        rE(f"{fjogf}.zerohome.zero configure -text {vars.ja_rbutton.get().upper()}0")
        if not homing_order_defined:
            pre = _('Home')
            widgets.homebutton.configure(text = f"{pre} {vars.ja_rbutton.get().upper()}")
    else:
        widget = getattr(widgets, f"joint_{vars.ja_rbutton.get()}")
        widget.focus()
    commands.axis_activated()

def joint_mode_switch(a, b, c):
    global lastMotionMode
    if vars.motion_mode.get() == linuxcnc.TRAJ_MODE_FREE and s.kinematics_type != linuxcnc.KINEMATICS_IDENTITY:
        rE(f"grid forget {fmanual}.axes")
        rE(f"grid {fmanual}.joints -column 0 -row 0 -padx 2 -sticky w")
        widget = getattr(widgets, 'joint_0')
        widget.focus()
        vars.ja_rbutton.set(0)
    elif lastMotionMode == linuxcnc.TRAJ_MODE_FREE or not lastMotionMode:
        rE(f"grid forget {fmanual}.joints")
        rE(f"grid {fmanual}.axes -column 0 -row 0 -padx 2 -sticky w")
        widget = getattr(widgets, f"axis_{first_axis}")
        widget.focus()
        vars.ja_rbutton.set(first_axis)
    lastMotionMode = vars.motion_mode.get()

##############################################################################
# MANUAL CUT                                                                 #
##############################################################################
def manual_cut(event):
    global manualCut
    if manual_ok():
        if not hal.get_value('spindle.0.on'):
            if not critical_button_check():
                return
            msg = bounds_check_probe(True)
            if msg:
                notifications.add('error', msg)
                return
            c.spindle(linuxcnc.SPINDLE_FORWARD, 1)
            manualCut['feed'] = vars.jog_speed.get()
            vars.jog_speed.set(int(rE(f"{fruns}.material.cut-feed-rate get")))
            manualCut['state'] = True
            rE('.pane.top.jogspeed.s configure -state disabled')
            clear_job_stats()
        else:
            manualCut['state'] = False
            c.spindle(linuxcnc.SPINDLE_OFF)
            vars.jog_speed.set(manualCut['feed'])
            rE('.pane.top.jogspeed.s configure -state normal')
            save_total_stats()


##############################################################################
# SINGLE CUT                                                                 #
##############################################################################
def single_cut():
    global result, singleCut
    result = False

    def cut_file(event):
        global result
        result = True
        sc.quit()

    def cancel_file(event):
        sc.quit()

    title = _('SINGLE CUT')
    valid, xLength, yLength = plasmacPopUp('sc', title, None, 'numpad').reply
    vkbData['required'] = False
    if not valid:
        return
    try:
        xLength = float(xLength)
        yLength = float(yLength)
    except:
        title = _('ENTRY ERROR')
        msg = _('Lengths need to be floating point values')
        plasmacPopUp('error', title, msg)
        return
    singleCut['state'] = True
    singleCut['G91'] = True if 910 in s.gcodes else False
    putPrefs(PREF, 'SINGLE_CUT', 'X length', xLength, float)
    putPrefs(PREF, 'SINGLE_CUT', 'Y length', yLength, float)
    xEnd = s.position[0] + float(xLength)
    yEnd = s.position[1] + float(yLength)
    scFile = os.path.join(tmpPath, 'single_cut.ngc')
    with open(scFile, 'w') as f:
        f.write('G90\n')
        f.write('F#<_hal[plasmac.cut-feed-rate]>\n')
        f.write(f"G53 G0 X{s.position[0]:0.6f} Y{s.position[1]:0.6f}\n")
        f.write('M3 $0 S1\n')
        f.write(f"G53 G1 X{xEnd:0.6f} Y{yEnd:0.6f}\n")
        f.write('M5 $0\n')
        f.write('M2\n')
    if loaded_file != scFile:
        pVars.preRflFile.set(loaded_file)
    open_file_guts(scFile, False, False)
    commands.set_view_z()


##############################################################################
# ALIGNMENT FUNCTIONS                                                        #
##############################################################################
def touch_off_xy(state, x, y):
    if state == '0' and isIdleHomed:
        if manual_ok():
            save_task_mode = s.task_mode
            ensure_mode(linuxcnc.MODE_MDI)
            c.mdi(f"G10 L20 P0 X{x} Y{y}")
            c.wait_complete()
            s.poll()
            o.tkRedraw()
            reload_file(False)
            ensure_mode(save_task_mode)
            set_motion_teleop(1)
            o.redraw_dro()

def laser_button_toggled(state, button):
    global laserTimer, laserButtonState, laserText, laserOffsets
    global machineBounds, isIdleHomed
    if state == '1': # button pressed
        if s.paused and not hal.get_value('plasmac.laser-recovery-state'):
            xPos = s.position[0] + laserOffsets['X']
            yPos = s.position[1] + laserOffsets['Y']
            if xPos < machineBounds['X-'] or xPos > machineBounds['X+'] or yPos < machineBounds['Y-'] or yPos > machineBounds['Y+']:
                msg0 = _('Torch cannot move outside the machine boundary')
                notifications.add('error', f"{msg0}")
                return
            hal.set_p('plasmac.laser-x-offset', f"{str(int(laserOffsets['X'] / hal.get_value('plasmac.offset-scale')))}")
            hal.set_p('plasmac.laser-y-offset', f"{str(int(laserOffsets['Y'] / hal.get_value('plasmac.offset-scale')))}")
            hal.set_p('plasmac.laser-recovery-start', '1')
            hal.set_p('plasmac.cut-recovery', '1')
            comp['laser-on'] = 1
            return
        # long press timer
        elif isIdleHomed:
            laserTimer = 700
            root_window.after(100, laser_long_press_timer)
    else: # button released
        if laserButtonState == 'reset':
            laserButtonState = 'laser'
            return
        laserTimer = 0
        if not isIdleHomed:
            return
        xPos = s.position[0] - laserOffsets['X']
        yPos = s.position[1] - laserOffsets['Y']
        if xPos < machineBounds['X-'] or xPos > machineBounds['X+'] or yPos < machineBounds['Y-'] or yPos > machineBounds['Y+']:
            msg0 = _('Laser is outside the machine boundary')
            notifications.add('error', f"{msg0}")
            return
        if laserButtonState == 'laser':
            pVars.laserText.set(_('Mark'))
            laserButtonState = 'markedge'
            comp['laser-on'] = 1
            return
        elif laserButtonState == 'setorigin':
            comp['laser-on'] = 0
        laserButtonState = sheet_align('laser', laserButtonState, laserOffsets['X'], laserOffsets['Y'])

def laser_long_press_timer():
    global laserTimer, laserButtonState
    if laserTimer > 0:
        laserTimer -= 100
        if laserTimer <= 0:
            comp['laser-on'] = 0
            laserButtonState = 'reset'
            pVars.laserText.set(_('Laser'))
        else:
            root_window.after(100, laser_long_press_timer)

def sheet_align(mode, buttonState, offsetX, offsetY):
    global relPos, startAlignPos
    msgList = []
    if buttonState == 'markedge':
#FIXME: NO CAMERA - IS A CAMERA WORTHWHILE ON A PLASMA TABLE?
#        zAngle = self.w.camview.rotation = 0
        ensure_mode(linuxcnc.MODE_MDI)
        c.mdi('G10 L2 P0 R0')
        c.wait_complete()
        live_plotter.clear()
#FIXME: NO CAMERA - IS A CAMERA WORTHWHILE ON A PLASMA TABLE?
#        self.w.cam_goto.setEnabled(False)
        pVars.laserText.set(_('Origin'))
        buttonState = 'setorigin'
        startAlignPos['X'] = relPos['X']
        startAlignPos['Y'] = relPos['Y']
    else:
        if mode == 'laser':
            pVars.laserText.set(_('Laser'))
            buttonState = 'laser'
        else:
            pVars.laserText.set(_('Mark'))
            buttonState = 'markedge'
        xDiff = relPos['X'] - startAlignPos['X']
        yDiff = relPos['Y'] - startAlignPos['Y']
        if xDiff and yDiff:
            zAngle = math.degrees(math.atan(yDiff / xDiff))
            if xDiff > 0:
                zAngle += 180
            elif yDiff > 0:
                zAngle += 360
            if abs(xDiff) < abs(yDiff):
                zAngle -= 90
        elif xDiff:
            if xDiff > 0:
                zAngle = 180
            else:
                zAngle = 0
        elif yDiff:
            if yDiff > 0:
                zAngle = 180
            else:
                zAngle = 0
        else:
            zAngle = 0
#FIXME: NO CAMERA - IS A CAMERA WORTHWHILE ON A PLASMA TABLE?
#        self.w.camview.rotation = zAngle
        ensure_mode(linuxcnc.MODE_MDI)
        c.mdi(f"G10 L20 P0 X{offsetX} Y{offsetY}")
        c.wait_complete()
        c.mdi(f"G10 L2 P0 R{zAngle}")
        c.wait_complete()
        if loaded_file:
            commands.reload_file(False)
        if 1:
            ensure_mode(linuxcnc.MODE_MDI)
            c.mdi('G0 X0 Y0')
            c.wait_complete()
#FIXME: NO CAMERA - IS A CAMERA WORTHWHILE ON A PLASMA TABLE?
#        self.w.cam_goto.setEnabled(True)
        live_plotter.clear()
    return buttonState


##############################################################################
# FRAMING FUNCTIONS                                                          #
##############################################################################
def frame_error(torch, msgList):
    title = _('AXIS LIMIT ERROR')
    if not torch:
        msgList += '\n'
        msgList += _('Do you want to try with the torch?')
        reply = plasmacPopUp('yesno', title, msgList).reply
    else:
        msgList += '\n'
        msgList += _('Framing cannot proceed')
        reply = plasmacPopUp('error', title, msgList).reply
    return reply

def frame_job(feed, height):
    global framingState, activeFunction
    if o.canon:
        msgList, frame_points = bounds_check_framing(laserOffsets['X'], laserOffsets['Y'], True)
        if msgList:
            reply = frame_error(False, msgList)
            if not reply:
                return
            msgList, frame_points = bounds_check_framing()
            if msgList:
                reply = frame_error(True, msgList)
                return
        if s.interp_state == linuxcnc.INTERP_IDLE:
            activeFunction = True
            if not msgList:
                comp['laser-on'] = 1
            if not feed:
                feed = widgetValues[f"{fruns}.material.cut-feed-rate"]
            ensure_mode(linuxcnc.MODE_MDI)
            previousMode = None
            # metric machine imperial units
            if s.linear_units == 1 and s.program_units == 1:
                previousMode = 'G20'
                c.mdi('G21')
            # imperial machine metric units
            elif s.linear_units != 1 and s.program_units == 2:
                    previousMode = 'G21'
                    c.mdi('G20')
            c.wait_complete()
            if not height:
                height = machineBounds['Z+'] - (hal.get_value('plasmac.max-offset') * unitsPerMm)
                c.mdi(f"G53 G0 Z{height:0.4f}")
            c.mdi(f"G53 G0 X{frame_points[1][0]:0.2f} Y{frame_points[1][1]:0.2f}")
            c.mdi(f"G53 G1 X{frame_points[2][0]:0.2f} Y{frame_points[2][1]:0.2f} F{feed:0.0f}")
            c.mdi(f"G53 G1 X{frame_points[3][0]:0.2f} Y{frame_points[3][1]:0.2f}")
            c.mdi(f"G53 G1 X{frame_points[4][0]:0.2f} Y{frame_points[4][1]:0.2f}")
            c.mdi(f"G53 G1 X{frame_points[1][0]:0.2f} Y{frame_points[1][1]:0.2f}")
            c.mdi('G0 X0 Y0')
            if previousMode:
                c.mdi(previousMode)
            framingState = True

def rotate_frame(coordinates):
    angle = math.radians(s.rotation_xy)
    cos = math.cos(angle)
    sin = math.sin(angle)
    frame_points = [coordinates[0]]
    ox = frame_points[0][0]
    oy = frame_points[0][1]
    for x, y in coordinates[1:]:
        tox = x - ox
        toy = y - oy
        rx = (tox * cos) - (toy * sin) + ox
        ry = (tox * sin) + (toy * cos) + oy
        frame_points.append([rx, ry])
    xMin = min(frame_points[1:])[0]
    xMax = max(frame_points[1:])[0]
    yMin = min(frame_points[1:])[1]
    yMax = max(frame_points[1:])[1]
    return frame_points, xMin, yMin, xMax, yMax

##############################################################################
# BOUNDS CHECKS                                                              #
##############################################################################
def bounds_check_file():
    msg = _('G-code')
    # canon units are always returned as imperial
    # ensure returned units are in machine units
    unitsMultiplier = 25.4 if s.linear_units == 1 else 1
    xMin = (o.canon.min_extents[0] * unitsMultiplier)
    xMax = (o.canon.max_extents[0] * unitsMultiplier)
    yMin = (o.canon.min_extents[1] * unitsMultiplier)
    yMax = (o.canon.max_extents[1] * unitsMultiplier)
    errMsg = bounds_compare(xMin, xMax, yMin, yMax, msg)
    return errMsg

def bounds_check_probe(local):
    msg = _('Move to probe offset')
    xPierceOffset = yPierceOffset = xMinP = xMaxP = yMinP = yMaxP = 0
    if local:
        xMinP = xMaxP = s.position[0] + probeOffsets['X']
        yMinP = yMaxP = s.position[1] + probeOffsets['Y']
    elif any((comp['x_min_pierce_extent'], comp['x_max_pierce_extent'], \
              comp['y_min_pierce_extent'], comp['y_max_pierce_extent'])):
        if comp['cut-type']:
            #TODO PHILL - Do you want this from the GUI or HAL??
            xPierceOffset = comp['x-pierce-offset']
            yPierceOffset = comp['y-pierce-offset']
        if s.linear_units == 1 and not vars.metric.get():
            unitsMultiplier = 25.4
        elif s.linear_units != 1 and vars.metric.get():
            unitsMultiplier = 1 / 25.4
        else:
            unitsMultiplier = 1
        xMinP = comp['x_min_pierce_extent'] * unitsMultiplier + probeOffsets['X'] + s.g5x_offset[0] + xPierceOffset
        xMaxP = comp['x_max_pierce_extent'] * unitsMultiplier + probeOffsets['X'] + s.g5x_offset[0] + xPierceOffset
        yMinP = comp['y_min_pierce_extent'] * unitsMultiplier + probeOffsets['Y'] + s.g5x_offset[1] + yPierceOffset
        yMaxP = comp['y_max_pierce_extent'] * unitsMultiplier + probeOffsets['Y'] + s.g5x_offset[1] + yPierceOffset
    errMsg = bounds_compare(xMinP, xMaxP, yMinP, yMaxP, msg)
    return errMsg

def bounds_check_framing(xOffset=0, yOffset=0, laser=False):
    msg = _('Framing move')
    if laser:
        msg1 = _('due to laser offset')
    else:
        msg1 = ''
    # canon units are always returned as imperial
    # ensure returned units are in machine units
    unitsMultiplier = 25.4 if s.linear_units == 1 else 1
    xMin = (o.canon.min_extents_zero_rxy[0] * unitsMultiplier + xOffset)
    xMax = (o.canon.max_extents_zero_rxy[0] * unitsMultiplier + xOffset)
    yMin = (o.canon.min_extents_zero_rxy[1] * unitsMultiplier + yOffset)
    yMax = (o.canon.max_extents_zero_rxy[1] * unitsMultiplier + yOffset)
    coordinates = [[s.g5x_offset[0], s.g5x_offset[1]], [xMin, yMin], [xMin, yMax], [xMax, yMax], [xMax, yMin]]
    frame_points, xMin, yMin, xMax, yMax = rotate_frame(coordinates)
    errMsg = bounds_compare(xMin, xMax, yMin, yMax, msg, msg1)
    return (errMsg, frame_points)

def bounds_compare(xMin, xMax, yMin, yMax, msg, msg1=''):
    global machineBounds
    errMsg = ''
    epsilon = 1e-4
    txtxMin = _('exceeds the X minimum limit by')
    txtxMax = _('exceeds the X maximum limit by')
    txtyMin = _('exceeds the Y minimum limit by')
    txtyMax = _('exceeds the Y maximum limit by')
    lessThan = _('less than')
    if xMin < machineBounds['X-']:
        errMsg += f'{msg} {txtxMin} {lessThan} {epsilon} {unitSuffix} {msg1}\n' if (machineBounds['X-'] - xMin) < epsilon \
            else f'{msg} {txtxMin} {machineBounds["X-"] - xMin:0.4f} {unitSuffix} {msg1}\n'
    if xMax > machineBounds['X+']:
        errMsg += f'{msg} {txtxMax} {lessThan} {epsilon} {unitSuffix} {msg1}\n' if (xMax - machineBounds['X+']) < epsilon \
            else f'{msg} {txtxMax} {xMax - machineBounds["X+"]:0.4f} {unitSuffix} {msg1}\n'
    if yMin < machineBounds['Y-']:
        errMsg += f'{msg} {txtyMin} {lessThan} {epsilon} {unitSuffix} {msg1}\n' if (machineBounds['Y-'] - yMin) < epsilon \
            else f'{msg} {txtyMin} {machineBounds["Y-"] - yMin:0.4f} {unitSuffix} {msg1}\n'
    if yMax > machineBounds['Y+']:
        errMsg += f'{msg} {txtyMax} {lessThan} {epsilon} {unitSuffix} {msg1}\n' if (yMax - machineBounds['Y+']) < epsilon \
            else f'{msg} {txtyMax} {yMax - machineBounds["Y+"]:0.4f} {unitSuffix} {msg1}\n'
    return errMsg

##############################################################################
# PERIPHERAL OFFSET FUNCTIONS                                                #
##############################################################################
def offsets_text():
    text = []
    text.append(_('1. Jog until the peripheral is centered on the mark'))
    text.append(_('2. Click the Yes button to get the offsets'))
    return text

def set_peripheral_offsets():
    toolFile = os.path.realpath(tooltable)
    setup_toggle(False)
    while 1:
        reply = offsets_show(toolFile)
        if not reply:
            break

def offsets_show(toolFile):
    text = []
    text.append(_('1. Place some scrap material under the torch'))
    text.append(_('2. Touchoff the torch to X0Y0'))
    text.append(_('3. Mark the material with a torch pulse'))
    text.append(_('4. Click the appropriate button to activate the peripheral'))
    reply = plasmacPopUp('offsets', _('SET PERIPHERAL OFFSETS'), ' \n'.join(text)).reply
    if reply == 'laser':
        comp['laser-on'] = True
        offsets_laser_clicked()
    elif reply == 'scribe':
        comp['offset-set-scribe'] = True
        offsets_scribe_clicked(toolFile)
    elif reply == 'probe':
        comp['offset-set-probe'] = True
        offsets_probe_clicked()
    else:
        comp['laser-on'] = False
        comp['offset-set-scribe'] = False
        comp['offset-set-probe'] = False
        return False
    return True

def offsets_laser_clicked():
    reply = plasmacPopUp('yesno', _('SET LASER OFFSETS'), ' \n'.join(offsets_text())).reply
    if not reply:
        comp['laser-on'] = False
        return
    newOffsets = {'X':round(s.position[0] - s.g5x_offset[0] - s.g92_offset[0], 4) + 0, \
                  'Y':round(s.position[1] - s.g5x_offset[1] - s.g92_offset[1], 4) + 0}
    if offsets_prompt(_('CHANGE LASER OFFSETS'), laserOffsets, newOffsets):
        laserOffsets['X'] = newOffsets['X']
        laserOffsets['Y'] = newOffsets['Y']
        putPrefs(PREF, 'LASER_OFFSET', 'X axis', laserOffsets['X'], float)
        putPrefs(PREF, 'LASER_OFFSET', 'Y axis', laserOffsets['Y'], float)
        laser_button_enable()
        comp['laser-on'] = False
        title = _('LASER OFFSETS')
        msg = _('Laser offsets have been saved')
        plasmacPopUp('info', title, msg)
    comp['laser-on'] = False

def offsets_scribe_clicked(toolFile):
    try:
        scribeOffsets = {'X': 0, 'Y': 0}
        tool = []
        with open(toolFile, 'r') as inFile:
            for line in inFile:
                if line.startswith('T1'):
                    tool = line.split()
                    inFile.close()
                    break
        if tool:
            for item in tool:
                if item.startswith('X'):
                    scribeOffsets['X'] = float(item.replace('X','').replace('+',''))
                elif item.startswith('Y'):
                    scribeOffsets['Y'] = float(item.replace('Y','').replace('+',''))
    except:
        title = _('TOOL FILE ERROR')
        msg = _('Could not get current scribe offsets from tooltable')
        plasmacPopUp('info', title, msg)
        return
    reply = plasmacPopUp('yesno', _('SET SCRIBE OFFSETS'), ' \n'.join(offsets_text())).reply
    if not reply:
        comp['offset-set-scribe'] = False
        return
    newOffsets = {'X':round(s.position[0] - s.g5x_offset[0] - s.g92_offset[0], 4) + 0, \
                  'Y':round(s.position[1] - s.g5x_offset[1] - s.g92_offset[1], 4) + 0}
    if offsets_prompt(_('CHANGE SCRIBE OFFSETS'), scribeOffsets, newOffsets):
        scribeOffsets['X'] = newOffsets['X']
        scribeOffsets['Y'] = newOffsets['Y']
        offsets_write_scribe(toolFile, scribeOffsets)
        c.load_tool_table()
        comp['offset-set-scribe'] = False
        title = _('SCRIBE OFFSETS')
        msg = _('Scribe offsets have been saved')
        plasmacPopUp('info', title, msg)
    comp['offset-set-scribe'] = False

def offsets_probe_clicked():
    reply = plasmacPopUp('yesno', _('SET PROBE OFFSETS'), ' \n'.join(offsets_text())).reply
    if not reply:
        comp['offset-set-probe'] = False
        return
    title = _('OFFSET PROBE DELAY')
    prompt = _('Delay (Seconds)')
    while 1:
        valid, delay = plasmacPopUp('entry', title, prompt, True, probeOffsets['Delay']).reply
        if not valid:
            comp['offset-set-probe'] = False
            return
        if not delay:
            delay = 0
        try:
            delay = float(delay)
            break
        except:
            msg = ('Delay entry is invalid')
            plasmacPopUp('info', title, msg)
    newOffsets = {'X':round(s.position[0] - s.g5x_offset[0] - s.g92_offset[0], 4) + 0, \
                  'Y':round(s.position[1] - s.g5x_offset[1] - s.g92_offset[1], 4) + 0, \
                  'Delay':delay}
    if offsets_prompt(_('CHANGE PROBE OFFSETS'), probeOffsets, newOffsets, True):
        probeOffsets['X'] = newOffsets['X']
        probeOffsets['Y'] = newOffsets['Y']
        probeOffsets['Delay'] = newOffsets['Delay']
        putPrefs(PREF, 'OFFSET_PROBING', 'X axis', probeOffsets['X'], float)
        putPrefs(PREF, 'OFFSET_PROBING', 'Y axis', probeOffsets['Y'], float)
        putPrefs(PREF, 'OFFSET_PROBING', 'Delay', probeOffsets['Delay'], float)
        offset_probe_setup()
        comp['offset-set-probe'] = False
        title = _('PROBE OFFSETS')
        msg = _('Probe offsets have been saved')
        plasmacPopUp('info', title, msg)
    comp['offset-set-probe'] = False

def offsets_prompt(title, oldOffsets, newOffsets, probe=False):
    dly = _('Delay')
    prompt  = _('From')
    prompt += f":\nX:{oldOffsets['X']:0.3f}  Y:{oldOffsets['Y']:0.3f}"
    if probe:
        prompt += f"  {dly}:{oldOffsets['Delay']:0.2f}"
    prompt += '\n\n'
    prompt += _('To')
    prompt += f":\nX:{newOffsets['X']:0.3f}  Y:{newOffsets['Y']:0.3f}"
    if probe:
        prompt += f"  {dly}:{newOffsets['Delay']:0.2f}"
    prompt += '\n'
    return plasmacPopUp('yesno', title, prompt, True).reply

def offsets_write_scribe(toolFile, offsets):
    written = False
    COPY(toolFile, f"{toolFile}~")
    with open(f"{toolFile}~", 'r') as inFile:
        with open(toolFile, 'w') as outFile:
            for line in inFile:
                if line.startswith('T1'):
                    outFile.write(f"T1 P1 X{offsets['X']:0.3f} Y{offsets['Y']:0.3f} ;scribe\n")
                    written = True
                else:
                    outFile.write(line)
            if not written:
                outFile.write(f"T1 P1 X{offsets['X']:0.3f} Y{offsets['Y']:0.3f} ;scribe\n")
    os.remove(f"{toolFile}~")


##############################################################################
# HEIGHT OVERRIDE                                                            #
##############################################################################
def height_lower():
    if rE(f"{foverride}.lower cget -state") == 'disabled':
        return
    global torchHeight
    torchHeight -= 0.1
    height_set(f"{torchHeight:.1f}")

def height_raise():
    if rE(f"{foverride}.raise cget -state") == 'disabled':
        return
    global torchHeight
    torchHeight += 0.1
    height_set(f"{torchHeight:.1f}")

def height_reset():
    if rE(f"{foverride}.reset cget -state") == 'disabled':
        return
    global torchHeight
    torchHeight = 0
    height_set(f"{torchHeight:.1f}")

def height_set(height):
    rE(f"{foverride}.height-override configure -text {height}V")
    hal.set_p('plasmac.height-override', height)


##############################################################################
# CUT RECOVERY FUNCTIONS                                                     #
##############################################################################
def cut_rec_default_changed(percent):
    rE(f"{fcrspeed}.display.cut-rec-speed set {percent}")

def cut_rec_slider_changed(percent):
    pVars.crSpeed.set(f"{int(widgetValues[f'{fruns}.material.cut-feed-rate']) * int(percent) * 0.01:0.0f}")

def cut_rec_motion(direction):
    if int(direction):
        speed = float(rE(f"{fcrspeed}.display.cut-rec-speed get")) * 0.01
        hal.set_p('plasmac.paused-motion-speed',f"{speed * int(direction):.2f}")
    else:
        hal.set_p('plasmac.paused-motion-speed','0')

def cut_rec_move(x, y):
    if not s.paused:
        return
    maxMove = 10
    if s.linear_units != 1:
        maxMove = 0.4
    laser = hal.get_value('plasmac.laser-recovery-state') > 0
    distX = float(pVars.kerfWidth.get()) * int(x)
    distY = float(pVars.kerfWidth.get()) * int(y)
    xNew = hal.get_value('plasmac.axis-x-position') + hal.get_value('axis.x.eoffset') - (laserOffsets['X'] * laser) + distX
    yNew = hal.get_value('plasmac.axis-y-position') + hal.get_value('axis.y.eoffset') - (laserOffsets['Y'] * laser) + distY
    if xNew < machineBounds['X-'] or xNew > machineBounds['X+'] or yNew < machineBounds['Y-'] or yNew > machineBounds['Y+']:
        return
    xTotal = hal.get_value('axis.x.eoffset') - (laserOffsets['X'] * laser) + distX
    yTotal = hal.get_value('axis.y.eoffset') - (laserOffsets['Y'] * laser) + distY
    if xTotal > maxMove or xTotal < -maxMove or yTotal > maxMove or yTotal < -maxMove:
        return
    moveX = int(distX / hal.get_value('plasmac.offset-scale'))
    moveY = int(distY / hal.get_value('plasmac.offset-scale'))
    hal.set_p('plasmac.x-offset', f"{str(hal.get_value('plasmac.x-offset') + moveX)}")
    hal.set_p('plasmac.y-offset', f"{str(hal.get_value('plasmac.y-offset') + moveY)}")
    hal.set_p('plasmac.cut-recovery', '1')

def cut_rec_cancel():
    hal.set_p('plasmac.cut-recovery', '0')
    hal.set_p('plasmac.laser-recovery-start', '0')
    hal.set_p('plasmac.x-offset', '0')
    hal.set_p('plasmac.y-offset', '0')
    comp['laser-on'] = 0


##############################################################################
# MONKEYPATCHED FUNCTIONS FROM AXIS.PY                                       #
##############################################################################
def get_view():
    ''' add view t '''
    x,y,z,p = 0,1,2,3
    if str(widgets.view_x['relief']) == 'sunken':
        view = x
    elif (str(widgets.view_y['relief']) == 'sunken' or
         str(widgets.view_y2['relief']) == 'sunken'):
        view = y
    elif (str(widgets.view_z['relief']) == 'sunken' or
          str(widgets.view_z2['relief']) == 'sunken' or
          str(widgets.view_t['relief']) == 'sunken'):
        view = z
    else:
        view = p
    return view

def filter_program_new(program_filter, infilename, outfilename):
    ''' remove temporary materials before calling original filter_program function '''
    mats = [m for m in materialNumList if m >= 1000000]
    if mats:
        for mat in mats:
            materialFileDict.pop(mat)
            materialNumList.remove(mat)
        if get_displayed_material() >= 1000000:
            change_default_material()
        insert_materials()
    exitcode, stderr = filter_program_old(program_filter, infilename, outfilename)
    return exitcode, stderr


##############################################################################
# MONKEYPATCHED FUNCTIONS FROM GLCANON.PY                                    #
##############################################################################
def draw_grid():
    ''' do not return if view = P '''
    x,y,z,p = 0,1,2,3
    view = get_view()
    rotation = math.radians(s.rotation_xy % 90)
    permutations = [
            lambda x_y_z: (x_y_z[2], x_y_z[1], x_y_z[0]),  # YZ X
            lambda x_y_z1: (x_y_z1[2], x_y_z1[0], x_y_z1[1]),  # ZX Y
            lambda x_y_z2: (x_y_z2[0], x_y_z2[1], x_y_z2[2])]  # XY Z
    inverse_permutations = [
            lambda z_y_x: (z_y_x[2], z_y_x[1], z_y_x[0]),  # YZ X
            lambda z_x_y: (z_x_y[1], z_x_y[2], z_x_y[0]),  # ZX Y
            lambda x_y_z3: (x_y_z3[0], x_y_z3[1], x_y_z3[2])]  # XY Z
    o.draw_grid_permuted(rotation, permutations[2],
            inverse_permutations[2])

def posstrs():
    ''' show height above material probed height '''
    s = o.stat
    limit = list(s.limit[:])
    homed = list(s.homed[:])
    spd = o.to_internal_linear_unit(s.current_vel)
    if not o.get_joints_mode() or o.no_joint_display:
        if o.get_show_commanded():
            positions = s.position
        else:
            positions = s.actual_position
        if o.get_show_relative():
            positions = [(i-j) for i, j in zip(positions, s.tool_offset)]
            positions = [(i-j) for i, j in zip(positions, s.g5x_offset)]
            t = -s.rotation_xy
            t = math.radians(t)
            x = positions[0]
            y = positions[1]
            positions[0] = x * math.cos(t) - y * math.sin(t)
            positions[1] = x * math.sin(t) + y * math.cos(t)
            positions = [(i-j) for i, j in zip(positions, s.g92_offset)]
        else:
            positions = list(positions)
        if o.get_a_axis_wrapped():
            positions[3] = math.fmod(positions[3], 360.0)
            if positions[3] < 0: positions[3] += 360.0
        if o.get_b_axis_wrapped():
            positions[4] = math.fmod(positions[4], 360.0)
            if positions[4] < 0: positions[4] += 360.0
        if o.get_c_axis_wrapped():
            positions[5] = math.fmod(positions[5], 360.0)
            if positions[5] < 0: positions[5] += 360.0
        positions = o.to_internal_units(positions)
        axisdtg = o.to_internal_units(s.dtg)
        g5x_offset = o.to_internal_units(s.g5x_offset)
        g92_offset = o.to_internal_units(s.g92_offset)
        tlo_offset = o.to_internal_units(s.tool_offset)
        dtg = o.to_internal_linear_unit(s.distance_to_go)
        if o.get_show_metric():
            positions = o.from_internal_units(positions, 1)
            axisdtg = o.from_internal_units(axisdtg, 1)
            g5x_offset = o.from_internal_units(g5x_offset, 1)
            g92_offset = o.from_internal_units(g92_offset, 1)
            tlo_offset = o.from_internal_units(tlo_offset, 1)
            dtg *= 25.4
            spd = spd * 25.4
        spd = spd * 60
        # show height above material probed height
        if s.linear_units == 1 and not vars.metric.get():
            scale = 1 / 25.4
        elif s.linear_units != 1 and vars.metric.get():
            scale = 25.4
        else:
            scale = 1
        if hal.get_value('plasmac.offsets-active') or isPaused:
            positions[2] = hal.get_value('plasmac.z-height') * scale
        return o.dro_format(o.stat,spd,dtg,limit,homed,positions,
                axisdtg,g5x_offset,g92_offset,tlo_offset)
    else:
        return o.joint_dro_format(s,spd,o.get_num_joints(),limit, homed)


##############################################################################
# TCL CALLED PYTHON FUNCTIONS HIJACKED FROM AXIS.PY                                        #
##############################################################################
def reload_file(refilter=True):
    ''' file to load depends on type of file
        set view to Z
        clear the plot '''
    if running(): return
    s.poll()
    if not loaded_file and not pVars.preRflFile.get():
        root_window.tk.call('set_mode_from_tab')
        return
    line = vars.highlight_line.get()
    o.set_highlight_line(None)
    if pVars.preRflFile.get():
        f = pVars.preRflFile.get()
    elif '/shape.ngc' in loaded_file:
        f = os.path.join(tmpPath, 'shape.ngc')
    else:
        f = loaded_file
    if refilter or not get_filter(loaded_file):
        # we copy the file to a temporary file so that even if it subsequently
        # changes on disk, LinuxCNC is parsing the file contents from the time
        # the user opened the file
        tempfile = os.path.join(tempdir, os.path.basename(loaded_file))
        if loaded_file != tempfile:
            shutil.copyfile(loaded_file, tempfile)
        open_file_guts(tempfile, False, False)
    else:
        tempfile = os.path.join(tempdir, "filtered-" + os.path.basename(loaded_file))
        open_file_guts(tempfile, True, False)
    msg = bounds_check_file()
    msg += bounds_check_probe(False)
    if msg:
        notifications.add('error', msg)
    commands.set_view_z()
    if line:
        o.set_highlight_line(line)
    live_plotter.clear()

def task_run(*event):
    global dryRun
    if event and event[0]== 'dry-run' and loaded_file:
        if laserOffsets['X'] or laserOffsets['Y']:
            framingError = bounds_check_framing(laserOffsets['X'], laserOffsets['Y'], True)[0]
            if framingError:
                title = _('AXIS LIMIT ERROR')
                plasmacPopUp('warn', title, framingError)
                return
            dryRun = [s.g5x_offset[0], s.g5x_offset[1]]
            newX = s.actual_position[0] - s.g5x_offset[0] - laserOffsets['X']
            newY = s.actual_position[1] - s.g5x_offset[1] - laserOffsets['Y']
            units = '21' if vars.metric.get() else '20'
            if s.linear_units == 1 and units == '20':
                unitsMultiplier = 1 / 25.4
            elif s.linear_units != 1 and units == '20':
                unitsMultiplier =  25.4
            else:
                unitsMultiplier = 1
            ensure_mode(linuxcnc.MODE_MDI)
            c.mdi(f'G{units} G10 L20 P0 X{newX * unitsMultiplier} Y{newY * unitsMultiplier}')
            c.wait_complete()
            comp['laser-on'] = 1
            hal.set_p('plasmac.dry-run', '1')
    if run_warn() or not critical_button_check():
        return
    global program_start_line, program_start_line_last
    program_start_line_last = program_start_line;
    ensure_mode(linuxcnc.MODE_AUTO)
    c.auto(linuxcnc.AUTO_RUN, program_start_line)
    program_start_line = 0
    t.tag_remove("ignored", "0.0", "end")
    o.set_highlight_line(None)

def task_run_line():
    ''' get leadin if required
        preprocess gcode up to selected line '''
    if vars.highlight_line.get() == 0:
        return
    pVars.startLine.set(vars.highlight_line.get() - 1)
    if loaded_file != os.path.join(tmpPath, 'rfl.ngc'):
        rflIn = os.path.join(tmpPath, 'filtered_bkp.ngc')
    else:
        rflIn = loaded_file
    if comp['development']:
        reload(RFL)
    # get rfl type
    rfl = {}
    title = _('RUN FROM LINE')
    valid, type_ = plasmacPopUp('rfl_type', title, None, 'numpad').reply
    vkbData['required'] = False
    if not valid:
        pVars.rflActive = False
        pVars.startLine.set(0)
        return
    lastLine = 0
    # get start and end of cutpath
    if type_ == 'cut':
        count = 0
        start = 0
        with open(rflIn, 'r') as inFile:
            for line in inFile:
                line = line.strip()
                if line[:3] == 'G00':
                    start = count
                if line[:3] == 'M05' and count > pVars.startLine.get():
                    break
                count += 1
        lastLine = count
        pVars.startLine.set(start)
        rfl['do'] = False
        rfl['length'] = 0.0
        rfl['angle'] = 0.0
    # get leadin parameters
    else:
        valid, rfl['do'], rfl['length'], rfl['angle'] = plasmacPopUp('rfl', title, None, 'numpad').reply
        vkbData['required'] = False
    # cancel rfl
    if not valid:
        pVars.rflActive = False
        pVars.startLine.set(0)
        return
    # get the old data
    setup = RFL.run_from_line_get(rflIn, pVars.startLine.get(), lastLine)
    if setup['error']:
    # cannot do run from line within a subroutine or if using cutter compensation
        if setup['compError']:
            msg0 = _('Cannot run from line while cutter compensation is active')
            notifications.add('error', f"{msg0}\n")
        if setup['subError']:
            msg0 = _('Cannot do run from line inside a subroutine')
            msg1 = ''
            for sub in setup['subError']:
                msg1 += f" {sub}"
                notifications.add('error', f"{msg0} {msg1}\n")
            pVars.rflActive = False
            pVars.startLine.set(0)
    else:
        # set the new data
        rflFile = os.path.join(tmpPath, 'rfl.ngc')
        result = RFL.run_from_line_set(rflFile, setup, rfl, unitsPerMm)
        # if leadin cannot be used
        if result['error']:
            msg0 = _('Unable to calculate a leadin for this cut')
            msg1 = _('Program will run from selected line with no leadin applied')
            notifications.add('error', f"{msg0}\n{msg1}\n")
        # load rfl file
        if loaded_file != os.path.join(tmpPath, 'rfl.ngc'):
            pVars.preRflFile.set(loaded_file)
        open_file_guts(os.path.join(tmpPath, 'rfl.ngc'), False, False)
        commands.set_view_z()

def open_file_name(f):    # from axis.py
    ''' set view to Z '''
    global loaded_file
    loaded_file = f
    open_file_guts(f)
    commands.set_view_z()
    if o.canon is not None:
        x = (o.canon.min_extents[0] + o.canon.max_extents[0])/2
        y = (o.canon.min_extents[1] + o.canon.max_extents[1])/2
        z = (o.canon.min_extents[2] + o.canon.max_extents[2])/2
        o.set_centerpoint(x, y, z)
    live_plotter.clear()
    msg = bounds_check_file()
    msg += bounds_check_probe(False)
    if msg:
        notifications.add('error', msg)

def set_view_p(event=None):    # from axis.py
    ''' add view t '''
    widgets.view_z.configure(relief='link')
    widgets.view_z2.configure(relief='link')
    widgets.view_x.configure(relief='link')
    widgets.view_y.configure(relief='link')
    widgets.view_y2.configure(relief='link')
    widgets.view_t.configure(relief='link')
    widgets.view_p.configure(relief='sunken')
    vars.view_type.set(5)
    o.set_view_p()

def set_view_z(event=None):    # from axis.py
    ''' add view t '''
    widgets.view_z.configure(relief='sunken')
    widgets.view_z2.configure(relief='link')
    widgets.view_x.configure(relief='link')
    widgets.view_y.configure(relief='link')
    widgets.view_y2.configure(relief='link')
    widgets.view_t.configure(relief='link')
    widgets.view_p.configure(relief='link')
    vars.view_type.set(1)
    o.set_view_z()

def get_jog_speed(a):
    ''' convert speeds to suit gcode units'''
    if vars.teleop_mode.get():
        if axis_type[a] == "LINEAR" :
            return vars.jog_speed.get() / 60 * pVars.jogMultiplier.get()
        else:
            return vars.jog_aspeed.get() / 60 * pVars.jogMultiplier.get()
    else:
        if joint_type[a] == 'LINEAR':
            return vars.jog_speed.get() / 60 * pVars.jogMultiplier.get()
        else:
            return vars.jog_aspeed.get() / 60 * pVars.jogMultiplier.get()

def send_mdi_command(command):
    ''' reload file if G10 in gcode '''
    ''' disallow M3, M4, M5, and G92 in MDI '''
    global mdi_history_index, mdi_history_save_filename
    if command != "":
        command= command.lstrip().rstrip()
        test = command.lower().replace(' ','')
        if 'g92' in test and not 'g92.1' in test:
            msg0 = _('G92 offsets are not allowed')
            notifications.add('error', f"{msg0}\n")
            return
        if 'm3' in test:
            msg0 = _('M3 commands are not allowed in MDI mode')
            notifications.add('error', f"{msg0}\n")
            return
        elif 'm4' in test:
            msg0 = _('M4 commands are not allowed in MDI mode')
            notifications.add('error', f"{msg0}\n")
            return
        elif 'm5' in test:
            msg0 = _('M5 commands are not allowed in MDI mode')
            notifications.add('error', f"{msg0}\n")
            return
        vars.mdi_command.set("")
        ensure_mode(linuxcnc.MODE_MDI)
        widgets.mdi_history.selection_clear(0, "end")
        ## check if input is already in list. If so, then delete old element
        #idx = 0
        #for ele in widgets.mdi_history.get(0, "end"):
        #    if ele == command:
        #        widgets.mdi_history.delete(idx)
        #        break
        #    idx += 1
        history_size = widgets.mdi_history.size()
        new_entry = 1
        if history_size > 1 and widgets.mdi_history.get(history_size - 2) == command:
            new_entry = 0
        if new_entry != 0:
            # if command is already at end of list, don't add it again
            widgets.mdi_history.insert(history_size - 1, f"{command}")
            history_size += 1
        widgets.mdi_history.see(history_size - 1)
        if history_size > (mdi_history_max_entries + 1):
            widgets.mdi_history.delete(0, 0)
            history_size= (mdi_history_max_entries + 1)
        # pdb.set_trace()
        mdi_history_index = widgets.mdi_history.index("end") - 1
        c.mdi(command)
        o.tkRedraw()
        commands.mdi_history_write_to_file(mdi_history_save_filename, history_size)
        if 'G10' in command.upper().replace(' ',''):
            reload_file()


##############################################################################
# TCL FUNCTIONS HIJACKED FROM AXIS.TCL                                       #
##############################################################################
def update_title(*args):
    ''' add plasmac to the title '''
    if vars.taskfile.get() == '':
        file = 'no file loaded'
        name = 'AXIS'
    else:
        file = name = os.path.basename(vars.taskfile.get())
    base = f"{vars.machine.get()}    plasmac2 v{VER} + AXIS {linuxcnc.version}"
    rE(f"wm title {root_window} {{{base}    ({file})}}")
    rE(f"wm iconname {root_window} {{{name}}}")

def update_jog_slider_vel(value):
    ''' inhibit slider update if manual cut is active
        use a linear response for jog slider '''
    if not manualCut['state']:
        rE(f"set jog_speed {int(float(value) * vars.max_speed.get() * 60)}")

def set_pane_minsize():
    '''we set our own min sizes dependent on font size '''
    pass


##############################################################################
# PYTHON FUNCTIONS HIJACKED FROM AXIS.PY                                     #
##############################################################################
def vupdate(var, val):
    ''' pretend no file is loaded if program is cleared '''
    if var == vars.taskfile and 'clear.ngc' in val:
        val = ''
    try:
        if var.get() == val: return
    except ValueError:
        pass
    var.set(val)

def get_coordinate_font(large):
    ''' remove selectable large font'''
    global coordinate_font
    global coordinate_linespace
    global coordinate_charwidth
    global fontbase
    coordinate_font = f"monospace {pVars.fontSize.get()}"
    if coordinate_font not in font_cache:
        font_cache[coordinate_font] = \
            glnav.use_pango_font(coordinate_font, 0, 128)
    fontbase, coordinate_charwidth, coordinate_linespace = \
            font_cache[coordinate_font]

def install_help(app):
    ''' we use install_kb_text and install_kp_text instead
        so we don't need to do anything here '''
    return

def prompt_touchoff(title, text, default, tool_only, system=None):
    ''' use our own prompt
        allow use of virtual keyboard'''
    title = _('TOUCH OFF')
    text = text.replace(':', '') % _('workpiece')
    valid, value, system = plasmacPopUp('touch', title, text, 'numpad', system).reply
    vkbData['required'] = False
    try:
        v = float(value)
    except:
        msg0 = _('Touch off entry is invalid')
        plasmacPopUp('error', title, f"{msg0[:16]}{value} {msg0[16:]}")
        value = 0.0
    if valid:
        return(value, system)
    else:
        return(None, None)


##############################################################################
# USER BUTTON FUNCTIONS                                                      #
##############################################################################
def critical_button_check():
    valid = True
    if hal.get_value('plasmac.torch-enable'):
        buttons = ''
        btnText = _('button is')
        for button in criticalButtons:
            if togglePins[str(button)]['runcritical'] and not togglePins[str(button)]['state']:
                if buttons:
                    buttons += ', '
                    btnText = _('buttons are')
                buttons += rE(f"{fbuttons}.button{str(button)} cget -text")
                valid = False
        if not valid:
            msg0 = _('not activated')
            msg1 = _('Continue the cut?')
            valid = plasmacPopUp('yesno', _('CRITICAL BUTTON'), f'{buttons} {btnText} {msg0}\n\n{msg1}').reply
    return valid

def validate_hal_pin(halpin, button, usage):
    valid = pBit = False
    for pin in halPinList:
        if halpin in pin['NAME']:
            pBit = isinstance(pin['VALUE'], bool)
            valid = True
            break
    if not valid:
        msg0 = _('does not exist for user button')
        notifications.add('error', f"{halpin} {msg0} #{button}")
    elif not pBit:
        msg0 = _('must be a bit pin for user button')
        notifications.add('error', f"{usage} {msg0} #{button}")
        valid = False
    return valid

def get_parameter_value(code, button):
    param, start, mid, end = '', '', '', ''
    error = False
    while '{' in code or '#<_ini[' in code:
        if '{' in code:
            if not '}' in code:
                msg0 = _('is missing for user button')
                notifications.add('error', f"'}}' {msg0} #{button}")
                code = ''
                error = True
                break
            param = code.split('{', 1)[1].split('}', 1)[0]
            start = '{'
            mid = ' '
            end = '}'
        else:
            if not '>' in code:
                msg0 = _('is missing for user button')
                notifications.add('error', f"'>' {msg0} #{button}")
                code = ''
                error = True
                break
            param = code.split('#<_ini[', 1)[1].split('>', 1)[0].replace(']', ' ')
            start = '#<_ini['
            mid = ']'
            end = '>'
        if param:
            section, option = param.split(' ', 1)
            if PREF.has_option(section, option):
                value = getPrefs(PREF, section.upper(), option, '', str)
            elif inifile.find(section.upper(), option.upper()):
                value = inifile.find(section.upper(), option.upper())
            else:
                msg0 = _('is invalid parameter for user button')
                notifications.add('error', f"'{section} {option}' {msg0} #{button}")
                code = ''
                error = True
                break
            code = code.replace(f"{start}{section}{mid}{option}{end}", value)
            param, start, mid, end = '', '', '', ''
    return(error, code)

def button_action(button, pressed):
    if int(pressed):
        user_button_pressed(button, buttonCodes[int(button)])
    else:
        user_button_released(button, buttonCodes[int(button)])

def user_button_setup():
    global buttonNames, buttonCodes, togglePins, pulsePins, machineBounds, criticalButtons
    global probeButton, probeText, torchButton, torchText, cChangeButton, invalidButtons
    singleCodes = ['ohmic-test','cut-type','single-cut','manual-cut','probe-test', \
                   'torch-pulse','change-consumables','framing','latest-file']
    buttonNames = {0:{'name':None}}
    buttonCodes = {0:{'code':None}}
    invalidButtons = []
    criticalButtons = []
    row = 1
    for n in range(1, maxUserButtons + 1):
        bLabel = None
        bName = getPrefs(PREF, 'BUTTONS', f"{n} Name", '', str)
        bCode = getPrefs(PREF, 'BUTTONS', f"{n} Code", '', str)
        outCode = {'code':None}
        parmError = False
        if '{' in bCode or '#<_ini[' in bCode:
            parmError, bCode = get_parameter_value(bCode, n)
        if bCode.strip() == 'ohmic-test' and not 'ohmic-test' in [(v['code']) for k, v in buttonCodes.items()]:
            outCode['code'] = 'ohmic-test'
        elif bCode.strip() == 'cut-type' and not 'cut-type' in buttonCodes:
            bName = bName.split(',')
            if len(bName) == 1:
                text = _('Pierce\Only') if '\\' in bName[0] else _('Pierce Only')
                bName.append(text)
            outCode = {'code':'cut-type', 'text':bName}
        elif bCode.strip() == 'single-cut' and not 'single-cut' in buttonCodes:
            outCode['code'] = 'single-cut'
        elif bCode.strip() == 'manual-cut' and not 'manual-cut' in buttonCodes:
            outCode['code'] = 'manual-cut'
        elif bCode.startswith('probe-test') and not 'probe-test' in [(v['code']) for k, v in buttonCodes.items()]:
            codes = bCode.lower().strip().replace(' ','').replace('probe-test','probe-test ').split()
            if len(codes) < 3:
                outCode = {'code':'probe-test', 'time':10}
                probeButton = str(n)
                probeText = bName.replace('\\', '\n')
                if len(codes) == 2:
                    try:
                        value = int(float(codes[1]))
                        outCode['time'] = value
                    except:
                        outCode['code'] = None
        elif bCode.startswith('torch-pulse') and not 'torch-pulse' in [(v['code']) for k, v in buttonCodes.items()]:
            codes = bCode.lower().strip().replace(' ','').replace('torch-pulse','torch-pulse ').split()
            if len(codes) < 3:
                outCode = {'code':'torch-pulse', 'time':1.0}
                torchButton = str(n)
                torchText = bName.replace('\\', '\n')
                if len(codes) == 2:
                    try:
                        value = round(float(codes[1]), 1)
                        outCode['time'] = value
                    except:
                        outCode['code'] = None
        elif bCode.startswith('change-consumables') and not 'change-consumables' in [(v['code']) for k, v in buttonCodes.items()]:
            codes = bCode.lower().strip().replace('x',' x').replace('y',' y').replace('f',' f').split()
            if len(codes) > 1 and len(codes) < 5:
                outCode = {'code':'change-consumables', 'X':None, 'Y':None, 'F':None}
                for l in 'xyf':
                    for c in range(1, len(codes)):
                        if codes[c].startswith(l):
                            try:
                                value = round(float(codes[c].replace(l,'')), 3)
                                outCode['XYF'['xyf'.index(l)]] = value
                            except:
                                outCode['code'] = None
                if outCode['X'] is None and outCode['Y'] is None:
                    outCode['code'] = None
                else:
                    buff = 10 * unitsPerMm # keep 10mm away from machine limits
                    for axis in 'XY':
                        if outCode[axis]:
                            if outCode[f"{axis}"] < machineBounds[f"{axis}-"] + buff:
                                outCode[f"{axis}"] = machineBounds[f"{axis}-"] + buff
                            elif outCode[f"{axis}"] > machineBounds[f"{axis}+"] - buff:
                                outCode[f"{axis}"] = machineBounds[f"{axis}+"] - buff
            if outCode['code']:
                cChangeButton = str(n)
        elif bCode.startswith('framing') and not 'framing' in [(v['code']) for k, v in buttonCodes.items()]:
            codes = bCode.lower().strip().replace(' ','').replace('fra','ra').replace('f',' f') \
                         .replace('us',' us').replace('ra','fra').split()
            if codes[0] == 'framing' and len(codes) < 4:
                outCode = {'code':'framing', 'F':False, 'Z':False}
                for c in range(1, len(codes)):
                    if codes[c].startswith('f'):
                        try:
                            value = round(float(codes[c].replace('f','')), 3)
                            outCode['F'] = value
                        except:
                            outCode['code'] = None
                    elif codes[c] == 'usecurrentzheight':
                        outCode['Z'] = True
                    else:
                        outCode['code'] = None
        elif bCode.startswith('load'):
            codes = bCode.strip().replace(' ','').replace('load','load ').split()
            if len(codes) > 1 and len(codes) < 3:
                if os.path.isfile(os.path.join(open_directory, codes[1])):
                    outCode = {'code':'load', 'file':os.path.join(open_directory, codes[1])}
        elif bCode.startswith('latest-file') and not 'latest-file' in [(v['code']) for k, v in buttonCodes.items()]:
            codes = bCode.strip().replace(' ','').replace('latest-file','latest-file ').split()
            if len(codes) < 3:
                outCode = {'code':'latest-file', 'dir':None}
                if len(codes) == 1:
                    outCode['dir'] = open_directory
                elif len(codes) == 2 and os.path.isdir(codes[1]):
                    outCode['dir'] = codes[1]
                else:
                    outCode['code'] = None
        elif bCode.startswith('pulse-halpin '):
            if len(bCode.split()) > 1 and len(bCode.split()) < 4:
                codes = bCode.strip().split()
                if validate_hal_pin(codes[1], n, 'pulse-halpin'):
                    try:
                        value = round(float(codes[2]), 1)
                        outCode = {'code':'pulse-halpin', 'pin':codes[1], 'time':value}
                        pulsePins[str(n)] = {'pin':codes[1], 'text':None, 'pulse':0, 'state':False}
                    except:
                        outCode = {'code':None}
                else:
                    parmError = True
        elif bCode.startswith('toggle-halpin '):
            if len(bCode.split()) > 1:
                ontext = bName
                if ';;' in bCode:
                    ontext = bCode.split(';;')[1] if ';;' in bCode else ''
                    bCode = bCode[:bCode.index(';;')].strip()
                codes = bCode.strip().split()
                if validate_hal_pin(codes[1], n, 'toggle-halpin'):
                    outCode = {'code':'toggle-halpin', 'pin':codes[1], 'critical':False, 'ontext':ontext}
                    outCode['pin'] = codes[1]
                    if len(codes) == 3 and (codes[2] == 'runcritical' or codes[2] == 'cutcritical'):
                        outCode['critical'] = True
                        criticalButtons.append(n)
                    togglePins[str(n)] = {'pin':outCode['pin'], 'state':hal.get_value(outCode['pin']), \
                                          'runcritical':outCode['critical'], 'ontext':outCode['ontext'], \
                                          'offtext':bName}
                else:
                    parmError = True
        elif bCode and bCode not in singleCodes:
            if 'dual-code' in bCode:
                # incoming code is: "dual-code" ;; code1 ;; label1 ;; code2 ;; checked (optional = true)
                data = bCode.split(';;')
                if len(data) not in [4, 5]:
                    outCode = {'code':None}
                    continue
                else:
                    if len(data) == 5 and data[4].strip().lower() == 'true':
                        checked = True
                    else:
                        checked = False
                    dualCodes[str(n)] = {'ontext':data[2].strip(), 'offtext':bName.strip(), 'checked':checked}
                codes = [code for code in data[1].split('\\') + ['dual-code'] + data[3].split('\\')]
            else:
                codes = bCode.strip().split('\\')
            codes = [x.strip() for x in codes]
            outCode['code'] = []
            for cn in range(len(codes)):
                if codes[cn][0] == '%':
                    if codes[cn][-3:] == '.py':
                        if '/' not in codes[cn]:
                            codes[cn] = f"%./{codes[cn].replace('%','')}"
                        if os.path.exists(os.path.expanduser(codes[cn][1:])):
                            outCode['code'].append(['python3', os.path.expanduser(codes[cn][1:])])
                        else:
                            outCode = {'code': None}
                    elif WHICH(codes[cn].split()[0][1:]) is not None:
                        outCode['code'].append(['shell', codes[cn][1:]])
                    else:
                        outCode = {'code': None}
                elif codes[cn][:2].lower() == 'o<':
                    outCode['code'].append(['ocode', codes[cn]])
                elif codes[cn][0].lower() in 'gm':
                    outCode['code'].append(['gcode', codes[cn]])
                elif codes[cn] == 'dual-code':
                    outCode['code'].append(codes[cn])
                else:
                    outCode = {'code': None}
                    break
            if 'dual-code' in bCode:
                dualCodes[str(n)]['oncode'] = outCode['code'][:outCode['code'].index('dual-code')]
                dualCodes[str(n)]['offcode'] = outCode['code'][outCode['code'].index('dual-code')+1:]
                outCode['code'] = 'dual-code'
        else:
            outCode = {'code':None}
        if rE(f"winfo exists {fbuttons}.button{n}") == '0':
             rE(f"button {fbuttons}.button{n} -takefocus 0 -width 10")
        if bName and outCode['code']:
            bHeight = 1
            if type(bName) == list:
                bHeight = max(len(bName[0].split('\\')), len(bName[1].split('\\')))
                bName = bName[0]
            else:
                bHeight = len(bName.split('\\'))
            bLabel = bName.replace('\\', '\n')
            rE(f"{fbuttons}.button{n} configure -text {{{bLabel}}} -height {bHeight} -bg {colorBack}")
            rE(f"bind {fbuttons}.button{n} <ButtonPress-1> {{button_action {n} 1}}")
            rE(f"bind {fbuttons}.button{n} <ButtonRelease-1> {{button_action {n} 0}}")
            row += 1
        elif bName or bCode:
            if not parmError:
                msg0 = _('is invalid code for user button')
                notifications.add('error', f"'{bCode}' {msg0} #{n}")
            bName = None
            outCode = {'code':None}
            invalidButtons.append(n)
        buttonNames[n] = {'name':bName}
        buttonCodes[n] = outCode
    user_button_load()
    populate_user_buttons()

def user_button_pressed(button, code):
    global colorBack, activeFunction
    global probePressed, probeTimer, probeButton
    global torchPressed, torchTimer, torchButton
    if rE(f"{fbuttons}.button{button} cget -state") == 'disabled' or not code:
        return
    if code['code'] == 'ohmic-test':
        hal.set_p('plasmac.ohmic-test','1')
    # print window sizes in development mode
    if comp['development']:
        print(f"Width = {rE('winfo width .')}   Height={rE('winfo height .')}")
    elif code['code'] == 'cut-type':
        pass # actioned from button_release
    elif code['code'] == 'single-cut':
        pass # actioned from button_release
    elif code['code'] == 'manual-cut':
        manual_cut(None)
    elif code['code'] == 'probe-test' and not hal.get_value('halui.program.is-running'):
        if probeTimer:
            probeTimer = 0
            probe_test_timer()
        elif not hal.get_value('plasmac.z-offset-counts'):
            msg = bounds_check_probe(True)
            if msg:
                notifications.add('error', msg)
                return
            activeFunction = True
            probePressed = True
            probeTimer = code['time'] * 1000
            hal.set_p('plasmac.probe-test','1')
            rE(f"{fbuttons}.button{probeButton} configure -text {str(int(probeTimer))}")
            rE(f"{fbuttons}.button{probeButton} configure -bg {colorActive}")
            root_window.after(100, probe_test_timer)
    elif code['code'] == 'torch-pulse':
        if torchTimer:
            torchTimer = 0
            torch_pulse_timer()
        elif not hal.get_value('plasmac.z-offset-counts'):
            torchPressed = True
            torchTimer = code['time'] * 1000
            hal.set_p('plasmac.torch-pulse-time',f"{torchTimer}")
            hal.set_p('plasmac.torch-pulse-start','1')
            rE(f"{fbuttons}.button{torchButton} configure -text {str(int(torchTimer))}")
            rE(f"{fbuttons}.button{torchButton} configure -bg {colorActive}")
            root_window.after(100, torch_pulse_timer)
    elif code['code'] == 'change-consumables' and not hal.get_value('plasmac.breakaway'):
        if hal.get_value('axis.x.eoffset-counts') or hal.get_value('axis.y.eoffset-counts'):
            hal.set_p('plasmac.consumable-change', '0')
            hal.set_p('plasmac.x-offset', '0')
            hal.set_p('plasmac.y-offset', '0')
            rE(f"{fbuttons}.button{button} configure -bg {colorBack}")
            activeFunction = False
        else:
            activeFunction = True
            xPos = s.position[0] if code['X'] is None else code['X']
            yPos = s.position[1] if code['Y'] is None else code['Y']
            if not code['F']:
                code['F'] = rE(f"{fruns}.material.cut-feed-rate get")
            hal.set_p('plasmac.xy-feed-rate', str(code['F']))
            hal.set_p('plasmac.x-offset', f"{(xPos - s.position[0]) / hal.get_value('plasmac.offset-scale'):.0f}")
            hal.set_p('plasmac.y-offset', f"{(yPos - s.position[1]) / hal.get_value('plasmac.offset-scale'):.0f}")
            hal.set_p('plasmac.consumable-change', '1')
            rE(f"{fbuttons}.button{button} configure -bg {colorActive}")
    elif code['code'] == 'framing':
        pass # actioned from button_release
    elif code['code'] == 'load':
        pass # actioned from button_release
    elif code['code'] == 'latest-file':
        pass # actioned from button_release
#FIXME - pin not turning off when button pressed and pin is on
    elif code['code'] == 'pulse-halpin' and hal.get_value('halui.machine.is-on'):
        if code['pin'].startswith('axisui.ext.out_'):
            comp[code['pin'].replace('axisui.','')] = not hal.get_value(code['pin'])
        else:
            hal.set_p(code['pin'], str(not hal.get_value(code['pin'])))
        if pulsePins[button]['pulse']:
            pulsePins[button]['pulse'] = 0
            rE(f"{fbuttons}.button{button} configure -text {{{pulsePins[button]['text']}}}")
        else:
            pulsePins[button]['pulse'] = code['time'] * 1000
            pulsePins[button]['text'] = rE(f"{fbuttons}.button{button} cget -text")
            root_window.after(100, pulse_halpin_off, button)
    elif code['code'] == 'toggle-halpin' and hal.get_value('halui.machine.is-on'):
        if code['pin'].startswith('axisui.ext.out_'):
            comp[code['pin'].replace('axisui.','')] = not hal.get_value(code['pin'])
        else:
            hal.set_p(code['pin'], str(not hal.get_value(code['pin'])))
    else:
        if button in dualCodes:
            if rE(f"{fbuttons}.button{button} cget -text") == dualCodes[button]['offtext']:
                rE(f"{fbuttons}.button{button} configure -text {{{dualCodes[button]['ontext']}}}")
                code['code'] = dualCodes[button]['oncode']
                if dualCodes[button]['checked']:
                    rE(f"{fbuttons}.button{button} configure -bg {colorActive}")
            else:
                rE(f"{fbuttons}.button{button} configure -text {{{dualCodes[button]['offtext']}}}")
                code['code'] = dualCodes[button]['offcode']
                rE(f"{fbuttons}.button{button} configure -bg {colorBack}")
        for n in range(len(code['code'])):
            if code['code'][n][0] == 'python3':
                cmd = f"python3 {code['code'][n][1]}"
                Popen(cmd, stdin = None, stdout=PIPE, stderr=PIPE, shell=True)
            elif code['code'][n][0] == 'shell':
                Popen(code['code'][n][1], stdout=PIPE, stderr=PIPE, shell=True)
            elif code['code'][n][0] in ['gcode', 'ocode']:
                if manual_ok():
                    ensure_mode(linuxcnc.MODE_MDI)
                    c.mdi(code['code'][n][1])
                    update_preview()

def user_button_released(button, code):
    global cutType, probePressed, torchPressed
    if rE(f"{fbuttons}.button{button} cget -state") == 'disabled' or not code: return
    if code['code'] == 'ohmic-test':
        hal.set_p('plasmac.ohmic-test','0')
    elif code['code'] == 'cut-type':
        if not hal.get_value('halui.program.is-running'):
            cutType ^= 1
            if cutType:
                comp['cut-type'] = 1
                text = code['text'][1].replace('\\', '\n')
                color = colorActive
            else:
                comp['cut-type'] = 0
                text = code['text'][0].replace('\\', '\n')
                color = colorBack
            rE(f"{fbuttons}.button{button} configure -bg {color} -text {{{text}}}")
            reload_file()
    elif code['code'] == 'single-cut':
        single_cut()
    elif code['code'] == 'manual-cut':
        pass
    elif code['code'] == 'probe-test':
        probePressed = False
    elif code['code'] == 'torch-pulse':
        torchPressed = False
    elif code['code'] == 'change-consumables':
        pass
    elif code['code'] == 'framing':
        if not code['F']:
            code['F'] = int(rE(f"{fruns}.material.cut-feed-rate get"))
        frame_job(code['F'], code['Z'])
    elif code['code'] == 'load':
        commands.open_file_name(code['file'])
    elif code['code'] == 'latest-file':
        files = GLOB(f"{code['dir']}/*.ngc")
        latest = max(files, key=os.path.getctime)
        commands.open_file_name(latest)
    elif code['code'] == 'pulse-halpin':
        pass
    elif code['code'] == 'toggle-halpin':
        pass
    else:
        pass

def user_button_load():
    rE(f"{fsetup}.r.torch.enabled delete 0 end")
    rE(f"{fsetup}.r.torch.disabled delete 0 end")
    rE(f"{fsetup}.r.torch.enabled insert end {{{getPrefs(PREF, 'BUTTONS', 'Torch enabled', 'Torch Enabled', str)}}}")
    rE(f"{fsetup}.r.torch.disabled insert end {{{getPrefs(PREF, 'BUTTONS','Torch disabled', 'Torch Disabled', str)}}}")
    for n in range(1, maxUserButtons + 1):
        rE(f"{fsetup}.r.ubuttons.canvas.frame.name{n} delete 0 end")
        rE(f"{fsetup}.r.ubuttons.canvas.frame.code{n} delete 0 end")
        if getPrefs(PREF, 'BUTTONS', f"{n} Name", '', str) or getPrefs(PREF, 'BUTTONS', f"{n} Code", '', str):
            rE(f"{fsetup}.r.ubuttons.canvas.frame.name{n} insert end {{{getPrefs(PREF, 'BUTTONS', f'{n} Name', '', str)}}}")
            rE(f"{fsetup}.r.ubuttons.canvas.frame.code{n} insert end {{{getPrefs(PREF, 'BUTTONS', f'{n} Code', '', str)}}}")
    color_user_buttons()

def user_button_save():
    global torchEnable
    putPrefs(PREF, 'BUTTONS', 'Torch enabled', rE(f"{fsetup}.r.torch.enabled get"), str)
    putPrefs(PREF, 'BUTTONS', 'Torch disabled', rE(f"{fsetup}.r.torch.disabled get"), str)
    torchEnable['enabled'] = getPrefs(PREF, 'BUTTONS', 'Torch enabled', 'Torch Enabled', str)
    torchEnable['disabled'] = getPrefs(PREF, 'BUTTONS','Torch disabled', 'Torch Disabled', str)
    if '\\' in torchEnable['enabled'] or '\\' in torchEnable['disabled']:
        rE(f"{fbuttons}.torch-enable configure -height 2")
    else:
        rE(f"{fbuttons}.torch-enable configure -height 1")
    color_torch()
    for n in range(1, maxUserButtons + 1):
        putPrefs(PREF, 'BUTTONS', f"{n} Name", rE(f"{fsetup}.r.ubuttons.canvas.frame.name{n} get"), str)
        putPrefs(PREF, 'BUTTONS', f"{n} Code", rE(f"{fsetup}.r.ubuttons.canvas.frame.code{n} get"), str)
        rE(f"{fsetup}.r.ubuttons.canvas.frame.name{n} delete 0 end")
        rE(f"{fsetup}.r.ubuttons.canvas.frame.code{n} delete 0 end")
    user_button_setup()

def probe_test_timer():
    global probeTimer, activeFunction
    if probeTimer > 0:
        probeTimer -= 100
        if hal.get_value('plasmac.probe-test-error') and not probePressed:
            probeTimer = 0
        if probeTimer > 0:
            rE(f"{fbuttons}.button{probeButton} configure -text {{{round(probeTimer / 1000, 1)}}}")
            root_window.after(100, probe_test_timer)
    if probeTimer <= 0:
        if probePressed:
            root_window.after(100, probe_test_timer)
        else:
            hal.set_p('plasmac.probe-test','0')
            rE(f"{fbuttons}.button{probeButton} configure -text {{{probeText}}}")
            rE(f"{fbuttons}.button{probeButton} configure -bg {colorBack}")
            probeTimer = 0
            activeFunction = False

def torch_pulse_timer():
    global torchTimer
    if torchTimer > 0:
        torchTimer -= 100
        if torchTimer > 0:
            rE(f"{fbuttons}.button{torchButton} configure -text {{{round(torchTimer / 1000, 1)}}}")
            root_window.after(100, torch_pulse_timer)
    if torchTimer <= 0:
        if torchPressed:
            root_window.after(100, torch_pulse_timer)
        else:
            hal.set_p('plasmac.torch-pulse-start','0')
            hal.set_p('plasmac.torch-pulse-time', '0')
            rE(f"{fbuttons}.button{torchButton} configure -text {{{torchText}}}")
            rE(f"{fbuttons}.button{torchButton} configure -bg {colorBack}")
            torchTimer = 0

def pulse_halpin_off(button):
    if pulsePins[button]['pulse'] > 0:
        pulsePins[button]['pulse'] -= 100
        if pulsePins[button]['pulse'] > 0:
            rE(f"{fbuttons}.button{button} configure -text {round(pulsePins[button]['pulse'] / 1000, 1)}")
            root_window.after(100, pulse_halpin_off, button)
    if pulsePins[button]['pulse'] <= 0:
        rE(f"{fbuttons}.button{button} configure -text {{{pulsePins[button]['text']}}}")
        if pulsePins[button]['pin'].startswith('axisui.ext.out_'):
            comp[pulsePins[button]['pin'].replace('axisui.','')] = not hal.get_value(pulsePins[button]['pin'])
        else:
            hal.set_p(pulsePins[button]['pin'], str(not hal.get_value(pulsePins[button]['pin'])))
        pulsePins[button]['pulse'] = 0


##############################################################################
# MATERIAL HANDLING FUNCTIONS                                                #
##############################################################################
def get_displayed_material():
    try:
        return int(rE(f"{fruns}.material.materials get").split(":")[0])
    except:
        return -1

def save_material_clicked():
    matNum = get_displayed_material()
    save_one_material(matNum)
    load_materials(matNum, True)

def reload_material_clicked():
    MATS.clear()
    MATS.read(MATS.fn)
    load_materials(get_displayed_material(), True)

def new_material_clicked():
    global materialNumList
    for n in range(1, 1000000):
        if n not in materialNumList:
            new = n
            break
    title = _('ADD MATERIAL')
    msg1 = _('Enter New Material Number')
    msgs = msg1
    while(1):
        valid, num = plasmacPopUp('entry', title, f"{msgs}:", 'numpad', new).reply
        if not valid:
            return
        if not num:
            msg0 = _('A material number is required')
            msgs = f"{msg0}.\n\n{msg1}:"
            continue
        try:
            num = int(num)
        except:
            msg0 = _('is not a valid number')
            msgs = f"{num} {msg0}.\n\n{msg1}:"
            continue
        if num == 0 or num in materialNumList:
            msg0 = _('Material')
            msg2 = _('is in use')
            msgs = f"{msg0} #{num} {msg2}.\n\n{msg1}:"
        elif num >= 1000000:
            msg0 = _('Material numbers need to be less than 1000000')
            msgs = f"{msg0}.\n\n{msg1}:"
        else:
            break
    msg1 = _('Enter New Material Name')
    msgs = msg1
    while(1):
        valid, nam = plasmacPopUp('entry', title, f"{msgs}:", 'keyboard').reply
        if not valid:
            return
        if not nam:
            msg0 = _('Material name is required')
            msgs = f"{msg0}.\n\n{msg1}:"
        else:
            break
    putPrefs(MATS,f"MATERIAL_NUMBER_{num}",'NAME', nam, str)
    save_one_material(num)
    sortPrefs(MATS)
    load_materials(num, True)

def delete_material_clicked():
    title = _('DELETE MATERIAL')
    msg0 = _('Default material cannot be deleted')
    material = int(rE(f"{fruns}.material.materials get").split(':')[0])
    if material == getPrefs(PREF, 'GUI_OPTIONS', 'Default material', 0, int):
        reply = plasmacPopUp('warn', title, msg0).reply
        return
    msg0 = _('Do you wish to delete material')
    reply = plasmacPopUp('yesno', title, f"{msg0} #{material}?").reply
    if not reply:
        return
    removePrefsSect(MATS,f"MATERIAL_NUMBER_{get_displayed_material()}")
    load_materials(-1, True)

def material_changed():
    global getMaterialBusy, materialAutoChange
    if rE(f"{fruns}.material.materials get"):
        if getMaterialBusy:
            comp['material-change'] = 0
            materialAutoChange = False
            return
        matnum = int(rE(f"{fruns}.material.materials get").split(': ', 1)[0])
        if matnum >= 1000000:
            rE(f"{toolmat}.save configure -state disabled")
        else:
            rE(f"{toolmat}.save configure -state normal")
        if materialAutoChange:
            hal.set_p('motion.digital-in-03','0')
            display_selected_material(matnum)
            comp['material-change'] = 2
            hal.set_p('motion.digital-in-03','1')
        else:
            display_selected_material(matnum)
    materialAutoChange = False

def material_change_pin_changed(halpin):
    if halpin == 0:
        hal.set_p('motion.digital-in-03','0')
    elif halpin == 3:
        hal.set_p('motion.digital-in-03','1')
        comp['material-change'] = 0

def material_change_number_pin_changed(halpin):
    global getMaterialBusy, materialAutoChange
    if getMaterialBusy:
        return
    if comp['material-change'] == 1:
        materialAutoChange = True
    if halpin == -1:
        halpin = pVars.matDefault.get()
    if not material_exists(halpin):
        materialAutoChange = False
        return
    index = f"@{materialNumList.index(halpin)}"
    rE(f"{fruns}.material.materials setvalue {index}")
    display_selected_material(halpin)

def material_change_timeout_pin_changed(halpin):
    if halpin:
        material = rE(f"{fruns}.material.materials get").split(': ', 1)[0]
        msg0 = _('Material change timeout occurred for material')
        notifications.add('error', f"{msg0} #{material}\n")
        comp['material-change-number'] = material
        comp['material-change-timeout'] = 0
        hal.set_p('motion.digital-in-03','0')

def material_temp_pin_changed(halpin):
    if halpin:
        TEMP.read(TEMP.fn)
        material = get_one_material(TEMP, f"MATERIAL_NUMBER_{halpin}", halpin)
        if not material['valid']:
            msg0 = _('Temporary material')
            msg1 = _('is invalid')
            notifications.add('error', f"{msg0} #{halpin} {msg1}\n")
            return
        materialFileDict[halpin] = { \
                'name': material['name'], \
                'kerf_width': material['kerf_width'], \
                'pierce_height': material['pierce_height'], \
                'pierce_delay': material['pierce_delay'], \
                'puddle_jump_height': material['puddle_jump_height'], \
                'puddle_jump_delay': material['puddle_jump_delay'], \
                'cut_height': material['cut_height'], \
                'cut_speed': material['cut_speed'], \
                'cut_amps': material['cut_amps'], \
                'cut_volts': material['cut_volts'], \
                'pause_at_end': material['pause_at_end'], \
                'gas_pressure': material['gas_pressure'], \
                'cut_mode': material['cut_mode']}
        insert_materials()
        comp['material-temp'] = 0

def insert_materials():
    global materialNumList, materialFileDict, matNames
    mats = ''
    matDefaults = ''
    materialNumList = []
    for matnum in sorted(materialFileDict):
        mats += f" {{{matnum}: {materialFileDict[matnum]['name']}}}"
        materialNumList.append(matnum)
        if matnum < 1000000:
            matDefaults += f" {{{matnum}}}"
    rE(f"{fruns}.material.materials configure -values {{{mats}}}")
    rE(f"{fsetup}.l.gui.matdefault configure -values {{{matDefaults}}}")
    matNames = mats

def display_selected_material(matnum, reload=False):
    rE(f"{fruns}.material.kerf-width set {materialFileDict[matnum]['kerf_width']}")
    rE(f"{fruns}.material.pierce-height set {materialFileDict[matnum]['pierce_height']}")
    rE(f"{fruns}.material.pierce-delay set {materialFileDict[matnum]['pierce_delay']}")
    rE(f"{fruns}.material.puddle-jump-height set {materialFileDict[matnum]['puddle_jump_height']}")
    rE(f"{fruns}.material.puddle-jump-delay set {materialFileDict[matnum]['puddle_jump_delay']}")
    rE(f"{fruns}.material.cut-height set {materialFileDict[matnum]['cut_height']}")
    rE(f"{fruns}.material.cut-feed-rate set {materialFileDict[matnum]['cut_speed']}")
    rE(f"{fruns}.material.cut-amps set {materialFileDict[matnum]['cut_amps']}")
    rE(f"{fruns}.material.cut-volts set {materialFileDict[matnum]['cut_volts']}")
    rE(f"{fruns}.material.pause-at-end set {materialFileDict[matnum]['pause_at_end']}")
    rE(f"{fruns}.material.gas-pressure set {materialFileDict[matnum]['gas_pressure']}")
    rE(f"{fruns}.material.cut-mode set {materialFileDict[matnum]['cut_mode']}")
    if not reload:
        comp['material-change-number'] = matnum

def save_one_material(num):
    section = f"MATERIAL_NUMBER_{num}"
    putPrefs(MATS,section, 'KERF_WIDTH', rE(f"{fruns}.material.kerf-width get"), float)
    putPrefs(MATS,section, 'PIERCE_HEIGHT', rE(f"{fruns}.material.pierce-height get"), float)
    putPrefs(MATS,section, 'PIERCE_DELAY', rE(f"{fruns}.material.pierce-delay get"), float)
    putPrefs(MATS,section, 'PUDDLE_JUMP_HEIGHT', rE(f"{fruns}.material.puddle-jump-height get"), int)
    putPrefs(MATS,section, 'PUDDLE_JUMP_DELAY',rE(f"{fruns}.material.puddle-jump-delay get"), float)
    putPrefs(MATS,section, 'CUT_HEIGHT', rE(f"{fruns}.material.cut-height get"), float)
    putPrefs(MATS,section, 'CUT_SPEED', rE(f"{fruns}.material.cut-feed-rate get"), int)
    putPrefs(MATS,section, 'CUT_AMPS', rE(f"{fruns}.material.cut-amps get"), int)
    putPrefs(MATS,section, 'CUT_VOLTS', rE(f"{fruns}.material.cut-volts get"), int)
    putPrefs(MATS,section, 'PAUSE_AT_END', rE(f"{fruns}.material.pause-at-end get"), float)
    putPrefs(MATS,section, 'GAS_PRESSURE', rE(f"{fruns}.material.gas-pressure get"), float)
    putPrefs(MATS,section, 'CUT_MODE', rE(f"{fruns}.material.cut-mode get"), int)

def set_saved_material():
    matnum = int(rE(f"{fruns}.material.materials get").split(':')[0])
    materialFileDict[matnum] = { \
            'name': rE(f"{fruns}.material.materials get").split(':')[1], \
            'kerf_width': rE(f"{fruns}.material.kerf-width get"), \
            'pierce_height': rE(f"{fruns}.material.pierce-height get"), \
            'pierce_delay': rE(f"{fruns}.material.pierce-delay get"), \
            'puddle_jump_height': rE(f"{fruns}.material.puddle-jump-height get"), \
            'puddle_jump_delay': rE(f"{fruns}.material.puddle-jump-delay get"), \
            'cut_height': rE(f"{fruns}.material.cut-height get"), \
            'cut_speed': rE(f"{fruns}.material.cut-feed-rate get"), \
            'cut_amps': rE(f"{fruns}.material.cut-amps get"), \
            'cut_volts': rE(f"{fruns}.material.cut-volts get"), \
            'pause_at_end': rE(f"{fruns}.material.pause-at-end get"), \
            'gas_pressure': rE(f"{fruns}.material.gas-pressure get"), \
            'cut_mode': rE(f"{fruns}.material.cut-mode get")}

def get_one_material(prefs, section, matnum):
    global notifications
    material = {}
    # we bring in some ints as floats so we can read QtPlasmaC material files
    material['name'] = getPrefs(prefs, section, 'NAME', 'Material', str)
    material['kerf_width'] = getPrefs(prefs, section, 'KERF_WIDTH', round(1 * unitsPerMm,  2), float)
    material['pierce_height'] = getPrefs(prefs, section, 'PIERCE_HEIGHT', round(3.0 * unitsPerMm,  2), float)
    material['pierce_delay'] = getPrefs(prefs, section, 'PIERCE_DELAY', 0, float)
    material['puddle_jump_height'] = int(getPrefs(prefs, section, 'PUDDLE_JUMP_HEIGHT', 0, float))
    material['puddle_jump_delay'] = getPrefs(prefs, section, 'PUDDLE_JUMP_DELAY', 0, float)
    material['cut_height'] = getPrefs(prefs, section, 'CUT_HEIGHT', round(1.0 * unitsPerMm,  2), float)
    material['cut_speed'] = int(getPrefs(prefs, section, 'CUT_SPEED', round(2540 * unitsPerMm,  0), float))
    material['cut_amps'] = int(getPrefs(prefs, section, 'CUT_AMPS', 45, float))
    material['cut_volts'] = int(getPrefs(prefs, section, 'CUT_VOLTS', 100, float))
    material['pause_at_end'] = getPrefs(prefs, section, 'PAUSE_AT_END', 0, float)
    material['gas_pressure'] = getPrefs(prefs, section, 'GAS_PRESSURE', 0, float)
    material['cut_mode'] = int(getPrefs(prefs, section, 'CUT_MODE', 1, float))
    material['valid'] = True
    required = {'pierce_height':material['pierce_height'],'cut_height':material['cut_height'],'cut_speed':material['cut_speed']}
    for item in required:
        if not required[item]:
            material['valid'] = False
            msg0 = _('is missing from Material')
            notifications.add('error',f"{item} {msg0} #{matnum}\n")
    return material

def set_material_dict(materialFileDict, matnum, material):
    materialFileDict[matnum] = { \
            'name': material['name'], \
            'kerf_width': material['kerf_width'], \
            'pierce_height': material['pierce_height'], \
            'pierce_delay': material['pierce_delay'], \
            'puddle_jump_height': material['puddle_jump_height'], \
            'puddle_jump_delay': material['puddle_jump_delay'], \
            'cut_height': material['cut_height'], \
            'cut_speed': material['cut_speed'], \
            'cut_amps': material['cut_amps'], \
            'cut_volts': material['cut_volts'], \
            'pause_at_end': material['pause_at_end'], \
            'gas_pressure': material['gas_pressure'], \
            'cut_mode': material['cut_mode']}

def load_materials(display, keepTemp):
    global getMaterialBusy, materialFileDict
    if keepTemp:
        pop = [key for key in materialFileDict if key < 1000000]
        for e in pop:
            materialFileDict.pop(e)
    else:
        materialFileDict = {}
    getMaterialBusy = True
    if not MATS.sections():
        msg0 = _('Creating new materials file')
        notifications.add('error', f"{msg0}\n")
        material = get_one_material(MATS, 'MATERIAL_NUMBER_0', 0)
        set_material_dict(materialFileDict, 0, material)
    else:
        for section in MATS.sections():
            matnum = int(section.rsplit('_', 1)[1].strip().strip(']'))
            if matnum >= 1000000:
                msg0 = _('Material number')
                msg1 = _('is invalid')
                msg2 = _('Material numbers need to be less than 1000000')
                notifications.add('error', f"{msg0} {matnum} {msg1}\n{msg2}\n")
                continue
            material = get_one_material(MATS, section, matnum)
            if not material['valid']:
                continue
            set_material_dict(materialFileDict, matnum, material)
        if not materialFileDict:
            msg0 = _('Materials file is empty or corrupt')
            notifications.add('error', f"{msg0}\n")
            return
    insert_materials()
    value = getPrefs(PREF, 'GUI_OPTIONS', 'Default material', materialNumList[0], int)
    pVars.matDefault.set(value)
    restoreSetup['matDefault'] = value
    if pVars.matDefault.get() not in materialNumList:
        mat = sorted(materialNumList)[0]
        msg0 = _('Default material')
        msg1 = _('was not found')
        msg2 = _('Changing default material to')
        notifications.add('info', f"{msg0} #{pVars.matDefault.get()} {msg1}\n{msg2} #{mat}\n")
        pVars.matDefault.set(mat)
        putPrefs(PREF, 'GUI_OPTIONS', 'Default material', pVars.matDefault.get(), int)
    if display == -1 or get_displayed_material() not in materialNumList:
        display = pVars.matDefault.get()
    index = f"@{materialNumList.index(display)}"
    rE(f"{fruns}.material.materials setvalue {index}")
    display_selected_material(display)
    material_list_width()
    getMaterialBusy = False

def change_default_material():
    display_selected_material(pVars.matDefault.get())

def material_exists(material):
    global materialNumList, materialAutoChange
    if int(material) in materialNumList:
        return True
    else:
        if materialAutoChange:
            comp['material-change'] = -1
            comp['material-change-number'] = rE(f"{fruns}.material.materials get").split(':')[0]
            msg0 = _('Material')
            msg1 = _('not in material list')
            notifications.add('error', f"{msg0} #{int(material)} {msg1}\n")
        return False


##############################################################################
# POWERMAX COMMUNICATIONS FUNCTIONS                                          #
##############################################################################
def pmx485_check(port, periodic=False):
    if not comPorts:
        return False
    msg1 = _('Powermax communications is disabled')
    ports = []
    for p in comPorts.comports():
        ports.append(p[0])
    if port in ports:
        try:
            sPort = serial.Serial(port, 19200)
            sPort.close()
        except Exception as err:
            if not periodic:
                notifications.add('error', f"{err}\n{msg1}")
            return False
    else:
        if not periodic:
            msg0 = _('cannot be found')
            notifications.add('error', f"{port} {msg0}\n{msg1}")
        return False
    return True

def pmx485_startup(port):
    global pmx485
    if pmx485_load(port):
        return
    pmx485['exists'] = True
    pVars.pmx485Enable.set(True)
    pmx485_enable_toggled()

def pmx485_load(port):
    import subprocess
    msg0 = _('PMX485 component is not able to be loaded,')
    msg1 = _('Powermax communications are not available')
    count = 0
    while not hal.component_exists('pmx485'):
        if count >= 3:
            notifications.add('error', f"{msg0}\n{msg1}\n")
            rE(f"{fruns}.pmx.info configure -text {{{_('Comms Error')}}} -bg red")
            return 1
        subprocess.run(['halcmd', 'loadusr', '-Wn', 'pmx485', 'pmx485', f"{port}"])
        count += 1
    return 0

def pmx485_enable_toggled():
    global pmx485
    if pVars.pmx485Enable.get():
        pmx485['commsError'] = False
        pmx485['retryTimer'] = 0
        if pmx485_load(pmPort):
            return
        # ensure valid parameters before trying to connect
        if pVars.cutMode.get() == 0 or pVars.cutAmps.get() == 0:
            msg0 = _('Invalid Cut Mode or Cut Amps')
            msg1 = _('cannot connect to Powermax')
            notifications.add('error', f"{msg0}\n{msg1}\n")
            pVars.pmx485Enable.set(False)
            return
        # good to go
        else:
            hal.set_p('pmx485.pressure_set', str(pVars.gasPressure.get()))
            hal.set_p('pmx485.current_set', str(pVars.cutAmps.get()))
            hal.set_p('pmx485.mode_set', str(pVars.cutMode.get()))
            hal.set_p('pmx485.enable', '1')
            rE(f"{fruns}.pmx.info configure -text {{{_('Connecting')}}} -bg {colorBack}")
            pmx485['commsTimer'] = 3
    else:
        if hal.component_exists('pmx485'):
            hal.set_p('pmx485.enable', '0')
        pmx485['connected'] = False
        pmx485['commsError'] = False
        pmx485['commsTimer'] == 0
        pmx485['retryTimer'] == 0
        blank = ''
        rE(f"{fruns}.pmx.info configure -text {blank} -bg {colorBack}")

def pmx485_current_changed(value):
    hal.set_p('pmx485.current_set', str(value))

def pmx485_mode_changed(value):
    pVars.gasPressure.set(0)
    hal.set_p('pmx485.pressure_set', '0')
    hal.set_p('pmx485.mode_set', str(value))

def pmx485_pressure_changed(value):
    if value > 0 and value < int(hal.get_value('pmx485.pressure_min') / 2):
        pVars.gasPressure.set(int(hal.get_value('pmx485.pressure_min')))
    elif value > int(hal.get_value('pmx485.pressure_min') / 2)  and value < hal.get_value('pmx485.pressure_min'):
        pVars.gasPressure.set(0)
    else:
        pVars.gasPressure.set(float(value))
    hal.set_p('pmx485.pressure_set', str(pVars.gasPressure.get()))

def pmx485_min_current_changed():
    rE(f"{fruns}.material.cut-amps configure -from hal.get_value('pmx485.current_min')")

def pmx485_max_current_changed():
    rE(f"{fruns}.material.cut-amps configure -to hal.get_value('pmx485.current_max')")

def pmx485_max_pressure_changed():
    rE(f"{fruns}.material.gas-pressure configure -to hal.get_value('pmx485.pressure_max')")
    if float(hal.get_value('pmx485.pressure_max')) > 15:
        rE(f"{fruns}.material.gas-pressureL configure -text {{{_('Gas Pressure psi')}}}")
        rE(f"{fruns}.material.gas-pressure configure -increment 1 -format %0.0f")
    else:
        rE(f"{fruns}.material.gas-pressureL configure -text _('Gas Pressure bar')")
        rE(f"{fruns}.material.gas-pressure configure -increment 0.1 -format %0.1f")

def pmx485_status_changed(state):
    global pmx485
    if state != pmx485['connected']:
        if state:
            pmx485['commsError'] = False
            rE(f"{fruns}.pmx.info configure -text {{{_('Connected')}}} -bg green")
            pmx485['connected'] = True
            # probably not required as we change the text directly in periodic
            #if pmx485['compArcTime']:
            #    pmx485_arc_time_changed(pmx485['compArcTime'])
            # probably not required as we check the fault in periodic
            #if hal.get_value('pmx485.fault'):
            #    pmx485_fault_changed(hal.get_value('pmx485.fault'))
            pmx485['commsTimer'] = 0
            pmx485['retryTimer'] = 0
        else:
            rE(f"{fruns}.pmx.info configure -text {{{_('Comms Error')}}} -bg red")
            pmx485['commsError'] = True
            pmx485['connected'] = False
            pmx485['retryTimer'] = 3

def pmx485_fault_changed(fault):
    global pmx485
    if pmx485['connected']:
        faultRaw = f"{fault:04.0f}"
        pmx485['compFaultCode'] = f"{faultRaw[0]}-{faultRaw[1:3]}-{faultRaw[3]}"
        code = _('Fault Code')
        if faultRaw == '0000':
            rE(f"{fruns}.pmx.info configure -text {{{_('Connected')}}} -bg green")
        elif faultRaw in pmx485FaultName.keys():
            if faultRaw == '0210' and hal.get_value('pmx485.current_max') > 110:
                faultMsg = pmx485FaultName[faultRaw][1]
            elif faultRaw == '0210':
                faultMsg = pmx485FaultName[faultRaw][0]
            else:
                faultMsg = pmx485FaultName[faultRaw]
            rE(f"{fruns}.pmx.info configure -text {{{code}: {pmx485['compFaultCode']}}} -bg red")
            msg0 = _('Code')
            notifications.add('error', f"{msg0}: {pmx485['compFaultCode']}\n{faultMsg}\n")
        else:
            rE(f"{fruns}.pmx.info configure -text {{{code}: {pmx485['compFaultCode']}}} -bg red")
            pmx485['labelState'] = None
            msg0 = _('Unknown Powermax fault code')
            notifications.add('error', f"{msg0}: {faultRaw}\n")

def pmx485_mesh_enable_toggled():
    global pmx485
    if pVars.meshEnable.get() and not pmx485['meshMode']:
        pmx485['oldMode'] = pVars.cutMode.get()
        pVars.cutMode.set(2)
        rE(f"{fruns}.material.cut-mode configure -state disabled")
        pmx485['meshMode'] = True
        hal.set_p('plasmac.mesh-enable', '1')
    elif hal.get_value('plasmac.mesh-enable') and pmx485['meshMode']:
        pVars.cutMode.set(pmx485['oldMode'])
        rE(f"{fruns}.material.cut-mode configure -state normal")
        pmx485['meshMode'] = False
        hal.set_p('plasmac.mesh-enable', '0')

def pmx485_comms_timeout():
    global pmx485
    rE(f"{fruns}.pmx.info configure -text {{{_('Comms Error')}}} -bg red")
    pmx485['commsError'] = True
    pmx485['connected'] = False
    pmx485['retryTimer'] = 3

def pmx485_retry_timeout():
    pmx485_enable_toggled()

pmx485FaultName = {
            '0110': _('Remote controller mode invalid'),
            '0111': _('Remote controller current invalid'),
            '0112': _('Remote controller pressure invalid'),
            '0120': _('Low input gas pressure'),
            '0121': _('Output gas pressure low'),
            '0122': _('Output gas pressure high'),
            '0123': _('Output gas pressure unstable'),
            '0130': _('AC input power unstable'),
            '0199': _('Power board hardware protection'),
            '0200': _('Low gas pressure'),
            '0210': (_('Gas flow lost while cutting'), _('Excessive arc voltage')),
            '0220': _('No gas input'),
            '0300': _('Torch stuck open'),
            '0301': _('Torch stuck closed'),
            '0320': _('End of consumable life'),
            '0400': _('PFC/Boost IGBT module under temperature'),
            '0401': _('PFC/Boost IGBT module over temperature'),
            '0402': _('Inverter IGBT module under temperature'),
            '0403': _('Inverter IGBT module over temperature'),
            '0500': _('Retaining cap off'),
            '0510': _('Start/trigger signal on at power up'),
            '0520': _('Torch not connected'),
            '0600': _('AC input voltage phase loss'),
            '0601': _('AC input voltage too low'),
            '0602': _('AC input voltage too high'),
            '0610': _('AC input unstable'),
            '0980': _('Internal communication failure'),
            '0990': _('System hardware fault'),
            '1000': _('Digital signal processor fault'),
            '1100': _('A/D converter fault'),
            '1200': _('I/O fault'),
            '2000': _('A/D converter value out of range'),
            '2010': _('Auxiliary switch disconnected'),
            '2100': _('Inverter module temp sensor open'),
            '2101': _('Inverter module temp sensor shorted'),
            '2110': _('Pressure sensor is open'),
            '2111': _('Pressure sensor is shorted'),
            '2200': _('DSP does not recognize the torch'),
            '3000': _('Bus voltage fault'),
            '3100': _('Fan speed fault'),
            '3101': _('Fan fault'),
            '3110': _('PFC module temperature sensor open'),
            '3111': _('PFC module temperature sensor shorted'),
            '3112': _('PFC module temperature sensor circuit fault'),
            '3200': _('Fill valve'),
            '3201': _('Dump valve'),
            '3201': _('Valve ID'),
            '3203': _('Electronic regulator is disconnected'),
            '3410': _('Drive fault'),
            '3420': _('5 or 24 VDC fault'),
            '3421': _('18 VDC fault'),
            '3430': _('Inverter capacitors unbalanced'),
            '3441': _('PFC over current'),
            '3511': _('Inverter saturation fault'),
            '3520': _('Inverter shoot-through fault'),
            '3600': _('Power board fault'),
            '3700': _('Internal serial communications fault'),
            }


##############################################################################
# EXTERNAL HAL PINS                                                          #
##############################################################################
# called during setup
def ext_hal_create():
    global extHalPins
    for pin in ['abort', 'power', 'run', 'pause', 'run-pause', 'touchoff',
                'probe','pulse', 'frame-job']:
        comp.newpin(f"ext.{pin}", hal.HAL_BIT, hal.HAL_IN)
        extHalPins[pin] = {'state': False, 'last': False}
    # external pins for user button toggle and pulse
    for pin in range(3):
        comp.newpin(f"ext.out_{pin}", hal.HAL_BIT, hal.HAL_OUT)
    # pins for pierce only offsets
    for pin in 'xy':
        comp.newpin(f"{pin}-pierce-offset", hal.HAL_FLOAT, hal.HAL_OUT)
    # pins for pierce coordinate extents
    for pin in ['x_min', 'y_min', 'x_max', 'y_max']:
        comp.newpin(f"{pin}_pierce_extent", hal.HAL_FLOAT, hal.HAL_IN)

# called every cycle by user_live_update
def ext_hal_watch():
    global extHalPins, isIdle, isIdleHomed, isPaused, isRunning, probePressed, torchPressed
    for pin in extHalPins:
        state = comp[f"ext.{pin}"]
        if state != extHalPins[pin]['last']:
            extHalPins[pin]['last'] = state
            # pressed commands
            if state:
                if pin == 'abort':
                    commands.task_stop()
                elif pin == 'power':
                    commands.onoff_clicked()
                elif pin == 'run' and isIdleHomed:
                    commands.task_run()
                elif pin == 'pause':
                    if isRunning:
                        commands.task_pause()
                    elif isPaused:
                        commands.task_resume()
                elif pin == 'run-pause':
                    if isIdleHomed:
                        commands.task_run()
                    elif isRunning:
                        commands.task_pause()
                    elif isPaused:
                        commands.task_resume()
                elif pin == 'touchoff':
                    touch_off_xy('1', '0', '0')
                elif pin == 'probe' and probeButton:
                    user_button_pressed(probeButton, buttonCodes[int(probeButton)])
                elif pin == 'pulse' and torchButton:
                    user_button_pressed(torchButton, buttonCodes[int(torchButton)])
            # released commands
            else:
                if pin == 'touchoff':
                    touch_off_xy('0', '0', '0')
                elif pin == 'probe' and probeButton:
                    user_button_released(probeButton, buttonCodes[int(probeButton)])
                elif pin == 'pulse' and torchButton:
                    user_button_released(torchButton, buttonCodes[int(torchButton)])
                elif pin == 'frame-job':
                    num = get_button_num('framing')
                    if num:
                        user_button_released(str(num), buttonCodes[num])

def get_button_num(name):
    num = None
    for num in buttonCodes:
        if buttonCodes[num]['code'] == name:
            break
    return num


##############################################################################
# HELP TEXT                                                                  #
##############################################################################
def help_text():
    global kb_text_1, kb_text_2, kp_text_1
    kb_text_1 = [
        ('ESC', _('Abort')),
        ('', ''),
        ('F1', _('Emergency stop')),
        ('F2', _('Machine on/off')),
        ('F3', _('Manual tab')),
        ('F4', _('Cycle through right side tabs')),
        ('F5', _('MDI tab')),
        ('F9', _('Manual cut start/stop')),
        ('', ''),
        ('X', _('Activate first axis')),
        ('Y', _('Activate second axis')),
        ('Z', _('Activate third axis')),
        ('A', _('Activate fourth axis')),
        ('B', _('Activate fifth axis')),
        ('', ''),
        ('C', _('Select continuous jog')),
        (_('I or Shift-I'), _('Select jog increment')),
        (_('Left or Right'), _('Jog first axis or joint')),
        (_('Up or Down'), _('Jog second axis or joint')),
        (_('PgUp or PgDn'), _('Jog third axis or joint')),
        ('[ or ]', _('Jog fourth axis or joint')),
        ('; or \'', _('Jog fifth axis or joint')),
        (_('Shift+ above jog'), _('Jog at traverse speed')),
    ]
    kb_text_2 = [
         (_('Ctrl-Home'), _('Emulate GUI home button')),
        (_('Shift-Home'), _('Home active joint')),
        (_('End'), _('Touchoff X and Y to zero')),
        (_('Ctrl-End'), _('Touchoff active axis to zero')),
        (_('Shift-End'), _('Set touchoff for current axis')),
        (_('Del'), _('Emulate GUI laser button')),
        ('', ''),
        ('O', _('Open program')),
        ('R', _('Run program')),
        ('T', _('Step program')),
        ('P or Space', _('Pause program')),
        ('S', _('Resume program')),
        (_('Ctrl-R'), _('Reload program')),
        (_('Ctrl-S'), _('Save G-code as')),
        ('', ''),
        (_('Ctrl-K'), _('Clear live plot')),
        (_('Ctrl-Space'), _('Clear notifications')),
        ('', ''),
        ('World Mode:', ''),
        ('` or 1~0', _('Feed override from 0% to 100%')),
        ('Ctrl+ ` or 1~0', _('Rapid Override from 0% to 100%')),
        ('Alt+ ` or 1~0', _('Jog speed from 0% to 100%')),
        ('Joint Mode:', ''),
        ('` or 0~8', _('Activate joints 1 thru 9')),
    ]
    kp_text_1 = [
        (_('Left'), _('Jog first axis or joint negative')),
        (_('Right'), _('Jog first axis or joint positive')),
        (_('Up'), _('Jog second axis or joint positive')),
        (_('Down'), _('Jog second axis or joint negative')),
        (_('PgUp'), _('Jog third axis or joint positive')),
        (_('PgDn'), _('Jog third axis or joint negative')),
        ('', ''),
        (_('Minus+ above jog'), _('Jog speed x 0.1')),
        (_('Plus+ above jog'), _('Jog speed x 10')),
        ('', ''),
        (_('Home'), _('Emulate GUI home button')),
        (_('End'), _('Touchoff X and Y to zero')),
        (_('Del'), _('Emulate GUI laser button')),
    ]

def install_kb_text(app):
    keys = nf.makewidget(app, Frame, '.keys.text')
    fixed = app.tk.call("linuxcnc::standard_fixed_font")
    for i in range(len(kb_text_1)):
        a, b = kb_text_1[i]
        Label(keys, text=a, font=fixed, padx=4, pady=0, highlightthickness=0).grid(row=i, column=0, sticky="w")
        Label(keys, text=b, padx=4, pady=0, highlightthickness=0).grid(row=i, column=1, sticky="w")
    for i in range(len(kb_text_2)):
        a, b = kb_text_2[i]
        Label(keys, text=a, font=fixed, padx=4, pady=0, highlightthickness=0).grid(row=i, column=3, sticky="w")
        Label(keys, text=b, padx=4, pady=0, highlightthickness=0).grid(row=i, column=4, sticky="w")
    Label(keys, text="    ").grid(row=0, column=2)

def install_kp_text(app):
    keyp = nf.makewidget(app, Frame, '.keyp.text')
    fixed = app.tk.call("linuxcnc::standard_fixed_font")
    for i in range(len(kp_text_1)):
        a, b = kp_text_1[i]
        Label(keyp, text=a, font=fixed, padx=4, pady=0, highlightthickness=0).grid(row=i, column=0, sticky="w")
        Label(keyp, text=b, padx=4, pady=0, highlightthickness=0).grid(row=i, column=1, sticky="w")


##############################################################################
# KEYBOARD BINDINGS                                                          #
##############################################################################
def key_pressed(key):
    global keyDelay
    if key not in keyDelay:
        keyDelay[key] = None
    if keyDelay[key]:
        root_window.after_cancel(keyDelay[key])
        keyDelay[key] = None
        return
    if key == 'Delete':
        laser_button_toggled('1', 1)
    if key == 'End':
        touch_off_xy('1', 0, 0)
    if key == 'Home':
        if homing_order_defined:
            commands.home_all_joints()
        else:
            commands.home_joint()
    elif key == 'jogfast':
        pVars.jogMultiplier.set(10)
    elif key == 'jogslow':
        pVars.jogMultiplier.set(0.1)

def key_off(key):
    global keyDelay
    keyDelay[key] = None
    if key == 'Delete':
        laser_button_toggled('0', 1)
    if key == 'End':
        touch_off_xy('0', 0, 0)
    elif key in ['jogfast', 'jogslow']:
        pVars.jogMultiplier.set(1)

def key_released(key):
    global keyDelay
    if key not in keyDelay:
        keyDelay[key] = None
    if keyDelay[key]:
        return
    keyDelay[key] = root_window.after_idle(lambda: key_off(key))

def kb_shortcuts_changed():
    keyboard_bindings(pVars.kbShortcuts.get())

def virtual_kb_changed():
    if pVars.useVirtKB.get():
        if not WHICH('onboard'):
            rE(f"{fsetup}.l.gui.useVirtKB deselect")
            msg0 = _('a valid "onboard" virtual keyboard is not installed')
            notifications.add('error', f"{msg0}\n")
            return
        pVars.kbShortcuts.set(0)
        rE(f"{fsetup}.l.gui.kbShortcutsL configure -state disabled")
        rE(f"{fsetup}.l.gui.kbShortcuts configure -state disabled")
        if rE('winfo ismapped .fconv'):
            vkb_show('numpad')
        elif rE(f"winfo ismapped {fparam}"):
            vkb_show('numpad')
        elif rE(f"winfo ismapped {fsetup}"):
            vkb_show('keyboard')
    else:
        rE(f"{fsetup}.l.gui.kbShortcutsL configure -state normal")
        rE(f"{fsetup}.l.gui.kbShortcuts configure -state normal")
        vkb_hide()

def make_lambda(func, value, state=None):
    if state is not None:
        return lambda event:func(value, state)
    else:
        return lambda event:func(value)

def keyboard_bindings(state):
    if firstRun:
        help_text()
    # delete kb shortcuts from help menu
    rE('.menu.help delete 3 5')
    # remove current bindings
    for key in root_window.bind():
        root_window.unbind(key)
    # create new key bindings if enabled
    if state:
        # Escape
        root_window.bind('<Escape>', commands.task_stop)
        # F keys
        root_window.bind_class('all', '<Key-F1>', commands.estop_clicked)
        root_window.bind('<Key-F2>', commands.onoff_clicked)
        root_window.bind('<Key-F3>', f"{pane_top}.tabs raise manual")
        root_window.bind('<Key-F4>', commands.next_tab)
        root_window.bind('<Key-F5>', f"{pane_top}.tabs raise mdi")
#FIXME - ???
#        root_window.bind('<Key-F5>', f"+{tabs_mdi}.command selection range 0 end")
        root_window.bind('<Key-F9>', commands.manual_cut)
        # axis selection
        root_window.bind('x', lambda event: activate_ja_widget('x'))
        root_window.bind('y', lambda event: activate_ja_widget('y'))
        root_window.bind('z', lambda event: activate_ja_widget('z'))
        root_window.bind('a', lambda event: activate_ja_widget('a'))
        root_window.bind('b', lambda event: activate_ja_widget('b'))
        #jogging
        root_window.bind('c', lambda event: jogspeed_continuous())
        root_window.bind('i', lambda event: jogspeed_incremental())
        root_window.bind('I', lambda event: jogspeed_incremental(-1))
        bind_axis('Left', 'Right', 0)
        bind_axis('Down', 'Up', 1)
        bind_axis('Next', 'Prior', 2)
        bind_axis('KP_Left', 'KP_Right', 0)
        bind_axis('KP_Down', 'KP_Up', 1)
        bind_axis('KP_Next', 'KP_Prior', 2)
#        bind_axis('KP_4', 'KP_6', 0)
#        bind_axis('KP_2', 'KP_8', 1)
#        bind_axis('KP_3', 'KP_9', 2)
        bind_axis('bracketleft', 'bracketright', 3)
        bind_axis('semicolon', 'quoteright', 4)
        root_window.bind('<Key-KP_Add>', lambda event: commands.key_pressed('jogfast'))
        root_window.bind('<KeyRelease-KP_Add>', lambda event: commands.key_released('jogfast'))
        root_window.bind('<Key-KP_Subtract>', lambda event: commands.key_pressed('jogslow'))
        root_window.bind('<KeyRelease-KP_Subtract>', lambda event: commands.key_released('jogslow'))
        # sliders
        root_window.bind('`', lambda event: activate_ja_widget_or_set_feedrate(0))
        root_window.bind('<Control-Key-quoteleft>',lambda event: set_rapidrate(0))
        root_window.bind('<Alt-Key-quoteleft>',lambda event: set_jog_slider(0.0))
        for n in range(10):
            rate = n if n > 0 else 10
            root_window.bind(f"{n}", make_lambda(activate_ja_widget_or_set_feedrate, rate))
            root_window.bind(f"<Control-Key-{n}>", make_lambda(set_rapidrate, rate * 10))
            root_window.bind(f"<Alt-Key-{n}>", make_lambda(set_jog_slider, rate * 0.1))
        # homing
        if homing_order_defined:
            root_window.bind('<Control-Home>', commands.home_all_joints)
        else:
            root_window.bind('<Control-Home>', commands.home_joint)
        root_window.bind('<Shift-Home>', commands.home_joint)
        # touchoff
        root_window.bind('<Key-End>', lambda event: commands.touch_off_xy('1', 0, 0))
        root_window.bind('<KeyRelease-End>', lambda event: commands.touch_off_xy('0', 0, 0))
        root_window.bind('<Control-End>', commands.set_axis_offset)
        root_window.bind('<Shift-End>', commands.touch_off_system)
        root_window.bind('<Key-Delete>', lambda event: commands.key_pressed('Delete'))
        root_window.bind('<KeyRelease-Delete>', lambda event: commands.key_released('Delete'))
        # program
        root_window.bind('o', commands.open_file)
        root_window.bind('r', commands.task_run)
        root_window.bind('p', commands.task_pause)
        root_window.bind('<space>', commands.task_pause)
        root_window.bind('s', commands.task_resume)
        root_window.bind('<Control-r>', commands.reload_file)
        root_window.bind('<Control-s>', commands.save_gcode)
        # clearing
        root_window.bind('<Control-k>', commands.clear_live_plot)
        root_window.bind('<Control-space>', lambda event: notifications.clear())
        # whacky keypad stuff
        keys1 = ['Decimal', '0', '1', '2', '3', '4', '5', '6', '7', '8', '9']
        # Down, Next, Left, Right, Up, and Prior are done in the jogging section above
        keys2 = ['Delete', 'Insert', 'End', None, None, None, 'Begin', None, 'Home', None, None]
        for k in range(len(keys2)):
            if keys2[k]:
                root_window.bind(f"<Key-KP_{keys2[k]}>", make_lambda(key_pressed, keys2[k]))
                root_window.bind(f"<KeyRelease-KP_{keys2[k]}>", make_lambda(key_released, keys2[k]))
                root_window.bind(f"<Mod2-KeyRelease-KP_{keys2[k]}>", make_lambda(key_released, keys1[k]))
        # add kb shortcuts to help menu
        rE('.menu.help add separator')
        rE('.menu.help add command -command {wm transient .keys .;wm deiconify .keys; focus .keys.ok}')
        rE(f"setup_menu_accel .menu.help end {{{_('Keyboard Shortcuts')}}}")
        rE('.menu.help add command -command {wm transient .keyp .;wm deiconify .keyp; focus .keyp.ok}')
        rE(f"setup_menu_accel .menu.help end {{{_('Keypad Shortcuts')}}}")
    # use arrow keys for spinboxes
    for item in rpList + cpList:
        widget = f"{item[0]}.{item[1]}"
        rE(f"bind {widget} <Up> {{%W invoke buttonup}}")
        rE(f"bind {widget} <Down> {{%W invoke buttondown}}")
        rE(f"bind {widget} <Left> {{::tk::EntrySetCursor %W [expr [%W index insert] - 1]}}")
        rE(f"bind {widget} <Right> {{::tk::EntrySetCursor %W [expr [%W index insert] + 1]}}")


##############################################################################
# ONBOARD VIRTUAL KEYBOARD                                                   #
##############################################################################
def vkb_validate():
    global vkbData
    vkbData = {'valid':False, 'visible':False, 'required':False, 'layout':None, 'width':0, 'height':0}
    if WHICH('onboard'):
        try:
            cmd = 'gsettings get org.onboard layout'
            vkbData['layout'] = Popen(cmd, stdout=PIPE, stderr=PIPE, shell=True).communicate()[0].decode().strip()
            cmd = 'gsettings get org.onboard.window.landscape width'
            vkbData['width'] = int(Popen(cmd, stdout=PIPE, stderr=PIPE, shell=True).communicate()[0].decode().strip())
            cmd = 'gsettings get org.onboard.window.landscape height'
            vkbData['height'] = int(Popen(cmd, stdout=PIPE, stderr=PIPE, shell=True).communicate()[0].decode().strip())
            vkbData['valid'] = True
        except:
            pass

def vkb_show(layout):
    global vkbData
    if firstRun: return
    if not pVars.useVirtKB.get(): return
    w = 240 if layout == 'numpad' else 740
    vkbData['visible'] = True
    vkb_settings(os.path.join(libPath, layout), w, 240)
    time.sleep(0.1)
    cmd  = 'dbus-send'
    cmd += ' --type=method_call'
    cmd += ' --dest=org.onboard.Onboard'
    cmd += ' /org/onboard/Onboard/Keyboard'
    cmd += ' org.onboard.Onboard.Keyboard.Show'
    Popen(cmd, stdout=PIPE, shell=True)

def vkb_hide(custom=False):
    vkbData['visible'] = False
    cmd  = 'dbus-send'
    cmd += ' --type=method_call'
    cmd += ' --dest=org.onboard.Onboard'
    cmd += ' /org/onboard/Onboard/Keyboard'
    cmd += ' org.onboard.Onboard.Keyboard.Hide'
    Popen(cmd, stdout=PIPE, shell=True)
    vkb_settings(vkbData['layout'], vkbData['width'], vkbData['height'])

def vkb_settings(layout, width, height):
    Popen(f"gsettings set org.onboard layout {layout}", stdout=PIPE, shell=True)
    Popen(f"gsettings set org.onboard.window.landscape width {width - 1}", stdout=PIPE, shell=True)
    Popen(f"gsettings set org.onboard.window.landscape height {height - 1}", stdout=PIPE, shell=True)
    time.sleep(0.1)
    Popen(f"gsettings set org.onboard.window.landscape width {width}", stdout=PIPE, shell=True)
    Popen(f"gsettings set org.onboard.window.landscape height {height}", stdout=PIPE, shell=True)

##############################################################################
# COLOR CHANGE                                                               #
##############################################################################
def read_colors():
    global colorFore, colorBack, colorDisable, colorActive, colorWarn
    global colorVolt, colorArc, colorLed, colorTrough
    colorFore = getPrefs(PREF, 'GUI_OPTIONS','Foreground color', '#000000', str)
    colorBack = getPrefs(PREF, 'GUI_OPTIONS','Background color', '#808080', str)
    colorDisable = getPrefs(PREF, 'GUI_OPTIONS','Disabled color', '#a0a0a0', str)
    colorActive = getPrefs(PREF, 'GUI_OPTIONS','Active color', '#00cc00', str)
    colorWarn = getPrefs(PREF, 'GUI_OPTIONS','Warning color', '#dd0000', str)
    colorVolt = getPrefs(PREF, 'GUI_OPTIONS','Voltage color', '#ff8800', str)
    colorArc = getPrefs(PREF, 'GUI_OPTIONS','Arc OK color', '#ff8800', str)
    colorLed = getPrefs(PREF, 'GUI_OPTIONS','LED color', '#eeff00', str)
    colorTrough = getPrefs(PREF, 'GUI_OPTIONS','Trough color', '#505050', str)

def color_user_buttons(fgc='#000000',bgc='#d9d9d9'):
    for b in criticalButtons:
        rE(f"{fbuttons}.button{b} configure -bg {colorWarn}")
    # user button entries in setup frame
    for n in range(1, maxUserButtons + 1):
        if n in invalidButtons:
            rE(f"{fsetup}.r.ubuttons.canvas.frame.name{n} configure -fg {colorWarn}")
            rE(f"{fsetup}.r.ubuttons.canvas.frame.code{n} configure -fg {colorWarn}")
        else:
            rE(f"{fsetup}.r.ubuttons.canvas.frame.name{n} configure -fg {colorFore}")
            rE(f"{fsetup}.r.ubuttons.canvas.frame.code{n} configure -fg {colorFore}")

def color_torch():
    if hal.get_value('plasmac.torch-enable'):
        txt = torchEnable['enabled'].replace('\\','\n')
        rE(f"{fbuttons}.torch-enable configure -fg {colorFore} -bg {colorActive} \
           -activebackground {colorActive} -text {{{txt}}}")
    else:
        txt = torchEnable['disabled'].replace('\\','\n')
        rE(f"{fbuttons}.torch-enable configure -fg {colorFore} -bg {colorWarn} \
           -activebackground {colorWarn} -text {{{txt}}}")

def get_all_children(parent):
    raw = rE(f"winfo children {parent}")
    children = raw.split()
    for item in children:
        if rE(f"winfo children {item}"):
            raw = (rE(f"winfo children {item}"))
            for item in raw.split():
                children.append(item)
    return(children)

def color_change():
    # menu colors
    rE(f"option add *Menu.Background {colorBack}")
    rE(f"option add *Menu.Foreground {colorFore}")
    widgetTypes = []
    for child in get_all_children('.'):
        if child in [f"{fbuttons}.torch-enable"]:
            continue
        w = rE(f"winfo class {child}")
        # root window
        rE(f". configure -bg {colorBack}")
        # all widgets
        try:
            rE(f"{child} configure -fg {colorFore}")
        except:
            pass
        try:
            rE(f"{child} configure -bg {colorBack}")
        except:
            pass
        try:
            rE(f"{child} configure -disabledforeground {colorDisable}")
        except:
            pass
        try:
            rE(f"{child} configure -activebackground {colorBack}")
        except:
            pass
        try:
            rE(f"{child} configure -buttonbackground {colorBack}")
        except:
            pass
        try:
            rE(f"{child} configure -highlightthickness 0")
        except:
            pass
        try:
            rE(f"{child} configure -highlightbackground {colorBack}")
        except:
            pass
        try:
            if w == 'Menu':
                rE(f"{child} configure -selectcolor {colorFore}")
            else:
                rE(f"{child} configure -selectcolor {colorActive}")
        except:
            pass
        try:
            # color the trough of override scales and user button scrollbars
            rE(f"{child} configure -troughcolor {colorTrough}")
            # color the trough of combobox lists
            rE(f"option add *Scrollbar.troughColor {colorTrough}")
            rE(f"option add *Scrollbar.background {colorBack}")
            rE(f"option add *Scrollbar.activeBackground {colorBack}")
        except:
            pass
        # all comboboxes except for the jog increment Combobox
        if w == 'ComboBox':
            rE(f"{child} configure -background {colorBack}")
            rE(f"{child} configure -selectbackground {colorBack}")
            rE(f"{child} configure -selectforeground {colorFore}")
            # lose the arrow
            rE(f"pack forget {child}.a")
        # the entry of the jog increment combobox
        elif '.jogincr' in child and w == 'Entry':
            rE(f"{child} configure -disabledforeground {colorFore}")
        # the listbox of the jog increment combobox
        elif '.jogincr' in child and w == 'Listbox':
            rE(f"{child} configure -selectforeground {colorFore}")
            rE(f"{child} configure -selectbackground {colorBack}")
        # all checkbuttons
        elif w in ['Checkbutton']:
            rE(f"{child} configure -relief raised -overrelief raised -bd 1")
    # notebook tabs - cutrecs is also done each time it is raised
    for nbook in [ftabs, fright]:
        pages = rE(f"{nbook} pages").split()
        for page in pages:
            rE(f"{nbook} itemconfigure {page} -foreground {colorFore} -background {colorBack}")
    # leds
    for led in ['arc-ok','torch','breakaway','thc-enabled','thc-active','ohmic','float','up','down','corner-locked','void-locked']:
        rE(f"{fleds}.led-{led} itemconfigure all -disabledfill {colorBack}")
    for led in ['breakaway','void-locked','corner-locked']:
        rE(f"{fleds}.led-{led} itemconfigure all -fill {colorWarn}")
    for led in ['thc-enabled','thc-active']:
        rE(f"{fleds}.led-{led} itemconfigure all -fill {colorActive}")
    for led in ['ohmic','float','up','down']:
        rE(f"{fleds}.led-{led} itemconfigure all -fill {colorLed}")
    rE(f"{fleds}.led-arc-ok itemconfigure all -fill {colorArc}")
    rE(f"{fleds}.led-torch itemconfigure all -fill {colorLed}")
    # arc voltage
    rE(f"{fplasma}.arc-voltage configure -fg {colorVolt}")
    color_user_buttons()
    color_torch()
    # the color setup buttons
    rE(f"{fsetup}.m.colors.fore configure -bg {colorFore} -activebackground {colorFore}")
    rE(f"{fsetup}.m.colors.back configure -bg {colorBack} -activebackground {colorBack}")
    rE(f"{fsetup}.m.colors.disable configure -bg {colorDisable} -activebackground {colorDisable}")
    rE(f"{fsetup}.m.colors.active configure -bg {colorActive} -activebackground {colorActive}")
    rE(f"{fsetup}.m.colors.warn configure -bg {colorWarn} -activebackground {colorWarn}")
    rE(f"{fsetup}.m.colors.volt configure -bg {colorVolt} -activebackground {colorVolt}")
    rE(f"{fsetup}.m.colors.arc configure -bg {colorArc} -activebackground {colorArc}")
    rE(f"{fsetup}.m.colors.led configure -bg {colorLed} -activebackground {colorLed}")
    rE(f"{fsetup}.m.colors.trough configure -bg {colorTrough} -activebackground {colorTrough}")
    # notifications
    rE(f"option add *!notification2.Frame.Background {colorBack}")
    rE(f"option add *!notification2.Frame.Label.Foreground {colorFore}")
    rE(f"option add *!notification2.Frame.Label.Background {colorBack}")
    rE(f"option add *!notification2.Frame.Button.Background {colorBack}")
    rE(f"option add *!notification2.Frame.Button.activeBackground {colorBack}")
    rE(f"option add *!notification2.Frame.Button.highlightThickness 0")
    # tags in gcode text
    t.tag_configure('ignored', background='#ffffff', foreground='#808080')
    t.tag_configure('lineno', foreground=colorTrough)
    t.tag_configure('executing', background=colorTrough)
    t.tag_configure('sel', background='red', foreground='blue')

def color_set(option):
    global colorFore, colorBack, colorDisable, colorActive, colorWarn, colorVolt, colorArc, colorLed, colorTrough
    if option == 'fore':
        color = colorFore
    elif option == 'back':
        color = colorBack
    elif option == 'disable':
        color = colorDisable
    elif option == 'active':
        color = colorActive
    elif option == 'warn':
       color = colorWarn
    elif option == 'volt':
       color = colorVolt
    elif option == 'arc':
        color = colorArc
    elif option == 'led':
        color = colorLed
    elif option == 'trough':
       color = colorTrough
    else:
        color = '#000000'
    colors = askcolor(color, title=_('plasmac2 Color Selector'))
    if colors[1]:
        if option == 'fore':
            colorFore = colors[1]
        elif option == 'back':
            colorBack = colors[1]
        elif option == 'disable':
            colorDisable = colors[1]
        elif option == 'active':
            colorActive = colors[1]
        elif option == 'warn':
            colorWarn = colors[1]
        elif option == 'volt':
            colorVolt = colors[1]
        elif option == 'arc':
            colorArc = colors[1]
        elif option == 'led':
            colorLed = colors[1]
        elif option == 'trough':
            colorTrough = colors[1]
        color_change()

##############################################################################
# ORIENTATION FUNCTIONS                                                      #
##############################################################################
def set_orientation():
    global orientStart
    orientStart = True
    matNum = int(rE(f"{fruns}.material.materials get").split(': ', 1)[0])
    pmxText = rE(f"{fruns}.pmx.info cget -text")
    set_orient_frames()
    recreate_widget_list()
    user_button_setup()
    populate_settings_frame()
    for box in rSpinBoxes:
        rE(f"{box} configure -font fontGui")
    color_change()
    set_window_size()
    load_materials(get_displayed_material(), keepTemp=True)
    index = f"@{materialNumList.index(matNum)}"
    rE(f"{fruns}.material.materials setvalue {index}")
    display_selected_material(matNum)
    rE(f"{fruns}.pmx.info configure -text {{{pmxText}}}")
    orientStart = False

def set_orient_frames():
    global fportrait, fruns, fbuttons, toolmat
    if not firstRun:
        rE(f"destroy {toolmat}")
        rE(f"destroy {fruns}")
        rE(f"destroy {fbuttons}")
        rE(f"destroy {fportrait}")
        rE(f"destroy {fruns}.material.materials")
    fportrait = '.fportrait'
    if pVars.orient.get() == 'portrait':
        fruns = '.fportrait.fruns'
        fbuttons = '.fportrait.fbuttons'
        toolmat = f"{fruns}.toolmat"
        rE(f"frame {fportrait} -relief raised -borderwidth 2")
    else:
        fruns = '.fruns'
        fbuttons = '.fbuttons'
        toolmat = '.toolmat'
    rE(f"frame {fbuttons} -relief flat")
    rE(f"frame {fruns} -relief flat")
    make_torch_button()
    make_dry_run_button()
    make_toolmat_frame()
    make_run_panel()

def make_torch_button():
    rE(f"button {fbuttons}.torch-enable -takefocus 0 -width 10 -highlightthickness 0")
    if '\\' in torchEnable['enabled'] or '\\' in torchEnable['disabled']:
        rE(f"{fbuttons}.torch-enable configure -height 2")
    else:
        rE(f"{fbuttons}.torch-enable configure -height 1")
    rE(f"grid {fbuttons}.torch-enable -column 0 -row 0 -sticky new -padx {{2 0}} -pady {{2 0}}")
    rE(f"bind {fbuttons}.torch-enable <ButtonPress-1> torch_enable")

def make_dry_run_button():
    if pVars.orient.get() == 'portrait':
        col = 1
        row = 0
    else:
        col = 0
        row = 1
    rE(f"button {fbuttons}.dry-run -takefocus 0 -width 10 -highlightthickness 0 -text {{Dry Run}} -command {{task_run dry-run}}")
    rE(f"grid {fbuttons}.dry-run -column {col} -row {row} -sticky new -padx {{2 0}} -pady {{2 0}}")

def make_toolmat_frame():
    rE(f"frame {toolmat} -borderwidth 1 -relief raised")
    rE(f"Button {toolmat}.delete -command {{delete_material_clicked}} -relief link -takefocus 0")
    rE(f"Button {toolmat}.new -command {{new_material_clicked}} -relief link -takefocus 0")
    rE(f"Button {toolmat}.reload -command {{reload_material_clicked}} -relief link -takefocus 0")
    rE(f"Button {toolmat}.save -command {{save_material_clicked}} -relief link -takefocus 0")
    rE(f"{toolmat}.delete configure -image {rE(f'load_image {imageAxis}/tool_zoomout')}")
    rE(f"{toolmat}.new configure -image {rE(f'load_image {imageAxis}/tool_zoomin')}")
    rE(f"{toolmat}.reload configure -image {rE(f'load_image {imageAxis}/tool_reload')}")
    rE(f"{toolmat}.save configure -image {rE(f'load_image {imagePath}/save')}")
    rE(f"pack {toolmat}.save -side left -padx {{0 4}}")
    rE(f"pack {toolmat}.reload -side left")
    rE(f"pack {toolmat}.delete -side right -padx {{4 0}}")
    rE(f"pack {toolmat}.new -side right")

def make_run_panel():
    global rpList, rSpinBoxes
    rSpinBoxes = []
    rE(f"labelframe {fruns}.material -text {{{_('Material')}}} -relief groove")
    rE(f"ComboBox {fruns}.material.materials -modifycmd {{material_changed}} -textvariable matCurrent -width 10 -editable 0")
    rE(f"grid  {fruns}.material.materials -column 0 -row 0 -columnspan 2 -sticky ew -padx 2 -pady 2")
    # spinboxes [parent, name, decimals, value, min, max, increment, label text]
    rpList = [[f"{fruns}.material",'kerf-width',2,0.5,0,5,0.01,'Kerf Width'], \
              [f"{fruns}.material",'pierce-height',2,4,0,25,0.01,'Pierce Height'], \
              [f"{fruns}.material",'pierce-delay',1,0.0,0,10,0.1,'Pierce Delay'], \
              [f"{fruns}.material",'cut-height',2,1,0,25,0.01,'Cut Height'], \
              [f"{fruns}.material",'cut-feed-rate',0,4000,0,19999,1,'Feed Rate'], \
              [f"{fruns}.material",'cut-amps',0,45,0,999,1,'Cut Amps'], \
              [f"{fruns}.material",'cut-volts',0,122,50,300,1,'Cut Volts'], \
              [f"{fruns}.material",'puddle-jump-height',0,0,0,200,1,'P-Jump Height'], \
              [f"{fruns}.material",'puddle-jump-delay',2,0,0,9,0.01,'P-Jump Delay'], \
              [f"{fruns}.material",'pause-at-end',1,0,0,9,0.1,'Pause At End'], \
              [f"{fruns}.material",'cut-mode',0,1,1,3,1,'Cut Mode'], \
              [f"{fruns}.material",'gas-pressure',1,0,0,150,0.1,'Gas Pressure'], \
             ]
    rpRow = 1
    for rpItem in rpList:
        if s.linear_units != 1:
            if rpItem[1] == 'kerf-width':
                rpItem[2:7] = [4,0.02,0,1,0.0001]
            elif rpItem[1] == 'pierce-height':
                rpItem[2:7] = [3,0.16,0,1,0.001]
            elif rpItem[1] == 'cut-height':
                rpItem[2:7] = [3,0.04,0,1,0.001]
            elif rpItem[1] == 'cut-feed-rate':
                rpItem[2:7] = [0,160,0,999,0.1]
        rpName = f"{rpItem[0]}.{rpItem[1]}"
        rpType = 'flt' if rpItem[2] > 0 else 'int'
        rE(f"spinbox {rpName} -width 9 -justify right -wrap true")
        rSpinBoxes.append(rpName)
        rE(f"{rpName} configure -from {rpItem[4]} -to {rpItem[5]}")
        if rpItem[1] == 'kerf-width':
            rE(f"{rpName} configure -textvariable kerfWidth")
        elif rpItem[1] == 'cut-amps':
            rE(f"{rpName} configure -textvariable cutAmps")
        elif rpItem[1] == 'cut-mode':
            rE(f"{rpName} configure -textvariable cutMode")
        elif rpItem[1] == 'gas-pressure':
            rE(f"{rpName} configure -textvariable gasPressure")
        rE(f"{rpName} configure -increment {rpItem[6]} -format %0.{rpItem[2]}f")
        rE(f"{rpName} configure -validate key -vcmd {{{valspin} %W {rpType} {rpItem[2]} %P %s}}")
        rE(f"label {rpName}L -text {{{rpItem[7]}}}")
        if rpItem[1] not in ['cut-mode', 'gas-pressure']:
            rE(f"grid  {rpName} -column 0 -row {rpRow} -padx {{2 0}} -pady {{0 2}}")
            rE(f"grid {rpName}L -column 1 -row {rpRow} -sticky W -pady {{0 2}}")
        rpRow += 1
    # checkbox frame
    rE(f"labelframe {fruns}.check -text {{{_('Cut')}}} -relief groove")
    rE(f"checkbutton {fruns}.check.av -variable autoVolts -command auto_volts_toggled -width 2 -anchor w -indicatoron 0")
    rE(f"label {fruns}.check.avL -text {{{_('Use Auto Volts')}}}")
    rE(f"checkbutton {fruns}.check.ohmic -variable ohmicEnable -command ohmic_enable_toggled -width 2 -anchor w -indicatoron 0")
    rE(f"label {fruns}.check.ohmicL -text {{{_('Ohmic Probe Enable')}}}")
    rE(f"checkbutton {fruns}.check.mesh -variable meshEnable -command pmx485_mesh_enable_toggled -width 2 -anchor w -indicatoron 0")
    rE(f"label {fruns}.check.meshL -text {{{_('Mesh Mode')}}}")
    rE(f"checkbutton {fruns}.check.ignoreok -variable ignorArcOk -command ignore_arc_ok_toggled -width 2 -anchor w -indicatoron 0")
    rE(f"label {fruns}.check.ignoreokL -text {{{_('Ignore Arc OK')}}}")
    rE(f"grid {fruns}.check.av -column 0 -row 0 -sticky w -padx {{2 0}} -pady {{0 2}}")
    rE(f"grid {fruns}.check.avL -column 1 -row 0 -sticky w -pady {{0 2}}")
    rE(f"grid {fruns}.check.ohmic -column 0 -row 1 -sticky w -padx {{2 0}} -pady {{0 2}}")
    rE(f"grid {fruns}.check.ohmicL -column 1 -row 1 -sticky w -pady {{0 2}}")
    rE(f"grid {fruns}.check.mesh -column 0 -row 2 -sticky w -padx {{2 0}} -pady {{0 2}}")
    rE(f"grid {fruns}.check.meshL -column 1 -row 2 -sticky w -pady {{0 2}}")
    rE(f"grid {fruns}.check.ignoreok -column 0 -row 3 -sticky w -padx {{2 0}} -pady {{0 2}}")
    rE(f"grid {fruns}.check.ignoreokL -column 1 -row 3 -sticky w -pady {{0 2}}")
    # pmx frame
    rE(f"labelframe {fruns}.pmx -text {{{_('Powermax')}}} -relief groove")
    rE(f"checkbutton {fruns}.pmx.enable -text {{{_('Powermax Comms')}}} -variable pmx485Enable -command pmx485_enable_toggled -width 2 -anchor w -indicatoron 0")
    rE(f"label {fruns}.pmx.info -anchor w")
    rE(f"pack {fruns}.pmx.enable -expand 1 -fill x")
    rE(f"pack {fruns}.pmx.info -expand 1 -fill x")

def populate_run_panel():
    if pVars.orient.get() == 'portrait':
        rE(f"grid {fportrait} -column 1 -row 3 -sticky nsew -padx 2 -pady {{0 2}}")
        rE(f"grid {fbuttons} -column 0 -row 0 -sticky nsew -padx 0 -pady 0")
        rE(f"grid {fruns} -column 2 -row 0 -sticky nsew -padx 1 -pady 1")
        rE(f"grid {fruns}.check -column 0 -row 1 -sticky new -padx 2 -pady {{0 2}}")
        if pmPort:
            rE(f"grid {fruns}.pmx -column 0 -row 2 -sticky new -padx 2 -pady {{0 2}}")
        rE(f"grid toolmat -column 2 -row 0 -sticky new -padx 2 -pady 2")
        rE(f"grid {fruns}.material -column 2 -row 1 -rowspan 99 -sticky new -padx 2 -pady {{0 2}}")
        rE(f"grid columnconfigure {fruns} 1 -weight 1")
        rE(f"grid columnconfigure {fportrait} 1 -weight 1")
        rE(f"grid columnconfigure {fportrait} 2 -weight 1")
    else:
        rE(f"grid {fbuttons} -column 0 -row 1 -rowspan 2 -sticky nsew -padx 0 -pady 0")
        rE(f"grid {toolmat} -column 3 -row 0 -sticky nesw")
        rE(f"grid {fruns}.material -column 0 -row 1 -sticky new -padx 2 -pady {{0 2}}")
        rE(f"grid {fruns}.check -column 0 -row 2 -sticky new -padx 2 -pady {{0 2}}")
        if pmPort:
            rE(f"grid {fruns}.pmx -column 0 -row 3 -sticky new -padx 2 -pady {{0 2}}")
        rE(f"grid {fruns} -column 3 -row 1 -rowspan 2 -sticky nsew -padx 1 -pady 1")
    if pmPort:
        rE(f"grid {fruns}.material.cut-mode -column 0 -row 20 -padx {{2 0}} -pady {{0 2}}")
        rE(f"grid {fruns}.material.cut-modeL -column 1 -row 20 -sticky W -pady {{0 2}}")
        rE(f"grid {fruns}.material.gas-pressure -column 0 -row 21 -padx {{2 0}} -pady {{0 2}}")
        rE(f"grid {fruns}.material.gas-pressureL -column 1 -row 21 -sticky W -pady {{0 2}}")

def populate_settings_frame():
    rE(f"grid forget {fsetup}.l")
    rE(f"grid forget {fsetup}.m")
    rE(f"grid forget {fsetup}.r")
    if pVars.orient.get() == 'portrait':
        rE(f"grid {fsetup}.l -column 0 -row 0 -sticky nw -padx {{4 0}} -pady {{4 4}}")
        rE(f"grid {fsetup}.m -column 2 -row 0 -sticky nw -padx {{4 4}} -pady {{4 4}}")
        rE(f"grid {fsetup}.r -column 0 -row 1 -columnspan 3 -sticky nsew -padx {{0 4}} -pady {{4 4}}")
        rE(f"grid columnconfigure {fsetup} 0 -weight 0")
        rE(f"grid columnconfigure {fsetup} 1 -weight 1")
        rE(f"grid columnconfigure {fsetup} 2 -weight 0")
        rE(f"grid columnconfigure {fsetup} 3 -weight 0")
        rE(f"grid columnconfigure {fsetup} 4 -weight 0")
        rE(f"grid rowconfigure {fsetup} 0 -weight 0")
        rE(f"grid rowconfigure {fsetup} 1 -weight 1")
    else:
        rE(f"grid {fsetup}.l -column 0 -row 0 -sticky nw -padx {{4 0}} -pady {{4 4}}")
        rE(f"grid {fsetup}.m -column 2 -row 0 -sticky nw -padx {{4 4}} -pady {{4 4}}")
        rE(f"grid {fsetup}.r -column 4 -row 0 -sticky nsew -padx {{0 4}} -pady {{4 4}}")
        rE(f"grid columnconfigure {fsetup} 0 -weight 0")
        rE(f"grid columnconfigure {fsetup} 1 -weight 1")
        rE(f"grid columnconfigure {fsetup} 2 -weight 0")
        rE(f"grid columnconfigure {fsetup} 3 -weight 1")
        rE(f"grid columnconfigure {fsetup} 4 -weight 9")
        rE(f"grid rowconfigure {fsetup} 0 -weight 1")
        rE(f"grid rowconfigure {fsetup} 1 -weight 0")

def populate_user_buttons():
    if pVars.orient.get() == 'portrait':
        col = 0
        row = 1
    else:
        col = 0
        row = 2
    width = 0
    for n in range(1, maxUserButtons + 1):
        rE(f"grid forget {fbuttons}.button{n}")
        if buttonNames[n]['name'] or buttonCodes[n]['code']:
            rE(f"grid {fbuttons}.button{n} -column {col} -row {row} -sticky new -padx {{2 0}} -pady {{2 0}}")
            if pVars.orient.get() == 'portrait':
                col += 1
                if col == 2:
                    col = 0
                    row += 1
            else:
                row += 1
            if len(buttonNames[n]['name']) > width:
                width = len(buttonNames[n]['name'])
    width = width if width > 10 else 10
    for n in range(1, maxUserButtons + 1):
        rE(f"{fbuttons}.button{n} configure -width {int(width)}")
        rE(f"{fsetup}.r.ubuttons.canvas.frame.name{n} configure -width {width + 2}")

def recreate_widget_list():
    global widgets
    # recreate widget list to move active gcodes and add new widgets
    widget_list_new = []
    for l in widget_list:
        if '.gcodes' in l[2]:
            l = ('code_text', Text, '.info.gcodef.gcodes')
        widget_list_new.append(l)
    widget_list_new.append(('buttonFrame', Frame, fbuttons))
    widget_list_new.append(('convFrame', Frame, '.fconv'))
    widget_list_new.append(('toolFrame', Frame, '.toolconv'))
    widget_list_new.append(('materials', bwidget.ComboBox, f"{fruns}.material.materials"))
    widget_list_new.append(('kerfWidth', Spinbox, f"{fruns}.material.kerf-width"))
    widget_list_new.append(('view_t', Button, '.toolbar.view_t'))
    widgets = nf.Widgets(root_window, *widget_list_new)


##############################################################################
# SETUP                                                                      #
##############################################################################
firstRun = 'valid'
orientStart = False
configPath = os.getcwd()
p2Path = os.path.join(configPath, 'plasmac2')
if os.path.isdir(os.path.join(p2Path, 'lib')):
    extHalPins = {}
    import sys
    libPath = os.path.join(p2Path, 'lib')
    sys.path.append(libPath)
    import tarfile
    from tkinter.colorchooser import askcolor
    from collections import OrderedDict
    from glob import glob as GLOB
    from shutil import copy as COPY
    from shutil import which as WHICH
    from subprocess import Popen, PIPE
    from importlib import reload
    from plasmac import run_from_line as RFL
    ''' comment after next line:
        suppresses unresolved module error in VSCode '''
    import conversational # type: ignore
    import webbrowser
    imagePath = os.path.join(libPath, 'images') # our own images
    imageAxis = os.path.join(BASE, 'share', 'axis', 'images') # images pinched from Axis
    if not os.path.isdir('/tmp/plasmac'):
        os.mkdir('/tmp/plasmac')
    tmpPath = '/tmp/plasmac'
    installType = 'pkg' if os.path.dirname(__file__) == '/bin' else 'rip'
    PREF = plasmacPreferences()
    MATS = plasmacMaterials()
    TEMP = plasmacTempMaterial()
    # for calling tcl code
    rE = root_window.tk.eval
    # set the app icon
    icon = PhotoImage(file=f"{imagePath}/chips_plasma.png")
    rE(f"wm iconphoto . {icon}")
    # set the version
    try:
        with open(os.path.join(p2Path, 'versions.html'), 'r') as inFile:
            for line in inFile:
                if 'v2.' in line:
                    VER = line.split('v')[1].split('<')[0]
                    break
    except:
        VER = '0'
    putPrefs(PREF, 'GUI_OPTIONS', 'Version', VER, str)
    # tk widget variables
    pVars = nf.Variables(root_window,
            ('plasmatool', StringVar),('thcAuto', BooleanVar), ('thcEnable', BooleanVar),
            ('cornerEnable', BooleanVar), ('voidEnable', BooleanVar), ('autoVolts', BooleanVar),
            ('ohmicEnable', BooleanVar), ('meshEnable', BooleanVar), ('ignorArcOk', BooleanVar),
            ('closeText', StringVar), ('closeDialog', BooleanVar), ('kbShortcuts', BooleanVar),
            ('useVirtKB', BooleanVar), ('winSize', StringVar), ('orient', StringVar),
            ('startLine', IntVar), ('rflActive', BooleanVar), ('preRflFile', StringVar),
            ('laserText', StringVar), ('crSpeed', IntVar), ('previewLarge', BooleanVar),
            ('jogSpeed', DoubleVar), ('plasmacMode', IntVar), ('fontSize', StringVar),
            ('guiFont', StringVar), ('codeFont', StringVar), ('coneSize', DoubleVar),
            ('popLocation', StringVar), ('kerfWidth', DoubleVar), ('cutAmps', IntVar),
            ('cutMode', IntVar), ('gasPressure', DoubleVar), ('pmx485Enable', BooleanVar),
            ('preConvFile', IntVar),('matDefault', IntVar), ('jogInhibitOvr', IntVar),
            ('lengthJ', StringVar), ('lengthT', StringVar), ('lengthS', DoubleVar), ('runJ', StringVar),
            ('pierceJ', StringVar), ('pierceT', StringVar), ('pierceS', IntVar), ('cutS', IntVar),
            ('rapidJ', StringVar), ('rapidT', StringVar), ('rapidS', IntVar), ('probeJ', StringVar),
            ('probeT', StringVar),('probeS', IntVar), ('torchJ', StringVar), ('torchT', StringVar),
            ('torchS', IntVar), ('cutJ', StringVar),('cutT', StringVar), ('pausedJ', StringVar),
            ('pausedT', StringVar), ('pausedS', IntVar), ('runT', StringVar), ('runS', IntVar),
            ('probeJ', StringVar), ('probeT', StringVar), ('probeS', IntVar), ('arcT', StringVar),
            ('jogMultiplier', DoubleVar))
    restoreSetup = {}
    pVars.plasmacMode.set(getPrefs(PREF, 'GUI_OPTIONS', 'Mode', 0, int))
    restoreSetup['plasmacMode'] = pVars.plasmacMode.get()
    pVars.winSize.set(getPrefs(PREF, 'GUI_OPTIONS', 'Window size', 'default', str).lower().replace(' ',''))
    restoreSetup['winSize'] = pVars.winSize.get()
    restoreSetup['winLast'] = getPrefs(PREF, 'GUI_OPTIONS', 'Window last', '932x562', str).split('+', 1)[0]
    pVars.orient.set(getPrefs(PREF, 'GUI_OPTIONS', 'Orientation', 'landscape', str).lower().replace(' ',''))
    restoreSetup['orient'] = pVars.orient.get()
    pVars.fontSize.set(getPrefs(PREF, 'GUI_OPTIONS','Font size', '10', str))
    restoreSetup['fontSize'] = pVars.fontSize.get()
    pVars.guiFont.set(getPrefs(PREF, 'GUI_OPTIONS','GUI font', 'Sans', str))
    restoreSetup['guiFont'] = pVars.guiFont.get()
    pVars.codeFont.set(getPrefs(PREF, 'GUI_OPTIONS','Gcode font', 'Sans', str))
    restoreSetup['codeFont'] = pVars.codeFont.get()
    # make some widget names to save typing
    ftop = '.pane.top'
    ftabs = f"{ftop}.tabs"
    fright = f"{ftop}.right"
    stats = f"{fright}.fstats.statistics"
    fmanual = f"{ftabs}.fmanual"
    faxes = f"{fmanual}.axes"
    fjoints = f"{fmanual}.joints"
    fjogf = f"{fmanual}.jogf"
    foverride = f"{fmanual}.override"
    flimitovr = f"{fmanual}.jogf.limitovr"
    fjogiovr = f"{fmanual}.jogf.inhibitovr"
    fplasma = f"{fmanual}.plasma"
    farcv = f"{fplasma}.arcv"
    fthc = f"{fplasma}.thc"
    fmdi = f"{ftabs}.fmdi"
    ft = '.pane.bottom.t'
    fleds = '.pane.bottom.leds'
    fcutrecs = f"{ftabs}.fcutrecs"
    fcrspeed = f"{fcutrecs}.pm"
    fcrbuttons = f"{fcutrecs}.buttons"
    fcrleadin = f"{fcutrecs}.leadins"
    fparam = '.param'
    fsetup = '.setup'
    # spinbox validator
    valspin = root_window.register(validate_spinbox)
    torchEnable = {}
    torchEnable['enabled'] = getPrefs(PREF, 'BUTTONS', 'Torch enabled', 'Torch Enabled', str)
    torchEnable['disabled'] = getPrefs(PREF, 'BUTTONS','Torch disabled', 'Torch Disabled', str)
    pmPort = getPrefs(PREF, 'POWERMAX', 'Port', '', str)
    set_orient_frames()
    recreate_widget_list()
    wLeds = {f"{fleds}.led-arc-ok": [_('Arc OK'), 8], \
             f"{fleds}.led-torch": [_('Torch On'), 8], \
             f"{fleds}.led-breakaway": [_('Break'), 8], \
             f"{fleds}.led-thc-enabled": [_('THC Enabled'), 11], \
             f"{fleds}.led-thc-active": [_('THC Active'), 11], \
             f"{fleds}.led-ohmic": [_('Ohmic Probe'), 11], \
             f"{fleds}.led-float": [_('Float Switch'), 11], \
             f"{fleds}.led-up": [_('Up'), 8], \
             f"{fleds}.led-down": [_('Down'), 8], \
             f"{fleds}.led-corner-locked": [_('Vel Lock'), 8], \
             f"{fleds}.led-void-locked": [_('Void Lock'), 8], \
            }
    read_colors()
    thcFeedRate = round((float(inifile.find('AXIS_Z', 'MAX_VELOCITY')) * float(inifile.find('AXIS_Z', 'OFFSET_AV_RATIO'))) * 60, 3)
    offsetFeedRate = min(float(inifile.find('AXIS_X', 'MAX_VELOCITY')) * 30, \
                         float(inifile.find('AXIS_Y', 'MAX_VELOCITY')) * 30, \
                         float(inifile.find('TRAJ', 'MAX_LINEAR_VELOCITYs') or 100000))
    maxHeight = round(hal.get_value('ini.z.max_limit') - hal.get_value('ini.z.min_limit'), 3)
    unitsPerMm = hal.get_value('halui.machine.units-per-mm')
    isIdle = False
    isIdleHomed = False
    isPaused = False
    isRunning = False
    homeInProgress = False
    getMaterialBusy = False
    materialFileDict = {}
    materialNumList = []
    materialAutoChange = False
    probePressed = False
    probeTimer = 0
    probeButton = ''
    torchPressed = False
    torchTimer = 0
    torchButton = ''
    torchHeight = 0
    cChangeButton = ''
    cutType = 0
    togglePins = {}
    pulsePins = {}
    dualCodes = {}
    currentTool = None
    manualCut = {'state':False, 'feed':vars.jog_speed.get()}
    singleCut = {'state':False, 'G91':False}
    framingState = False
    runState = False
    dryRun = None
    lastMotionMode = None
    laserTimer = 0.0
    laserButtonState = 'laser'
    laserOffsets = {}
    laserOffsets['X'] = getPrefs(PREF, 'LASER_OFFSET', 'X axis', 0, float)
    laserOffsets['Y'] = getPrefs(PREF, 'LASER_OFFSET', 'Y axis', 0, float)
    probeOffsets = {}
    probeOffsets['X'] = getPrefs(PREF, 'OFFSET_PROBING', 'X axis', 0, float)
    probeOffsets['Y'] = getPrefs(PREF, 'OFFSET_PROBING', 'Y axis', 0, float)
    probeOffsets['Delay'] = getPrefs(PREF, 'OFFSET_PROBING', 'Delay', 0, float)
    machineBounds = {}
    machineBounds['X-'] = round(float(inifile.find('AXIS_X', 'MIN_LIMIT')), 6)
    machineBounds['X+'] = round(float(inifile.find('AXIS_X', 'MAX_LIMIT')), 6)
    machineBounds['Y-'] = round(float(inifile.find('AXIS_Y', 'MIN_LIMIT')), 6)
    machineBounds['Y+'] = round(float(inifile.find('AXIS_Y', 'MAX_LIMIT')), 6)
    machineBounds['Z-'] = round(float(inifile.find('AXIS_Z', 'MIN_LIMIT')), 6)
    machineBounds['Z+'] = round(float(inifile.find('AXIS_Z', 'MAX_LIMIT')), 6)
    machineBounds['xLen'] = machineBounds['X+'] - machineBounds['X-']
    machineBounds['yLen'] = machineBounds['Y+'] - machineBounds['Y-']
    machineBounds['zLen'] = machineBounds['Z+'] - machineBounds['Z-']
    startAlignPos = {X:0, Y:0}
    relPos = {X:0, Y:0}
    activeFunction = False
    pausedState = False
    convFirstRun = True
    convViewOptions = {}
    lastViewType = None
    preConvFile = os.path.join(tmpPath, 'pre_conv.ngc')
    keyDelay = {}
    pmx485 = {'exists':False, 'meshMode':False}
    spinBoxes = []
    widgetValues = {}
    statValues = {'length':0, 'pierce':0, 'rapid':0, 'probe':0, 'torch':0, 'cut':0, 'paused':0, 'run':0}
    toolButtons  = ['machine_estop','machine_power','file_open','reload','program_run',
                    'program_step','program_pause','program_stop','program_blockdelete',
                    'program_optpause','view_zoomin','view_zoomout','view_z','view_z2',
                    'view_x','view_y','view_y2','view_p','view_t','rotate','clear_plot']
    matButtons   = ['delete','new','reload','save']
    maxUserButtons = 20
    # allow right-click to select start from line
    o.bind('<Button-3>', rClicker, '+')
    # hijacked functions from axis.py
    #   vupdate
    #   get_coordinate_font
    #   install_help
    #   prompt_touchoff
    # monkeypatched functions from axis.py
    o.get_view = get_view
    filter_program_old = filter_program
    filter_program = filter_program_new
    # monkeypatched functions from glcanon.py
    o.draw_grid = draw_grid
    o.posstrs = posstrs
    # tcl called python functions hijacked from axis.py
    TclCommands.reload_file = reload_file
    TclCommands.task_run = task_run
    TclCommands.task_run_line = task_run_line
    TclCommands.open_file_name = open_file_name
    TclCommands.set_view_p = set_view_p
    TclCommands.set_view_z = set_view_z
    TclCommands.get_jog_speed = get_jog_speed
    TclCommands.send_mdi_command = send_mdi_command
    # tcl functions hijacked from axis.tcl
    TclCommands.update_title = update_title
    TclCommands.update_jog_slider_vel = update_jog_slider_vel
    TclCommands.set_pane_minsize = set_pane_minsize
    # new functions
    TclCommands.set_view_t = set_view_t
    TclCommands.param_changed = param_changed
    TclCommands.material_changed = material_changed
    TclCommands.save_material_clicked = save_material_clicked
    TclCommands.reload_material_clicked = reload_material_clicked
    TclCommands.new_material_clicked = new_material_clicked
    TclCommands.delete_material_clicked = delete_material_clicked
    TclCommands.change_default_material = change_default_material
    TclCommands.save_param_clicked = save_param_clicked
    TclCommands.load_param_clicked = load_param_clicked
    TclCommands.backup_clicked = backup_clicked
    TclCommands.save_setup_clicked = save_setup_clicked
    TclCommands.load_setup_clicked = load_setup_clicked
    TclCommands.clear_program = clear_program
    TclCommands.preview_toggle = preview_toggle
    TclCommands.setup_toggle = setup_toggle
    TclCommands.param_toggle = param_toggle
    TclCommands.conv_toggle = conv_toggle
    TclCommands.thc_enable_toggled = thc_enable_toggled
    TclCommands.corner_enable_toggled = corner_enable_toggled
    TclCommands.void_enable_toggled = void_enable_toggled
    TclCommands.auto_volts_toggled = auto_volts_toggled
    TclCommands.ohmic_enable_toggled = ohmic_enable_toggled
    TclCommands.ignore_arc_ok_toggled = ignore_arc_ok_toggled
    TclCommands.close_window = close_window
    TclCommands.set_window_size = set_window_size
    TclCommands.set_orientation = set_orientation
    TclCommands.button_action = button_action
    TclCommands.mode_changed = mode_changed
    TclCommands.font_changed = font_changed
    TclCommands.cone_size_changed = cone_size_changed
    TclCommands.height_raise = height_raise
    TclCommands.height_lower = height_lower
    TclCommands.height_reset = height_reset
    TclCommands.torch_enable = torch_enable
    TclCommands.joint_mode_switch = joint_mode_switch
    TclCommands.ja_button_activated = ja_button_activated
    TclCommands.manual_cut = manual_cut
    TclCommands.touch_off_xy = touch_off_xy
    TclCommands.laser_button_toggled = laser_button_toggled
    TclCommands.cut_rec_motion = cut_rec_motion
    TclCommands.cut_rec_default_changed = cut_rec_default_changed
    TclCommands.cut_rec_slider_changed = cut_rec_slider_changed
    TclCommands.cut_rec_move = cut_rec_move
    TclCommands.cut_rec_cancel = cut_rec_cancel
    TclCommands.pmx485_mode_changed = pmx485_mode_changed
    TclCommands.pmx485_pressure_changed = pmx485_pressure_changed
    TclCommands.pmx485_enable_toggled = pmx485_enable_toggled
    TclCommands.pmx485_mesh_enable_toggled = pmx485_mesh_enable_toggled
    TclCommands.reset_all_stats = reset_all_stats
    TclCommands.set_peripheral_offsets = set_peripheral_offsets
    TclCommands.kb_shortcuts_changed = kb_shortcuts_changed
    TclCommands.virtual_kb_changed = virtual_kb_changed
    TclCommands.key_pressed = key_pressed
    TclCommands.key_released = key_released
    TclCommands.color_set = color_set
    TclCommands.user_manual = user_manual
    commands = TclCommands(root_window)
    # set up the fonts
    lastFontSize = pVars.fontSize.get()
    fontsCode = []
    fontsGui = []
    for font in tuple_from_tcl_string(rE('font families')):
        if 'caps' in font.lower() or 'slanted' in font.lower() or 'light' in font.lower() \
           or 'dingbat' in font.lower() or 'symbols' in font.lower():
            pass
        else:
            if 'mono' in font.lower() or 'courier' in font.lower():
                if font not in fontsCode:
                    fontsCode.append(font)
            else:
                if font not in fontsGui:
                    fontsGui.append(font)
    if pVars.guiFont.get() not in fontsCode:
        fontsCode.append(pVars.guiFont.get())
    if pVars.guiFont.get() not in fontsGui:
        fontsGui.append(pVars.guiFont.get())
    rE(f"font create fontGui -family {{{pVars.guiFont.get()}}} -size {pVars.fontSize.get()}")
    rE(f"font create fontArc -family {{{pVars.guiFont.get()}}} -size {int(pVars.fontSize.get())*3}")
    rE(f"font create fontCode -family {{{pVars.codeFont.get()}}} -size {pVars.fontSize.get()}")
    update_check()


##############################################################################
# GUI ALTERATIONS AND ADDITIONS                                              #
##############################################################################
    # keep tab label sizes the same
    rE(f"{ftabs} configure -homogeneous True")
    rE(f"{fright} configure -homogeneous True")

    # reduce margins on manual/mdi tabs
    rE(f"{ftabs} configure -internalborderwidth 1")
    rE(f"grid columnconfigure {fmanual} 0 -weight 1")
    rE(f"grid columnconfigure {fmanual} 99 -weight 0")

    # reduce margins on preview tabs
    rE(f"{fright} configure -internalborderwidth 1")

    # hide some existing widgets
    rE('pack forget .toolbar.rule9')
    rE('pack forget .toolbar.view_zoomin')
    rE('pack forget .toolbar.view_zoomout')
    rE('pack forget .toolbar.view_z')
    rE('pack forget .toolbar.view_z2')
    rE('pack forget .toolbar.view_x')
    rE('pack forget .toolbar.view_y')
    rE('pack forget .toolbar.view_y2')
    rE('pack forget .toolbar.view_p')
    rE('pack forget .toolbar.rotate')
    rE('pack forget .toolbar.rule12')
    rE('pack forget .toolbar.clear_plot')
    rE(f"grid forget {fmanual}.axis")
    rE(f"grid forget {fmanual}.jogf")
    rE(f"grid forget {fmanual}.space1")
    rE(f"grid forget {fmanual}.spindlel")
    rE(f"grid forget {fmanual}.spindlef")
    rE(f"grid forget {fmanual}.space2")
    rE(f"grid forget {fmanual}.coolant")
    rE(f"grid forget {fmanual}.mist")
    rE(f"grid forget {fmanual}.flood")
    rE(f"pack forget {ftop}.jogspeed.l1")
    rE(f"grid forget {ftop}.spinoverride")
    rE(f"grid forget {ftop}.maxvel")

    # modify the toolbar
    rE('vrule .toolbar.rule10')
    rE('vrule .toolbar.rule11')
    rE('pack .toolbar.rule9 -side left -padx {66 4} -pady 4 -fill y')
    rE('pack .toolbar.clear_plot -side left')
    rE('pack .toolbar.rule10 -side left -padx 4 -pady 4 -fill y')
    rE('pack .toolbar.view_z -side left')
    rE('pack .toolbar.view_p -side left')
    rE(f".toolbar.view_p configure -image {rE(f'load_image {imagePath}/view_p')}")
    rE(f".toolbar.view_z configure -image {rE(f'load_image {imagePath}/view_z')}")
    rE(f"Button .toolbar.view_t -image {rE(f'load_image {imagePath}/view_t')} -command set_view_t -relief link -takefocus 0")
    rE('pack .toolbar.view_t -side left')
    rE('pack .toolbar.rule11 -side left -padx 4 -pady 4 -fill y')
    rE('pack .toolbar.view_zoomout -side left')
    rE('pack .toolbar.view_zoomin -side left')
    rE('pack .toolbar.rule12 -side left -padx 4 -pady 4 -fill y')
    rE('pack .toolbar.rotate -side left')
    rE('.toolbar.rotate configure -takefocus 0')

    # new setup toolbar
    # create widgets
    rE('frame .toolsetup -borderwidth 1 -relief raised')
    rE(f"Button .toolsetup.save -command save_setup_clicked -width 8 -takefocus 0 -text {{{_('Save All')}}} -padx 0")
    rE(f"Button .toolsetup.reload -command load_setup_clicked -width 8 -takefocus 0 -text {{{_('Reload')}}} -padx 0")
    rE(f"Button .toolsetup.bkp -command backup_clicked -width 8 -takefocus 0 -text {{{_('Backup')}}} -padx 0")
    rE(f"Button .toolsetup.close -command {{setup_toggle 0}} -width 8 -takefocus 0 -text {{{_('Close')}}} -padx 0")
    # populate the tool bar
    rE('pack .toolsetup.save -side left')
    rE('pack .toolsetup.reload -side left -padx {8 0}')
    rE('pack .toolsetup.bkp -side left -padx {8 0}')
    rE('pack .toolsetup.close -side left -padx {16 0}')
    rE('grid forget .toolsetup')

    # new parameters toolbar
    # create widgets
    rE('frame .toolparam -borderwidth 1 -relief raised')
    rE(f"Button .toolparam.save -command save_param_clicked -width 8 -takefocus 0 -text {{{_('Save All')}}} -padx 0")
    rE(f"Button .toolparam.reload -command load_param_clicked -width 8 -takefocus 0 -text {{{_('Reload')}}} -padx 0")
    rE(f"Button .toolparam.close -command {{param_toggle 0}} -width 8 -takefocus 0 -text {{{_('Close')}}} -padx 0")
    # populate the tool bar
    rE('pack .toolparam.save -side left')
    rE('pack .toolparam.reload -side left -padx {8 0}')
    rE('pack .toolparam.close -side left -padx {16 0}')
    rE('grid forget .toolparam')

    # new statistics page
    rE(f"{fright} insert end stats -text {{{_('Statistics')}}}")
    rE(f"{fright}.fstats configure -bd 2")
    rE(f"labelframe {stats} -relief groove")
    rE(f"label {stats}.itemL -text {{{_('Item')}}} -width 13 -anchor e")
    rE(f"label {stats}.jobL -text {{{_('Job')}}} -width 9 -anchor e")
    rE(f"label {stats}.totalL -text {{{_('Total')}}} -width 9 -anchor e")
    rE(f"label {stats}.lengthL -text {{{_('Cut Length')}}} -width 13 -anchor e")
    rE(f"label {stats}.lengthJ -textvariable lengthJ -width 9 -anchor e")
    rE(f"label {stats}.lengthT -textvariable lengthT -width 9 -anchor e")
    rE(f"button {stats}.lengthB -command {{reset_all_stats length}} -text {{{_('Reset')}}} -pady 0")
    rE(f"label {stats}.pierceL -text {{{_('Torch Starts')}}} -width 13 -anchor e")
    rE(f"label {stats}.pierceJ -textvariable pierceJ -width 9 -anchor e")
    rE(f"label {stats}.pierceT -textvariable pierceT -width 9 -anchor e")
    rE(f"button {stats}.pierceB -command {{reset_all_stats pierce}} -text {{{_('Reset')}}} -pady 0")
    rE(f"label {stats}.rapidL -text {{{_('Rapid Time')}}} -width 13 -anchor e")
    rE(f"label {stats}.rapidJ -textvariable rapidJ -width 9 -anchor e")
    rE(f"label {stats}.rapidT -textvariable rapidT -width 9 -anchor e")
    rE(f"button {stats}.rapidB -command {{reset_all_stats rapid}} -text {{{_('Reset')}}} -pady 0")
    rE(f"label {stats}.probeL -text {{{_('Probe Time')}}} -width 13 -anchor e")
    rE(f"label {stats}.probeJ -textvariable probeJ -width 9 -anchor e")
    rE(f"label {stats}.probeT -textvariable probeT -width 9 -anchor e")
    rE(f"button {stats}.probeB -command {{reset_all_stats probe}} -text {{{_('Reset')}}} -pady 0")
    rE(f"label {stats}.torchL -text {{{_('Torch On Time')}}} -width 13 -anchor e")
    rE(f"label {stats}.torchJ -textvariable torchJ -width 9 -anchor e")
    rE(f"label {stats}.torchT -textvariable torchT -width 9 -anchor e")
    rE(f"button {stats}.torchB -command {{reset_all_stats torch}} -text {{{_('Reset')}}} -pady 0")
    rE(f"label {stats}.cutL -text {{{_('Cutting Time')}}} -width 13 -anchor e")
    rE(f"label {stats}.cutJ -textvariable cutJ -width 9 -anchor e")
    rE(f"label {stats}.cutT -textvariable cutT -width 9 -anchor e")
    rE(f"button {stats}.cutB -command {{reset_all_stats cut}} -text {{{_('Reset')}}} -pady 0")
    rE(f"label {stats}.pausedL -text {{{_('Paused Time')}}} -width 13 -anchor e")
    rE(f"label {stats}.pausedJ -textvariable pausedJ -width 9 -anchor e")
    rE(f"label {stats}.pausedT -textvariable pausedT -width 9 -anchor e")
    rE(f"button {stats}.pausedB -command {{reset_all_stats paused}} -text {{{_('Reset')}}} -pady 0")
    rE(f"label {stats}.runL -text {{{_('Total Run Time')}}} -width 13 -anchor e")
    rE(f"label {stats}.runJ -textvariable runJ -width 9 -anchor e")
    rE(f"label {stats}.runT -textvariable runT -width 9 -anchor e")
    rE(f"button {stats}.runB -command {{reset_all_stats run}} -text {{{_('Reset')}}} -pady 0")
    rE(f"label {stats}.arcL -text {{{_('Arc On Time')}}} -width 13 -anchor e")
    rE(f"label {stats}.arcT -textvariable arcT -width 10 -anchor e")
    rE(f"grid {stats}.itemL -column 0 -row 0")
    rE(f"grid {stats}.jobL -column 1 -row 0")
    rE(f"grid {stats}.totalL -column 2 -row 0")
    rE(f"grid {stats}.lengthL -column 0 -row 1 -pady {{8 0}}")
    rE(f"grid {stats}.lengthJ -column 1 -row 1 -pady {{8 0}}")
    rE(f"grid {stats}.lengthT -column 2 -row 1 -pady {{8 0}}")
    rE(f"grid {stats}.lengthB -column 3 -row 1 -padx {{16 0}} -pady {{8 0}}")
    rE(f"grid {stats}.pierceL -column 0 -row 2 -pady {{8 0}}")
    rE(f"grid {stats}.pierceJ -column 1 -row 2 -pady {{8 0}}")
    rE(f"grid {stats}.pierceT -column 2 -row 2 -pady {{8 0}}")
    rE(f"grid {stats}.pierceB -column 3 -row 2 -padx {{16 0}} -pady {{8 0}}")
    rE(f"grid {stats}.rapidL -column 0 -row 3 -pady {{8 0}}")
    rE(f"grid {stats}.rapidJ -column 1 -row 3 -pady {{8 0}}")
    rE(f"grid {stats}.rapidT -column 2 -row 3 -pady {{8 0}}")
    rE(f"grid {stats}.rapidB -column 3 -row 3 -padx {{16 0}} -pady {{8 0}}")
    rE(f"grid {stats}.probeL -column 0 -row 4 -pady {{8 0}}")
    rE(f"grid {stats}.probeJ -column 1 -row 4 -pady {{8 0}}")
    rE(f"grid {stats}.probeT -column 2 -row 4 -pady {{8 0}}")
    rE(f"grid {stats}.probeB -column 3 -row 4 -padx {{16 0}} -pady {{8 0}}")
    rE(f"grid {stats}.torchL -column 0 -row 5 -pady {{8 0}}")
    rE(f"grid {stats}.torchJ -column 1 -row 5 -pady {{8 0}}")
    rE(f"grid {stats}.torchT -column 2 -row 5 -pady {{8 0}}")
    rE(f"grid {stats}.torchB -column 3 -row 5 -padx {{16 0}} -pady {{8 0}}")
    rE(f"grid {stats}.cutL -column 0 -row 6 -pady {{8 0}}")
    rE(f"grid {stats}.cutJ -column 1 -row 6 -pady {{8 0}}")
    rE(f"grid {stats}.cutT -column 2 -row 6 -pady {{8 0}}")
    rE(f"grid {stats}.cutB -column 3 -row 6 -padx {{16 0}} -pady {{8 0}}")
    rE(f"grid {stats}.pausedL -column 0 -row 7 -pady {{8 0}}")
    rE(f"grid {stats}.pausedJ -column 1 -row 7 -pady {{8 0}}")
    rE(f"grid {stats}.pausedT -column 2 -row 7 -pady {{8 0}}")
    rE(f"grid {stats}.pausedB -column 3 -row 7 -padx {{16 0}} -pady {{8 0}}")
    rE(f"grid {stats}.runL -column 0 -row 8 -pady {{8 0}}")
    rE(f"grid {stats}.runJ -column 1 -row 8 -pady {{8 0}}")
    rE(f"grid {stats}.runT -column 2 -row 8 -pady {{8 0}}")
    rE(f"grid {stats}.runB -column 3 -row 8 -padx {{16 0}} -pady {{8 0}}")
    if pmPort:
        rE(f"grid {stats}.arcL -column 0 -row 9 -pady {{8 0}}")
        rE(f"grid {stats}.arcT -column 2 -row 9 -pady {{8 0}}")
    rE(f"pack {stats} -fill both -expand True -padx 2 -pady 2")

    # destroy existing axes and joints
    rE(f"destroy {faxes}")
    rE(f"destroy {fjoints}")
    # create widgets
    rE(f"labelframe {faxes} -text {{{_('Axis:')}}} -relief flat -bd 0")
    rE(f"labelframe {fjoints} -text {{{_('Joint:')}}} -relief flat -bd 0")
    # make joints radiobuttons
    for number in range(0,linuxcnc.MAX_JOINTS):
        ja_button_setup(f"{fjoints}.joint{number}", number, number)
    # populate joints frame
    count = 0
    for row in range(0,2):
        for column in range(0,5):
            if count == jointcount: break
            pad = '{0 0}' if column == 0 else '{8 0}'
            rE(f"grid {fjoints}.joint{count} -row {row} -column {column} -padx {pad}")
            count += 1
    # make axis radiobuttons
    for letter in 'xyzabcuvw':
        ja_button_setup(f"{faxes}.axis{letter}", letter, letter.upper())
    # populate the axes frame
    count = 0
    letters = 'xyzabcuvw'
    first_axis = ''
    for row in range(0,2):
        for column in range(0,5):
            if letters[count] in trajcoordinates:
                if first_axis == '':
                    first_axis = letters[count]
                pad = '{0 0}' if column == 0 else '{8 0}'
                rE(f"grid {faxes}.axis{letters[count]} -row {row} -column {column} -padx {pad}")
            count += 1
            if count == 9: break

    # rework the jogf frame
    rE(f"destroy {fjogf}")
    # create the widgets
    rE(f"frame {fjogf}")
    rE(f"labelframe {fjogf}.jog -text {{{_('Jog:')}}} -relief flat -bd 0")
    rE(f"button {fjogf}.jog.jogminus -command {{if ![is_continuous] {{jog_minus 1}}}} -height 1 -width 1 -text -")
    rE(f"button {fjogf}.jog.jogplus -command {{if ![is_continuous] {{jog_plus 1}}}} -height 1 -width 1 -text +")
    rE(f"combobox {fjogf}.jog.jogincr -editable 0 -textvariable jogincrement -value {{{_('Continuous')}}} -width 10")
    rE(f"{fjogf}.jog.jogincr list insert end {{{_('Continuous')}}}")
    if increments:
        for inc in increments:
            rE(f"{fjogf}.jog.jogincr list insert end {inc}")
    rE(f"labelframe {fjogf}.zerohome -text {{{_('Zero:')}}} -relief flat -bd 0")
    rE(f"button {fjogf}.zerohome.home -command home_joint -height 1 -width 8 -padx 0")
    rE(f"setup_widget_accel {fjogf}.zerohome.home {{{_('Home Axis')}}}")
    rE(f"button {fjogf}.zerohome.zero -command touch_off_system -height 1 -width 5 -padx 0")
    rE(f"setup_widget_accel {fjogf}.zerohome.zero X0")
    rE(f"button {fjogf}.zerohome.zeroxy -height 1 -width 5 -text {{{_('X0Y0')}}} -padx 0")
    rE(f"button {fjogf}.zerohome.laser -height 1 -width 5 -textvariable laserText -padx 0")
    rE(f"button {fjogf}.zerohome.tooltouch") # unused... kept for tk hierarchy
    # widget bindings
    rE(f"bind {fjogf}.jog.jogminus <Button-1> {{if [is_continuous] {{jog_minus}}}}")
    rE(f"bind {fjogf}.jog.jogminus <ButtonRelease-1> {{if [is_continuous] {{jog_stop}}}}")
    rE(f"bind {fjogf}.jog.jogplus <Button-1> {{if [is_continuous] {{jog_plus}}}}")
    rE(f"bind {fjogf}.jog.jogplus <ButtonRelease-1> {{if [is_continuous] {{jog_stop}}}}")
    rE(f"bind {fjogf}.zerohome.zeroxy <Button-1> {{touch_off_xy 1 0 0}}")
    rE(f"bind {fjogf}.zerohome.zeroxy <ButtonRelease-1> {{touch_off_xy 0 0 0}}")
    rE(f"bind {fjogf}.zerohome.laser <Button-1> {{laser_button_toggled 1 1}}")
    rE(f"bind {fjogf}.zerohome.laser <ButtonRelease-1> {{laser_button_toggled 0 1}}")
    # populate the frame
    rE(f"grid {fjogf}.jog.jogminus -row 0 -column 0 -sticky nsew")
    rE(f"grid {fjogf}.jog.jogincr -row 0 -column 1 -sticky nsew -padx 8")
    rE(f"grid {fjogf}.jog.jogplus -row 0 -column 2 -sticky nsew")
    rE(f"grid {fjogf}.jog -row 0 -column 0 -sticky ew")
    rE(f"grid {fjogf}.zerohome.home -row 0 -column 0 -padx {{0 4}}")
    rE(f"grid {fjogf}.zerohome.zero -row 0 -column 1 -padx {{0 4}}")
    rE(f"grid {fjogf}.zerohome.zeroxy -row 0 -column 2 -padx {{0 4}}")
    rE(f"grid {fjogf}.zerohome -row 1 -column 0 -pady {{2 0}} -sticky w")
    rE(f"grid {fjogf} -column 0 -row 1 -padx 2 -pady {{0 0}} -sticky w")

    # make home button a home all button if required
    if homing_order_defined:
        if ja_name.startswith('A'):
            hbName = 'axes'
        else:
            hbName ='joints'
        widgets.homebutton.configure(text = _('Home All'), command = 'home_all_joints')
    else:
        pre = _('Home')
        widgets.homebutton.configure(text = f"{pre} X")

    # add a spacer to keep the plasma widgets at the bottom
    rE(f"vspace {fmanual}.vspace -height 1")
    rE(f"grid {fmanual}.vspace -column 0 -row 2 -pady 0 -sticky nsew")
    rE(f"grid rowconfigure {fmanual} 2 -weight 99")

    # new limit switch override frame
    rE(f"frame {flimitovr} -bd 0")
    rE(f"button {fjogf}.override") # dummy button to placate original axis code
    rE(f"checkbutton {flimitovr}.button -command toggle_override_limits -width 2 -anchor w -indicatoron 0")
    rE(f"label {flimitovr}.label -text {{{_('Override Limits')}}} -anchor center")
    # populate the frame
    rE(f"grid {flimitovr}.button -column 0 -row 1 -sticky nsw")
    rE(f"grid {flimitovr}.label -column 1 -row 1 -sticky w -padx {{4 0}}")
    rE(f"grid rowconfigure {flimitovr} 0 -weight 1")
    rE(f"grid rowconfigure {flimitovr} 1 -weight 0")
    rE(f"grid rowconfigure {flimitovr} 2 -weight 1")

    # new jog inhibit override frame
    rE(f"frame {fjogiovr} -bd 0")
    rE(f"checkbutton {fjogiovr}.button -variable jogInhibitOvr -width 2 -anchor w -indicatoron 0")
    rE(f"label {fjogiovr}.label -text {{{_('Jog Inhibit Override')}}} -anchor center")
    # populate the frame
    rE(f"grid {fjogiovr}.button -column 0 -row 1 -sticky nsw")
    rE(f"grid {fjogiovr}.label -column 1 -row 1 -sticky w -padx {{4 0}}")
    rE(f"grid rowconfigure {fjogiovr} 0 -weight 1")
    rE(f"grid rowconfigure {fjogiovr} 1 -weight 0")
    rE(f"grid rowconfigure {fjogiovr} 2 -weight 1")

    # new height override frame
    # create the widgets
    rE(f"labelframe {foverride} -text {{{_('THC Height Override:')}}} -relief flat -bd 0")
    rE(f"Button {foverride}.lower -text - -width 1 -takefocus 0")
    rE(f"Button {foverride}.raise -text + -width 1 -takefocus 0")
    rE(f"label {foverride}.height-override -text {torchHeight:.1f}V -width 20")
    rE(f"Button {foverride}.reset -text {{{_('Reset')}}} -width 1 -takefocus 0 -width 3")
    # populate the frame
    rE(f"grid {foverride}.lower -column 0 -row 0")
    rE(f"grid {foverride}.raise -column 1 -row 0 -padx {{4 0}}")
    rE(f"grid {foverride}.height-override -column 2 -row 0 -sticky ew")
    rE(f"grid {foverride}.reset -column 3 -row 0")
    rE(f"grid {foverride} -column 0 -row 3 -padx 2 -pady {{0 4}} -sticky ew")
    rE(f"grid columnconfigure {foverride} 2 -weight 1")
    # widget bindings
    rE(f"bind {foverride}.lower <ButtonPress-1> height_lower")
    rE(f"bind {foverride}.raise <ButtonPress-1> height_raise")
    rE(f"bind {foverride}.reset <ButtonPress-1> height_reset")

    # new plasma frame
    # create the widgets
    rE(f"frame {fplasma} -bd 0")
    rE(f"label {fplasma}.arcvl -anchor nw -text {{{_('Arc Voltage:')}}}")
    rE(f"label {fplasma}.arc-voltage -anchor se -width 4")
    rE(f"checkbutton {fplasma}.thc -variable thcEnable -command thc_enable_toggled -width 2 -anchor w -indicatoron 0")
    rE(f"label {fplasma}.thcL -anchor nw -text {{{_('THC Enable')}}} -width 11")
    rE(f"checkbutton {fplasma}.vel -variable cornerEnable -command corner_enable_toggled -width 2 -anchor w -indicatoron 0")
    rE(f"label {fplasma}.velL -anchor nw -text {{{_('Velocity Lock')}}} -width 11")
    rE(f"checkbutton {fplasma}.void -variable voidEnable -command void_enable_toggled -width 2 -anchor w -indicatoron 0")
    rE(f"label {fplasma}.voidL -anchor nw -text {{{_('Void Lock')}}} -width 11")
    # populate the frame
    rE(f"grid {fplasma}.thc -column 2 -row 0 -sticky e -pady {{0 2}}")
    rE(f"grid {fplasma}.thcL -column 3 -row 0 -sticky e -pady {{0 2}}")
    rE(f"grid {fplasma}.vel -column 2 -row 1 -sticky e -pady {{0 2}}")
    rE(f"grid {fplasma}.velL -column 3 -row 1 -sticky e -pady {{0 2}}")
    rE(f"grid {fplasma}.void -column 2 -row 2 -sticky e -pady {{0 2}}")
    rE(f"grid {fplasma}.voidL -column 3 -row 2 -sticky e -pady {{0 2}}")
    rE(f"grid {fplasma} -column 0 -row 4 -padx 2 -pady {{0 0}} -sticky ew")
    rE(f"grid columnconfigure {fplasma} 1 -weight 1")
    # shorten some text for nicer layout.
    rE(f".pane.top.rapidoverride.l configure -text {{{_('Rapid Ovr:')}}}")
    rE(f".pane.top.feedoverride.l configure -text {{{_('Feed Ovr:')}}}")

    # hide bottom pane until modified
    rE('pack forget .pane.bottom.t.text')
    rE('pack forget .pane.bottom.t.sb')

    # new led frame
    # create the widgets
    rE(f"frame {fleds} -relief flat -bd 0")
    for led in wLeds:
        # leds
        rE(f"canvas {led} -width 20 -height 20 -bd 2 -relief ridge")
        rE(f"{led} create rectangle {{1 1 22 22}} -width 0")
        # labels
        rE(f"label {led}L -text {{{wLeds[led][0]}}} -anchor w -width {wLeds[led][1]}")
    # populate the frame
    rE(f"grid {fleds}.led-arc-ok -column 0 -row 0 -padx {{4 0}} -pady {{4 0}} -sticky EW")
    rE(f"grid {fleds}.led-arc-okL -column 1 -row 0 -padx {{0 0}} -pady {{4 0}} -sticky W")
    rE(f"grid {fleds}.led-torch -column 0 -row 1 -padx {{4 0}} -pady {{4 0}}")
    rE(f"grid {fleds}.led-torchL -column 1 -row 1 -padx {{0 0}} -pady {{4 0}} -sticky W")
    rE(f"grid {fleds}.led-breakaway -column 0 -row 3 -padx {{4 0}} -pady {{4 0}}")
    rE(f"grid {fleds}.led-breakawayL -column 1 -row 3 -padx {{0 0}} -pady {{4 0}} -sticky W")
    rE(f"grid {fleds}.led-thc-enabled -column 2 -row 0 -padx {{0 0}} -pady {{4 0}}")
    rE(f"grid {fleds}.led-thc-enabledL -column 3 -row 0 -padx {{0 0}} -pady {{4 0}} -sticky W")
    rE(f"grid {fleds}.led-thc-active -column 2 -row 1 -padx {{0 0}} -pady {{4 0}}")
    rE(f"grid {fleds}.led-thc-activeL -column 3 -row 1 -padx {{0 0}} -pady {{4 0}} -sticky W")
    rE(f"grid {fleds}.led-ohmic -column 2 -row 2 -padx {{0 0}} -pady {{4 0}}")
    rE(f"grid {fleds}.led-ohmicL -column 3 -row 2 -padx {{0 0}} -pady {{4 0}} -sticky W")
    rE(f"grid {fleds}.led-float -column 2 -row 3 -padx {{0 0}} -pady {{4 0}}")
    rE(f"grid {fleds}.led-floatL -column 3 -row 3 -padx {{0 0}} -pady {{4 0}} -sticky W")
    rE(f"grid {fleds}.led-up -column 4 -row 0 -padx {{4 0}} -pady {{4 0}}")
    rE(f"grid {fleds}.led-upL -column 5 -row 0 -padx {{0 0}} -pady {{4 0}} -sticky W")
    rE(f"grid {fleds}.led-down -column 4 -row 1 -padx {{4 0}} -pady {{4 0}}")
    rE(f"grid {fleds}.led-downL -column 5 -row 1 -padx {{0 0}} -pady {{4 0}} -sticky W")
    rE(f"grid {fleds}.led-corner-locked -column 4 -row 2 -padx {{4 0}} -pady {{4 0}}")
    rE(f"grid {fleds}.led-corner-lockedL -column 5 -row 2 -padx {{0 0}} -pady {{4 0}} -sticky W")

    # rename keyboard shortcut window
    rE('wm title .keys {plasmac2 Keyboard Shortcuts}')
    # new keypad shortcut window
    rE('toplevel .keyp')
    rE('bind .keyp <Key-Return> {wm withdraw .keyp}')
    rE('bind .keyp <Key-Escape> {wm withdraw .keyp}')
    rE('frame .keyp.text')
    rE('button .keyp.ok -command {wm wi .keyp} -default active -padx 0 -pady 0 -width 10')
    rE('setup_widget_accel .keyp.ok OK')
    rE('pack .keyp.text -expand 1 -fill y')
    rE('pack .keyp.ok')
    rE('wm title .keyp {plasmac2 Keypad Shortcuts}')
    rE('wm iconname .keyp ')
    rE('wm resiz .keyp 0 0')
    rE('wm minsize .keyp 1 1')
    rE('wm protocol .keyp WM_DELETE_WINDOW {wm wi .keys}')
    rE('wm withdraw .keyp')

    # menu alterations
    # delete existing
    rE('.menu.file delete last')
    rE('.menu.view delete 20')
    rE('.menu.view delete 8')
    rE('.menu.view delete 7')
    rE('.menu.view delete 6')
    rE('.menu delete last')
    # add new menu items
    rE('.menu.file insert 2 command -command clear_program')
    rE(f"setup_menu_accel .menu.file 2 {{{_('_Clear')}}}")
    rE('.menu.file add command -command close_window')
    rE(f"setup_menu_accel .menu.file end {{{_('_Quit')}}}")
    rE('.menu.view insert 0 checkbutton -variable previewLarge -command preview_toggle')
    rE(f"setup_menu_accel .menu.view 0 {{{_('Large Pre_view')}}}")
    rE('.menu.view insert 1 separator')
    rE('.menu add command -command {conv_toggle 1}')
    rE(f"setup_menu_accel .menu end {{{_('_Conversational')}}}")
    rE('.menu add command -command {param_toggle 1}')
    rE(f"setup_menu_accel .menu end {{{_('_Parameters')}}}")
    rE('.menu add command -command {setup_toggle 1}')
    rE(f"setup_menu_accel .menu end {{{_('_Setup')}}}")
    rE('.menu add cascade -menu .menu.help')
    rE(f"setup_menu_accel .menu end {{{_('_Help')}}}")
    rE('.menu.help delete 0 1')
    rE('.menu.help add command -command {wm transient .about .;wm deiconify .about;show_all .about.message;focus .about.ok}')
    rE(f"setup_menu_accel .menu.help end {{{_('About AXIS')}}}")
    rE('.menu.help add separator')
    rE('.menu.help add command -command user_manual')
    rE(f"setup_menu_accel .menu.help end {{{_('User Manual')}}}")

    # rework the status bar
    rE(f"grid forget {ftop}.gcodel")
    rE(f"grid forget {ftop}.gcodes")
    rE('pack forget .info.tool')
    rE('.info.task_state configure -width 6')
    rE('.info.tool configure -width 10 -textvariable plasmatool')
    rE('.info.position configure -width 25')
    rE('grid .info -columnspan 4')
    rE('labelframe .info.gcodef -relief sunken -bd 2')
    rE('label .info.gcodef.gcodel -bd 0')
    rE('setup_widget_accel .info.gcodef.gcodel {Active Codes:}')
    rE('text .info.gcodef.gcodes -height 1 -width 10 -relief flat -bd 0 -pady 0')
    rE('.info.gcodef.gcodes configure -undo 0 -wrap word')
    rE('pack .info.tool -side left')
    rE('pack .info.gcodef.gcodel -side left')
    rE('pack .info.gcodef.gcodes -side left -fill x -expand 1')
    rE('pack .info.gcodef -side left -fill x -expand 1')

    # move existing pane to column 1
    rE('grid .pane -column 1 -row 1 -rowspan 2 -sticky nsew')
    # set pane column growth
    rE('grid columnconfigure .pane.top 0 -weight 1')
    rE('grid columnconfigure .pane.top 1 -weight 255')
    # set root window column growth
    rE('grid columnconfigure . 0 -weight 0')
    rE('grid columnconfigure . 1 -weight 1')
    rE('grid columnconfigure . 2 -weight 0')

    # new cut recovery tab
    # create the widgets
    rE(f"{ftabs} insert end cutrecs -text {{{_('Cut Recovery')}}}")
    rE(f"{fcutrecs} configure -borderwidth 2")
    rE(f"{ftabs} delete cutrecs 0")
    rE(f"labelframe {fcrspeed} -text {{{_('Speed:')}}} -relief flat")
    rE(f"Button {fcrspeed}.reverse -text {{{_('Rev')}}} -takefocus 0 -width 3")
    rE(f"bind {fcrspeed}.reverse <Button-1> {{cut_rec_motion -1}}")
    rE(f"bind {fcrspeed}.reverse <ButtonRelease-1> {{cut_rec_motion 0}}")
    rE(f"frame {fcrspeed}.display -relief flat")
    rE(f"frame {fcrspeed}.display.top -relief flat")
    rE(f"label {fcrspeed}.display.top.value -textvariable crSpeed -width 6 -anchor center")
    rE(f"scale {fcrspeed}.display.cut-rec-speed -takefocus 0 -orient horizontal -showvalue 0 -command cut_rec_slider_changed")
    rE(f"{fcrspeed}.display.cut-rec-speed configure -from 1 -to 100 -resolution 1")
    rE(f"Button {fcrspeed}.forward -text {{{_('Fwd')}}} -takefocus 0 -width 3")
    rE(f"frame {fcrbuttons} -relief flat")
    rE(f"button {fcrbuttons}.laser -text {{{_('Laser')}}} -takefocus 0 -width 6")
    rE(f"button {fcrbuttons}.cancel -text {{{_('Cancel')}}} -command cut_rec_cancel -takefocus 0 -width 6")
    rE(f"labelframe {fcrleadin} -text {{{_('Leadins:')}}} -relief flat")
    rE(f"Button {fcrleadin}.nw -takefocus 0 -image {rE(f'load_image {imagePath}/nw')}")
    rE(f"Button {fcrleadin}.n -takefocus 0 -image {rE(f'load_image {imagePath}/n')}")
    rE(f"Button {fcrleadin}.ne -takefocus 0 -image {rE(f'load_image {imagePath}/ne')}")
    rE(f"Button {fcrleadin}.w -takefocus 0 -image {rE(f'load_image {imagePath}/w')}")
    rE(f"Label {fcrleadin}.offset -textvariable kerfWidth -takefocus 0")
    rE(f"Button {fcrleadin}.e -takefocus 0 -image {rE(f'load_image {imagePath}/e')}")
    rE(f"Button {fcrleadin}.sw -takefocus 0 -image {rE(f'load_image {imagePath}/sw')}")
    rE(f"Button {fcrleadin}.s -takefocus 0 -image {rE(f'load_image {imagePath}/s')}")
    rE(f"Button {fcrleadin}.se -takefocus 0 -image {rE(f'load_image {imagePath}/se')}")
    # populate the frame
    rE(f"pack {fcrspeed}.reverse -side left -fill y")
    rE(f"pack {fcrspeed}.display.top.value -side left -fill y")
    rE(f"pack {fcrspeed}.display.top -fill y")
    rE(f"pack {fcrspeed}.display.cut-rec-speed -fill y")
    rE(f"pack {fcrspeed}.display -side left -fill x -expand 1")
    rE(f"pack {fcrspeed}.forward -side right -fill y")
    rE(f"grid {fcrbuttons}.laser -column 0 -row 0")
    rE(f"grid {fcrbuttons}.cancel -column 2 -row 0")
    rE(f"grid {fcrleadin}.nw -column 0 -row 0")
    rE(f"grid {fcrleadin}.n -column 1 -row 0")
    rE(f"grid {fcrleadin}.ne -column 2 -row 0")
    rE(f"grid {fcrleadin}.w -column 0 -row 1")
    rE(f"grid {fcrleadin}.offset -column 1 -row 1")
    rE(f"grid {fcrleadin}.e -column 2 -row 1")
    rE(f"grid {fcrleadin}.sw -column 0 -row 2")
    rE(f"grid {fcrleadin}.s -column 1 -row 2")
    rE(f"grid {fcrleadin}.se -column 2 -row 2")
    rE(f"grid {fcrspeed} -column 0 -row 0 -sticky wne")
    rE(f"grid {fcrbuttons} -column 0 -row 1 -sticky wne -pady 16")
    rE(f"grid {fcrleadin} -column 0 -row 2 -sticky nsew")# -pady 12")
    # set direction buttons stretch
    rE(f"grid columnconfigure {fcrbuttons} 1 -weight 1")
    rE(f"grid columnconfigure {fcrleadin} 0 -weight 1")
    rE(f"grid columnconfigure {fcrleadin} 1 -weight 1")
    rE(f"grid columnconfigure {fcrleadin} 2 -weight 1")
    rE(f"grid rowconfigure {fcrleadin} 0 -weight 1")
    rE(f"grid rowconfigure {fcrleadin} 1 -weight 1 -pad 20")
    rE(f"grid rowconfigure {fcrleadin} 2 -weight 1")
    # widget bindings
    rE(f"bind {fcrspeed}.forward <Button-1> {{cut_rec_motion 1}}")
    rE(f"bind {fcrspeed}.forward <ButtonRelease-1> {{cut_rec_motion 0}}")
    rE(f"bind {fcrleadin}.nw <Button-1> {{cut_rec_move -1 1}}")
    rE(f"bind {fcrleadin}.n <Button-1> {{cut_rec_move 0 1}}")
    rE(f"bind {fcrleadin}.ne <Button-1> {{cut_rec_move 1 1}}")
    rE(f"bind {fcrleadin}.w <Button-1> {{cut_rec_move -1 0}}")
    rE(f"bind {fcrleadin}.e <Button-1> {{cut_rec_move 1 0}}")
    rE(f"bind {fcrleadin}.sw <Button-1> {{cut_rec_move -1 -1}}")
    rE(f"bind {fcrleadin}.s <Button-1> {{cut_rec_move 0 -1}}")
    rE(f"bind {fcrleadin}.se <Button-1> {{cut_rec_move 1 -1}}")
    rE(f"bind {fcrbuttons}.laser <Button-1> {{laser_button_toggled 1 0}}")
    rE(f"bind {fcrbuttons}.laser <ButtonRelease-1> {{laser_button_toggled 0 0}}")

    # new parameters frame
    rE(f"frame {fparam}")
    # create the widgets
    rE(f"frame {fparam}.c1")
    rE(f"frame {fparam}.c2")
    rE(f"frame {fparam}.c3")
    rE(f"labelframe {fparam}.c1.probe -text {{{_('Probing')}}} -relief groove")
    rE(f"labelframe {fparam}.c1.motion -text {{{_('Motion')}}} -relief groove")
    rE(f"labelframe {fparam}.c1.safety -text {{{_('Safety')}}} -relief groove")
    rE(f"labelframe {fparam}.c2.thc -text {{{_('THC')}}} -relief groove")
    rE(f"labelframe {fparam}.c2.scribe -text {{{_('Scribe')}}} -relief groove")
    rE(f"labelframe {fparam}.c2.pierce -text {{{_('Pierce Only')}}} -relief groove")
    rE(f"labelframe {fparam}.c3.arc -text {{{_('Arc')}}} -relief groove")
    rE(f"labelframe {fparam}.c3.spotting -text {{{_('Spotting')}}} -relief groove")
    rE(f"label {fparam}.c2.thc.thc-autoL -text {{{_('Auto')}}} -width 15 -anchor e")
    rE(f"checkbutton {fparam}.c2.thc.thc-auto -width 2 -anchor w -indicatoron 0")
    rE(f"{fparam}.c2.thc.thc-auto configure -variable thcAuto -command mode_changed")
    # spinboxes [parent, name, decimals, value, min, max, increment, label text, prefs option]
    cpList = [[f"{fparam}.c1.probe",'float-switch-travel',2,1.5,-25,25,0.01,'Float Travel','Float Switch Travel'], \
              [f"{fparam}.c1.probe",'probe-feed-rate',0,300,1,thcFeedRate,1,'Probe Speed','Probe Feed Rate'], \
              [f"{fparam}.c1.probe",'probe-start-height',0,38,1,maxHeight,1,'Probe Height','Probe Start Height'], \
              [f"{fparam}.c1.probe",'ohmic-probe-offset',2,0,-25,+25,0.01,'Ohmic Z Offset','Ohmic Probe Offset'], \
              [f"{fparam}.c1.probe",'ohmic-max-attempts',0,0,0,10,1,'Ohmic Retries','Ohmic Maximum Attempts'], \
              [f"{fparam}.c1.probe",'skip-ihs-distance',0,0,0,999,1,'Skip IHS','Skip IHS Distance'], \
              [f"{fparam}.c1.probe",'offset-feed-rate',0,offsetFeedRate*0.8,0,offsetFeedRate,1,'Offset Speed','Offset Feed Rate'], \
              [f"{fparam}.c1.motion",'setup-feed-rate',0,int(thcFeedRate * 0.8),1000,thcFeedRate,1,'Setup Speed','Setup Feed Rate'], \
              [f"{fparam}.c1.safety",'safe-height',0,20,0,maxHeight,1,'Safe Height','Safe Height'], \
              [f"{fparam}.c2.thc",'thc-delay',1,0.5,0,9,0.1,'Start Delay','THC Delay'], \
              [f"{fparam}.c2.thc",'thc-sample-counts',0,50,10,1000,1,'Auto Counts','THC Sample Counts'], \
              [f"{fparam}.c2.thc",'thc-sample-threshold',1,1,0.1,9,0.1,'Auto Threshold (V)','THC Sample Threshold'], \
              [f"{fparam}.c2.thc",'thc-threshold',2,1,0.05,9,0.01,'Cut Threshold (V)','THC Threshold'], \
              [f"{fparam}.c2.thc",'pid-p-gain',0,10,0,1000,1,'PID P Gain (Speed)','Pid P Gain'], \
              [f"{fparam}.c2.thc",'pid-i-gain',0,0,0,1000,1,'PID I Gain','Pid I Gain'], \
              [f"{fparam}.c2.thc",'pid-d-gain',0,0,0,1000,1,'PID D Gin','Pid D Gain'], \
              [f"{fparam}.c2.thc",'cornerlock-threshold',0,90,1,99,1,'VAD Threshold (%)','Velocity Anti Dive Threshold'], \
              [f"{fparam}.c2.thc",'voidlock-slope',0,500,1,10000,1,'Void Slope (V/sec)','Void Sense Slope'], \
              [f"{fparam}.c2.scribe",'scribe-arm-delay',1,0,0,9,0.1,'Arm Delay','Scribe Arming Delay'], \
              [f"{fparam}.c2.scribe",'scribe-on-delay',1,0,0,9,0.1,'On delay','Scribe On Delay'], \
              [f"{fparam}.c2.pierce",'x-pierce-offset',1,1.6,-5,5,0.1,'X Offset','X Pierce Offset'], \
              [f"{fparam}.c2.pierce",'y-pierce-offset',1,0,-5,5,0.1,'Y Offset','Y Pierce Offset'], \
              [f"{fparam}.c3.arc",'arc-fail-delay',1,3,0.1,60,0.1,'Fail Timeout','Arc Fail Timeout'], \
              [f"{fparam}.c3.arc",'arc-max-starts',0,3,1,9,1,'Max. Attempts','Arc Maximum Starts'], \
              [f"{fparam}.c3.arc",'restart-delay',0,3,1,60,1,'Retry Delay','Arc Restart Delay'], \
              [f"{fparam}.c3.arc",'arc-voltage-scale',6,1,-9999,9999,0.000001,'Voltage Scale','Arc Voltage Scale'], \
              [f"{fparam}.c3.arc",'arc-voltage-offset',3,0,-999999,999999,0.001,'Voltage Offset','Arc Voltage Offset'], \
              [f"{fparam}.c3.arc",'arc-ok-high',0,99999,0,99999,1,'OK High Volts','Arc OK High'], \
              [f"{fparam}.c3.arc",'arc-ok-low',0,60,0,100,1,'OK Low Volts','Arc OK Low'], \
              [f"{fparam}.c3.arc",'height-per-volt',3,0.1,0.025,0.5,0.01,'Height Per Volt','Height Per Volt'], \
              [f"{fparam}.c3.spotting",'spotting-threshold',0,1,0,199,1,'Threshold (V)','Spotting Threshold'], \
              [f"{fparam}.c3.spotting",'spotting-time',0,0,0,9999,1,'On Time (mS)','Spotting Time'], \
             ]
    cpRow = 0
    cpFrame = cpList[0][0]
    for cpItem in cpList:
        if s.linear_units != 1:
            if cpItem[1] == 'float-switch-travel':
                cpItem[2:7] = [3,0.06,-1,1,0.001]
            elif cpItem[1] == 'probe-feed-rate':
                cpItem[2:7] = [1,12,0.1,thcFeedRate,.1]
            elif cpItem[1] == 'probe-start-height':
                cpItem[2:7] = [2,1.5,.1,maxHeight,0.01]
            elif cpItem[1] == 'ohmic-probe-offset':
                cpItem[2:7] = [3,0,-1,1,0.001]
            elif cpItem[1] == 'skip-ihs-distance':
                cpItem[2:7] = [1,0,0,99,.1]
            elif cpItem[1] == 'offset-feed-rate':
                cpItem[2:7] = [1,offsetFeedRate * 0.8,40,offsetFeedRate,.1]
            elif cpItem[1] == 'setup-feed-rate':
                cpItem[2:7] = [1,int(thcFeedRate * 0.8),0.1,thcFeedRate,0.1]
            elif cpItem[1] == 'safe-height':
                cpItem[2:7] = [2,0.75,0,maxHeight,0.01]
            elif cpItem[1] == 'height-per-volt':
                cpItem[2:7] = [4,0.004,0.001,0.020,0.001]
            elif cpItem[1] == 'x-pierce-offset':
                cpItem[2:7] = [2,0.06,-0.2,0.2,0.01]
            elif cpItem[1] == 'y-pierce-offset':
                cpItem[2:7] = [2,0,-0.2,0.2,0.01]
        if cpItem[0] != cpFrame:
            cpFrame = cpItem[0]
            cpRow = 0
        if cpItem[1] == 'thc-delay':
            cpRow += 1
        cpName = f"{cpItem[0]}.{cpItem[1]}"
        cpType = 'flt' if cpItem[2] > 0 else 'int'
        rE(f"label {cpName}L -text {{{cpItem[7]}}} -width 15 -anchor e")
        rE(f"spinbox {cpName} -width  8 -justify right -wrap true")
        spinBoxes.append(cpName)
        rE(f"{cpName} configure -from {cpItem[4]} -to {cpItem[5]}")
        rE(f"{cpName} configure -increment {cpItem[6]} -format %0.{cpItem[2]}f")
        rE(f"{cpName} configure -validate key -vcmd {{{valspin} %W {cpType} {cpItem[2]} %P %s}}")
        if cpItem[1] == 'setup-feed-rate':
            rE(f"label {cpItem[0]}.maxzL -text {{{_('Max Z Speed')}}} -width 15 -anchor e")
            rE(f"label {cpItem[0]}.maxz -text {int(thcFeedRate)} -width 8 -anchor e")
            rE(f"grid {cpItem[0]}.maxzL -column 0 -row {cpRow} -sticky e -padx {{0 0}} -pady {{4 0}}")
            rE(f"grid {cpItem[0]}.maxz -column 1 -row {cpRow} -sticky e -padx {{0 8}} -pady {{4 0}}")
            cpRow += 1
        rE(f"grid {cpName}L -column 0 -row {cpRow} -sticky e -padx {{4 0}} -pady {{4 0}}")
        rE(f"grid {cpName} -column 1 -row {cpRow} -sticky e -padx {{0 4}} -pady {{4 0}}")
        cpRow += 1
    # populate parameters frame
    rE(f"grid {fparam}.c1.probe -column 0 -row 0 -sticky new -padx {{4 2}} -pady {{4 0}} -ipady 4")
    rE(f"grid {fparam}.c1.motion -column 0 -row 1 -sticky new -padx {{4 2}} -pady {{4 0}} -ipady 4")
    rE(f"grid {fparam}.c1.safety -column 0 -row 2 -sticky new -padx {{4 2}} -pady {{4 0}} -ipady 4")
    rE(f"grid {fparam}.c2.thc -column 0 -row 0 -sticky new -padx {{2 2}} -pady {{4 0}} -ipady 4")
    rE(f"grid {fparam}.c2.scribe -column 0 -row 1 -sticky new -padx {{2 2}} -pady {{4 0}} -ipady 4")
    rE(f"grid {fparam}.c2.pierce -column 0 -row 2 -sticky new -padx {{2 2}} -pady {{4 0}} -ipady 4")
    rE(f"grid {fparam}.c3.arc -column 0 -row 0 -sticky new -padx {{2 2}} -pady {{4 0}} -ipady 4")
    rE(f"grid {fparam}.c3.spotting -column 0 -row 1 -sticky new -padx {{2 2}} -pady {{4 0}} -ipady 4")
    rE(f"grid {fparam}.c1 -column 0 -row 0 -sticky n")
    rE(f"grid {fparam}.c2 -column 2 -row 0 -sticky n")
    rE(f"grid {fparam}.c3 -column 4 -row 0 -sticky n")
    rE(f"grid columnconfigure {fparam} 1 -weight 1")
    rE(f"grid columnconfigure {fparam} 3 -weight 1")

    # new settings frame
    rE(f"frame {fsetup}")
    # left panel
    rE(f"frame {fsetup}.l")
    # gui frame
    rE(f"labelframe {fsetup}.l.gui -text {{{_('GUI')}}} -relief groove")
    rE(f"label {fsetup}.l.gui.closedialogL -text {{{_('Close Dialog')}}} -anchor e")
    rE(f"checkbutton {fsetup}.l.gui.closedialog -variable closeDialog -width 2 -anchor w -indicatoron 0")
    rE(f"label {fsetup}.l.gui.wsizeL -text {{{_('Window Size')}}} -width  13 -anchor e")
    rE(f"ComboBox {fsetup}.l.gui.wsize -modifycmd {{set_window_size combo}} -textvariable winSize -bd 1 -width 10 -justify right -editable 0")
    rE(f"{fsetup}.l.gui.wsize configure -values {{default last fullscreen maximized}}")
    rE(f"label {fsetup}.l.gui.orientL -text {{{_('Window Orient')}}} -width  13 -anchor e")
    rE(f"ComboBox {fsetup}.l.gui.orient -modifycmd set_orientation -textvariable orient -bd 1 -width 10 -justify right -editable 0")
    rE(f"{fsetup}.l.gui.orient configure -values {{landscape portrait}}")
    rE(f"label {fsetup}.l.gui.fsizeL -text {{{_('Font Size')}}} -anchor e")
    rE(f"ComboBox {fsetup}.l.gui.fsize -modifycmd font_changed -textvariable fontSize -bd 1 -width 10 -justify right -editable 0")
    rE(f"{fsetup}.l.gui.fsize configure -values {{7 8 9 10 11 12 13 14 15 16 17 18 19 20}}")
    rE(f"label {fsetup}.l.gui.fguiL -text {{{_('GUI Font')}}} -anchor e")
    rE(f"ComboBox {fsetup}.l.gui.fgui -modifycmd {{font_changed 1}} -textvariable guiFont -bd 1 -width 10 -justify right -editable 0")
    for font in sorted(fontsGui):
        rE(f"{fsetup}.l.gui.fgui insert end {{{font}}}")
    rE(f"label {fsetup}.l.gui.fcodeL -text {{{_('Gcode Font')}}} -anchor e")
    rE(f"ComboBox {fsetup}.l.gui.fcode -modifycmd font_changed -textvariable codeFont -bd 1 -width 10 -justify right -editable 0")
    populate_gcode_fonts()
    rE(f"label {fsetup}.l.gui.coneL -text {{{_('Cone Size')}}} -anchor e")
    rE(f"ComboBox {fsetup}.l.gui.cone -modifycmd cone_size_changed -textvariable coneSize -bd 1 -width 10 -justify right -editable 0")
    rE(f"{fsetup}.l.gui.cone configure -values {{0.1 0.2 0.3 0.4 0.5 0.6 0.7 0.8 0.9 1.0}}")
    rE(f"label {fsetup}.l.gui.popLocationL -text {{{_('Popup Location')}}} -width  14 -anchor e")
    rE(f"ComboBox {fsetup}.l.gui.popLocation -textvariable popLocation -bd 1 -width 10 -justify right -editable 0")
    rE(f"{fsetup}.l.gui.popLocation configure -values {{pointer gui center screen center}}")
    rE(f"label {fsetup}.l.gui.zoomL -text {{{_('Table Zoom')}}} -anchor e")
    rE(f"spinbox {fsetup}.l.gui.zoom -width  10 -justify right -wrap true -from 0.1 -to 10.0 -increment 0.1 -format %0.1f")
    rE(f"{fsetup}.l.gui.zoom configure -validate key -vcmd {{{valspin} %W flt 1 %P %s}}")
    spinBoxes.append(f"{fsetup}.l.gui.zoom")
    rE(f"label {fsetup}.l.gui.jogspeedL -width  14 -text {{{_('Jog Speed')}}} -anchor e")
    rE(f"spinbox {fsetup}.l.gui.jogspeed -width  10 -justify right -wrap true")
    minJogSpeed = int(float(rE(f"{ftop}.jogspeed.s cget -from")) * vars.max_speed.get() * 60)
    rE(f"{fsetup}.l.gui.jogspeed configure -from {minJogSpeed} -to {vars.max_speed.get() * 60} -increment 1")
    rE(f"{fsetup}.l.gui.jogspeed configure -format %0.0f")
    rE(f"{fsetup}.l.gui.jogspeed configure -validate key -vcmd {{{valspin} %W int 0 %P %s}}")
    spinBoxes.append(f"{fsetup}.l.gui.jogspeed")
    rE(f"label {fsetup}.l.gui.crspeedL -width  14 -text {{{_('Cut Rec Speed %')}}} -anchor e")
    rE(f"spinbox {fsetup}.l.gui.crspeed -width  10 -justify right -wrap true")
    minJogSpeed = int(float(rE(f"{ftop}.jogspeed.s cget -from")) * vars.max_speed.get() * 60)
    rE(f"{fsetup}.l.gui.crspeed configure -from 1 -to 100 -increment 1")
    rE(f"{fsetup}.l.gui.crspeed configure -format %0.0f")
    rE(f"{fsetup}.l.gui.crspeed configure -validate key -vcmd {{{valspin} %W int 0 %P %s}}")
    spinBoxes.append(f"{fsetup}.l.gui.crspeed")
    rE(f"label {fsetup}.l.gui.matdefaultL -text {{{_('Default Material')}}} -width  14 -anchor e")
    rE(f"ComboBox {fsetup}.l.gui.matdefault -modifycmd change_default_material -textvariable matDefault -bd 1 -width 10 -justify right -editable 0")
    rE(f"label {fsetup}.l.gui.kbShortcutsL -text {{{_('Use KB Shortcuts')}}} -anchor e")
    rE(f"checkbutton {fsetup}.l.gui.kbShortcuts -variable kbShortcuts -command kb_shortcuts_changed -width 2 -anchor w -indicatoron 0")
    rE(f"label {fsetup}.l.gui.useVirtKBL -text {{{_('Use Virtual KB')}}} -anchor e")
    rE(f"checkbutton {fsetup}.l.gui.useVirtKB -variable useVirtKB -command virtual_kb_changed -width 2 -anchor w -indicatoron 0")
    # populate gui frame
    rE(f"grid {fsetup}.l.gui.closedialogL -column 0 -row 0 -sticky e -padx  {{4 0}} -pady  {{4 0}}")
    rE(f"grid {fsetup}.l.gui.closedialog -column 1 -row 0 -sticky e -padx  {{0 4}} -pady  {{4 0}}")
    rE(f"grid {fsetup}.l.gui.wsizeL -column 0 -row 1 -sticky e -padx  {{4 0}} -pady  {{4 0}}")
    rE(f"grid {fsetup}.l.gui.wsize -column 1 -row 1 -sticky ew -padx  {{0 4}} -pady  {{4 0}}")
    rE(f"grid {fsetup}.l.gui.orientL -column 0 -row 2 -sticky e -padx  {{4 0}} -pady  {{4 0}}")
    rE(f"grid {fsetup}.l.gui.orient -column 1 -row 2 -sticky ew -padx  {{0 4}} -pady  {{4 0}}")
    rE(f"grid {fsetup}.l.gui.fsizeL -column 0 -row 3 -sticky e -padx  {{4 0}} -pady  {{4 4}}")
    rE(f"grid {fsetup}.l.gui.fsize -column 1 -row 3 -sticky ew -padx  {{0 4}} -pady  {{4 4}}")
    rE(f"grid {fsetup}.l.gui.fguiL -column 0 -row 4 -sticky e -padx  {{4 0}} -pady  {{4 4}}")
    rE(f"grid {fsetup}.l.gui.fgui -column 1 -row 4 -sticky ew -padx  {{0 4}} -pady  {{4 4}}")
    rE(f"grid {fsetup}.l.gui.fcodeL -column 0 -row 5 -sticky e -padx  {{4 0}} -pady  {{4 4}}")
    rE(f"grid {fsetup}.l.gui.fcode -column 1 -row 5 -sticky ew -padx  {{0 4}} -pady  {{4 4}}")
    rE(f"grid {fsetup}.l.gui.coneL -column 0 -row 6 -sticky e -padx  {{4 0}} -pady  {{4 4}}")
    rE(f"grid {fsetup}.l.gui.cone -column 1 -row 6 -sticky ew -padx  {{0 4}} -pady  {{4 4}}")
    rE(f"grid {fsetup}.l.gui.popLocationL -column 0 -row 7 -sticky e -padx  {{4 0}} -pady  {{4 4}}")
    rE(f"grid {fsetup}.l.gui.popLocation -column 1 -row 7 -sticky ew -padx  {{0 4}} -pady  {{4 4}}")
    rE(f"grid {fsetup}.l.gui.zoomL -column 0 -row 8 -sticky e -padx  {{4 0}} -pady  {{4 4}}")
    rE(f"grid {fsetup}.l.gui.zoom -column 1 -row 8 -sticky ew -padx  {{0 4}} -pady  {{4 4}}")
    rE(f"grid {fsetup}.l.gui.jogspeedL -column 0 -row 9 -sticky e -padx  {{4 0}} -pady  {{4 4}}")
    rE(f"grid {fsetup}.l.gui.jogspeed -column 1 -row 9 -sticky ew -padx  {{0 4}} -pady  {{4 4}}")
    rE(f"grid {fsetup}.l.gui.crspeedL -column 0 -row 10 -sticky e -padx  {{4 0}} -pady  {{4 4}}")
    rE(f"grid {fsetup}.l.gui.crspeed -column 1 -row 10 -sticky ew -padx  {{0 4}} -pady  {{4 4}}")
    rE(f"grid {fsetup}.l.gui.matdefaultL -column 0 -row 11 -sticky e -padx  {{4 0}} -pady  {{4 4}}")
    rE(f"grid {fsetup}.l.gui.matdefault -column 1 -row 11 -sticky ew -padx  {{0 4}} -pady  {{4 4}}")
    rE(f"grid {fsetup}.l.gui.kbShortcutsL -column 0 -row 12 -sticky e -padx  {{4 0}} -pady  {{4 4}}")
    rE(f"grid {fsetup}.l.gui.kbShortcuts -column 1 -row 12 -sticky e -padx  {{0 4}} -pady  {{4 4}}")
    rE(f"grid {fsetup}.l.gui.useVirtKBL -column 0 -row 13 -sticky e -padx  {{4 0}} -pady  {{4 4}}")
    rE(f"grid {fsetup}.l.gui.useVirtKB -column 1 -row 13 -sticky e -padx  {{0 4}} -pady  {{4 4}}")
    rE(f"grid columnconfigure {fsetup}.l.gui 0 -weight 1")
    # plasmac frame
    rE(f"labelframe {fsetup}.l.plasmac -text {{{_('Plasmac')}}} -relief groove")
    rE(f"label {fsetup}.l.plasmac.modeL -width  13 -text {{{_('Mode')}}} -anchor e")
    rE(f"ComboBox {fsetup}.l.plasmac.mode -modifycmd mode_changed -textvariable plasmacMode -bd 1 -width 12 -justify right -editable 0")
    rE(f"{fsetup}.l.plasmac.mode configure -values {{0 1 2}}")
    # populate plasmac frame
    rE(f"grid {fsetup}.l.plasmac.modeL -column 0 -row 0 -sticky e -padx  {{4 0}} -pady  {{4 4}}")
    rE(f"grid {fsetup}.l.plasmac.mode -column 1 -row 0 -sticky ew -padx  {{0 4}} -pady  {{4 4}}")
    rE(f"grid columnconfigure {fsetup}.l.plasmac 0 -weight 1")
    #populate left panel
    rE(f"grid {fsetup}.l.gui -column 0 -row 0 -sticky new")
    rE(f"grid {fsetup}.l.plasmac -column 0 -row 2 -sticky new")

    # middle panel for utilities
    rE(f"frame {fsetup}.m")
    # utilities frame
    rE(f"labelframe {fsetup}.m.utilities -text {{{_('Utilities')}}} -relief groove")
    rE(f"button {fsetup}.m.utilities.offsets -command set_peripheral_offsets -text {{{_('Peripheral Offsets')}}} -padx 0")
    # populate utilities frame
    rE(f"grid {fsetup}.m.utilities.offsets -column 0 -row 0 -sticky new -padx 4 -pady {{0 4}}")
    rE(f"grid columnconfigure {fsetup}.m.utilities 0 -weight 1")
    # color frame
    rE(f"labelframe {fsetup}.m.colors -text {{{_('Colors')}}} -relief groove")
    rE(f"label {fsetup}.m.colors.foreL -width  13 -text {{{_('Foreground')}}} -anchor e")
    rE(f"button {fsetup}.m.colors.fore -command {{color_set fore}}")
    rE(f"label {fsetup}.m.colors.backL -width  13 -text {{{_('Background')}}} -anchor e")
    rE(f"button {fsetup}.m.colors.back -command {{color_set back}}")
    rE(f"label {fsetup}.m.colors.disableL -width  13 -text {{{_('Disabled')}}} -anchor e")
    rE(f"button {fsetup}.m.colors.disable -command {{color_set disable}}")
    rE(f"label {fsetup}.m.colors.activeL -width  13 -text {{{_('Active')}}} -anchor e")
    rE(f"button {fsetup}.m.colors.active -command {{color_set active}}")
    rE(f"label {fsetup}.m.colors.warnL -width  13 -text {{{_('Warning')}}} -anchor e")
    rE(f"button {fsetup}.m.colors.warn -command {{color_set warn}}")
    rE(f"label {fsetup}.m.colors.voltL -width  13 -text {{{_('Arc Voltage')}}} -anchor e")
    rE(f"button {fsetup}.m.colors.volt -command {{color_set volt}}")
    rE(f"label {fsetup}.m.colors.arcL -width  13 -text {{{_('Arc OK')}}} -anchor e")
    rE(f"button {fsetup}.m.colors.arc -command {{color_set arc}}")
    rE(f"label {fsetup}.m.colors.ledL -width  13 -text {{{_('LED''s')}}} -anchor e")
    rE(f"button {fsetup}.m.colors.led -command {{color_set led}}")
    rE(f"label {fsetup}.m.colors.troughL -width  13 -text {{{_('Slider Trough')}}} -anchor e")
    rE(f"button {fsetup}.m.colors.trough -command {{color_set trough}}")
    # populate color frame
    rE(f"grid {fsetup}.m.colors.foreL -column 0 -row 0 -sticky e -padx 4 -pady {{4 4}}")
    rE(f"grid {fsetup}.m.colors.fore -column 1 -row 0 -sticky e -padx {{0 4}} -pady {{4 4}}")
    rE(f"grid {fsetup}.m.colors.backL -column 0 -row 1 -sticky e -padx 4 -pady {{4 4}}")
    rE(f"grid {fsetup}.m.colors.back -column 1 -row 1 -sticky e -padx {{0 4}} -pady {{4 4}}")
    rE(f"grid {fsetup}.m.colors.disableL -column 0 -row 2 -sticky e -padx 4 -pady {{4 4}}")
    rE(f"grid {fsetup}.m.colors.disable -column 1 -row 2 -sticky e -padx {{0 4}} -pady {{4 4}}")
    rE(f"grid {fsetup}.m.colors.activeL -column 0 -row 3 -sticky e -padx 4 -pady {{4 4}}")
    rE(f"grid {fsetup}.m.colors.active -column 1 -row 3 -sticky e -padx {{0 4}} -pady {{4 4}}")
    rE(f"grid {fsetup}.m.colors.warnL -column 0 -row 4 -sticky e -padx 4 -pady {{4 4}}")
    rE(f"grid {fsetup}.m.colors.warn -column 1 -row 4 -sticky e -padx {{0 4}} -pady {{4 4}}")
    rE(f"grid {fsetup}.m.colors.voltL -column 0 -row 5 -sticky e -padx 4 -pady {{4 4}}")
    rE(f"grid {fsetup}.m.colors.volt -column 1 -row 5 -sticky e -padx {{0 4}} -pady {{4 4}}")
    rE(f"grid {fsetup}.m.colors.arcL -column 0 -row 6 -sticky e -padx 4 -pady {{4 4}}")
    rE(f"grid {fsetup}.m.colors.arc -column 1 -row 6 -sticky e -padx {{0 4}} -pady {{4 4}}")
    rE(f"grid {fsetup}.m.colors.ledL -column 0 -row 7 -sticky e -padx 4 -pady {{4 4}}")
    rE(f"grid {fsetup}.m.colors.led -column 1 -row 7 -sticky e -padx {{0 4}} -pady {{4 4}}")
    rE(f"grid {fsetup}.m.colors.troughL -column 0 -row 8 -sticky e -padx 4 -pady {{4 4}}")
    rE(f"grid {fsetup}.m.colors.trough -column 1 -row 8 -sticky e -padx {{0 4}} -pady {{4 4}}")
    rE(f"grid columnconfigure {fsetup}.m.colors 0 -weight 1")
    # populate middle panel
    rE(f"grid {fsetup}.m.utilities -column 0 -row 0 -sticky new")
    rE(f"grid {fsetup}.m.colors -column 0 -row 1 -sticky new")

    # right panel for text entries
    rE(f"frame {fsetup}.r")
    # frame for torch enable
    rE(f"labelframe {fsetup}.r.torch -text {{{_('Torch Enable Text')}}} -relief groove")
    rE(f"label {fsetup}.r.torch.blankL -width 2 -anchor w")
    rE(f"label {fsetup}.r.torch.enabledL -text {{{_('Enabled')}}} -width 14 -anchor w")
    rE(f"label {fsetup}.r.torch.disabledL -text {{{_('Disabled')}}} -width 14 -anchor w")
    rE(f"entry {fsetup}.r.torch.enabled -bd 1 -width 14")
    rE(f"entry {fsetup}.r.torch.disabled -bd 1 -width 14")
    rE(f"grid {fsetup}.r.torch.blankL -column 0 -row 0 -sticky nw -padx {{4 0}}")
    rE(f"grid {fsetup}.r.torch.enabledL -column 1 -row 0 -sticky nw -padx {{4 0}}")
    rE(f"grid {fsetup}.r.torch.disabledL -column 2 -row 0 -sticky nw -padx 4")
    rE(f"grid {fsetup}.r.torch.enabled -column 1 -row 1 -sticky nw -padx {{4 0}} -pady {{0 4}}")
    rE(f"grid {fsetup}.r.torch.disabled -column 2 -row 1 -sticky nw -padx 4 -pady {{0 4}}")
    # frame for user buttons
    rE(f"labelframe {fsetup}.r.ubuttons -text {{{_('User Buttons')}}} -relief groove")
    # canvas for scrolling
    rE(f"canvas {fsetup}.r.ubuttons.canvas")
    rE(f"frame {fsetup}.r.ubuttons.canvas.frame")
    rE(f"scrollbar {fsetup}.r.ubuttons.yscroll -orient vertical -command {{{fsetup}.r.ubuttons.canvas yview}}")
    rE(f"scrollbar {fsetup}.r.ubuttons.xscroll -orient horizontal -command {{{fsetup}.r.ubuttons.canvas xview}}")
    rE(f"{fsetup}.r.ubuttons.canvas create window {{0 0}} -anchor nw -window {{{fsetup}.r.ubuttons.canvas.frame}}")
    rE(f"{fsetup}.r.ubuttons.canvas configure -xscrollcommand {{{fsetup}.r.ubuttons.xscroll set}}")
    rE(f"{fsetup}.r.ubuttons.canvas configure -yscrollcommand {{{fsetup}.r.ubuttons.yscroll set}}")
    # layout the canvas
    rE(f"grid {fsetup}.r.ubuttons.yscroll -column 0 -row 0 -sticky nsw -padx {{4 0}}")
    rE(f"grid {fsetup}.r.ubuttons.canvas -column 1 -row 0 -sticky nsew")
    rE(f"grid {fsetup}.r.ubuttons.xscroll -column 1 -row 1 -sticky new -padx {{4 0}}")
    rE(f"grid columnconfigure {fsetup}.r.ubuttons 1 -weight 1")
    rE(f"grid rowconfigure {fsetup}.r.ubuttons 0 -weight 1")
    # user button widgets
    rE(f"label {fsetup}.r.ubuttons.canvas.frame.numL -text  # -width 2 -anchor e")
    rE(f"label {fsetup}.r.ubuttons.canvas.frame.nameL -text {{{_('Name')}}} -width 14 -anchor w")
    rE(f"label {fsetup}.r.ubuttons.canvas.frame.codeL -text {{{_('Code')}}} -width 60 -anchor w")
    for n in range(1, maxUserButtons + 1):
        rE(f"label {fsetup}.r.ubuttons.canvas.frame.num{n} -text {str(n)} -anchor e")
        rE(f"entry {fsetup}.r.ubuttons.canvas.frame.name{n} -bd 1 -width 14")
        rE(f"entry {fsetup}.r.ubuttons.canvas.frame.code{n} -bd 1")
        rE(f"grid {fsetup}.r.ubuttons.canvas.frame.num{n} -column 0 -row {n} -sticky ne -padx {{4 0}} -pady {{0 4}}")
        rE(f"grid {fsetup}.r.ubuttons.canvas.frame.name{n} -column 1 -row {n} -sticky nw -padx {{4 0}} -pady {{0 4}}")
        rE(f"grid {fsetup}.r.ubuttons.canvas.frame.code{n} -column 2 -row {n} -sticky new -padx {{4 4}} -pady {{0 4}}")
    rE(f"grid {fsetup}.r.ubuttons.canvas.frame.numL -column 0 -row 0 -sticky ne -padx {{4 0}}")
    rE(f"grid {fsetup}.r.ubuttons.canvas.frame.nameL -column 1 -row 0 -sticky nw -padx {{4 0}}")
    rE(f"grid {fsetup}.r.ubuttons.canvas.frame.codeL -column 2 -row 0 -sticky nw -padx {{4 4}}")
    rE(f"grid columnconfigure {fsetup}.r.ubuttons.canvas.frame 2 -weight 1")
    # frame for shutdown message
    rE(f"labelframe {fsetup}.r.shutdown -text {{{_('Shutdown Message')}}} -relief groove")
    rE(f"entry {fsetup}.r.shutdown.msg -textvariable closeText -bd 1 -width 54")
    rE(f"grid {fsetup}.r.shutdown.msg -column 0 -row 0 -sticky new -padx 4 -pady {{0 4}}")
    rE(f"grid columnconfigure {fsetup}.r.shutdown 0 -weight 1")
    # populate right panel
    rE(f"grid {fsetup}.r.torch -column 0 -row 0 -sticky new")
    rE(f"grid {fsetup}.r.ubuttons -column 0 -row 1 -sticky nsew")
    rE(f"grid {fsetup}.r.shutdown -column 0 -row 2 -sticky new")
    rE(f"grid rowconfigure {fsetup}.r 1 -weight 1")
    rE(f"grid columnconfigure {fsetup}.r 0 -weight 1")

    # populate settings frame
    populate_settings_frame()

    # bottom pane widgets
    # configure
    rE(f"{ft} configure -relief flat -pady 0 -bd 0")
    rE(f"{ft}.sb configure -bd 1")
    rE(f"{ft}.text configure -width 10 -height 6 -borderwidth 1 -relief sunken")
    # populate
    rE(f"grid {fleds} -column 0 -row 1 -padx {{2 0}} -pady {{0 2}} -sticky nsew")
    rE(f"grid {ft} -padx {{0 2}} -pady {{0 2}}")
    rE(f"pack {ft}.sb -fill y -side left -padx 1")
    rE(f"pack {ft}.text -fill both -expand 1 -side left -pady 0")

    # new conversational frames
    rE('frame .toolconv -borderwidth 1 -relief raised')
    rE('frame .fconv -relief flat')

    # change jog slider resolution
    rE('.pane.top.jogspeed.s configure -resolution 0.00001')

    # remove all tooltips
    rE('DynamicHelp::delete ftabs_manual.axes.axisx')
    for w in toolButtons:
        rE(f"DynamicHelp::delete .toolbar.{w}")

    # populate run panel
    populate_run_panel()


##############################################################################
# INITIALIZATION                                                             #
##############################################################################
    # reinitialize notifications to keep them on top of new widgets
    notify_list = []
    for nw in rE(f"winfo children {notifications}").split():
        for n in rE(f"winfo children {nw}").split():
            if n[-5:] == 'label':
                img = rE(f"{n} cget -image").rsplit('_',1)[1]
            elif n[-6:] == 'label2':
                txt = rE(f"{n} cget -text")
        notify_list.append([img, txt])
    notifications.clear()
    notifications.__init__(root_window)
    if notify_list:
        for n in notify_list:
            notifications.add(n[0], n[1])
    update_title()
    o.show_overlay = False
    o.colors['overlay_alpha'] = 0
    pVars.jogMultiplier.set(1)
    hal.set_p('plasmac.mode', f"{pVars.plasmacMode.get()}")
    hal.set_p('plasmac.torch-enable', '0')
    hal.set_p('plasmac.height-override', f"{torchHeight:0.3f}")
    hal.set_p('plasmac.thc-feed-rate', f"{thcFeedRate}")
    pVars.previewLarge.set(False)
    pVars.laserText.set(_('Laser'))
    value = getPrefs(PREF, 'ENABLE_OPTIONS', 'THC auto', False, bool)
    pVars.thcAuto.set(value)
    value = getPrefs(PREF, 'ENABLE_OPTIONS', 'THC enable', False, bool)
    pVars.thcEnable.set(value)
    hal.set_p('plasmac.thc-enable', str(value))
    value = getPrefs(PREF, 'ENABLE_OPTIONS', 'Corner lock enable', False, bool)
    pVars.cornerEnable.set(value)
    hal.set_p('plasmac.cornerlock-enable', str(value))
    value = getPrefs(PREF, 'ENABLE_OPTIONS', 'Void lock enable', False, bool)
    pVars.voidEnable.set(value)
    hal.set_p('plasmac.voidlock-enable', str(value))
    value = getPrefs(PREF, 'ENABLE_OPTIONS', 'Use auto volts', False, bool)
    pVars.autoVolts.set(value)
    hal.set_p('plasmac.use-auto-volts', str(value))
    value = getPrefs(PREF, 'ENABLE_OPTIONS', 'Ohmic probe enable', False, bool)
    pVars.ohmicEnable.set(value)
    hal.set_p('plasmac.ohmic-probe-enable', str(value))
    value = getPrefs(PREF, 'GUI_OPTIONS', 'Jog speed', int(vars.max_speed.get() * 60 * 0.5), int)
    rE(f"{fsetup}.l.gui.jogspeed set {value}")
    restoreSetup['jogSpeed'] = value
    set_jog_slider(value / vars.max_speed.get() / 60)
    value = getPrefs(PREF, 'GUI_OPTIONS', 'Exit warning text', '', str)
    pVars.closeText.set(value)
    restoreSetup['closeText'] = value
    value = getPrefs(PREF, 'GUI_OPTIONS', 'Exit warning', True, bool)
    pVars.closeDialog.set(value)
    restoreSetup['closeDialog'] = value
    root_window.protocol('WM_DELETE_WINDOW', close_window)
    value = getPrefs(PREF, 'GUI_OPTIONS', 'Preview cone size', 0.5, float)
    pVars.coneSize.set(value)
    restoreSetup['coneSize'] = value
    cone_size_changed()
    value = getPrefs(PREF, 'GUI_OPTIONS', 'Popup location', 'pointer', str)
    pVars.popLocation.set(value)
    restoreSetup['popLocation'] = value
    value = getPrefs(PREF, 'GUI_OPTIONS', 'Table zoom', 1, float)
    rE(f"{fsetup}.l.gui.zoom set {value}")
    restoreSetup['tableZoom'] = value
    value = getPrefs(PREF, 'GUI_OPTIONS','Cut recovery speed %', 20, int)
    restoreSetup['crPercent'] = value
    rE(f"{fsetup}.l.gui.crspeed set {value}")
    # check for valid onboard virtual keyboard
    vkb_validate()
    if vkbData['valid']:
        value = getPrefs(PREF, 'GUI_OPTIONS', 'Use soft keyboard', False, bool)
    else:
        value = False
    pVars.useVirtKB.set(value)
    restoreSetup['useVirtKB'] = value
    if vkbData['valid']:
        virtual_kb_changed()
    # disable keyboard shortcuts if virtual keyboard enabled
    if value:
        value = False
        rE(f"{fsetup}.l.gui.kbShortcutsL configure -state disabled")
        rE(f"{fsetup}.l.gui.kbShortcuts configure -state disabled")
    else:
        value = getPrefs(PREF, 'GUI_OPTIONS', 'Use keyboard shortcuts', True, bool)
    pVars.kbShortcuts.set(value)
    restoreSetup['kbShortcuts'] = value
    keyboard_bindings(value)
    statDivisor = 1000 if unitsPerMm == 1 else 1
    statSuffix = 'M' if unitsPerMm == 1 else '"'
    unitSuffix = 'mm' if unitsPerMm == 1 else 'in'
    # we bring in some ints as floats so we can read QtPlasmaC statistics
    pVars.lengthS.set(getPrefs(PREF, 'STATISTICS', 'Cut length', 0, float))
    pVars.lengthJ.set(f"0.00{statSuffix}")
    pVars.lengthT.set(f"{pVars.lengthS.get() / statDivisor:0.2f}{statSuffix}")
    pVars.pierceS.set(getPrefs(PREF, 'STATISTICS', 'Pierce count', 0, int))
    pVars.pierceJ.set('0')
    pVars.pierceT.set(pVars.pierceS.get())
    pVars.rapidS.set(int(getPrefs(PREF, 'STATISTICS', 'Rapid time', 0, float)))
    pVars.rapidJ.set('00:00:00')
    pVars.rapidT.set(secs_to_hms(pVars.rapidS.get()))
    pVars.probeS.set(int(getPrefs(PREF, 'STATISTICS', 'Probe time', 0, float)))
    pVars.probeJ.set('00:00:00')
    pVars.probeT.set(secs_to_hms(pVars.probeS.get()))
    pVars.torchS.set(int(getPrefs(PREF, 'STATISTICS', 'Torch on time', 0, float)))
    pVars.torchJ.set('00:00:00')
    pVars.torchT.set(secs_to_hms(pVars.torchS.get()))
    pVars.pausedS.set(int(getPrefs(PREF, 'STATISTICS', 'Paused time', 0, float)))
    pVars.pausedJ.set('00:00:00')
    pVars.pausedT.set(secs_to_hms(pVars.pausedS.get()))
    pVars.cutS.set(int(getPrefs(PREF, 'STATISTICS', 'Cut time', 0, float)))
    pVars.cutJ.set('00:00:00')
    pVars.cutT.set(secs_to_hms(pVars.cutS.get()))
    pVars.runS.set(int(getPrefs(PREF, 'STATISTICS', 'Program run time', 0, float)))
    pVars.runJ.set('00:00:00')
    pVars.runT.set(secs_to_hms(pVars.runS.get()))
    laser_button_enable()
    offset_probe_setup()
    for widget in wLeds:
        rE(f"{widget} configure -state disabled")
        widgetValues[widget] = 0
    load_param_clicked()
    mode_changed()
    font_changed()
    ucFile = os.path.join(configPath, 'user_commands.py')
    if os.path.isfile(ucFile):
        exec(compile(open(ucFile, "rb").read(), ucFile, 'exec'))
else:
    firstRun = 'invalid'
    title = _('LOAD ERROR')
    msg0 = _('Cannot find valid library directory')
    msg1 = _('plasmac2 extensions are not loaded')
    sys.exit()


##############################################################################
# HAL SETUP - CALLED DIRECTLY FROM AXIS ONCE AT STARTUP                      #
##############################################################################
def user_hal_pins():
    global firstRun, previewSize, halPinList, serial, comPorts
    global materialChangePin, materialChangeNumberPin, materialChangeTimeoutPin
    global materialReloadPin, materialTempPin
    if firstRun == 'invalid':
        return
    # create new hal pins
    comp.newpin('arc-voltage', hal.HAL_FLOAT, hal.HAL_IN)
    comp.newpin('led-arc-ok', hal.HAL_BIT, hal.HAL_IN)
    comp.newpin('led-torch', hal.HAL_BIT, hal.HAL_IN)
    comp.newpin('led-thc-enabled', hal.HAL_BIT, hal.HAL_IN)
    comp.newpin('led-ohmic', hal.HAL_BIT, hal.HAL_IN)
    comp.newpin('led-float', hal.HAL_BIT, hal.HAL_IN)
    comp.newpin('led-breakaway', hal.HAL_BIT, hal.HAL_IN)
    comp.newpin('led-thc-active', hal.HAL_BIT, hal.HAL_IN)
    comp.newpin('led-up', hal.HAL_BIT, hal.HAL_IN)
    comp.newpin('led-down', hal.HAL_BIT, hal.HAL_IN)
    comp.newpin('led-corner-locked', hal.HAL_BIT, hal.HAL_IN)
    comp.newpin('led-void-locked', hal.HAL_BIT, hal.HAL_IN)
    comp.newpin('refresh', hal.HAL_S32, hal.HAL_IN)
    comp.newpin('material-change-number', hal.HAL_S32, hal.HAL_IN)
    comp.newpin('material-change', hal.HAL_S32, hal.HAL_IN)
    comp.newpin('material-change-timeout', hal.HAL_BIT, hal.HAL_IN)
    comp.newpin('material-reload', hal.HAL_BIT, hal.HAL_IN)
    comp.newpin('material-temp', hal.HAL_S32, hal.HAL_IN)
    comp.newpin('cut-type', hal.HAL_S32, hal.HAL_IN)
    comp.newpin('thc-enable-out', hal.HAL_BIT, hal.HAL_OUT)
    comp.newpin('program-is-idle', hal.HAL_BIT, hal.HAL_IN)
    comp.newpin('laser-on', hal.HAL_BIT, hal.HAL_OUT)
    comp.newpin('preview-tab', hal.HAL_BIT, hal.HAL_IN)
    comp.newpin('development', hal.HAL_BIT, hal.HAL_IN)
    comp.newpin('conv-block-loaded', hal.HAL_BIT, hal.HAL_IN)
    comp.newpin('offset-set-probe', hal.HAL_BIT, hal.HAL_OUT)
    comp.newpin('offset-set-scribe', hal.HAL_BIT, hal.HAL_OUT)
    ext_hal_create()
    # create some new signals and connect pins
    hal_data = [[0,'plasmac:arc-voltage-out','plasmac.arc-voltage-out','axisui.arc-voltage'],\
                [1,'plasmac:axis-x-min-limit','ini.x.min_limit','plasmac.axis-x-min-limit'],\
                [2,'plasmac:axis-x-max-limit','ini.x.max_limit','plasmac.axis-x-max-limit'],\
                [3,'plasmac:axis-y-min-limit','ini.y.min_limit','plasmac.axis-y-min-limit'],\
                [4,'plasmac:axis-y-max-limit','ini.y.max_limit','plasmac.axis-y-max-limit'],\
                [5,'plasmac:axis-z-min-limit','ini.z.min_limit','plasmac.axis-z-min-limit'],\
                [6,'plasmac:axis-z-max-limit','ini.z.max_limit','plasmac.axis-z-max-limit'],\
                [7,'plasmac:arc-ok-out','plasmac.arc-ok-out','axisui.led-arc-ok'],\
                [8,'plasmac:thc-enabled','plasmac.thc-enabled','axisui.led-thc-enabled'],\
                [9,'plasmac:thc-active','plasmac.thc-active','axisui.led-thc-active'],\
                [10,'plasmac:led-up','plasmac.led-up','axisui.led-up'],\
                [11,'plasmac:led-down','plasmac.led-down','axisui.led-down'],\
                [11,'plasmac:cornerlock-is-locked','plasmac.cornerlock-is-locked','axisui.led-corner-locked'],\
                [13,'plasmac:voidlock-is-locked','plasmac.voidlock-is-locked','axisui.led-void-locked'],\
                [14,'plasmac:offset-set-probe','plasmac.offset-set-probe','axisui.offset-set-probe'],\
                [15,'plasmac:offset-set-scribe','plasmac.offset-set-scribe','axisui.offset-set-scribe'],\
                ]
    for line in hal_data:
        if line[0] < 7:
            hal.new_sig(line[1],hal.HAL_FLOAT)
        else:
            hal.new_sig(line[1],hal.HAL_BIT)
        hal.connect(line[2],line[1])
        hal.connect(line[3],line[1])
    # connect pins to some existing signals
    hal.connect('axisui.led-ohmic','plasmac:ohmic-probe-out')
    hal.connect('axisui.led-float','plasmac:float-switch-out')
    hal.connect('axisui.led-breakaway','plasmac:breakaway-switch-out')
    hal.connect('axisui.led-torch','plasmac:torch-on')
    # connect laser-on if it exists
    laser = False
    for signal in hal.get_info_signals():
        if signal['NAME'] == 'plasmac:laser-on':
            laser = True
            break
    if laser:
        hal.connect('axisui.laser-on','plasmac:laser-on')
    # initialize halpin variables
    materialChangePin = comp['material-change']
    materialChangeNumberPin = comp['material-change-number']
    materialChangeTimeoutPin = comp['material-change-timeout']
    materialReloadPin = comp['material-reload']
    materialTempPin = comp['material-temp']
    # do user button setup after hal pin creation
    halPinList = hal.get_info_pins()
    user_button_setup()
    # load materials when setup is complete
    load_materials(-1, False)
    # start powermax comms if valid port
    if pmPort:
        global pmx485
        for parm in ['compStatus','commsError','connected','exists','meshMode']:
            pmx485[parm] = False
        for parm in ['compArcTime','compFault','commsTimer','retryTimer','oldPressure']:
            pmx485[parm] = 0.0
        for parm in ['compMinC','compMaxC','compMinP','compMaxP']:
            pmx485[parm] = 0.0
        for parm in ['oldMode']:
            pmx485[parm] = 0
        try:
            import serial
            import serial.tools.list_ports as comPorts
            if pmx485_check(pmPort):
                pmx485_startup(pmPort)
        except:
            comPorts = None
            msg0 = _('python3-serial cannot be found')
            msg1 = _('Install python3-serial or linuxcnc-dev')
            notifications.add('error', f"{msg0}\n{msg1}")
    # check preferences for a file to load
    addRecent = True
    openFile = getPrefs(PREF, 'GUI_OPTIONS','Open file', '', str)
    loadLast = True if openFile == 'last' else False
    if loadLast:
        openFile = ap.getpref('recentfiles', [], repr).pop(0) if len(ap.getpref('recentfiles', [], repr)) else None
        addRecent = False
    elif openFile:
        openFile = os.path.join(open_directory,openFile)
    # load the file
    if openFile:
        if os.path.exists(openFile):
            open_file_guts(openFile, False, addRecent)
            commands.set_view_z()
        else:
            msg0 = _('does not exist')
            notifications.add('error', f"'{os.path.realpath(openFile)}' {msg0}\n")
    # run users custom hal commands if it exists
    uhFile = os.path.join(configPath, 'user_hal.py')
    if os.path.isfile(uhFile):
        exec(compile(open(uhFile, "rb").read(), uhFile, 'exec'))
    # setup shortcuts help
    install_kb_text(root_window)
    install_kp_text(root_window)
    # setup the about text
    rE('.about.message configure -height 14')
    text = f"\n\nplasmac2 extensions v{VER}\nCopyright (C) 2022, 2023\nPhillip A Carter and Gregory D Carl"
    rE('.about.message configure -state normal')
    rE(f".about.message insert end {{{text}}}")
    rE('.about.message configure -state disabled')
    previewSize = {'w':rE(f"winfo width {tabs_preview}"), 'h':rE(f"winfo height {tabs_preview}")}


##############################################################################
# PERIODIC FUNCTION - CALLED FROM AXIS EVERY CYCLE                           #
##############################################################################
def user_live_update():
    global firstRun, upFile
    # don't do any updates until first run is complete.
    if firstRun:
        # test this last command in axis to see if we are loaded
        if widgets.numbers_text.bind() and extHalPins:
            upFile = os.path.join(configPath, 'user_periodic.py')
            # setup the colors
            color_change()
            # set the pierce offset hal pins
            comp['x-pierce-offset'] = widgetValues['.param.c2.pierce.x-pierce-offset']
            comp['y-pierce-offset'] = widgetValues['.param.c2.pierce.y-pierce-offset']
            firstRun = None
        return
    if orientStart:
        return
    global isIdle, isIdleHomed, isPaused, isRunning
    global runState, pausedState, relPos, previewSize
    global probeTimer, laserButtonState
    global framingState, activeFunction
    global currentTool, pmx485, homeInProgress
    global materialChangePin, materialChangeNumberPin
    global materialReloadPin, materialTempPin
    global materialChangeTimeoutPin, dryRun
    # set machine state variables
    isIdle = s.task_state == linuxcnc.STATE_ON and s.interp_state == linuxcnc.INTERP_IDLE
    isIdleHomed = isIdle and all_homed()
    isPaused = s.task_state == linuxcnc.STATE_ON and s.paused
    isRunning = not s.interp_state == linuxcnc.INTERP_IDLE and not s.paused
    # check external hal pins
    ext_hal_watch()
    # set current x and y relative positions
    relPos['X'] = round(s.position[0] - s.g5x_offset[0] - s.g92_offset[0], 6)
    relPos['Y'] = round(s.position[1] - s.g5x_offset[1] - s.g92_offset[1], 6)
    # set display units
    if o.canon:
        if 200 in o.canon.state.gcodes:
            vars.metric.set(0)
        else:
            vars.metric.set(1)
    else:
        vars.metric.set(s.linear_units == 1)
    # program has started
    if running() and not runState:
        runState = True
        clear_job_stats()
    # program has ended
    elif not running() and runState:
        runState = False
        save_total_stats()
        # reset single-cut
        if singleCut['G91']:
            ensure_mode(linuxcnc.MODE_MDI)
            c.mdi('G91')
        # reset moving pierce
        for pin in ['creep-speed', 'creep-speed-distance', 'cut-height-delay', 'gouge-speed', \
                    'gouge-speed-distance', 'pierce-end-height', 'pierce-motion-delay', 'pierce-type']:
            hal.set_p(f'plasmac.{pin}', '0')
    # override standard tool info
    if current_tool.id != currentTool:
        currentTool = current_tool.id
        if currentTool < 1:
            tool = 'Torch'
        elif currentTool == 1:
            tool = 'Scribe'
        vupdate(pVars.plasmatool, f"Tool: {tool}")
    # update status leds
    for widget in wLeds:
        tmp, item = widget.rsplit('.',1)
        if comp[item] != widgetValues[widget]:
            widgetValues[widget] = comp[item]
            if comp[item] == 1:
                rE(f"{widget} configure -state normal")
            else:
                rE(f"{widget} configure -state disabled")
    # update arc voltage
    rE(f"{fplasma}.arc-voltage configure -text {comp['arc-voltage']:3.0f}v")
    # halpin toggle
    for button in togglePins:
        if hal.get_value(togglePins[button]['pin']) != togglePins[button]['state']:
            set_toggle_pins(button)
    # halpin pulse
    for button in pulsePins:
        # set button color for pulse-halpin buttons
        if hal.get_value(pulsePins[button]['pin']) != pulsePins[button]['state']:
            pulsePins[button]['state'] = hal.get_value(pulsePins[button]['pin'])
            if pulsePins[button]['state']:
                rE(f"{fbuttons}.button{button} configure -bg {colorActive}")
            else:
                rE(f"{fbuttons}.button{button} configure -bg {colorBack}")
    # reset a consumable change
    if (hal.get_value('plasmac.consumable-change') or hal.get_value('plasmac.consumable-change')) and \
       (hal.get_value('axisui.abort') or not s.paused):
        hal.set_p('plasmac.consumable-change', '0')
        hal.set_p('plasmac.x-offset', '0')
        hal.set_p('plasmac.y-offset', '0')
        hal.set_p('plasmac.xy-feed-rate', '0')
        rE(f"{fbuttons}.button{cChangeButton} configure -bg {colorBack}")
        activeFunction = False
    # abort a manual cut
    if manualCut['state'] and not hal.get_value('spindle.0.on'):
        manualCut['state'] = False
        vars.jog_speed.set(manualCut['feed'])
        rE(f".pane.top.jogspeed.s configure -state normal")
        save_total_stats()
    # show cut recovery tab
    if s.paused and 'manual' in rE(f"{ftabs} pages"):
        rE(f"{ftabs} delete manual 0")
        rE(f"{ftabs} delete mdi 0")
        rE(f"{ftabs} insert end cutrecs")
        rE(f"{ftabs} raise cutrecs")
        rE(f"{ftabs} itemconfigure cutrecs -text {{{_('Cut Recovery')}}} -background {colorBack} -foreground {colorFore}")
    # hide cut recovery tab
    if not s.paused and 'cutrecs' in  rE(f"{ftabs} pages") and \
       (not hal.get_value('plasmac.paused-motion') or \
       hal.get_value('axisui.abort') or \
       s.task_state == linuxcnc.STATE_ESTOP):
        rE(f"{ftabs} delete cutrecs 0")
        rE(f"{ftabs} insert end manual")
        rE(f"{ftabs} insert end mdi")
        rE(f"{ftabs} raise manual")
        rE(f"{ftabs} itemconfigure manual -text {{{_('Manual')}}} -background {colorBack} -foreground {colorFore}")
        rE(f"{ftabs} itemconfigure mdi -text {{{_('MDI')}}} -background {colorBack} -foreground {colorFore}")
    # if resumed and component has completed a cut recovery then cancel cut recovey
    if not s.paused and not hal.get_value('plasmac.cut-recovering') and hal.get_value('plasmac.cut-recovery'):
        cut_rec_cancel()
    # if consumable change finished then clear offsets
    if hal.get_value('plasmac.state-out') == 26:
        hal.set_p('plasmac.x-offset', '0')
        hal.set_p('plasmac.y-offset', '0')
    # if resumed and laser recovery is active then undo laser offset
    if not s.paused and  hal.get_value('plasmac.laser-recovery-state') and hal.get_value('plasmac.laser-recovery-start'):
        hal.set_p('plasmac.laser-recovery-start', '0')
    # reset framing when complete
    if framingState and not hal.get_value('motion.motion-type'):
        comp['laser-on'] = 0
        framingState = False
        activeFunction = False
        live_plotter.clear()
    # dry run button state
    wState = 'disabled' if rE('.toolbar.program_run cget -state') == 'disabled' or not loaded_file else 'normal'
    rE(f"{fbuttons}.dry-run configure -state {wState}")
    # reset after dry run
    if dryRun and isIdle:
        oldX = s.actual_position[0] - dryRun[0]
        oldY = s.actual_position[1] - dryRun[1]
        units = '20' if s.linear_units != 1 else '21'
        ensure_mode(linuxcnc.MODE_MDI)
        c.mdi(f'G{units} G10 L20 P0 X{oldX} Y{oldY}')
        c.wait_complete()
        dryRun = None
        comp['laser-on'] = 0
        hal.set_p('plasmac.dry-run', '0')
        live_plotter.clear()
    # set X0Y0, laser, and offset_setup buttons state
    wState = 'normal' if isIdleHomed else 'disabled'
    rE(f"{fjogf}.zerohome.zeroxy configure -state {wState}")
    rE(f"{fjogf}.zerohome.laser configure -state {wState}")
    rE(f"{fsetup}.m.utilities.offsets configure -state {wState}")
    # set height override buttons state
    wState = 'normal' if isIdle or isPaused or isRunning else 'disabled'
    rE(f"{foverride}.raise configure -state {wState}")
    rE(f"{foverride}.lower configure -state {wState}")
    rE(f"{foverride}.reset configure -state {wState}")
    # set material panel state
    wState = 'normal' if not (isPaused or isRunning) else 'disabled'
    rE(f"{fruns}.material.materials configure -state {wState}")
    rE(f"{fruns}.material.cut-feed-rate configure -state {wState}")
    for child in get_all_children(toolmat):
        rE(f"{child} configure -state {wState}")
    for n in range(1, maxUserButtons + 1):
        if buttonCodes[n]['code']:
            if buttonCodes[n]['code'] == 'ohmic-test':
                if (isIdle or isPaused) and hal.get_value('plasmac.ohmic-probe-enable'):
                    rE(f"{fbuttons}.button{n} configure -state normal")
                else:
                    rE(f"{fbuttons}.button{n} configure -state disabled")
            elif buttonCodes[n]['code'] == 'cut-type':
                if not isRunning and not isPaused:
                    rE(f"{fbuttons}.button{n} configure -state normal")
                else:
                    rE(f"{fbuttons}.button{n} configure -state disabled")
            elif buttonCodes[n]['code'] == 'single-cut':
                if isIdleHomed and not activeFunction and str(rE('.toolbar.program_run cget -state')) == 'normal':
                    rE(f"{fbuttons}.button{n} configure -state normal")
                else:
                    rE(f"{fbuttons}.button{n} configure -state disabled")
            elif buttonCodes[n]['code'] == 'manual-cut':
                if isIdleHomed and not activeFunction and str(rE('.toolbar.program_run cget -state')) == 'normal':
                    rE(f"{fbuttons}.button{n} configure -state normal")
                else:
                    rE(f"{fbuttons}.button{n} configure -state disabled")
            elif buttonCodes[n]['code'] == 'probe-test':
                if isIdleHomed and (not activeFunction or probeTimer):
                    rE(f"{fbuttons}.button{n} configure -state normal")
                else:
                    rE(f"{fbuttons}.button{n} configure -state disabled")
            elif buttonCodes[n]['code'] == 'torch-pulse':
                if (isIdleHomed or isPaused) and not activeFunction and hal.get_value('plasmac.torch-enable'):
                    rE(f"{fbuttons}.button{n} configure -state normal")
                else:
                    rE(f"{fbuttons}.button{n} configure -state disabled")
            elif buttonCodes[n]['code'] == 'change-consumables':
                if isPaused and (not activeFunction or hal.get_value('plasmac.consumable-change')):
                    rE(f"{fbuttons}.button{n} configure -state normal")
                else:
                    rE(f"{fbuttons}.button{n} configure -state disabled")
            elif buttonCodes[n]['code'] == 'framing':
                if isIdleHomed and not activeFunction:
                    rE(f"{fbuttons}.button{n} configure -state normal")
                else:
                    rE(f"{fbuttons}.button{n} configure -state disabled")
            elif buttonCodes[n]['code'] == 'load':
                if isIdleHomed and not activeFunction:
                    rE(f"{fbuttons}.button{n} configure -state normal")
                else:
                    rE(f"{fbuttons}.button{n} configure -state disabled")
            elif buttonCodes[n]['code'] == 'latest-file':
                if isIdleHomed and not activeFunction:
                    rE(f"{fbuttons}.button{n} configure -state normal")
                else:
                    rE(f"{fbuttons}.button{n} configure -state disabled")
            elif buttonCodes[n]['code'] == 'pulse-halpin':
                if hal.get_value('halui.machine.is-on'):
                    rE(f"{fbuttons}.button{n} configure -state normal")
                else:
                    rE(f"{fbuttons}.button{n} configure -state disabled")
            elif buttonCodes[n]['code'] == 'toggle-halpin':
                if hal.get_value('halui.machine.is-on'):
                    rE(f"{fbuttons}.button{n} configure -state normal")
                else:
                    rE(f"{fbuttons}.button{n} configure -state disabled")
            elif 'shell' in buttonCodes[n]['code'][0]:
                if hal.get_value('halui.machine.is-on'):
                    rE(f"{fbuttons}.button{n} configure -state normal")
                else:
                    rE(f"{fbuttons}.button{n} configure -state disabled")
            else:
                if isIdleHomed and not activeFunction:
                    rE(f"{fbuttons}.button{n} configure -state normal")
                else:
                    rE(f"{fbuttons}.button{n} configure -state disabled")
        wState = 'normal' if not (isPaused or isRunning) else 'disabled'
        rE(f"{fsetup}.r.ubuttons.canvas.frame.name{n} configure -state {wState}")
        rE(f"{fsetup}.r.ubuttons.canvas.frame.code{n} configure -state {wState}")

    # material handling
    if materialChangePin != comp['material-change']:
        materialChangePin = comp['material-change']
        material_change_pin_changed(materialChangePin)
    if materialChangeNumberPin != comp['material-change-number']:
        materialChangeNumberPin = comp['material-change-number']
        material_change_number_pin_changed(materialChangeNumberPin)
    if materialChangeTimeoutPin != comp['material-change-timeout']:
        materialChangeTimeoutPin = comp['material-change-timeout']
        material_change_timeout_pin_changed(materialChangeTimeoutPin)
    if materialReloadPin != comp['material-reload']:
        materialReloadPin = comp['material-reload']
        material_reload_pin_changed(materialReloadPin)
    if materialTempPin != comp['material-temp']:
        materialTempPin = comp['material-temp']
        material_temp_pin_changed(materialTempPin)
    # try to show the preview tab if hal pin is set
    if comp['preview-tab']:
        try:
            rE('.pane.top.right raise preview')
            comp['preview-tab'] = 0
        except:
            pass
    # allows user to set a HAL pin to initiate the sequence of:
    # reloading the program
    if comp['refresh'] == 1:
        comp['refresh'] = 2
        commands.reload_file()
    # clearing the live plot
    elif comp['refresh'] == 2:
        comp['refresh'] = 3
        commands.clear_live_plot()
    # rezooming the axis
    elif comp['refresh'] == 3:
        comp['refresh'] = 0
        commands.set_view_z()
    # powermax signals
    if pmx485['exists']:
        if pmx485['compStatus'] != hal.get_value('pmx485.status'):
            pmx485['compStatus'] = hal.get_value('pmx485.status')
            pmx485_status_changed(pmx485['compStatus'])
        if pmx485['compFault'] != hal.get_value('pmx485.fault'):
            pmx485['compFault'] = hal.get_value('pmx485.fault')
            pmx485_fault_changed(pmx485['compFault'])
        if pmx485['compArcTime'] != hal.get_value('pmx485.arcTime'):
            pmx485['compArcTime'] = hal.get_value('pmx485.arcTime')
            pVars.arcT.set(secs_to_hms(pmx485['compArcTime']))
        if pmx485['compMinC'] != hal.get_value('pmx485.current_min'):
            pmx485['compMinC'] = hal.get_value('pmx485.current_min')
            pmx485_min_current_changed()
        if pmx485['compMaxC'] != hal.get_value('pmx485.current_max'):
            pmx485['compMaxC'] = hal.get_value('pmx485.current_max')
            pmx485_max_current_changed()
        if pmx485['compMaxP'] != hal.get_value('pmx485.pressure_max'):
            pmx485['compMaxP'] = hal.get_value('pmx485.pressure_max')
            pmx485_max_pressure_changed()
        if pmx485['commsTimer']:
            pmx485['commsTimer'] -= 0.1
            if pmx485['commsTimer'] <= 0:
                pmx485['commsTimer'] = 0
                pmx485_comms_timeout()
        if pmx485['retryTimer']:
            pmx485['retryTimer'] -= 0.1
            if pmx485['retryTimer'] <= 0:
                pmx485['retryTimer'] = 0
                pmx485_retry_timeout()
    # hide/show override limits checkbox
    if vars.on_any_limit.get() and not rE(f"winfo ismapped {flimitovr}"):
        homeInProgress = False
        for j in range(jointcount):
            if hal.get_value(f"joint.{j}.homing"):
                homeInProgress = True
                break
        if not homeInProgress:
            rE(f"grid {flimitovr} -row 1 -column 0 -columnspan 3 -sticky nsew")
    elif not vars.on_any_limit.get() and rE(f"winfo ismapped {flimitovr}"):
        rE(f"{flimitovr}.button invoke")
        rE(f"grid forget {flimitovr}")
    # hide/show jog inhibit override checkbox
    if rE(f"winfo ismapped {fjogiovr}"):
        if pVars.jogInhibitOvr.get():
            hal.set_p('plasmac.override-jog', '1')
        if not hal.get_value('plasmac.breakaway') and \
           not hal.get_value('plasmac.float-switch') and \
           not hal.get_value('plasmac.ohmic-probe'):
            pVars.jogInhibitOvr.set(False)
            rE(f"grid forget {fjogiovr}")
    else:
        if hal.get_value('plasmac.jog-inhibit'):
            if isIdleHomed:
                rE(f"grid {fjogiovr} -column 0 -row 1 -columnspan 3 -sticky nsew")
        else:
            if not hal.get_value('plasmac.breakaway') and \
               not hal.get_value('plasmac.float-switch') and \
               not hal.get_value('plasmac.ohmic-probe'):
                hal.set_p('plasmac.override-jog', '0')
    # statistics
    if runState or manualCut['state']:
        # cut length
        sNow = int(hal.get_value('plasmac.cut-length') + 0.5)
        if statValues['length'] != sNow:
            if sNow:
                pVars.lengthJ.set(f"{sNow / statDivisor:0.2f}{statSuffix}")
                if hal.get_value('plasmac.torch-enable'):
                    pVars.lengthT.set(f"{(sNow + pVars.lengthS.get()) / statDivisor:0.2f}{statSuffix}")
            statValues['length'] = sNow
        # pierce count
        sNow = hal.get_value('plasmac.pierce-count')
        if statValues['pierce'] != sNow:
            if sNow:
                pVars.pierceJ.set(sNow)
                if hal.get_value('plasmac.torch-enable'):
                    pVars.pierceT.set(sNow + pVars.pierceS.get())
            statValues['pierce'] = sNow
        # rapid time
        sNow = int(hal.get_value('plasmac.rapid-time') + 0.5)
        if statValues['rapid'] != sNow:
            if sNow:
                pVars.rapidJ.set(secs_to_hms(sNow))
                if hal.get_value('plasmac.torch-enable'):
                    pVars.rapidT.set(secs_to_hms(sNow + pVars.rapidS.get()))
            statValues['rapid'] = sNow
        # probe time
        sNow = int(hal.get_value('plasmac.probe-time') + 0.5)
        if statValues['probe'] != sNow:
            if sNow:
                pVars.probeJ.set(secs_to_hms(sNow))
                if hal.get_value('plasmac.torch-enable'):
                    pVars.probeT.set(secs_to_hms(sNow + pVars.probeS.get()))
            statValues['probe'] = sNow
        # torch time
        sNow = int(hal.get_value('plasmac.torch-time') + 0.5)
        if statValues['torch'] != sNow:
            if sNow:
                pVars.torchJ.set(secs_to_hms(sNow))
                if hal.get_value('plasmac.torch-enable'):
                    pVars.torchT.set(secs_to_hms(sNow + pVars.torchS.get()))
            statValues['torch'] = sNow
        # cut time
        sNow = int(hal.get_value('plasmac.cut-time') + 0.5)
        if statValues['cut'] != sNow:
            if sNow:
                pVars.cutJ.set(secs_to_hms(sNow))
                if hal.get_value('plasmac.torch-enable'):
                    pVars.cutT.set(secs_to_hms(sNow + pVars.cutS.get()))
            statValues['cut'] = sNow
        # paused time
        sNow = int(hal.get_value('plasmac.paused-time') + 0.5)
        if statValues['paused'] != sNow:
            if sNow:
                pVars.pausedJ.set(secs_to_hms(sNow))
                if hal.get_value('plasmac.torch-enable'):
                    pVars.pausedT.set(secs_to_hms(sNow + pVars.pausedS.get()))
            statValues['paused'] = sNow
        # run time
        sNow = int(hal.get_value('plasmac.run-time') + 0.5)
        if statValues['run'] != sNow:
            if sNow:
                pVars.runJ.set(secs_to_hms(sNow))
                if hal.get_value('plasmac.torch-enable'):
                    pVars.runT.set(secs_to_hms(sNow + pVars.runS.get()))
            statValues['run'] = sNow
    # set table view zoom if window resized
    if get_view_type() == 't' and (previewSize['w'] != rE(f"winfo width {tabs_preview}") or previewSize['h'] != rE(f"winfo height {tabs_preview}")):
        previewSize = {'w':rE(f"winfo width {tabs_preview}"), 'h':rE(f"winfo height {tabs_preview}")}
        root_window.update_idletasks()
        commands.set_view_t()
    # show/hide scrollbars for user buttons setup
    if  int(rE(f"winfo ismapped {fsetup}")):
        if int(rE(f"winfo width {fsetup}.r.ubuttons.canvas.frame")) > \
           int(rE(f"winfo width {fsetup}.r")) - \
           (20 * int(rE(f"winfo ismapped {fsetup}.r.ubuttons.yscroll"))):
            if not int(rE(f"winfo ismapped {fsetup}.r.ubuttons.xscroll")):
                rE(f"grid {fsetup}.r.ubuttons.xscroll -column 1 -row 1 -sticky new -padx {{4 0}}")
        else:
            if int(rE(f"winfo ismapped {fsetup}.r.ubuttons.xscroll")):
                rE(f"grid forget {fsetup}.r.ubuttons.xscroll")
        if int(rE(f"winfo height {fsetup}.r.ubuttons.canvas.frame")) > \
           int(rE(f"winfo height {fsetup}.r.ubuttons")) - \
           (int(pVars.fontSize.get()) * 2 + 16 * int(rE(f"winfo ismapped {fsetup}.r.ubuttons.xscroll"))):
            if not int(rE(f"winfo ismapped {fsetup}.r.ubuttons.yscroll")):
                rE(f"grid {fsetup}.r.ubuttons.yscroll -column 0 -row 0 -sticky nsw -padx {{4 0}}")
        else:
            if int(rE(f"winfo ismapped {fsetup}.r.ubuttons.yscroll")):
                rE(f"grid forget {fsetup}.r.ubuttons.yscroll")
        cbbox = rE(f"{fsetup}.r.ubuttons.canvas bbox all")
        rE(f"{fsetup}.r.ubuttons.canvas configure -scrollregion {{{cbbox}}}")
    # show/hide virtual keyboard for materials
    if str(rE('focus'))[:15] == f"{fruns}.material.":
        if pVars.useVirtKB.get() and not vkbData['visible']:
            vkb_show('numpad')
    else:
        if vkbData['visible'] and not vkbData['required']:
            vkb_hide()
    # resize manual tab
    ttWidth = int(rE(f"winfo width {fjogf}.zerohome.home")) + int(rE(f"winfo width {fjogf}.zerohome.zero")) * 3 + 18
    rE(f".pane.top.tabs configure -width {ttWidth}")
    # start powermax comms if a valid port exists and it is not yet started
    if pmPort and not hal.component_exists('pmx485') and pmx485_check(pmPort, True):
        pmx485_startup(pmPort)
    # run users custom periodic commands if it exists
    if os.path.isfile(upFile):
        exec(open(upFile).read())
    o.tkRedraw()
