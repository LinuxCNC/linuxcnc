#!/bin/sh
# the next line restarts using emcsh \
exec $EMC2_EMCSH "$0" "$@"

###############################################################
# Description:  emccalib.tcl
#               A Tcl/Tk script that interfaces with EMC2 
#               through GUIs.  It configures ini tuning variables 
#               on the fly and saves if desired.
#
#  Author: Ray Henry
#  License: GPL Version 2
#
#  Copyright (c) 2003 All rights reserved.
#
#  Last change:
# $Revision$
# $Author$
# $Date$
###############################################################
# EMC parameter setting application
# Needs emcsh to run
# Saves changes to ini file if requested
###############################################################


# FIXME allow multiple reads of ini var.
# this is a one way system. It finds ini referenced variable in hal files
# and builds widgets to display and manipulate their values
# it does not and will not sort on hal components.

source [file join [file dirname [info script]] .. emc.tcl]

package require BWidget

# add a few default characteristics to the display
foreach class { Button Checkbutton Entry Label Listbox Menu Menubutton \
    Message Radiobutton Scale } {
    option add *$class.borderWidth 1  100
}

set numaxes [emc_ini "AXES" "TRAJ"]

# Find the name of the ini file used for this run.
set thisinifile "$EMC_INIFILE"
set thisconfigdir [file dirname $thisinifile]


#----------start toplevel----------
#
# trap mouse click on window manager delete
# might add a test for up to date ini before closing
wm protocol . WM_DELETE_WINDOW askKill
proc askKill {} {
    destroy .
    exit
}

# clean up a possible problems during shutdown
proc killHalConfig {} {
    global fid
    if {[info exists fid] && $fid != ""} {
        catch flush $fid
        catch close $fid
    }
    destroy .
    exit
}

# Wizard logo
set wizard_image_search {
    /etc/emc2/emc2-wizard.gif \
    /usr/local/etc/emc2/emc2-wizard.gif \
    emc2-wizard.gif
}
set logo ""
foreach wizard_image $wizard_image_search {
    if { [file exists $wizard_image] } {
        set logo [image create photo -file $wizard_image]
        break
    }
}

wm title . [msgcat::mc "EMC2 Axis Calibration"]
set logo [label .logo -image $logo]
set main [frame .main ]
set top [NoteBook .main.top]
pack $logo -side left -anchor nw
pack $main -side left -expand yes -fill both \
    -padx 18 -pady 18 -anchor n
pack $top -side top -fill both -expand yes
set terminate [frame $main.bot]
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
set menubar [menu $top.menubar -tearoff 0]
set filemenu [menu $menubar.file -tearoff 0]
$menubar add cascade -label [msgcat::mc "File"] \
    -menu $filemenu
$filemenu add command -label [msgcat::mc "Refresh"] \
    -command {refreshHAL}
$filemenu add command -label [msgcat::mc "Save"] \
    -command {saveHAL save}


# Make a text widget to hold the ini file
set initext [text $top.initext]
set haltext [text $top.haltext]
$initext config -state normal
$initext delete 1.0 end
if {[catch {open $thisinifile} programin]} {
    return 
} else {
    $initext insert end [read $programin]
    catch {close $programin}
}

# setup array with section names and line numbers
array set sectionarray {}
set sectionlist {}
scan [$initext index end] %d nl
set inilastline $nl
for {set i 1} {$i < $nl} {incr i} {
    if { [$initext get $i.0] == "\[" } {
        set inisectionname [$initext get $i.1 $i.end]
        set inisectionname [string trimright $inisectionname \] ]
        array set sectionarray "$inisectionname $i"
        lappend sectionlist $inisectionname
    }
}

array set sectionarray "END_OF_INI_FILE [expr $nl-1]"
lappend sectionlist "END_OF_INI_FILE"

# Find the HALFILE names between HAL and the next section
set startline $sectionarray(HAL)
set endline $sectionarray([lindex $sectionlist [expr [lsearch -exact $sectionlist HAL] + 1 ]])

set halfilelist ""
for {set i $startline} {$i < $endline} {incr i} {
    set thisstring [$initext get $i.0 $i.end]
    if { [lindex $thisstring 0] == "HALFILE" } {
        set thishalname [lindex $thisstring end]
        lappend halfilelist $thisconfigdir/$thishalname
    }
}

