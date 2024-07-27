#!/usr/bin/python3
"""
    g-code_ripper G-Code-Ripper
    
    Copyright (C) <2013-2021>  <Scorch>
    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.

    Version 0.01 Initial code

    Version 0.02 - Added feed rate scaling
                 - Eliminated read abort on N codes (the N code line numbers are simply ignored now)

    Version 0.03 - Added ability to map X or Y axis moves to A or B rotary axis for cutting on a cylindrical surface.
                 - Added more plot view orientation options

    Version 0.04 - Fixed bug relating to arcs without explicit g-codes (G2,G3) on each line
                 - Added ability to have zero decimal places on feed rates
                 
    Version 0.05 - Added "Export" to the g-code operations. Now g-code tool path data can be exported as DXF
                   or CSV (Comma Separated Values) formatted files. With or without rapid motions.

    Version 0.06 - Fixed code to again automatically convert arcs to lines when raping code (auto conversion was broken in V0.5)
                 - Fixed evaluation of equations using exponents "**"
                 - Added Auto Probe to g-code operations
                 
    Version 0.07 - Fixed bug in "Auto Probe" that caused an error when the Z offset is set to zero.
                 - Modified code to make it compatible with Python 2.5

    Version 0.08 - Fixed bug in g-code wrapping resulting in mapping failure resulting from to zero length tool path.
                 - Added logic for dealing with ambiguous start positions (when the tool position is not set by G0 commands before a G1, G2 or G3 command)
                 - Added a warning pop-up message when assumptions are made about the tool starting position.
                 - Fixed automatic conversion from arcs to lines when required by conversion type selected.

    Version 0.09 - Increased default decimal places to 1 for the feed rate.  The adjusted feed rates for
                   g-code mapped to a cylinder were being rounded to the nearest integer resulting in
                   unpredictable cutting speeds.
                 - Added the ability to save and read probe data files for auto-probing.

    Version 0.10 - Updated to be compatible with Python 3.x

    Version 0.11 - Fixed a minor bug that cause a failed file read in rare cases (tries to calculate square root of negative number)

    Version 0.12 - Updated the probe data file reading routine to be less sensitive to file formatting
                 - Fixed error in probe Z offsets when using an external probe data file

    Version 0.13 - Changed "Probe & Cut" to add the "Probe Z Safe" value to the height of the rapid moves ensuring the tool does not crash
                 - Fixed a bug in the conversion of arcs to lines when the arc spanned more than one Z value.
    
    Version 0.14 - Added dialog that allows users to skip over errors when reading g-code files
                 - Fixed bug that prevented the feed rate to return to the input value after a rapid move in a split file until a G1 command was issued
                 - Fixed bug that resulted in the feed rate being scaled at the z axis scale value
                 - Fixed variable handling for some cases
    
    Version 0.16 - Added support for MACH4 auto-probing
                 - Added pass through for G43 values
                 - Fixed bug in splitting code
                 
    Version 0.17 - Fixed low travel speed problem with rotary conversion
                 - Updated icon
                 
    Version 0.18 - Changed how height of rapid moves is calculated in adjusted g-code.  No longer adds Z offset value to the rapid height.
                 - Fixed problem that made the rapid height to high in adjusted g-code because the calculated maximum probe value would never go below zero.
                 - Added option for selecting if all points are probed or just required points
                 - Added option for selecting if probe data is saved when running probe and cut operation
                 - G-code ripper no longer takes into account the rapid moved when determining the design size
                 - Changed so G-Code Ripper does not try to load a file on startup.

    Version 0.19 - Changed autoprobe safe height calculation with MACH3/4 for "Probe and Cut" Mach was unable to perform the calculated height like LinuxCNC.

    Version 0.20 - Fixed problem that occurred when the Z position was not set by a rapid move before and X,Y move when probing.
                   (It now uses the first Z position in the g-code for the z-safe for probe moves before a z position is specified.)

    Version 0.21 - Added option to disable the g-code path display (might speed things up for large g-code files
                 - Fixed handling of G43 commands with G0 moves mixed in on the same line (who writes g-code like that anyway...)
                 - Bounding box now displays when in auto-probe mode 

    Version 0.22 - Added support for DDCS probing to file
                 - Added pass through for G53 commands
                 - Changed icon behavior
                 - fixed problem with comments with parentheses inside parentheses
                 - Fixed a small problem with Python 3.x

"""
version = '0.22'

import sys

from tkinter import *
from tkinter.filedialog import *
import tkinter.messagebox

try:
    import psyco
    psyco.full()
    sys.stdout.write("(Psyco loaded)\n")
except:
    pass
        
from math import *
import os
import re
import binascii
import getopt
import webbrowser
import struct

#import gcode_ripper_lib

Zero      = 0.0000001
STOP_CALC = 0

#Setting QUIET to True will stop almost all console messages
QUIET = False
DEBUG = False

################################################################################
#             Function for outputting messages to different locations          #
#            depending on what options are enabled                             #
################################################################################
def fmessage(text,newline=True):
    global QUIET
    if (not QUIET):
        if newline==True:
            try:
                sys.stdout.write(text)
                sys.stdout.write("\n")
            except:
                pass
        else:
            try:
                sys.stdout.write(text)
            except:
                pass

################################################################################
#                         Debug Message Box                                    #
################################################################################
def debug_message(message):
    global DEBUG
    title = "Debug Message"
    if DEBUG:
        tkinter.messagebox.showinfo(title,message)
        
def message_box(title,message):
    tkinter.messagebox.showinfo(title,message)
    
def message_ask_ok_cancel(title, mess):
    result=tkinter.messagebox.askokcancel(title, mess)
    return result

def error_message(message):
    error_report = Toplevel(width=525,height=60)
    error_report.title("G-Code Ripper: G-Code Reading Errors")
    error_report.iconname("G-Code Errors")
    error_report.grab_set()
    return_value =  StringVar()
    return_value.set("abort")

    def Stop_Click(event):
        return_value.set("abort")
        error_report.destroy()
        
    def Ignore_Click(event):
        return_value.set("ignore")
        error_report.destroy()

    #Text Box
    Error_Frame = Frame(error_report)
    scrollbar = Scrollbar(Error_Frame, orient=VERTICAL)
    Error_Text = Text(Error_Frame, width="80", height="20",yscrollcommand=scrollbar.set,bg='white')
    for line in message:
        Error_Text.insert(END,line+"\n")
    scrollbar.config(command=Error_Text.yview)
    scrollbar.pack(side=RIGHT,fill=Y)
    #End Text Box

    Button_Frame = Frame(error_report)
    stop_button = Button(Button_Frame,text="Abort G-Code Reading")
    stop_button.bind("<ButtonRelease-1>", Stop_Click)
    ignore_button = Button(Button_Frame,text="Ignore Errors")
    ignore_button.bind("<ButtonRelease-1>", Ignore_Click)
    stop_button.pack(side=RIGHT,fill=X)
    ignore_button.pack(side=LEFT,fill=X)

    Error_Text.pack(side=LEFT,fill=BOTH,expand=1)
    Button_Frame.pack(side=BOTTOM)
    Error_Frame.pack(side=LEFT,fill=BOTH,expand=1)
    
    root.wait_window(error_report)
    return return_value.get()

#Define cmp_new, cmp replacement for Python 3 compatibility
def cmp_new(A,B):
    if A==B:
        return False
    else:
        return True
    
############################################################################
# routine takes an x and a y coords and does a coordinate transformation    #
# to a new coordinate system at angle from the initial coordinate system   #
# Returns new x,y tuple                                                    #
############################################################################
def Transform(x,y,angle):
    newx = x * cos(angle) - y * sin(angle)
    newy = x * sin(angle) + y * cos(angle)
    return newx,newy

############################################################################
# routine takes an sin and cos and returns the angle (between 0 and 360)  #
############################################################################
def Get_Angle(y,x):
    angle = 90.0-degrees(atan2(x,y))
    if angle < 0:
        angle = 360 + angle
    return angle
    
################################################################################
class Line:
    def __init__(self, coords):
        self.xstart, self.ystart, self.xend, self.yend = coords
        self.xmax = max(self.xstart, self.xend)
        self.ymax = max(self.ystart, self.yend)
        self.ymin = min(self.ystart, self.yend)

    def __repr__(self):
        return "Line([%s, %s, %s, %s])" % (self.xstart, self.ystart, self.xend, self.yend)

################################################################################
class Application(Frame):
    def __init__(self, master):
        Frame.__init__(self, master)
        self.w = 780
        self.h = 490
        frame = Frame(master, width= self.w, height=self.h)
        self.master = master
        self.x = -1
        self.y = -1
        #self.g_rip = gcode_ripper_lib.G_Code_Rip()
        self.g_rip = G_Code_Rip()
        self.createWidgets()
       
    def createWidgets(self):
        self.initComplete = 0
        self.master.bind("<Configure>", self.Master_Configure)
        self.master.bind('<Enter>', self.bindConfigure)
        #self.master.bind('<Escape>', self.KEY_ESC)
        self.master.bind('<F1>', self.KEY_F1)
        self.master.bind('<F2>', self.KEY_F2)
        self.master.bind('<F5>', self.KEY_F5) #self.Recalculate_Click)
        self.master.bind('<Prior>', self.KEY_ZOOM_IN) # Page Up
        self.master.bind('<Next>', self.KEY_ZOOM_OUT) # Page Down
        self.master.bind('<Control-g>', self.KEY_CTRL_G)
        
        self.show_axis  = BooleanVar()      
        self.show_box   = BooleanVar()
        self.show_path  = BooleanVar()
        
        self.rotateb    = BooleanVar()
        self.plot_view  = StringVar()        
       
        self.arc2line   = BooleanVar()
        self.var_dis    = BooleanVar()
        self.WriteAll   = BooleanVar()
        self.NoComments = BooleanVar()
        
        self.SCALEXY    = StringVar()
        self.SCALEZ     = StringVar()
        self.ROTATE     = StringVar()
        self.SCALEF     = StringVar()

        self.gcode_op   = StringVar()
        
        self.SPLITA     = StringVar()
        self.SPLITX     = StringVar()
        self.SPLITY     = StringVar()
        self.ZSAFE      = StringVar()

        self.WRAP_DIA   = StringVar()
        self.WRAP_TYPE  = StringVar()
        self.WRAP_FSCALE= StringVar()

        self.EXP_TYPE   = StringVar()
        self.Exp_Rapids = BooleanVar()

        self.origin     = StringVar()
        self.units      = StringVar()
        self.segarc     = StringVar()
        self.accuracy   = StringVar()
        self.funits     = StringVar()
        self.FEED       = StringVar()
        
        self.gpre       = StringVar()
        self.gpost      = StringVar()

        self.DPlaces_L  = StringVar()
        self.DPlaces_R  = StringVar()
        self.DPlaces_F  = StringVar()


        self.sr_tool_dia= StringVar()
        self.sr_step    = StringVar()
        self.sr_minx    = StringVar()
        self.sr_maxx    = StringVar()
        self.sr_zsafe   = StringVar()
        self.sr_feed    = StringVar()
        self.sr_remove  = StringVar()
        self.sr_plungef = StringVar()
        self.SRmin_label= StringVar()
        self.SRmax_label= StringVar()
        self.sr_climb     = BooleanVar()
        self.Wrap_Rev_Rot = BooleanVar()

        self.probe_feed    = StringVar()
        self.probe_depth   = StringVar()
        self.probe_safe    = StringVar()
        self.probe_nX      = StringVar()
        self.probe_nY      = StringVar()
        #self.probe_xPartitionLength =  StringVar()
        #self.probe_yPartitionLength =  StringVar()
                
        self.probe_istep   = StringVar()
        self.probe_offsetX = StringVar()
        self.probe_offsetY = StringVar()
        self.probe_offsetZ = StringVar()
        self.probe_precodes= StringVar()
        self.probe_pcodes  = StringVar()
        self.probe_soft    = StringVar()
        self.probe_points  = StringVar()
        self.savepts       = BooleanVar()

        self.current_input_file = StringVar()
         
        ###########################################################################
        #                         INITIALIZE VARIABLES                            #
        #    if you want to change a default setting this is the place to do it   #
        ###########################################################################
        self.show_axis.set(1)
        self.show_box.set(1)
        self.show_path.set(1)
        self.rotateb.set(0)
        self.arc2line.set(0)
        self.var_dis.set(1)

        self.plot_view.set("XY") 
        self.SCALEXY.set("100")
        self.SCALEZ.set("100")
        self.SCALEF.set("100")

        self.gcode_op.set("split")
        self.SPLITA.set("0.0")
        self.SPLITX.set("0.0")
        self.SPLITY.set("0.0")
        self.ZSAFE.set("0.25")       
        self.ROTATE.set("0.0")


        self.WRAP_DIA.set("10.0")
        self.WRAP_TYPE.set("Y2A")       # Options are: "Y2A","X2B","Y2B","X2A"
        self.WRAP_FSCALE.set("Scale-Rotary") # Options are: "Scale-Rotary", "None"
        
        self.EXP_TYPE.set("DXF")
        self.Exp_Rapids.set(1)


        self.origin.set("Default")      # Options are "Default",
                                        #             "Top-Left", "Top-Center", "Top-Right",
                                        #             "Mid-Left", "Mid-Center", "Mid-Right",
                                        #             "Bot-Left", "Bot-Center", "Bot-Right" 
                                            
        self.units.set("in")            # Options are "in" and "mm"
        self.FEED.set("5.0")

        self.segarc.set("5.0")
        self.accuracy.set("0.001")

        self.DPlaces_L.set("4")
        self.DPlaces_R.set("3")
        self.DPlaces_F.set("2")
        self.WriteAll.set(0)
        self.NoComments.set(0)


        self.sr_tool_dia.set("0.25")
        self.sr_step.set("25")
        self.sr_minx.set("0")
        self.sr_maxx.set("3")
        self.sr_zsafe.set(".5")
        self.sr_remove.set("-0.05")
        self.sr_feed.set("10")
        self.sr_plungef.set("5")
        self.sr_climb.set(0)
        self.Wrap_Rev_Rot.set(0)


        self.probe_feed.set("5")
        self.probe_depth.set("-.5")
        self.probe_safe.set(".25")
        self.probe_nX.set("3")
        self.probe_nY.set("3")
        self.probe_istep.set("4")
        self.probe_offsetX.set("0.0")
        self.probe_offsetY.set("0.0")
        self.probe_offsetZ.set("0.0")
        self.probe_precodes.set("(G Code)")
        self.probe_pcodes.set("(G Code)")
        self.probe_soft.set("LinuxCNC") # "LinuxCNC", "MACH3", "MACH4" or "DDCS"
        self.probe_points.set("All")  #"All". "Required"
        self.savepts.set(1)

        self.segID      = []
        self.gcode      = []
        self.coords     = []
        self.probe_data = []
        
        self.MAXX    = 0
        self.MINX    = 0
        self.MAXY    = 0
        self.MINY    = 0
        self.MAXZ    = 0
        self.MINZ    = 0
        self.HOME_DIR     =  os.path.expanduser("~")
        self.NGC_OUTPUT   = (self.HOME_DIR+"/None")
        self.NGC_INPUT    = (self.HOME_DIR+"/None")
        self.PROBE_INPUT  = (self.HOME_DIR+"/None")
        self.current_input_file.set(" ")
        
        # PAN and ZOOM STUFF
        self.panx = 0
        self.panx = 0
        self.lastx = 0
        self.lasty = 0
        
        # Derived variables
            
        if self.units.get() == 'in':
            self.funits.set('in/min')
        else:
            self.units.set('mm')
            self.funits.set('mm/min')
            
        ##########################################################################
        #                         G-Code Default Preamble                        #
        ##########################################################################
        self.gpre.set("(G-Code Preamble)")
        ##########################################################################
        #                         G-Code Default Postamble                       #
        ##########################################################################
        self.gpost.set("(G-Code Postamble)")
        
        ##########################################################################
        ###                     END INITIALIZING VARIABLES                     ###
        ##########################################################################
        self.config_file = "g-code-ripper_config.ngc"
        home_config1 = self.HOME_DIR + "/" + self.config_file
        config_file2 = ".gcoderipperrc"
        home_config2 = self.HOME_DIR + "/" + config_file2
        if ( os.path.isfile(self.config_file) ):
            self.Open_Config_File(self.config_file)
        elif ( os.path.isfile(home_config1) ):
            self.Open_Config_File(home_config1)
        elif ( os.path.isfile(home_config2) ):
            self.Open_Config_File(home_config2)

        opts, args = None, None
        try:
            opts, args = getopt.getopt(sys.argv[1:], "hg:c:d:",["help", "gcode_file=","config_file=","defdir="])
        except:
            fmessage('Unable interpret command line options')
            sys.exit()
        for option, value in opts:
            if option in ('-h','--help'):
                fmessage(' ')
                fmessage('Usage: python g-code-ripper.py [-g file | -d directory ]')
                fmessage('-c    : config file to read (also --config_file)')
                fmessage('-g    : gcode file to read (also --gcode_file)')
                fmessage('-d    : default directory (also --defdir)')
                fmessage('-h    : print this help (also --help)\n')
                sys.exit()
            if option in ('-g','--gcode_file'):
                self.Open_G_Code_File(value,Refresh=False)
                self.NGC_INPUT = value
            if option in ('-c','--config_file'):
                self.Open_Config_File(value)
            if option in ('-d','--defdir'):
                self.HOME_DIR = value
                if str.find(self.NGC_OUTPUT,'/None') != -1:
                    self.NGC_OUTPUT = (self.HOME_DIR+"/None")
                if str.find(self.NGC_INPUT,'/None') != -1:
                    self.NGC_INPUT = (self.HOME_DIR+"/None")
        
        ##########################################################################

        # make a Status Bar
        self.statusMessage = StringVar()
        self.statusMessage.set("")
        self.statusbar = Label(self.master, textvariable=self.statusMessage, \
                                   bd=1, relief=SUNKEN , height=1)
        self.statusbar.pack(anchor=SW, fill=X, side=BOTTOM)
        self.statusMessage.set("Welcome to G-Code-Ripper")

        # Buttons
        self.Recalculate = Button(self.master,text="Recalculate")
        self.Recalculate.bind("<ButtonRelease-1>", self.Recalculate_Click)

        self.WriteBaseButton = Button(self.master,text="Save G-Code File - Base",
                                      command=self.menu_File_Save_G_Code_Base)
        
        self.WriteRightButton = Button(self.master,text="Save G-Code File - White",
                                       command=self.menu_File_Save_G_Code_Right)
        self.WriteLeftButton = Button(self.master,text="Save G-Code File - Black",
                                      command=self.menu_File_Save_G_Code_Left)

        # Canvas
        lbframe = Frame( self.master )
        self.PreviewCanvas_frame = lbframe
        self.PreviewCanvas = Canvas(lbframe, width=self.w-525, \
                                        height=self.h-200, background="grey75")
        self.PreviewCanvas.pack(side=LEFT, fill=BOTH, expand=1)
        self.PreviewCanvas_frame.place(x=230, y=10)
        
        self.PreviewCanvas.bind("<Button-4>" , self._mouseZoomIn)
        self.PreviewCanvas.bind("<Button-5>" , self._mouseZoomOut)
        self.PreviewCanvas.bind("<2>"        , self.mousePanStart)
        self.PreviewCanvas.bind("<B2-Motion>", self.mousePan)
        self.PreviewCanvas.bind("<1>"        , self.mouseZoomStart)
        self.PreviewCanvas.bind("<B1-Motion>", self.mouseZoom)
        self.PreviewCanvas.bind("<3>"        , self.mousePanStart)
        self.PreviewCanvas.bind("<B3-Motion>", self.mousePan)

        # Left Column #
        #----------------------
        self.Label_Gcode_Operations = Label(self.master,text="G-Code Base Operations:", anchor=W)
        
        self.Label_GscaleXY = Label(self.master,text="Scale XY", anchor=CENTER)
        self.Label_GscaleXY_u = Label(self.master,text="%", anchor=W)
        self.Entry_GscaleXY = Entry(self.master,width="15")
        self.Entry_GscaleXY.configure(textvariable=self.SCALEXY)
        self.Entry_GscaleXY.bind('<Return>', self.Recalculate_Click)
        self.SCALEXY.trace_variable("w", self.Entry_GscaleXY_Callback)
        self.NormalColor =  self.Entry_GscaleXY.cget('bg')


        self.Label_GscaleZ = Label(self.master,text="Scale Z", anchor=CENTER)
        self.Label_GscaleZ_u = Label(self.master,text="%", anchor=W)
        self.Entry_GscaleZ = Entry(self.master,width="15")
        self.Entry_GscaleZ.configure(textvariable=self.SCALEZ)
        self.Entry_GscaleZ.bind('<Return>', self.Recalculate_Click)
        self.SCALEZ.trace_variable("w", self.Entry_GscaleZ_Callback)
        self.NormalColor =  self.Entry_GscaleZ.cget('bg')

        self.Label_GscaleF = Label(self.master,text="Scale Feed", anchor=CENTER)
        self.Label_GscaleF_u = Label(self.master,text="%", anchor=W)
        self.Entry_GscaleF = Entry(self.master,width="15")
        self.Entry_GscaleF.configure(textvariable=self.SCALEF)
        self.Entry_GscaleF.bind('<Return>', self.Recalculate_Click)
        self.SCALEF.trace_variable("w", self.Entry_GscaleF_Callback)
        self.NormalColor =  self.Entry_GscaleF.cget('bg')
        
        self.Label_Rotate = Label(self.master,text="Rotate")
        self.Label_Rotate_u = Label(self.master,text="deg", anchor=W)
        self.Entry_Rotate = Entry(self.master,width="15")
        self.Entry_Rotate.configure(textvariable=self.ROTATE)
        self.Entry_Rotate.bind('<Return>', self.Recalculate_Click)
        self.ROTATE.trace_variable("w", self.Entry_Rotate_Callback)

        self.Label_Origin      = Label(self.master,text="Origin", anchor=CENTER )
        self.Origin_OptionMenu = OptionMenu(root, self.origin,
                                            "Top-Left",
                                            "Top-Center",
                                            "Top-Right",
                                            "Mid-Left",
                                            "Mid-Center",
                                            "Mid-Right",
                                            "Bot-Left",
                                            "Bot-Center",
                                            "Bot-Right",
                                            "Default", command=self.Recalculate_RQD_Click)
        #----------------------
        self.Label_view_opt = Label(self.master,text="View Plane:", anchor=W)
        self.Radio_View_XY = Radiobutton(self.master,text="XY", value="XY",
                                         width="100", anchor=W)
        self.Radio_View_XY.configure(variable=self.plot_view)
        self.Radio_View_XZ = Radiobutton(self.master,text="XZ", value="XZ",
                                         width="100", anchor=W)
        self.Radio_View_XZ.configure(variable=self.plot_view)
        self.Radio_View_YZ = Radiobutton(self.master,text="YZ", value="YZ",
                                         width="100", anchor=W)
        self.Radio_View_YZ.configure(variable=self.plot_view)
        self.Radio_View_ISO1 = Radiobutton(self.master,text="ISO1", value="ISO1",
                                         width="100", anchor=W)
        self.Radio_View_ISO1.configure(variable=self.plot_view)
        self.Radio_View_ISO2 = Radiobutton(self.master,text="ISO2", value="ISO2",
                                         width="100", anchor=W)
        self.Radio_View_ISO2.configure(variable=self.plot_view)
        self.Radio_View_ISO3 = Radiobutton(self.master,text="ISO3", value="ISO3",
                                         width="100", anchor=W)
        self.Radio_View_ISO3.configure(variable=self.plot_view)
        self.plot_view.trace_variable("w", self.menu_View_Refresh_Callback)


        #-----------------------
        self.Label_code_ops = Label(self.master,text="G-Code Operations:", anchor=W)
        self.Radio_Gcode_None = Radiobutton(self.master,text="None", value="none",
                                         width="100", anchor=W)
        self.Radio_Gcode_None.configure(variable=self.gcode_op )

        self.Radio_Gcode_Split = Radiobutton(self.master,text="Split", value="split",
                                         width="100", anchor=W)
        self.Radio_Gcode_Split.configure(variable=self.gcode_op )
        
        self.Radio_Gcode_Wrap = Radiobutton(self.master,text="Wrap", value="wrap",
                                         width="100", anchor=W)
        self.Radio_Gcode_Wrap.configure(variable=self.gcode_op )

        self.Radio_Gcode_Export = Radiobutton(self.master,text="Export (DXF, CSV)", value="export",
                                         width="100", anchor=W)
        self.Radio_Gcode_Export.configure(variable=self.gcode_op )

        self.Radio_Gcode_Probe = Radiobutton(self.master,text="Auto Probe", value="probe",
                                         width="100", anchor=W)
        self.Radio_Gcode_Probe.configure(variable=self.gcode_op )
        
        self.gcode_op.trace_variable("w", self.Entry_recalc_var_Callback)

        
        # End Left Column #
        self.separator1 = Frame(height=2, bd=1, relief=SUNKEN)
        self.separator2 = Frame(height=2, bd=1, relief=SUNKEN)
        self.separator3 = Frame(height=2, bd=1, relief=SUNKEN)
        self.separator4 = Frame(height=2, bd=1, relief=SUNKEN)
        self.separator5 = Frame(height=2, bd=1, relief=SUNKEN)
        self.separator6 = Frame(height=2, bd=1, relief=SUNKEN)
        self.separator7 = Frame(height=2, bd=1, relief=SUNKEN)
        
        # Right Column #

        ### SPLIT ###
        self.Label_Gcode_Split_Properties = Label(self.master,text="G-Code Split Properties:",\
                                           anchor=W)
        
        self.Label_SplitX = Label(self.master,text="Split X Position", anchor=CENTER )
        self.Label_SplitX_u = Label(self.master,textvariable=self.units, anchor=W)
        self.Entry_SplitX = Entry(self.master,width="15")
        self.Entry_SplitX.configure(textvariable=self.SPLITX)
        self.Entry_SplitX.bind('<Return>', self.Recalculate_Click)
        self.SPLITX.trace_variable("w", self.Entry_SplitX_Callback)

        self.Label_SplitY = Label(self.master,text="Split Y Position", anchor=CENTER )
        self.Label_SplitY_u = Label(self.master,textvariable=self.units, anchor=W)
        self.Entry_SplitY = Entry(self.master,width="15")
        self.Entry_SplitY.configure(textvariable=self.SPLITY)
        self.Entry_SplitY.bind('<Return>', self.Recalculate_Click)
        self.SPLITY.trace_variable("w", self.Entry_SplitY_Callback)
        
        self.Label_SplitA = Label(self.master,text="Split Angle", anchor=CENTER )
        self.Label_SplitA_u = Label(self.master,text="deg", anchor=W)
        self.Entry_SplitA = Entry(self.master,width="15")
        self.Entry_SplitA.configure(textvariable=self.SPLITA)
        self.Entry_SplitA.bind('<Return>', self.Recalculate_Click)
        self.SPLITA.trace_variable("w", self.Entry_SplitA_Callback)

        self.Label_rotateb = Label(self.master,text="Rotate Black")
        self.Checkbutton_rotateb = Checkbutton(self.master,text=" ", anchor=W)   
        self.Checkbutton_rotateb.configure(variable=self.rotateb)
        self.rotateb.trace_variable("w", self.Entry_recalc_var_Callback)

        self.Label_gcode_opt = Label(self.master,text="G-Code Properties:", anchor=W)
        
        self.Label_Feed = Label(self.master,text="Plunge Feed")
        self.Label_Feed_u = Label(self.master,textvariable=self.funits, anchor=W)
        self.Entry_Feed = Entry(self.master,width="15")
        self.Entry_Feed.configure(textvariable=self.FEED)
        self.Entry_Feed.bind('<Return>', self.Recalculate_Click)
        self.FEED.trace_variable("w", self.Entry_Feed_Callback)

        self.Label_Zsafe = Label(self.master,text="Z Safe")
        self.Label_Zsafe_u = Label(self.master,textvariable=self.units, anchor=W)
        self.Entry_Zsafe = Entry(self.master,width="15")
        self.Entry_Zsafe.configure(textvariable=self.ZSAFE)
        self.Entry_Zsafe.bind('<Return>', self.Recalculate_Click)
        self.ZSAFE.trace_variable("w", self.Entry_Zsafe_Callback)

        ### WRAP ###
        self.Label_Gcode_Wrap_Properties = Label(self.master,text="G-Code Wrap Properties:",\
                                           anchor=W)
        
        self.Label_Wrap_DIA = Label(self.master,text="Wrap Diameter", anchor=CENTER )
        self.Label_Wrap_DIA_u = Label(self.master,textvariable=self.units, anchor=W)
        self.Entry_Wrap_DIA = Entry(self.master,width="15")
        self.Entry_Wrap_DIA.configure(textvariable=self.WRAP_DIA)
        self.Entry_Wrap_DIA.bind('<Return>', self.Recalculate_Click)
        self.WRAP_DIA.trace_variable("w", self.Entry_Wrap_DIA_Callback)

        self.Label_Radio_Wrap = Label(self.master,text="Axis Wrap Type:", anchor=W)
        self.Radio_Wrap_Y2A = Radiobutton(self.master,text="Y-axis to A-axis", value="Y2A",
                                         width="100", anchor=W)
        self.Radio_Wrap_Y2A.configure(variable=self.WRAP_TYPE )        
        self.Radio_Wrap_X2B = Radiobutton(self.master,text="X-axis to B-axis", value="X2B",
                                         width="100", anchor=W)
        self.Radio_Wrap_X2B.configure(variable=self.WRAP_TYPE )
        self.Radio_Wrap_Y2B = Radiobutton(self.master,text="Y-axis to B-axis", value="Y2B",
                                         width="100", anchor=W)
        self.Radio_Wrap_Y2B.configure(variable=self.WRAP_TYPE )
        self.Radio_Wrap_X2A = Radiobutton(self.master,text="X-axis to A-axis", value="X2A",
                                         width="100", anchor=W)
        self.Radio_Wrap_X2A.configure(variable=self.WRAP_TYPE )
        self.WRAP_TYPE.trace_variable("w", self.Entry_recalc_var_Callback)
        
        self.Label_WRAP_FSCALE = Label(self.master,text="Feed Adjust:", anchor=W)
        self.WRAP_FSCALE_OptionMenu = OptionMenu(root, self.WRAP_FSCALE, "Scale-Rotary", "None")
                
        self.Label_WRAP_REV_ROT = Label(self.master,text="Reverse Rotary Axis", anchor=W)
        self.Checkbutton_WRAP_REV_ROT = Checkbutton(self.master,text="", anchor=W)
        self.Checkbutton_WRAP_REV_ROT.configure(variable=self.Wrap_Rev_Rot)

        self.WriteWrapButton = Button(self.master,text="Save G-Code File - Wrap",
                                      command=self.menu_File_Save_G_Code_Wrap)
        
        self.WriteRoundButton = Button(self.master,text="Stock Rounding",
                                      command=self.STOCK_Round_Window)

        ## Define "Export" mode input fields here
        self.Label_Gcode_Export_Properties = Label(self.master,text="Export Properties:",\
                                           anchor=W)

        self.Label_Radio_Export = Label(self.master,text="File Type:", anchor=W)
        self.Radio_Export_DXF = Radiobutton(self.master,text="DXF", value="DXF",
                                        width="100", anchor=W)
        self.Radio_Export_DXF.configure(variable=self.EXP_TYPE )
        
        self.Radio_Export_CSV = Radiobutton(self.master,text="CSV (text)", value="CSV",
                                         width="100", anchor=W)
        self.Radio_Export_CSV.configure(variable=self.EXP_TYPE )
        self.WRAP_TYPE.trace_variable("w", self.Entry_recalc_var_Callback)


        self.Label_EXP_RAPIDS = Label(self.master,text="Include Rapid Moves", anchor=W)
        self.Checkbutton_EXP_RAPIDS = Checkbutton(self.master,text="", anchor=W)
        self.Checkbutton_EXP_RAPIDS.configure(variable=self.Exp_Rapids)

        
        self.WriteExportButton = Button(self.master,text="Export File",
                                      command=self.menu_File_Save_Export_Write)

        ### PROBE ###
        self.Label_Gcode_Probe_Properties = Label(self.master,text="Auto-Probe Properties:",\
                                           anchor=W)

        self.Label_ProbeOffsetX   = Label(self.master,text="Probe X Offset", anchor=CENTER )
        self.Label_ProbeOffsetX_u = Label(self.master,textvariable=self.units, anchor=W)
        self.Entry_ProbeOffsetX   = Entry(self.master,width="15")
        self.Entry_ProbeOffsetX.configure(textvariable=self.probe_offsetX)
        self.Entry_ProbeOffsetX.bind('<Return>', self.Recalculate_Click)
        self.probe_offsetX.trace_variable("w", self.Entry_ProbeOffsetX_Callback)

        self.Label_ProbeOffsetY   = Label(self.master,text="Probe Y Offset", anchor=CENTER )
        self.Label_ProbeOffsetY_u = Label(self.master,textvariable=self.units, anchor=W)
        self.Entry_ProbeOffsetY   = Entry(self.master,width="15")
        self.Entry_ProbeOffsetY.configure(textvariable=self.probe_offsetY)
        self.Entry_ProbeOffsetY.bind('<Return>', self.Recalculate_Click)
        self.probe_offsetY.trace_variable("w", self.Entry_ProbeOffsetY_Callback)

        self.Label_ProbeOffsetZ   = Label(self.master,text="Probe Z Offset", anchor=CENTER )
        self.Label_ProbeOffsetZ_u = Label(self.master,textvariable=self.units, anchor=W)
        self.Entry_ProbeOffsetZ   = Entry(self.master,width="15")
        self.Entry_ProbeOffsetZ.configure(textvariable=self.probe_offsetZ)
        self.Entry_ProbeOffsetZ.bind('<Return>', self.Recalculate_Click)
        self.probe_offsetZ.trace_variable("w", self.Entry_ProbeOffsetZ_Callback)
        
        self.Label_ProbeSafe = Label(self.master,text="Probe Z Safe")
        self.Label_ProbeSafe_u = Label(self.master,textvariable=self.units, anchor=W)
        self.Entry_ProbeSafe = Entry(self.master,width="15")
        self.Entry_ProbeSafe.configure(textvariable=self.probe_safe)
        self.probe_safe.trace_variable("w", self.Entry_ProbeSafe_Callback)

        self.Label_ProbeDepth = Label(self.master,text="Probe Depth")
        self.Label_ProbeDepth_u = Label(self.master,textvariable=self.units, anchor=W)
        self.Entry_ProbeDepth = Entry(self.master,width="15")
        self.Entry_ProbeDepth.configure(textvariable=self.probe_depth)
        self.Entry_ProbeDepth.bind('<Return>', self.Recalculate_Click)
        self.probe_depth.trace_variable("w", self.Entry_ProbeDepth_Callback)

        self.Label_ProbeFeed = Label(self.master,text="Probe Feed")
        self.Label_ProbeFeed_u = Label(self.master,textvariable=self.funits, anchor=W)
        self.Entry_ProbeFeed = Entry(self.master,width="15")
        self.Entry_ProbeFeed.configure(textvariable=self.probe_feed)
        self.probe_feed.trace_variable("w", self.Entry_ProbeFeed_Callback)
        
        self.Label_Probe_Num_X = Label(self.master,text="X Points")
        self.Entry_Probe_Num_X = Entry(self.master,width="15")
        self.Entry_Probe_Num_X.configure(textvariable=self.probe_nX)
        self.Entry_Probe_Num_X.bind('<Return>', self.Recalculate_Click)
        self.probe_nX.trace_variable("w", self.Entry_probe_nX_Callback)

        self.Label_Probe_Num_Y = Label(self.master,text="Y Points")
        self.Entry_Probe_Num_Y = Entry(self.master,width="15")
        self.Entry_Probe_Num_Y.configure(textvariable=self.probe_nY)
        self.Entry_Probe_Num_Y.bind('<Return>', self.Recalculate_Click)
        self.probe_nY.trace_variable("w", self.Entry_probe_nY_Callback)

