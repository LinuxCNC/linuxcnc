#!/bin/sh
# the next line restarts using wish \
exec wish "$0" "$@"

###############################################################
# Description:  genedit.tcl
#               This is a simple TK text editor.
#
#  Derived from a work by Fred Proctor & Will Shackleford
#  License: GPL Version 2
#
#  Copyright (c) 2005-2009 All rights reserved.
###############################################################
# genedit.tcl
#
# geneditStart <name> ?<initial file>? ?<browser types>? runs an editor
# named <name>. Optional parameters are the <initial file> to open,
# default "untitled.txt", and the <browser types>, default
# {{"All files" *} {Text files} {.txt}}
#
# geneditStart creates a top-level window named ".<name>", so if you call
# "geneditStart myeditor", you'll have ".myeditor" as a top-level window.
# You can have as many as you want. Sub-widgets include:
#   .<name>.textframe.textwin
#   .<name>.textframe.scrolly
#   .<name>.menubar.file
#   .<name>.menubar.edit
#   .<name>.menubar.help
#
# You can pack additional widgets in .<name>, and access the widgets above
# by name to extract things like the insertion point, selected text, etc.
# Mods to fix copy, paste, delete add find and line number by rh 12/1999
# Mods to add line numbering and find and replace by rh 12/1999.
# Mod for a script menu that looks for *.ncw files in $linuxcnc::TCL_SCRIPT_DIR directory.
###############################################################

# Load the linuxcnc.tcl file, which defines variables for various useful paths
source [file join [file dirname [info script]] .. linuxcnc.tcl]