proc makeIniTune {} {
    global axisentry top initext sectionarray thisconfigdir haltext
    global numaxes ininamearray commandarray thisinifile halfilelist

    for {set j 0} {$j<$numaxes} {incr j} {
        global af$j
        set af$j [$top insert [expr $j+3] page$j \
        -text [msgcat::mc "Tune %d" $j]  -raisecmd "selectAxis $j" ]
    }
 
    # label display columns       
    for {set j 0} {$j < $numaxes} {incr j} {
        set col0 [label [set af$j].collabel0 -text [msgcat::mc "INI Name"]]
        set col1 [label [set af$j].collabel1 -text [msgcat::mc "HAL's Value"]]
        set col2 [label [set af$j].space -text "   "]
        set col3 [label [set af$j].collabel3 -text [msgcat::mc "Next Value"]]
        grid $col0 $col1 $col2 $col3 -ipady 5
    }

    foreach fname $halfilelist {
        $haltext config -state normal
        $haltext delete 1.0 end
        if {[catch {open $fname} programin]} {
            return
        } else {
            $haltext insert end [read $programin]
            catch {close $programin}
        }

        # find the ini references in this hal and build widgets        
        scan [$haltext index end] %d nl
        for {set i 1} { $i < $nl } {incr i} {
            set tmpstring [$haltext get $i.0 $i.end]
            if {[lsearch -regexp $tmpstring AXIS] > -1 && ![string match *#* $tmpstring]} {
	      set halcommand [split $tmpstring " "]
	      if { [lindex $halcommand 0] == "setp" || [lindex $halcommand 1] == "\=" } {
                        if { [lindex $halcommand 1] == "\=" } {
                            set tmpstring "setp [lindex $halcommand 0] [lindex $halcommand 2]"
                        }
                for {set j 0} {$j < $numaxes} {incr j} {
                    if {[lsearch -regexp $tmpstring AXIS_$j] > -1} {
                        # this is a hal file search ordered loop
                        set thisininame [string trimright [lindex [split $tmpstring "\]" ] end ]]
                        set lowername "[string tolower $thisininame]"
                        set thishalcommand [lindex $tmpstring 1]
                        set tmpval [expr [lindex [split \
                            [exec halcmd -s show param $thishalcommand] " "] 3]]
                        global axis$j-$lowername axis$j-$lowername-next
                        set axis$j-$lowername $tmpval
                        set axis$j-$lowername-next $tmpval
                        if { [catch { set thisname [label [set af$j].label-$j-$lowername \
                            -text "$thisininame:  " -width 20 -anchor e] } ] } {
                        } else {
                            set thisval [label [set af$j].entry-$lowername -width 8 \
                              -textvariable axis$j-$lowername -anchor e -foreg firebrick4 ]
                            set thisentry [entry [set af$j].next-$lowername -width 8 \
                              -width 12 -textvariable axis$j-$lowername-next ]
                            grid $thisname $thisval x $thisentry
                        }
                        lappend ininamearray($j) $lowername
                        lappend commandarray($j) $thishalcommand
                        break
                    }
                }
              }
            }
        }
    }

    # build the buttons to control axis variables.
    for {set j 0} {$j<$numaxes } {incr j} {
        set bframe [frame [set af$j].buttons]
        grid configure $bframe -columnspan 4 -sticky nsew 
        set tmptest [button $bframe.test -text [msgcat::mc "Test"] \
            -command {iniTuneButtonpress test}]
        set tmpok [button $bframe.ok -text [msgcat::mc "OK"] \
            -command {iniTuneButtonpress ok} -state disabled  ]
        set tmpcancel [button $bframe.cancel -text [msgcat::mc "Cancel"] \
            -command {iniTuneButtonpress cancel} -state disabled ]
        pack $tmpok $tmptest $tmpcancel -side left -fill both -expand yes -pady 5
    }
}

proc selectAxis {which} {
    global axisentry
    set axisentry $which
}

proc iniTuneButtonpress {which} {
    global axisentry ininamearray
    set labelname ".main.top.fpage$axisentry.next"
    set basename ".main.top.fpage$axisentry.buttons"
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
        default {
        }
    }
    foreach name [set ininamearray($axisentry)] {
        $labelname-$name configure -state $entrystate
    }
    changeIni $which
}

