 # ----------------------------------------------------------------------
 #
 # cbutton.tcl --
 #
 #       Example of how to provide button-like behavior on canvas
 #       items. (Posted on comp.lang.tcl by Kevin Kenny)
 #
 #       source: https://wiki.tcl-lang.org/page/Canvas+Buttons
 
 set ::RCSID([info script]) \
   {$Id: 1383,v 1.3 2006-09-24 06:00:06 jcw Exp $}
 
 package provide canvasbutton 1.0
 
 namespace eval canvasbutton {
 
 # nexttag - Next unique tag number for a "button" being
 #           created
 
 variable nexttag 0
 
 # command - command(tag#) contains the command to execute when
 #           a "button" is selected.
 
 variable command
 
 # cursor - cursor(pathName) contains the (saved) cursor
 #          symbol of the widget when the pointer is in
 #          a "button"
 
 variable cursor
 
 # enteredButton - contains the tag number of the button
 #                 containing the pointer.
 
 variable enteredButton {}
 
 # pressedButton - contains the tag number of the "button"
 #                 in which the mouse button was pressed
 
 variable pressedButton {}
 
 namespace export canvasbutton
 }
 
 # ----------------------------------------------------------------------
 #
 # canvasbutton::canvasbutton --
 #
 #       Create a button-like object on a canvas.
 #
 # Parameters:
 #       w       Path name of the canvas
 #       x0      Canvas X co-ordinate of left edge
 #       y0      Canvas Y co-ordinate of top edge
 #       x1      Canvas X co-ordinate of right edge
 #       y1      Canvas Y co-ordinate of bottom edge
 #       text    Text to display in the button
 #       cmd     Command to execute when the button is selected.
 #
 # Results:
 #       Unique canvas tag assigned to the items that make
 #       up the button.
 #
 # Side effects:
 #       A rectangle and a text item are created in the canvas,
 #       and bindings are established to give them button-like
 #       behavior.
 #
 #----------------------------------------------------------------------
 
 proc canvasbutton::canvasbutton {w x0 y0 wd h text cmd state} {
     variable nexttag
     variable command
 
     set btag [list canvasb# [incr nexttag]]
 
     set command($btag) $cmd
 

 
     set x [expr { $x0 + ($wd / 2) }]
     set y [expr { $y0 + ($h / 2) + 1}]

    if {$state} {

        $w create rectangle $x0 $y0 [expr {$x0 + $wd}] [expr {$y0 + $h}] \
                -fill lightgray -outline black -width 1 \
                -tags [list canvasb $btag [linsert $btag end frame]]

        $w create text $x $y -anchor center -justify center \
                -text $text \
                -tags [list canvasb $btag [linsert $btag end text]]
    
        $w bind canvasb <Enter> [list [namespace current]::enter %W]
        $w bind canvasb <Leave> [list [namespace current]::leave %W]
        $w bind canvasb <ButtonPress-1> \
                [list [namespace current]::press %W]
        $w bind canvasb <ButtonRelease-1> \
                [list [namespace current]::release %W]
    } else {
        $w create rectangle $x0 $y0 [expr {$x0 + $wd}] [expr {$y0 + $h}] \
                -fill lightgray -outline grey65 -width 1 
        $w create text $x $y -anchor center -justify center \
                -text $text -fill grey65
    }
 
     return $btag
 }
 
 # ----------------------------------------------------------------------
 #
 # canvasbutton::enter --
 #
 #       Process the <Enter> event on a canvas-button.
 #
 # Parameters:
 #       w       Path name of the canvas
 #
 # Results:
 #       None.
 #
 # Side effects:
 #       When the mouse pointer is in a button, the button is
 #       highlighted with a broad outline and the cursor
 #       symbol changes to an arrow.  When the active button
 #       is pressed, it is highlighted in green.
 #
 # ----------------------------------------------------------------------
 
 proc canvasbutton::enter {w} {
     variable enteredButton
     variable pressedButton
     variable cursor
 
     set enteredButton [findBtag $w]
     set frame [linsert $enteredButton end frame]
     set cursor($w) [$w cget -cursor]
     $w configure -cursor arrow
     #$w itemconfigure $frame -width 3
     $w itemconfigure $frame -fill grey93
     if {![string compare $enteredButton $pressedButton]} {
         $w itemconfigure $frame -fill grey60
     }
 }
 
 # ----------------------------------------------------------------------
 #
 # canvasbutton::leave --
 #
 #       Process the <Leave> event on a canvas-button.
 #
 # Parameters:
 #       w       Path name of the canvas
 #
 # Results:
 #       None.
 #
 # Side effects:
 #       Reverts the cursor symbol, the border width
 #       if needed, the highlight color of the button.
 #
 # ----------------------------------------------------------------------
 
 proc canvasbutton::leave {w} {
     variable enteredButton
     variable pressedButton
     variable cursor
     if {[string compare $enteredButton {}]} {
         set btag [findBtag $w]
         set frame [linsert $btag end frame]
         #$w itemconfigure $frame -width 1
         $w itemconfigure $frame -fill lightgray
         $w configure -cursor $cursor($w)
         unset cursor($w)
         if {![string compare $btag $pressedButton]} {
             $w itemconfigure $frame -fill white
         }
         set enteredButton {}
     }
     return
 }
 
 # ----------------------------------------------------------------------
 #
 # canvasbutton::press --
 #
 #       Process the <ButtonPress-1> event on a canvas-button.
 #
 # Parameters:
 #       w       Path name of the canvas
 #
 # Results:
 #       None.
 #
 # Side effects:
 #       Highlights the selected button in green.
 #
 # ----------------------------------------------------------------------
 
 proc canvasbutton::press {w} {
     variable pressedButton
     set pressedButton [findBtag $w]
     $w itemconfigure [linsert $pressedButton end frame] \
             -fill grey60
     return
 }
 
 # ----------------------------------------------------------------------
 #
 # canvasbutton::release --
 #
 #       Process the <ButtonRelease-1> event on a canvas-button.
 #
 # Parameters:
 #       w       Path name of the canvas
 #
 # Results:
 #       None.
 #
 # Side effects:
 #       Reverts the highlight color on the button.  If the
 #       mouse has not left the button, invokes the button's
 #       command.
 #
 # ----------------------------------------------------------------------
 
 proc canvasbutton::release {w} {
     variable enteredButton
     variable pressedButton
     variable command
 
     set pressedButtonWas $pressedButton
     set pressedButton {}
 
     $w itemconfigure [linsert $pressedButtonWas end frame] \
             -fill grey93
 
     if {![string compare $enteredButton $pressedButtonWas]} {
         uplevel #0 $command($pressedButtonWas)
     }
     return
 }
 
 # ----------------------------------------------------------------------
 #
 # canvasbutton::btag --
 #
 #       Locate the unique tag of a canvas-button
 #
 # Parameters:
 #       w       Path name of the canvas
 #
 # Results:
 #       Button tag, or the null string if the current
 #       item is not a canvas-button
 #
 # Side effects:
 #       Searches the tag list of the current canvas item
 #       for a tag that begins with the string, `canvasb#',
 #       and returns the first two elements of the tag
 #       interpreted as a Tcl list.
 #
 # ----------------------------------------------------------------------
 
 proc canvasbutton::findBtag {w} {
     foreach tag [$w itemcget current -tags] {
         if {[regexp {^canvasb#} [lindex $tag 0]]} {
             return [lrange $tag 0 1]
         }
     }
     return {}
 }
 
 if {![string compare $argv0 [info script]]} {
 
     grid [canvas .c -width 300 -height 200 -cursor crosshair]
     
     namespace import canvasbutton::*
 
     .c create text 150 150 -anchor n -tags label \
             -font {Helvetica 10 bold}
 
     canvasbutton .c 10 60 90 140 "First\nButton" {
         .c itemconfigure label -text One
     }
     canvasbutton .c 110 60 190 140 "Second\nButton" {
         .c itemconfigure label -text Two
     }
     canvasbutton .c 210 60 290 140 "Third\nButton" {
         .c itemconfigure label -text Three
     }
     canvasbutton .c 240 160 290 190 "Quit" exit
 }