proc geneditStart {name {ifilename "untitled.txt"} {itypes { {"All files" *} {"Text files" {.txt} }}}} {

    global geneditFilename geneditTypes textwin TCLSCRIPTS

    if {[winfo exists .$name]} {
        wm deiconify .$name
        raise .$name
        focus .$name
        return
    }

    set ed .$name
    toplevel $ed

    # initialize filename and browser types
    set geneditFilename($name) $ifilename
    set geneditTypes($name) $itypes

    wm title $ed [set geneditFilename($name)]

    set textframe $ed.textframe
    frame $textframe
    set textwin $textframe.textwin
    # rh replaced -setgrid true with padx 4 for easier left margin read.
    text $textwin -width 80 -height 24 -padx 4 -wrap word -yscrollcommand \
            "geneditScrolltext $textframe"
    set scrolly $textframe.scrolly
    scrollbar $scrolly -orient vert -command "$textwin yview"
    pack $scrolly -side right -fill y
    pack $textwin -side top -fill both -expand true
    pack $textframe -side top

    set menubar $ed.menubar
    menu $menubar -tearoff 0

    set filemenu $menubar.file
    menu $filemenu -tearoff 0
    $menubar add cascade -label [msgcat::mc "File"] -menu $filemenu -underline 0
    $filemenu add command -label [msgcat::mc "New"] -underline 0 \
            -command "geneditNewFile $name" -accelerator "Ctrl+N"
    $filemenu add command -label [msgcat::mc "Open..."] -underline 0 \
            -command "geneditOpenFile $name" -accelerator "Ctrl+O"
    $filemenu add command -label [msgcat::mc "Save"] -underline 0 \
            -command "geneditSaveFile $name" -accelerator "Ctrl+S"
    $filemenu add command -label [msgcat::mc "Save As..."] -underline 5 \
            -command "geneditSaveFileAs $name"
    $filemenu add separator
    $filemenu add command -label [msgcat::mc "Exit"] -command "destroy $ed" -underline 1

    set editmenu $menubar.edit
    menu $editmenu -tearoff 0
    $menubar add cascade -label [msgcat::mc "Edit"] -menu $editmenu -underline 0
    $editmenu add command -label [msgcat::mc "Cut"] -underline 2 \
            -command "geneditCutIt $textwin" -accelerator "Ctrl+X"
    $editmenu add command -label [msgcat::mc "Copy"] -underline 0 \
            -command "geneditCopyIt $textwin" -accelerator "Ctrl+C"
    $editmenu add command -label [msgcat::mc "Paste"] -underline 0 \
            -command "geneditPasteIt $textwin" -accelerator "Ctrl+V"
    $editmenu add separator
    $editmenu add command -label [msgcat::mc "Select All"] -underline 7 \
            -command "focus $textwin; geneditSelectAll $textwin" -accelerator "Ctrl+A"

    set helpmenu $menubar.help
    menu $helpmenu -tearoff 0
    $menubar add cascade -label [msgcat::mc "Help"] -menu $helpmenu -underline 0
    $helpmenu add command -label [msgcat::mc "About..."] -underline 0 \
            -command "geneditShowAbout $name"

    $ed configure -menu $menubar

    bind $ed <Control-n> "geneditNewFile $name; break"
    bind $ed <Control-o> "geneditOpenFile $name; break"
    bind $ed <Control-s> "geneditSaveFile $name; break"
    bind $ed <Control-h> "geneditShowAbout $name; break"

    bind $textwin <Control-c> "geneditCopyIt $textwin; break"
    bind $textwin <Control-v> "geneditPasteIt $textwin; break"
    bind $textwin <Control-x> "geneditCutIt $textwin; break"
    bind $textwin <Control-a> "geneditSelectAll $textwin; break"

    # insert contents of filename, if it exists
    if {! [catch {open $geneditFilename($name)} filein]} {
        $ed.textframe.textwin delete 1.0 end
        $ed.textframe.textwin insert end [read $filein]
        catch {close $filein}
    }

    # Extra menu items for program editor
    if {$name == "programEditor"} {
        $filemenu add separator
        $filemenu add command -label [msgcat::mc "Save and Reload"] -command "geneditSaveFile $name; loadProgramText" -underline 1

        $editmenu add separator
        $editmenu add command -label [msgcat::mc "Find..."] -underline 0 \
                -command "geneditEnterText $textwin"
        $editmenu add command -label [msgcat::mc "Renumber File..."] -underline 0 \
                -command "geneditNumber"

        set settingsmenu $menubar.settings
        menu $settingsmenu -tearoff 0
        $menubar add cascade -label [msgcat::mc "Settings"] -menu $settingsmenu -underline 0
        $settingsmenu add command -label [msgcat::mc "No Numbering"] -underline 0 \
                -command "set startnumbering 0"
        $settingsmenu add separator
        $settingsmenu add command -label [msgcat::mc "Line Numbering..."] -underline 0 \
                -command "geneditSetLineNumber"

        # adds a script menu that looks for *.ncw files and adds their filename to script menu
        set scriptmenu $menubar.script
        menu $scriptmenu
        $menubar add cascade -label [msgcat::mc "Scripts"] -menu $scriptmenu -underline 1
        #replaced scriptdir
	#set scriptdir tcl/scripts
        set files [exec /bin/ls $linuxcnc::TCL_SCRIPT_DIR]
	foreach file $files {
    	    if {[string match *.ncw $file]} {
        	set geneditfname [file rootname $file]
            	$scriptmenu add command -label $geneditfname -command "source $linuxcnc::TCL_SCRIPT_DIR/$file"
    	    }
    	}
    }
}

proc geneditScrolltext {tf a b} {
    $tf.scrolly set $a $b
}

proc geneditShowAbout {name} {
    set ed .$name

    if {[winfo exists $ed.about]} {
        wm deiconify $ed.about
        raise $ed.about
        focus $ed.about
        return
    }
    toplevel $ed.about
    wm title $ed.about [msgcat::mc "About TkEditor"]
    message $ed.about.msg -aspect 1000 -justify center -font {Helvetica 12 bold} \
            -text [msgcat::mc "TkEditor\n\nSimple Tcl/Tk Text Editor\n\nGPL Version 2"]
    button $ed.about.ok -text [msgcat::mc "OK"] -command "destroy $ed.about"
    pack $ed.about.msg $ed.about.ok -side top
    bind $ed.about <Return> "destroy $ed.about"
}

