#!/bin/sh
# the next line restarts using emcsh \
exec $LINUXCNC_EMCSH "$0" "$@"

###############################################################
# Description:  emccalib.tcl
#               A Tcl/Tk script that interfaces with LinuxCNC
#               through GUIs.  It configures ini tuning variables
#               on the fly and saves if desired.
#
#  Author: Ray Henry
#  License: GPL Version 2
#
#  Copyright (c) 2003-2009 All rights reserved.
###############################################################
# EMC parameter setting application
# Needs emcsh to run
# Saves changes to ini file if requested
###############################################################


# FIXME allow multiple reads of ini var.
# this is a one way system. It finds ini referenced variable in hal files
# and builds widgets to display and manipulate their values
# it does not and will not sort on hal components.

source [file join [file dirname [info script]] .. linuxcnc.tcl]
eval emc_init $argv

source [file join $::env(HALLIB_DIR) hal_procs_lib.tcl]

package require BWidget

# add a few default characteristics to the display
foreach class { Button Checkbutton Entry Label Listbox Menu Menubutton \
    Message Radiobutton Scale } {
    option add *$class.borderWidth 1  100
}

set ::EC(numjoints) [emc_ini "JOINTS" "KINS"]

#--------------------------------------------------------------
# Tuning stanzas
set ::EC(stanzas)  [list TUNE JOINT_ AXIS_]

# Specify a stanza for tuning with:
# ::EC(stanza_name,howmany)  == number of allowed names
# ::EC(stanza_name,suffixes) == list of allowed suffixes
#
# Special case: howmany==1 and items=="", no allowed suffixes
#
# Examples:
#  item==JOINT_ specifies name/value pairs like:
#               [JOINT_n]name=value
#               where n is a suffix in {0 1 ...}
#
#  item==TUNE   using howmany==1 and items=="" specfies
#               name/value pairs with no suffix like:
#               [TUNE]name=value

set ::EC(TUNE,howmany)  1
set ::EC(TUNE,suffixes) ""

set ::EC(JOINT_,howmany) $::EC(numjoints)
for {set i 0} {$i < $::EC(JOINT_,howmany)} {incr i} {
  lappend ::EC(JOINT_,suffixes) $i
}

set ::EC(AXIS_,suffixes) {X Y Z A B C U V W}
set ::EC(AXIS_,howmany)  [llength $::EC(AXIS_,suffixes)]
#--------------------------------------------------------------

# Find the name of the ini file used for this run.
set ::EC(thisinifile) "$::EMC_INIFILE"
set ::EC(thisconfigdir) [file dirname $::EC(thisinifile)]


#----------start toplevel----------
#
# trap mouse click on window manager delete
# might add a test for up to date ini before closing
wm protocol . WM_DELETE_WINDOW askKill
proc askKill {} {
    destroy .
    exit
}

# Wizard logo
set lname $::env(LINUXCNC_HOME)/share/linuxcnc/linuxcnc-wizard.gif
if { [file exists $lname] } {
   set logo [image create photo -file $lname]
} else {
   set logo ""
}

wm title . [msgcat::mc "LinuxCNC Calibration"]
set logo [label .logo -image $logo]
set ::EC(main) [frame .main ]
set ::EC(nbook) [NoteBook .main.top]
pack $logo -side left -anchor nw
pack $::EC(main) -side left -expand yes -fill both \
    -padx 18 -pady 18 -anchor n
pack $::EC(nbook) -side top -fill both -expand yes
set terminate [frame $::EC(main).bot]
pack $terminate -side top -fill x
button $terminate.save -text [msgcat::mc "Save To File"] -command {saveIni tune}
button $terminate.quit -text [msgcat::mc "Quit"] -command {saveIni quit}
pack $terminate.save  $terminate.quit \
    -pady 4 -side left -fill both -expand yes

# remove slider
# slider process is used for several widgets
proc sSlide {f a b} {
    $f.sc set $a $b
}

# Build menu
# fixme clean up the underlines so they are unique under each set
set menubar [menu $::EC(nbook).menubar -tearoff 0]
#. configure -menu $menubar
set filemenu [menu $menubar.file -tearoff 0]
$menubar add cascade -label [msgcat::mc "File"] \
    -menu $filemenu
$filemenu add command -label [msgcat::mc "Refresh"] \
    -command {refreshHAL}
