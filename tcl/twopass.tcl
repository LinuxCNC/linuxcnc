# twopass.tcl:
#
# This file is sourced by haltcl when the inifile item HAL:TWOPASS
# is present and it evaluates all inifile HAL:HALFILE items in two passes.
# HALFILE items may be halcmd files (.hal) or .tcl files
#
# pass0:
#       All HAL:HALFILEs are read.
#       loadrt, loadusr commands are combined and executed at the end
#       of pass0 loadrt commands may be invoked multiple times for the
#       same mod_name.  The "count=", "num_chan=", and "names=" forms for
#       loadrt are supported but are mutually exclusive for each module.
#       addf commands are deferred to pass1
#
#       Some components (viz. pid) support a debug=dbg specifier on the
#       loadrt line.  dbg values are ORed together.
#
# pass1:
#       All HAL:HALFILES are reread, commands (except the loadrt and
#       loadusr completed commands) are executed and addf commands
#       are executed in order of occurrence.
#
# The inifile item HAL:TWOPASS can be any non-null string.  This string
# can be used to pass keywords for supported options:
#     verbose  -- enables extra reporting
#     nodelete -- disables deletion of temporary tcl files that
#                 are created from .hal files
#
# A few internal procs are useful by HALFILE tcl scripts:
#
# ::tp::passnumber   returns 0 | 1 for the current pass number
# ::tp::no_puts      disable the tcl "puts" command
# ::tp::alter_puts   alters  the tcl "puts" command so that it
#                    reports <pass0> or <pass1>
# ::tp::restore_puts restores the tcl "puts" command
#

#--------------------------------------------------------------------------
# Copyright: 2011
# Author:    Dewey Garrett <dgarrett@panix.com>
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
#--------------------------------------------------------------------------

namespace eval ::tp {
  namespace export passnumber
}

set ::TP(combine_addfs) 0 ;# alternate 1 ==> all addfs at end of pass1

# library procs:
source [file join $::env(HALLIB_DIR) hal_procs_lib.tcl]
#--------------------------------------------------------------------------
proc ::tp::passnumber {} {
  return $::TP(passnumber)
} ;# passnumber

proc ::tp::no_puts {} {
  rename puts orig_puts
  proc puts {args} {}
} ;# no_puts

proc ::tp::alter_puts {} {
  # puts ?-nonewline? ?channelId? string
  rename puts orig_puts
  switch [passnumber] {
  0 { proc puts {args} {
        set args [lreplace $args end end "<pass0> [lindex $args end]"]
        eval orig_puts "$args"
      }
    }
  1 { proc puts {args} {
        set args [lreplace $args end end "<pass1> [lindex $args end]"]
        eval orig_puts "$args"
      }
    }
   default {return -code error "alter_puts: unknown passno <$passno>"}
  }
} ;# alter_puts

proc ::tp::restore_puts {} {
  rename puts {}
  rename orig_puts puts
} ;# restore_puts

#--------------------------------------------------------------------------

proc ::tp::loadusr_substitute {args} {
  set pass [passnumber]
  #puts "loadusr_substitute<$pass> <$args>"
  if {$pass == 0} {
    #puts "loadusr_substitute<$pass> ignored"
  } else {
    eval orig_loadusr $args
  }
} ;# loadusr_substitute