#        self.Label_ProbeInterpSpace = Label(self.master,text="Interp. Steps")
#        self.Entry_ProbeInterpSpace = Entry(self.master,width="15")
#        self.Entry_ProbeInterpSpace.configure(textvariable=self.probe_istep)
#        self.probe_istep.trace_variable("w", self.Entry_ProbeIStep_Callback)

        self.Label_ProbePreCodes = Label(self.master,text="Pre Probe")
        self.Entry_ProbePreCodes = Entry(self.master,width="15")
        self.Entry_ProbePreCodes.configure(textvariable=self.probe_precodes)

        self.Label_ProbePauseCodes = Label(self.master,text="Post Probe")
        self.Label_ProbePauseCodes_u = Label(self.master,textvariable=self.units, anchor=W)
        self.Entry_ProbePauseCodes = Entry(self.master,width="15")
        self.Entry_ProbePauseCodes.configure(textvariable=self.probe_pcodes)

        self.Label_ProbeSoft = Label(self.master,text="Controller:", anchor=W)
        self.ProbeSoft_OptionMenu = OptionMenu(root, self.probe_soft, "LinuxCNC", "MACH3", "MACH4", "DDCS")

        self.Label_ProbePoints = Label(self.master,text="Probe Points:", anchor=W)
        self.ProbePoints_OptionMenu = OptionMenu(root, self.probe_points, "All", "Required")
        
        self.Label_SavePoints = Label(self.master,text="Save Probe Data")
        self.Checkbutton_SavePoints = Checkbutton(self.master,text=" ", anchor=W)   
        self.Checkbutton_SavePoints.configure(variable=self.savepts)

        
        self.WriteProbeOnlyButton = Button(self.master,text="Save G-Code File - Probe Only",
                                    command=self.menu_File_Save_G_Code_ProbeOnly)
        
        self.ReadProbeButton = Button(self.master,text="Read Probe Data File",
                                    command=self.menu_File_Read_Probe_data)

        self.ClearProbeButton = Button(self.master,text="Clear Probe Data",
                                    command=self.menu_Clear_Probe_data)
                
        self.WriteAdjustedButton = Button(self.master,text="Save G-Code File - Adjusted",
                                    command=self.menu_File_Save_G_Code_Adjusted)

        self.WriteProbeButton = Button(self.master,text="Save G-Code File - Probe & Cut",
                                    command=self.menu_File_Save_G_Code_Probe_n_Cut)
        
        ############

        # End Right Column #

        #GEN Setting Window Entry initialization
        self.Entry_ArcAngle  = Entry()
        self.Entry_Accuracy  = Entry()
        self.Entry_DPlaces_L = Entry()
        self.Entry_DPlaces_R = Entry()
        self.Entry_DPlaces_F = Entry()
        

        # Make Menu Bar
        self.menuBar = Menu(self.master, relief = "raised", bd=2)
        
        self.top_File = Menu(self.menuBar, tearoff=0)
        self.top_File.add("command", label = "Open G-Code File", \
                         command = self.menu_File_Open_G_Code_File)
        self.top_File.add("command", label = "Save G-Code File - Base", \
                         command = self.menu_File_Save_G_Code_Base)
        #self.top_File.add("command", label = "Save G-Code File - Black", \
        #                 command = self.menu_File_Save_G_Code_Left)
        #self.top_File.add("command", label = "Save G-Code File - White", \
        #                 command = self.menu_File_Save_G_Code_Right)

        self.top_File.add("command", label = "Exit", command = self.menu_File_Quit)
        self.menuBar.add("cascade", label="File", menu=self.top_File)

        self.top_Edit = Menu(self.menuBar, tearoff=0)
        self.top_Edit.add("command", label = "Copy G-Code Data to Clipboard - Base", \
                         command = self.menu_CopyClipboard_GCode_Base)
        #self.top_Edit.add("command", label = "Copy G-Code Data to Clipboard - Black", \
        #                 command = self.menu_CopyClipboard_GCode_Left)
        #self.top_Edit.add("command", label = "Copy G-Code Data to Clipboard - White", \
        #                 command = self.menu_CopyClipboard_GCode_Right)
        
        self.menuBar.add("cascade", label="Edit", menu=self.top_Edit)

        self.top_View = Menu(self.menuBar, tearoff=0)
        self.top_View.add("command", label = "Recalculate", command = self.menu_View_Recalculate)
        self.top_View.add_separator()    

        self.top_View.add("command", label = "Zoom In <Page Up>", command = self.menu_View_Zoom_in)
        self.top_View.add("command", label = "Zoom Out <Page Down>", command = self.menu_View_Zoom_out)
        self.top_View.add("command", label = "Zoom Fit <F5>", command = self.menu_View_Refresh)

        self.top_View.add_separator()
        
        self.top_View.add_checkbutton(label = "Show Origin Axis",  variable=self.show_axis , \
                                     command= self.menu_View_Refresh)
        self.top_View.add_checkbutton(label = "Show Bounding Box", variable=self.show_box  , \
                                     command= self.menu_View_Refresh)
        self.top_View.add_checkbutton(label = "Plot G-code Path",  variable=self.show_path , \
                                     command= self.menu_View_Refresh)
        self.menuBar.add("cascade", label="View", menu=self.top_View)

        self.top_Settings = Menu(self.menuBar, tearoff=0)
        self.top_Settings.add("command", label = "General Settings", \
                             command = self.GEN_Settings_Window)
                
        self.menuBar.add("cascade", label="Settings", menu=self.top_Settings)

        self.top_Help = Menu(self.menuBar, tearoff=0)
        self.top_Help.add("command", label = "About", command = self.menu_Help_About)
        self.top_Help.add("command", label = "Help (Web Page)", command = self.menu_Help_Web)
        self.menuBar.add("cascade", label="Help", menu=self.top_Help)

        self.master.config(menu=self.menuBar)

        ## Load g-code file 
        #self.Open_G_Code_File(self.NGC_INPUT,Refresh=False)
        
        
        