$filemenu add command -label [msgcat::mc "Save"] \
    -command {saveHAL save}

# Make a text widget to hold the ini file
set ::EC(initext) [text $::EC(nbook).initext]
set ::EC(haltext) [text $::EC(nbook).haltext]
$::EC(initext) config -state normal
$::EC(initext) delete 1.0 end
set programin [open $::EC(thisinifile)]
$::EC(initext) insert end [read $programin]
close $programin

# setup array with section names and line numbers
set ::EC(sectionlist) {}
scan [$::EC(initext) index end] %d nl
set inilastline $nl
for {set i 1} {$i < $nl} {incr i} {
    if { [$::EC(initext) get $i.0] == "\[" } {
        set inisectionname [$::EC(initext) get $i.1 $i.end]
        set tab "	"
        set spc " "
        set inisectionname [string trimright $inisectionname "$spc$tab]" ]
        set ::EC(sectionline,$inisectionname) $i
        lappend ::EC(sectionlist) $inisectionname
    }
}

set     ::EC(sectionline,END_OF_INI_FILE) [expr $nl-1]
lappend ::EC(sectionlist) END_OF_INI_FILE

# Find the HALFILE names between HAL and the next section
set startline $::EC(sectionline,HAL)
set endline   $::EC(sectionline,[lindex $::EC(sectionlist) \
              [expr [lsearch -exact $::EC(sectionlist) HAL] + 1 ]])

set ::EC(halfilelist) ""

for {set i $startline} {$i < $endline} {incr i} {
    set thisstring [string trim [$::EC(initext) get $i.0 $i.end]]
    if {   ([string first         "HALFILE" "$thisstring"] == 0)
        || ([string first "POSTGUI_HALFILE" "$thisstring"] == 0)
       } {
        set arglist [lindex [split $thisstring =] 1]
        set thishalname [lindex $arglist 0]
        lappend ::EC(halfilelist) [find_file_in_hallib_path $thishalname $::EC(thisinifile)]
    }
}