proc ::tp::loadrt_substitute {arg1 args} {
  set arg1split [split $arg1]
  # detect multiple items in arg1
  if {[llength $arg1split] > 1} {
    #puts "loadrt_substitute arg1split=<$arg1split>"
    #example:                arg1split=<\{trivkins coordinates=xyz\}>
    set arg1split [string range $arg1 1 [expr -2 + [string len $arg1]]]
    set arg1args  ""
    for {set i 0} {$i < [llength $arg1split]} {incr i} {
      set arg1args [concat $arg1args [lindex $arg1split $i]]
    }
    set theargs [concat $arg1args $args]
  } else {
    set theargs [concat $arg1 $args]
  }
  set parms  [split $theargs]
  set module [lindex $parms 0]
  set pass   [passnumber]

  # keep track of loadrt for each module in order to detect
  # unsupportable loadrt calls in pass1. The ct is the number of
  # loadrt calls not the count total module instances
  if ![info exists ::TP($module,$pass,ct)] {
    set ::TP($module,$pass,ct) 1
  } else {
    incr ::TP($module,$pass,ct) 1
  }

  if {[lsearch -exact $::TP(loaded,modules) $module] >= 0} {
    if {$pass == 0} {
      return -code error "loadrt_substitute: <$module> already loaded"
    } else {
      if {$::TP($module,1,ct) > $::TP($module,0,ct)} {
        puts "loadrt_substitute:<$pass> Ignoring loadrt,\
              $module already loaded:"
        puts "                  <loadrt $parms>"
      }
      return
    }
  }
  if {$pass > 0} {
    if [catch {eval orig_loadrt $parms} msg] {
      puts "\ntwopass:loadrt_substitute parms=<$parms>\n$msg\n"
    }
    return
  }
  # pass0 only follows ------------------------------------
  if ![info exists ::TP(modules)] {
    lappend ::TP(modules) $module
  } else {
    if {[lsearch -exact $::TP(modules) $module] < 0} {
      lappend ::TP(modules) $module
    }
  }

  set parms   [lreplace $parms 0 0]
  foreach pair $parms {
    set l     [split $pair =]
    set item  [lindex $l 0]
    set value [lindex $l 1]

    if {("$item"=="count") || ("$item"=="num_chan") || ("$item"=="names")} {
      if ![info exists ::TP($module,form)] {
        set ::TP($module,form) $item
      } else {
        if {$::TP($module,form) != "$item"} {
          return -code error \
          "loadrt_substitute: cannot mix count=, num_chan= and names= forms\
          (module=$module, first used form=$::TP($module,form)"
        }
      }
    }

    switch "$item" {
      count {
              if ![info exists ::TP($module,count)] {
                set ::TP($module,count) $value
              } else {
                incr ::TP($module,count) $value
              }
            }
      num_chan {
              if ![info exists ::TP($module,num_chan)] {
                set ::TP($module,num_chan) $value
              } else {
                incr ::TP($module,num_chan) $value
              }
            }
      names {
              if ![info exists ::TP($module,names)] {
                set ::TP($module,names) $value
              } else {
                set ::TP($module,names) "$::TP($module,names),$value"
              }
            }
      personality {
              if ![info exists ::TP($module,personality)] {
                set ::TP($module,personality) $value
              } else {
                set ::TP($module,personality) "$::TP($module,personality),$value"
              }
            }
     debug  {
              if ![info exists ::TP($module,debug)] {
                set ::TP($module,debug) $value
              } else {
                # the pid component uses debug>1 to cause export
                # of additional pins (for all instances)
                #
                # here, logical OR multiple specifiers of debug=
                # so any setting of debug=1 will be honored (for
                # all instances)
                set ::TP($module,debug) [expr $::TP($module,debug) | $value]
              }
            }
     default {
               if ![info exists ::TP($module,other)] {
                 set ::TP($module,other) $pair
               } else {
                 set ::TP($module,other) "$::TP($module,other) $pair"
               }
             }
    }
  } ;# foreach pair
} ;# loadrt_substitute

proc ::tp::addf_substitute {args} {
  # syntax: addf func thread position
  set pass [passnumber]
  if {$pass == 0} {
    lappend ::TP(addf) $args
    #puts "addf_substitute:<$pass> Deferring <$args>"
  } else {
    #puts "addf_substitute:<$pass> Ignoring <$args>"
  }
} ;# addf_substitute

proc ::tp::hide_cmds {} {
  set ::TP(cmds) [hal --commands]

  # nochange,cmds:
  #   addf, loadrt, loadusr:  substituted directly herein
  #   list, gets: needed when procedure source_the_files sources
  #
  #   Note: 'list' and 'gets' are also conflictwords
  #   and substituted in procedure hal_to_tcl when converting
  #   a hal file to a temporary tcl file
  set ::TP(nochange,cmds) {addf loadrt loadusr source list gets}

  rename loadusr            orig_loadusr
  rename loadusr_substitute loadusr
  rename loadrt             orig_loadrt
  rename loadrt_substitute  loadrt
  rename addf               orig_addf
  rename addf_substitute    addf

  foreach cmd $::TP(cmds) {
    if {[lsearch -exact $::TP(nochange,cmds) $cmd] >= 0} continue
    rename $cmd orig_$cmd
    #puts "hide_cmds: renamed $cmd"
    #proc $cmd {args} [subst {puts "DUMMY $cmd <\$args>"}]
    proc $cmd {args} return
  }
} ;# hide_cmds

proc ::tp::unhide_cmds {} {
  foreach cmd $::TP(cmds) {
    if {[lsearch -exact $::TP(nochange,cmds) $cmd] >= 0} continue
    rename $cmd {}
    rename orig_$cmd $cmd
  }
  if {!$::TP(combine_addfs)} {
    # execute addf in place
    rename addf addf_done
    rename orig_addf addf
  }
} ;# unhide_cmds

proc ::tp::pass0 {} {
  verbose "pass0:BEGIN"
  if [info exists ::TP(passnumber)] {
    return -code error "pass0: unexpected passnumber <$::TP(passnumber)>"
  }
  set ::TP(passnumber) 0
  prep_the_files
  set ::TP(loaded,modules) ""
  hide_cmds
    source_the_files
    load_the_modules
  unhide_cmds
  verbose "pass0:END"
} ;# pass0

