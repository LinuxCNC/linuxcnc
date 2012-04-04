#!/bin/sh
# the next line restarts using emcsh \
exec $LINUXCNC_EMCSH "$0" "$@"

###############################################################
# Description:  genedit.tcl
#               This file dumps info to a logging file from tkmc.
#
#  Authors: Ray Henry & Paul Corner
#  License: GPL Version 2
#
#  Copyright (c) 2009 All rights reserved.
###############################################################
# This is plot processes for tkemc popup under the view menu 
# of tkemc.  The fixed size of the canvas viewing area is 500x250 
# pixels less scrollbars.  The full canvas could be much larger.
# Several local variables may be set below to match machines.
###############################################################

# Load the linuxcnc.tcl file, which defines variables for various useful paths
source [file join [file dirname [info script]] .. linuxcnc.tcl]

# These offset plot zero from canvas zero.
set xoffset 0
set yoffset 0
set zoffset 0

# These set the axis directions.  +1 one plots like graph
# paper with positive up and to the right.  The canvas widgets orient so that
# xy plot is looking down on the work area from the operator position.
# x is left right and y is up down.
# xz plot is looking into the work area along the axis of y
# yz plot is looking into the work area along the axis of x
# to reverse as axis change its value to -1 (don't put other numbers here)
set xdir 1
set ydir 1
set zdir 1

# These set the size of the canvas widget behind the viewing area.
# Set for the size of your machine or larger.  They can also be set so that you
# place your machine home about the same location in canvas space that it has on the
# real machine.
# They need not be the same for each axis.
# the value used is pixels so multiply your machine workspace size in inches by 72.

# the default shipped here is for a center zero'd machine with 1000 pixels both left/right
# and 500 pixels both top/bottom in all axis.
# Read variable names (xytopx) as "xy plot topleft x" value etc.

# If your machine were top right homed on xy, you might use
#   xy -1900 -100 100 900
#   xz -1900 -100 100 900
#   yz -100 -100 1900 900

# These set top left and bottom right of xy canvas
set xytopx -1000
set xytopy -500
set xybotx 1000
set xyboty 500

# These set top left and bottom right of xz canvas
set xztopx -1000
set xztopz -500
set xzbotx 1000
set xzbotz 500

# These set top left and bottom right of yz canvas
set yztopy -1000
set yztopz -500
set yzboty 1000
set yzbotz 500

# This variable sets the default size of the plot and can be thought of as number of
# pixels per inch of machine motion.  The default (40) will give about .55 screen
# inch per inch moved.

set size 40

#These variables set the defalut angles used for the 3d plot.  in this release,
#only the xyangle is used though the others are also shown on the setup screen

# These three can go.
set xyangle 30
set xangle 30
set yangle 30
set zangle 60
set xycosangle 0
set xysinangle 0

# I used these in the latest edit - Paul.