proc find_ini_refs {stanza} {
    # find the ini references in this hal and build widgets
    scan [$::EC(haltext) index end] %d nl
    for {set i 1} { $i < $nl } {incr i} {
        set tmpstring [string trim [$::EC(haltext) get $i.0 $i.end]]
        if {[string match *${stanza}* $tmpstring] && ![string match *#* $tmpstring]} {
          set halcommand [split $tmpstring " \t"]
          if { [lindex $halcommand 0] == "setp" || [lindex $halcommand 1] == "\=" } {
            if { [lindex $halcommand 1] == "\=" } {
                set tmpstring "setp [lindex $halcommand 0] [lindex $halcommand 2]"
            }
            for {set sfx 0} {$sfx < $::EC($stanza,howmany)} {incr sfx} {
                set tabno $::EC($stanza,$sfx,tabno)
                set itag [lindex $::EC($stanza,suffixes) $sfx]
                if {[string match *${stanza}${itag}* $tmpstring]} {
                    # this is a hal file search ordered loop
                    set thisininame [string trimright [lindex [split $tmpstring "\]" ] end ]]
                    set lowername "[string tolower $thisininame]"
                    set thishalcommand [lindex $tmpstring 1]
                    set tmpval [string trim [hal getp [halcmdSubstitute $thishalcommand]]]
                    set ::EC(value,$tabno,$lowername)      $tmpval
                    set ::EC(value,$tabno,$lowername,next) $tmpval
                    if { [catch { set thisname [label $::EC(tab,$tabno).label-$itag-$lowername \
                        -text "$thisininame:  " -width 20 -anchor e] } msg ] } {
                        #puts msg=$msg
                    } else {
                        set thisval [label $::EC(tab,$tabno).entry-$lowername -width 8 \
                          -textvariable ::EC(value,$tabno,$lowername) -anchor e -foreg firebrick4 ]
                        set thisentry [entry $::EC(tab,$tabno).next-$lowername -width 8 \
                          -width 12 -textvariable ::EC(value,$tabno,$lowername,next) ]
                        grid $thisname $thisval x $thisentry
                    }
                    lappend ::EC(ininamearray,$tabno) $lowername
                    lappend ::EC(commandarray,$tabno) $thishalcommand
                    break
                }
            }
          }
        }
    }
} ;# find_ini_refs

proc makeIniTune {} {
    set tabno 0
    foreach stanza $::EC(stanzas) {
        for {set sfx 0} {$sfx < $::EC($stanza,howmany)} {incr sfx} {
            set ::EC($stanza,$sfx,tabno) $tabno
            set ::EC($tabno,stanza) $stanza ;# cross reference tabno-->stanza
            set itag [lindex $::EC($stanza,suffixes) $sfx]

            if {$::EC($stanza,howmany) == 1} {
                set tablabel ${stanza}
            } else {
                set tablabel ${stanza}$itag
            }

            set ::EC(tab,$tabno) [$::EC(nbook) insert [expr $tabno+3] page$tabno \
                -text $tablabel -raisecmd "selectTab $tabno" ]
            set col0 [label $::EC(tab,$tabno).collabel0 -text [msgcat::mc "INI Name"]]
            set col1 [label $::EC(tab,$tabno).collabel1 -text [msgcat::mc "HAL's Value"]]
            set col2 [label $::EC(tab,$tabno).space -text "   "]
            set col3 [label $::EC(tab,$tabno).collabel3 -text [msgcat::mc "Next Value"]]
            grid $col0 $col1 $col2 $col3 -ipady 5
            grid rowconfigure $::EC(tab,$tabno) 9999 -weight 1
            incr tabno
        }
    }

    foreach fname $::EC(halfilelist) {
        $::EC(haltext) config -state normal
        $::EC(haltext) delete 1.0 end
        if {[catch {open $fname} programin]} {
            continue
        } else {
            $::EC(haltext) insert end [read $programin]
            catch {close $programin}
        }
        foreach stanza $::EC(stanzas) {
            find_ini_refs $stanza
        }
    }
    if {[array names ::EC ininamearray,*] == ""} {
        incompatible_ini_file
        return
    }

    # build the buttons to control variables.
    foreach stanza $::EC(stanzas) {
        for {set sfx 0} {$sfx < $::EC($stanza,howmany)} {incr sfx} {
            set itag [lindex $::EC($stanza,suffixes) $sfx]
            set tabno $::EC($stanza,$sfx,tabno)
            if ![info exists ::EC(startpagenum)] {
                set ::EC(startpagenum) $tabno
            }
            if ![info exists ::EC(ininamearray,$tabno)] {
                $::EC(nbook) delete page$tabno
                if {$tabno == $::EC(startpagenum)} {
                    unset ::EC(startpagenum)
                }
                continue
            }
            set bframe [frame $::EC(tab,$tabno).buttons]
            grid configure $bframe -columnspan 4 -sticky nsew
            set tmptest [button $bframe.test -text [msgcat::mc "Test"] \
                -command {iniTuneButtonpress test}]
            set tmpok [button $bframe.ok -text [msgcat::mc "OK"] \
                -command {iniTuneButtonpress ok} -state disabled  ]
            set tmpcancel [button $bframe.cancel -text [msgcat::mc "Cancel"] \
                -command {iniTuneButtonpress cancel} -state disabled ]
            set tmprefresh [button $bframe.refresh -text [msgcat::mc "Refresh"] \
                -command {iniTuneButtonpress refresh}]
            pack $tmpok $tmptest $tmpcancel $tmprefresh -side left -fill both -expand yes -pady 5
        }
    }
} ;# makeIniTune

proc incompatible_ini_file {} {
   set progname [file tail $::argv0]
   set fname [file tail $::EMC_INIFILE]
   set answer [tk_messageBox \
      -title "Ini file not compatible with $progname" \
      -message [msgcat::mc "\
               No Tuneable items in ini file:\n\
               $fname\n\n\
               Sections supported are:\n\
               \[JOINT_N\]name=value\n\
               \[AXIS_L\]name=value\n\
               \[TUNE\]name=value\n\n\
               N is a  joint number\n\
               L is an axis letter\n\n\
               A Halfile must include a setp\n\
               for a suppored section item\n\n\
               Inifile example:\n\
               \[JOINT_0\]\n\
               PGAIN=1\n\
               ...\n\
               \[TUNE\]\n\
               SPEED=100\n\
               ...\n\n\
               Halfile example:\n\
               setp pid.0.Pgain \[JOINT_0\]PGAIN\n\
               setp mux2.0.in1 \[TUNE\]SPEED\n\
               ...\n\n\
               \n\
               " ] \
      -type ok \
      -icon info]
      exit 1
} ;# incompatible_ini_file

proc selectTab {which} {
    set ::curtab $which ;# current tab number
}

proc iniTuneButtonpress {which} {
    set labelname ".main.top.fpage$::curtab.next"
    set basename ".main.top.fpage$::curtab.buttons"
    # which can be (test, ok, cancel)
    switch -- $which {
        test {
            $basename.test configure -state disabled
            $basename.ok configure -state normal
            $basename.cancel configure -state normal
            set entrystate "disabled"
        }
        cancel {
            $basename.test configure -state normal
            $basename.ok configure -state disabled
            $basename.cancel configure -state disabled
            set entrystate "normal"
        }
        ok {
            $basename.test configure -state normal
            $basename.ok configure -state disabled
            $basename.cancel configure -state disabled
            set entrystate "normal"
        }
        refresh {
            $basename.test configure -state normal
            $basename.ok configure -state disabled
            $basename.cancel configure -state disabled
            set entrystate "normal"
        }
        default {
        }
    }
    foreach name $::EC(ininamearray,$::curtab) {
        $labelname-$name configure -state $entrystate
    }
    changeIni $which $::EC($::curtab,stanza)
} ;# iniTuneButtonpress

proc changeIni {how stanza} {
    switch -- $how {
        cancel {-}
        test {
            set varnames    $::EC(ininamearray,$::curtab)
            set varcommands $::EC(commandarray,$::curtab)
            set maxvarnum   [llength $varnames]
            set swapvars $varnames
            # handle each variable and values inside loop
            for {set listnum 0} {$listnum < $maxvarnum} {incr listnum} {
                set vname   [lindex $varnames $listnum]
                set var     ::EC(value,$::curtab,$vname)
                set varnext ::EC(value,$::curtab,$vname,next)
                # remove this item from the "to be swapped" list
                set thisvar [lindex $swapvars 0]
                set swapvars [lrange $swapvars 1 end]
                # get the values
                set oldval [set $var]
                set newval [set $varnext]
                # get the parameter name string
                set tmpcmd [lindex $varcommands $listnum]
                # set new parameter value in hal
                set thisret [hal setp [halcmdSubstitute $tmpcmd] $newval]
                # see if we need to swap the new and old control values
                if {[lsearch -exact $swapvars $thisvar] < 0} {
                    # set the current value for display
                    set $var $newval
                    # set the tmp value as next in display
                    set $varnext $oldval
                }
            }
        }
        ok {
            set varnames    $::EC(ininamearray,$::curtab)
            set varcommands $::EC(commandarray,$::curtab)
            set maxvarnum   [llength $varnames]
            # handle each variable and values inside loop
            for {set listnum 0} {$listnum < $maxvarnum} {incr listnum} {
                set vname   [lindex $varnames $listnum]
                set var     ::EC(value,$::curtab,$vname)
                set varnext ::EC(value,$::curtab,$vname,next)
                # get the values
                set oldval [set $var]
                # set the tmp value as next in display
                set $varnext $oldval
            }
        }
        refresh {
            set varnames    $::EC(ininamearray,$::curtab)
            set varcommands $::EC(commandarray,$::curtab)
            set maxvarnum   [llength $varnames]
            # handle each variable and values inside loop
            for {set listnum 0} {$listnum < $maxvarnum} {incr listnum} {
                set vname   [lindex $varnames $listnum]
                set var     ::EC(value,$::curtab,$vname)
                set varnext ::EC(value,$::curtab,$vname,next)
                # get the parameter name string
                set tmpcmd [lindex $varcommands $listnum]
                # get parameter value from hal
                set val [string trim [hal getp [halcmdSubstitute $tmpcmd]]]
                set $var     $val
                set $varnext $val
            }
        }
        quit {
            puts stderr "Unexpected changeIni quit"
            # was never called, removed deadcode here
        }
        default {}
    }
} ;# changeIni

proc saveIni {which} {
    if {![file writable $::EC(thisinifile)]} {
        tk_messageBox -type ok -message [msgcat::mc "Not permitted to save here.\n\n \
            You need to copy a configuration to your home directory and work there."]
    }
    switch -- $which {
        tune {
            update_initext $::EC($::curtab,stanza)
            saveFile $::EC(thisinifile) $::EC(initext)
        }
        quit {destroy .; exit}
    }
} ;# saveIni

proc update_initext {stanza} {
    set anames [array names ::EC ininamearray,*]
    foreach stanza $::EC(stanzas) {
        for {set sfx 0} {$sfx < $::EC($stanza,howmany)} {incr sfx} {
            set itag [lindex $::EC($stanza,suffixes) $sfx]
            set tabno $::EC($stanza,$sfx,tabno)
            if {[lsearch $anames ininamearray,$tabno] < 0} continue
            set varnames    $::EC(ininamearray,$tabno)
            set upvarnames  [string toupper $varnames]
            set varcommands $::EC(commandarray,$tabno)
            set maxvarnum   [llength $varnames]
            set sectname    "${stanza}$itag"
            # get the name of the next ini section
            set endsect [lindex $::EC(sectionlist) \
                [expr [lsearch -exact $::EC(sectionlist) $sectname] + 1 ]]
            set sectnum "[set ::EC(sectionline,$sectname)]"
            set nextsectnum "[set ::EC(sectionline,$endsect)]"
            $::EC(initext) configure -state normal
            for {set ind $sectnum} {$ind < $nextsectnum} {incr ind} {
                switch -- [$::EC(initext) get $ind.0] {
                    "#" {}
                    default {
                        set tmpstr [$::EC(initext) get $ind.0 $ind.end]
                        set tmpvar [lindex [split $tmpstr "="] 0]
                        set tmpvar [string trim $tmpvar]
                        set tmpindx [lsearch $upvarnames $tmpvar]
                        if {$tmpindx != -1} {
                            set cmd [lindex $varcommands $tmpindx]
                            $::EC(initext) mark set insert $ind.0
                            set newval [string trim [hal getp [halcmdSubstitute $cmd]]]
                            set tmptest [string first "e" $newval]
                            if {[string first "e" $newval] > 1} {
                                set newval [format %f $newval]
                            }
                            regsub {(^.*=[ \t]*)[^ \t]*(.*)} $tmpstr "\\1$newval\\2" newvar
                            $::EC(initext) delete insert "insert lineend"
                            $::EC(initext) insert insert $newvar
                        }
                    }
                }
            }
            $::EC(initext) configure -state disabled
        }
    }
} ;# update_initext

proc halcmdSubstitute {s} {
    set pat {\[([^]]+)\](\([^)]+\)|[^() \r\n\t]+)}
    set pos 0
    set result {}
    while {$pos < [string length $s]
            && [regexp -start $pos -indices $pat $s indx]} {
        set start [lindex $indx 0]
        set end [lindex $indx 1]

        append result [string range $s $pos [expr $start-1]]

        set query [string range $s $start $end]
        regexp $pat $query - section var
        set var [regsub {\((.*)\)} $var {\1}]
        append result [emc_ini $var $section]

        set pos [expr $end+1]
    }
    append result [string range $s $pos end]
    return $result
} ;# halcmdSubstitute