proc ::tp::prep_the_files {} {
  set passno [passnumber]
  if {$passno == 0} {
    # find file by search rules
    set libtag "LIB:"
    foreach file_plus_args $::HAL(HALFILE) {
      set f [lindex $file_plus_args 0]          ;# the file
      set f_argv [lrange $file_plus_args 1 end] ;# possibly has args
      if [catch {set foundfile [find_file_in_hallib_path $f]} msg] {
        puts "twopass:CANNOT FIND FILE FOR:$f"
        if [info exists ::env(PRINT_FILE)] {
           set fd [open $::env(PRINT_FILE) a]
           puts $fd "twopass:CANNOT FIND FILE FOR:$f"
           close $fd
        }
        exit 1
      }
      puts "twopass:found $foundfile"
      set f $foundfile

      # convert to a temporary tcl file if necessary
      set suffix [filesuffix $f]
      switch -exact $suffix {
        tcl {
             if {[llength $f_argv]} {
               set f "$f $f_argv" ;# optional args
             }
             lappend ::TP(runfiles) $f
             verbose "tclfile: $f"
            }
        hal {set ::TP($f,tmp) /tmp/[file tail $f].tmp
             verbose "convert $f to $::TP($f,tmp)"
             hal_to_tcl $f $::TP($f,tmp)
             lappend ::TP(runfiles) $::TP($f,tmp)
             set ::TP(origfile,$::TP($f,tmp)) $f
        }
        default {return -code error \
                "prep_the_files:unknown file type <$suffix>"}
      }
    }
  }
} ;# prep_the_files

