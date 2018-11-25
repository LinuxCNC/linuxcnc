# hal_procs_lib.tcl
package require Hal ;# provides hal commands

proc pin_exists {name} {
  # return 1 if pin exists
  if { [lindex [hal list pin "$name"] 0] == "$name"} {return 1}
  return 0
} ;# pin_exists

proc connection_info {result_name pinname } {
  # return 0 if pinname not found, else 1
  # result_name is associative array with details
  #  owner type direction value separator signame
  upvar $result_name ay
  set ans [hal show pin $pinname]
  set lines [split $ans \n]
  set sig ""
  set sep ""
  set ct [scan [lindex $lines 2] "%s %s %s %s %s %s %s" \
                                 owner type dir val name sep sig]
  if {$ct <0} {
    return 0
  }
  set ay(owner)     $owner
  set ay(type)      $type
  set ay(direction) $dir
  set ay(value)     $val
  set ay(separator) $sep
  set ay(signame)   $sig
  return 1
} ;# connection_info

proc is_connected {pinname {signame {} } } {
  # return is_input or is_output or is_io or not_connected
  # set signame to signal name if connected
  upvar $signame thesig
  set ans [hal show pin $pinname]
  set lines [split $ans \n]
  set thesig ""
  set sep ""
  set ct [scan [lindex $lines 2] "%s %s %s %s %s %s %s" \
                                 owner type dir val name sep thesig]
  if {$ct <0}          {return not_connected}
  if {"$sep" == "<=="} {return is_input}
  if {"$sep" == "==>"} {return is_output}
  if {"$sep" == "<=>"} {return is_io}
  return "not_connected"
} ;# is_connected

proc thread_info {ay_name} {
  # return details about threads in associative array ay_name
  # items for each $threadname:
  #       ($threadname,$componentname)  $threadname index for $componentname
  #       ($threadname,fp)              $threadname uses floatingpoint
  #       ($threadname,period)          $threadname period
  #       ($threadname,index,last)      $threadname last index used
  #
  # motion specific items:
  #       (motion-command-handler,threadname) threadname
  #       (motion-command-handler,index)      index
  #       (motion-controller,threadname)      threadname
  #       (motion-controller,index)           index
  # other:
  #       (threadnames)                        list of all threanames
  #
  # return string is $ay(motion-controller,threadname)

  upvar $ay_name ay
  set ans [hal show thread]
  set lines [split $ans \n]
  set header_len 2
  set lines [lreplace $lines 0 [expr $header_len -1]]
  set lines [lreplace $lines end end]
  set remainder ""
  set ct 0
  foreach line $lines {
    catch {unset f1 f2 f3 f4}
    scan [lindex $lines $ct] \
                "%s %s %s %s" \
                 f1 f2 f3 f4
    if ![info exists f3] {
      set index $f1; set compname $f2
      set ay($threadname,$compname)  $index
      set ay($threadname,index,last) $index
      if {"$compname" == "motion-command-handler"} {
        set ay(motion-command-handler,threadname) $threadname
        set ay(motion-command-handler,index)      $index
      }
      if {"$compname" == "motion-controller"} {
        set ay(motion-controller,threadname) $threadname
        set ay(motion-controller,index)      $index
      }
    } else {
      set period $f1; set fp $f2; set threadname $f3
      lappend ay(threadnames) $threadname
      set ay($threadname,fp) $fp
      set ay($threadname,period) $period
    }
    incr ct
  }
  if [info exists ay(motion-controller,threadname)] {
    if [info exists ay(motion-command-handler,threadname)] {
      if {   "$ay(motion-controller,threadname)" \
          != "$ay(motion-command-handler,threadname)" \
         } {
        return -code error "thread_info: mot funcs on separate threads"
        if {  $ay(motion-command-handler,index) \
            > $ay(motion-controller,index) \
           } {
          return -code error "thread_info: mot funcs OUT-OF-SEQUENCE"
        }
      }
      return $ay(motion-controller,threadname)
    } else {
      return -code error "thread_info: motion-controller not found"
    }
  }
} ;# thread_info

proc get_netlist {inpins_name outpin_name iopins_name signame} {
  # input:   signame
  # outputs:
  #          inpins_name list of input pins for signame or ""
  #          iopins_name list of io pins for signame    or ""
  #          outpin_name output pin for signame         or ""
  upvar $inpins_name inpins
  upvar $iopins_name iopins
  upvar $outpin_name outpin

  set inpins ""
  set iopins ""
  set outpin ""

  set header_len 2
  set lines [split [hal show signal $signame] \n]
  set lines [lreplace $lines 0 [expr $header_len -1]]
  set lines [lreplace $lines end end]
  set ct 0
  foreach line $lines {set l($ct) [string trim $line];incr ct}
  set ct_max $ct
  set ct 0
  for {set ct 0} {$ct < $ct_max} {incr ct} {
    set v0 [lindex $l($ct) 0]
    set v1 [lindex $l($ct) 1]
    set v2 [lindex $l($ct) 2]
    switch $v0 {
       float -
       bit   -
       u32   -
       s32   {set sname  $v2} ;# set on 1st non-header line
       "==>" {}
       "<==" {}
       "<=>" {}
       *     {return -code error "get_netlist: Unexpected <$line">}
    }
    if {"$signame"=="$sname"} {
      switch $v0 {
         "==>" {lappend inpins $v1}
         "<==" {lappend outpin $v1}
         "<=>" {lappend iopins $v1}
      }
    }
  }
} ;# get_netlist

proc find_file_in_hallib_path {filename {inifile .}} {
  # find halfile using path HALLIB_PATH=.:HALLIB_DIR (see scripts/linuxcnc.in)
  set libtag LIB: ;# special prefix for explicit use of hallib file
  set halname [lindex $filename end]
  if {[string first "$libtag" $halname] == 0} {
    # explicit $libtag used
    set halname [string range $halname [string len $libtag] end]
    set usehalname [file join $::env(HALLIB_DIR) $halname]
  } else {
    if {[file pathtype $filename] == "absolute"} {
      set usehalname $filename
    } else {
      # relative file specifier (relative to ini file directory)
      set usehalname [file join [file dirname $inifile] $halname]
      if ![file readable $usehalname] {
        # use ::env(HALLIB_DIR)
        set usehalname [file join $::env(HALLIB_DIR) $halname]
      }
    }
  }
  if [file readable $usehalname] {
    return $usehalname
  }
  return -code error "find_file_in_hallib_path: cannot find: <$filename>"
} ;# find_file_in_hallib_path