# pop up the plot window
proc popupPlot {} {
    global screen tab
    global xytopx xytopy xybotx xyboty
    global xztopx xztopz xzbotx xzbotz
    global yztopy yztopz yzboty yzbotz
    global xycenterx xycentery xzcenterx xzcenterz yzcentery yzcenterz


    set d .plot

    if {[winfo exists $d]} {
        wm deiconify $d
        raise $d
        focus $d
        return
    }
    toplevel $d
    wm title $d [msgcat::mc "TkLinuxCNC BackPlot"]

# use "pw" as name of top level from now on
set pw .plot

set tab [frame $pw.tabs]
button $tab.xy -relief flat -font {Helvetica -10 bold} -anchor n -width 8 \
    -text {X - Y} -command {selectPlot xy} -borderwidth 0
button $tab.xz -relief flat -font {Helvetica -10 bold} -anchor n -width 8 \
    -text {X - Z} -command {selectPlot xz} -borderwidth 0
button $tab.yz -relief flat -font {Helvetica -10 bold} -anchor n -width 8 \
    -text {Y - Z} -command {selectPlot yz} -borderwidth 0
button $tab.3d -relief flat -font {Helvetica -10 bold} -anchor n -width 8 \
    -text { 3D } -command {selectPlot 3d} -borderwidth 0
button $tab.config  -relief raised -font {Helvetica -10 bold} -anchor n -width 8 \
    -text [msgcat::mc "SETUP"] -command {selectPlot config} -borderwidth 1
button $tab.reset  -relief raised -font {Helvetica -10 bold} -anchor n -width 8 \
    -text [msgcat::mc "RESET"] -command {erasePlot} -borderwidth 1
button $tab.can  -relief raised -font {Helvetica -10 bold} -anchor n -width 8 \
    -text [msgcat::mc "CANCEL"] -command {destroy .plot} -borderwidth 1
pack $tab.xy $tab.xz $tab.yz $tab.3d $tab.config $tab.reset $tab.can -side left \
     -fill both
pack $tab -side top -fill x


set xlast 0
set ylast 0
set zlast 0
set 3dxnext 0
set 3dyrnext 0
set plotcolor black

# Computes (0,0) and attempts to center it in view.
# Computation for xy plot
set xytotalx [expr {abs($xytopx) + abs($xybotx)}]
set xylowerx [expr {abs($xytopx) - 270}]
if {$xylowerx <= 0} {
    set xycenterx 0
    } else {
    set xycenterx [expr {double($xylowerx) / double($xytotalx)} * 1.1 ]
}

set xytotaly [expr {abs($xytopy) + abs($xyboty)}]
set xylowery [expr {abs($xytopy) - 125}]
if {$xylowery <= 0} {
    set xycentery 0
    } else {
    set xycentery [expr {double($xylowery) / double($xytotaly)} * .8 ]
}

# Computation for xz plot
set xztotalx [expr {abs($xztopx) + abs($xzbotx)}]
set xzlowerx [expr {abs($xztopx) - 270}]
if {$xzlowerx <= 0} {
    set xzcenterx 0
    } else {
    set xzcenterx [expr {double($xzlowerx) / double($xztotalx)} * 1.1]
}

set xztotalz [expr {abs($xztopz) + abs($xzbotz)}]
set xzlowerz [expr {abs($xztopz) - 125}]
if {$xzlowerz <= 0} {
    set xzcenterz 0
    } else {
    set xzcenterz [expr {double($xzlowerz) / double($xztotalz)} * .8]
}

# Computation for yz plot
set yztotaly [expr {abs($yztopy) + abs($yzboty)}]
set yzlowery [expr {abs($yztopy) - 270}]
if {$yzlowery <= 0} {
    set yzcentery 0
    } else {
    set yzcentery [expr {double($yzlowery) / double($yztotaly)} * 1.1]
}

set yztotalz [expr {abs($yztopz) + abs($yzbotz)}]
set yzlowerz [expr {abs($yztopz) - 125}]
if {$yzlowerz <= 0} {
    set yzcenterz 0
    } else {
    set yzcenterz [expr {double($yzlowerz) / double($yztotalz)} * .8]
}

set screen [frame $pw.frame -width 500 -height 260 ]

frame $screen.xyplot
canvas $screen.xyplot.canv -borderwidth 0 -scrollregion "$xytopx $xytopy $xybotx $xyboty" \
     -relief flat -xscrollcommand "$screen.xyplot.hscroll set" \
    -yscrollcommand "$screen.xyplot.vscroll set" \
    -xscrollincrement 10  -yscrollincrement 10 -width 500 -height 260
scrollbar $screen.xyplot.hscroll -orient horizontal -command  "$screen.xyplot.canv xview"
scrollbar $screen.xyplot.vscroll -orient vertical -command "$screen.xyplot.canv yview"
grid $screen.xyplot.canv -row 0 -column 0 -sticky nsew
grid $screen.xyplot.vscroll -row 0 -column 1 -sticky ns
grid $screen.xyplot.hscroll -row 1 -column 0 -sticky ew
grid rowconfigure $screen.xyplot 0 -weight 1
grid columnconfigure $screen.xyplot 0 -weight 1

frame $screen.xzplot
canvas $screen.xzplot.canv -borderwidth 0 -scrollregion "$xztopx $xztopz $xzbotx $xzbotz" \
     -relief flat -xscrollcommand "$screen.xzplot.hscroll set" \
    -yscrollcommand "$screen.xzplot.vscroll set" \
    -xscrollincrement 10  -yscrollincrement 10 -width 500 -height 260
scrollbar $screen.xzplot.hscroll -orient horizontal -command  "$screen.xzplot.canv xview"
scrollbar $screen.xzplot.vscroll -orient vertical -command "$screen.xzplot.canv yview"
grid $screen.xzplot.canv -row 0 -column 0 -sticky nsew
grid $screen.xzplot.vscroll -row 0 -column 1 -sticky ns
grid $screen.xzplot.hscroll -row 1 -column 0 -sticky ew
grid rowconfigure $screen.xzplot 0 -weight 1
grid columnconfigure $screen.xzplot 0 -weight 1

frame $screen.yzplot
canvas $screen.yzplot.canv -borderwidth 0 -scrollregion "$yztopy $yztopz $yzboty $yzbotz" \
     -relief flat -xscrollcommand "$screen.yzplot.hscroll set" \
    -yscrollcommand "$screen.yzplot.vscroll set" \
    -xscrollincrement 10  -yscrollincrement 10 -width 500 -height 260
scrollbar $screen.yzplot.hscroll -orient horizontal -command  "$screen.yzplot.canv xview"
scrollbar $screen.yzplot.vscroll -orient vertical -command "$screen.yzplot.canv yview"
grid $screen.yzplot.canv -row 0 -column 0 -sticky nsew
grid $screen.yzplot.vscroll -row 0 -column 1 -sticky ns
grid $screen.yzplot.hscroll -row 1 -column 0 -sticky ew
grid rowconfigure $screen.yzplot 0 -weight 1
grid columnconfigure $screen.yzplot 0 -weight 1

frame $screen.3dplot
canvas $screen.3dplot.canv -borderwidth 0 -scrollregion "$yztopy $yztopz $yzboty $yzbotz" \
     -relief flat -xscrollcommand "$screen.3dplot.hscroll set" \
    -yscrollcommand "$screen.3dplot.vscroll set" \
    -xscrollincrement 10  -yscrollincrement 10 -width 500 -height 260
scrollbar $screen.3dplot.hscroll -orient horizontal -command  "$screen.3dplot.canv xview"
scrollbar $screen.3dplot.vscroll -orient vertical -command "$screen.3dplot.canv yview"
grid $screen.3dplot.canv -row 0 -column 0 -sticky nsew
grid $screen.3dplot.vscroll -row 0 -column 1 -sticky ns
grid $screen.3dplot.hscroll -row 1 -column 0 -sticky ew
grid rowconfigure $screen.3dplot 0 -weight 1
grid columnconfigure $screen.3dplot 0 -weight 1

set cft [frame $screen.config ]

label $cft.label1 -text [msgcat::mc "AXIS"] -anchor e
label $cft.label2 -text [msgcat::mc "OFFSET"]
label $cft.label3 -text [msgcat::mc "ANGLE"]
label $cft.label4 -text [msgcat::mc "DIRECTION"]

label $cft.labelx -text "X :" -anchor e
entry $cft.exo -relief sunken -textvariable xoffset -width 8
entry $cft.exa -relief sunken -textvariable xangle -width 8
button $cft.bxd -textvariable xdir -command {changeDirection xdir} -width 10

label $cft.labely -text "Y :" -anchor e
entry $cft.eyo -relief sunken -textvariable yoffset -width 8
entry $cft.eya -relief sunken -textvariable yangle -width 8
button $cft.byd -textvariable ydir -command {changeDirection ydir} -width 10

label $cft.labelz -text "Z :" -anchor e
entry $cft.ezo -relief sunken -textvariable zoffset -width 8
entry $cft.eza -relief sunken -textvariable zangle -width 8
button $cft.bzd -textvariable zdir -command {changeDirection zdir} -width 10

label $cft.elabel -text [msgcat::mc "SIZE :"] -anchor e  -width 8
entry $cft.esize -relief sunken -textvariable size -width 8

grid $cft.label1 $cft.label2 $cft.label3 $cft.label4 $cft.elabel -padx 6
grid $cft.labelx $cft.exo $cft.exa $cft.bxd $cft.esize -padx 6 -pady 2
grid $cft.labely $cft.eyo $cft.eya $cft.byd -padx 6 -pady 2
grid $cft.labelz $cft.ezo $cft.eza $cft.bzd -padx 6 -pady 2

pack $cft -side top -pady 10 -fill both -expand yes
pack $screen -side top -fill both -expand yes

erasePlot
selectPlot 3d

}

