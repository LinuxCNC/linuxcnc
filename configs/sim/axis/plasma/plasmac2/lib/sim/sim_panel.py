#!/usr/bin/python3

'''
SIMULATOR PANEL FOR PLASMAC CONFIGS
'''

from tkinter import *
from tkinter.constants import *
from subprocess import call as CALL
import linuxcnc
import hal
import signal
import time

def unload(e):
    print('\nPreceding KeyboardInterrupt exception can be disregarded')
    raise SystemExit('f{e}\nunloading plasmac sim panel\n')

def time_out(signum, frame):
    unload('axis gui was closed')

def arc_voltage_changed(value):
    try:
        hal.set_p('sim-torch.offset-in', value)
    except Exception as e:
        unload(e)

def button_changed(event, button, pin):
    try:
        global buttonStart, arcOkPressed
        if event.type == EventType.Button:
            buttonStart = time.time()
            hal.set_p(pin, '1')
            if mode > 0:
                if button == arcOkB:
                    arcOkPressed = True
            if button == breakB:
                button.configure(background=buttonRd)
                button.configure(activebackground=buttonRd)
            else:
                button.configure(background=buttonGn)
                button.configure(activebackground=buttonGn)
        elif time.time() - buttonStart < 1:
            hal.set_p(pin, '0')
            if mode > 0:
                if button == arcOkB:
                    arcOkPressed = False
            button.configure(background=buttonBg)
            button.configure(activebackground=buttonBg)
    except Exception as e:
        unload(e)

def mode_change(mode):
    arcF.pack_forget()
    switchF.pack_forget()
    okF.pack_forget()
    moveF.pack_forget()
    if mode == 0:
        arcF.pack(padx=2,pady=2,fill='x')
        switchF.pack(padx=2,pady=2,fill='x')
    if mode == 1:
        arcF.pack(padx=2,pady=2,fill='x')
        switchF.pack(padx=2,pady=2,fill='x')
        okF.pack(padx=2,pady=2,fill='x')
    if mode == 2:
        switchF.pack(padx=2,pady=2,fill='x')
        okF.pack(padx=2,pady=2,fill='x')
        moveF.pack(padx=2,pady=2,fill='x')#, expand=True)

def periodic():
    try:
        global mode
    # handle window updates on mode changes
        current_mode = hal.get_value('plasmac.mode')
        if mode != current_mode:
            mode = current_mode
            mode_change(mode)
    # activate float or ohmic when probing
        if hal.get_value('plasmac.state-out') == 2 and \
           hal.get_value('plasmac-sim.z-position') < probeHeight:
            if selectedSensor.get():
                hal.set_p('db_float.in', '1')
                floatB.configure(activebackground=buttonGn)
                floatB.configure(bg=buttonGn)
            elif hal.get_value('plasmac.ohmic-probe-enable'):
                hal.set_p('db_ohmic.in', '1')
                ohmicB.configure(activebackground=buttonGn)
                ohmicB.configure(bg=buttonGn)
        elif hal.get_value('plasmac.state-out') == 2 or hal.get_value('plasmac.state-out') == 3 and \
            hal.get_value('plasmac-sim.z-position') > probeHeight:
            hal.set_p('db_float.in', '0')
            floatB.configure(activebackground=buttonBg)
            floatB.configure(bg=buttonBg)
            hal.set_p('db_ohmic.in', '0')
            ohmicB.configure(activebackground=buttonBg)
            ohmicB.configure(bg=buttonBg)
    # set arc ok if mode = 1 or 2
        if mode > 0:
            if not arcOkPressed:
                if mode and hal.get_value('plasmac.torch-on'):
                    hal.set_p('db_arc-ok.in', '1')
                    arcOkB.configure(background=buttonGn)
                    arcOkB.configure(activebackground=buttonGn)
                else:
                    hal.set_p('db_arc-ok.in', '0')
                    arcOkB.configure(background=buttonBg)
                    arcOkB.configure(activebackground=buttonBg)
        # if axis is running do it all again in 100mS
        if hal.component_exists('axisui') or hal.component_exists('qtplasmac'):
            signal.alarm(1)
            window.after(100, periodic)
    except Exception as e:
        unload(e)

def estop_pressed(event):
    print('estop pressed')
    if hal.get_value('estop_or.in0') == 0:
        hal.set_p('estop_or.in0', '1')
        estopB.configure(background=buttonRd)
        estopB.configure(activebackground=buttonRd)
    else:
        hal.set_p('estop_or.in0', '0')
        estopB.configure(background=buttonGn)
        estopB.configure(activebackground=buttonGn)