################################################################################
    def entry_set(self, val2, calc_flag=0, new=0):
        if calc_flag == 0 and new==0:
            try:
                self.statusbar.configure( bg = 'yellow' )
                val2.configure( bg = 'yellow' )
                self.statusMessage.set(" Recalculation required.")
            except:
                pass
        elif calc_flag == 3:
            try:
                val2.configure( bg = 'red' )
                self.statusbar.configure( bg = 'red' )
                self.statusMessage.set(" Value should be a number. ")
            except:
                pass
        elif calc_flag == 2:
            try:
                self.statusbar.configure( bg = 'red' )
                val2.configure( bg = 'red' )
                #self.statusMessage.set(message)
            except:
                pass
        elif (calc_flag == 0 or calc_flag == 1) and new==1 :
            try:
                self.statusbar.configure( bg = 'white' )
                self.statusMessage.set(" ")
                val2.configure( bg = 'white' )
            except:
                pass
        elif (calc_flag == 1) and new==0 :
            try:
                self.statusbar.configure( bg = 'white' )
                self.statusMessage.set(" ")
                val2.configure( bg = 'white' )
            except:
                pass

            
        elif (calc_flag == 0 or calc_flag == 1) and new==2:
            return 0
        return 1


    ################################################################################
    def Write_Config_File(self, event):
        config = []
        config.append('( Configuration File for G-Code Ripper-'+version+'.py widget )')
        config.append('( by Scorch - 2013-2021 )')
        config.append("(=========================================================)")
        # BOOL
        config.append('(g-code_ripper_set show_axis  %s )' %( int(self.show_axis.get())  ))
        config.append('(g-code_ripper_set show_box   %s )' %( int(self.show_box.get())   ))
        config.append('(g-code_ripper_set show_path   %s )' %( int(self.show_path.get())   ))
        config.append('(g-code_ripper_set rotateb    %s )' %( int(self.rotateb.get())    ))
        config.append('(g-code_ripper_set arc2line   %s )' %( int(self.arc2line.get())   ))
        config.append('(g-code_ripper_set var_dis    %s )' %( int(self.var_dis.get())    ))
        config.append('(g-code_ripper_set WriteAll   %s )' %( int(self.WriteAll.get())   ))
        config.append('(g-code_ripper_set NoComments %s )' %( int(self.NoComments.get()) ))
        config.append('(g-code_ripper_set Exp_Rapids %s )' %( int(self.Exp_Rapids.get()) ))
        config.append('(g-code_ripper_set savepts    %s )' %( int(self.savepts.get())    ))
        
        # STRING.get()
        config.append('(g-code_ripper_set units      %s )' %( self.units.get()    ))
        config.append('(g-code_ripper_set SCALEXY    %s )' %( self.SCALEXY.get()  ))
        config.append('(g-code_ripper_set SCALEZ     %s )' %( self.SCALEZ.get()   ))
        config.append('(g-code_ripper_set SCALEF     %s )' %( self.SCALEF.get()   ))
        config.append('(g-code_ripper_set ROTATE     %s )' %( self.ROTATE.get()   ))
        config.append('(g-code_ripper_set SPLITA     %s )' %( self.SPLITA.get()   ))
        config.append('(g-code_ripper_set SPLITX     %s )' %( self.SPLITX.get()   ))
        config.append('(g-code_ripper_set SPLITY     %s )' %( self.SPLITY.get()   ))
        config.append('(g-code_ripper_set ZSAFE      %s )' %( self.ZSAFE.get()    ))
        config.append('(g-code_ripper_set origin     %s )' %( self.origin.get()   ))
        config.append('(g-code_ripper_set segarc     %s )' %( self.segarc.get()   ))
        config.append('(g-code_ripper_set accuracy   %s )' %( self.accuracy.get() ))
        config.append('(g-code_ripper_set FEED       %s )' %( self.FEED.get()     ))
        config.append('(g-code_ripper_set GCODE_OP   %s )' %( self.gcode_op.get() ))
        config.append('(g-code_ripper_set WRAP_DIA   %s )' %( self.WRAP_DIA.get() ))
        config.append('(g-code_ripper_set WRAP_TYPE  %s )' %( self.WRAP_TYPE.get()))
        config.append('(g-code_ripper_set WRAP_FSCALE %s )' %( self.WRAP_FSCALE.get()))
        config.append('(g-code_ripper_set EXP_TYPE %s   )' %( self.EXP_TYPE.get()))
        
        config.append('(g-code_ripper_set DPlaces_L  %s )' %( self.DPlaces_L.get()  ))
        config.append('(g-code_ripper_set DPlaces_R  %s )' %( self.DPlaces_R.get()  ))
        config.append('(g-code_ripper_set DPlaces_F  %s )' %( self.DPlaces_F.get()  ))

        config.append('(g-code_ripper_set sr_tool_dia %s )' %( self.sr_tool_dia.get() ))
        config.append('(g-code_ripper_set sr_step     %s )' %( self.sr_step.get()     ))
        config.append('(g-code_ripper_set sr_minx     %s )' %( self.sr_minx.get()     ))
        config.append('(g-code_ripper_set sr_maxx     %s )' %( self.sr_maxx.get()     ))
        config.append('(g-code_ripper_set sr_zsafe    %s )' %( self.sr_zsafe.get()    ))
        config.append('(g-code_ripper_set sr_remove   %s )' %( self.sr_remove.get()   ))
        config.append('(g-code_ripper_set sr_feed     %s )' %( self.sr_feed.get()     ))
        config.append('(g-code_ripper_set sr_plungef  %s )' %( self.sr_plungef.get()  ))
        config.append('(g-code_ripper_set sr_climb     %s )' %( int(self.sr_climb.get())     ))
        config.append('(g-code_ripper_set Wrap_Rev_Rot %s )' %( int(self.Wrap_Rev_Rot.get()) ))

        config.append('(g-code_ripper_set probe_feed    %s )' %( self.probe_feed.get()    ))
        config.append('(g-code_ripper_set probe_depth   %s )' %( self.probe_depth.get()   ))
        config.append('(g-code_ripper_set probe_safe    %s )' %( self.probe_safe.get()    ))
        config.append('(g-code_ripper_set probe_nX      %s )' %( self.probe_nX.get()    ))
        config.append('(g-code_ripper_set probe_nY      %s )' %( self.probe_nY.get()    ))
        config.append('(g-code_ripper_set probe_istep   %s )' %( self.probe_istep.get()   ))
        config.append('(g-code_ripper_set probe_offsetX %s )' %( self.probe_offsetX.get() ))
        config.append('(g-code_ripper_set probe_offsetY %s )' %( self.probe_offsetY.get() ))
        config.append('(g-code_ripper_set probe_offsetZ %s )' %( self.probe_offsetZ.get() ))
        config.append('(g-code_ripper_set probe_precodes  \042%s\042 )' %( self.probe_precodes.get()  ))
        config.append('(g-code_ripper_set probe_pcodes  \042%s\042 )' %( self.probe_pcodes.get()  ))
        config.append('(g-code_ripper_set probe_soft    %s )' %( self.probe_soft.get()    ))
        config.append('(g-code_ripper_set probe_points  %s )' %( self.probe_points.get()    ))

        config.append('(g-code_ripper_set gpre       \042%s\042 )' %( self.gpre.get() ))
        config.append('(g-code_ripper_set gpost      \042%s\042 )' %( self.gpost.get()))
        config.append('(g-code_ripper_set NGC_INPUT  \042%s\042 )' %( self.NGC_INPUT  ))
        config.append('(g-code_ripper_set NGC_OUTPUT \042%s\042 )' %( self.NGC_OUTPUT ))
        config.append("(=========================================================)")
        
        configname_full   = os.path.expanduser("~")+"/"+self.config_file

        current_name = event.widget.winfo_parent()
        win_id = event.widget.nametowidget(current_name)
        
        if ( os.path.isfile(configname_full) ):
            try:
                win_id.withdraw()
            except:
                pass
                
            if not message_ask_ok_cancel("Replace", "Replace Exiting Configuration File?\n"+configname_full):
                try:
                    win_id.deiconify()
                except:
                    pass
                return 
        try:
            fout = open(configname_full,'w')
        except:
            self.statusMessage.set("Unable to open file for writing: %s" %(configname_full))
            self.statusbar.configure( bg = 'red' )
            return
        for line in config:
            try:
                fout.write(line+'\n')
            except:
                fout.write('(skipping line)\n')
        fout.close

        try:
            win_id.withdraw()
            win_id.deiconify()
        except:
            pass
    ################################################################################
    def Open_Config_File(self, filename):
        
        try:
            fin = open(filename,'r')
        except:
            fmessage("Unable to open file: %s" %(filename))
            return
        
        text_codes=[]
        ident = "g-code_ripper_set"
        for line in fin:
            if ident in line:
                # BOOL
                if   "show_axis"  in line:
                    self.show_axis.set(line[line.find("show_axis"):].split()[1])
                if   "show_box"  in line:
                    self.show_box.set(line[line.find("show_box"):].split()[1])
                if   "show_path"  in line:
                    self.show_path.set(line[line.find("show_path"):].split()[1])
                if   "rotateb"  in line:
                    self.rotateb.set(line[line.find("rotateb"):].split()[1])
                if   "arc2line"  in line:
                    self.arc2line.set(line[line.find("arc2line"):].split()[1])
                if   "var_dis"  in line:                    
                    self.var_dis.set(line[line.find("var_dis"):].split()[1])
                if   "WriteAll"  in line:                    
                    self.WriteAll.set(line[line.find("WriteAll"):].split()[1])
                if   "NoComments"  in line:                    
                    self.NoComments.set(line[line.find("NoComments"):].split()[1])
                if   "Exp_Rapids"  in line:                    
                    self.Exp_Rapids.set(line[line.find("Exp_Rapids"):].split()[1])
                if   "savepts"  in line:                    
                    self.savepts.set(line[line.find("savepts"):].split()[1])


                elif "units"    in line:
                    self.units.set(line[line.find("units"):].split()[1])                    
                elif "SCALEXY"    in line:
                    self.SCALEXY.set(line[line.find("SCALEXY"):].split()[1])  
                elif "SCALEZ"    in line:
                    self.SCALEZ.set(line[line.find("SCALEZ"):].split()[1])  
                elif "SCALEF"    in line:
                    self.SCALEF.set(line[line.find("SCALEF"):].split()[1])
                elif "ROTATE"    in line:
                    self.ROTATE.set(line[line.find("ROTATE"):].split()[1])
                elif "SPLITA"    in line:
                    self.SPLITA.set(line[line.find("SPLITA"):].split()[1])
                elif "SPLITX"    in line:
                    self.SPLITX.set(line[line.find("SPLITX"):].split()[1])
                elif "SPLITY"    in line:
                    self.SPLITY.set(line[line.find("SPLITY"):].split()[1])
                elif "ZSAFE"    in line:
                    self.ZSAFE.set(line[line.find("ZSAFE"):].split()[1])
                elif "origin"    in line:
                    self.origin.set(line[line.find("origin"):].split()[1])
                elif "segarc"    in line:
                    self.segarc.set(line[line.find("segarc"):].split()[1])
                elif "accuracy"    in line:
                    self.accuracy.set(line[line.find("accuracy"):].split()[1])
                elif "FEED"    in line:
                    self.FEED.set(line[line.find("FEED"):].split()[1])
                elif "GCODE_OP"    in line:
                    self.gcode_op.set(line[line.find("GCODE_OP"):].split()[1])
                elif "WRAP_DIA"    in line:
                    self.WRAP_DIA.set(line[line.find("WRAP_DIA"):].split()[1])
                elif "WRAP_TYPE"    in line:
                    self.WRAP_TYPE.set(line[line.find("WRAP_TYPE"):].split()[1])
                elif "WRAP_FSCALE"    in line:
                    self.WRAP_FSCALE.set(line[line.find("WRAP_FSCALE"):].split()[1])
                elif "EXP_TYPE"    in line:
                    self.EXP_TYPE.set(line[line.find("EXP_TYPE"):].split()[1])

                elif "sr_tool_dia" in line:
                    self.sr_tool_dia.set(line[line.find("sr_tool_dia"):].split()[1])
                elif "sr_step"     in line:
                    self.sr_step.set(line[line.find("sr_step"):].split()[1])
                elif "sr_minx"     in line:
                    self.sr_minx.set(line[line.find("sr_minx"):].split()[1])
                elif "sr_maxx"     in line:
                    self.sr_maxx.set(line[line.find("sr_maxx"):].split()[1])
                elif "sr_zsafe"    in line:
                    self.sr_zsafe.set(line[line.find("sr_zsafe"):].split()[1])
                elif "sr_remove"   in line:
                    self.sr_remove.set(line[line.find("sr_remove"):].split()[1])
                elif "sr_feed"     in line:
                    self.sr_feed.set(line[line.find("sr_feed"):].split()[1])
                elif "sr_plungef"  in line:
                    self.sr_plungef.set(line[line.find("sr_plungef"):].split()[1])
                elif "sr_climb"    in line:
                   self.sr_climb.set(line[line.find("sr_climb"):].split()[1])
                elif "Wrap_Rev_Rot"    in line:
                   self.Wrap_Rev_Rot.set(line[line.find("Wrap_Rev_Rot"):].split()[1])

                elif "DPlaces_L"    in line:
                    self.DPlaces_L.set(line[line.find("DPlaces_L"):].split()[1])
                elif "DPlaces_R"    in line:
                    self.DPlaces_R.set(line[line.find("DPlaces_R"):].split()[1])
                elif "DPlaces_F"    in line:
                    self.DPlaces_F.set(line[line.find("DPlaces_F"):].split()[1])

                elif "probe_feed"    in line:
                    self.probe_feed.set(line[line.find("probe_feed"):].split()[1])
                elif "probe_depth"   in line:
                    self.probe_depth.set(line[line.find("probe_depth"):].split()[1])
                elif "probe_safe"    in line:
                    self.probe_safe.set(line[line.find("probe_safe"):].split()[1])
                elif "probe_nX"    in line:
                    self.probe_nX.set(line[line.find("probe_nX"):].split()[1])
                elif "probe_nY"    in line:
                    self.probe_nY.set(line[line.find("probe_nY"):].split()[1])
                elif "probe_istep"   in line:
                    self.probe_istep.set(line[line.find("probe_istep"):].split()[1])
                elif "probe_offsetX" in line:
                    self.probe_offsetX.set(line[line.find("probe_offsetX"):].split()[1])
                elif "probe_offsetY" in line:
                    self.probe_offsetY.set(line[line.find("probe_offsetY"):].split()[1])
                elif "probe_offsetZ" in line:
                    self.probe_offsetZ.set(line[line.find("probe_offsetZ"):].split()[1])
                elif "probe_precodes"  in line:
                    self.probe_precodes.set(line[line.find("probe_precodes"):].split("\042")[1])
                elif "probe_pcodes"  in line:
                    self.probe_pcodes.set(line[line.find("probe_pcodes"):].split("\042")[1])
                elif "probe_soft"    in line:
                    self.probe_soft.set(line[line.find("probe_soft"):].split()[1])
                elif "probe_points"    in line:
                    self.probe_points.set(line[line.find("probe_points"):].split()[1])

                    
                elif "gpre"    in line:
                    self.gpre.set(line[line.find("gpre"):].split("\042")[1])
                elif "gpost"    in line:
                    self.gpost.set(line[line.find("gpost"):].split("\042")[1])
                elif "NGC_INPUT"    in line:
                    self.NGC_INPUT=(line[line.find("NGC_INPUT"):].split("\042")[1]) 
                elif "NGC_OUTPUT"    in line:
                    self.NGC_OUTPUT=(line[line.find("NGC_OUTPUT"):].split("\042")[1])  
        fin.close()
        
        if self.units.get() == 'in':
            self.funits.set('in/min')
        else:
            self.units.set('mm')
            self.funits.set('mm/min')

        if self.initComplete == 1:
            self.menu_Mode_Change()
            self.NGC_FILE = filename


    ################################################################################
    def File_Read_Probe_data(self, filename):
        try:
            fin = open(filename,'r')
        except:
            fmessage("Unable to open file: %s" %(filename))
            return      
        input_probe_data = []
        line_number = 0
        for line in fin:
            line_number = line_number+1
            if len(line.split("#")) > 1:
                line = line.split("#")[0]
            if len(line.split("//")) > 1:
                line = line.split("//")[0]
                
            delimiter = ""
            if len(line.split(",")) > 1:
                delimiter = ","
            elif len(line.split("\t")) > 1:
                line = line.replace("\t"," ")
                delimiter = " "
            elif len(line.split(" ")) > 1:
                delimiter = " "
            
            try:
                values_text = line.split(delimiter)
                values_text = list(filter(None, values_text)) #remove blank values
            except:
                values_text = ""
                
            if len(values_text) == 0:
                    continue
                
            data_line = []
            if (len(values_text)<3):
                fmessage("Error reading probe data file: %s" %(filename))
                fmessage("Less than three coordinates found for probe point.")
                fmessage("Input line #%d: %s" %(line_number,line))
                continue
                #return
            for cnt in range(0,3):
                try:
                    value_text_mod= re.sub('[ XYZxyzABCabc]', '', values_text[cnt])
                    data_line.append(float(value_text_mod))
                    #data_line.append(float(values_text[cnt]))
                except:
                    fmessage("Error reading probe data file: %s" %(filename))
                    fmessage("Could not convert input text to number value")
                    fmessage("Line #%d: Input text: %s" %(line_number,values_text[cnt]))
                    #print value_text_mod
                    return
            input_probe_data.append(data_line)
        fin.close()

        if input_probe_data == []:
            fmessage("No probe data found in probe data file: %s" %(filename))
            return
        
        len_probe_data = len(input_probe_data)
        probe_data_sortX = sorted(input_probe_data, key=lambda XYZ: XYZ[0])
        probe_data_sortY = sorted(input_probe_data, key=lambda XYZ: XYZ[1])

        #Find minimum and maximum X and Y probe values 
        probe_minx = probe_data_sortX[0][0]
        probe_maxx = probe_data_sortX[len_probe_data-1][0]
        probe_miny = probe_data_sortY[0][1]
        probe_maxy = probe_data_sortY[len_probe_data-1][1]
        

        #Set arbitrary maximum number of divisions to prevent superfine mesh
        Nmax = 100 

        # Find the smallest Step in X direction
        Xsize=probe_maxx-probe_minx
        min_grid_size_x = Xsize/Nmax
        Xcurrent = probe_data_sortX[0][0]
        min_delta_x = probe_maxx-probe_minx
        for XYZ in probe_data_sortX:
            delta = XYZ[0]-Xcurrent
            if delta > min_grid_size_x and delta < min_delta_x:
                min_delta_x = delta
                
        # Find the smallest Step in Y direction
        Ysize=probe_maxy-probe_miny
        min_grid_size_y = Ysize/Nmax
        Ycurrent = probe_data_sortY[0][1]
        min_delta_y = probe_maxy-probe_miny
        for XYZ in probe_data_sortY:
            delta = XYZ[1]-Ycurrent
            if delta > min_grid_size_y and delta < min_delta_y:
                min_delta_y = delta
                
        # Set up a new uniform grid based on the probe points read in from file
        nX = int(1+round(Xsize/min_delta_x))
        nY = int(1+round(Ysize/min_delta_y)) 
        self.probe_nX.set(nX)
        self.probe_nY.set(nY)

        large_dist_squared = Xsize*Xsize + Ysize*Ysize
        self.probe_data = []
        for Y in range(0,nY):
            for X in range(0,nX):
                Xpos = probe_minx + X*min_delta_x
                Ypos = probe_miny + Y*min_delta_y
                
                min_dsquared = large_dist_squared
                for probxy in input_probe_data:
                    dx = Xpos-probxy[0]
                    dy = Ypos-probxy[1]
                    dsquared = dx*dx+dy*dy
                    if dsquared < min_dsquared:
                        min_dsquared = dsquared
                        Zpos = probxy[2]
                self.probe_data.append([Xpos,Ypos,Zpos])
                    
        self.DoIt()
        self.menu_View_Refresh()

        
    ################################################################################
    def WriteGCode(self,side="base",Rstock=0.0,Wrap="XYZ"):
        global Zero
        self.gcode=[]

        if side == "round":
            self.gcode = self.g_rip.generate_round_gcode(
                                              Lmin = float(self.sr_minx.get()),
                                              Lmax = float(self.sr_maxx.get()),
                                              cut_depth = float(self.sr_remove.get()),
                                              tool_dia = float(self.sr_tool_dia.get()),
                                              step_over = float(self.sr_step.get()),
                                              feed = float(self.sr_feed.get()),
                                              plunge_feed=float(self.sr_plungef.get()),
                                              z_safe=float(self.sr_zsafe.get()),
                                              no_variables=bool(self.var_dis.get()),
                                              Rstock=Rstock,
                                              Wrap=Wrap,
                                              preamble=self.gpre.get(),
                                              postamble=self.gpost.get(),
                                              PLACES_L=int(self.DPlaces_L.get()),
                                              PLACES_R=int(self.DPlaces_R.get()),
                                              PLACES_F=int(self.DPlaces_F.get()),                      
                                              climb_mill=bool(self.sr_climb.get()),
                                              Reverse_Rotary = bool(self.Wrap_Rev_Rot.get()),
                                              FSCALE=self.WRAP_FSCALE.get() )
            return
        ####
                    
        elif side == "base":
            data =  self.g_rip.scaled_trans
            gen_rapids = False
        ####
        elif side == "left":
            data = self.g_rip.left_side
            gen_rapids = True
        ####
        elif side == "right":
            data = self.g_rip.right_side
            gen_rapids = True


        if (side == "probe_n_cut") or (side == "probe_only") or (side =="probe_adjusted"):
            gcode1 = []
            gcode2 = []
            #data = self.g_rip.probe_gcode
            if (side == "probe_n_cut") or (side == "probe_only"):
                if (side == "probe_only"):
                    close_file=True
                else:
                    close_file=False
                gcode1 = self.g_rip.generate_probing_gcode(self.g_rip.probe_coords,
                                                      float(self.probe_safe.get()),
                                                      float(self.probe_feed.get()),
                                                      float(self.probe_depth.get()),
                                                      pre_codes=self.probe_precodes.get(),
                                                      pause_codes=self.probe_pcodes.get(),
                                                      probe_offsetX=float(self.probe_offsetX.get()),
                                                      probe_offsetY=float(self.probe_offsetY.get()),
                                                      probe_offsetZ=float(self.probe_offsetZ.get()),
                                                      probe_soft=self.probe_soft.get(),
                                                      close_file = close_file,
                                                      postamble=self.gpost.get(),
                                                      savepts=self.savepts.get(),
                                                      allpoints = self.probe_points.get()=="All")

            if (side == "probe_n_cut") or (side == "probe_adjusted"):
                gcode2 = self.g_rip.generategcode_probe(self.g_rip.probe_gcode,
                                                      z_safe=float(self.ZSAFE.get()),
                                                      plunge_feed=float(self.FEED.get()),
                                                      no_variables=bool(self.var_dis.get()),
                                                      Rstock=Rstock,
                                                      Wrap=Wrap,
                                                      preamble=self.gpre.get(),
                                                      postamble=self.gpost.get(),
                                                      PLACES_L=int(self.DPlaces_L.get()),
                                                      PLACES_R=int(self.DPlaces_R.get()),
                                                      PLACES_F=int(self.DPlaces_F.get()),
                                                      WriteAll=bool(self.WriteAll.get()),
                                                      FSCALE=self.WRAP_FSCALE.get(),
                                                      Reverse_Rotary = bool(self.Wrap_Rev_Rot.get()),
                                                      NoComments=bool(self.NoComments.get()),
                                                      probe_data = self.probe_data,
                                                      probe_offsetZ=float(self.probe_offsetZ.get()),
                                                      probe_safe =float(self.probe_safe.get()) )
            self.gcode = gcode1 + gcode2
            
        else:
            self.gcode = self.g_rip.generategcode(data,
                                                  z_safe=float(self.ZSAFE.get()),
                                                  plunge_feed=float(self.FEED.get()),
                                                  no_variables=bool(self.var_dis.get()),
                                                  Rstock=Rstock,
                                                  Wrap=Wrap,
                                                  preamble=self.gpre.get(),
                                                  postamble=self.gpost.get(),
                                                  gen_rapids = gen_rapids,
                                                  PLACES_L=int(self.DPlaces_L.get()),
                                                  PLACES_R=int(self.DPlaces_R.get()),
                                                  PLACES_F=int(self.DPlaces_F.get()),
                                                  WriteAll=bool(self.WriteAll.get()),
                                                  FSCALE=self.WRAP_FSCALE.get(),
                                                  Reverse_Rotary = bool(self.Wrap_Rev_Rot.get()),
                                                  NoComments=bool(self.NoComments.get()) )

    ################################################################################
    def WriteExportCode(self,filetype="DXF",Rapids=True):
        global Zero
        self.gcode=[]
        
        if filetype == "DXF":
            data =  self.g_rip.scaled_trans
            self.gcode = self.g_rip.generate_dxf_write_gcode(data,Rapids)
        elif filetype == "CSV":
            data =  self.g_rip.scaled_trans
            self.gcode = self.g_rip.generate_csv_write_gcode(data,Rapids)
        return

    

    def menu_CopyClipboard_GCode_Base(self): 
        self.CopyClipboard_GCode("base")
    def menu_CopyClipboard_GCode_Left(self): 
        self.CopyClipboard_GCode("left")
    def menu_CopyClipboard_GCode_Right(self): 
        self.CopyClipboard_GCode("right")
        
    def CopyClipboard_GCode(self,side="base"): 
        self.clipboard_clear()
        if (self.Check_All_Variables() > 0):
            return
        self.WriteGCode(side)
        for line in self.gcode:
            self.clipboard_append(line+'\n')

    def Quit_Click(self, event): 
        self.statusMessage.set("Exiting!")
        root.destroy()

    def ZOOM_ITEMS(self,x0,y0,z_factor):
        all = self.PreviewCanvas.find_all()
        for i in all:
            self.PreviewCanvas.scale(i, x0, y0, z_factor, z_factor)
            w=self.PreviewCanvas.itemcget(i,"width")
            self.PreviewCanvas.itemconfig(i, width=float(w)*z_factor)
        self.PreviewCanvas.update_idletasks()

    def ZOOM(self,z_inc):
        all = self.PreviewCanvas.find_all()
        x = int(self.PreviewCanvas.cget("width" ))/2.0
        y = int(self.PreviewCanvas.cget("height"))/2.0
        for i in all:
            self.PreviewCanvas.scale(i, x, y, z_inc, z_inc)
            w=self.PreviewCanvas.itemcget(i,"width")
            self.PreviewCanvas.itemconfig(i, width=float(w)*z_inc)
        self.PreviewCanvas.update_idletasks()

    def menu_View_Zoom_in(self):
        x = int(self.PreviewCanvas.cget("width" ))/2.0
        y = int(self.PreviewCanvas.cget("height"))/2.0
        self.ZOOM_ITEMS(x, y, 2.0)
        
    def menu_View_Zoom_out(self):
        x = int(self.PreviewCanvas.cget("width" ))/2.0
        y = int(self.PreviewCanvas.cget("height"))/2.0
        self.ZOOM_ITEMS(x, y, 0.5)

    def _mouseZoomIn(self,event):
        self.ZOOM_ITEMS(event.x, event.y, 1.25)
    
    def _mouseZoomOut(self,event):
        self.ZOOM_ITEMS(event.x, event.y, 0.75)

    def mouseZoomStart(self,event):
        self.zoomx0 = event.x
        self.zoomy  = event.y
        self.zoomy0 = event.y

    def mouseZoom(self,event):
        dy = event.y-self.zoomy
        if dy < 0.0:
            self.ZOOM_ITEMS(self.zoomx0, self.zoomy0, 1.15)
        else:
            self.ZOOM_ITEMS(self.zoomx0, self.zoomy0, 0.85)
        self.lasty = self.lasty + dy
        self.zoomy = event.y

    def mousePanStart(self,event):
        self.panx = event.x
        self.pany = event.y

    def mousePan(self,event):
        all = self.PreviewCanvas.find_all()
        dx = event.x-self.panx
        dy = event.y-self.pany
        for i in all:
            self.PreviewCanvas.move(i, dx, dy)
        self.lastx = self.lastx + dx
        self.lasty = self.lasty + dy
        self.panx = event.x
        self.pany = event.y

    def Recalculate_Click(self, event):
        self.DoIt()

    def Settings_ReLoad_Click(self, event, arg1="", arg2=""):
        current_name = event.widget.winfo_parent()
        win_id = event.widget.nametowidget(current_name)
        self.Open_G_Code_File(self.NGC_INPUT,Refresh=True)
        try:
            win_id.withdraw()
            win_id.deiconify()
        except:
            pass
        
    ######################
    def Close_Current_Window_Click(self,event):
        current_name = event.widget.winfo_parent()
        win_id = event.widget.nametowidget(current_name)
        win_id.destroy()

        
    def Stop_Click(self, event):
        global STOP_CALC
        STOP_CALC=1

    # Left Column #
    #############################
    def Entry_GscaleXY_Check(self):
        try:
            value = float(self.SCALEXY.get())
            if  value <= 0.0:
                self.statusMessage.set(" Scale should be greater than 0 ")
                return 2 # Value is invalid number
        except:
            return 3     # Value not a number
        return 0         # Value is a valid number
    def Entry_GscaleXY_Callback(self, varName, index, mode):
        self.entry_set(self.Entry_GscaleXY, self.Entry_GscaleXY_Check() )
    #############################
    def Entry_GscaleZ_Check(self):
        try:
            value = float(self.SCALEZ.get())
            if  value <= 0.0:
                self.statusMessage.set(" Scale should be greater than 0 ")
                return 2 # Value is invalid number
        except:
            return 3     # Value not a number
        return 0         # Value is a valid number
    def Entry_GscaleZ_Callback(self, varName, index, mode):
        self.entry_set(self.Entry_GscaleZ, self.Entry_GscaleZ_Check() )
    #############################
    def Entry_GscaleF_Check(self):
        try:
            value = float(self.SCALEF.get())
            if  value <= 0.0:
                self.statusMessage.set(" Scale should be greater than 0 ")
                return 2 # Value is invalid number
        except:
            return 3     # Value not a number
        return 0         # Value is a valid number
    def Entry_GscaleF_Callback(self, varName, index, mode):
        self.entry_set(self.Entry_GscaleF, self.Entry_GscaleF_Check() )
    #############################           
    def Entry_Rotate_Check(self):
        try:
            value = float(self.ROTATE.get())
            if  value < -360.0 or value > 360.0:
                self.statusMessage.set(" Angle should be between -360 and 360 ")
                return 2 # Value is invalid number
        except:
            return 3     # Value not a number
        return 0         # Value is a valid number
    def Entry_Rotate_Callback(self, varName, index, mode):
        self.entry_set(self.Entry_Rotate, self.Entry_Rotate_Check() )        
    #############################      
    def Entry_SplitA_Check(self):
        try:
            value = float(self.SPLITA.get())
            if  value < -360.0 or value > 360.0:
                self.statusMessage.set(" Angle should be between -360 and 360 ")
                return 2 # Value is invalid number
        except:
            return 3     # Value not a number
        return 0         # Value is a valid number
    def Entry_SplitA_Callback(self, varName, index, mode):
        self.entry_set(self.Entry_SplitA, self.Entry_SplitA_Check() )
    ############################# 
    def Entry_SplitX_Check(self):
        try:
            value = float(self.SPLITX.get())
            #if  value < 0.0:
            #    self.statusMessage.set(" Radius should be greater than or equal to 0 ")
            #    return 2 # Value is invalid number
        except:
            return 3     # Value not a number
        return 0         # Value is a valid number
    def Entry_SplitX_Callback(self, varName, index, mode):
        self.entry_set(self.Entry_SplitX, self.Entry_SplitX_Check() )

    ############################# 
    def Entry_SplitY_Check(self):
        try:
            value = float(self.SPLITY.get())
            #if  value < 0.0:
            #    self.statusMessage.set(" Radius should be greater than or equal to 0 ")
            #    return 2 # Value is invalid number
        except:
            return 3     # Value not a number
        return 0         # Value is a valid number
    def Entry_SplitY_Callback(self, varName, index, mode):
        self.entry_set(self.Entry_SplitY, self.Entry_SplitY_Check() )
    
    # End Left Column #

    # Right Column #
    #############################
    def Entry_Feed_Check(self):
        try:
            value = float(self.FEED.get())
        except:
            return 3     # Value not a number
        return 1         # Value is a valid number changes do not require recalc        
    def Entry_Feed_Callback(self, varName, index, mode):
        self.entry_set(self.Entry_Feed,self.Entry_Feed_Check())
    #############################
    def Entry_Zsafe_Check(self):
        try:
            value = float(self.ZSAFE.get())
        except:
            return 3     # Value not a number
        return 1         # Value is a valid number changes do not require recalc        
    def Entry_Zsafe_Callback(self, varName, index, mode):
        self.entry_set(self.Entry_Zsafe,self.Entry_Zsafe_Check())


    ############################# 
    def Entry_Wrap_DIA_Check(self):
        try:
            value = float(self.WRAP_DIA.get())
            if  value < 0.0:
                self.statusMessage.set(" Radius should be greater than 0 ")
                return 2 # Value is invalid number
        except:
            return 3     # Value not a number
        return 0         # Value is a valid number
    def Entry_Wrap_DIA_Callback(self, varName, index, mode):
        self.entry_set(self.Entry_Wrap_DIA,self.Entry_Wrap_DIA_Check() )

    ############################# 
    def Entry_probe_nX_Check(self):
        try:
            value = float(self.probe_nX.get())
            if  value < 2.0:
                self.statusMessage.set(" Value should be greater than or equal to 2")
                return 2 # Value is invalid number
        except:
            return 3     # Value not a number
        return 0         # Value is a valid number
    def Entry_probe_nX_Callback(self, varName, index, mode):
        self.entry_set(self.Entry_Probe_Num_X,self.Entry_probe_nX_Check() )
        
    ############################# 
    def Entry_probe_nY_Check(self):
        try:
            value = float(self.probe_nY.get())
            if  value < 2.0:
                self.statusMessage.set(" Value should be greater than or equal to 2")
                return 2 # Value is invalid number
        except:
            return 3     # Value not a number
        return 0         # Value is a valid number
    def Entry_probe_nY_Callback(self, varName, index, mode):
        self.entry_set(self.Entry_Probe_Num_Y,self.Entry_probe_nY_Check() )
        
    ############################# 
    def Entry_ProbeDepth_Check(self):
        try:
            value = float(self.probe_depth.get())
            if  value > 0.0:
                self.statusMessage.set(" Value should be less than or equal to 0.0")
                return 2 # Value is invalid number
        except:
            return 3     # Value not a number
        return 0         # Value is a valid number
    def Entry_ProbeDepth_Callback(self, varName, index, mode):
        self.entry_set(self.Entry_ProbeDepth,self.Entry_ProbeDepth_Check() )

    #############################
    def Entry_ProbeOffsetX_Check(self):
        try:
            value = float(self.probe_offsetX.get())
        except:
            return 3     # Value not a number
        return 0         # Value is a valid number changes do not require recalc        
    def Entry_ProbeOffsetX_Callback(self, varName, index, mode):
        self.entry_set(self.Entry_ProbeOffsetX,self.Entry_ProbeOffsetX_Check())
    #############################
    def Entry_ProbeOffsetY_Check(self):
        try:
            value = float(self.probe_offsetY.get())
        except:
            return 3     # Value not a number
        return 0         # Value is a valid number changes do not require recalc        
    def Entry_ProbeOffsetY_Callback(self, varName, index, mode):
        self.entry_set(self.Entry_ProbeOffsetY,self.Entry_ProbeOffsetY_Check())
    #############################
    def Entry_ProbeOffsetZ_Check(self):
        try:
            value = float(self.probe_offsetZ.get())
        except:
            return 3     # Value not a number
        return 0         # Value is a valid number changes do not require recalc        
    def Entry_ProbeOffsetZ_Callback(self, varName, index, mode):
        self.entry_set(self.Entry_ProbeOffsetZ,self.Entry_ProbeOffsetZ_Check())


        
    #############################
    def Entry_ProbeSafe_Check(self):
        try:
            value = float(self.probe_safe.get())
        except:
            return 3     # Value not a number
        return 1         # Value is a valid number changes do not require recalc        
    def Entry_ProbeSafe_Callback(self, varName, index, mode):
        self.entry_set(self.Entry_ProbeSafe,self.Entry_ProbeSafe_Check())
    #############################
    def Entry_ProbeFeed_Check(self):
        try:
            value = float(self.probe_feed.get())
        except:
            return 3     # Value not a number
        return 1         # Value is a valid number changes do not require recalc        
    def Entry_ProbeFeed_Callback(self, varName, index, mode):
        self.entry_set(self.Entry_ProbeFeed,self.Entry_ProbeFeed_Check())
    #############################
    def Entry_ProbeIStep_Check(self):
        try:
            value = float(self.probe_istep.get())
        except:
            return 3     # Value not a number
        return 1         # Value is a valid number changes do not require recalc        
    def Entry_ProbeIStep_Callback(self, varName, index, mode):
        self.entry_set(self.Entry_ProbeInterpSpace,self.Entry_ProbeIStep_Check())
    #############################
 

    #############################
    # End Right Column #
    #############################
    
    ######################################
    #    Settings Window Call Backs      #
    ######################################
    #############################
    def Entry_ArcAngle_Check(self):
        try:
            value = float(self.segarc.get())
        except:
            return 3     # Value not a number
        return 0         # Value is a valid number        
    def Entry_ArcAngle_Callback(self, varName, index, mode):
        self.entry_set(self.Entry_ArcAngle,self.Entry_ArcAngle_Check())
    #############################
    def Entry_Accuracy_Check(self):
        try:
            value = float(self.accuracy.get())
            if  value < 0.0:
                self.statusMessage.set(" Accuracy should be greater than or equal to 0 ")
                return 2 # Value is invalid number
        except:
            return 3     # Value not a number
        return 0         # Value is a valid number        
    def Entry_Accuracy_Callback(self, varName, index, mode):
        self.entry_set(self.Entry_Accuracy,self.Entry_Accuracy_Check())
    #############################
    def Entry_DPlaces_L_Check(self):
        try:
            value = int(self.DPlaces_L.get())
            if  value < 1:
                self.statusMessage.set(" Decimal places should be greater than 0 ")
                return 2 # Value is invalid number
        except:
            return 3     # Value not a number
        return 1         # Value is a valid number        
    def Entry_DPlaces_L_Callback(self, varName, index, mode):
        self.entry_set(self.Entry_DPlaces_L,self.Entry_DPlaces_L_Check())
    #############################
    def Entry_DPlaces_R_Check(self):
        try:
            value = int(self.DPlaces_R.get())
            if  value < 1:
                self.statusMessage.set(" Decimal places should be greater than 0 ")
                return 2 # Value is invalid number
        except:
            return 3     # Value not a number
        return 1         # Value is a valid number        
    def Entry_DPlaces_R_Callback(self, varName, index, mode):
        self.entry_set(self.Entry_DPlaces_R,self.Entry_DPlaces_R_Check())
    #############################
    def Entry_DPlaces_F_Check(self):
        try:
            value = int(self.DPlaces_F.get())
            if  value < 0:
                self.statusMessage.set(" Decimal places should be greater than or equal to 0 ")
                return 2 # Value is invalid number
        except:
            return 3     # Value not a number
        return 1         # Value is a valid number        
    def Entry_DPlaces_F_Callback(self, varName, index, mode):
        self.entry_set(self.Entry_DPlaces_F,self.Entry_DPlaces_F_Check())
    #############################


    ######################################
    #     ROUNDING Window callbacks      #
    ######################################
    #############################
    def Entry_SR_Tool_DIA_Check(self):
        try:
            value = float(self.sr_tool_dia.get())
            if  value <= 0.0:
                self.statusMessage.set(" Tool diameter should be greater than 0 ")
                return 2 # Value is invalid number
        except:
            return 3     # Value not a number
        return 0         # Value is a valid number        
    def Entry_SR_Tool_DIA_Callback(self, varName, index, mode):
        self.entry_set(self.Entry_SR_Tool_DIA,self.Entry_SR_Tool_DIA_Check(),1)

 
    #############################
  
    #############################
    def Entry_SR_Step_Check(self):
        try:
            value = float(self.sr_step.get())
            if  value <= 0.0:
                self.statusMessage.set(" Step over should be greater than 0 ")
                return 2 # Value is invalid number
        except:
            return 3     # Value not a number
        return 0         # Value is a valid number        
    def Entry_SR_Step_Callback(self, varName, index, mode):
        self.entry_set(self.Entry_SR_Step,self.Entry_SR_Step_Check(),1)
    #############################

    #############################
    def Entry_SR_Remove_Check(self):
        try:
            value = float(self.sr_remove.get())
            if  value > 0.0:
                self.statusMessage.set(" Stock to remove should be less than or equal to 0 ")
                return 2 # Value is invalid number
        except:
            return 3     # Value not a number
        return 0         # Value is a valid number        
    def Entry_SR_Remove_Callback(self, varName, index, mode):
        self.entry_set(self.Entry_SR_Remove,self.Entry_SR_Remove_Check(),1)
    #############################

    #############################
    def Entry_SR_MIN_X_Check(self):
        try:
            value = float(self.sr_minx.get())
        except:
            return 3     # Value not a number
        return 0         # Value is a valid number        
    def Entry_SR_MIN_X_Callback(self, varName, index, mode):
        self.entry_set(self.Entry_SR_MIN_X,self.Entry_SR_MIN_X_Check(),1)
    #############################

    #############################
    def Entry_SR_MAX_X_Check(self):
        try:
            value = float(self.sr_maxx.get())
        except:
            return 3     # Value not a number
        return 0         # Value is a valid number        
    def Entry_SR_MAX_X_Callback(self, varName, index, mode):
        self.entry_set(self.Entry_SR_MAX_X,self.Entry_SR_MAX_X_Check(),1)
    #############################

    #############################
    def Entry_SR_ZSafe_Check(self):
        try:
            value = float(self.sr_zsafe.get())
            if  value <= 0.0:
                self.statusMessage.set(" Z safe should be greater than 0 ")
                return 2 # Value is invalid number
        except:
            return 3     # Value not a number
        return 0         # Value is a valid number        
    def Entry_SR_ZSafe_Callback(self, varName, index, mode):
        self.entry_set(self.Entry_SR_ZSafe,self.Entry_SR_ZSafe_Check(),1)
        
    #############################

    #############################
    def Entry_SR_Feed_Check(self):
        try:
            value = float(self.sr_feed.get())
            if  value <= 0.0:
                self.statusMessage.set(" Feed should be greater than 0 ")
                return 2 # Value is invalid number
        except:
            return 3     # Value not a number
        return 0         # Value is a valid number        
    def Entry_SR_Feed_Callback(self, varName, index, mode):
        self.entry_set(self.Entry_SR_Feed,self.Entry_SR_Feed_Check(),1)
    #############################

    #############################
    def Entry_SR_PlungeFeed_Check(self):
        try:
            value = float(self.sr_plungef.get())
            if  value < 0.0:
                self.statusMessage.set(" Plunge feed should be greater than or equal to 0 ")
                return 2 # Value is invalid number
        except:
            return 3     # Value not a number
        return 0         # Value is a valid number        
    def Entry_SR_PlungeFeed_Callback(self, varName, index, mode):
        self.entry_set(self.Entry_SR_PlungeFeed,self.Entry_SR_PlungeFeed_Check(),1)
    #############################



    ##########################################################################
    ##########################################################################
    def Check_All_Variables(self):
        MAIN_error_cnt= \
        self.entry_set(self.Entry_GscaleXY,   self.Entry_GscaleXY_Check()  ,2) +\
        self.entry_set(self.Entry_GscaleZ,    self.Entry_GscaleZ_Check()   ,2) +\
        self.entry_set(self.Entry_GscaleF,    self.Entry_GscaleF_Check()   ,2) +\
        self.entry_set(self.Entry_Rotate,     self.Entry_Rotate_Check()    ,2) +\
        self.entry_set(self.Entry_SplitX,     self.Entry_SplitX_Check()    ,2) +\
        self.entry_set(self.Entry_SplitY,     self.Entry_SplitY_Check()    ,2) +\
        self.entry_set(self.Entry_SplitA,     self.Entry_SplitA_Check()    ,2) +\
        self.entry_set(self.Entry_Feed,       self.Entry_Feed_Check()      ,2) +\
        self.entry_set(self.Entry_Zsafe,      self.Entry_Zsafe_Check()     ,2) +\
        self.entry_set(self.Entry_Wrap_DIA,   self.Entry_Wrap_DIA_Check()  ,2) +\
        self.entry_set(self.Entry_Probe_Num_X,self.Entry_probe_nX_Check()  ,2) +\
        self.entry_set(self.Entry_Probe_Num_Y,self.Entry_probe_nY_Check()  ,2) +\
        self.entry_set(self.Entry_ProbeDepth, self.Entry_ProbeDepth_Check(),2)

        
        GEN_error_cnt= \
        self.entry_set(self.Entry_Accuracy,self.Entry_Accuracy_Check()  ,2) +\
        self.entry_set(self.Entry_ArcAngle,self.Entry_ArcAngle_Check()  ,2) +\
        self.entry_set(self.Entry_DPlaces_L,self.Entry_DPlaces_L_Check(),2) +\
        self.entry_set(self.Entry_DPlaces_R,self.Entry_DPlaces_R_Check(),2) +\
        self.entry_set(self.Entry_DPlaces_F,self.Entry_DPlaces_F_Check(),2)

        try:
            SR_error_cnt= \
            self.entry_set(self.Entry_SR_Tool_DIA,self.Entry_SR_Tool_DIA_Check()     ,2) +\
            self.entry_set(self.Entry_SR_Step,self.Entry_SR_Step_Check()             ,2) +\
            self.entry_set(self.Entry_SR_Remove,self.Entry_SR_Remove_Check()         ,2) +\
            self.entry_set(self.Entry_SR_MIN_X,self.Entry_SR_MIN_X_Check()           ,2) +\
            self.entry_set(self.Entry_SR_MAX_X,self.Entry_SR_MAX_X_Check()           ,2) +\
            self.entry_set(self.Entry_SR_ZSafe,self.Entry_SR_ZSafe_Check()           ,2) +\
            self.entry_set(self.Entry_SR_Feed,self.Entry_SR_Feed_Check()             ,2) +\
            self.entry_set(self.Entry_SR_PlungeFeed,self.Entry_SR_PlungeFeed_Check() ,2)
        except:
            SR_error_cnt = 0
        ERROR_cnt = MAIN_error_cnt + GEN_error_cnt + SR_error_cnt

        if (ERROR_cnt > 0):
            self.statusbar.configure( bg = 'red' )
        if (GEN_error_cnt > 0):
            self.statusMessage.set(\
                " Entry Error Detected: Check Entry Values in General Settings Window ")
        if (MAIN_error_cnt > 0):
            self.statusMessage.set(\
                " Entry Error Detected: Check Entry Values in Main Window ")
        if (SR_error_cnt > 0):
            self.statusMessage.set(\
                " Entry Error Detected: Check Entry Values in Stock Rounding Window (in Wrap mode) ")
            
        return ERROR_cnt        

    ##########################################################################
    ##########################################################################


    def Entry_recalc_var_Callback(self, varName, index, mode):

        self.menu_Mode_Change()

    def Entry_units_var_Callback(self, varName, index, mode):
        if self.units.get() == 'in':
            self.funits.set('in/min')
            self.accuracy.set("0.001")
        else:
            self.funits.set('mm/min')
            self.accuracy.set("0.025")
        self.Open_G_Code_File(self.NGC_INPUT,Refresh=True)
    
    def menu_File_Open_G_Code_File(self):
        init_dir = os.path.dirname(self.NGC_INPUT)
        if ( not os.path.isdir(init_dir) ):
            init_dir = self.HOME_DIR
        fileselect = askopenfilename(filetypes=[("G-code Files","*.ngc"),\
                                                ("All Files","*")],\
                                                 initialdir=init_dir)
        if fileselect != '' and fileselect != ():
            self.Open_G_Code_File(fileselect,Refresh=True)
            self.NGC_INPUT = fileselect
            
    def Open_G_Code_File(self,filename,Refresh=True):
        try:
            self.entry_set(self.Entry_ArcAngle,self.Entry_ArcAngle_Check(),1)
            self.entry_set(self.Entry_Accuracy,self.Entry_Accuracy_Check(),1)
        except:
            pass
        if self.gcode_op.get() == "wrap":
            Arc2Line = True
        elif self.gcode_op.get() == "export":
            Arc2Line = True
        elif self.gcode_op.get() == "probe":
            Arc2Line = True
        else:
            Arc2Line = self.arc2line.get()
        
        fileName, fileExtension = os.path.splitext(filename)
        init_file=os.path.basename(fileName)
        if init_file != "None":
            MSG = self.g_rip.Read_G_Code(filename,
                                         XYarc2line = Arc2Line,
                                         arc_angle=float(self.segarc.get()),
                                         units=self.units.get(),
                                         Accuracy=float(self.accuracy.get()) )
            if MSG != []:
                if error_message(MSG) == "ignore":
                    pass
                else:
                    self.g_rip = G_Code_Rip()
        
        if Refresh==True:
            self.DoIt()
        
    def menu_File_Save_G_Code_Base(self,junk=""):
        self.File_Save_G_Code_File("base")

    def menu_File_Save_G_Code_Left(self,junk=""):
        self.File_Save_G_Code_File("left")
        
    def menu_File_Save_G_Code_Right(self,junk=""):
        self.File_Save_G_Code_File("right")

    def menu_File_Save_G_Code_Wrap(self,junk=""):
        Rstock = float(self.WRAP_DIA.get())/2.0
        Wrap  = self.WRAP_TYPE.get()
        self.File_Save_G_Code_File("base",Rstock=Rstock,Wrap=Wrap)

    def menu_File_Save_Export_Write(self,junk=""):
        Type   = self.EXP_TYPE.get()
        Rapids = self.Exp_Rapids.get()
        self.File_Save_Export_File(Type,Rapids)

    def menu_File_Save_G_Code_round(self,junk=""):
        Rstock = float(self.WRAP_DIA.get())/2.0
        Wrap  = self.WRAP_TYPE.get()
        self.File_Save_G_Code_File("round",Rstock=Rstock,Wrap=Wrap)


    def menu_File_Save_G_Code_Probe_n_Cut(self,junk=""):
        self.File_Save_G_Code_File("probe_n_cut")

    def menu_File_Save_G_Code_Adjusted(self,junk=""):
        self.File_Save_G_Code_File("probe_adjusted")

    def menu_File_Save_G_Code_ProbeOnly(self,junk=""):
        self.File_Save_G_Code_File("probe_only")

    def menu_File_Read_Probe_data(self,junk=""):
        init_dir = os.path.dirname(self.PROBE_INPUT)
        if ( not os.path.isdir(init_dir) ):
            init_dir = os.path.dirname(self.NGC_INPUT)
        if ( not os.path.isdir(init_dir) ):
            init_dir = self.HOME_DIR
        fileselect = askopenfilename(filetypes=[("Probe Data Files","*.txt"),\
                                                ("All Files","*")],\
                                                 initialdir=init_dir)
        if fileselect != '' and fileselect != ():
            self.PROBE_INPUT = fileselect
            self.File_Read_Probe_data(fileselect)


    def menu_Clear_Probe_data(self,junk=""):
        #print "clear probe data"
        self.probe_data = []
        self.DoIt()
        self.menu_View_Refresh()

  
    def File_Save_G_Code_File(self,side="base",Rstock=0.0,Wrap="XYZ"):
        if (self.Check_All_Variables() > 0):
            return
        
        self.WriteGCode(side=side,Rstock=Rstock,Wrap=Wrap)
        
        init_dir = os.path.dirname(self.NGC_OUTPUT)
        if ( not os.path.isdir(init_dir) ):
            init_dir = self.HOME_DIR

        fileName, fileExtension = os.path.splitext(self.NGC_INPUT)
        init_file=os.path.basename(fileName)
        
        if side == "base":
            if Wrap == "XYZ":
                init_file = init_file + "_mod"
            else:
                init_file = init_file + "_wrap"
        elif side == "left":
            init_file = init_file + "_split_black"
        elif side == "right":
            init_file = init_file + "_split_white"
        elif side == "round":
            init_file = init_file + "_round"
        elif side == "export":
            init_file = init_file + "_dxf_write"
        elif side == "probe_n_cut":
            init_file = init_file + "_probe_n_cut"
        elif side == "probe_only":
            init_file = init_file + "_probe_only"
        elif side == "probe_adjusted":
            init_file = init_file + "_adjusted"

    
        filename = asksaveasfilename(defaultextension='.ngc', \
                                     filetypes=[("EMC2 Files","*.ngc"),("All Files","*")],\
                                     initialdir=init_dir,\
                                     initialfile= init_file )

        if filename != '' and filename != ():                         
            try:
                fout = open(filename,'w')
            except:
                self.statusMessage.set("Unable to open file for writing: %s" %(filename))
                self.statusbar.configure( bg = 'red' )
                return
            for line in self.gcode:
                try:
                    fout.write(line+'\n')
                except:
                    fout.write('(skipping line)\n')
            fout.close
            self.NGC_OUTPUT=filename
            self.statusMessage.set("File Saved: %s" %(filename))
            self.statusbar.configure( bg = 'white' )



    def File_Save_Export_File(self,filetype="DXF",Rapids=True ):
        if (self.Check_All_Variables() > 0):
            return
        self.WriteExportCode(filetype,Rapids)
        
        init_dir = os.path.dirname(self.NGC_OUTPUT)
        if ( not os.path.isdir(init_dir) ):
            init_dir = self.HOME_DIR

        fileName, fileExtension = os.path.splitext(self.NGC_INPUT)
        init_file=os.path.basename(fileName)
        
        if filetype == "DXF":
            init_file = init_file
            def_ext = '.dxf'
            fts=[("DXF Files","*.dxf"),("All Files","*")]
        elif filetype == "CSV":
            init_file = init_file
            def_ext = '.csv'
            fts=[("DXF Files","*.csv"),("All Files","*")]
        
        filename = asksaveasfilename(defaultextension=def_ext, \
                                     filetypes=fts,\
                                     initialdir=init_dir,\
                                     initialfile= init_file )

        if filename != '' and filename != ():                         
            try:
                fout = open(filename,'w')
            except:
                self.statusMessage.set("Unable to open file for writing: %s" %(filename))
                self.statusbar.configure( bg = 'red' )
                return
            for line in self.gcode:
                try:
                    fout.write(line+'\n')
                except:
                    fout.write('(skipping line)\n')
            fout.close
            self.NGC_OUTPUT=filename
            self.statusMessage.set("File Saved: %s" %(filename))
            self.statusbar.configure( bg = 'white' )


    def menu_File_Quit(self):
        if message_ask_ok_cancel("Exit", "Exiting G-Code-Ripper...."):
            self.Quit_Click(None)
            
    def menu_View_Refresh_Callback(self, varName, index, mode):
        self.menu_View_Refresh()

    def menu_View_Refresh(self):
        dummy_event = Event()
        dummy_event.widget=self.master
        self.Master_Configure(dummy_event,1)      

    def menu_Mode_Change(self):
        dummy_event = Event()
        dummy_event.widget=self.master
        self.Open_G_Code_File(self.NGC_INPUT,Refresh=True) #V0.8 needed so auto arc2line is active during reading
        self.Master_Configure(dummy_event,1,Plot_Flag=False)
        self.DoIt()

    def menu_View_Recalculate(self):
        self.DoIt()
        
    def menu_Help_About(self):
        about = "G-Code-Ripper V%s by Scorch.\n" %(version)
        about = about + "\163\143\157\162\143\150\100\163\143\157\162"
        about = about + "\143\150\167\157\162\153\163\056\143\157\155\n"
        about = about + "http://www.scorchworks.com/"
        message_box("About G-Code-Ripper",about)


    def menu_Help_About(self):
        application="G-Code Ripper"
        about = "%s Version %s\n\n" %(application,version)
        about = about + "By Scorch.\n"
        about = about + "\163\143\157\162\143\150\100\163\143\157\162"
        about = about + "\143\150\167\157\162\153\163\056\143\157\155\n"
        about = about + "https://www.scorchworks.com/\n\n"
        try:
            python_version = "%d.%d.%d" %(sys.version_info.major,sys.version_info.minor,sys.version_info.micro)
        except:
            python_version = ""
        about = about + "Python "+python_version+" (%d bit)" %(struct.calcsize("P") * 8)
        message_box("About %s" %(application),about)

    def menu_Help_Web(self):
        webbrowser.open_new(r"http://www.scorchworks.com/Gcoderipper/g_code_ripper_doc.html")

    def KEY_F1(self, event):
        self.menu_Help_About()

    def KEY_F2(self, event):
        self.GEN_Settings_Window()

    def KEY_F5(self, event):
        #self.Recalculate_Click()
        self.menu_View_Refresh()

    def KEY_ZOOM_IN(self, event):
        self.menu_View_Zoom_in()

    def KEY_ZOOM_OUT(self, event):
        self.menu_View_Zoom_out()

    def KEY_CTRL_G(self, event):
        self.menu_CopyClipboard_GCode_Base()

        
    def bindConfigure(self, event):
        if not self.initComplete:
            self.initComplete = 1
            self.DoIt()

    def Master_Configure(self, event, update=0, Plot_Flag=True):
        if event.widget != self.master:
            return
        
        x = int(self.master.winfo_x())
        y = int(self.master.winfo_y())
        w = int(self.master.winfo_width())
        h = int(self.master.winfo_height())
        if (self.x, self.y) == (-1,-1):
            self.x, self.y = x,y
        if abs(self.w-w)>10 or abs(self.h-h)>10 or update==1:
            ###################################################
            #  Form changed Size (resized) adjust as required #
            ###################################################
            self.w=w
            self.h=h
            #canvas
            
            # Left Column #
            w_label=90
            w_entry=60
            w_units=35

            x_label_L=10
            x_entry_L=x_label_L+w_label+10        
            x_units_L=x_entry_L+w_entry+5

            Yloc=6
            self.Label_Gcode_Operations.place(x=x_label_L, y=Yloc, width=w_label*2, height=21)
            Yloc=Yloc+24
            self.Label_GscaleXY.place(x=x_label_L, y=Yloc, width=w_label, height=21)
            self.Label_GscaleXY_u.place(x=x_units_L, y=Yloc, width=w_units, height=21)
            self.Entry_GscaleXY.place(x=x_entry_L, y=Yloc, width=w_entry, height=23)

            Yloc=Yloc+24
            self.Label_GscaleZ.place(x=x_label_L, y=Yloc, width=w_label, height=21)
            self.Label_GscaleZ_u.place(x=x_units_L, y=Yloc, width=w_units, height=21)
            self.Entry_GscaleZ.place(x=x_entry_L, y=Yloc, width=w_entry, height=23)
            
            Yloc=Yloc+24
            self.Label_GscaleF.place(x=x_label_L, y=Yloc, width=w_label, height=21)
            self.Label_GscaleF_u.place(x=x_units_L, y=Yloc, width=w_units, height=21)
            self.Entry_GscaleF.place(x=x_entry_L, y=Yloc, width=w_entry, height=23)

            Yloc=Yloc+24
            self.Label_Rotate.place(x=x_label_L,   y=Yloc, width=w_label, height=21)
            self.Label_Rotate_u.place(x=x_units_L, y=Yloc, width=w_units, height=21)
            self.Entry_Rotate.place(x=x_entry_L,   y=Yloc, width=w_entry, height=23)                  

            Yloc=Yloc+24
            self.Label_Origin.place(x=x_label_L, y=Yloc, width=w_label, height=21)
            self.Origin_OptionMenu.place(x=x_entry_L, y=Yloc, width=w_entry+40, height=23)

            Yloc=Yloc+24*2
            self.WriteBaseButton.place(x=12, y=Yloc, width=95*2, height=30)

            #_______________
            Yloc=Yloc+24+12
            self.separator1.place(x=x_label_L, y=Yloc,width=w_label+75+40, height=2)
            Yloc=Yloc+6
            self.Label_view_opt.place(x=x_label_L, y=Yloc, width=w_label, height=21)

            Yloc=Yloc+24
            self.Radio_View_XY.place(x=x_label_L, y=Yloc, width=w_label, height=21)
            self.Radio_View_ISO1.place(x=x_entry_L, y=Yloc, width=w_label, height=21)

            Yloc=Yloc+24
            self.Radio_View_XZ.place(x=x_label_L, y=Yloc, width=w_label, height=21)
            self.Radio_View_ISO2.place(x=x_entry_L, y=Yloc, width=w_label, height=21)

            Yloc=Yloc+24
            self.Radio_View_YZ.place(x=x_label_L, y=Yloc, width=w_label, height=21)
            self.Radio_View_ISO3.place(x=x_entry_L, y=Yloc, width=w_label, height=21)
            
            #_______________
            Yloc=Yloc+24+12
            self.separator2.place(x=x_label_L, y=Yloc,width=w_label+75+40, height=2)
            Yloc=Yloc+6

            self.Label_code_ops.place(x=x_label_L, y=Yloc, width=w_label*2, height=21)
            Yloc=Yloc+24
            self.Radio_Gcode_None.place(x=x_label_L, y=Yloc, width=150, height=23)
            Yloc=Yloc+24
            self.Radio_Gcode_Split.place(x=x_label_L, y=Yloc, width=150, height=23)
            Yloc=Yloc+24
            self.Radio_Gcode_Wrap.place(x=x_label_L, y=Yloc, width=150, height=23)
            Yloc=Yloc+24
            self.Radio_Gcode_Export.place(x=x_label_L, y=Yloc, width=150, height=23)
            Yloc=Yloc+24
            self.Radio_Gcode_Probe.place(x=x_label_L, y=Yloc, width=150, height=23)
           
            # End Left Column #

            # Start Right Column
            w_label=90
            w_entry=60
            w_units=35
            
            x_label_R=self.w - 220
            x_entry_R=x_label_R+w_label+10        
            x_units_R=x_entry_R+w_entry+5

            ## SPLIT ##
            self.Label_Gcode_Split_Properties.place_forget()
            self.Label_SplitX.place_forget()
            self.Label_SplitX_u.place_forget()
            self.Entry_SplitX.place_forget()
            self.Label_SplitY.place_forget()
            self.Label_SplitY_u.place_forget()
            self.Entry_SplitY.place_forget()
            self.Label_SplitA.place_forget()
            self.Label_SplitA_u.place_forget()
            self.Entry_SplitA.place_forget()
            self.Label_rotateb.place_forget()
            self.Checkbutton_rotateb.place_forget()
            self.WriteLeftButton.place_forget()
            self.WriteRightButton.place_forget()

            self.separator3.place_forget()
            
            self.Label_gcode_opt.place_forget()
            self.Entry_Feed.place_forget()
            self.Label_Feed.place_forget()
            self.Label_Feed_u.place_forget()
            self.Entry_Zsafe.place_forget()
            self.Label_Zsafe.place_forget()
            self.Label_Zsafe_u.place_forget()
            self.separator4.place_forget()

            ## WRAP ##
            self.Label_Gcode_Wrap_Properties.place_forget()
            self.Label_Wrap_DIA.place_forget()
            self.Entry_Wrap_DIA.place_forget()
            self.Label_Wrap_DIA_u.place_forget()
            self.Label_Radio_Wrap.place_forget()
            self.Radio_Wrap_Y2A.place_forget()
            self.Radio_Wrap_X2B.place_forget()
            self.Radio_Wrap_Y2B.place_forget()
            self.Radio_Wrap_X2A.place_forget()
            self.WriteWrapButton.place_forget()
            self.WriteRoundButton.place_forget()
            self.Label_WRAP_FSCALE.place_forget()
            self.WRAP_FSCALE_OptionMenu.place_forget()
            self.Label_WRAP_REV_ROT.place_forget()
            self.Checkbutton_WRAP_REV_ROT.place_forget()

            ## Export ##
            self.Label_Gcode_Export_Properties.place_forget()
            self.Label_Radio_Export.place_forget()
            self.Radio_Export_DXF.place_forget()
            self.Radio_Export_CSV.place_forget()
            self.Label_EXP_RAPIDS.place_forget()
            self.Checkbutton_EXP_RAPIDS.place_forget()
            self.WriteExportButton.place_forget()


            ## PROBE ##
            self.Label_Gcode_Probe_Properties.place_forget()
            self.Label_ProbeOffsetX.place_forget()
            self.Label_ProbeOffsetX_u.place_forget()
            self.Entry_ProbeOffsetX.place_forget()
            self.Label_ProbeOffsetY.place_forget()
            self.Label_ProbeOffsetY_u.place_forget()
            self.Entry_ProbeOffsetY.place_forget()
            self.Label_ProbeOffsetZ.place_forget()
            self.Label_ProbeOffsetZ_u.place_forget()
            self.Entry_ProbeOffsetZ.place_forget()
            self.Label_ProbeSafe.place_forget()
            self.Label_ProbeSafe_u.place_forget()
            self.Entry_ProbeSafe.place_forget()
            self.Label_ProbeDepth.place_forget()
            self.Label_ProbeDepth_u.place_forget()
            self.Entry_ProbeDepth.place_forget()
            self.Label_ProbeFeed.place_forget()
            self.Label_ProbeFeed_u.place_forget()
            self.Entry_ProbeFeed.place_forget()
            self.Label_Probe_Num_X.place_forget()
            self.Entry_Probe_Num_X.place_forget()
            self.Label_Probe_Num_Y.place_forget()
            self.Entry_Probe_Num_Y.place_forget()
            #self.Label_ProbeInterpSpace.place_forget()
            #self.Entry_ProbeInterpSpace.place_forget()
            self.Label_ProbePreCodes.place_forget()
            self.Entry_ProbePreCodes.place_forget()
            self.Label_ProbePauseCodes.place_forget()
            self.Entry_ProbePauseCodes.place_forget()
            self.Label_ProbeSoft.place_forget()
            self.ProbeSoft_OptionMenu.place_forget()
            self.Label_ProbePoints.place_forget()
            self.ProbePoints_OptionMenu.place_forget()
            self.Label_SavePoints.place_forget()
            self.Checkbutton_SavePoints.place_forget()
        
            self.WriteProbeOnlyButton.place_forget()
            self.ReadProbeButton.place_forget()
            self.ClearProbeButton.place_forget()
            self.WriteAdjustedButton.place_forget()
            self.WriteProbeButton.place_forget()
            

            # Buttons/Canvas.
            Ybut=self.h-60
            self.Recalculate.place(x=12, y=Ybut, width=95, height=30)
            self.PreviewCanvas.configure( width = self.w-455, height = self.h-50 )
            self.PreviewCanvas_frame.place(x=220, y=10)

            # Menu Items
            #self.top_File.entryconfig(2,state="disabled")
            #self.top_File.entryconfig(3,state="disabled")
            #self.top_Edit.entryconfig(1,state="disabled")
            #self.top_Edit.entryconfig(2,state="disabled")


            #### MODE SPECIFIC ITEMS ####
            Yloc=6
            if self.gcode_op.get() == "split":
                #self.top_File.entryconfig(2,state="normal")
                #self.top_File.entryconfig(3,state="normal")
                #self.top_Edit.entryconfig(1,state="normal")
                #self.top_Edit.entryconfig(2,state="normal")
                
                ###########
                ###########
                self.Label_Gcode_Split_Properties.place(x=x_label_R, y=Yloc, width=w_label*2, height=21)
                Yloc=Yloc+24
                
                self.Label_SplitX.place(x=x_label_R, y=Yloc, width=w_label, height=21)
                self.Label_SplitX_u.place(x=x_units_R, y=Yloc, width=w_units, height=21)
                self.Entry_SplitX.place(x=x_entry_R, y=Yloc, width=w_entry, height=23)

                Yloc=Yloc+24
                self.Label_SplitY.place(x=x_label_R, y=Yloc, width=w_label, height=21)
                self.Label_SplitY_u.place(x=x_units_R, y=Yloc, width=w_units, height=21)
                self.Entry_SplitY.place(x=x_entry_R, y=Yloc, width=w_entry, height=23)

                Yloc=Yloc+24
                self.Label_SplitA.place(x=x_label_R, y=Yloc, width=w_label, height=21)
                self.Label_SplitA_u.place(x=x_units_R, y=Yloc, width=w_units, height=21)
                self.Entry_SplitA.place(x=x_entry_R, y=Yloc, width=w_entry, height=23)

                Yloc=Yloc+24
                self.Label_rotateb.place(x=x_label_R, y=Yloc, width=w_label, height=21)
                self.Checkbutton_rotateb.place(x=x_entry_R, y=Yloc, width=w_entry+40, height=23)

                Yloc=Yloc+24*2
                self.WriteLeftButton.place(x=x_label_R, y=Yloc, width=95*2, height=30)

                Yloc=Yloc+24+7
                self.WriteRightButton.place(x=x_label_R, y=Yloc, width=95*2, height=30)
                
                Yloc=Yloc+24+12
                self.separator3.place(x=x_label_R, y=Yloc,width=w_label+75+40, height=2)
                ###########
                ###########
                
                Yloc=Yloc+6
                self.Label_gcode_opt.place(x=x_label_R, y=Yloc, width=w_label*2, height=21)
                
                Yloc=Yloc+24
                self.Entry_Feed.place(  x=x_entry_R, y=Yloc, width=w_entry, height=23)
                self.Label_Feed.place(  x=x_label_R, y=Yloc, width=w_label, height=21)
                self.Label_Feed_u.place(x=x_units_R, y=Yloc, width=w_units+15, height=21)
                
                Yloc=Yloc+24
                self.Entry_Zsafe.place(  x=x_entry_R, y=Yloc, width=w_entry, height=23)
                self.Label_Zsafe.place(  x=x_label_R, y=Yloc, width=w_label, height=21)
                self.Label_Zsafe_u.place(x=x_units_R, y=Yloc, width=w_units, height=21)

                Yloc=Yloc+24+12
                self.separator4.place(x=x_label_R, y=Yloc,width=w_label+75+40, height=2)
                Yloc=Yloc+6


            elif self.gcode_op.get() == "wrap":
                self.Label_Gcode_Wrap_Properties.place(  x=x_label_R, y=Yloc, width=w_label*2, height=21)
                Yloc=Yloc+24
                self.Label_Wrap_DIA.place(  x=x_label_R, y=Yloc, width=w_label, height=21)
                self.Label_Wrap_DIA_u.place(x=x_units_R, y=Yloc, width=w_units+15, height=21)
                self.Entry_Wrap_DIA.place(x=x_entry_R, y=Yloc, width=w_entry, height=23)
                
                Yloc=Yloc+24
                self.Label_Radio_Wrap.place(  x=x_label_R, y=Yloc, width=w_label*2, height=21)
                Yloc=Yloc+24
                self.Radio_Wrap_Y2A.place(  x=x_label_R, y=Yloc, width=w_label*2, height=21)
                Yloc=Yloc+24
                self.Radio_Wrap_X2B.place(  x=x_label_R, y=Yloc, width=w_label*2, height=21)
                Yloc=Yloc+24
                self.Radio_Wrap_Y2B.place(  x=x_label_R, y=Yloc, width=w_label*2, height=21)
                Yloc=Yloc+24
                self.Radio_Wrap_X2A.place(  x=x_label_R, y=Yloc, width=w_label*2, height=21)
                Yloc=Yloc+24
                self.Label_WRAP_FSCALE.place(x=x_label_R, y=Yloc, width=w_label, height=21)
                self.WRAP_FSCALE_OptionMenu.place(x=x_entry_R-20, y=Yloc, width=w_entry+50, height=23)

                Yloc=Yloc+24
                self.Label_WRAP_REV_ROT.place(x=x_label_R, y=Yloc, width=w_label+30, height=21)
                self.Checkbutton_WRAP_REV_ROT.place(x=x_entry_R+30, y=Yloc, width=75, height=23)
                
                Yloc=Yloc+24+7
                self.WriteWrapButton.place(x=x_label_R, y=Yloc, width=95*2, height=30)

                Yloc=Yloc+24+7
                self.WriteRoundButton.place(x=x_label_R, y=Yloc, width=95*2, height=30)

            elif self.gcode_op.get() == "export":
                self.Label_Gcode_Export_Properties.place(x=x_label_R, y=Yloc, width=w_label*2, height=21)

                Yloc=Yloc+24
                self.Label_EXP_RAPIDS.place(x=x_label_R, y=Yloc, width=w_label+30, height=21)
                self.Checkbutton_EXP_RAPIDS.place(x=x_entry_R+30, y=Yloc, width=75, height=23)
                
                Yloc=Yloc+24
                self.Label_Radio_Export.place(  x=x_label_R, y=Yloc, width=w_label*2, height=21)
                Yloc=Yloc+24
                self.Radio_Export_DXF.place(  x=x_label_R, y=Yloc, width=w_label*2, height=21)
                Yloc=Yloc+24
                self.Radio_Export_CSV.place(  x=x_label_R, y=Yloc, width=w_label*2, height=21)
                
                Yloc=Yloc+24+7
                self.WriteExportButton.place(x=x_label_R, y=Yloc, width=95*2, height=30)


            elif self.gcode_op.get() == "probe":
                self.Label_Gcode_Probe_Properties.place(x=x_label_R, y=Yloc, width=w_label*2, height=21)

                Yloc=Yloc+24
                self.Label_ProbeOffsetX.place(x=x_label_R, y=Yloc, width=w_label, height=21)
                self.Label_ProbeOffsetX_u.place(x=x_units_R, y=Yloc, width=w_units, height=21)
                self.Entry_ProbeOffsetX.place(x=x_entry_R, y=Yloc, width=w_entry, height=23)

                Yloc=Yloc+24
                self.Label_ProbeOffsetY.place(x=x_label_R, y=Yloc, width=w_label, height=21)
                self.Label_ProbeOffsetY_u.place(x=x_units_R, y=Yloc, width=w_units, height=21)
                self.Entry_ProbeOffsetY.place(x=x_entry_R, y=Yloc, width=w_entry, height=23)

                Yloc=Yloc+24
                self.Label_ProbeOffsetZ.place(x=x_label_R, y=Yloc, width=w_label, height=21)
                self.Label_ProbeOffsetZ_u.place(x=x_units_R, y=Yloc, width=w_units, height=21)
                self.Entry_ProbeOffsetZ.place(x=x_entry_R, y=Yloc, width=w_entry, height=23)
 
                Yloc=Yloc+24
                self.Label_ProbeSafe.place(x=x_label_R, y=Yloc, width=w_label, height=21)
                self.Label_ProbeSafe_u.place(x=x_units_R, y=Yloc, width=w_units, height=21)
                self.Entry_ProbeSafe.place(x=x_entry_R, y=Yloc, width=w_entry, height=23)

                Yloc=Yloc+24
                self.Label_ProbeDepth.place(x=x_label_R, y=Yloc, width=w_label, height=21)
                self.Label_ProbeDepth_u.place(x=x_units_R, y=Yloc, width=w_units, height=21)
                self.Entry_ProbeDepth.place(x=x_entry_R, y=Yloc, width=w_entry, height=23)

                Yloc=Yloc+24
                self.Label_ProbeFeed.place(x=x_label_R, y=Yloc, width=w_label, height=21)
                self.Label_ProbeFeed_u.place(x=x_units_R, y=Yloc, width=w_units+15, height=21)
                self.Entry_ProbeFeed.place(x=x_entry_R, y=Yloc, width=w_entry, height=23)

                Yloc=Yloc+24
                self.Label_Probe_Num_X.place(x=x_label_R, y=Yloc, width=w_label, height=21)
                self.Entry_Probe_Num_X.place(x=x_entry_R, y=Yloc, width=w_entry, height=23)

                Yloc=Yloc+24
                self.Label_Probe_Num_Y.place(x=x_label_R, y=Yloc, width=w_label, height=21)
                self.Entry_Probe_Num_Y.place(x=x_entry_R, y=Yloc, width=w_entry, height=23)

                #Yloc=Yloc+24
                #self.Label_ProbeInterpSpace.place(x=x_label_R, y=Yloc, width=w_label, height=21)
                #self.Entry_ProbeInterpSpace.place(x=x_entry_R, y=Yloc, width=w_entry, height=23)

                Yloc=Yloc+24
                self.Label_ProbePreCodes.place(x=x_label_R, y=Yloc, width=w_label, height=21)
                self.Entry_ProbePreCodes.place(x=x_entry_R, y=Yloc, width=w_entry+50, height=23)
                
                Yloc=Yloc+24
                self.Label_ProbePauseCodes.place(x=x_label_R, y=Yloc, width=w_label, height=21)
                self.Entry_ProbePauseCodes.place(x=x_entry_R, y=Yloc, width=w_entry+50, height=23)

                Yloc=Yloc+24
                self.Label_ProbePoints.place(x=x_label_R, y=Yloc, width=w_label, height=21)
                self.ProbePoints_OptionMenu.place(x=x_entry_R, y=Yloc, width=w_entry+50, height=23)

                Yloc=Yloc+24
                self.Label_SavePoints.place(x=x_label_R, y=Yloc, width=w_label, height=21)
                self.Checkbutton_SavePoints.place(x=x_entry_R, y=Yloc, width=w_entry+50, height=23)
                
                Yloc=Yloc+24
                self.Label_ProbeSoft.place(x=x_label_R, y=Yloc, width=w_label, height=21)
                self.ProbeSoft_OptionMenu.place(x=x_entry_R, y=Yloc, width=w_entry+50, height=23)

                Yloc=Yloc+24+7
                self.ReadProbeButton.place(x=x_label_R, y=Yloc, width=95*2, height=30)
                Yloc=Yloc+24+7
                self.ClearProbeButton.place(x=x_label_R, y=Yloc, width=95*2, height=30)
                Yloc=Yloc+24+7
                self.WriteAdjustedButton.place(x=x_label_R, y=Yloc, width=95*2, height=30)
                
                Yloc=Yloc+24+7
                self.WriteProbeOnlyButton.place(x=x_label_R, y=Yloc, width=95*2, height=30)
                Yloc=Yloc+24+7
                self.WriteProbeButton.place(x=x_label_R, y=Yloc, width=95*2, height=30)



                if (self.probe_data == []):
                    self.Label_ProbeSafe.configure(state="normal")
                    self.Label_ProbeSafe_u.configure(state="normal")
                    self.Entry_ProbeSafe.configure(state="normal")
                    self.Label_ProbeDepth.configure(state="normal")
                    self.Label_ProbeDepth_u.configure(state="normal")
                    self.Entry_ProbeDepth.configure(state="normal")
                    self.Label_ProbeFeed.configure(state="normal")
                    self.Label_ProbeFeed_u.configure(state="normal")
                    self.Entry_ProbeFeed.configure(state="normal")
                    self.Label_Probe_Num_X.configure(state="normal")
                    self.Entry_Probe_Num_X.configure(state="normal")
                    self.Label_Probe_Num_Y.configure(state="normal")
                    self.Entry_Probe_Num_Y.configure(state="normal")
                    self.Label_ProbePreCodes.configure(state="normal")
                    self.Entry_ProbePreCodes.configure(state="normal")
                    self.Label_ProbePauseCodes.configure(state="normal")
                    self.Label_ProbePauseCodes_u.configure(state="normal")
                    self.Entry_ProbePauseCodes.configure(state="normal")
                    self.Label_SavePoints.configure(state="normal")
                    self.Checkbutton_SavePoints.configure(state="normal")
                    self.Label_ProbePoints.configure(state="normal")
                    self.ProbePoints_OptionMenu.configure(state="normal")

                    self.WriteProbeOnlyButton.configure(state="normal")
                    self.WriteProbeButton.configure(state="normal")
                    self.WriteAdjustedButton.configure(state="disabled")
                    self.ClearProbeButton.configure(state="disabled")
                    
                else:
                    self.Label_ProbeSafe.configure(state="disabled")
                    self.Label_ProbeSafe_u.configure(state="disabled")
                    self.Entry_ProbeSafe.configure(state="disabled")
                    self.Label_ProbeDepth.configure(state="disabled")
                    self.Label_ProbeDepth_u.configure(state="disabled")
                    self.Entry_ProbeDepth.configure(state="disabled")
                    self.Label_ProbeFeed.configure(state="disabled")
                    self.Label_ProbeFeed_u.configure(state="disabled")
                    self.Entry_ProbeFeed.configure(state="disabled")
                    self.Label_Probe_Num_X.configure(state="disabled")
                    self.Entry_Probe_Num_X.configure(state="disabled")
                    self.Label_Probe_Num_Y.configure(state="disabled")
                    self.Entry_Probe_Num_Y.configure(state="disabled")
                    self.Label_ProbePreCodes.configure(state="disabled")
                    self.Entry_ProbePreCodes.configure(state="disabled")
                    self.Label_ProbePauseCodes.configure(state="disabled")
                    self.Label_ProbePauseCodes_u.configure(state="disabled")
                    self.Entry_ProbePauseCodes.configure(state="disabled")
                    self.Label_SavePoints.configure(state="disabled")
                    self.Checkbutton_SavePoints.configure(state="disabled")
                    self.Label_ProbePoints.configure(state="disabled")
                    self.ProbePoints_OptionMenu.configure(state="disabled")

                    self.WriteProbeOnlyButton.configure(state="disabled")
                    self.WriteProbeButton.configure(state="disabled")
                    self.WriteAdjustedButton.configure(state="normal")
                    self.ClearProbeButton.configure(state="normal")
                    self.Label_ProbeSafe.configure(state="normal")
            ###########################################################
            if Plot_Flag:
                self.Plot_Data()

    def ISO_COORDS(self,X,Y,Z,VIEW="XY"):

        if VIEW == "XY":
            return X,Y

        elif VIEW == "XZ":
            return X,Z

        elif VIEW == "YZ":
            return Y,Z
 
        elif VIEW == "ISO1":
            s60 = 0.866025404 #sin(radians(60))
            c60 = 0.5         #cos(radians(60))
            Xout =  X*s60 + Y*s60
            Yout = -X*c60 + Y*c60 + Z
            return Xout,Yout

        elif VIEW == "ISO2":
            s60 = 0.866025404 #sin(radians(60))
            c60 = 0.5         #cos(radians(60))
            Xout =  X*s60 - Y*s60
            Yout =  X*c60 + Y*c60 + Z
            return Xout,Yout

        elif VIEW == "ISO3":
            s60 = 0.866025404 #sin(radians(60))
            c60 = 0.5         #cos(radians(60))
            Xout = -X*s60 - Y*s60
            Yout =  X*c60 - Y*c60 + Z
            return Xout,Yout


    def Plot_Line_ISO(self,XX1,YY1,ZZ1,XX2,YY2,ZZ2,midx,midy,cszw,cszh,PlotScale,col,radius=0,dash=False):
        
        #if (isinstance(XX1, complex)): XX1=0 #V0.8
        #if (isinstance(YY1, complex)): YY1=0 #V0.8
        #if (isinstance(ZZ1, complex)): ZZ1=0 #V0.8
        #if (isinstance(XX2, complex)): XX2=0 #V0.8
        #if (isinstance(YY2, complex)): YY2=0 #V0.8
        #if (isinstance(ZZ2, complex)): ZZ2=0 #V0.8
        
        #if (isinstance(XX1, complex)): XX1=XX2 #V0.8
        #if (isinstance(YY1, complex)): YY1=YY2 #V0.8
        #if (isinstance(ZZ1, complex)): ZZ1=ZZ2 #V0.8
        
        VIEW = self.plot_view.get()
        XX1,YY1 = self.ISO_COORDS(XX1,YY1,ZZ1,VIEW=VIEW)
        XX2,YY2 = self.ISO_COORDS(XX2,YY2,ZZ2,VIEW=VIEW)

        #print midx, midy, XX1, XX2, YY1, YY2
        x1 =  cszw/2 + (XX1-midx) / PlotScale
        x2 =  cszw/2 + (XX2-midx) / PlotScale
        y1 =  cszh/2 - (YY1-midy) / PlotScale
        y2 =  cszh/2 - (YY2-midy) / PlotScale
        if radius==0:
            thick=0
        else:
            thick  =  radius*2 / PlotScale
        if not dash:
            self.segID.append( self.PreviewCanvas.create_line(x1,y1,x2,y2,fill = col, capstyle="round", width=thick, arrow="none"))
        else:
            self.segID.append( self.PreviewCanvas.create_line(x1,y1,x2,y2,fill = col, capstyle="round", width=thick, dash=(4,4)))
        
    ###############################################################################################
    def Plot_Arc(self,dir,XX1,YY1,ZZ1, XX2,YY2,ZZ2, xxc,yyc,midx,midy,cszw,cszh,PlotScale,col,radius=0):
        global Zero
        if radius==0:
            thick=0
        else:
            thick  =  radius*2 / PlotScale

        if dir==3:
            xa = XX1
            ya = YY1
            za = ZZ1 #
            xb = XX2
            yb = YY2
            zb = ZZ2 #
        else:
            xa = XX2
            ya = YY2
            za = ZZ2 #
            xb = XX1 
            yb = YY1
            zb = ZZ1 #
        
        Lx1 = xa-xxc
        Ly1 = ya-yyc
        L1  = sqrt( Lx1**2 + Ly1**2 )
        s1  = Ly1/L1
        c1  = Lx1/L1
        A1  = Get_Angle(s1,c1)

        Lx2 = xb-xxc
        Ly2 = yb-yyc
        L2  = sqrt( Lx2**2 + Ly2**2 )
        Tx2,Ty2 = Transform(Lx2,Ly2,radians(-A1))

        s2  = Ty2/L2
        c2  = Tx2/L2
        A2  = Get_Angle(s2,c2)
        
        start_angle  = A1
        extent_angle = A2

        if abs(extent_angle) < Zero:
            A2=360.0
            extent_angle=360.0
        ##########################################
        ### Plot arcs in canvas AS LINES       ###
        ##########################################
        data = self.g_rip.arc2lines([XX1,YY1,ZZ1], [XX2,YY2,ZZ2], [xxc,yyc], dir, plane="17")
        for line in data:
            XY=line
            self.Plot_Line_ISO(XY[0],XY[1],XY[2],XY[3],XY[4],XY[5],midx,midy,cszw,cszh,PlotScale,col,radius=0,dash=False)