proc selectPlot {which} {
    global screen tab
    switch -- $which {
        xy {
            pack $screen.xyplot -side top -fill both -expand yes
            pack forget $screen.xzplot $screen.yzplot $screen.3dplot $screen.config
            $tab.xy configure   -borderwidth 0 -relief flat
            $tab.xz configure  -borderwidth 1 -relief sunken
            $tab.yz configure  -borderwidth 1 -relief sunken
            $tab.3d configure  -borderwidth 1 -relief sunken
        }
        xz {
            pack $screen.xzplot -side top -fill both -expand yes
            pack forget $screen.xyplot $screen.yzplot $screen.3dplot $screen.config
            $tab.xy configure   -borderwidth 1 -relief sunken
            $tab.xz configure   -borderwidth 0 -relief flat
            $tab.yz configure   -borderwidth 1 -relief sunken
            $tab.3d configure   -borderwidth 1 -relief sunken
        }
        yz {
            pack $screen.yzplot -side top -fill both -expand yes
            pack forget $screen.xyplot $screen.xzplot $screen.3dplot $screen.config
            $tab.xy configure   -borderwidth 1 -relief sunken
            $tab.xz configure   -borderwidth 1 -relief sunken
            $tab.yz configure   -borderwidth 0 -relief flat
            $tab.3d configure   -borderwidth 1 -relief sunken
        }
        3d {
            pack $screen.3dplot -side top -fill both -expand yes
            pack forget $screen.xyplot $screen.xzplot $screen.yzplot $screen.config
            $tab.xy configure   -borderwidth 1 -relief sunken
            $tab.xz configure   -borderwidth 1 -relief sunken
            $tab.yz configure   -borderwidth 1 -relief sunken
            $tab.3d configure   -borderwidth 0 -relief flat
        }
        config {
            pack $screen.config -side top -fill both -expand yes
            pack forget $screen.xyplot $screen.xzplot $screen.yzplot $screen.3dplot
            $tab.xy configure  -borderwidth 1 -relief sunken
            $tab.xz configure  -borderwidth 1 -relief sunken
            $tab.yz configure  -borderwidth 1 -relief sunken
            $tab.3d configure  -borderwidth 1 -relief sunken
        }
        default {
        }
    }
}