try:
    window = Tk()
    window.title('Plasmac Sim')
    window.resizable(False,False)
    window.attributes('-topmost', True)
    S = linuxcnc.stat()
    S.poll()
    C = linuxcnc.command()
    I = linuxcnc.ini(S.ini_filename)
    # create new hal component
    H = hal.component('plasmac-sim')
    H.newpin('z-position', hal.HAL_FLOAT, hal.HAL_IN)
    H.ready()
    # disconnect any conflicting hal pins
    if hal.pin_has_writer('db_arc-ok.in'):
        CALL(['halcmd','unlinkp','db_arc-ok.in'])
    if hal.pin_has_writer('db_float.in'):
        CALL(['halcmd','unlinkp','db_float.in'])
    if hal.pin_has_writer('db_ohmic.in'):
        CALL(['halcmd','unlinkp','db_ohmic.in'])
    if hal.pin_has_writer('db_breakaway.in'):
        CALL(['halcmd','unlinkp','db_breakaway.in'])
    if hal.pin_has_writer('plasmac.move-up'):
        CALL(['halcmd','unlinkp','plasmac.move-up'])
    if hal.pin_has_writer('plasmac.move-down'):
        CALL(['halcmd','unlinkp','plasmac.move-down'])
    # load the simulated torch
    CALL(['halcmd', 'loadusr', '-Wn', 'sim-torch', 'sim-torch'])
    # connect to existing plasmac connections
    CALL(['halcmd', 'net', 'plasmac:torch-on', 'sim-torch.start'])
    CALL(['halcmd', 'net', 'plasmac:cut-volts', 'sim-torch.voltage-in'])
    # create new sim connection
    CALL(['halcmd', 'net', 'sim:arc-voltage-in', 'sim-torch.voltage-out', 'plasmac.arc-voltage-in'])
    # connect to existing z axis height
    simStepConf = False
    for sig in hal.get_info_signals():
        if sig['NAME'] == 'Zjoint-pos-fb':
            simStepconf = True
            break
    if simStepConf:
        CALL(['halcmd', 'net', 'Zjoint-pos-fb', 'plasmac-sim.z-position'])
    else:
        CALL(['halcmd', 'net', 'plasmac:axis-position', 'plasmac-sim.z-position'])
    # set button colors
    buttonRd = '#dd0000'
    buttonGn = '#00cc00'
    # set python variables
    arcOkPressed = False
    buttonStart = 0.0
    mode = None
    units = I.find('TRAJ','LINEAR_UNITS') or 'mm'
    if units == 'inch':
#        probeHeight = hal.get_value('ini.z.min_limit') + 0.4
        probeHeight = hal.get_value('ini.z.min_limit') + 0.8
    else:
#        probeHeight = hal.get_value('ini.z.min_limit') + 10
        probeHeight = hal.get_value('ini.z.min_limit') + 20
    # set tkinter variables
    selectedSensor = IntVar()
    # set up arc voltage frame
    arcF = LabelFrame(window)
    arcF.configure(relief='groove',text='Arc Voltage Offset',highlightbackground='#5e5e5e')
    arc_voltage = Scale(arcF, from_=-10, to=10, resolution=0.1)
    arc_voltage.configure(length='160',orient='horizontal',command=arc_voltage_changed, \
                          foreground='#0000dd',font='arial 14')
    arc_voltage.pack(padx=2,pady=2,fill='x')
    # set up sensor switch frame
    switchF = LabelFrame(window)
    switchF.configure(relief='groove',text='Switches')
    ohmicB = Button(switchF)
    ohmicB.configure(borderwidth='2',compound='left',padx=4,width=5,text='OHMIC')
    ohmicB.grid(row=0,column=0,sticky='ew')
    ohmicB.bind('<Button-1>',lambda e:button_changed(e,ohmicB,'db_ohmic.in'))
    ohmicB.bind('<ButtonRelease-1>',lambda e:button_changed(e,ohmicB,'db_ohmic.in'))
    floatB = Button(switchF)
    floatB.configure(borderwidth='2',compound='left',padx=4,width=5,text='FLOAT')
    floatB.grid(row=0,column=1,sticky='ew')
    floatB.bind('<Button-1>',lambda e:button_changed(e,floatB,'db_float.in'))
    floatB.bind('<ButtonRelease-1>',lambda e:button_changed(e,floatB,'db_float.in'))
    breakB = Button(switchF)
    breakB.configure(borderwidth='2',compound='left',padx=4,width=5,text='BREAK')
    breakB.grid(row=0,column=2,sticky='ew')
    breakB.bind('<Button-1>',lambda e:button_changed(e,breakB,'db_breakaway.in'))
    breakB.bind('<ButtonRelease-1>',lambda e:button_changed(e,breakB,'db_breakaway.in'))
    ohmicC = Radiobutton(switchF)
    ohmicC.configure(value='0',indicatoron='0',selectcolor=buttonGn)
    ohmicC.configure(variable=selectedSensor)
    ohmicC.grid(row=1,column=0,padx=16,pady=4,sticky='ew')
    floatC = Radiobutton(switchF)
    floatC.configure(value='1',indicatoron='0',selectcolor=buttonGn)
    floatC.configure(variable=selectedSensor)
    floatC.grid(row=1,column=1,padx=16,pady=4,sticky='ew')
    switchF.columnconfigure(0,weight=1)
    switchF.columnconfigure(1,weight=1)
    switchF.columnconfigure(2,weight=1)
    # set up arc ok frame
    okF = LabelFrame(window)
    okF.configure(relief='groove',text='Arc OK')
    arcOkB = Button(okF)
    arcOkB.configure(borderwidth='2',compound='left',text='ARC OK')
    arcOkB.pack(fill='x')
    arcOkB.bind('<Button-1>',lambda e:button_changed(e,arcOkB,'db_arc-ok.in'))
    arcOkB.bind('<ButtonRelease-1>',lambda e:button_changed(e,arcOkB,'db_arc-ok.in'))
    # set up external thc frame
    moveF = LabelFrame(window)
    moveF.configure(relief='groove',text='Move')
    upB = Button(moveF)
    upB.configure(borderwidth='2',compound='left',text='UP',width=4)
    upB.grid(row=0,column=0,sticky='ew')
    upB.bind('<Button-1>',lambda e:button_changed(e,upB,'plasmac.move-up'))
    upB.bind('<ButtonRelease-1>',lambda e:button_changed(e,upB,'plasmac.move-up'))
    downB = Button(moveF)
    downB.configure(borderwidth='2',compound='left',text='DOWN',width=4)
    downB.grid(row=0,column=1,sticky='ew')
    downB.bind('<Button-1>',lambda e:button_changed(e,downB,'plasmac.move-down'))
    downB.bind('<ButtonRelease-1>',lambda e:button_changed(e,downB,'plasmac.move-down'))
    moveF.columnconfigure(0,weight=1)
    moveF.columnconfigure(1,weight=1)
    # convoluted estop for qtplasmac
    if 'qtvcp' in I.find('DISPLAY','DISPLAY').lower():
        estopF = LabelFrame(window)
        estopF.configure(relief='groove',text='Estop')
        estopB = Button(estopF)
        estopB.configure(borderwidth='2',compound='left',text='ESTOP')
        estopB.pack()
        estopB.bind('<Button-1>',estop_pressed) #lambda e:button_changed(e,estopB,'db_arc-ok.in'))
#        estopB.bind('<ButtonRelease-1>',lambda e:button_changed(e,estopB,'db_arc-ok.in'))
        estopF.pack(padx=2,pady=2,fill='x')
        estopB.configure(background=buttonRd)
        estopB.configure(activebackground=buttonRd)
        hal.set_p('estop_or.in0', '1')
        # ---ESTOP HANDLING---
        # loadrt or2 names=estop_or
        # loadrt not names=estop_not,estop_not_1
        # addf estop_or    servo-thread
        # addf estop_not   servo-thread
        # addf estop_not_1 servo-thread
        # net sim:estop-raw estop_or.out  => estop_not.in
        # net sim:estop-out estop_not.out => iocontrol.0.emc-enable-in
    # def set_estop(self):
    #     if self.prefs.getpref('Estop type', 0, int, 'GUI_OPTIONS') == 2:
    #         RUN(['halcmd', 'net', 'sim:estop-1-raw', 'iocontrol.0.user-enable-out', 'estop_not_1.in'])
    #         RUN(['halcmd', 'net', 'sim:estop-1-in', 'estop_not_1.out', 'estop_or.in1'])

    # basic estop for axis
    else:
        CALL(['halcmd', 'net', 'sim:estop-loop', 'iocontrol.0.user-enable-out', 'iocontrol.0.emc-enable-in'])
    # set default button color
    buttonBg = ohmicB['activebackground']
    # default to ohmic if ohmic probe is enabled
    if hal.get_value('plasmac.ohmic-probe-enable'):
        ohmicC.select()
    else:
        floatC.select()
    # for termination if axis not found
    signal.signal(signal.SIGALRM, time_out)
    # start periodic running
    periodic()
    window.mainloop()
except KeyboardInterrupt:
    pass
except Exception as e:
    unload(e)