proc geneditOpenFile {name} {
    global geneditFilename geneditTypes programEntry
    set ed .$name
    set fname [tk_getOpenFile -initialdir [file dirname $geneditFilename($name)] -filetypes $geneditTypes($name)]
    set geneditFileDirectory [file dirname $fname]
    set geneditFilename($name) [file dirname $fname]
    if {[string length $fname] == 0} {
        return
    }

    $ed.textframe.textwin delete 1.0 end
    if {[catch {open $fname} filein]} {
        puts stdout [msgcat::mc "can't open %s" $fname]
    } else {
        $ed.textframe.textwin insert end [read $filein]
        catch {close $filein}
        set geneditFilename($name) $fname
        wm title $ed [set geneditFilename($name)]
    }
}

proc geneditSaveFile {name} {
    global geneditFilename
    set ed .$name
    catch {file copy -force $geneditFilename($name) $geneditFilename($name).bak}
    if {[catch {open $geneditFilename($name) w} fileout]} {
        puts stdout [msgcat::mc "can't save %s" $geneditFilename($name)]
        return
    }
    puts $fileout [$ed.textframe.textwin get 1.0 end]
    catch {close $fileout}
}

proc geneditNewFile {name} {
    set ed .$name
    if {[string length [geneditSaveFileAs $name]]} {
        $ed.textframe.textwin delete 1.0 end
    }
}

proc geneditSaveFileAs {name} {
    global geneditFilename geneditTypes
    set ed .$name
    set fname [tk_getSaveFile -filetypes $geneditTypes($name) \
            -initialfile $geneditFilename($name)]
    if {[string length $fname] == 0} {
        return
    }
    set geneditFilename($name) $fname
    wm title $ed [set geneditFilename($name)]
    geneditSaveFile $name
    return $fname
}

proc geneditCutIt {w} {
    global selecttext
    set selecttext [selection get STRING]
    $w delete "insert - [string length $selecttext] chars" insert
}

proc geneditCopyIt {w} {
    global selecttext
    set selecttext [selection get STRING]
    # should drop text tags here
    # should disable copy until a paste
}

proc geneditPasteIt {w} {
    global selecttext
    $w insert insert $selecttext
    # should set copy menubutton to normal here
}

proc geneditSelectAll {w} {
    event generate $w <Control-slash>
}

# Find text processes - geneditEnterText includes hard location from top right.
proc geneditEnterText {ed} {
    global textwin
    toplevel $ed.find
    grab $ed.find
    wm title $ed.find [msgcat::mc "Find"]
    wm geometry $ed.find 325x150-50+100

    label $ed.find.label1 -text [msgcat::mc "Find:"] -anchor e
    place $ed.find.label1 -x 5 -y 5 -width 80 -height 20

    entry $ed.find.entry1 -relief sunken -textvariable sword
    place $ed.find.entry1 -x 90  -y 5 -width 110 -height 20

    label $ed.find.label2 -text [msgcat::mc "Replace:"] -anchor e
    place $ed.find.label2 -x 5 -y 30 -width 80 -height 20

    entry $ed.find.entry2 -relief sunken -textvariable rword
    place $ed.find.entry2 -x 90 -y 30 -width 110 -height 20

    button $ed.find.button1 -text [msgcat::mc "Find All"] -command {geneditFindAll $sword}
    place $ed.find.button1 -x 5 -y 70 -width 150 -height 30

    button $ed.find.button2 -text [msgcat::mc "Replace All"] -command {geneditReplaceAll $sword $rword}
    place $ed.find.button2 -x 5 -y 110 -width 150 -height 30

    button $ed.find.button3 -text [msgcat::mc "Skip This"] -command {geneditSkipWord $sword}
    place $ed.find.button3 -x 170 -y 70 -width 150 -height 30

    button $ed.find.button4 -text [msgcat::mc "Replace This"] -command {geneditReplaceWord $sword $rword}
    place $ed.find.button4 -x 170 -y 110 -width 150 -height 30

    button $ed.find.button5 -text [msgcat::mc "Cancel"] -command "focus -force $textwin; destroy $ed.find"
    place $ed.find.button5 -x 220 -y 5 -width 100 -height 30

    button $ed.find.button6 -text [msgcat::mc "Clear"] -command {
        $textwin tag delete $sword
        $textwin tag delete q
    }
    place $ed.find.button6 -x 220 -y 35 -width 100 -height 30

    # set focus to entry widget 1
    focus $ed.find.entry1

}