proc changeDirection {axisvariable} {
    global xdir ydir zdir
    if {$axisvariable == "xdir"} {
        if {$xdir == 1} {
            set xdir -1
        } else {
            set xdir 1
        }
    } elseif {$axisvariable == "ydir"} {
        if {$ydir == 1} {
            set ydir -1
        } else {
            set ydir 1
        }
    } elseif {$axisvariable == "zdir"} {
        if {$zdir == 1} {
            set zdir -1
        } else {
            set zdir 1
        }
    }
    erasePlot
}

set xnext 0; set ynext 0; set znext 0; set yrnext 0; set zrnext 0
set 3dxnext 0; set 3dynext 0; set 3dznext 0; set 3dyrnext 0
set 3dylast 0; set 3dxlast 0; set 3dzlast 0; set 3dyrlast 0

proc backPlot {} {
    global screen
    global size xoffset yoffset zoffset xdir ydir zdir
    global xnext ynext znext yrnext zrnext 3dxnext 3dyrnext
    $screen.xyplot.canv create oval [expr $xnext -2] [expr $yrnext -2] \
        [expr $xnext + 2] [expr $yrnext +2] -outline red -tags xytool
    $screen.xzplot.canv create line $xnext $zrnext $xnext [expr $zrnext - 20] \
        -fill red -arrow first -tags xzarrow
    $screen.yzplot.canv create line $ynext $zrnext $ynext [expr $zrnext - 20] \
        -fill red -arrow first -tags yzarrow
    $screen.3dplot.canv create line $3dxnext $3dyrnext [expr $3dxnext + 10] [expr $3dyrnext -15] \
        -fill red -arrow first -tags 3darrow
}