##        step=10
##        at=step
##        my_range=[]
##        while at < A2:
##            my_range.append(at)
##            at = at+step
##        my_range.append(A2)
##
##        XXa = xa
##        YYa = ya
##        ZZa = za #ZZ1
##        #ZZb = zb #ZZ2
##        dZ  = zb-za #ZZ2-ZZ1
##        for at in my_range:
##            print A2," ",at, at/A2
##            ZZb = ZZa + at/A2*dZ 
##            xt,yt = Transform(L1,0,radians(A1+at))
##            XXb = xxc + xt
##            YYb = yyc + yt
##
##            self.Plot_Line_ISO(XXa,YYa,ZZa,XXb,YYb,ZZb,midx,midy,cszw,cszh,PlotScale,col,radius=0,dash=False)
##
##            XXa=XXb
##            YYa=YYb
##            ZZa=ZZb

                
    ###############################################################################################
    def Plot_Circ(self,XX1,YY1,midx,midy,cszw,cszh,PlotScale,color,Rad,fill):
        dd=Rad
        ZZ1 = 0
        VIEW = self.plot_view.get()
        XXX1,YYY1 = self.ISO_COORDS(XX1,YY1,ZZ1,VIEW=VIEW)
        
        x1 =  cszw/2 + (XXX1-dd-midx) / PlotScale
        x2 =  cszw/2 + (XXX1+dd-midx) / PlotScale
        y1 =  cszh/2 - (YYY1-dd-midy) / PlotScale
        y2 =  cszh/2 - (YYY1+dd-midy) / PlotScale
        if fill ==0:
            self.segID.append( self.PreviewCanvas.create_oval(x1,y1,x2,y2, outline=color, fill=None, width=1 ))
        else:
            self.segID.append( self.PreviewCanvas.create_oval(x1,y1,x2,y2, outline=color, fill=color, width=0 ))

    def Recalculate_RQD_Nocalc(self, event):
        self.statusbar.configure( bg = 'yellow' )
        self.statusMessage.set(" Recalculation required.")

    def Recalculate_RQD_Click(self, event):
        self.statusbar.configure( bg = 'yellow' )
        self.statusMessage.set(" Recalculation required.")
        self.DoIt()

    def Recalc_RQD(self):
        self.statusbar.configure( bg = 'yellow' )
        self.statusMessage.set(" Recalculation required.")

    ##########################################
    #        CANVAS PLOTTING STUFF           #
    ##########################################
    def Plot_Data(self):
        self.master.update_idletasks()
        # erase old segs/display objects
        self.PreviewCanvas.delete(ALL)
        self.segID = []

        cszw = int(self.PreviewCanvas.cget("width"))
        cszh = int(self.PreviewCanvas.cget("height"))
        buff=10

        maxx = self.MAXX
        minx = self.MINX
        maxy = self.MAXY
        miny = self.MINY
        maxz = self.MAXZ
        minz = self.MINZ

        VIEW = self.plot_view.get()

        if VIEW == "XY":
            maxH = maxx
            minH = minx
            maxV = maxy
            minV = miny

        elif VIEW == "XZ":
            maxH = maxx
            minH = minx
            maxV = maxz
            minV = minz

        elif VIEW == "YZ":
            maxH = maxy
            minH = miny
            maxV = maxz
            minV = minz
        
        elif VIEW == "ISO1":
            maxH, junk = self.ISO_COORDS(maxx,maxy,0.0 ,VIEW=VIEW)
            minH, junk = self.ISO_COORDS(minx,miny,0.0 ,VIEW=VIEW)
            junk, maxV = self.ISO_COORDS(minx,maxy,maxz,VIEW=VIEW)
            junk, minV = self.ISO_COORDS(maxx,miny,minz,VIEW=VIEW)

        elif VIEW == "ISO2":
            maxH, junk = self.ISO_COORDS(maxx,miny,0.0 ,VIEW=VIEW)
            minH, junk = self.ISO_COORDS(minx,maxy,0.0 ,VIEW=VIEW)
            junk, maxV = self.ISO_COORDS(maxx,maxy,maxz,VIEW=VIEW)
            junk, minV = self.ISO_COORDS(minx,miny,minz,VIEW=VIEW)

        elif VIEW == "ISO3":
            maxH, junk = self.ISO_COORDS(minx,miny,0.0 ,VIEW=VIEW)
            minH, junk = self.ISO_COORDS(maxx,maxy,0.0 ,VIEW=VIEW)
            junk, maxV = self.ISO_COORDS(maxx,miny,maxz,VIEW=VIEW)
            junk, minV = self.ISO_COORDS(minx,maxy,minz,VIEW=VIEW)

        PlotScale = max((maxH-minH)/(cszw-buff), (maxV-minV)/(cszh-buff))
        midH = (maxH+minH)/2
        midV = (maxV+minV)/2

        if PlotScale <= 0:
            PlotScale=1.0

        Wrap = self.WRAP_TYPE.get()

        #if ( self.show_box.get() == True ) and ( VIEW == "XY" ) and ( (self.gcode_op.get()=="none") or (self.gcode_op.get()=="split") ):
        if ( self.show_box.get() == True ) and ( VIEW == "XY" ) and (self.gcode_op.get()!="wrap"):
            x_lft = cszw/2 + (minH-midH) / PlotScale
            x_rgt = cszw/2 + (maxH-midH) / PlotScale
            y_bot = cszh/2 + (maxV-midV) / PlotScale
            y_top = cszh/2 + (minV-midV) / PlotScale
            self.segID.append( self.PreviewCanvas.create_rectangle(
                    x_lft, y_bot, x_rgt, y_top, fill="gray80", outline="gray80", width = 0) )

##        if ( self.show_box.get() == True ):
##            color="gray80"
##            rad = 3.0*(PlotScale/2.0) #this makes the line thickness two pixels
##            self.Plot_Line_ISO(  minx,maxy,0.0,maxx,maxy,0.0, midH,midV,cszw,cszh,PlotScale,color,radius=rad)
##            self.Plot_Line_ISO(  maxx,maxy,0.0,maxx,miny,0.0, midH,midV,cszw,cszh,PlotScale,color,radius=rad)
##            self.Plot_Line_ISO(  maxx,miny,0.0,minx,miny,0.0, midH,midV,cszw,cszh,PlotScale,color,radius=rad)
##            self.Plot_Line_ISO(  minx,miny,0.0,minx,maxy,0.0, midH,midV,cszw,cszh,PlotScale,color,radius=rad)
            