proc geneditFindAll {sword} {
    global textwin
    set firstplace 1.0
    set l1 [string length $sword]
    scan [$textwin index end] %d nl
    set thisplace [$textwin index insert]
    for {set i 1} {$i < $nl} {incr i} {
        $textwin mark set last $i.end
        set lastplace [$textwin index last]
        set thisplace [$textwin search -forwards -nocase $sword $thisplace $lastplace]
        if {$thisplace != ""} {
            $textwin mark set insert $thisplace
            scan [$textwin index "insert + $l1 chars"] %f lastplace
            $textwin tag add $sword $thisplace $lastplace
            $textwin tag configure $sword -background lightblue
            $textwin mark set insert "insert + $l1 chars"
            set thisplace $lastplace
        } else {
            set thisplace $lastplace
        }
    }
    $textwin mark set insert 1.0
    geneditNextWord $sword
}

proc geneditNextWord {sword} {
    global textwin
    set findnext [$textwin tag nextrange $sword insert]
    if {$findnext == ""} {
        $textwin mark set insert 1.0
        $textwin see insert
        return
    }
    set start [lindex $findnext 0]
    set last [lindex $findnext end]
    catch {$textwin mark set insert $start}
    $textwin tag add q $start $last
    $textwin tag raise q
    $textwin tag configure q -background darkred -foreground white
    $textwin see "insert + 5 lines"
}

proc geneditSkipWord {sword} {
    global textwin
    set l1 [string length $sword]
    $textwin tag remove q insert "insert + $l1 chars"
    $textwin tag remove $sword insert "insert + $l1 chars"
    geneditNextWord $sword
}

proc geneditReplaceWord {sword rword} {
    global textwin
    set l1 [string length $sword]
    set l2 [string length $rword]
    $textwin tag remove q insert "insert + $l1 chars"
    $textwin tag remove $sword insert "insert + $l1 chars"
    $textwin delete insert "insert + $l1 chars"
    $textwin insert insert $rword
    $textwin mark set insert "insert + $l2 chars"
    geneditNextWord $sword
}

proc geneditReplaceAll {sword rword} {
    global textwin
    set l1 [string length $sword]
    set l2 [string length $rword]
    scan [$textwin index end] %d nl
    set thisplace [$textwin index 1.0]
    for {set i 1} {$i < $nl} {incr i} {
        $textwin mark set last $i.end
        set lastplace [$textwin index last]
        set thisplace [$textwin search -forwards -nocase $sword $thisplace $lastplace]
        if {$thisplace != ""} {
            $textwin mark set insert $thisplace
            $textwin delete insert "insert + $l1 chars"
            $textwin insert insert $rword
            $textwin mark set insert "insert + $l2 chars"
            set thisplace [$textwin index insert]
        } else {
            set thisplace $lastplace
        }
    }
}


# These are three variables used by the line numbering routine.
# They can be changed for a run by using the settings menu.
# If you want to set a default value different from what comes up change these here.

# Any positive integer can be used for lineincrement.
# Values of 1,2,5,10 show and can be accessed from settings menu radiobuttons.
# A 0 startnumbering value means lines will not be numbered when enter is pressed.
set startnumbering 0

# Space refers to the distance between n words and other text. Tab space is set
# here but could be single or double space.  Change what's between the "".
set space "     "

# Number refers to the start up value of line numbering.
set number 0
set lineincrement 10

proc geneditLineIncrement {} {
    global startnumbering number lineincrement space textwin
    if {$startnumbering != 0} {
        $textwin insert insert "n$number$space"
        incr number $lineincrement
    }
}