proc erasePlot {} {
    global screen
    $screen.xyplot.canv addtag all all
    $screen.xyplot.canv delete all
    $screen.xzplot.canv addtag all all
    $screen.xzplot.canv delete all
    $screen.yzplot.canv addtag all all
    $screen.yzplot.canv delete all
    $screen.3dplot.canv addtag all all
    $screen.3dplot.canv delete all
    backPlot
    markPlot
}

# Process markPlot shows initial conditions and display for plots
proc markPlot {} {
    global xdir ydir zdir xoffset yoffset zoffset 3dxyangle xycosangle xysinangle
    global xycenterx xycentery xzcenterx xzcenterz yzcentery yzcenterz xyangle
    global xangle yangle zangle
    global screen
    $screen.xyplot.canv create line 0 0 [expr $xdir * 50] 0 -arrow last -fill blue
    $screen.xyplot.canv create text [expr $xdir * (50 + 10)] 2 -text X+ -fill blue
    $screen.xyplot.canv create line 0 0 0 [expr $ydir * -50] -arrow last -fill blue
    $screen.xyplot.canv create text -2 [expr $ydir * -(50 + 10)] -text Y+ -fill blue
    $screen.xyplot.canv xview moveto $xycenterx
    $screen.xyplot.canv yview moveto $xycentery

    $screen.xzplot.canv create line 0 0 [expr $xdir * 50] 0 -arrow last -fill blue
    $screen.xzplot.canv create text [expr $xdir * (50 + 10)] 2 -text X+ -fill blue
    $screen.xzplot.canv create line 0 0 0 [expr $zdir * -50] -arrow last -fill blue
    $screen.xzplot.canv create text -2 [expr $zdir * -(50 + 10)] -text Z+ -fill blue
    $screen.xzplot.canv xview moveto $xzcenterx
    $screen.xzplot.canv yview moveto $xzcenterz
    $screen.yzplot.canv create line 0 0 [expr $ydir * 50] 0 -arrow last -fill blue
    $screen.yzplot.canv create text [expr $ydir * (50 + 10)] 2 -text Y+ -fill blue
    $screen.yzplot.canv create line 0 0 0 [expr $zdir * -50] -arrow last -fill blue
    $screen.yzplot.canv create text -2 [expr $zdir * -(50 + 10)] -text Z+ -fill blue
    $screen.yzplot.canv xview moveto $yzcentery
    $screen.yzplot.canv yview moveto $yzcenterz

    set xyangle $xangle
    set 3dxyangle [expr $xyangle * 0.017453]
    set xycosangle [expr cos($3dxyangle)]
    set xysinangle [expr sin($3dxyangle)]

    $screen.3dplot.canv create line 0 0 [expr $xdir * $xycosangle * 50] \
        [expr $xdir * $xysinangle * -50] -arrow last -fill blue
    $screen.3dplot.canv create text [expr $xdir * $xycosangle * 60]\
        [expr $xdir * $xysinangle * -60]  -text X+ -fill blue
    $screen.3dplot.canv create line 0 0 [expr $ydir * $xycosangle * -50] \
        [expr $ydir * $xysinangle * -50] -arrow last -fill blue
    $screen.3dplot.canv create text [expr $ydir * $xycosangle * -60] \
        [expr $ydir * $xysinangle * -60] -text Y+ -fill blue
    $screen.3dplot.canv create line 0 0 0 [expr $zdir * -50] -arrow last -fill blue
    $screen.3dplot.canv create text -2 [expr $zdir * - 60] -text Z+ -fill blue
    $screen.3dplot.canv xview moveto $xycenterx
    $screen.3dplot.canv yview moveto $xycentery
}