proc changeIni {how } {
    global axisentry sectionarray ininamearray commandarray HALCMD
    global numaxes main initext
    for {set i 0} {$i<$numaxes} {incr i} {
        global af$i
    }
    switch -- $how {
        cancel {-}
        test {
            set varnames [lindex [array get ininamearray $axisentry] end]
            set varcommands [lindex [array get commandarray $axisentry] end]
            set maxvarnum [llength $varnames]
            set swapvars $varnames
            # handle each variable and values inside loop 
            for {set listnum 0} {$listnum < $maxvarnum} {incr listnum} {
                set var "axis$axisentry-[lindex $varnames $listnum]"
                global $var $var-next
                # remove this item from the "to be swapped" list
                set thisvar [lindex $swapvars 0]
                set swapvars [lrange $swapvars 1 end]
                # get the values
                set oldval [set $var]
                set newval [set $var-next]
                # get the halcmd string
                set tmpcmd [lindex $varcommands $listnum]
                # use halcmd to set new parameter value in hal
                set thisret [exec $HALCMD setp $tmpcmd $newval]
                # see if we need to swap the new and old control values
                if {[lsearch -exact $swapvars $thisvar] < 0} {
                    # set the current value for display
                    set $var $newval
                    # set the tmp value as next in display
                    set $var-next $oldval
                }
            }        
        }
        ok {
            set varnames [lindex [array get ininamearray $axisentry] end]
            set varcommands [lindex [array get commandarray $axisentry] end]
            set maxvarnum [llength $varnames]
            # handle each variable and values inside loop 
            for {set listnum 0} {$listnum < $maxvarnum} {incr listnum} {
                set var "axis$axisentry-[lindex $varnames $listnum]"
                global $var $var-next
                # get the values
                set oldval [set $var]
                # set the tmp value as next in display
                set $var-next $oldval
            }        
        }
        quit {
            # build a check for changed values here and ask
            for {set j 0} {$j < $numaxes} {incr j} {
                set oldvals [lindex [array get valarray $j] 1]
                set cmds [lindex [array get commandarray $j] 1]
                set k 0
                foreach cmd $cmds {
                    set tmpval [expr [lindex [split \
                        [exec $HALCMD -s show param $cmd] " "] 3]]
                    set oldval [lindex $oldvals $k]
                    if {$tmpval != $oldval} {
                        set answer [tk_messageBox \
                            -message [msgcat::mc "The HAL parameter \n \
                            %s \n has changed. \n Really quit?" $cmd] \
                            -type yesno -icon question] 
                        switch -- $answer { \
                                yes exit
                                no {return}
                        }
                    
                    }
                    incr k
                }
            }
            destroy .
        }
        default {}
    }
}

proc saveIni {which} {
    global HALCMD thisconfigdir thisinifile
    global sectionarray ininamearray commandarray HALCMD
    global numaxes initext sectionlist
    
    if {![file writable $thisinifile]} {
        tk_messageBox -type ok -message [msgcat::mc "Not permitted to save here.\n\n \
            You need to copy a configuration to your home directory and work there."] 
        killHalConfig 
    }
    switch -- $which {
        tune {
            for {set i 0} {$i<$numaxes} {incr i} {
                global af$i
            }
            for {set i 0} {$i<$numaxes} {incr i} {
            set varnames [lindex [array get ininamearray $i] end]
                set upvarnames [string toupper $varnames]
                set varcommands [lindex [array get commandarray $i] end]
                set maxvarnum [llength $varnames]
                set sectname "AXIS_$i"
                # get the name of the next ini section
                set endsect [lindex $sectionlist [expr [lsearch -exact $sectionlist $sectname] + 1 ]]
                set sectnum "[set sectionarray($sectname)]"
                set nextsectnum "[set sectionarray($endsect)]"
                $initext configure -state normal
                for {set ind $sectnum} {$ind < $nextsectnum} {incr ind} {
                    switch -- [$initext get $ind.0] {
                        "#" {}
                        default {
                            set tmpstr [$initext get $ind.0 $ind.end]
                            set tmpvar [lindex $tmpstr 0]
                            set tmpindx [lsearch $upvarnames $tmpvar]
                            if {$tmpindx != -1} {
                                set cmd [lindex $varcommands $tmpindx]
                                $initext mark set insert $ind.0
                                set newval [expr [lindex [split \
                                    [exec $HALCMD -s show param $cmd] " "] 3]]
                                set tmptest [string first "e" $newval]
                                if {[string first "e" $newval] > 1} {
                                    set newval [format %f $newval]
                                }
                                regsub {(^.*=[ \t]*)[^ \t]*(.*)} $tmpstr "\\1$newval\\2" newvar
                                $initext delete insert "insert lineend"
                                $initext insert insert $newvar
                            }
                        }
                    }
                }
                $initext configure -state disabled
            }
            saveFile $thisinifile $initext
        }
        quit {
            killHalConfig
        }
    }
}

proc saveFile {filename contents} {
    catch {file copy -force $filename $filename.bak}
    if { [catch {open $filename w} fileout] } {
        puts stdout [msgcat::mc "can't save %s" $halconfFilename($name)]
        return
    }
    switch -- [string index $contents 0] {
        "/" {
            # a filename
            puts "I got a filename"
        }
        "." {
            # this is a widget name so get contents
            puts $fileout [$contents get 1.0 end]
            catch {close $fileout}
        }
        default {
            # assumes that contents is plain text so stuff it away
            puts $fileout $contents
            catch {close $fileout}
        }
    }    
}    

makeIniTune
$top raise page0