##            color="gray80"
##            #TOP BOX
##            self.Plot_Line_ISO(  minx,maxy,minz,maxx,maxy,minz, midH,midV,cszw,cszh,PlotScale,color,radius=rad)
##            self.Plot_Line_ISO(  maxx,maxy,minz,maxx,miny,minz, midH,midV,cszw,cszh,PlotScale,color,radius=rad)
##            self.Plot_Line_ISO(  maxx,miny,minz,minx,miny,minz, midH,midV,cszw,cszh,PlotScale,color,radius=rad)
##            self.Plot_Line_ISO(  minx,miny,minz,minx,maxy,minz, midH,midV,cszw,cszh,PlotScale,color,radius=rad)
##            #BOTTOM BOX
##            self.Plot_Line_ISO(  minx,maxy,maxz,maxx,maxy,maxz, midH,midV,cszw,cszh,PlotScale,color,radius=rad)
##            self.Plot_Line_ISO(  maxx,maxy,maxz,maxx,miny,maxz, midH,midV,cszw,cszh,PlotScale,color,radius=rad)
##            self.Plot_Line_ISO(  maxx,miny,maxz,minx,miny,maxz, midH,midV,cszw,cszh,PlotScale,color,radius=rad)
##            self.Plot_Line_ISO(  minx,miny,maxz,minx,maxy,maxz, midH,midV,cszw,cszh,PlotScale,color,radius=rad)
##            #CONNECTING LINES
##            self.Plot_Line_ISO(  minx,maxy,minz,minx,maxy,maxz, midH,midV,cszw,cszh,PlotScale,color,radius=rad)
##            self.Plot_Line_ISO(  maxx,maxy,minz,maxx,maxy,maxz, midH,midV,cszw,cszh,PlotScale,color,radius=rad)
##            self.Plot_Line_ISO(  maxx,miny,minz,maxx,miny,maxz, midH,midV,cszw,cszh,PlotScale,color,radius=rad)
##            self.Plot_Line_ISO(  minx,miny,minz,minx,miny,maxz, midH,midV,cszw,cszh,PlotScale,color,radius=rad)


        Rstock = float(self.WRAP_DIA.get())/2.0
        MaxLen = radians(float(self.segarc.get())) * Rstock

        
        for line in self.coords:
            XY = line
            if XY[6]==1:
                color="blue"
                if ( self.show_path.get() == False ):
                    continue
            elif XY[6]==2:
                color="black"
                if ( self.show_path.get() == False ):
                    continue
            elif XY[6]==3:
                color="white"
                if (( self.show_path.get() == False ) and self.gcode_op.get() != "probe"):
                    continue
            else:
                color="green"
                if ( self.show_path.get() == False ):
                    continue

            if XY[7] == 1:

                if self.gcode_op.get() == "wrap" and Rstock > 0.0:
                    
                    DX = XY[3]-XY[0]
                    DY = XY[4]-XY[1]
                    DZ = XY[5]-XY[2]
                    Xlast = XY[0]
                    Ylast = XY[1]
                    Zlast = XY[2]
                        
                    if Wrap == "XYZ":
                        coordA=[ XY[0],                 XY[1],                XY[2] ]
                        coordB=[ XY[3],                 XY[4],                XY[5] ]

                    elif Wrap == "Y2A" or Wrap == "Y2B":
                        STEPS = int(floor(abs(DY) / MaxLen)+1)
                        for i in range(1,STEPS+1):
                            Xstep = XY[0]+DX/STEPS*i
                            Ystep = XY[1]+DY/STEPS*i
                            Zstep = XY[2]+DZ/STEPS*i
                            
                            angleA = Ylast/Rstock
                            yt,zt  = Transform(0.0,Zlast+Rstock, -angleA )
                            coordA = [ Xlast, yt ,zt-Rstock ]
                            
                            angleB = Ystep/Rstock
                            yt,zt  = Transform(0.0,Zstep+Rstock, -angleB )
                            coordB = [ Xstep, yt ,zt-Rstock ]

                            self.Plot_Line_ISO(  coordA[0],coordA[1],coordA[2],coordB[0],coordB[1],coordB[2], midH,midV,cszw,cszh,PlotScale,color,radius=0)
                            Xlast = Xstep
                            Ylast = Ystep
                            Zlast = Zstep                            

                    elif Wrap == "X2B" or Wrap == "X2A":
                        STEPS = int(floor(abs(DX) / MaxLen)+1)
                        for i in range(1,STEPS+1):
                            Xstep = XY[0]+DX/STEPS*i
                            Ystep = XY[1]+DY/STEPS*i
                            Zstep = XY[2]+DZ/STEPS*i
                            
                            angleA = Xlast/Rstock
                            zt, xt = Transform(Zlast+Rstock, 0.0, angleA )
                            coordA = [ xt, Ylast ,zt-Rstock ]
                            
                            angleB = Xstep/Rstock
                            zt, xt = Transform(Zstep+Rstock,0.0, angleB )
                            coordB = [ xt, Ystep ,zt-Rstock ]

                            self.Plot_Line_ISO(  coordA[0],coordA[1],coordA[2],coordB[0],coordB[1],coordB[2], midH,midV,cszw,cszh,PlotScale,color,radius=0)
                            Xlast = Xstep
                            Ylast = Ystep
                            Zlast = Zstep

                else:
                    self.Plot_Line_ISO(  XY[0],XY[1],XY[2],XY[3],XY[4],XY[5], midH,midV,cszw,cszh,PlotScale,color,radius=0)

            else:
                self.Plot_Arc(XY[7],XY[0],XY[1],XY[2],XY[3],XY[4],XY[5],XY[8],XY[9],midH,midV,cszw,cszh,PlotScale,color,radius=0)
        
        axis_length=(maxH-minH)/4
        axis_x1 =  cszw/2 + (-midH             ) / PlotScale
        axis_x2 =  cszw/2 + ( axis_length-midH ) / PlotScale
        axis_y1 =  cszh/2 - (-midV             ) / PlotScale
        axis_y2 =  cszh/2 - ( axis_length-midV ) / PlotScale

        if self.show_axis.get() == True:
            # Plot coordinate system origin
            self.Plot_Line_ISO(0,0,0,axis_length,0,0, midH,midV,cszw,cszh,PlotScale,"red",radius=0, dash=False)
            self.Plot_Line_ISO(0,0,0,0,axis_length,0, midH,midV,cszw,cszh,PlotScale,"green",radius=0, dash=False)
            self.Plot_Line_ISO(0,0,0,0,0,axis_length, midH,midV,cszw,cszh,PlotScale,"blue",radius=0, dash=False)

        # Plot Splitting line and center circle
        if self.gcode_op.get() == "split" and self.rotateb.get() == False:            
            split_angle  = float(self.SPLITA.get() )
            split_x      = float(self.SPLITX.get() )
            split_y      = float(self.SPLITY.get() )                       
            
            y_maxx = (maxx-split_x) * tan(radians(90-split_angle)) + split_y
            y_minx = (minx-split_x) * tan(radians(90-split_angle)) + split_y
            x_maxy = (maxy-split_y) * tan(radians(split_angle)) + split_x
            x_miny = (miny-split_y) * tan(radians(split_angle)) + split_x

            plt_crds=[]
            if y_maxx <= maxy and y_maxx >= miny:
                plt_crds.append([maxx,y_maxx])
                
            if y_minx <= maxy and y_minx >= miny:
                plt_crds.append([minx,y_minx])

            if x_maxy <= maxx and x_maxy >= minx:
                plt_crds.append([x_maxy,maxy])

            if x_miny <= maxx and x_miny >= minx:
                plt_crds.append([x_miny,miny])

            handle_rad=(maxx-minx)/100
            self.Plot_Circ(split_x,split_y,midH,midV,cszw,cszh,PlotScale,"yellow",handle_rad,1) 
            if len(plt_crds) > 1:
                self.Plot_Line_ISO(plt_crds[0][0],plt_crds[0][1],0.0,plt_crds[1][0],plt_crds[1][1],0.0,
                               midH,midV,cszw,cszh,PlotScale,"yellow",radius=0, dash=True)
            
    ############################################################################
    #                         Perform  Calculations                            #
    ############################################################################
    def DoIt(self):
        if self.initComplete == 0:
            return
        if (self.Check_All_Variables() > 0):
            return
        self.statusbar.configure( bg = 'yellow' )
        self.statusMessage.set(" Calculating.........")
        self.master.update_idletasks()

        # erase old data
        self.gcode   = []
        self.coords  = []
        
        for seg in self.segID:
            self.PreviewCanvas.delete(seg)
        self.segID = []

        #####################################################
        try:            
            scale_xy     = float(self.SCALEXY.get())/100.0
            scale_z      = float(self.SCALEZ.get() )/100.0
            scale_f      = float(self.SCALEF.get() )/100.0
            gcode_rotate = float(self.ROTATE.get() )
            split_angle  = float(self.SPLITA.get() )
            split_x      = float(self.SPLITX.get() )
            split_y      = float(self.SPLITY.get() )
            
            probe_depth  = float(self.probe_depth.get())
            probe_nX     = int(float((self.probe_nX.get())))
            probe_nY     = int(float((self.probe_nY.get())))
            probe_istep  = float(self.probe_istep.get())
        except:
            self.statusMessage.set(" Unable to create paths.  Check Settings Entry Values.")
            self.statusbar.configure( bg = 'red' )
            return
        
        ######################################################   
        message2 = ""
        temp,minx,maxx,miny,maxy,minz,maxz  = self.g_rip.scale_rotate_code(self.g_rip.g_code_data,[scale_xy,scale_xy,scale_z,scale_f],gcode_rotate)

        ##########################################
        #         ORIGIN LOCATING STUFF          #
        ##########################################
        midx = (minx+maxx)/2
        midy = (miny+maxy)/2
        CASE = str(self.origin.get())
        if     CASE == "Top-Left":
            x_zero = minx
            y_zero = maxy
        elif   CASE == "Top-Center":
            x_zero = midx
            y_zero = maxy
        elif   CASE == "Top-Right":
            x_zero = maxx
            y_zero = maxy
        elif   CASE == "Mid-Left":
            x_zero = minx
            y_zero = midy
        elif   CASE == "Mid-Center":
            x_zero = midx
            y_zero = midy
        elif   CASE == "Mid-Right":
            x_zero = maxx
            y_zero = midy
        elif   CASE == "Bot-Left":
            x_zero = minx
            y_zero = miny
        elif   CASE == "Bot-Center":
            x_zero = midx
            y_zero = miny
        elif   CASE == "Bot-Right":
            x_zero = maxx
            y_zero = miny
        elif   CASE == "Arc-Center":
            x_zero = 0
            y_zero = 0
        else:          #"Default"
            x_zero = 0
            y_zero = 0
        ##########################################

        temp = self.g_rip.scale_translate(temp,translate=[x_zero,y_zero,0.0])

        self.g_rip.scaled_trans = temp
        minx = minx - x_zero
        maxx = maxx - x_zero
        miny = miny - y_zero
        maxy = maxy - y_zero

        if self.gcode_op.get() == "split":
            self.g_rip.split_code(self.g_rip.scaled_trans,shift=[split_x,split_y,0],angle=split_angle)

            if self.rotateb.get() == True:
                self.g_rip.left_side,minx1,maxx1,miny1,maxy1,minz1,maxz1  = \
                    self.g_rip.scale_rotate_code(self.g_rip.left_side,[1,1,1,1],180)

                self.g_rip.right_side,minx2,maxx2,miny2,maxy2,minz2,maxz2  = \
                    self.g_rip.scale_rotate_code(self.g_rip.right_side,[1,1,1,1],0)

                minx = min(minx1,minx2)
                maxx = max(maxx1,maxx2)
                miny = min(miny1,miny2)
                maxy = max(maxy1,maxy2)
                minz = min(minz1,minz2)
                maxz = max(maxz1,maxz2)

            for line in self.g_rip.left_side:
                if line[0] == 3 or line[0] == 2 or line[0] == 1:
                    x1 = line[1][0]
                    y1 = line[1][1]
                    z1 = line[1][2]
                    x2 = line[2][0]
                    y2 = line[2][1]
                    z2 = line[2][2]
                if line[0] == 1:
                    self.coords.append([x1, y1, z1, x2, y2, z2, 2, line[0] ])
                elif line[0] == 3 or line[0] == 2:
                    xc = line[3][0]
                    yc = line[3][1]
                    self.coords.append([x1, y1, z1, x2, y2, z2, 2, line[0], xc, yc  ])
            for line in self.g_rip.right_side:
                if line[0] == 3 or line[0] == 2 or line[0] == 1:
                    x1 = line[1][0]
                    y1 = line[1][1]
                    z1 = line[1][2]
                    x2 = line[2][0]
                    y2 = line[2][1]
                    z2 = line[2][2]
                if line[0] == 1:
                    self.coords.append([x1, y1, z1, x2, y2, z2, 3, line[0] ])
                elif line[0] == 3 or line[0] == 2:
                    xc = line[3][0]
                    yc = line[3][1]
                    self.coords.append([x1, y1, z1, x2, y2, z2, 3, line[0], xc, yc  ])
                    
        elif self.gcode_op.get() == "probe":
            if (self.probe_data==[]):
                min_probe_x = minx
                min_probe_y = miny
                max_probe_x = maxx
                max_probe_y = maxy                
                
            else:
                Xoffset = float(self.probe_offsetX.get())
                Yoffset = float(self.probe_offsetY.get())
            
                min_probe_x = self.probe_data[0][0] - Xoffset
                min_probe_y = self.probe_data[0][1] - Yoffset

                last_point = len(self.probe_data)-1

                max_probe_x = self.probe_data[last_point][0] - Xoffset
                max_probe_y = self.probe_data[last_point][1] - Yoffset
                
            nX = int(max(2,int(probe_nX)))
            nY = int(max(2,int(probe_nY)))
            xPartitionLength = (max_probe_x-min_probe_x)/(nX-1)
            if (xPartitionLength < Zero): xPartitionLength=.001
            yPartitionLength = (max_probe_y-min_probe_y)/(nY-1)
            if (yPartitionLength < Zero): yPartitionLength=.001
            
            self.g_rip.probe_code(self.g_rip.scaled_trans,nX,nY,probe_istep,min_probe_x,min_probe_y,xPartitionLength,yPartitionLength)

            for line in self.g_rip.probe_gcode:
                if line[0] == 3 or line[0] == 2 or line[0] == 1:
                    x1 = line[1][0]
                    y1 = line[1][1]
                    z1 = line[1][2]
                    x2 = line[2][0]
                    y2 = line[2][1]
                    z2 = line[2][2]
                if line[0] == 1:
                    self.coords.append([x1, y1, z1, x2, y2, z2, 1, line[0] ])
                elif line[0] == 3 or line[0] == 2:
                    xc = line[3][0]
                    yc = line[3][1]
                    self.coords.append([x1, y1, z1, x2, y2, z2, 1, line[0], xc, yc  ])
                    
            if (self.probe_data==[]):
                for line in self.g_rip.probe_coords:
                    xp  = line[2]
                    yp  = line[3]
                    zp  = probe_depth
                    Lpt= xPartitionLength/5
                    if (line[0]):
                        self.coords.append([xp-Lpt, yp    , zp, xp+Lpt, yp    , zp, 3, 1 ])
                        self.coords.append([xp    , yp+Lpt, zp, xp    , yp-Lpt, zp, 3, 1 ])
                        self.coords.append([xp    , yp    ,  0, xp    , yp    , zp, 3, 1 ])
                    else:
                        self.coords.append([xp-Lpt, yp    , zp, xp+Lpt, yp    , zp, 2, 1 ])
                        self.coords.append([xp    , yp+Lpt, zp, xp    , yp-Lpt, zp, 2, 1 ])
                        self.coords.append([xp    , yp    ,  0, xp    , yp    , zp, 2, 1 ])
            else:
                for line in self.probe_data:
                    Xoffset = float(self.probe_offsetX.get())
                    Yoffset = float(self.probe_offsetY.get())
                    Zoffset = float(self.probe_offsetZ.get())
                    xp  = line[0] - Xoffset
                    yp  = line[1] - Yoffset
                    zp  = line[2] - Zoffset
                    Lpt= xPartitionLength/5
                    self.coords.append([xp-Lpt, yp    , zp, xp+Lpt, yp    , zp, 2, 1 ])
                    self.coords.append([xp    , yp+Lpt, zp, xp    , yp-Lpt, zp, 2, 1 ])
                    self.coords.append([xp    , yp    ,  0, xp    , yp    , zp, 2, 1 ])
            
        else:
            for line in self.g_rip.scaled_trans:
                if line[0] == 3 or line[0] == 2 or line[0] == 1:
                    x1 = line[1][0]
                    y1 = line[1][1]
                    z1 = line[1][2]
                    x2 = line[2][0]
                    y2 = line[2][1]
                    z2 = line[2][2]
                if line[0] == 1:
                    self.coords.append([x1, y1, z1, x2, y2, z2, 1, line[0] ])
                elif line[0] == 3 or line[0] == 2:
                    xc = line[3][0]
                    yc = line[3][1]
                    self.coords.append([x1, y1, z1, x2, y2, z2, 1, line[0], xc, yc  ])

        self.MAXX=maxx 
        self.MINX=minx 
        self.MAXY=maxy
        self.MINY=miny
        self.MAXZ=maxz
        self.MINZ=minz

        # Reset Status Bar and Entry Fields

        self.entry_set(self.Entry_GscaleXY,   self.Entry_GscaleXY_Check() ,1)
        self.entry_set(self.Entry_GscaleZ,    self.Entry_GscaleZ_Check()  ,1)
        self.entry_set(self.Entry_GscaleF,    self.Entry_GscaleF_Check()  ,1)
        self.entry_set(self.Entry_Rotate,     self.Entry_Rotate_Check()   ,1)
        self.entry_set(self.Entry_SplitX,     self.Entry_SplitX_Check()   ,1)
        self.entry_set(self.Entry_SplitY,     self.Entry_SplitY_Check()   ,1)
        self.entry_set(self.Entry_SplitA,     self.Entry_SplitA_Check()   ,1)
        self.entry_set(self.Entry_Feed,       self.Entry_Feed_Check()     ,1)
        self.entry_set(self.Entry_Zsafe,      self.Entry_Zsafe_Check()    ,1)
        self.entry_set(self.Entry_Wrap_DIA,   self.Entry_Wrap_DIA_Check() ,1)
        self.entry_set(self.Entry_Probe_Num_X,self.Entry_probe_nX_Check() ,1)
        self.entry_set(self.Entry_Probe_Num_Y,self.Entry_probe_nY_Check() ,1)
        self.entry_set(self.Entry_ProbeOffsetX,self.Entry_ProbeOffsetX_Check() ,1)
        self.entry_set(self.Entry_ProbeOffsetY,self.Entry_ProbeOffsetY_Check() ,1)
        self.entry_set(self.Entry_ProbeOffsetZ,self.Entry_ProbeOffsetZ_Check() ,1)
        self.entry_set(self.Entry_ProbeDepth,self.Entry_ProbeDepth_Check() ,1)
        
        

        if (self.gcode_op.get()=="none") or (self.gcode_op.get()=="split") or (self.gcode_op.get()=="probe"):
            self.statusMessage.set("Bounding Box = "    +
                                   "%.3g" % (maxx-minx)      +
                                   " %s " % self.units.get() +
                                   " x " +
                                   "%.3g" % (maxy-miny)      +
                                   " %s " % self.units.get() +
                                   " %s" % message2)
        else:
            self.statusMessage.set(" ")
        
        self.Plot_Data()
        ################
        #   End DoIt   #
        ################

################################################################################
#                         General Settings Window                              #
################################################################################  
    def GEN_Settings_Window(self):
        gen_settings = Toplevel(width=560, height=485)
        gen_settings.grab_set() # Use grab_set to prevent user input in the main window during calculations
        gen_settings.resizable(0,0)
        gen_settings.title('Settings')
        gen_settings.iconname("Settings")
       
        D_Yloc  = 6
        D_dY = 24
        xd_label_L = 12

        w_label=150
        w_entry=60
        w_units=35
        xd_entry_L=xd_label_L+w_label+10        
        xd_units_L=xd_entry_L+w_entry+5

        #Radio Button
        D_Yloc=D_Yloc+D_dY
        self.Label_Units = Label(gen_settings,text="Units")
        self.Label_Units.place(x=xd_label_L, y=D_Yloc, width=w_label, height=21)
        self.Radio_Units_IN = Radiobutton(gen_settings,text="inch", value="in",
                                         width="100", anchor=W)
        self.Radio_Units_IN.place(x=w_label+22, y=D_Yloc, width=75, height=23)
        self.Radio_Units_IN.configure(variable=self.units )
        self.Radio_Units_MM = Radiobutton(gen_settings,text="mm", value="mm",
                                         width="100", anchor=W)
        self.Radio_Units_MM.place(x=w_label+110, y=D_Yloc, width=75, height=23)
        self.Radio_Units_MM.configure(variable=self.units )
        self.units.trace_variable("w", self.Entry_units_var_Callback)
        
        D_Yloc=D_Yloc+D_dY
        self.Label_ArcAngle = Label(gen_settings,text="Arc Angle")
        self.Label_ArcAngle.place(x=xd_label_L, y=D_Yloc, width=w_label, height=21)
        self.Label_ArcAngle_u = Label(gen_settings,text="deg", anchor=W)
        self.Label_ArcAngle_u.place(x=xd_units_L, y=D_Yloc, width=w_units, height=21)
        self.Entry_ArcAngle = Entry(gen_settings,width="15")
        self.Entry_ArcAngle.place(x=xd_entry_L, y=D_Yloc, width=w_entry, height=23)
        self.Entry_ArcAngle.configure(textvariable=self.segarc)
        self.Entry_ArcAngle.bind('<Return>', self.Settings_ReLoad_Click)
        self.segarc.trace_variable("w", self.Entry_ArcAngle_Callback)
        self.entry_set(self.Entry_ArcAngle,self.Entry_ArcAngle_Check(),2)

        D_Yloc=D_Yloc+D_dY
        self.Label_Accuracy = Label(gen_settings,text="Accuracy")
        self.Label_Accuracy.place(x=xd_label_L, y=D_Yloc, width=w_label, height=21)
        self.Label_Accuracy_u = Label(gen_settings,textvariable=self.units, anchor=W)
        self.Label_Accuracy_u.place(x=xd_units_L, y=D_Yloc, width=w_units, height=21)
        self.Entry_Accuracy = Entry(gen_settings,width="15")
        self.Entry_Accuracy.place(x=xd_entry_L, y=D_Yloc, width=w_entry, height=23)
        self.Entry_Accuracy.configure(textvariable=self.accuracy)
        self.Entry_Accuracy.bind('<Return>', self.Settings_ReLoad_Click)
        self.accuracy.trace_variable("w", self.Entry_Accuracy_Callback)
        self.entry_set(self.Entry_Accuracy,self.Entry_Accuracy_Check(),2)
        
        D_Yloc=D_Yloc+D_dY
        self.Label_clean = Label(gen_settings,text="XY Arcs to Lines")
        self.Label_clean.place(x=xd_label_L, y=D_Yloc, width=w_label, height=21)
        self.Checkbutton_clean = Checkbutton(gen_settings,text="", anchor=W)
        self.Checkbutton_clean.place(x=xd_entry_L, y=D_Yloc, width=75, height=23)
        self.Checkbutton_clean.configure(variable=self.arc2line)
        self.arc2line.trace_variable("w", self.Settings_ReLoad_Click)
        
        D_Yloc=D_Yloc+D_dY
        self.Label_Gpre = Label(gen_settings,text="G Code Header")
        self.Label_Gpre.place(x=xd_label_L, y=D_Yloc, width=w_label, height=21)
        self.Entry_Gpre = Entry(gen_settings,width="15")
        self.Entry_Gpre.place(x=xd_entry_L, y=D_Yloc, width=300, height=23)
        self.Entry_Gpre.configure(textvariable=self.gpre)

        D_Yloc=D_Yloc+D_dY
        self.Label_Gpost = Label(gen_settings,text="G Code Postscript")
        self.Label_Gpost.place(x=xd_label_L, y=D_Yloc, width=w_label, height=21)
        self.Entry_Gpost = Entry(gen_settings,width="15")
        self.Entry_Gpost.place(x=xd_entry_L, y=D_Yloc, width=300, height=23)
        self.Entry_Gpost.configure(textvariable=self.gpost)

        D_Yloc=D_Yloc+D_dY
        self.Label_var_dis = Label(gen_settings,text="Disable Variables")
        self.Label_var_dis.place(x=xd_label_L, y=D_Yloc, width=w_label, height=21)
        self.Checkbutton_var_dis = Checkbutton(gen_settings,text="", anchor=W)
        self.Checkbutton_var_dis.place(x=xd_entry_L, y=D_Yloc, width=75, height=23)
        self.Checkbutton_var_dis.configure(variable=self.var_dis)

        D_Yloc=D_Yloc+D_dY
        self.Label_DPlaces_L = Label(gen_settings,text="Decimal Places (XYZ)")
        self.Label_DPlaces_L.place(x=xd_label_L, y=D_Yloc, width=w_label, height=21)
        self.Entry_DPlaces_L = Entry(gen_settings,width="15")
        self.Entry_DPlaces_L.place(x=xd_entry_L, y=D_Yloc, width=w_entry, height=23)
        self.Entry_DPlaces_L.configure(textvariable=self.DPlaces_L)
        self.Entry_DPlaces_L.bind('<Return>', self.Settings_ReLoad_Click)
        self.DPlaces_L.trace_variable("w", self.Entry_DPlaces_L_Callback)
        self.entry_set(self.Entry_DPlaces_L,self.Entry_DPlaces_L_Check(),2)
        
        D_Yloc=D_Yloc+D_dY
        self.Label_DPlaces_R = Label(gen_settings,text="Decimal Places (Rotary)")
        self.Label_DPlaces_R.place(x=xd_label_L, y=D_Yloc, width=w_label, height=21)
        self.Entry_DPlaces_R = Entry(gen_settings,width="15")
        self.Entry_DPlaces_R.place(x=xd_entry_L, y=D_Yloc, width=w_entry, height=23)
        self.Entry_DPlaces_R.configure(textvariable=self.DPlaces_R)
        self.Entry_DPlaces_R.bind('<Return>', self.Settings_ReLoad_Click)
        self.DPlaces_R.trace_variable("w", self.Entry_DPlaces_R_Callback)
        self.entry_set(self.Entry_DPlaces_R,self.Entry_DPlaces_R_Check(),2)

        D_Yloc=D_Yloc+D_dY
        self.Label_DPlaces_F = Label(gen_settings,text="Decimal Places (Feed)")
        self.Label_DPlaces_F.place(x=xd_label_L, y=D_Yloc, width=w_label, height=21)
        self.Entry_DPlaces_F = Entry(gen_settings,width="15")
        self.Entry_DPlaces_F.place(x=xd_entry_L, y=D_Yloc, width=w_entry, height=23)
        self.Entry_DPlaces_F.configure(textvariable=self.DPlaces_F)
        self.Entry_DPlaces_F.bind('<Return>', self.Settings_ReLoad_Click)
        self.DPlaces_F.trace_variable("w", self.Entry_DPlaces_F_Callback)
        self.entry_set(self.Entry_DPlaces_F,self.Entry_DPlaces_F_Check(),2)
        
        D_Yloc=D_Yloc+D_dY
        self.Label_WriteAll = Label(gen_settings,text="Write all Coords")
        self.Label_WriteAll.place(x=xd_label_L, y=D_Yloc, width=w_label, height=21)
        self.Checkbutton_WriteAll = Checkbutton(gen_settings,text="", anchor=W)
        self.Checkbutton_WriteAll.place(x=xd_entry_L, y=D_Yloc, width=75, height=23)
        self.Checkbutton_WriteAll.configure(variable=self.WriteAll)

        D_Yloc=D_Yloc+D_dY
        self.Label_NoComments = Label(gen_settings,text="Remove Comments")
        self.Label_NoComments.place(x=xd_label_L, y=D_Yloc, width=w_label, height=21)
        self.Checkbutton_NoComments = Checkbutton(gen_settings,text="", anchor=W)
        self.Checkbutton_NoComments.place(x=xd_entry_L, y=D_Yloc, width=75, height=23)
        self.Checkbutton_NoComments.configure(variable=self.NoComments)

        D_Yloc=D_Yloc+D_dY
        self.Label_SaveConfig = Label(gen_settings,text="Configuration File")
        self.Label_SaveConfig.place(x=xd_label_L, y=D_Yloc, width=w_label, height=21)
        self.GEN_SaveConfig = Button(gen_settings,text="Save")
        self.GEN_SaveConfig.place(x=xd_entry_L, y=D_Yloc, width=w_entry, height=21, anchor="nw")
        self.GEN_SaveConfig.bind("<ButtonRelease-1>", self.Write_Config_File)
        
        ## Buttons ##
        gen_settings.update_idletasks()
        Ybut=int(gen_settings.winfo_height())-30
        Xbut=int(gen_settings.winfo_width()/2)

        self.GEN_Reload = Button(gen_settings,text="Recalculate")
        self.GEN_Reload.place(x=Xbut-65, y=Ybut, width=130, height=30, anchor="e")
        self.GEN_Reload.bind("<ButtonRelease-1>", self.Recalculate_Click)
        
        self.GEN_Recalculate = Button(gen_settings,text="Re-Load G-Code")
        self.GEN_Recalculate.place(x=Xbut, y=Ybut, width=130, height=30, anchor="c")
        self.GEN_Recalculate.bind("<ButtonRelease-1>", self.Settings_ReLoad_Click)
        
        self.GEN_Close = Button(gen_settings,text="Close")
        self.GEN_Close.bind("<ButtonRelease-1>", self.Close_Current_Window_Click)
        
        self.GEN_Close.place(x=Xbut+65, y=Ybut, width=130, height=30, anchor="w")