# this process plots the motion of the tool center
# it is commanded by a line [if winfo exists .plot] in updateStatus

set xlast 0
set ylast 0
set zlast 0
set plotcolor brown

proc updatePlot {} {
    global size screen unitsetting
    global xdir ydir zdir
    global xnext ynext znext yrnext zrnext xlast ylast zlast
    global xoffset yoffset zoffset xycosangle xysinangle
    global programfiletext posdigit0 posdigit1 posdigit2 activeLine plotcolor
    global 3dxnext 3dynext 3dznext 3dyrnext 3dylast 3dxlast 3dzlast 3dyrlast

    # hack to divide scale for mm plotting
    if {$unitsetting == "(mm)" } {
        set scaler 25.4
    } else {
        set scaler 1
    }
    set posdigitx0 [expr $posdigit0 / $scaler]
    set posdigitx1 [expr $posdigit1 / $scaler]
    set posdigitx2 [expr $posdigit2 / $scaler]
    
    # Color plot line by setting active line to upcase thisstring
        set thisstring [string toupper [$programfiletext get $activeLine.0 $activeLine.end]]
    # Search thisstring for g0-3 but make modal with no else
    if { [string first G2 $thisstring] != -1 || \
        [string first G02 $thisstring] != -1  } {
            set plotcolor red
    } elseif { [string first G3 $thisstring] != -1 || \
        [string first G03 $thisstring] != -1 } {
            set plotcolor blue
    } elseif { [string first G1 $thisstring] != -1 || \
        [string first G01 $thisstring] != -1 } {
            set plotcolor black
    } elseif { [string first G$screen $thisstring] != -1 } {
            continue
    } elseif { [string first G0 $thisstring] != -1 } {
        set plotcolor lightgreen
    }

    if {$size < 1} {set size 1} {
        set xnext [expr $xoffset + ($posdigitx0 * $size * $xdir)]
        set ynext [expr $yoffset + ($posdigitx1 * $size * $ydir)]
        set znext [expr $zoffset + ($posdigitx2 * $size * $zdir)]

        if {$xlast != $xnext || $ylast != $ynext || $zlast != $znext} {
        # these routines reverse y direction, plot xy and move tool
            set yrlast [expr -1 * $ylast]
            set yrnext [expr -1 * $ynext]
            $screen.xyplot.canv create line $xlast $yrlast $xnext $yrnext -fill $plotcolor
            $screen.xyplot.canv move xytool [expr $xnext - $xlast] [expr $yrnext - $yrlast]

        # these routines reverse z direction plot xz and yz and move arrows
            set zrlast [expr -1 * $zlast]
            set zrnext [expr -1 * $znext]
            $screen.xzplot.canv create line $xlast $zrlast $xnext $zrnext -fill $plotcolor
            $screen.xzplot.canv move xzarrow [expr $xnext - $xlast] [expr $zrnext - $zrlast]
            $screen.yzplot.canv create line $ylast $zrlast $ynext $zrnext -fill $plotcolor
            $screen.yzplot.canv move yzarrow [expr $ynext - $ylast] [expr $zrnext - $zrlast]

       # 3D plot calculations
          set 3dxnext [expr [expr $xnext * $xycosangle] + [expr $ynext * -1 * $xycosangle]]
          set 3dynext [expr [expr $xnext * $xysinangle] + [expr $ynext * $xysinangle] + $znext]
       # Plot 3D xy and move tool
            set 3dyrlast [expr -1 * $3dylast]
            set 3dyrnext [expr -1 * $3dynext]
            $screen.3dplot.canv create line $3dxlast $3dyrlast $3dxnext $3dyrnext -fill $plotcolor
            $screen.3dplot.canv move 3darrow [expr $3dxnext - $3dxlast] [expr $3dyrnext - $3dyrlast]

            set 3dxlast $3dxnext
            set 3dylast $3dynext
            set 3dzlast $3dznext
            set xlast $xnext
            set ylast $ynext
            set zlast $znext
        }
    }
}