proc ::tp::hal_to_tcl {ifile ofile} {
  # When hal files are specified with HAL:HALFILE,
  # try to make them work (preferred way is use tcl files).
  # This could probably be done with one regularexpression

  # Some hal commands conflict with tcl commands.
  # Converting a standard hal file here, so a warning is issued
  # if the possible use of one of these commands is detected.
  # Doubtful these are used much in .hal files to configure.
  # I couldn't find any usage examples.

  set ::TP(conflictwords) {list gets}
  # in a .tcl file, use "hal list" and "hal gets" instead

  if [catch {set fdin  [open $ifile r]
             set fdout [open $ofile w]
            } msg
     ] {
    puts "twopass: Error: $msg"
    exit 1
  }
  puts $fdout "# temporary tcl file generated by twopass.tcl"
  set lno 0
  while 1 {
    if [eof $fdin] break
    incr lno
    set theline [gets $fdin]
    set line [string trim $theline]
    if {"$line" == ""} continue
    if {[string first # $line] == 0} continue
    foreach suspect $::TP(conflictwords) {
      if {   ([string first "$suspect "  $line] == 0)
          || ([string first " $suspect " $line] >= 0)
        } {
         puts "hal_to_tcl:NOTE: in file $ifile, line $lno: \"$suspect\"\
         conflicts with haltcl usage,\nprepended with 'hal' for compatibility"
         puts "$lno:<$theline>"
         # prepend hal command to convert conflictword:
         set line "# hal_to_tcl prepended hal:\nhal $line"
      }
    }
    set idx 0
    while {$idx >= 0} {
      set l [string first \[ $line $idx]
      if {$l < 0} break
      set r [string first \] $line $idx]
      if {$r < 0} break
      set stanza [string range $line [expr $l + 1] [expr $r -1]]

      set new "[string range $line 0 [expr $l -1]]"
      set new "${new}\$::$stanza\("

      # handle [SECTION](VAR)<any char>
      if {[string range $line [expr $r+1] [expr $r+1]] == "("} {
        set r2 [string first \) $line $r]
        if {$r2 < 0} break
        set item [string range $line [expr $r+2] [expr $r2-1]]
        set new "${new}${item}\)"
        set idx [expr [string length $new] -1]
        set new "${new}[string range $line [expr $r2 +1] end]"
        set line $new
        continue
      }

      # handle [SECTION]VAR<whitespace>
      set s [string first " " $line $r]
      if {$s <0} {
        set item   [string range $line [expr $r + 1] end]
        set line "${new}${item}\) "
        set idx  -1
      } else {
        set item   [string range $line [expr $r + 1] [expr $s -1]]
        set new "${new}${item}\) "
        set idx [expr [string length $new] -1]
        set new "${new}[string range $line [expr $s +1] end]"
        set line $new
      }
    }
    # Anything following "#" on a line is a comment
    set cidx [string first "#" $line]
    if {$cidx > 0} {
      set notcomment "[string range $line 0 [expr -1 + $cidx]]"
      set    comment ";[string range $line $cidx end]"
      set line "$notcomment$comment"
    }
    puts $fdout $line
    if {[string trim "$theline"] != [string trim "$line"]} {
      verbose "converted hal line for tcl from $ifile:"
      verbose "   hal: $theline"
      verbose "   tcl: $line"
    }
  }
  close $fdin
  close $fdout
  return $ofile
} ;# hal_to_tcl

proc ::tp::source_the_files {} {
  foreach file_plus_args $::TP(runfiles) {
    catch {unset ::argv}
    set f [lindex $file_plus_args 0]
    if {[filesuffix $f] == "tcl"} {
      # note: ::argv supplies the file_plus_args to the sourced file
      #       ::argv0 is not set to maintain compatibility with
      #       the way the linuxcnc script uses haltcl to use tcl files
      set ::argv [lrange $file_plus_args 1 end]
    }
    verbose "sourcing: $f"
    set errct 0

    if [catch {source $f} msg] {
       if [info exists ::TP(origfile,$f)] {
         set f $::TP(origfile,$f)
       }
       puts "twopass: Error in file $f:\n    $msg"
       if { [string first "invalid command name" $msg] >= 0} {
          puts "Command not found using ::auto_path=\n$::auto_path"
       }
       incr errct
    }
  }
  if {$errct} {
    exit 1
  }
} ;# source_the_files

proc ::tp::filesuffix {f} {
  set dot [string last . $f]
  if {$dot < 0} {return -code error "filesuffix: no suffix <$f>"}
  return [string range $f [expr 1 + $dot ] end]
} ;# filesuffix

proc ::tp::load_the_modules {} {
  if ![info exists ::TP(modules)] {
    # no modules unlikely, but can occur in testing
    set ::TP(modules) ""
  }
  foreach m $::TP(modules) {
    set cmd "orig_loadrt $m" ;# this is the real loadrt
    if [info exists ::TP($m,count)] {
      set cmd "$cmd count=$::TP($m,count)"
    } elseif [info exists ::TP($m,num_chan)] {
      set cmd "$cmd num_chan=$::TP($m,num_chan)"
    } elseif [info exists ::TP($m,names)] {
      set cmd "$cmd names=$::TP($m,names)"
    }

    if [info exists ::TP($m,personality)] {
      set cmd "$cmd personality=$::TP($m,personality)"
    }
    if [info exists ::TP($m,debug)] {
      set cmd "$cmd debug=$::TP($m,debug)"
    }
    if [info exists ::TP($m,other)] {
      set cmd "$cmd $::TP($m,other)"
    }
    verbose "[string range $cmd 5 end]" ;# omit leading orig_
    if [catch { eval $cmd} msg] {
       puts "\ntwopass: load_the_modules cmd=<$cmd>\n$msg\n"
    }
  }
  set ::TP(loaded,modules) $::TP(modules)
  set ::TP(modules) ""
} ;# load_the_modules

proc ::tp::addf_the_funcs {} {
  foreach args $::TP(addf) {
    eval orig_addf $args
  }
} ;# addf_the_funcs

proc ::tp::pass1 {} {
  verbose "pass1:BEGIN"
  incr ::TP(passnumber)
  if { [info exists ::env(INI_FILE_NAME)]} {
    parse_ini $::env(INI_FILE_NAME)
  }
  source_the_files
  if {$::TP(combine_addfs)} {
    # execute all addf's at end of pass1
    addf_the_funcs
  }
  foreach name [array names ::TP *,tmp] {
    if $::tp::nodelete {
      verbose "nodelete: $::TP($name)"
    } else {
      verbose "deleting: $::TP($name)"
      file delete $::TP($name)
    }
  }
  verbose "pass1:END"
} ;# pass1

proc ::tp::verbose {msg} {
  if !$::tp::verbose return
  puts "twopass: $msg"
} ;# verbose

#----------------------------------------------------------------------
# begin
package require Linuxcnc ;# parse_ini
set ::tp::options ""
set ::tp::verbose 0
if {[string first verbose [string tolower $::HAL(TWOPASS)]] >=0} {
  set ::tp::verbose 1
  lappend ::tp::options verbose
}
set ::tp::nodelete 0
if {[string first nodelete [string tolower $::HAL(TWOPASS)]] >=0} {
  set ::tp::nodelete 1
  lappend ::tp::options nodelete
}

# the linuxcnc script exports LINUXCNC_TCL_DIR
# make sure it is first in case run-in-place with existing deb install
if [info exists ::auto_path] {
  # prepend:
  set ::auto_path [lreplace $::auto_path -1 -1 $::env(LINUXCNC_TCL_DIR)]
} else {
  set ::auto_path $::env(LINUXCNC_TCL_DIR)
}

puts "twopass:invoked with <$::tp::options> options"

::tp::pass0
::tp::pass1