################################################################################
#                           Stock Round Window                                 #
################################################################################  
    def STOCK_Round_Window(self):
        stock_round_win = Toplevel(width=300, height=350)
        stock_round_win.grab_set() # Use grab_set to prevent user input in the main window during calculations
        stock_round_win.resizable(0,0)
        stock_round_win.title('Stock Rounding')
        stock_round_win.iconname("Stock Rounding")
       
        #####################################
        if self.WRAP_TYPE.get() == "Y2A" or self.WRAP_TYPE.get() == "Y2B":
            self.SRmin_label.set("Min X")
            self.SRmax_label.set("Max X")
        else:
            self.SRmin_label.set("Min Y")
            self.SRmax_label.set("Max Y")
        #####################################
        
        D_Yloc  = 6
        D_dY = 24
        xd_label_L = 12

        w_label=130
        w_entry=60
        w_units=35
        xd_entry_L=xd_label_L+w_label+10        
        xd_units_L=xd_entry_L+w_entry+5

        D_Yloc=D_Yloc+D_dY      
        self.Label_SR_DIA_REF = Label(stock_round_win,text="Wrap Diameter")
        self.Label_SR_DIA_REF.place(x=xd_label_L, y=D_Yloc, width=w_label, height=21)
        self.Label_SR_DIA_REF_u = Label(stock_round_win,textvariable=self.units, anchor=W)
        self.Label_SR_DIA_REF_u.place(x=xd_units_L, y=D_Yloc, width=w_units, height=21)
        self.Label_SR_DIA_DATA = Label(stock_round_win,textvariable=self.WRAP_DIA)
        self.Label_SR_DIA_DATA.place(x=xd_entry_L, y=D_Yloc, width=w_entry, height=23)
 
        D_Yloc=D_Yloc+D_dY
        self.Label_SR_Tool_DIA = Label(stock_round_win,text="Tool Diameter")
        self.Label_SR_Tool_DIA.place(x=xd_label_L, y=D_Yloc, width=w_label, height=21)
        self.Label_SR_Tool_DIA_u = Label(stock_round_win,textvariable=self.units, anchor=W)
        self.Label_SR_Tool_DIA_u.place(x=xd_units_L, y=D_Yloc, width=w_units, height=21)
        self.Entry_SR_Tool_DIA = Entry(stock_round_win,width="15")
        self.Entry_SR_Tool_DIA.place(x=xd_entry_L, y=D_Yloc, width=w_entry, height=23)
        self.Entry_SR_Tool_DIA.configure(textvariable=self.sr_tool_dia)
        self.sr_tool_dia.trace_variable("w", self.Entry_SR_Tool_DIA_Callback)
        self.entry_set(self.Entry_SR_Tool_DIA,self.Entry_SR_Tool_DIA_Check(),2)

        D_Yloc=D_Yloc+D_dY
        self.Label_SR_Step = Label(stock_round_win,text="Step Over")
        self.Label_SR_Step.place(x=xd_label_L, y=D_Yloc, width=w_label, height=21)
        self.Label_SR_Step_u = Label(stock_round_win,text="%", anchor=W)
        self.Label_SR_Step_u.place(x=xd_units_L, y=D_Yloc, width=w_units, height=21)
        self.Entry_SR_Step = Entry(stock_round_win,width="15")
        self.Entry_SR_Step.place(x=xd_entry_L, y=D_Yloc, width=w_entry, height=23)
        self.Entry_SR_Step.configure(textvariable=self.sr_step)
        self.sr_step.trace_variable("w", self.Entry_SR_Step_Callback)
        self.entry_set(self.Entry_SR_Step,self.Entry_SR_Step_Check(),2)

        D_Yloc=D_Yloc+D_dY
        self.Label_SR_Remove = Label(stock_round_win,text="Cut Depth")
        self.Label_SR_Remove.place(x=xd_label_L, y=D_Yloc, width=w_label, height=21)
        self.Label_SR_Remove_u = Label(stock_round_win,textvariable=self.units, anchor=W)
        self.Label_SR_Remove_u.place(x=xd_units_L, y=D_Yloc, width=w_units, height=21)
        self.Entry_SR_Remove = Entry(stock_round_win,width="15")
        self.Entry_SR_Remove.place(x=xd_entry_L, y=D_Yloc, width=w_entry, height=23)
        self.Entry_SR_Remove.configure(textvariable=self.sr_remove)
        self.Entry_SR_Remove.bind('<Return>', self.Settings_ReLoad_Click)
        self.sr_remove.trace_variable("w", self.Entry_SR_Remove_Callback)
        self.entry_set(self.Entry_SR_Remove,self.Entry_SR_Remove_Check(),2)

        D_Yloc=D_Yloc+D_dY
        self.Label_SR_MIN_X = Label(stock_round_win,textvariable=self.SRmin_label)
        self.Label_SR_MIN_X.place(x=xd_label_L, y=D_Yloc, width=w_label, height=21)
        self.Label_SR_MIN_X_u = Label(stock_round_win,textvariable=self.units, anchor=W)
        self.Label_SR_MIN_X_u.place(x=xd_units_L, y=D_Yloc, width=w_units, height=21)
        self.Entry_SR_MIN_X = Entry(stock_round_win,width="15")
        self.Entry_SR_MIN_X.place(x=xd_entry_L, y=D_Yloc, width=w_entry, height=23)
        self.Entry_SR_MIN_X.configure(textvariable=self.sr_minx)
        self.sr_minx.trace_variable("w", self.Entry_SR_MIN_X_Callback)
        self.entry_set(self.Entry_SR_MIN_X,self.Entry_SR_MIN_X_Check(),2)

        D_Yloc=D_Yloc+D_dY
        self.Label_SR_MAX_X = Label(stock_round_win,textvariable=self.SRmax_label)
        self.Label_SR_MAX_X.place(x=xd_label_L, y=D_Yloc, width=w_label, height=21)
        self.Label_SR_MAX_X_u = Label(stock_round_win,textvariable=self.units, anchor=W)
        self.Label_SR_MAX_X_u.place(x=xd_units_L, y=D_Yloc, width=w_units, height=21)
        self.Entry_SR_MAX_X = Entry(stock_round_win,width="15")
        self.Entry_SR_MAX_X.place(x=xd_entry_L, y=D_Yloc, width=w_entry, height=23)
        self.Entry_SR_MAX_X.configure(textvariable=self.sr_maxx)
        self.sr_maxx.trace_variable("w", self.Entry_SR_MAX_X_Callback)
        self.entry_set(self.Entry_SR_MAX_X,self.Entry_SR_MAX_X_Check(),2)

        D_Yloc=D_Yloc+D_dY
        self.Label_SR_ZSafe = Label(stock_round_win,text="Z Safe")
        self.Label_SR_ZSafe.place(x=xd_label_L, y=D_Yloc, width=w_label, height=21)
        self.Label_SR_ZSafe_u = Label(stock_round_win,textvariable=self.units, anchor=W)
        self.Label_SR_ZSafe_u.place(x=xd_units_L, y=D_Yloc, width=w_units, height=21)
        self.Entry_SR_ZSafe = Entry(stock_round_win,width="15")
        self.Entry_SR_ZSafe.place(x=xd_entry_L, y=D_Yloc, width=w_entry, height=23)
        self.Entry_SR_ZSafe.configure(textvariable=self.sr_zsafe)
        self.sr_zsafe.trace_variable("w", self.Entry_SR_ZSafe_Callback)
        self.entry_set(self.Entry_SR_ZSafe,self.Entry_SR_ZSafe_Check(),2)

        D_Yloc=D_Yloc+D_dY
        self.Label_SR_Feed = Label(stock_round_win,text="Feed Rate")
        self.Label_SR_Feed.place(x=xd_label_L, y=D_Yloc, width=w_label, height=21)
        self.Label_SR_Feed_u = Label(stock_round_win,textvariable=self.funits, anchor=W)
        self.Label_SR_Feed_u.place(x=xd_units_L, y=D_Yloc, width=w_units+15, height=21)
        self.Entry_SR_Feed = Entry(stock_round_win,width="15")
        self.Entry_SR_Feed.place(x=xd_entry_L, y=D_Yloc, width=w_entry, height=23)
        self.Entry_SR_Feed.configure(textvariable=self.sr_feed)
        self.Entry_SR_Feed.bind('<Return>', self.Settings_ReLoad_Click)
        self.sr_feed.trace_variable("w", self.Entry_SR_Feed_Callback)
        self.entry_set(self.Entry_SR_Feed,self.Entry_SR_Feed_Check(),2)

        D_Yloc=D_Yloc+D_dY
        self.Label_SR_PlungeFeed = Label(stock_round_win,text="Plunge Feed")
        self.Label_SR_PlungeFeed.place(x=xd_label_L, y=D_Yloc, width=w_label, height=21)
        self.Label_SR_PlungeFeed_u = Label(stock_round_win,textvariable=self.funits, anchor=W)
        self.Label_SR_PlungeFeed_u.place(x=xd_units_L, y=D_Yloc, width=w_units+15, height=21)
        self.Entry_SR_PlungeFeed = Entry(stock_round_win,width="15")
        self.Entry_SR_PlungeFeed.place(x=xd_entry_L, y=D_Yloc, width=w_entry, height=23)
        self.Entry_SR_PlungeFeed.configure(textvariable=self.sr_plungef)
        self.Entry_SR_PlungeFeed.bind('<Return>', self.Settings_ReLoad_Click)
        self.sr_plungef.trace_variable("w", self.Entry_SR_PlungeFeed_Callback)
        self.entry_set(self.Entry_SR_PlungeFeed,self.Entry_SR_PlungeFeed_Check(),2)

        ##self.round_type #"axial" or "spiral"
        ##Radio Button
        #D_Yloc=D_Yloc+D_dY
        #self.Label_SR_Type = Label(stock_round_win,text="Type")
        #self.Label_SR_Type.place(x=xd_label_L, y=D_Yloc, width=113, height=21)
        #self.Radio_SR_Type_Spiral = Radiobutton(stock_round_win,text="Spiral", value="spiral", width="100", anchor=W)
        #self.Radio_SR_Type_Spiral.place(x=w_label+22, y=D_Yloc, width=75, height=23)
        #self.Radio_SR_Type_Spiral.configure(variable=self.round_type )
        #self.Radio_SR_Type_Linear = Radiobutton(stock_round_win,text="Axial", value="axial", width="100", anchor=W)
        #self.Radio_SR_Type_Linear.place(x=w_label+110, y=D_Yloc, width=75, height=23)
        #self.Radio_SR_Type_Linear.configure(variable=self.round_type )

        D_Yloc=D_Yloc+D_dY
        self.Label_SR_Climb = Label(stock_round_win,text="Climb Mill")
        self.Label_SR_Climb.place(x=xd_label_L, y=D_Yloc, width=w_label, height=21)
        self.Checkbutton_SR_Climb = Checkbutton(stock_round_win,text="", anchor=W)
        self.Checkbutton_SR_Climb.place(x=xd_entry_L, y=D_Yloc, width=75, height=23)
        self.Checkbutton_SR_Climb.configure(variable=self.sr_climb)

        ## Buttons ##
        stock_round_win.update_idletasks()
        Ybut=int(stock_round_win.winfo_height())-30
        Xbut=int(stock_round_win.winfo_width()/2)
 
        self.SR_Save = Button(stock_round_win,text="Save Rounding G-Code", command=self.menu_File_Save_G_Code_round)
        self.SR_Save.place(x=Xbut, y=Ybut, width=140, height=30, anchor="e")
        
        self.GEN_Close = Button(stock_round_win,text="Close")
        self.GEN_Close.bind("<ButtonRelease-1>", self.Close_Current_Window_Click)        
        self.GEN_Close.place(x=Xbut, y=Ybut, width=140, height=30, anchor="w")

#################
### START LIB ###
#################
############################################################################
class G_Code_Rip:
    def __init__(self):
        self.Zero      = 0.0000001
        self.g_code_data  = []
        self.scaled_trans = []
        self.right_side   = []
        self.left_side    = []
        self.probe_gcode  = []
        self.probe_coords = []
        self.arc_angle    = 10
        self.accuracy     = .001
        self.units        = "in"

    def Read_G_Code(self,filename, XYarc2line = False, arc_angle=10, units="in", Accuracy=""):
        self.g_code_data  = []
        self.scaled_trans = []
        self.right_side   = []
        self.left_side    = []
        self.probe_gcode  = []
        self.probe_coords = []
        self.arc_angle    = arc_angle
        self.units        = units
        if Accuracy == "":
            if units == "in":
                self.accuracy = .001
            else:
                self.accuracy = .025
        else:
            self.accuracy = float(Accuracy)
        
        READ_MSG = []

        # Try to open file for reading
        try:
            fin = open(filename,'r')
        except:
            READ_MSG.append("Unable to open file: %s" %(filename))
            return READ_MSG

        scale = 1
        variables = []
        line_number = 0

        xind=0
        yind=1
        zind=2

        mode_arc  = "incremental" # "absolute"
        mode_pos = "absolute"    # "incremental"

        mvtype = 1  # G0 (Rapid), G1 (linear), G2 (clockwise arc) or G3 (counterclockwise arc). 
        plane  = "17" # G17 (Z-axis, XY-plane), G18 (Y-axis, XZ-plane), or G19 (X-axis, YZ-plane)
        pos     =['','','']
        pos_last=['','','']
        POS     =[complex(0,1),complex(0,1),complex(0,1)]
        feed = 0        
        
        #########################
        for line in fin:
            line_number = line_number + 1
            #print line_number
            line = line.replace("\n","")
            line = line.replace("\r","")
            code_line=[]
            
            #####################
            ### FIND COMMENTS ###
            #####################
            if line.find("(") != -1:
                s = line.find("(")
                p_cnt=0
                e = len(line)
                for i_txt in range(s,len(line)):
                    if line[i_txt]=="(":
                        p_cnt=p_cnt+1
                    if line[i_txt]==")":
                        p_cnt=p_cnt-1
                    if p_cnt==0:
                        e=i_txt
                        #print(e,line[s:e+1])
                        break
                code_line.append([ ";", line[s:e+1] ])
                line = self.rm_text(line,s,e)
            
            if line.find(";") != -1:
                s = line.find(";")
                e = len(line)
                code_line.append([ ";", line[s:e] ])
                line = self.rm_text(line,s,e)
            # If comment exists write it to output 
            if code_line!= []: 
                for comment in code_line:
                    self.g_code_data.append(comment)
                code_line=[]

            # Switch remaining non comment data to upper case
            # and remove spaces
            line = line.upper()
            line = line.replace(" ","")

            
            #####################################################
            # Find # chars and check for a variable definition  #
            #####################################################
            if line.find("#") != -1:
                s = line.rfind("#")
                while s != -1:
                    if line[s+1] == '<':
                        e = s+2
                        while line[e] != '>' and e <= len(line):
                            e = e+1
                        e = e+1
                        vname = line[s:e].lower()
                    else:
                        vname = re.findall(r'[-+]?\d+',line[s:])[0]	
                        e = s + 1 + len(vname)
                        vname = line[s:e]

                    DEFINE = False
                    if e < len(line):
                        if line[e]=="=":
                            DEFINE = True
                    if DEFINE:
                        try:
                            vval = "%.4f" %(float(line[e+1:]))
                            line = ''
                        except:
                            try:
                                vval = self.EXPRESSION_EVAL(line[e+1:])
                                line = ''
                            except:
                                READ_MSG.append(str(sys.exc_info()[1]))
                                return READ_MSG
                            
                        variables.append([vname,vval])
                        line  = self.rm_text(line,s,e-1)
                    else:
                        line = self.rm_text(line,s,e-1)
                        VALUE = ''
                        for V in variables:
                            if V[0] == vname:
                                VALUE = V[1]
                                
                        line = self.insert_text(line,VALUE,s)
                        
                    s = line.rfind("#")

            #########################
            ### FIND MATH REGIONS ###
            #########################
            if line.find("[") != -1 and line.find("[") != 0:
                ############################
                s = line.find("[")
                while s != -1:
                    e = s + 1
                    val = 1
                    while val > 0:
                        if e >= len(line):
                            MSG = "ERROR: Unable to evaluate expression: G-Code Line %d" %(line_number)
                            raise ValueError(MSG)
                        if line[e]=="[":
                            val = val + 1
                        elif line[e] == "]":
                            val = val - 1
                        e = e + 1
                        
                    new_val = self.EXPRESSION_EVAL(line[s:e])
                    
                    line = self.rm_text(line,s,e-1)
                    line = self.insert_text(line,new_val,s)
                    s = line.find("[")
                #############################


            ####################################
            ### FIND FULLY UNSUPPORTED CODES ###
            ####################################
            # D Tool radius compensation number
            # E ...
            # L ...
            # O ... Subroutines
            # Q Feed increment in G73, G83 canned cycles
            # A A axis of machine
            # B B axis of machine
            # C C axis of machine
            # U U axis of machine
            # V V axis of machine
            # W W axis of machine

            UCODES = ("A","B","C","D","E","L","O","Q","U","V","W")
            skip = False
            for code in UCODES:
                if line.find(code) != -1:
                    READ_MSG.append("Warning: %s Codes are not supported ( G-Code File Line: %d )" %(code,line_number))
                    skip = True
            if skip:
                continue
                    

            ##############################
            ###    FIND ALL CODES      ###
            ##############################
            # F Feed rate
            # G General function
            # I X offset for arcs and G87 canned cycles
            # J Y offset for arcs and G87 canned cycles
            # K Z offset for arcs and G87 canned cycles. Spindle-Motion Ratio for G33 synchronized movements.
            # M Miscellaneous function (See table Modal Groups)
            # P Dwell time in canned cycles and with G4. Key used with G10. Used with G2/G3.
            # R Arc radius or canned cycle plane
            # S Spindle speed
            # T Tool selection
            # X X axis of machine
            # Y Y axis of machine
            # Z Z axis of machine
            
            ALL = ("A","B","C","D","E","F","G","H","I","J",\
                   "K","L","M","N","O","P","Q","R","S","T",\
                   "U","V","W","X","Y","Z","#","=")
            temp = []
            line = line.replace(" ","")
            for code in ALL:
                index=-1
                index = line.find(code,index+1)
                while index != -1:
                    temp.append([code,index])
                    index = line.find(code,index+1)
            temp.sort(key=lambda a:a[1])

            code_line=[]
            if temp != []:
                x = 0
                while x <= len(temp)-1:
                    s = temp[x][1]+1
                    if x == len(temp)-1:
                        e = len(line)
                    else:    
                        e = temp[x+1][1]
                    
                    CODE  = temp[x][0]
                    VALUE = line[s:e]
                    code_line.append([ CODE, VALUE ])
                    x = x + 1

            #################################
                    
            mv_flag   = 0
            POS_LAST = POS[:]
            #CENTER  = ['','','']
            CENTER   = POS_LAST[:]
            passthru = ""
            for i in range(len(code_line)):
            #for com in code_line:
                com = code_line[i]
                if com[0] == "G":
                    Gnum = "%g" %(float(com[1]))
                    if Gnum == "0" or Gnum == "1":
                        mvtype = int(Gnum)
                    elif Gnum == "2" or Gnum == "3":
                        mvtype = int(Gnum)
                        #CENTER = POS_LAST[:]
                    elif Gnum == "17":
                        plane = Gnum
                    elif Gnum == "18":
                        plane = Gnum
                    elif Gnum == "19":
                        plane = Gnum
                    elif Gnum == "20":
                        if units == "in":
                            scale = 1
                        else:
                            scale = 25.4
                    elif Gnum == "21":
                        if units == "mm":
                            scale = 1
                        else:
                            scale = 1.0/25.4
                    elif Gnum == "81":
                        READ_MSG.append("Warning: G%s Codes are not supported ( G-Code File Line: %d )" %(Gnum,line_number))
                    elif Gnum == "90.1":
                        mode_arc = "absolute"
                        
                    elif Gnum == "90":
                        mode_pos = "absolute"

                    elif Gnum == "91":
                        mode_pos = "incremental"
                    
                    elif Gnum == "91.1":
                        mode_arc = "incremental"

                    elif Gnum == "92":
                        READ_MSG.append("Warning: G%s Codes are not supported ( G-Code File Line: %d )" %(Gnum,line_number))
                        
                    elif Gnum == "38.2":
                        READ_MSG.append("Warning: G%s Codes are not supported ( G-Code File Line: %d )" %(Gnum,line_number))

                    elif Gnum == "43":
                        passthru = passthru +  "%s%s " %(com[0],com[1])

                    elif Gnum == "53":
                        READ_MSG.append("Warning: G%s Codes are not fully supported ( G-Code File Line: %d )" %(Gnum,line_number))
                        passthru = passthru +  "%s%s " %(com[0],com[1])
                        for i_dump in range(i+1,len(code_line)):
                            print(code_line[i_dump])
                            passthru = passthru +  "%s%s " %(code_line[i_dump][0],code_line[i_dump][1])
                        break
                        
                    else:
                        passthru = passthru +  "%s%s " %(com[0],com[1])
                
                elif com[0] == "X":
                    if mode_pos == "absolute":
                        POS[xind] = float(com[1])*scale
                    else:
                        POS[xind] = float(com[1])*scale + POS_LAST[xind]
                    mv_flag = 1

                elif com[0] == "Y":
                    if mode_pos == "absolute":
                        POS[yind] = float(com[1])*scale
                    else:
                        POS[yind] = float(com[1])*scale + POS_LAST[yind]
                    mv_flag = 1

                elif com[0] == "Z":                        
                    if mode_pos == "absolute":
                        POS[zind] = float(com[1])*scale
                    else:
                        POS[zind] = float(com[1])*scale + POS_LAST[zind]
                    mv_flag = 1

                ###################
                elif com[0] == "I":
                    if mode_arc == "absolute":
                        CENTER[xind] = float(com[1])*scale
                    else:
                        CENTER[xind] = float(com[1])*scale + POS_LAST[xind]
                    if (mvtype==2 or mvtype==3):
                        mv_flag = 1
                    
                elif com[0] == "J":
                    if mode_arc == "absolute":
                        CENTER[yind] = float(com[1])*scale
                    else:
                        CENTER[yind] = float(com[1])*scale + POS_LAST[yind]
                    if (mvtype==2 or mvtype==3):
                        mv_flag = 1
                elif com[0] == "K":
                    if mode_arc == "absolute":
                        CENTER[zind] = float(com[1])*scale
                    else:
                        CENTER[zind] = float(com[1])*scale + POS_LAST[zind]
                    if (mvtype==2 or mvtype==3):
                        mv_flag = 1

                elif com[0] == "R":
                    Rin= float(com[1])*scale
                    CENTER = self.get_center(POS,POS_LAST,Rin,mvtype,plane)
                        
                ###################
                elif com[0] == "F":
                    feed = float(com[1]) * scale

                elif com[0] == ";":
                    passthru = passthru + "%s " %(com[1])

                elif com[0] == "P" and mv_flag == 1 and mvtype > 1:
                    READ_MSG.append("Aborting G-Code Reading: P word specifying the number of full or partial turns of arc are not supported")
                    return READ_MSG

                elif com[0] == "M":
                    Mnum = "%g" %(float(com[1]))
                    if Mnum == "2":
                        self.g_code_data.append([ "M2", "(END PROGRAM)" ])
                    passthru = passthru + "%s%s " %(com[0],com[1])

                elif com[0] == "N":
                    pass
                    #print "Ignoring Line Number %g" %(float(com[1]))
                    
                else:
                    passthru = passthru + "%s%s " %(com[0],com[1])


            pos      = POS[:]
            pos_last = POS_LAST[:]
            center = CENTER[:]
 
            # Most command on a line are executed prior to a move so 
            # we will write the passthru commands on the line before we found them
            # only "M0, M1, M2, M30 and M60" are executed after the move commands
            # there is a risk that one of these commands could stop the program before
            # the move is completed

            if passthru != '':
                self.g_code_data.append("%s" %(passthru))


            ###############################################################################
            if mv_flag == 1:
                if mvtype == 0:
                    self.g_code_data.append([mvtype,pos_last[:],pos[:]])
                if mvtype == 1:
                    self.g_code_data.append([mvtype,pos_last[:],pos[:],feed])
                if mvtype == 2 or mvtype == 3:
                    if plane == "17":
                        if XYarc2line == False:
                            self.g_code_data.append([mvtype,pos_last[:],pos[:],center[:],feed])
                        else:
                            data = self.arc2lines(pos_last[:],pos[:],center[:], mvtype, plane)
                            
                            for line in data:
                                XY=line
                                self.g_code_data.append([1,XY[:3],XY[3:],feed])
                                
                    elif plane == "18":
                        data = self.arc2lines(pos_last[:],pos[:],center[:], mvtype, plane)
                        for line in data:
                            XY=line
                            self.g_code_data.append([1,XY[:3],XY[3:],feed])
                            
                    elif plane == "19":
                        data = self.arc2lines(pos_last[:],pos[:],center[:], mvtype, plane)
                        for line in data:
                            XY=line
                            self.g_code_data.append([1,XY[:3],XY[3:],feed])
            ###############################################################################
            #################################
        fin.close()
                    
        ## Post process the g-code data to remove complex numbers
        cnt = 0
        firstx = complex(0,1)
        firsty = complex(0,1)
        firstz = complex(0,1)
        first_sum = firstx + firsty + firstz
        while ((cnt < len(self.g_code_data)) and (isinstance(first_sum, complex))):
            line = self.g_code_data[cnt]
            if line[0] == 0 or line[0] == 1 or line[0] == 2 or line[0] == 3:
                if (isinstance(firstx, complex)): firstx = line[2][0]
                if (isinstance(firsty, complex)): firsty = line[2][1]
                if (isinstance(firstz, complex)): firstz = line[2][2]
            cnt=cnt+1
            first_sum = firstx + firsty + firstz
        max_cnt = cnt
        cnt = 0
        ambiguousX = False
        ambiguousY = False
        ambiguousZ = False
        while (cnt < max_cnt):
            line = self.g_code_data[cnt]
            if line[0] == 1 or line[0] == 2 or line[0] == 3:
                # X Values
                if (isinstance(line[1][0], complex)):
                    line[1][0] = firstx
                    ambiguousX = True
                if (isinstance(line[2][0], complex)):
                    line[2][0] = firstx
                    ambiguousX = True
                # Y values
                if (isinstance(line[1][1], complex)):
                    line[1][1] = firsty
                    ambiguousY = True
                if (isinstance(line[2][1], complex)):
                    line[2][1] = firsty
                    ambiguousY = True
                # Z values
                if (isinstance(line[1][2], complex)):
                    line[1][2] = firstz
                    ambiguousZ = True
                if (isinstance(line[2][2], complex)):
                    line[2][2] = firstz
                    ambiguousZ = True
            cnt=cnt+1
        if (ambiguousX or ambiguousY or ambiguousZ):
            MSG = "Ambiguous G-Code start location:\n"
            if (ambiguousX):  MSG = MSG + "X position is not set by a G0(rapid) move prior to a G1,G2 or G3 move.\n"
            if (ambiguousY):  MSG = MSG + "Y position is not set by a G0(rapid) move prior to a G1,G2 or G3 move.\n"
            if (ambiguousZ):  MSG = MSG + "Z position is not set by a G0(rapid) move prior to a G1,G2 or G3 move.\n"
            MSG = MSG + "!! Review output files carefully !!"
            READ_MSG.append(MSG)
        
        return READ_MSG

    def get_center(self,POS,POS_LAST,Rin,mvtype,plane="17"):
        if   plane == "18":
            xind=2
            yind=0
            zind=1
        elif plane == "19":
            xind=1
            yind=2
            zind=0
        elif plane == "17":
            xind=0
            yind=1
            zind=2

        CENTER=["","",""]
        cord = sqrt( (POS[xind]-POS_LAST[xind])**2 + (POS[yind]-POS_LAST[yind])**2 )
        v1 = cord/2.0
        
        #print "rin=%f v1=%f (Rin**2 - v1**2)=%f" %(Rin,v1,(Rin**2 - v1**2))
        v2_sq = Rin**2 - v1**2
        if v2_sq<0.0:
            v2_sq = 0.0
        v2 = sqrt( v2_sq )

        theta = self.Get_Angle2(POS[xind]-POS_LAST[xind],POS[yind]-POS_LAST[yind])

        if mvtype == 3:
            dxc,dyc = self.Transform(-v2,v1,radians(theta-90))
        elif mvtype == 2:
            dxc,dyc = self.Transform(v2,v1,radians(theta-90))
        else:
            return "Center Error"

        xcenter = POS_LAST[xind] + dxc
        ycenter = POS_LAST[yind] + dyc

        CENTER[xind] = xcenter
        CENTER[yind] = ycenter
        CENTER[zind] = POS_LAST[zind]

        return CENTER

    #######################################
    def split_code(self,code2split,shift=[0,0,0],angle=0.0):
        xsplit=0.0
        mvtype = -1  # G0 (Rapid), G1 (linear), G2 (clockwise arc) or G3 (counterclockwise arc).

        passthru = ""
        POS     =[0,0,0]
        feed = 0
        self.right_side = []
        self.left_side  = []

        L = 0
        R = 1
        for line in code2split:
            if line[0] == 1:
                mvtype   = line[0]
                POS_LAST = line[1][:]
                POS      = line[2][:]
                CENTER   = ['','','']
                feed     = line[3]

            elif line[0] == 3 or line[0] == 2:
                mvtype   = line[0]
                POS_LAST = line[1][:]
                POS      = line[2][:]
                CENTER   = line[3][:]
                feed     = line[4]

            else:
                mvtype  = -1
                passthru = line
                
            ###############################################################################
            if mvtype >= 1 and mvtype <= 3:
                pos      = self.coordop(POS,shift,angle)
                pos_last = self.coordop(POS_LAST,shift,angle)

                if CENTER[0]!='' and CENTER[1]!='':
                    center = self.coordop(CENTER,shift,angle)
                else:
                    center = CENTER

                this=""
                other=""
                
                if pos_last[0] > xsplit+self.Zero:
                    flag_side = R
                elif pos_last[0] < xsplit-self.Zero:
                    flag_side = L
                else:
                    if mvtype == 1:
                        if pos[0] >= xsplit:
                            flag_side = R
                        else:
                            flag_side = L
                            
                    elif mvtype == 2:
                        
                        if abs(pos_last[1]-center[1]) < self.Zero:
                            if center[0] > xsplit:
                                flag_side = R
                            else:
                                flag_side = L
                        else:
                            if   pos_last[1] >= center[1]:
                                flag_side = R
                            else:
                                flag_side = L
                                
                    else: #(mvtype == 3)
                        if abs(pos_last[1]-center[1]) < self.Zero:
                            if center[0] > xsplit:
                                flag_side = R
                            else:
                                flag_side = L
                        else:
                            if   pos_last[1] >= center[1]:
                                flag_side = L
                            else:
                                flag_side = R

                if flag_side == R:
                    this  = 1
                    other = 0
                else:
                    this  = 0
                    other = 1
                    
                app=[self.apright, self.apleft]
                
                #############################
                if mvtype == 0:
                    pass

                if mvtype == 1:
                    A  = self.coordunop(pos_last[:],shift,angle)
                    C  = self.coordunop(pos[:]     ,shift,angle)
                    cross = self.get_line_intersect(pos_last, pos, xsplit)

                    if len(cross) > 0: ### Line crosses boundary ###
                        B  = self.coordunop(cross[0]   ,shift,angle)
                        app[this] ( [mvtype,A,B,feed] )
                        app[other]( [mvtype,B,C,feed] )
                    else:
                        app[this] ( [mvtype,A,C,feed] )

                if mvtype == 2 or mvtype == 3:
                    A  = self.coordunop(pos_last[:],shift,angle)
                    C  = self.coordunop(pos[:]     ,shift,angle)
                    D  = self.coordunop(center     ,shift,angle)
                    cross = self.get_arc_intersects(pos_last[:], pos[:], xsplit, center[:], "G%d" %(mvtype))

                    if len(cross) > 0: ### Arc crosses boundary at least once ###
                        B  = self.coordunop(cross[0]   ,shift,angle)
                        #Check length of arc before writing
                        if sqrt((A[0]-B[0])**2 + (A[1]-B[1])**2) > self.accuracy:
                            app[this]( [mvtype,A,B,D,feed])
                            
                        if len(cross) == 1: ### Arc crosses boundary only once ###
                            #Check length of arc before writing
                            if sqrt((B[0]-C[0])**2 + (B[1]-C[1])**2) > self.accuracy:
                                app[other]([ mvtype,B,C,D, feed] )
                        if len(cross) == 2: ### Arc crosses boundary twice ###
                            E  = self.coordunop(cross[1],shift,angle)
                            #Check length of arc before writing
                            if sqrt((B[0]-E[0])**2 + (B[1]-E[1])**2) > self.accuracy:
                                app[other]([ mvtype,B,E,D, feed] )
                            #Check length of arc before writing
                            if sqrt((E[0]-C[0])**2 + (E[1]-C[1])**2) > self.accuracy:
                                app[this] ([ mvtype,E,C,D, feed] )
                    else: ### Arc does not cross boundary ###
                        app[this]([ mvtype,A,C,D, feed])

            ###############################################################################
            else:
                if passthru != '':
                    self.apboth(passthru)

    #######################################
    def probe_code(self,code2probe,nX,nY,probe_istep,minx,miny,xPartitionLength,yPartitionLength): #,Xoffset,Yoffset):
    #def probe_code(self,code2probe,nX,nY,probe_istep,minx,miny,xPartitionLength,yPartitionLength,Xoffset,Yoffset,Zoffset):
        #print "nX,nY =",nX,nY 
        probe_coords = []
        BPN=500
        POINT_LIST = [False for i in range(int((nY)*(nX)))]
        
        if code2probe == []:
            return 
        
        mvtype = -1  # G0 (Rapid), G1 (linear), G2 (clockwise arc) or G3 (counterclockwise arc).
        passthru = ""
        POS      = [0,0,0]
        feed     = 0
        out      = []

        min_length = min(xPartitionLength,yPartitionLength) / probe_istep
        if (min_length < Zero):
            min_length = max(xPartitionLength,yPartitionLength) / probe_istep
        if (min_length < Zero):
            min_length = 1

        for line in code2probe:
            if line[0] == 0  or line[0] == 1:
                mvtype   = line[0]
                POS_LAST = line[1][:]
                POS      = line[2][:]
                CENTER   = ['','','']
                if line[0] == 1:
                    feed     = line[3]

            elif line[0] == 3 or line[0] == 2:
                mvtype   = line[0]
                POS_LAST = line[1][:]
                POS      = line[2][:]
                CENTER   = line[3][:]
                feed     = line[4]
            else:
                mvtype  = -1
                passthru = line

            ###############################################################################
            if mvtype >= 0 and mvtype <=3:
                pos = POS[:]
                pos_last = POS_LAST[:]
                center = CENTER[:]
                
                #############################
                if mvtype == 0:
                    out.append( [mvtype,pos_last,pos] )
                    
                if mvtype == 1:
                    dx = pos[0]-pos_last[0]
                    dy = pos[1]-pos_last[1]
                    dz = pos[2]-pos_last[2]
                    length = sqrt(dx*dx + dy*dy)
                    if (length <= min_length):
                        out.append( [mvtype,pos_last,pos,feed] )
                    else:
                        Lsteps = max(2,int(ceil(length / min_length)))
                        xstp0 = float(pos_last[0])
                        ystp0 = float(pos_last[1])
                        zstp0 = float(pos_last[2])
                        for n in range(1,Lsteps+1):
                            xstp1 = n/float(Lsteps)*dx + pos_last[0] 
                            ystp1 = n/float(Lsteps)*dy + pos_last[1]
                            zstp1 = n/float(Lsteps)*dz + pos_last[2]
                            out.append( [mvtype,[xstp0,ystp0,zstp0],[xstp1,ystp1,zstp1],feed] )
                            xstp0 = float(xstp1)
                            ystp0 = float(ystp1)
                            zstp0 = float(zstp1)
                            
                if mvtype == 2 or mvtype == 3:
                    out.append( [ mvtype,pos_last,pos,center, feed] )                    
            ###############################################################################
            else:
                if passthru != '':
                    out.append(passthru)

        ################################
        ##  Loop through output to    ##
        ##  find needed probe points  ##
        ################################
        for i in range(len(out)):
            line = out[i]
            if line[0] == 0  or line[0] == 1:
                mvtype   = line[0]
                POS_LAST = line[1][:]
                POS      = line[2][:]
                CENTER   = ['','','']
                if line[0] == 1:
                    feed     = line[3]

            elif line[0] == 3 or line[0] == 2:
                mvtype   = line[0]
                POS_LAST = line[1][:]
                POS      = line[2][:]
                CENTER   = line[3][:]
                feed     = line[4]
            else:
                mvtype  = -1
                passthru = line

            if mvtype >= 1 and mvtype <=3:
                pos = POS[:]
                pos_last = POS_LAST[:]
                center = CENTER[:]


                #### ADD ADDITIONAL DATA TO POS_LAST DATA ####
                i_x,i_y = self.get_ix_iy((pos_last[0]-minx),(pos_last[1]-miny),xPartitionLength,yPartitionLength)
                #i_x = i_x+Xoffset
                #i_y = i_y+Yoffset
                if i_x < 0:
                    i_x=0
                if i_y < 0:
                    i_y=0
                if (i_x+1 >= nX):
                    i_x = nX-2
                    #i_x = i_x-1 #commented 02/22
                    #print "adjust i_x POS_LAST"
                i_x2 = i_x+1
                if (i_y+1 >= nY):
                    i_y = nY-2
                    #i_y = i_y-1  #commented 02/22
                    #print "adjust i_y POS_LAST"
                i_y2 = i_y+1
                
                p_index_A =  int(i_y* nX + i_x )
                p_index_B =  int(i_y2*nX + i_x )
                p_index_C =  int(i_y *nX + i_x2)
                p_index_D =  int(i_y2*nX + i_x2)                    
                
                Xfraction=((pos_last[0]-minx)-(i_x*xPartitionLength))/xPartitionLength
                Yfraction=((pos_last[1]-miny)-(i_y*yPartitionLength))/yPartitionLength

                if Xfraction>1.0:
                    #print "ERROR POS_LAST: Xfraction = ", Xfraction
                    Xfraction = 1.0
                if Xfraction <0.0:
                    #print "ERROR POS_LAST: Xfraction = ", Xfraction
                    Xfraction = 0.0
                if Yfraction > 1.0:
                    #print "ERROR POS_LAST: Yfraction = ", Yfraction
                    Yfraction = 1.0
                if Yfraction<0.0:
                    #print "ERROR POS_LAST: Yfraction = ", Yfraction
                    Yfraction = 0.0

                BPN=500
                out[i][1].append(p_index_A+BPN)
                out[i][1].append(p_index_B+BPN)
                out[i][1].append(p_index_C+BPN)
                out[i][1].append(p_index_D+BPN)
                out[i][1].append(Xfraction)
                out[i][1].append(Yfraction)

                try:
                    POINT_LIST[p_index_A ] = True
                    POINT_LIST[p_index_B ] = True
                    POINT_LIST[p_index_C ] = True
                    POINT_LIST[p_index_D ] = True
                except:
                    pass 
                #### ADD ADDITIONAL DATA TO POS_LAST DATA ####
                i_x,i_y = self.get_ix_iy((pos[0]-minx),(pos[1]-miny),xPartitionLength,yPartitionLength)
                #i_x = i_x+Xoffset
                #i_y = i_y+Yoffset
                if i_x < 0:
                    i_x=0
                if i_y < 0:
                    i_y=0
                if (i_x+1 >= nX):
                    i_x = nX-2
                    #i_x = i_x-1 #commented 02/22
                    #print "adjust i_x POS"
                i_x2 = i_x+1
                if (i_y+1 >= nY):
                    i_y = nY-2
                    #i_y = i_y-1#commented 02/22
                    #print "adjust i_y POS"
                i_y2 = i_y+1
                
                p_index_A =  int(i_y* nX + i_x )
                p_index_B =  int(i_y2*nX + i_x )
                p_index_C =  int(i_y *nX + i_x2)
                p_index_D =  int(i_y2*nX + i_x2)
                Xfraction=((pos[0]-minx)-(i_x*xPartitionLength))/xPartitionLength
                Yfraction=((pos[1]-miny)-(i_y*yPartitionLength))/yPartitionLength
                
                if Xfraction>1.0:
                    Xfraction = 1.0
                    #print "ERROR POS: Xfraction = ", Xfraction
                if Xfraction <0.0:
                    Xfraction = 0.0
                    #print "ERROR POS: Xfraction = ", Xfraction
                if Yfraction > 1.0:
                    Yfraction = 1.0
                    #print "ERROR POS: Yfraction = ", Yfraction
                if Yfraction<0.0:
                    Yfraction = 0.0
                    #print "ERROR POS: Yfraction = ", Yfraction
                    
                out[i][2].append(p_index_A+BPN)
                out[i][2].append(p_index_B+BPN)
                out[i][2].append(p_index_C+BPN)
                out[i][2].append(p_index_D+BPN)
                out[i][2].append(Xfraction)
                out[i][2].append(Yfraction)
                try:
                    POINT_LIST[p_index_A ] = True
                    POINT_LIST[p_index_B ] = True
                    POINT_LIST[p_index_C ] = True
                    POINT_LIST[p_index_D ] = True
                except:
                    pass
        self.probe_gcode = out
        #for line in out:
        #    print line
        
        ################################
        ##  Generate Probing Code     ##
        ##  For needed points         ##
        ################################

        for i in range(len(POINT_LIST)):
            i_x = i % nX
            i_y = int(i / nX)
            xp  = i_x * xPartitionLength + minx
            yp  = i_y * yPartitionLength + miny
            probe_coords.append([POINT_LIST[i],i+BPN,xp,yp])

        self.probe_coords = probe_coords
        return

    def get_ix_iy(self,x,y,xPartitionLength,yPartitionLength):
        i_x=int(x/xPartitionLength)
        i_y=int(y/yPartitionLength)
        return i_x,i_y

    ####################################### 
    def scale_rotate_code(self,code2scale,scale=[1.0,1.0,1.0,1.0],angle=0.0):
        if code2scale == []:
            return code2scale,0,0,0,0,0,0
        minx =  99999
        maxx = -99999
        miny =  99999
        maxy = -99999
        minz =  99999
        maxz = -99999
        mvtype = -1  # G0 (Rapid), G1 (linear), G2 (clockwise arc) or G3 (counterclockwise arc).

        passthru = ""
        POS     =[0,0,0]
        feed = 0
        out = []

        L = 0
        R = 1
        flag_side = 1  

        for line in code2scale:
            if line[0] == 0  or line[0] == 1:
                mvtype   = line[0]
                POS_LAST = line[1][:]
                POS      = line[2][:]
                CENTER   = ['','','']
                if line[0] == 1:
                    feed     = line[3] * scale[3]

            elif line[0] == 3 or line[0] == 2:
                mvtype   = line[0]
                POS_LAST = line[1][:]
                POS      = line[2][:]
                CENTER   = line[3][:]
                feed     = line[4] * scale[3]
            else:
                mvtype  = -1
                passthru = line

            ###############################################################################
            if mvtype >= 0 and mvtype <=3:

                pos      = self.scale_rot_coords(POS,scale,angle)
                pos_last = self.scale_rot_coords(POS_LAST,scale,angle)


                if CENTER[0]!='' and CENTER[1]!='':
                    center = self.scale_rot_coords(CENTER,scale,angle)
                else:
                    center = CENTER
                 
                #############################
                if mvtype != 0:
                    try:
                        minx = min( minx, min(pos[0],pos_last[0]) ) 
                        maxx = max( maxx, max(pos[0],pos_last[0]) )
                    except:
                        pass
                    try:
                        miny = min( miny, min(pos[1],pos_last[1]) ) 
                        maxy = max( maxy, max(pos[1],pos_last[1]) )
                    except:
                        pass
                    try:
                        minz = min( minz, min(pos[2],pos_last[2]) ) 
                        maxz = max( maxz, max(pos[2],pos_last[2]) )
                    except:
                        pass

                if mvtype == 0:
                    out.append( [mvtype,pos_last,pos] )
                
                if mvtype == 1:
                    out.append( [mvtype,pos_last,pos,feed] )

                if mvtype == 2 or mvtype == 3:
                    out.append( [ mvtype,pos_last,pos,center, feed] )
                    
                    if mvtype == 3:
                        ang1 = self.Get_Angle2(pos_last[0]-center[0],pos_last[1]-center[1])
                        xtmp,ytmp = self.Transform(pos[0]-center[0],pos[1]-center[1],radians(-ang1))
                        ang2 = self.Get_Angle2(xtmp,ytmp)

                    else:
                        ang1 = self.Get_Angle2(pos[0]-center[0],pos[1]-center[1])
                        xtmp,ytmp = self.Transform(pos_last[0]-center[0],pos_last[1]-center[1],radians(-ang1))
                        ang2 = self.Get_Angle2(xtmp,ytmp)
                        
                    if ang2 == 0:
                        ang2=359.999
                        
                    Radius = sqrt( (pos[0]-center[0])**2 +(pos[1]-center[1])**2 )
                            
                    if ang1 > 270:
                        da = 270
                    elif ang1 > 180:
                        da = 180
                    elif ang1 > 90:
                        da = 90
                    else:
                        da = 0
                    for side in [90,180,270,360]:
                        spd = side + da
                        if ang2 > (spd-ang1):
                            if spd > 360:
                                spd=spd-360
                            if spd==90:
                                maxy = max( maxy, center[1]+Radius )
                            if spd==180:
                                minx = min( minx, center[0]-Radius)
                            if spd==270:
                                miny = min( miny, center[1]-Radius )
                            if spd==360:
                                maxx = max( maxx, center[0]+Radius )           
            ###############################################################################
            else:
                if passthru != '':
                    out.append(passthru)
                    
        return out,minx,maxx,miny,maxy,minz,maxz


    #######################################
    def scale_translate(self,code2translate,translate=[0.0,0.0,0.0]):
        
        if translate[0]==0 and translate[1]==0 and translate[2]==0:
            return code2translate
        
        mvtype = -1  # G0 (Rapid), G1 (linear), G2 (clockwise arc) or G3 (counterclockwise arc).
        passthru = ""
        POS     =[0,0,0]
        pos     =[0,0,0]
        pos_last=[0,0,0]
        feed = 0
        out = []

        L = 0
        R = 1
        flag_side = 1  

        for line in code2translate:
            if line[0] == 1 or line[0] == 0:
                mvtype   = line[0]
                POS_LAST = line[1][:]
                POS      = line[2][:]
                CENTER   = ['','','']
                if line[0] == 1:
                    feed     = line[3]

            elif line[0] == 3 or line[0] == 2:
                mvtype   = line[0]
                POS_LAST = line[1][:]
                POS      = line[2][:]
                CENTER   = line[3][:]
                feed     = line[4]
            else:
                mvtype  = -1
                passthru = line

            ###############################################################################
            if mvtype >= 0 and mvtype <=3:
                pos      = self.scale_trans_coords(POS,translate)
                pos_last      = self.scale_trans_coords(POS_LAST,translate)
                if CENTER[0]!='' and CENTER[1]!='':
                    center      = self.scale_trans_coords(CENTER,translate)
                else:
                    center = CENTER[:]
                 
                #############################
                if mvtype == 0:
                    out.append( [mvtype,pos_last,pos] )
                
                if mvtype == 1:
                    out.append( [mvtype,pos_last,pos,feed] )

                if mvtype == 2 or mvtype == 3:
                    out.append( [ mvtype,pos_last,pos,center, feed] )
            ###############################################################################
            else:
                if passthru != '':
                    out.append(passthru)
        return out

    def scale_trans_coords(self,coords,trans):
        x = coords[0] - trans[0]
        y = coords[1] - trans[1]
        z = coords[2] - trans[2]
        return [x,y,z]

    def scale_rot_coords(self,coords,scale,rot):
        x = coords[0] * scale[0]
        y = coords[1] * scale[1]
        z = coords[2] * scale[2]

        x,y = self.Transform(x,y, radians(rot) )
        return [x,y,z]
    
    def generate_probing_gcode(self,
                               probe_coords,
                               probe_safe,
                               probe_feed,
                               probe_depth,
                               pre_codes=" ",
                               pause_codes=" ",
                               probe_offsetX=0.0,
                               probe_offsetY=0.0,
                               probe_offsetZ=0.0,
                               probe_soft="LinuxCNC",
                               close_file = False,
                               postamble=" ",
                               savepts=1,
                               allpoints=1):

        savepts = savepts or close_file
        ################################
        ##  Generate Probing Code     ##
        ##  For needed points         ##
        ################################
        g_code = []
        g_code.append("%")
        g_code.append("( G-Code Modified by G-Code Ripper                        )")
        g_code.append("( by Scorch - 2013-2021 www.scorchworks.com                    )")
        if self.units == "in":
            g_code.append("G20   (set units to inches)")
        else:
            g_code.append("G21   (set units to mm)")

        Gprobe      = "G38.2"
        ZProbeValue = "#5422"
        datafileopen ="(Insert code to open data file for writing here.)"
        datafileclose ="(Insert code to close data file for writing here.)"
        datafileclose
        if (probe_soft=="LinuxCNC"):
            Gprobe       = "G38.2"
            ZProbeValue  = "#5422"
            if savepts:
                datafileopen = "(PROBEOPEN probe_points.txt)"
                datafileclose= "(PROBECLOSE)"
                msg =       "The Probe point data from LinuxCNC will\n"
                msg = msg + "be witten to a file named 'probe_points.txt'.\n"
                msg = msg + "The file will be located in the linuxcnc\n"
                msg = msg + "configuration folder."
                message_box("Probe Data File",msg)
        
        elif (probe_soft=="MACH3"):
            if savepts:
                datafileopen = "M40"
                datafileclose= "M41"
            Gprobe       = "G31"
            ZProbeValue  = "#2002"
            
        elif (probe_soft=="MACH4"):
            if savepts:
                datafileopen = "M40"
                datafileclose= "M41"
            Gprobe       = "G31"
            ZProbeValue  = "#5063"  # The Z value variable changed from #2002 to #5063 in Mach4

        elif (probe_soft=="DDCS"):
            if savepts:
                datafileopen = "ClearCoords[0]"
                datafileclose= ""
                msg =       "The Probe point data from DDCS will\n"
                msg = msg + "be witten to a file named 'ProbeMap0.txt'."
                message_box("Probe Data File",msg)
            Gprobe="M101\nG01"
            ZProbeValue = "#701"
        
        elif (probe_soft=="GRBL"):
            #need to figure out if GRBL can do this
            #Sadly GRBL cannot work this way
            pass
            
        g_code.append("G90")

        for line in pre_codes.split('|'):
            g_code.append(line)
            
        g_code.append(datafileopen)		
        max_probe_safe = max(probe_safe,probe_safe+probe_offsetZ)

        g_code.append("G0 Z%.3f" %(max_probe_safe))
        xp = 0.0
        yp = 0.0
        if (probe_soft=="LinuxCNC"):
            g_code.append("#499 = %.3f" %(probe_depth))
        else:
            g_code.append("#499 = %.3f" %(probe_safe))
        #g_code.append("(PRINT,value of variable 499 INIT is: #499)")
        for i in range(len(probe_coords)):
            if (probe_coords[i][0] or allpoints): #or close_file):
                xp = probe_coords[i][2]
                yp = probe_coords[i][3]
                
                g_code.append("G0 X%.3fY%.3f" %(xp+probe_offsetX,yp+probe_offsetY))
                g_code.append("%s Z%.3f F%.1f" %(Gprobe, probe_depth+probe_offsetZ,probe_feed))

                if (probe_soft=="DDCS"):
                    g_code.append("M102")
                    g_code.append("G04P0")
                    g_code.append("RecordCoords[0,#699,#700,#701,#702]")
                    
                if (close_file == False):
                    if (probe_offsetZ==0.0):
                        g_code.append("#%d = %s" %(probe_coords[i][1],ZProbeValue))
                    else:
                        g_code.append("#%d = [%s-%.4f]" %(probe_coords[i][1],ZProbeValue,probe_offsetZ))
                if (probe_soft=="LinuxCNC"):
                    g_code.append("#499= [ [[#%d GE #499]*#%d] + [[#%d LT #499]*#499] ]" %(probe_coords[i][1],probe_coords[i][1],probe_coords[i][1]))
                #g_code.append("(PRINT,value of variable 499 is: #499)")
                g_code.append("G0 Z%.3f" %(probe_safe+probe_offsetZ) )

        g_code.append("G0 Z%.3f" %(max_probe_safe))
        g_code.append("G0 X%.3fY%.3f" %(xp,yp))

        g_code.append(datafileclose)
        for line in pause_codes.split('|'):
            g_code.append(line)


        if (close_file == False):
            g_code.append("M0 (PAUSE PROGRAM)")
        else:
            for entry in postamble.split('|'):
                g_code.append(entry)
            g_code.append("M2")
