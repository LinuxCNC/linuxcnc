'''
plasmac_axis.py
Copyright (C) 2018 2019  Phillip A Carter

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


# call to tk window
w = root_window.tk.call

################################################################################
# disable the 'do you want to close' dialog

w('wm','protocol','.','WM_DELETE_WINDOW','destroy .')



################################################################################
# set the default font, gcode font and help balloons

font = inifile.find('PLASMAC','FONT') or 'sans 10'
fname, fsize = font.split()
w('font','configure','TkDefaultFont','-family', fname, '-size', fsize)
w('.pane.bottom.t.text','configure','-height','10','-font', font, '-foreground','blue')
w('DynamicHelp::configure','-borderwidth','5','-topbackground','yellow','-bg','yellow')



################################################################################
# set the window size
wsize = inifile.find('PLASMAC','MAXIMISED') or '0'
if wsize == '0':
    fsizes = ['9','10','11','12','13','14','15','16']
    heights = ['688','708','736','748','816','858','900','950']
    if (inifile.find('DISPLAY','GLADEVCP') or '0') == '0':
        aspect = 1.5
    else:
        aspect = 1.7
    width = str(int(float(heights[fsizes.index(fsize)]) * aspect))
    height = str(int(float(heights[fsizes.index(fsize)])))
    wxpos = '20'
    wypos = '20'
else:
    # change pad_width and pad_height for smaller than fullscreen
    pad_width = 0
    pad_height = 0
    maxgeo = w('wm','maxsize','.')
    if type(maxgeo) == tuple:
        fullsize = str(maxgeo[0]),str(maxgeo[1])
    else:
        fullsize = maxgeo.split(' ')[0],maxgeo.split(' ')[1]
    width = str(int(fullsize[0])-pad_width)
    height = str(int(fullsize[1])-pad_height)
    wxpos = str(pad_width/2)
    wypos = str(pad_height/2)
w('wm','geometry','.','{0}x{1}-{2}-{3}'.format(width,height,wxpos,wypos))
print('\nAxis window is {0} x {1}\n'.format(width,height))


################################################################################
# change dro screen

w('.pane.top.right.fnumbers.text','configure','-foreground','green','-background','black')



################################################################################
# widget setup

# some names to save fingers
ftop = '.pane.top'
ftabs = ftop + '.tabs'
fright = ftop + '.right'
fmanual = ftabs + '.fmanual'
faxes = fmanual + '.axes'
fjoints = fmanual + '.joints'
fjogf = fmanual + '.jogf'
ftorch = fmanual + '.torch'
foverride = fmanual + '.override'
fpausedmotion = fmanual + '.pausedmotion'
fmdi = ftabs + '.fmdi'
ft = '.pane.bottom.t'
fcommon = '.pane.bottom.t.common'
fmonitor = '.pane.bottom.t.common.monitor'
fbuttons = '.pane.bottom.t.common.buttons'

# redo the text in tabs so they resize for the new default font
w(ftabs,'configure','-arcradius','2','-tabbevelsize','8')
w(ftabs,'itemconfigure','manual','-text',' Manual - F3 ')
w(ftabs,'itemconfigure','mdi','-text',' MDI - F5 ')
w(ftabs,'configure','-bd','1')
w(fright,'configure','-arcradius','2','-tabbevelsize','8')
w(fright,'itemconfigure','preview','-text',' Preview ')
w(fright,'itemconfigure','numbers','-text',' DRO ')
w(fright,'configure','-bd','1')

# hide some original widgets
w('pack','forget','.toolbar.rule0')
w('pack','forget','.toolbar.rule4')
w('pack','forget','.toolbar.rule8')
w('pack','forget','.toolbar.rule9')
w('grid','forget',fmanual + '.axis')
w('grid','forget',fmanual + '.jogf')
w('grid','forget',fmanual + '.space2')
w('grid','forget',fmanual + '.spindlel')
w('grid','forget',fmanual + '.spindlef')
w('grid','forget',fmanual + '.space2')
w('grid','forget',fmanual + '.coolant')
w('grid','forget',fmanual + '.mist')
w('grid','forget',fmanual + '.flood')
w('grid','forget',ftop + '.spinoverride')

# change layout for some scales
w('pack','forget',ftop + '.jogspeed.l0')
w('pack','forget',ftop + '.jogspeed.l')
w('pack','forget',ftop + '.jogspeed.l1')
w('pack','forget',ftop + '.jogspeed.s')
w('pack','forget',ftop + '.maxvel.l0')
w('pack','forget',ftop + '.maxvel.l')
w('pack','forget',ftop + '.maxvel.l1')
w('pack','forget',ftop + '.maxvel.s')
w('pack',ftop + '.jogspeed.s','-side','right')
w('pack',ftop + '.jogspeed.l1','-side','right')
w('pack',ftop + '.jogspeed.l','-side','right')
w('pack',ftop + '.jogspeed.l0','-side','left')
w('pack',ftop + '.maxvel.s','-side','right')
w('pack',ftop + '.maxvel.l1','-side','right')
w('pack',ftop + '.maxvel.l','-side','right')
w('pack',ftop + '.maxvel.l0','-side','left')

# modify the toolbar
w('label','.toolbar.space1','-width','5')
w('label','.toolbar.space2','-width','5')
w('label','.toolbar.space3','-width','5')
w('label','.toolbar.space4','-width','10')
w('pack','.toolbar.space1','-after','.toolbar.machine_power','-side','left')
w('pack','.toolbar.space2','-after','.toolbar.reload','-side','left')
w('pack','.toolbar.space3','-after','.toolbar.program_stop','-side','left')
w('pack','.toolbar.space4','-after','.toolbar.program_optpause','-side','left')

# set some sizes for widgets
swidth = 5  # spinboxes width
lwidth = 15 # labels width
bwidth = 12  # buttons width
cwidth = int(fsize) * 2 #canvas width
cheight = int(fsize) * 2 #canvas height
ledwidth = cwidth - 2 #led width
ledheight = cheight - 2 #led height
ledx = cwidth-ledwidth # led x start
ledy = cheight-ledheight # led y start

# rework the axis/joints frame
w('destroy',faxes)
w('labelframe',faxes,'-text','Axis:','-relief','flat')
w('destroy',fjoints)
w('labelframe',fjoints,'-text','Joint:','-relief','flat')
# make axis radiobuttons
for letter in 'xyzabcuvw':
    w('radiobutton',faxes + '.axis' + letter,\
                        '-anchor','w',\
                        '-padx','0',\
                        '-value',letter,\
                        '-variable','ja_rbutton',\
                        '-width','2',\
                        '-text',letter.upper(),\
                        '-command','ja_button_activated',\
                        )
# populate the axes frame
count = 0
letters = 'xyzabcuvw'
first_axis = ''
for row in range(0,2):
    for column in range(0,5):
        if letters[count] in trajcoordinates:
            if first_axis == '':
                first_axis = letters[count]
            w('grid',faxes + '.axis' + letters[count],'-row',row,'-column',column,'-padx','4')
        count += 1
        if count == 9: break
# make joints radiobuttons
for number in range(0,linuxcnc.MAX_JOINTS):
    w('radiobutton',fjoints + '.joint' + str(number),\
                        '-anchor','w',\
                        '-padx','0',\
                        '-value',number,\
                        '-variable','ja_rbutton',\
                        '-width','2',\
                        '-text',number,\
                        '-command','ja_button_activated',\
                        )
# populate joints frame
count = 0
for row in range(0,2):
    for column in range(0,5):
        if count == jointcount: break
        w('grid',fjoints + '.joint' + str(count),'-row',row,'-column',column,'-padx','4')
        count += 1

# rework the jogf frame
w('destroy',fjogf)
w('labelframe',fjogf,'-relief','flat','-bd','0')
w('labelframe',fjogf + '.jog','-text','Jog','-relief','flat')
w('button',fjogf + '.jog.jogminus','-command','if ![is_continuous] {jog_minus 1}','-height','1','-text','-')
w('bind',fjogf + '.jog.jogminus','<Button-1>','if [is_continuous] {jog_minus}')
w('bind',fjogf + '.jog.jogminus','<ButtonRelease-1>','if [is_continuous] {jog_stop}')
w('button',fjogf + '.jog.jogplus','-command','if ![is_continuous] {jog_plus 1}','-height','1','-text','+')
w('bind',fjogf + '.jog.jogplus','<Button-1>','if [is_continuous] {jog_plus}')
w('bind',fjogf + '.jog.jogplus','<ButtonRelease-1>','if [is_continuous] {jog_stop}')
w('combobox',fjogf + '.jog.jogincr','-editable','0','-textvariable','jogincrement','-value',_('Continuous'),'-width','10')
w(fjogf + '.jog.jogincr','list','insert','end',_('Continuous'))
if increments:
    w(fjogf + '.jog.jogincr','list','insert','end',*increments)
w('labelframe',fjogf + '.zerohome','-text','Zero','-relief','flat')
w('button',fjogf + '.zerohome.home','-command','home_joint','-height','1')
w('setup_widget_accel',fjogf + '.zerohome.home',_('Home Axis'))
w('button',fjogf + '.zerohome.zero','-command','touch_off_system','-height','1')
w('setup_widget_accel',fjogf + '.zerohome.zero',_('Touch Off'))
# unused, just for tcl hierarchy
w('button',fjogf + '.zerohome.tooltouch')
w('checkbutton',fjogf + '.override')
# populate the jog frame
w('grid',fjogf + '.jog.jogminus','-row','0','-column','0','-padx','0 3','-sticky','nsew')
w('grid',fjogf + '.jog.jogplus','-row','0','-column','1','-padx','3 3','-sticky','nsew')
w('grid',fjogf + '.jog.jogincr','-row','0','-column','2','-padx','3 0','-sticky','nsew')
w('grid',fjogf + '.jog','-row','0','-column','0','-sticky','ew')
w('grid',fjogf + '.zerohome.home','-row','0','-column','0','-padx','0 3','-sticky','ew')
w('grid',fjogf + '.zerohome.zero','-row','0','-column','1','-padx','3 0','-sticky','ew')
w('grid',fjogf + '.zerohome','-row','1','-column','0','-pady','4 0','-sticky','ew')
w('grid',fjogf,'-column','0','-row','1','-padx','4','-pady','2 0','-sticky','ew')
w('grid','columnconfigure',fmanual,'0','-weight','1')
w('grid','columnconfigure',fjogf,'0','-weight','1')
w('grid','columnconfigure',fjogf + '.jog','0 1 2','-weight','1')
w('grid','columnconfigure',fjogf + '.zerohome','0 1','-weight','1')
w('DynamicHelp::add',fjogf + '.jog.jogminus','-text','Jog selected axis\nin negative direction')
w('DynamicHelp::add',fjogf + '.jog.jogplus','-text','Jog selected axis\nin positive direction')
w('DynamicHelp::add',fjogf + '.jog.jogincr','-text','Select jog increment')
if homing_order_defined:
    if ja_name.startswith('A'):
        hbName = 'axes'
    else:
        hbName ='joints'
    widgets.homebutton.configure(text = _('Home All'), command = 'home_all_joints')
    w('DynamicHelp::add',fjogf + '.zerohome.home','-text','Home all %s [Ctrl-Home]' % hbName)
else:
    w('DynamicHelp::add',fjogf + '.zerohome.home','-text','Home selected %s [Home]' % ja_name.lower())
w('DynamicHelp::add',fjogf + '.zerohome.zero','-text','Touch off selected axis\nto workpiece [Home]')

# new torch frame
w('labelframe',ftorch,'-text','Torch:','-relief','flat')
w('Button',ftorch + '.torch-button','-text','PULSE','-takefocus','0','-width','3')
w('bind',ftorch + '.torch-button','<Button-1>','torch_pulse 1')
w('bind',ftorch + '.torch-button','<ButtonRelease-1>','torch_pulse 0')
w('append','manualgroup',' ' + ftorch + '.torch-button')
w('scale',ftorch + '.torch-pulse-time','-orient','horizontal','-variable','torchPulse','-showvalue','0')
w('label',ftorch + '.torch-time','-textvariable','torchPulse','-width','3','-anchor','e')
w('label',ftorch + '.torch-label','-text','Sec','-anchor','e')
# populate the torch frame
w('pack',ftorch + '.torch-button','-side','left','-pady','2')
w('pack',ftorch + '.torch-pulse-time','-side','left','-fill','x','-expand','1')
w('pack',ftorch + '.torch-label','-side','right')
w('pack',ftorch + '.torch-time','-side','right')
w('grid',ftorch,'-column','0','-row','2','-columnspan','1','-padx','4','-pady','2 0','-sticky','ew')
w('DynamicHelp::add',ftorch + '.torch-button','-text','Pulse torch on for\nselected time')
w('DynamicHelp::add',ftorch + '.torch-pulse-time','-text','Length of torch pulse (seconds)')

# new override frame
w('labelframe',foverride,'-text','Height Override:','-relief','flat')
w('Button',foverride + '.raise','-text','Raise','-takefocus','0','-width','3')
w('bind',foverride + '.raise','<ButtonPress-1>','height_raise')
w('Button',foverride + '.lower','-text','Lower','-takefocus','0','-width','3')
w('bind',foverride + '.lower','<ButtonPress-1>','height_lower')
w('label',foverride + '.height-override','-width','3','-justify','center')
w('Button',foverride + '.reset','-text','Reset','-takefocus','0','-width','3')
w('bind',foverride + '.reset','<ButtonPress-1>','height_reset')
# populate the override frame
w('pack',foverride + '.raise','-side','left')
w('pack',foverride + '.lower','-side','left')
w('pack',foverride + '.height-override','-side','left','-fill','x','-expand','1')
w('pack',foverride + '.reset','-side','right')
w('grid',foverride,'-column','0','-row','3','-columnspan','1','-padx','4','-pady','2 0','-sticky','ew')
w('DynamicHelp::add',foverride + '.raise','-text','Voltage value to raise height')
w('DynamicHelp::add',foverride + '.lower','-text','Voltage value to lower height')
w('DynamicHelp::add',foverride + '.reset','-text','Set height override to 0')
w('DynamicHelp::add',foverride + '.height-override','-text','Voltage value of height override')

# new paused motion frame
w('labelframe',fpausedmotion,'-text','Paused Motion Speed: %','-relief','flat')
w('Button',fpausedmotion + '.reverse','-text','Rev','-takefocus','0','-width','3')
w('bind',fpausedmotion + '.reverse','<Button-1>','paused_motion -1')
w('bind',fpausedmotion + '.reverse','<ButtonRelease-1>','paused_motion 0')
w('scale',fpausedmotion + '.paused-motion-speed','-orient','horizontal')
w('Button',fpausedmotion + '.forward','-text','Fwd','-takefocus','0','-width','3')
w('bind',fpausedmotion + '.forward','<Button-1>','paused_motion 1')
w('bind',fpausedmotion + '.forward','<ButtonRelease-1>','paused_motion 0')
# populate the paused motion frame
w('pack',fpausedmotion + '.reverse','-side','left','-fill','y')
w('pack',fpausedmotion + '.paused-motion-speed','-side','left','-fill','x','-expand','1')
w('pack',fpausedmotion + '.forward','-side','right','-fill','y')
w('DynamicHelp::add',fpausedmotion + '.reverse','-text','Move while paused\nin reverse direction')
w('DynamicHelp::add',fpausedmotion + '.forward','-text','Move while paused\nin foward direction')
w('DynamicHelp::add',fpausedmotion + '.paused-motion-speed','-text','Paused motion speed (% of feed rate)')
# only show paused motion if LinuxCNC version 2.9 or later
if int(linuxcnc.version.split('.')[0] + linuxcnc.version.split('.')[1]) >= 29:
    w('grid',fpausedmotion,'-column','0','-row','4','-columnspan','1','-padx','4','-pady','2 0','-sticky','ew')

# hide bottom pane until modified
w('pack','forget','.pane.bottom.t.text')
w('pack','forget','.pane.bottom.t.sb')

# new common frame
w('labelframe',fcommon,'-text','','-relief','flat','-bd','1')

# new monitor frame
w('labelframe',fmonitor,'-text','','-relief','flat')
arcfont = fname + ' ' + str(int(fsize) * 3) + ' bold'
w('label',fmonitor + '.arc-voltage','-anchor','e','-width',3,'-fg','blue','-font',arcfont)
w('label',fmonitor + '.aVlab','-text','Arc Voltage')
w('canvas',fmonitor + '.led-arc-ok','-width',cwidth,'-height',cheight)
w(fmonitor + '.led-arc-ok','create','oval',ledx,ledy,ledwidth,ledheight,'-fill','#37F608','-disabledfill','grey')
w('label',fmonitor + '.lAOlab','-text','Arc OK')
w('canvas',fmonitor + '.led-torch','-width',cwidth,'-height',cheight)
w(fmonitor + '.led-torch','create','oval',ledx,ledy,ledwidth,ledheight,'-fill','#F99B0B','-disabledfill','grey')
w('label',fmonitor + '.lTlab','-text','Torch On')
w('canvas',fmonitor + '.led-thc-enabled','-width',cwidth,'-height',cheight)
w(fmonitor + '.led-thc-enabled','create','oval',ledx,ledy,ledwidth,ledheight,'-fill','#37F608','-disabledfill','grey')
w('label',fmonitor + '.lTElab','-text','THC Enabled')
w('canvas',fmonitor + '.led-ohmic','-width',cwidth,'-height',cheight)
w(fmonitor + '.led-ohmic','create','oval',ledx,ledy,ledwidth,ledheight,'-fill','yellow','-disabledfill','grey')
w('label',fmonitor + '.lOlab','-text','Ohmic Probe')
w('canvas',fmonitor + '.led-float','-width',cwidth,'-height',cheight)
w(fmonitor + '.led-float','create','oval',ledx,ledy,ledwidth,ledheight,'-fill','yellow','-disabledfill','grey')
w('label',fmonitor + '.lFlab','-text','Float Switch')
w('canvas',fmonitor + '.led-breakaway','-width',cwidth,'-height',cheight)
w(fmonitor + '.led-breakaway','create','oval',ledx,ledy,ledwidth,ledheight,'-fill','red','-disabledfill','grey')
w('label',fmonitor + '.lBlab','-text','Breakaway')
w('canvas',fmonitor + '.led-thc-active','-width',cwidth,'-height',cheight)
w(fmonitor + '.led-thc-active','create','oval',ledx,ledy,ledwidth,ledheight,'-fill','#37F608','-disabledfill','grey')
w('label',fmonitor + '.lTAlab','-text','THC Active')
w('labelframe',fmonitor + '.updown','-text','','-relief','flat','-width','20')
w('canvas',fmonitor + '.led-up','-width',cwidth,'-height',cheight)
w(fmonitor + '.led-up','create','oval',ledx,ledy,ledwidth,ledheight,'-fill','yellow','-disabledfill','grey')
w('canvas',fmonitor + '.updown.led-down','-width',cwidth,'-height',cheight)
w(fmonitor + '.updown.led-down','create','oval',ledx,ledy,ledwidth,ledheight,'-fill','yellow','-disabledfill','grey')
w('label',fmonitor + '.updown.lab','-text','Up> THC <Down')
w('pack',fmonitor + '.updown.led-down','-side','right','-fill','none','-expand','0')
w('pack',fmonitor + '.updown.lab','-side','right','-fill','x','-expand','1')
w('canvas',fmonitor + '.led-corner-locked','-width',cwidth,'-height',cheight)
w(fmonitor + '.led-corner-locked','create','oval',ledx,ledy,ledwidth,ledheight,'-fill','red','-disabledfill','grey')
w('label',fmonitor + '.lCLlab','-text','THC Velocity Lock')
w('canvas',fmonitor + '.led-kerf-locked','-width',cwidth,'-height',cheight)
w(fmonitor + '.led-kerf-locked','create','oval',ledx,ledy,ledwidth,ledheight,'-fill','red','-disabledfill','grey')
w('label',fmonitor + '.lKLlab','-text','THC Void Lock')
# populate the monitor frame
w('grid',fmonitor + '.arc-voltage','-row','0','-column','0','-rowspan','2','-sticky','e')
w('grid',fmonitor + '.aVlab','-row','0','-column','1','-rowspan','2','-sticky','w')
w('grid',fmonitor + '.led-arc-ok','-row','2','-column','0','-sticky','e')
w('grid',fmonitor + '.lAOlab','-row','2','-column','1','-sticky','w')
w('grid',fmonitor + '.led-torch','-row','3','-column','0','-sticky','e')
w('grid',fmonitor + '.lTlab','-row','3','-column','1','-sticky','w')
w('grid',fmonitor + '.led-thc-enabled','-row','0','-column','2','-sticky','e')
w('grid',fmonitor + '.lTElab','-row','0','-column','3','-sticky','w')
w('grid',fmonitor + '.led-ohmic','-row','1','-column','2','-sticky','e')
w('grid',fmonitor + '.lOlab','-row','1','-column','3','-sticky','w')
w('grid',fmonitor + '.led-float','-row','2','-column','2','-sticky','e')
w('grid',fmonitor + '.lFlab','-row','2','-column','3','-sticky','w')
w('grid',fmonitor + '.led-breakaway','-row','3','-column','2','-sticky','e')
w('grid',fmonitor + '.lBlab','-row','3','-column','3','-sticky','w')
w('grid',fmonitor + '.led-thc-active','-row','0','-column','4','-sticky','e')
w('grid',fmonitor + '.lTAlab','-row','0','-column','5','-sticky','w')
w('grid',fmonitor + '.led-up','-row','1','-column','4','-sticky','e')
w('grid',fmonitor + '.updown','-row','1','-column','5','-sticky','e')
w('grid',fmonitor + '.led-corner-locked','-row','2','-column','4','-sticky','e')
w('grid',fmonitor + '.lCLlab','-row','2','-column','5','-sticky','w')
w('grid',fmonitor + '.led-kerf-locked','-row','3','-column','4','-sticky','e')
w('grid',fmonitor + '.lKLlab','-row','3','-column','5','-sticky','w')
w('grid','rowconfigure',fmonitor,'0 1 2 3','-pad','4')
w('DynamicHelp::add',fmonitor + '.arc-voltage','-text','current arc voltage')
w('DynamicHelp::add',fmonitor + '.led-arc-ok','-text','a valid arc is established')
w('DynamicHelp::add',fmonitor + '.led-torch','-text','torch on signal is being sent to plasma supply')
w('DynamicHelp::add',fmonitor + '.led-thc-enabled','-text','THC is enabled')
w('DynamicHelp::add',fmonitor + '.led-ohmic','-text','the ohmic probe is sensed')
w('DynamicHelp::add',fmonitor + '.led-float','-text','the float switch is activated')
w('DynamicHelp::add',fmonitor + '.led-breakaway','-text','the breakaway switch is activated')
w('DynamicHelp::add',fmonitor + '.led-thc-active','-text','THC is active')
w('DynamicHelp::add',fmonitor + '.led-up','-text','THC is moving the Z axis up')
w('DynamicHelp::add',fmonitor + '.updown.led-down','-text','THC is moving the Z axis down')
w('DynamicHelp::add',fmonitor + '.led-corner-locked','-text','THC is locked due to velocity constraints')
w('DynamicHelp::add',fmonitor + '.led-kerf-locked','-text','THC is locked due to void sensing constraints')

# new buttons frame
w('labelframe',fbuttons,'-relief','groove')
w('button',fbuttons + '.torch-enable','-width',bwidth/2,'-height','2')
w('button',fbuttons + '.button1','-width',bwidth/2,'-height','2')
w('button',fbuttons + '.button2','-width',bwidth/2,'-height','2')
w('button',fbuttons + '.button3','-width',bwidth/2,'-height','2')
w('button',fbuttons + '.button4','-width',bwidth/2,'-height','2')
w('button',fbuttons + '.button5','-width',bwidth/2,'-height','2')
iniButtonName = ['Names']
iniButtonCode = ['Codes']
for button in range(1,6):
    bname = inifile.find('PLASMAC', 'BUTTON_' + str(button) + '_NAME') or '0'
    iniButtonName.append(bname)
    iniButtonCode.append(inifile.find('PLASMAC', 'BUTTON_' + str(button) + '_CODE'))
    if bname != '0':
        bname = bname.split('\\')
        if len(bname) > 1:
            blabel = bname[0] + '\n' + bname[1]
        else:
            blabel = bname[0]
        w(fbuttons + '.button' + str(button),'configure','-text',blabel)
w('bind',fbuttons + '.torch-enable','<ButtonPress-1>','torch_enable')
w('bind',fbuttons + '.button1','<ButtonPress-1>','button_action 1 1')
w('bind',fbuttons + '.button1','<ButtonRelease-1>','button_action 1 0')
w('bind',fbuttons + '.button2','<ButtonPress-1>','button_action 2 1')
w('bind',fbuttons + '.button2','<ButtonRelease-1>','button_action 2 0')
w('bind',fbuttons + '.button3','<ButtonPress-1>','button_action 3 1')
w('bind',fbuttons + '.button3','<ButtonRelease-1>','button_action 3 0')
w('bind',fbuttons + '.button4','<ButtonPress-1>','button_action 4 1')
w('bind',fbuttons + '.button4','<ButtonRelease-1>','button_action 4 0')
w('bind',fbuttons + '.button5','<ButtonPress-1>','button_action 5 1')
w('bind',fbuttons + '.button5','<ButtonRelease-1>','button_action 5 0')
# populate the buttons frame
w('grid',fbuttons + '.torch-enable','-row','0','-column','0')
w('grid',fbuttons + '.button1','-row','0','-column','1')
w('grid',fbuttons + '.button2','-row','0','-column','2')
w('grid',fbuttons + '.button3','-row','0','-column','3')
w('grid',fbuttons + '.button4','-row','0','-column','4')
w('grid',fbuttons + '.button5','-row','0','-column','5')
w('grid','rowconfigure',fbuttons,0,'-weight','1')

w('DynamicHelp::add',fbuttons + '.torch-enable','-text','enable/disable torch\nif disabled when run pressed then dry run will commence')
w('DynamicHelp::add',fbuttons + '.button1','-text','User button 1\nconfigured in ini file')
w('DynamicHelp::add',fbuttons + '.button2','-text','User button 2\nconfigured in ini file')
w('DynamicHelp::add',fbuttons + '.button3','-text','User button 3\nconfigured in ini file')
w('DynamicHelp::add',fbuttons + '.button4','-text','User button 4\nconfigured in ini file')
w('DynamicHelp::add',fbuttons + '.button5','-text','User button 5\nconfigured in ini file')

w('pack',fmonitor,'-fill','y','-side','top')
w('pack',fbuttons,'-fill','y','-side','top')
w('pack',fcommon,'-fill','both','-side','left')
w('pack',ft + '.sb','-fill','y','-side','left','-padx','1')
w('pack',ft + '.text','-fill','both','-expand','1','-side','left','-padx','0','-pady','0')
w(ft,'configure','-relief','flat')
w(ft + '.sb','configure','-width', '16')
w(ft + '.text','configure','-width', '42', '-borderwidth','1','-relief','sunken')



################################################################################
# some new commands for TCL

def button_action(button,pressed):
    if int(pressed):
        user_button_pressed(button,iniButtonCode[int(button)])
    else:
        user_button_released(button,iniButtonCode[int(button)])

def torch_pulse(value):
    hal.set_p('plasmac.torch-pulse-start',value)

def paused_motion(direction):
    if w(fpausedmotion + '.forward','cget','-state') == 'normal' or\
       w(fpausedmotion + '.reverse','cget','-state') == 'normal':
        speed = float(w(fpausedmotion + '.paused-motion-speed','get')) * 0.01
        hal.set_p('plasmac.paused-motion-speed','%f' % (speed * int(direction)))

def height_lower():
        global torch_height 
        torch_height -= 0.1
        w(foverride + '.height-override','configure','-text','%0.1f V' % (torch_height))
        hal.set_p('plasmac.height-override','%f' %(torch_height))

def height_raise():
        global torch_height 
        torch_height += 0.1
        w(foverride + '.height-override','configure','-text','%0.1f V' % (torch_height))
        hal.set_p('plasmac.height-override','%f' %(torch_height))

def height_reset():
        global torch_height 
        torch_height = 0
        w(foverride + '.height-override','configure','-text','%0.1f V' % (torch_height))
        hal.set_p('plasmac.height-override','%f' %(torch_height))

def torch_enable():
    if hal.get_value('plasmac.torch-enable'):
        hal.set_p('plasmac.torch-enable','0')
        w(fbuttons + '.torch-enable','configure','-bg','red','-activebackground','#AA0000','-text','Torch\nDisabled')
    else:
        hal.set_p('plasmac.torch-enable','1')
        w(fbuttons + '.torch-enable','configure','-bg','green','-activebackground','#00AA00','-text','Torch\nEnabled')

def joint_mode_switch(a,b,c):
    if vars.motion_mode.get() == linuxcnc.TRAJ_MODE_FREE and s.kinematics_type != linuxcnc.KINEMATICS_IDENTITY:
        w('grid','forget',fmanual + '.axes')
        w('grid',fmanual + '.joints','-column','0','-row','0','-padx','4','-pady','0','-sticky','ew')
        widget = getattr(widgets, "joint_%d" % 0)
        widget.focus()
        vars.ja_rbutton.set(0)
    else:
        w('grid','forget',fmanual + '.joints')
        w('grid',fmanual + '.axes','-column','0','-row','0','-padx','4','-pady','0','-sticky','ew')
        widget = getattr(widgets, "axis_%s" % first_axis)
        widget.focus()
        vars.ja_rbutton.set(first_axis)

def ja_button_activated():
    if vars.ja_rbutton.get() in 'xyzabcuvw':
        widget = getattr(widgets, "axis_%s" % vars.ja_rbutton.get())
        widget.focus()
    else:
        widget = getattr(widgets, "joint_%s" % vars.ja_rbutton.get())
        widget.focus()
    commands.axis_activated

# add the commands to TclCommands
TclCommands.button_action = button_action
TclCommands.torch_pulse = torch_pulse
TclCommands.paused_motion = paused_motion
TclCommands.height_raise = height_raise
TclCommands.height_lower = height_lower
TclCommands.height_reset = height_reset
TclCommands.torch_enable = torch_enable
TclCommands.joint_mode_switch = joint_mode_switch
TclCommands.ja_button_activated = ja_button_activated
commands = TclCommands(root_window)



################################################################################
# some python functions

def user_button_pressed(button,commands):
    if w(fbuttons + '.button' + button,'cget','-state') == 'disabled' or \
       not commands: return
    from subprocess import Popen,PIPE
    if 'change-consumables' in commands.lower():
        if hal.get_value('axis.x.eoffset-counts') or hal.get_value('axis.y.eoffset-counts'):
            hal.set_p('plasmac.consumable-change', '0')
        else:
            global ccX
            global ccY
            global ccScale
            if ccX or ccX == 0:
                hal.set_p('plasmac.x-offset', '{:.0f}'.format((ccX - s.position[0]) / ccScale, 0))
            else:
                hal.set_p('plasmac.x-offset', '0')
            if ccY or ccY == 0:
                hal.set_p('plasmac.y-offset', '{:.0f}'.format((ccY - s.position[1]) / ccScale, 0))
            else:
                hal.set_p('plasmac.y-offset', '0')
            hal.set_p('plasmac.consumable-change', '1')
    elif 'ohmic-test' in commands.lower():
        hal.set_p('plasmac.ohmic-test','1')
    elif 'probe-test' in commands.lower():
        global probePressed
        global probeTimer
        global probeButton
        probePressed = True
        probeButton = button
        if commands.lower().replace('probe-test','').strip():
            probeTimer = float(commands.lower().replace('probe-test','').strip()) + time.time()
        hal.set_p('plasmac.probe-test','1')
    elif 'cut-type' in commands.lower() and not hal.get_value('halui.program.is-running'):
        global cutType
        cutType ^= 1
        bgc = w('ttk::style', 'lookup', 'TButton', '-background')
        abgc = w('ttk::style', 'lookup', 'TButton', '-background', 'active')
        if cutType:
            hal.set_p('plasmac_run.cut-type','1')
            w(fbuttons + '.button' + button,'configure','-bg','orange','-activebackground','darkorange1','-text','Pierce\nOnly')
        else:
            hal.set_p('plasmac_run.cut-type','0')
            w(fbuttons + '.button' + button,'configure','-bg',bgc,'-activebackground',abgc,'-text','Pierce\n & Cut')
        Popen('axis-remote -r', stdout = PIPE, shell = True)
    else:
        for command in commands.split('\\'):
            if command.strip()[0] == '%':
                command = command.strip().strip('%') + '&'
                Popen(command,stdout=PIPE,stderr=PIPE, shell=True)
            else:
                if '{' in command:
                    newCommand = subCommand = ''
                    for char in command:
                        if char == '{':
                            subCommand = ':'
                        elif char == '}':
                            f1, f2 = subCommand.replace(':',"").split()
                            newCommand += inifile.find(f1,f2)
                            subCommand = ''
                        elif subCommand.startswith(':'):
                            subCommand += char
                        else:
                            newCommand += char
                    command = newCommand
                s.poll()
                if not s.estop and s.enabled and s.homed and (s.interp_state == linuxcnc.INTERP_IDLE):
                    mode = s.task_mode
                    if mode != linuxcnc.MODE_MDI:
                        mode = s.task_mode
                        c.mode(linuxcnc.MODE_MDI)
                        c.wait_complete()
                    c.mdi(command)
                    s.poll()
                    while s.interp_state != linuxcnc.INTERP_IDLE:
                        s.poll()
                    c.mode(mode)
                    c.wait_complete()

def user_button_released(button,commands):
    if w(fbuttons + '.button' + button,'cget','-state') == 'disabled' or \
       not commands: return
    global probeButton
    global probePressed
    probePressed = False
    if commands.lower() == 'ohmic-test':
        hal.set_p('plasmac.ohmic-test','0')
    elif commands.lower() == 'probe-test':
        if not probeTimer and button == probeButton:
            hal.set_p('plasmac.probe-test','0')
            probeButton = ''

# this is run from axis every cycle
# original in axis.py line 3000
def user_live_update():
    stat = linuxcnc.stat()
    stat.poll()
    global firstrundone
    global probeTimer
    global probePressed
    if not firstrundone:
        spaceWidth = w('winfo','width',fmanual)
        spaceWidth -= w('winfo','width',fmonitor)
        spaceWidth -= w('winfo','width',fbuttons)
        firstrundone = True
    # send scales value to plasmac component
    for widget in wScalesHal:
        tmp, item = widget.rsplit('.',1)
        value = float(w(widget,'get'))
        if value != widgetValues[widget]:
            widgetValues[widget] = value
            hal.set_p('plasmac.%s' % (item),'%f' % (value))
    # update status leds
    for widget in wLeds:
        tmp, item = widget.rsplit('.',1)
        if comp[item] != widgetValues[widget]:
            widgetValues[widget] = comp[item]
            if comp[item] == 1:
                w(widget,'configure','-state','normal')
            else:
                w(widget,'configure','-state','disabled')
    # update arc voltage
    w(fmonitor + '.arc-voltage','configure','-text','%3.0f' % (comp['arc-voltage']))
    # set machine state
    isIdleHomed = True
    isIdleOn = True
    if hal.get_value('halui.program.is-idle') and hal.get_value('halui.machine.is-on'):
        if hal.get_value('plasmac.arc-ok-out'):
            isIdleOn = False
        for joint in range(0,int(inifile.find('KINS','JOINTS'))):
                if not stat.homed[joint]:
                    isIdleHomed = False
                    break
    else:
        isIdleHomed = False
        isIdleOn = False 
    # set buttons state
    for n in range(1,6):
        if 'change-consumables' in iniButtonCode[n]:
            if hal.get_value('halui.program.is-paused'):
                w(fbuttons + '.button' + str(n),'configure','-state','normal')
            else:
                w(fbuttons + '.button' + str(n),'configure','-state','disabled')
        elif iniButtonCode[n] in ['ohmic-test']:
            if isIdleOn or hal.get_value('halui.program.is-paused'):
                w(fbuttons + '.button' + str(n),'configure','-state','normal')
            else:
                w(fbuttons + '.button' + str(n),'configure','-state','disabled')
        elif not iniButtonCode[n] in ['ohmic-test'] and not iniButtonCode[n] in ['cut-type'] and not iniButtonCode[n].startswith('%'):
            if isIdleHomed:
                w(fbuttons + '.button' + str(n),'configure','-state','normal')
            else:
                w(fbuttons + '.button' + str(n),'configure','-state','disabled')
    if hal.get_value('halui.machine.is-on') and (hal.get_value('halui.program.is-idle') or hal.get_value('halui.program.is-paused')):
        w(ftorch + '.torch-button','configure','-state','normal')
    else:
        w(ftorch + '.torch-button','configure','-state','disabled')
    if hal.get_value('halui.program.is-paused') or hal.get_value('plasmac.paused-motion-speed'):
        w(fpausedmotion + '.reverse','configure','-state','normal')
        w(fpausedmotion + '.forward','configure','-state','normal')
    else:
        w(fpausedmotion + '.reverse','configure','-state','disabled')
        w(fpausedmotion + '.forward','configure','-state','disabled')
    if hal.get_value('halui.machine.is-on'):
        w(foverride + '.raise','configure','-state','normal')
        w(foverride + '.lower','configure','-state','normal')
        w(foverride + '.reset','configure','-state','normal')
    else:
        w(foverride + '.raise','configure','-state','disabled')
        w(foverride + '.lower','configure','-state','disabled')
        w(foverride + '.reset','configure','-state','disabled')
    # decrement probe timer if active
    if probeTimer:
        if time.time() >= probeTimer:
            probeTimer = 0
            if not probePressed:
                hal.set_p('plasmac.probe-test','0')
    if (hal.get_value('axis.x.eoffset') or hal.get_value('axis.y.eoffset')) and not hal.get_value('halui.program.is-paused'):
        hal.set_p('plasmac.consumable-change', '0')
    try:
        if hal.get_value('plasmac_run.preview-tab'):
            root_window.tk.call('.pane.top.right','raise','preview')
            hal.set_p('plasmac_run.preview-tab', '0')
    except:
        pass
def user_hal_pins():
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
    comp.newpin('led-kerf-locked', hal.HAL_BIT, hal.HAL_IN)
    comp.ready()
    # create new signals and connect pins
    hal_data = [[0,'plasmac:arc-voltage-out','plasmac.arc-voltage-out','axisui.arc-voltage'],\
                [1,'plasmac:axis-min-limit','ini.z.min_limit','plasmac.axis-z-min-limit'],\
                [2,'plasmac:axis-max-limit','ini.z.max_limit','plasmac.axis-z-max-limit'],\
                [3,'plasmac:arc-ok-out','plasmac.arc-ok-out','axisui.led-arc-ok'],\
                [4,'plasmac:thc-enabled','plasmac.thc-enabled','axisui.led-thc-enabled'],\
                [5,'plasmac:thc-active','plasmac.thc-active','axisui.led-thc-active'],\
                [6,'plasmac:led-up','plasmac.led-up','axisui.led-up'],\
                [7,'plasmac:led-down','plasmac.led-down','axisui.led-down'],\
                [8,'plasmac:cornerlock-is-locked','plasmac.cornerlock-is-locked','axisui.led-corner-locked'],\
                [9,'plasmac:kerfcross-is-locked','plasmac.kerfcross-is-locked','axisui.led-kerf-locked'],\
                ]
    for line in hal_data:
        if line[0] < 3:
            hal.new_sig(line[1],hal.HAL_FLOAT)
        else:
            hal.new_sig(line[1],hal.HAL_BIT)
        hal.connect(line[2],line[1])
        hal.connect(line[3],line[1])
    #connect pins to existing signals
    hal.connect('axisui.led-ohmic','plasmac:ohmic-probe-out')
    hal.connect('axisui.led-float','plasmac:float-switch-out')
    hal.connect('axisui.led-breakaway','plasmac:breakaway-switch-out')
    hal.connect('axisui.led-torch','plasmac:torch-on')

def configure_widgets():
    w(ftorch + '.torch-pulse-time','configure','-from','0','-to','3','-resolution','0.1')
    w(fpausedmotion + '.paused-motion-speed','configure','-from','0','-to','100','-resolution','1')

def consumable_change_setup(ccParm):
    global ccX
    global ccY
    global ccScale
    ccX = ccY = ccF = ''
    X = Y = F = ''
    ccAxis = [X, Y, F]
    ccName = ['x', 'y', 'f']
    for loop in range(3):
        count = 0
        if ccName[loop] in ccParm:
            while 1:
                if not ccParm[count]: break
                if ccParm[count] == ccName[loop]:
                    count += 1
                    break
                count += 1
            while 1:
                if count == len(ccParm): break
                if ccParm[count].isdigit() or ccParm[count] in '.-':
                    ccAxis[loop] += ccParm[count]
                else:
                    break
                count += 1
            if ccName[loop] == 'x' and ccAxis[loop]:
                ccX = float(ccAxis[loop])
            elif ccName[loop] == 'y' and ccAxis[loop]:
                ccY = float(ccAxis[loop])
            elif ccName[loop] == 'f' and ccAxis[loop]:
                ccF = float(ccAxis[loop])
    if ccX and \
       (ccX < round(float(inifile.find('AXIS_X', 'MIN_LIMIT')), 6) or \
       ccX > round(float(inifile.find('AXIS_X', 'MAX_LIMIT')), 6)):
        print('x out of bounds for consumable change\n')
        raise SystemExit()
    if ccY and \
       (ccY < round(float(inifile.find('AXIS_Y', 'MIN_LIMIT')), 6) or \
       ccY > round(float(inifile.find('AXIS_Y', 'MAX_LIMIT')), 6)):
        print('y out of bounds for consumable change\n')
        raise SystemExit()
    if not ccF:
        print('invalid consumable change feed rate\n')
        raise SystemExit()
    ccScale = round(hal.get_value('plasmac.offset-scale'), 3) / 100
    ccVel = int(1 / hal.get_value('halui.machine.units-per-mm') / 60 * ccF * 100)
    hal.set_p('axis.x.eoffset-scale', str(ccScale))
    hal.set_p('axis.y.eoffset-scale', str(ccScale))   
    hal.set_p('plasmac.x-y-velocity', str(ccVel))
    hal.set_p('axis.x.eoffset-enable', '1')
    hal.set_p('axis.y.eoffset-enable', '1')



################################################################################
# setup
firstrundone = False
probePressed = False
probeTimer = 0
probeButton = ''
torchPulse = 0
torch_height = 0
cutType = 0
hal.set_p('plasmac.torch-enable','0')
hal.set_p('plasmac.height-override','%f' % (torch_height))
w(fbuttons + '.torch-enable','configure','-bg','red','-activebackground','#AA0000','-text','Torch\nDisabled')
w(foverride + '.height-override','configure','-text','%0.1f V' % (torch_height))
for button in range(1,6):
    if 'change-consumables' in inifile.find('PLASMAC', 'BUTTON_' + str(button) + '_CODE'):
        ccParm = inifile.find('PLASMAC','BUTTON_' + str(button) + '_CODE').replace('change-consumables','').replace(' ','').lower() or None
        if ccParm:
            consumable_change_setup(ccParm)
        else:
            print('consumable change parameters required\n')
        break
wLabels = [\
    fmonitor + '.aVlab',\
    fmonitor + '.lTlab',\
    fmonitor + '.lAOlab',\
    fmonitor + '.lTElab',\
    fmonitor + '.lFlab',\
    fmonitor + '.lBlab',\
    fmonitor + '.lOlab',\
    fmonitor + '.updown.lab',\
    fmonitor + '.lCLlab',\
    fmonitor + '.lKLlab',\
    ]
wScales = [\
    ftorch + '.torch-pulse-time',\
    fpausedmotion + '.paused-motion-speed',\
    ]
wScalesHal = [\
    ftorch + '.torch-pulse-time',\
    ]
wLeds = [\
    fmonitor + '.led-arc-ok',\
    fmonitor + '.led-torch',\
    fmonitor + '.led-thc-enabled',\
    fmonitor + '.led-ohmic',\
    fmonitor + '.led-float',\
    fmonitor + '.led-breakaway',\
    fmonitor + '.led-thc-active',\
    fmonitor + '.led-up',\
    fmonitor + '.updown.led-down',\
    fmonitor + '.led-corner-locked',\
    fmonitor + '.led-kerf-locked',\
    ]
configure_widgets()
w(fpausedmotion + '.paused-motion-speed','set',inifile.find('PLASMAC','PAUSED_MOTION_SPEED') or '50')
w(ftorch + '.torch-pulse-time','set',inifile.find('PLASMAC','TORCH_PULSE_TIME') or '1')
hal.set_p('plasmac.torch-pulse-time',inifile.find('PLASMAC','TORCH_PULSE_TIME') or '1')

for widget in wLabels:
    w(widget,'configure','-anchor','w','-width',lwidth)
widgetValues = {}
for widget in wScales:
    widgetValues[widget] = float(w(widget,'get'))
for widget in wLeds:
    w(widget,'configure','-state','disabled')
    widgetValues[widget] = 0
commands.set_view_z()