proc saveFile {filename contents} {
    catch {file copy -force $filename $filename.bak}
    if { [catch {open $filename w} fileout] } {
        puts stdout [msgcat::mc "can't save %s" $halconfFilename($name)]
        return
    }
    if {-1 != [string first ".ini.expanded" $filename]} {
       set fname [file tail $filename]
       set answer [tk_messageBox \
          -title "Expanded File" \
          -message [msgcat::mc "\
                   Save to: <$fname>\n\n\
                   Expanded file for \#INCLUDEs\n\n\
                   \#INCLUDE files (*.inc)\n\
                   must be edited separately\n\
                   \n\
                   " ] \
          -type ok \
          -icon info]
    }
    switch -- [string index $contents 0] {
        "/" {
            # a filename
            puts "I got a filename"
        }
        "." {
            # "." means this is a widget name so get contents
            set lines [$contents count -lines 1.0 end]
            # was: puts $fileout [$contents get 1.0 end]
            # avoid writing empty lines at end of file:
            incr lines -1
            set last ${lines}.end
            puts $fileout [$contents get 1.0 $last]
            catch {close $fileout}
        }
        default {
            # assumes that contents is plain text so stuff it away
            puts $fileout $contents
            catch {close $fileout}
        }
    }
} ;# saveFile

makeIniTune
$::EC(nbook) raise page$::EC(startpagenum)