#DEBUG
            g_code.append("%")            
        return g_code
    
###
###
    def generategcode_probe(self,side,z_safe=.5,
                      plunge_feed=10.0,
                      no_variables=False,
                      Rstock=0.0,
                      Wrap="XYZ",
                      preamble="",
                      postamble="",
                      PLACES_L=4,
                      PLACES_R=3,
                      PLACES_F=1,
                      WriteAll=False,
                      FSCALE="Scale-Rotary",
                      Reverse_Rotary = False,
                      NoComments=False,
                      probe_data=[],
                      probe_offsetZ=0,
                      probe_safe=.5):

        g_code = []

        sign = 1
        if Reverse_Rotary:
            sign = -1

        self.MODAL_VAL={'X':" ", 'Y':" ", 'Z':" ", 'F':" ", 'A':" ", 'B':" ", 'I':" ", 'J':" "}
        LASTX = 0
        LASTY = 0
        LASTZ = z_safe

        g_code.append("( G-Code Modified by G-Code Ripper                        )")
        g_code.append("( by Scorch - 2013-2021 www.scorchworks.com                    )")
        AXIS=["X"     , "Y"     , "Z"     ]
        DECP=[PLACES_L, PLACES_L, PLACES_L]  
            
        g_code.append("G90   (set absolute distance mode)")
        g_code.append("G90.1 (set absolute distance mode for arc centers)")
        g_code.append("G17   (set active plane to XY)")
        
        if self.units == "in":
            g_code.append("G20   (set units to inches)")
        else:
            g_code.append("G21   (set units to mm)")
            
        if no_variables==False:
            g_code.append("#<z_safe> = % 5.3f " %(z_safe))
            g_code.append("#<plunge_feed> = % 5.0f " %(plunge_feed))
            
        for line in preamble.split('|'):
            g_code.append(line)

        g_code.append("(---------------------------------------------------------)")
        ###################
        ## GCODE WRITING ##
        ###################
        Z_probe_max = probe_offsetZ
        if probe_data!=[]:
            try:
                Z_probe_max = probe_data[1][2] #set initial max value
            except:
                Z_probe_max = 0
            for point_data in probe_data:
                if point_data[2] > Z_probe_max:
                    Z_probe_max=point_data[2]

        First_Z_Safe = 0+0j
        for line in side:
            if line[0] == 0:
                if (not isinstance(line[1][2], complex)):
                    First_Z_Safe = line[1][2]
                    break
            
        for line in side:
            if line[0] == 1 or line[0] == 2 or line[0] == 3 or (line[0] == 0):
                D0 = line[2][0]-line[1][0] 
                D1 = line[2][1]-line[1][1] 
                D2 = line[2][2]-line[1][2]
                D012 = sqrt((D0+0j).real**2+(D1+0j).real**2+(D2+0j).real**2)
                
                coordA=[ line[1][0], line[1][1], line[1][2] ]
                coordB=[ line[2][0], line[2][1], line[2][2] ]
                
                dx = coordA[0]-LASTX
                dy = coordA[1]-LASTY
                dz = coordA[2]-LASTZ

                LASTX = coordB[0]
                LASTY = coordB[1]
                LASTZ = coordB[2]
                
            if (line[0] == 1) or (line[0] == 2) or (line[0] == 3):
                Feed_adj = line[3]
                
                LINE = "G%d" %(line[0])
                if probe_data!=[]:
                    # Write probe adjusted values
                    Z1 = probe_data[line[2][3]-500][2] 
                    Z2 = probe_data[line[2][4]-500][2] 
                    Z3 = probe_data[line[2][5]-500][2] 
                    Z4 = probe_data[line[2][6]-500][2] 
                    F1 = line[2][7]
                    F2 = line[2][8]

                    v102 = Z1 + F2*Z2 - F2*Z1
                    v101 = Z3 + F2*Z4 - F2*Z3
                    v100 = v102 + F1*v101 - F1*v102
                    Z_calculated = coordB[2] + v100 - probe_offsetZ
                    
                    LINE = self.app_gcode_line(LINE,AXIS[0],coordB[0],DECP[0],WriteAll)
                    LINE = self.app_gcode_line(LINE,AXIS[1],coordB[1],DECP[1],WriteAll)
                    LINE = self.app_gcode_line(LINE,AXIS[2],Z_calculated,DECP[2],WriteAll)                    
                    LINE = self.app_gcode_line(LINE,"F",Feed_adj  ,PLACES_F,WriteAll)
                else:
                    g_code.append("#102 = [#%d + %.3f*#%d - %.3f*#%d]" %(line[2][3],line[2][8],line[2][4],line[2][8],line[2][3]))
                    g_code.append("#101 = [#%d + %.3f*#%d - %.3f*#%d]" %(line[2][5],line[2][8],line[2][6],line[2][8],line[2][5]))
                    g_code.append("#100 = [#102+ %.3f*#101- %.3f*#102]"%(line[2][7],line[2][7]))

                    LINE = self.app_gcode_line(LINE,AXIS[0],coordB[0],DECP[0],WriteAll)
                    LINE = self.app_gcode_line(LINE,AXIS[1],coordB[1],DECP[1],WriteAll)
                    # Do not delete the following line that sets junk value
                    junk = self.app_gcode_line(LINE,AXIS[2],coordB[2],DECP[2],WriteAll)
                    LINE = LINE + " Z[%.3f+#100] " %(coordB[2])
                    LINE = self.app_gcode_line(LINE,"F",Feed_adj  ,PLACES_F,WriteAll)

                g_code.append(LINE)
                
            elif (line[0] == 0):
                LINE = "G%d" %(line[0])
                LINE = self.app_gcode_line(LINE,AXIS[0],coordB[0],DECP[0],WriteAll)
                LINE = self.app_gcode_line(LINE,AXIS[1],coordB[1],DECP[1],WriteAll)
                if probe_data!=[]:
                    LINE = self.app_gcode_line(LINE,AXIS[2],coordB[2]+Z_probe_max-probe_offsetZ,DECP[2],WriteAll)
                else:
                    if (not isinstance(coordB[2], complex)):
                        LINE = LINE + " %s[%.3f+#499]" %(AXIS[2],coordB[2])
                    else:
                        LINE = LINE + " %s[%.3f+#499]" %(AXIS[2],First_Z_Safe)
                
                g_code.append(LINE)

            elif line[0] == ";":
                if not NoComments:
                    g_code.append("%s" %(line[1]))
                
            elif line[0] == "M2":
                for entry in postamble.split('|'):
                    g_code.append(entry)
            else:
                g_code.append(line)

        ########################
        ## END G-CODE WRITING ##
        ########################
        return g_code