# geneditSetLineNumber also uses a hard coded popup location from top right.
proc geneditSetLineNumber {} {
    global  startnumbering number lineincrement textwin
    toplevel $textwin.linenumber
    wm title $textwin.linenumber [msgcat::mc "Set Line Numbering"]
    wm geometry $textwin.linenumber 280x160-50+100
    label $textwin.linenumber.label1 -text [msgcat::mc "Increment"]
    place $textwin.linenumber.label1 -x 5 -y 5
    radiobutton $textwin.linenumber.incr1 -text [msgcat::mc "One"] \
            -variable lineincrement -value 1 -anchor w
    place $textwin.linenumber.incr1 -x 10 -y 25 -width 80 -height 20
    radiobutton $textwin.linenumber.incr2 -text [msgcat::mc "Two"] \
            -variable lineincrement -value 2 -anchor w
    place $textwin.linenumber.incr2 -x 10 -y 45 -width 80 -height 20
    radiobutton $textwin.linenumber.incr5 -text [msgcat::mc "Five"] \
            -variable lineincrement -value 5 -anchor w
    place $textwin.linenumber.incr5 -x 10 -y 65 -width 80 -height 20
    radiobutton $textwin.linenumber.incr10 -text [msgcat::mc "Ten"] \
            -variable lineincrement -value 10 -anchor w
    place $textwin.linenumber.incr10 -x 10 -y 85 -width 80 -height 20

    label $textwin.linenumber.label2 -text [msgcat::mc "Space"]
    place $textwin.linenumber.label2 -x 130 -y 5
    radiobutton $textwin.linenumber.space1 -text [msgcat::mc "Single Space"] \
            -variable space -value { } -anchor w
    place $textwin.linenumber.space1 -x 140 -y 25
    radiobutton $textwin.linenumber.space2 -text [msgcat::mc "Double Space"] \
            -variable space -value {  } -anchor w
    place $textwin.linenumber.space2 -x 140 -y 45
    radiobutton $textwin.linenumber.space3 -text [msgcat::mc "Tab Space"] \
            -variable space -value {    } -anchor w
    place $textwin.linenumber.space3 -x 140 -y 65
    button $textwin.linenumber.ok -text [msgcat::mc "OK"] -command "destroy $textwin.linenumber" \
            -height 1 -width 9
    place $textwin.linenumber.ok -x 180 -y 127
    label $textwin.linenumber.label3 -text [msgcat::mc "Next Number:"] -anchor e
    place $textwin.linenumber.label3 -x 5 -y 130 -width 95
    entry $textwin.linenumber.entry -width 6 -textvariable number
    place $textwin.linenumber.entry -x 100 -y 130

    tkwait visibility $textwin.linenumber
    grab $textwin.linenumber
    focus -force $textwin.linenumber.entry
    set temp [expr $number - $lineincrement]
    if {$temp > 0} {
        set number $temp
    } else {
        set number 0
    }
    set startnumbering 1
    bind $textwin <KeyRelease-Return> {geneditLineIncrement}
}

proc geneditNumber {} {
    global textwin
    geneditSetLineNumber
    button $textwin.linenumber.renumber -text [msgcat::mc "Renumber"] -command geneditReNumber \
            -height 1 -width 9
    place $textwin.linenumber.renumber -x 160 -y 96
}


# String match with a while loop [0-9 tab space] 1 if true 0 if no match
proc geneditReNumber {} {
    global textwin number lineincrement space
    scan [$textwin index end] %d nl
    for {set i 1} {$i < $nl} {incr i} {
        if {$number > 99999} {set number 0}
        $textwin insert $i.0 n$number$space
        set l1 [string length n$number$space]
        $textwin mark set insert "$i.$l1"
        set character [$textwin get insert]
        if {$character == "/"} {
            $textwin insert $i.0 "/"
            $textwin delete insert
        }
        set character [$textwin get insert]
        if {$character == "n" || $character == "N"} {
            set firstplace [$textwin index insert]

            # find the last number in the n word
            $textwin mark set insert "insert + 1 chars"
            set character [$textwin get insert]
            while {[string match {[0-9]} $character] == 1} {
                $textwin mark set insert "insert + 1 chars"
                set character [$textwin get insert]
            }

            # find the first character of the next word using space and tab
            while {$character == " " || $character == " "} {
                $textwin mark set insert "insert + 1 chars"
                set character [$textwin get insert]
            }
            $textwin delete "$firstplace" "insert"
        }
        incr number $lineincrement
    }
    set startnumbering 0
}

# if we're not running inside tkemc, then pop us up in root window
if {! [info exists tkemc]} {
    geneditStart genEditor
}