###
###
    def generategcode(self,side,z_safe=.5,
                      plunge_feed=10.0,
                      no_variables=False,
                      Rstock=0.0,
                      Wrap="XYZ",
                      preamble="",
                      postamble="",
                      gen_rapids=False,
                      PLACES_L=4,
                      PLACES_R=3,
                      PLACES_F=1,
                      WriteAll=False,
                      FSCALE="Scale-Rotary",
                      Reverse_Rotary = False,
                      NoComments=False):

        g_code = []

        sign = 1
        if Reverse_Rotary:
            sign = -1

        self.MODAL_VAL={'X':" ", 'Y':" ", 'Z':" ", 'F':" ", 'A':" ", 'B':" ", 'I':" ", 'J':" "}
        LASTX = 0
        LASTY = 0
        LASTZ = z_safe

        g_code.append("( G-Code Modified by G-Code Ripper                        )")
        g_code.append("( by Scorch - 2013-2021 www.scorchworks.com                    )")
        if Wrap == "XYZ":
            AXIS=["X"     , "Y"     , "Z"     ]
            DECP=[PLACES_L, PLACES_L, PLACES_L]  
        elif Wrap == "Y2A":
            AXIS=["X"     , "A"     , "Z"     ]
            DECP=[PLACES_L, PLACES_R, PLACES_L]
            WriteAll=False
            g_code.append("(G-Code Ripper has mapped the Y-Axis to the A-Axis      )")
        elif Wrap == "X2B":
            AXIS=["B"     , "Y"     , "Z"     ]
            DECP=[PLACES_R, PLACES_L, PLACES_L]
            WriteAll=False
            g_code.append("(G-Code Ripper has mapped the X-Axis to the B-Axis      )")
        elif Wrap == "Y2B":
            AXIS=["X"     , "B"     , "Z"     ]
            DECP=[PLACES_L, PLACES_R, PLACES_L]
            WriteAll=False
            g_code.append("(G-Code Ripper has mapped the Y-Axis to the B-Axis      )")
        elif Wrap == "X2A":
            AXIS=["A"     , "Y"     , "Z"     ]
            DECP=[PLACES_R, PLACES_L, PLACES_L]
            WriteAll=False
            g_code.append("(G-Code Ripper has mapped the X-Axis to the A-Axis      )")
            
        if Wrap != "XYZ":
            g_code.append("(A nominal stock radius of %f was used.             )" %(Rstock))
            g_code.append("(Z-axis zero position is the surface of the round stock.  )")
            g_code.append("(---------------------------------------------------------)")
            
        g_code.append("G90   (set absolute distance mode)")
        g_code.append("G90.1 (set absolute distance mode for arc centers)")
        g_code.append("G17   (set active plane to XY)")
        
        if self.units == "in":
            g_code.append("G20   (set units to inches)")
        else:
            g_code.append("G21   (set units to mm)")
            
        if no_variables==False:
            g_code.append("#<z_safe> = % 5.3f " %(z_safe))
            g_code.append("#<plunge_feed> = % 5.0f " %(plunge_feed))
            
        for line in preamble.split('|'):
            g_code.append(line)

        g_code.append("(---------------------------------------------------------)")
        ###################
        ## GCODE WRITING ##
        ###################
        for line in side:
            if line[0] == 1 or line[0] == 2 or line[0] == 3 or (line[0] == 0 and gen_rapids == False):
                D0 = line[2][0]-line[1][0] 
                D1 = line[2][1]-line[1][1] 
                D2 = line[2][2]-line[1][2]
                D012 = sqrt((D0+0j).real**2+(D1+0j).real**2+(D2+0j).real**2)
                
                coordA=[ line[1][0], line[1][1], line[1][2] ]
                coordB=[ line[2][0], line[2][1], line[2][2] ]
                if Wrap == "Y2A" or Wrap == "Y2B":
                    #if line[1][1].imag == 0:
                    if (not isinstance(line[1][1], complex)):
                        coordA[1]=sign*degrees(line[1][1]/Rstock)
                    #if line[2][1].imag == 0:
                    if (not isinstance(line[2][1], complex)):
                        coordB[1]=sign*degrees(line[2][1]/Rstock)
                elif Wrap == "X2B" or Wrap == "X2A":
                    #if line[1][0].imag == 0:
                    if (not isinstance(line[1][0], complex)):
                        coordA[0]=sign*degrees(line[1][0]/Rstock)
                    #if line[2][0].imag == 0:
                    if (not isinstance(line[2][0], complex)):
                        coordB[0]=sign*degrees(line[2][0]/Rstock)

                dx = coordA[0]-LASTX
                dy = coordA[1]-LASTY
                dz = coordA[2]-LASTZ

                # Check if next point is coincident with the
                # current point within the set accuracy
                if sqrt((dx+0j).real**2 + (dy+0j).real**2 + (dz+0j).real**2) > self.accuracy and gen_rapids == True:
                    ### Move tool to safe height (z_safe) ###
                    if no_variables==False:
                        g_code.append("G0 %c #<z_safe> " %(AXIS[2]) )
                        self.MODAL_VAL[AXIS[2]] = z_safe
                    else:
                        LINE = "G0"
                        LINE = self.app_gcode_line(LINE,AXIS[2],z_safe,DECP[2],WriteAll)
                        if len(LINE) > 2: g_code.append(LINE)
                    
                    ### Move tool to coordinates of next cut ###
                    LINE = "G0"
                    LINE = self.app_gcode_line(LINE,AXIS[0],coordA[0],DECP[0],WriteAll)
                    LINE = self.app_gcode_line(LINE,AXIS[1],coordA[1],DECP[1],WriteAll)
                    if len(LINE) > 2: g_code.append(LINE)

                    if float(coordA[2]) < float(z_safe):
                        if no_variables==False:
                            LINE = "G1"
                            LINE = self.app_gcode_line(LINE,AXIS[2],coordA[2],DECP[2],WriteAll)
                            LINE = LINE + " F #<plunge_feed>"
                            self.MODAL_VAL["F"] = plunge_feed
                            if len(LINE) > 2: g_code.append(LINE)                 
                        else:
                            LINE = "G1"
                            LINE = self.app_gcode_line(LINE,AXIS[2],coordA[2]  ,DECP[2]  , WriteAll)
                            LINE = self.app_gcode_line(LINE,"F"    ,plunge_feed,PLACES_F, WriteAll)
                            if len(LINE) > 2: g_code.append(LINE)

                LASTX = coordB[0]
                LASTY = coordB[1]
                LASTZ = coordB[2]

            if (line[0] == 1) or (line[0] == 2) or (line[0] == 3) or (line[0] == 0 and (gen_rapids == False)):
                try:    LAST0 = float(self.MODAL_VAL[AXIS[0]])
                except: LAST0 = coordB[0]
                try:    LAST1 = float(self.MODAL_VAL[AXIS[1]])
                except: LAST1 = coordB[1]
                try:    LAST2 = float(self.MODAL_VAL[AXIS[2]])
                except: LAST2 = coordB[2]
                
                LINE = "G%d" %(line[0])
                LINE = self.app_gcode_line(LINE,AXIS[0],coordB[0],DECP[0],WriteAll)
                LINE = self.app_gcode_line(LINE,AXIS[1],coordB[1],DECP[1],WriteAll)
                LINE = self.app_gcode_line(LINE,AXIS[2],coordB[2],DECP[2],WriteAll)
                if (line[0] == 1):
                    if ((LINE.find("A") > -1) or (LINE.find("B") > -1)) and (FSCALE == "Scale-Rotary") and (D012>self.Zero):
                        if (LINE.find("X") > -1) or (LINE.find("Y") > -1) or (LINE.find("Z") > -1):
                            if Wrap == "Y2A" or Wrap == "Y2B":
                                Df = hypot( coordB[0]-LAST0, coordB[2]-LAST2 )
                            elif Wrap == "X2B" or Wrap == "X2A":
                                Df = hypot( coordB[1]-LAST1, coordB[2]-LAST2 )
                            Feed_adj = abs(Df / (D012/line[3]))
                        else:
                            if Wrap == "Y2A" or Wrap == "Y2B":
                                DAf = coordB[1]-LAST1
                                Feed_adj = abs(DAf / (D012/line[3]))
                            elif Wrap == "X2B" or Wrap == "X2A":
                                DAf = coordB[0]-LAST0
                                Feed_adj = abs(DAf / (D012/line[3]))
                    else:
                        Feed_adj = line[3]
                    LINE = self.app_gcode_line(LINE,"F",Feed_adj  ,PLACES_F,WriteAll)
                    
                elif (line[0] == 2) or (line[0] == 3):
                    Feed_adj = line[4]
                    LINE = self.app_gcode_line(LINE,"I",line[3][0],DECP[0]  ,WriteAll)
                    LINE = self.app_gcode_line(LINE,"J",line[3][1],DECP[1]  ,WriteAll)
                    LINE = self.app_gcode_line(LINE,"F",Feed_adj  ,PLACES_F,WriteAll)
                if len(LINE) > 2: g_code.append(LINE)

            elif (line[0] == 0 and gen_rapids == True):
                pass

            elif line[0] == ";":
                if not NoComments:
                    g_code.append("%s" %(line[1]))
                
            elif line[0] == "M2":
                if gen_rapids == True:
                    if no_variables==False:
                        g_code.append("G0 %c #<z_safe> " %(AXIS[2]) )
                        self.MODAL_VAL[AXIS[2]] = z_safe
                    else:
                        LINE = "G0"
                        LINE = self.app_gcode_line(LINE,AXIS[2],z_safe,DECP[2],WriteAll)
                        g_code.append(LINE)
                        
                for entry in postamble.split('|'):
                    g_code.append(entry)
                #g_code.append(line[0])
            else:
                g_code.append(line)
        ########################
        ## END G-CODE WRITING ##
        ########################
        return g_code

    
    ##################################################
    ###  Begin Dxf_Write G-Code Writing Function   ###
    ##################################################
    def generate_dxf_write_gcode(self,side,Rapids=True):
        g_code = []
        # Create a header section just in case the reading software needs it
        g_code.append("999")
        g_code.append("DXF created by G-Code Ripper <by Scorch, www.scorchworks.com>")
        
        g_code.append("0")
        g_code.append("SECTION")
        g_code.append("2")
        g_code.append("HEADER")
        g_code.append("0")
        g_code.append("ENDSEC")
        #         
        #Tables Section
        #These can be used to specify predefined constants, line styles, text styles, view 
        #tables, user coordinate systems, etc. We will only use tables to define some layers 
        #for use later on. Note: not all programs that support DXF import will support 
        #layers and those that do usually insist on the layers being defined before use
        #
        # The following will initialise layers 1 and 2 for use with moves and rapid moves.
        g_code.append("0")
        g_code.append("SECTION")
        g_code.append("2")
        g_code.append("TABLES")
        g_code.append("0")
        g_code.append("TABLE")
        g_code.append("2")
        g_code.append("LTYPE")
        g_code.append("70")
        g_code.append("1")
        g_code.append("0")
        g_code.append("LTYPE")
        g_code.append("2")
        g_code.append("CONTINUOUS")
        g_code.append("70")
        g_code.append("64")
        g_code.append("3")
        g_code.append("Solid line")
        g_code.append("72")
        g_code.append("65")
        g_code.append("73")
        g_code.append("0")
        g_code.append("40")
        g_code.append("0.000000")
        g_code.append("0")
        g_code.append("ENDTAB")
        g_code.append("0")
        g_code.append("TABLE")
        g_code.append("2")
        g_code.append("LAYER")
        g_code.append("70")
        g_code.append("6")
        g_code.append("0")
        g_code.append("LAYER")
        g_code.append("2")
        g_code.append("1")
        g_code.append("70")
        g_code.append("64")
        g_code.append("62")
        g_code.append("7")
        g_code.append("6")
        g_code.append("CONTINUOUS")
        g_code.append("0")
        g_code.append("LAYER")
        g_code.append("2")
        g_code.append("2")
        g_code.append("70")
        g_code.append("64")
        g_code.append("62")
        g_code.append("7")
        g_code.append("6")
        g_code.append("CONTINUOUS")
        g_code.append("0")
        g_code.append("ENDTAB")
        g_code.append("0")
        g_code.append("TABLE")
        g_code.append("2")
        g_code.append("STYLE")
        g_code.append("70")
        g_code.append("0")
        g_code.append("0")
        g_code.append("ENDTAB")
        g_code.append("0")
        g_code.append("ENDSEC")
        
        #This block section is not necessary but apparently it's good form to include one anyway.
        #The following is an empty block section.
        g_code.append("0")
        g_code.append("SECTION")
        g_code.append("2")
        g_code.append("BLOCKS")
        g_code.append("0")
        g_code.append("ENDSEC")

        # Start entities section
        g_code.append("0")
        g_code.append("SECTION")
        g_code.append("2")
        g_code.append("ENTITIES")
        g_code.append("  0")

        #################################
        ## GCODE WRITING for Dxf_Write ##
        #################################
        for line in side:
            if line[0] == 1 or (line[0] == 0 and Rapids):
                g_code.append("LINE")
                g_code.append("  5")
                g_code.append("30")
                g_code.append("100")
                g_code.append("AcDbEntity")
                g_code.append("  8") #layer Code #g_code.append("0")
                if line[0] == 1:
                    g_code.append("1")
                else:
                    g_code.append("2")    
                g_code.append(" 62") #color code
                if line[0] == 1:
                    g_code.append("10")
                else:
                    g_code.append("150")
                g_code.append("100")
                g_code.append("AcDbLine")
                g_code.append(" 10")
                g_code.append("%.4f" %((line[1][0]+0j).real)) #x1 coord
                g_code.append(" 20")
                g_code.append("%.4f" %((line[1][1]+0j).real)) #y1 coord
                g_code.append(" 30")
                g_code.append("%.4f" %((line[1][2]+0j).real)) #z1 coord
                g_code.append(" 11")
                g_code.append("%.4f" %((line[2][0]+0j).real)) #x2 coord
                g_code.append(" 21")
                g_code.append("%.4f" %((line[2][1]+0j).real)) #y2 coord
                g_code.append(" 31")
                g_code.append("%.4f" %((line[2][2]+0j).real)) #z2 coord
                g_code.append("  0")

        g_code.append("ENDSEC")
        g_code.append("0")
        g_code.append("EOF")
        ######################################
        ## END G-CODE WRITING for Dxf_Write ##
        ######################################
        return g_code
    ##################################################
    ###    End Dxf_Write G-Code Writing Function   ###
    ##################################################

    #####################################
    ###  Begin CSV Writing Function   ###
    #####################################
    def generate_csv_write_gcode(self,side,Rapids=True):
        g_code = []
        mv_type = 1
        g_code.append("Type,X,Y,Z")
        for line in side:
            type_last  = mv_type
            if (line[0] == 1) or (line[0] == 0 and Rapids):
                mv_type = line[0]
                if type_last != mv_type:
                    g_code.append("%d,%.4f,%.4f,%.4f" %(mv_type,
                                                  (line[1][0]+0j).real,
                                                  (line[1][1]+0j).real,
                                                  (line[1][2]+0j).real))
                g_code.append("%d,%.4f,%.4f,%.4f" %(mv_type,
                                              (line[2][0]+0j).real,
                                              (line[2][1]+0j).real,
                                              (line[2][2]+0j).real))
            elif line[0] == 0:
                g_code.append(",,,")
            # end for line in side               
        return g_code
    
    #####################################
    ###    End CSV Writing Function   ###
    #####################################

    
    def generate_round_gcode(self,
                      Lmin = 0.0,
                      Lmax = 3.0,
                      cut_depth = 0.03,
                      tool_dia = .25,
                      step_over = 25.0,
                      feed = 20,
                      plunge_feed=10.0,
                      z_safe=.5,
                      no_variables=False,
                      Rstock=0.0,
                      Wrap="XYZ",
                      preamble="",
                      postamble="",
                      PLACES_L=4,
                      PLACES_R=3,
                      PLACES_F=1,
                      climb_mill=False,
                      Reverse_Rotary = False,
                      FSCALE="Scale-Rotary"):

        g_code = []
        Feed_adj = feed
        if PLACES_F > 0:
            FORMAT_FEED = "%% .%df" %(PLACES_F)
        else:
            FORMAT_FEED = "%d"
            
        if Lmin < Lmax:
            Lmin_tp = Lmin + tool_dia/2.0
            Lmax_tp = Lmax - tool_dia/2.0
        else:
            Lmin_tp = Lmax + tool_dia/2.0
            Lmax_tp = Lmin - tool_dia/2.0
        
        sign = 1
        if Reverse_Rotary:
            sign = -1 * sign

        if not climb_mill:
            sign = -1 * sign

        g_code.append("( G-Code Generated by G-Code Ripper                       )")
        g_code.append("( by Scorch - 2013-2021 www.scorchworks.com                    )")
        if Lmax_tp-Lmin_tp < tool_dia:
            g_code.append("( Tool diameter too large for defined cleanup area.       )")
            return g_code
        
        if Wrap == "XYZ":
            return
        elif Wrap == "Y2A":
            LINEAR="X"
            ROTARY="A"
            g_code.append("( Rounding A-axis, Linear axis is X-axis)")
        elif Wrap == "X2B":
            LINEAR="Y"
            ROTARY="B"
            g_code.append("( Rounding B-axis, Linear axis is Y-axis)")
        elif Wrap == "Y2B":
            LINEAR="X"
            ROTARY="B"
            g_code.append("( Rounding B-axis, Linear axis is X-axis)")
        elif Wrap == "X2A":
            LINEAR="Y"
            ROTARY="A"
            g_code.append("( Rounding A-axis, Linear axis is Y-axis)")
            
        g_code.append("(A nominal stock radius of %f was used.             )" %(Rstock))
        g_code.append("(Z-axis zero position is the surface of the round stock.  )")
        g_code.append("(---------------------------------------------------------)")    
        g_code.append("G90   (set absolute distance mode)")
        g_code.append("G90.1 (set absolute distance mode for arc centers)")
        g_code.append("G17   (set active plane to XY)")
        
        if self.units == "in":
            g_code.append("G20   (set units to inches)")
        else:
            g_code.append("G21   (set units to mm)")
            
        if no_variables==False:
            FORMAT = "#<z_safe> = %% .%df" %(PLACES_L)
            g_code.append(FORMAT %(plunge_feed))

            #FORMAT= "#<plunge_feed> = %% .%df" %(PLACES_F)
            FORMAT = "#<plunge_feed> = %s" %(FORMAT_FEED)

            g_code.append(FORMAT %(plunge_feed))

        for line in preamble.split('|'):
            g_code.append(line)

        g_code.append("(---------------------------------------------------------)")
        ###################
        ## GCODE WRITING ##
        ###################

        if no_variables==False:
            g_code.append("G0 Z#<z_safe>")
        else:
            FORMAT = "G0 Z%%.%df" %(PLACES_L)
            g_code.append(FORMAT %(z_safe) )

        FORMAT = "G0 %%c%%.%df %%c%%.%df" %(PLACES_L,PLACES_R)
        g_code.append(FORMAT %(LINEAR, Lmin_tp, ROTARY, 0.0) )
        
        FORMAT = "G1 Z%%.%df F%s" %(PLACES_L,FORMAT_FEED)
        g_code.append(FORMAT %(cut_depth, plunge_feed ) )
     
        Angle = 0

        Dangle = 360.0*sign
        Angle  = Angle + Dangle
        if FSCALE == "Scale-Rotary":
            Dist   = radians(Dangle)*Rstock
            Feed_adj = abs(Dangle / (Dist/feed) )
        FORMAT="G1 %%c%%.%df F%s" %(PLACES_R,FORMAT_FEED)
        g_code.append(FORMAT %(ROTARY, Angle, Feed_adj) )

        Dangle = 360*(Lmax_tp-Lmin_tp)/(tool_dia*step_over/100)*sign
        Angle  = Angle + Dangle
        if FSCALE == "Scale-Rotary":
            Dist   = sqrt((radians(Dangle)*Rstock)**2 + (Lmax_tp-Lmin_tp)**2)
            Fdist  = Lmax_tp-Lmin_tp
            Feed_adj = abs( Fdist / (Dist/feed) )
        FORMAT = "G1 %%c%%.%df %%c%%.%df F%s" %(PLACES_L, PLACES_R, FORMAT_FEED)
        g_code.append(FORMAT %(LINEAR, Lmax_tp, ROTARY, Angle, Feed_adj) )


        Dangle = 360.0*sign
        Angle  = Angle + Dangle
        if FSCALE == "Scale-Rotary":
            Dist   = abs(radians(Dangle)*Rstock)
            Feed_adj = abs(Dangle / (Dist/feed) )
        FORMAT = "G1 %%c%%.%df F%s" %(PLACES_R,FORMAT_FEED)
        g_code.append(FORMAT %(ROTARY, Angle, Feed_adj) )


        if no_variables==False:
            g_code.append("G0 Z #<z_safe>")
        else:
            FORMAT = "G0 Z%%.%df" %(PLACES_L)
            g_code.append(FORMAT %(z_safe) )
                
        ########################
        ## END G-CODE WRITING ##
        ########################
        for entry in postamble.split('|'):
            g_code.append(entry)
        g_code.append("M5 M2")
        return g_code


    def app_gcode_line(self,LINE,CODE,VALUE,PLACES,WriteAll):
        if isinstance(VALUE, complex):
            return LINE
        #if VALUE.imag != 0:
        #    return LINE

        if CODE == "F":
            if (VALUE*10**PLACES) < 1:
                # Fix Zero feed rate
                VALUE = 1.0/(10**PLACES)

        if PLACES > 0:
            FORMAT="%% .%df" %(PLACES)
        else:
            FORMAT="%d"
        VAL = FORMAT %(VALUE)

        if ( VAL != self.MODAL_VAL[CODE] )\
            or ( CODE=="I" ) \
            or ( CODE=="J" ) \
            or  (WriteAll):
            LINE = LINE +  " %s%s" %(CODE, VAL)
            self.MODAL_VAL[CODE] = VAL

        return LINE

    def get_arc_intersects(self, p1, p2, xsplit, cent, code):
        xcross1= xsplit
        xcross2= xsplit
     
        R = sqrt( (cent[0]-p1[0])**2 + (cent[1]-p1[1])**2 )
        Rt= sqrt( (cent[0]-p2[0])**2 + (cent[1]-p2[1])**2 )
        if abs(R-Rt) > self.accuracy:  fmessage("Radius Warning: R1=%f R2=%f"%(R,Rt))

        val =  R**2 - (xsplit - cent[0])**2
        if val >= 0.0:
            root = sqrt( val )
            ycross1 = cent[1] - root
            ycross2 = cent[1] + root
        else:
            return []

        theta = self.Get_Angle2(p1[0]-cent[0],p1[1]-cent[1])

        xbeta,ybeta = self.Transform(p2[0]-cent[0],p2[1]-cent[1],radians(-theta))
        beta  = self.Get_Angle2(xbeta,ybeta,code)

        if abs(beta) <= self.Zero: beta = 360.0

        xt,yt = self.Transform(xsplit-cent[0],ycross1-cent[1],radians(-theta))
        gt1 = self.Get_Angle2(xt,yt,code)

        xt,yt = self.Transform(xsplit-cent[0],ycross2-cent[1],radians(-theta))
        gt2 = self.Get_Angle2(xt,yt,code)
 
        if gt1 < gt2:
           gamma1 = gt1
           gamma2 = gt2
        else:
           gamma1 = gt2
           gamma2 = gt1
           temp = ycross1
           ycross1 = ycross2
           ycross2 = temp

        dz = p2[2] - p1[2]    
        da = beta
        mz = dz/da
        zcross1 = p1[2] + gamma1 * mz
        zcross2 = p1[2] + gamma2 * mz

        output=[]
        if gamma1 < beta and gamma1 > self.Zero and gamma1 < beta-self.Zero:
            output.append([xcross1,ycross1,zcross1])
        if gamma2 < beta and gamma1 > self.Zero and gamma2 < beta-self.Zero:
            output.append([xcross2,ycross2,zcross2])
        
        #print(" start: x1 =%5.2f y1=%5.2f z1=%5.2f" %(p1[0],     p1[1],     p1[2]))
        #print("   end: x2 =%5.2f y2=%5.2f z2=%5.2f" %(p2[0],     p2[1],     p2[2]))
        #print("center: xc =%5.2f yc=%5.2f xsplit=%5.2f code=%s" %(cent[0],cent[1],xsplit,code))
        #print("R = %f" %(R))
        #print("theta =%5.2f" %(theta))
        #print("beta  =%5.2f gamma1=%5.2f gamma2=%5.2f\n" %(beta,gamma1,gamma2))
        #cnt=0
        #for line in output:
        #    cnt=cnt+1
        #    print("arc cross%d: %5.2f, %5.2f, %5.2f" %(cnt, line[0], line[1], line[2]))
        #print(output)
        #print("----------------------------------------------\n")

        return output

    def arc2lines(self, p1, p2, cent, code, plane="17"):
        if   plane == "18":
            xind=2
            yind=0
            zind=1
        elif plane == "19":
            xind=1
            yind=2
            zind=0
        elif plane == "17":
            xind=0
            yind=1
            zind=2
        
        R = sqrt( (cent[xind]-p1[xind])**2 + (cent[yind]-p1[yind])**2 )
        Rt= sqrt( (cent[xind]-p2[xind])**2 + (cent[yind]-p2[yind])**2 )
        if abs(R-Rt) > self.accuracy:  fmessage("Radius Warning: R1=%f R2=%f "%(R,Rt))

        if code == 3:
            theta = self.Get_Angle2(p1[xind]-cent[xind],p1[yind]-cent[yind])
            xbeta,ybeta = self.Transform(p2[xind]-cent[xind],p2[yind]-cent[yind],radians(-theta))
            X1 = p1[xind]
            Y1 = p1[yind]
            Z1 = p1[zind]
            zstart = Z1
            zend   = p2[zind]
        if code == 2:
            theta = self.Get_Angle2(p2[xind]-cent[xind],p2[yind]-cent[yind])
            xbeta,ybeta = self.Transform(p1[xind]-cent[xind],p1[yind]-cent[yind],radians(-theta))
            X1 = p2[xind]
            Y1 = p2[yind]
            Z1 = p2[zind]
            zstart = Z1
            zend   = p1[zind]
            
        beta  = self.Get_Angle2(xbeta,ybeta) #,code)
        
        if abs(beta) <= self.Zero: beta = 360.0
        ##########################################
        arc_step=self.arc_angle
        
        my_range=[]
        
        at=arc_step
        while at < beta:
            my_range.append(at)
            at = at+arc_step
        my_range.append(beta)

        

        new_lines=[]
        for at in my_range:
            xt,yt = self.Transform(R,0,radians(theta+at))

            X2 = cent[xind] + xt
            Y2 = cent[yind] + yt
            #Z2 = p1[zind] + at*(p2[zind]-p1[zind])/beta
            Z2 = zstart + at*(zend-zstart)/beta
            data = ["","","","","",""]


            if code == 3:
                data[xind]=X1
                data[yind]=Y1
                data[zind]=Z1
                data[3+xind]=X2
                data[3+yind]=Y2
                data[3+zind]=Z2
                new_lines.append(data)
            else:
                data[xind]=X2
                data[yind]=Y2
                data[zind]=Z2
                data[3+xind]=X1
                data[3+yind]=Y1
                data[3+zind]=Z1
                new_lines.insert(0, data)
        
            X1=X2
            Y1=Y2
            Z1=Z2
            at = at+arc_step

        return new_lines
    
    def get_line_intersect(self,p1, p2, xsplit):
        dx = p2[0] - p1[0]
        dy = p2[1] - p1[1]
        dz = p2[2] - p1[2]
        
        xcross = xsplit 
        try:
            my = dy/dx
            by = p1[1] - my * p1[0]
            ycross = my*xsplit + by
        except:
            ycross = p1[1]
        try:
            mz = dz/dx
            bz = p1[2] - mz * p1[0]
            zcross = mz*xsplit + bz
        except:
            zcross = p1[2]

        output=[]
        if xcross > min(p1[0],p2[0])+self.Zero and xcross < max(p1[0],p2[0])-self.Zero:
            output.append([xcross,ycross,zcross])            
        return output


    def apleft(self, gtext):
        self.left_side.append(gtext)


    def apright(self, gtext):
        self.right_side.append(gtext)


    def apboth(self, gtext):
        self.left_side.append(gtext)
        self.right_side.append(gtext)


    def rm_text(self,line,s,e):
        if e == -1:
            e = len(line)
        temp1 = line[0:s]
        temp2 = line[e+1:len(line)]
        return temp1+temp2


    def insert_text(self,line,itext,s):
        temp1 = line[0:s]
        temp2 = line[s:len(line)]
        return temp1+itext+temp2


    def coordop(self,coords,offset,rot):
        x = coords[0]
        y = coords[1] 
        z = coords[2]
        x = x - offset[0]
        y = y - offset[1]
        z = z - offset[2] 
        x,y = self.Transform(x,y, radians(rot) )
        return [x,y,z]


    def coordunop(self,coords,offset,rot):
        x = coords[0]
        y = coords[1] 
        z = coords[2]
        x,y = self.Transform(x,y, radians(-rot) )
        x = x + offset[0]
        y = y + offset[1]
        z = z + offset[2] 
        return [x,y,z]

    #######################################    #######################################
    #######################################    #######################################
    #######################################    #######################################
    #######################################    #######################################

    def FUNCTION_EVAL(self,list):
        #list consists of [name,val1,val2]
        name = list[0]
        val1 = float(list[1])
        fval = float(val1)
        #############################################
        ########### G-CODE FUNCTIONS ################
        #############################################
        # ATAN[Y]/[X] Four quadrant inverse tangent #
        # ABS[arg]    Absolute value                #
        # ACOS[arg]   Inverse cosine                #
        # ASIN[arg]   Inverse sine                  #
        # COS[arg]    Cosine                        #
        # EXP[arg]    e raised to the given power   #
        # FIX[arg]    Round down to integer         #
        # FUP[arg]    Round up to integer           #
        # ROUND[arg]  Round to nearest integer      #
        # LN[arg]     Base-e logarithm              #
        # SIN[arg]    Sine                          #
        # SQRT[arg]   Square Root                   #
        # TAN[arg]    Tangent                       #
        # EXISTS[arg]	Check named Parameter   #
        #############################################
        if name == "ATAN":
            fval2 = float(list[2])
            atan2(fval1,fval2)
        if name == "ABS":
            return abs(fval)
        if name == "ACOS":
            return degrees(acos(fval))
        if name == "ASIN":
            return degrees(asin(fval))
        if name == "COS":
            return cos(radians(fval))
        if name == "EXP":
            return exp(fval)
        if name == "FIX":
            return floor(fval)
        if name == "FUP":
            return ceil(fval)
        if name == "ROUND":
            return round(fval)
        if name == "LN":
            return log(fval)
        if name == "SIN":
            return sin(radians(fval))
        if name == "SQRT":
            return sqrt(fval)
        if name == "TAN":
            return tan(radians(fval))
        if name == "EXISTS":
            pass
        
    def EXPRESSION_EVAL(self,line):
        ###################################################
        ###          EVALUATE MATH IN REGION            ###
        ###################################################
        line_in = line
        P = 0
        if P==1: fmessage("line=%s" %(line))

        if len(line)<2:
            MSG = "ERROR EXP-1: Unable to evaluate expression: %s\n" %(line_in)
            raise ValueError(MSG)
        
        
        line = line.replace(" ","")
        
        #################################################
        ###           G-CODE OPERATORS                ###
        ###          In Precedence Order              ###
        #################################################
        ##    **                                        #
        ##    * / MOD                                   #
        ##    + -                                       #
        ##    EQ NE GT GE LT LE                         #
        ##    AND OR XOR                                #
        #################################################

        #################################################
        ### Replace multiple +/- with single operator ###
        #################################################
        cnt = 1
        while cnt > 0:
            if (not cmp_new(line[cnt],'+')) or (not cmp_new(line[cnt],'-')):
                sign = 1
                span = 0
                FLAG = True
                while FLAG:
                    if not cmp_new(line[cnt+span],'+'):
                        span = span + 1
                    elif not cmp_new(line[cnt+span],'-'):
                        sign = -sign
                        span = span + 1
                    else:
                        FLAG = False
                tmp1=line[:(cnt)]
                tmp2=line[(cnt+span):]
                if sign > 0:
                    line = tmp1+'+'+tmp2
                else:
                    line = tmp1+'-'+tmp2    
            cnt=cnt + 1
            if cnt >= len(line):
                cnt = -1
                
        #########################################
        ### Replace multi-character operators ###
        ### with single character identifiers ###
        #########################################
        line = line.replace("XOR","|")
        line = line.replace("AND","&")
        line = line.replace("LE","l")
        line = line.replace("LT","<")
        line = line.replace("GE","g")
        line = line.replace("GT",">")
        line = line.replace("NE","!")
        line = line.replace("EQ","=")
        line = line.replace("**","^")

        #########################################
        ###     Split the text into a list    ###
        #########################################
        line = re.split( "([\[,\],\^,\*,\/,\%,\+,\-,\|  ,\&  ,\l ,\< ,\g ,\> ,\! ,\= ])", line)
        
        #########################################
        ### Remove empty items from the list  ###
        #########################################
        for i in range(line.count('')): line.remove('')
        
        #########################################
        ###   Find the last "[" in the list   ###
        #########################################
        s=-1
        for cnt in range(s+1,len(line)):
            if line[cnt] == '[':
                s = cnt
        if s == -1:
            MSG = "ERROR EXP-2: Unable to evaluate expression: %s" %(line_in)
            #fmessage(MSG)
            raise ValueError(MSG)
        
        #################################################################
        ###  While there are still brackets "[...]" keep processing   ###
        #################################################################
        while s != -1:
            ###############################################################
            ### Find the first occurrence of "]" after the current "["  ###
            ###############################################################
            e=-1
            for cnt in range(len(line)-1,s,-1):
                if line[cnt] == ']':
                    e = cnt
                    
            #############################################
            ###  Find the items between the brackets  ###
            #############################################
            temp = line[s+1:e]
            
            ##############################################################
            ###  Fix Some Special Cases                                ###
            ##############################################################
            ### **-  *-  MOD-                                          ###
            ##############################################################
            for cnt in range(0,len(temp)):
                if (not cmp_new(temp[cnt],'^')) or \
                   (not cmp_new(temp[cnt],'*')) or \
                   (not cmp_new(temp[cnt],'%')):
                    if not cmp_new(temp[cnt+1],'-'):
                        temp[cnt+1]=''
                        temp[cnt+2]= -float(temp[cnt+2])
                    elif not cmp_new(temp[cnt+1],'+'):
                        temp[cnt+1]=''
                        temp[cnt+2]= float(temp[cnt+2])
            for i in range(temp.count('')): temp.remove('')

            #####################################
            XOR_operation = self.list_split(temp,"|") #XOR
            for iXOR in range(0,len(XOR_operation)):
                #####################################
                AND_operation = self.list_split(XOR_operation[iXOR],"&") #AND
                for iAND in range(0,len(AND_operation)):
                    #####################################
                    LE_operation = self.list_split(AND_operation[iAND],"l") #LE
                    for iLE in range(0,len(LE_operation)):
                        #####################################
                        LT_operation = self.list_split(LE_operation[iLE],"<") #LT
                        for iLT in range(0,len(LT_operation)):
                            #####################################
                            GE_operation = self.list_split(LT_operation[iLT],"g") #GE
                            for iGE in range(0,len(GE_operation)):
                                #####################################
                                GT_operation = self.list_split(GE_operation[iGE],">") #GT
                                for iGT in range(0,len(GT_operation)):
                                    #####################################
                                    NE_operation = self.list_split(GT_operation[iGT],"!") #NE
                                    for iNE in range(0,len(NE_operation)):
                                        #####################################
                                        EQ_operation = self.list_split(NE_operation[iNE],"=") #EQ
                                        for iEQ in range(0,len(EQ_operation)):
                                            #####################################
                                            add = self.list_split(EQ_operation[iEQ],"+")
                                            for cnt in range(1,len(add)):
                                                if add[cnt-1]==[]:
                                                    add[cnt-1]  = ''
                                            for i in range(add.count('')): add.remove('')      
                                            for iadd in range(0,len(add)):
                                                #####################################
                                                subtract = self.list_split(add[iadd],"-")         
                                                for cnt in range(1,len(subtract)):
                                                    if subtract[cnt-1]==[]:
                                                        subtract[cnt-1]  = ''
                                                        subtract[cnt][0] = -float(subtract[cnt][0])
                                                for i in range(subtract.count('')): subtract.remove('')
                                                for isub in range(0,len(subtract)):
                                                    #####################################
                                                    multiply = self.list_split(subtract[isub],"*")
                                                    for imult in range(0,len(multiply)):
                                                        #####################################
                                                        divide = self.list_split(multiply[imult],"/")
                                                        for idiv in range(0,len(divide)):
                                                            #####################################
                                                            mod = self.list_split(divide[idiv],"%")
                                                            for imod in range(0,len(mod)):
                                                                #####################################
                                                                power = self.list_split(mod[imod],"^")
                                                                for ipow in range(0,len(power)):
                                                                    if power[ipow]==[]:
                                                                        MSG = "ERROR EXP-3: Unable to evaluate expression: %s" %(line_in)
                                                                        raise ValueError(MSG)
                                                                    
                                                                    if type(power[0]) is list:
                                                                        power_len = len(power[0])
                                                                    else:
                                                                        power_len = 1
                                                                    if power_len > 1:
                                                                        power[ipow] = self.FUNCTION_EVAL(power[0])
                                                                    else:
                                                                        power[ipow] = float(power[ipow][0])
                                                                #####################################
                                                                res_power=power[0]
                                                                for k in range(1,len(power)):
                                                                    res_power = res_power**power[k]
                                                                    if P==True: fmessage("  POWER"+str(power)+"="+str(res_power))
                                                                mod[imod]=res_power
                                                            #####################################
                                                            #REVERSE MOD
                                                            res_mod=mod[len(mod)-1]
                                                            for k in range(len(mod),1,-1):
                                                                res_mod = mod[k-2]%res_mod
                                                                fmessage("     MOD"+str(mod)+"="+str(res_mod))
                                                            divide[idiv]=res_mod
                                                        #####################################
                                                        res_divide=divide[0]
                                                        for k in range(1,len(divide),1):
                                                            res_divide = res_divide/divide[k]
                                                            if P==True: fmessage("  DIVIDE"+str(divide)+"="+str(res_divide))
                                                        multiply[imult]=res_divide
                                                    #####################################
                                                    res_multiply=multiply[0]
                                                    for k in range(1,len(multiply)):
                                                        res_multiply = res_multiply*multiply[k]
                                                        if P==True: fmessage("MULTIPLY"+str(multiply)+"="+str(res_multiply))         
                                                    subtract[isub]=res_multiply
                                                #####################################
                                                res_subtract=subtract[0]
                                                for k in range(1,len(subtract)):
                                                    res_subtract = res_subtract-subtract[k]
                                                    if P==True: fmessage("SUBTRACT"+str(subtract)+"="+str(res_subtract))
                                                add[iadd]=res_subtract
                                            #####################################
                                            res_add=add[len(add)-1]
                                            for k in range(len(add),1,-1):
                                                res_add = add[k-2]+res_add
                                                if P==True: fmessage("     ADD"+str(add)+"="+str(res_add))
                                            EQ_operation[iEQ]=res_add
                                        #####################
                                        res_EQ=EQ_operation[0]
                                        for k in range(1,len(EQ_operation),1):
                                            if res_EQ == EQ_operation[k]:
                                                res_EQ = 1
                                            else:
                                                res_EQ = 0
                                            if P==True: fmessage("      EQ"+str(EQ_operation)+"="+str(res_EQ))
                                        NE_operation[iNE]=res_EQ
                                    #####################
                                    res_NE=NE_operation[0]
                                    for k in range(1,len(NE_operation),1):
                                        if res_NE != NE_operation[k]:
                                            res_NE = 1
                                        else:
                                            res_NE = 0
                                        if P==True: fmessage("      NE"+str(NE_operation)+"="+str(res_NE))
                                    GT_operation[iGT]=res_NE
                                #####################
                                res_GT=GT_operation[0]
                                for k in range(1,len(GT_operation),1):
                                    if res_GT > GT_operation[k]:
                                        res_GT = 1
                                    else:
                                        res_GT = 0
                                    if P==True: fmessage("      GT"+str(GT_operation),"="+str(res_GT))
                                GE_operation[iGE]=res_GT
                            #####################
                            res_GE=GE_operation[0]
                            for k in range(1,len(GE_operation),1):
                                if res_GE >= GE_operation[k]:
                                    res_GE = 1
                                else:
                                    res_GE = 0
                                if P==True: fmessage("      GE"+str(GE_operation)+"="+str(res_GE))
                            LT_operation[iLT]=res_GE
                        #####################
                        res_LT=LT_operation[0]
                        for k in range(1,len(LT_operation),1):
                            if res_LT < LT_operation[k]:
                                res_LT = 1
                            else:
                                res_LT = 0
                            if P==True: fmessage("      LT"+str(LT_operation)+"="+str(res_LT))
                        LE_operation[iLE]=res_LT
                    #####################
                    res_LE=LE_operation[0]
                    for k in range(1,len(LE_operation),1):
                        if res_LE <= LE_operation[k]:
                            res_LE = 1
                        else:
                            res_LE = 0
                        if P==True: fmessage("      LE"+str(LE_operation)+"="+str(res_LE))
                    AND_operation[iAND]=res_LE
                #####################
                res_AND=AND_operation[0]
                for k in range(1,len(AND_operation),1):
                    if res_AND and AND_operation[k]:
                        res_AND = 1
                    else:
                        res_AND = 0
                    if P==True: fmessage("      AND"+str(AND_operation)+"="+str(res_AND))
                XOR_operation[iXOR]=res_AND
            #####################
            res_XOR=XOR_operation[0]
            for k in range(1,len(XOR_operation),1):
                if bool(res_XOR) ^ bool(XOR_operation[k]):
                    res_XOR = 1
                else:
                    res_XOR = 0
                if P==True: fmessage("      XOR"+str(XOR_operation)+"="+str(res_XOR))

            #####################################
            ### Return NEW VALUE to the list  ###
            #####################################
            for i in range(e,s-1,-1): line.pop(i)
            line.insert(int(s),res_XOR)

            #############################
            # Find Last "[" in the list #
            #############################
            s=-1
            for cnt in range(s+1,len(line)):
                if line[cnt] == '[':
                    s = cnt
        #################################################################    
        ###  END of While there are still brackets "[...]"            ###
        #################################################################
        
        if len(line) > 1:
            MSG = "ERROR EXP-5: Unable to evaluate expression: %s" %(line_in)
            raise ValueError(MSG)
        return "%.4f" %(line[0])


    def list_split(self,lst,obj):
        loc=[]
        index = -1
        for cnt in range(0,len(lst)):
            if not cmp_new(lst[cnt],obj):
                loc.append( lst[index+1:cnt] )
                index=cnt
        loc.append( lst[index+1:len(lst)])
        return loc

    ############################################################################
    # routine takes an x and a y coords and does a coordinate transformation   #
    # to a new coordinate system at angle from the initial coordinate system   #
    # Returns new x,y tuple                                                    #
    ############################################################################
    def Transform(self,x,y,angle):
        newx = x * cos(angle) - y * sin(angle)
        newy = x * sin(angle) + y * cos(angle)
        return newx,newy

    ############################################################################
    # routine takes an sin and cos and returnss the angle (between 0 and 360)  #
    ############################################################################
    def Get_Angle2(self,x,y,code=""):
        angle = 90.0-degrees(atan2(x,y))
        if angle < 0:
            angle = 360 + angle
        if code == "G2":
            return (360.0 - angle)
        else:
            return angle

###############
### END LIB ###
###############

def main_is_frozen():
   return (hasattr(sys, "frozen") or # new py2exe or pyinstaller
           hasattr(sys, "importers")) # old py2exe

################################################################################
#                          Startup Application                                 #
################################################################################
root = Tk()
root.wm_attributes("-topmost", 1)
app = Application(root)
app.master.title("G-Code-Ripper V"+version)
app.master.iconname("G-Code-Ripper")
app.master.minsize(780,540)
try:
    try:
        import tkFont
        default_font = tkFont.nametofont("TkDefaultFont")
    except:
        import tkinter.font
        default_font = tkinter.font.nametofont("TkDefaultFont")

    default_font.configure(size=9)
    default_font.configure(family='arial')
    #print(default_font.cget("size"))
    #print(default_font.cget("family"))
    debug_message("Font Set success!")
except:
    debug_message("Font Set Failed.")

################################## Set Icon  ########################################
Icon_Set=False
if main_is_frozen():
    print("frozen")
    try:
        root.iconbitmap(default=sys.argv[0])
        Icon_Set=True
    except:
        Icon_Set=False
        
if not Icon_Set:
    try:
        scorch_ico_B64=b'R0lGODlhEAAQAIYAAA\
        AAABAQEBYWFhcXFxsbGyUlJSYmJikpKSwsLC4uLi8vLzExMTMzMzc3Nzg4ODk5OTs7Oz4+PkJCQkRERE\
        VFRUtLS0xMTE5OTlNTU1dXV1xcXGBgYGVlZWhoaGtra3FxcXR0dHh4eICAgISEhI+Pj5mZmZ2dnaKioq\
        Ojo62tra6urrS0tLi4uLm5ub29vcLCwsbGxsjIyMzMzM/Pz9PT09XV1dbW1tjY2Nzc3OHh4eLi4uXl5e\
        fn5+jo6Ovr6+/v7/Hx8fLy8vT09PX19fn5+fv7+/z8/P7+/v///wAAAAAAAAAAAAAAAAAAAAAAAAAAAA\
        AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA\
        AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA\
        AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAACH5BAEKAEkALAAAAAAQABAAQAj/AJMIFBhBQYAACRIkWbgwAA\
        4kEFEECACAxBAkGH8ESEKgBZIiAIQECBAjAA8kNwIkScKgQhAkRggAIJACCZIaJxgk2clgAY4OAAoEAO\
        ABCIIDSZIwkIHEBw0YFAAA6IGDCBIkLAhMyICka9cAKZCIRTLEBIMkaA0MSNGjSBEVIgpESEK3LgMCI1\
        aAWCFDA4EDSQInwaDACBEAImLwCAFARw4HFJJcgGADyZEAL3YQcMGBBpIjHx4EeIGkRoMFJgakWADABx\
        IkPwIgcIGkdm0AMJDo1g3jQBIBRZAINyKAwxEkyHEUSMIcwYYbEgwYmQGgyI8SD5Jo327hgIIAAQ5cBs\
        CQpHySgAA7'
        icon_im =PhotoImage(data=scorch_ico_B64, format='gif')
        root.call('wm', 'iconphoto', root._w, '-default', icon_im)
    except:
        pass
#####################################################################################
        
    
root.mainloop()
