#!/usr/bin/wish

#-----------------------------------------------------------------------
# ngcgui.tcl is a front-end gui that reads one or more single function
# gcode subroutine files, provides user prompts for parameters for an
# arbitrary number of invocations, and creates a single output file
# of gcode.

# ngcgui can be run as a standalone application or its functionality
# can be embedded in a parent tcl application including the axis gui.

# Example standalone Usage, create link:
#  $ ln -s somewhere/ngcgui.tcl directory_in_your_PATH/ngcgui
#
# Usage:
#   ngcgui --help | -?
#   ngcgui [Options] -D nc_files_directory_name
#   ngcgui [Options] -i LinuxCNC_inifile_name
#   ngcgui [Options]
#
#   Options:
#          [-S subroutine_file]
#          [-p preamble_file]
#          [-P postamble_file]
#          [-o output_file]
#          [-a autosend_file]            (autosend to axis default:auto.ngc)
#          [--noauto]                    (no autosend to axis)
#          [-N | --nom2]                 (no m2 terminator (use %))
#          [--font [big|small|fontspec]] (default: "Helvetica -10 bold")
#          [--horiz|--vert]              (default: --horiz)
#          [--cwidth comment_width]      (width of comment field)
#          [--vwidth varname_width]      (width of varname field)
#          [--quiet]                     (fewer comments in outfile)
#          [--noiframe]                  (default: frame displays image)
#
#-----------------------------------------------------------------------
# ngcgui was first developed on git-master version 2.4.0-pre
# named "O" words available since: LinuxCNC 2.3.0, April 19, 2009

#-----------------------------------------------------------------------
# Copyright: 2010-2013
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
# Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
#-----------------------------------------------------------------------

# ngcgui allows a user to write subroutine files that contain
# a single subroutine as described in
#      3.7 Calling Files of the LinuxCNC ________ manual
# and then use or test them with a gui frontend that simplifies
# user entry of calling arguments (positional parameters #1,#2,...)

# If the subroutine includes lines to equate positional parameters
# (#n) to named parameters (#<parmname>) on special association lines like:
#
#    #<parmname> = #n (optional_comment_text)
#
# then the positional parameter will be supplemented with the more
# descriptive #<parmname> in the gui entry box and any optional_comment_text
# will be included in the gui.  Use of the descriptive #<parmname> in
# the body of the subroutine will make it more readable but is not
# mandatory.
#
# When this format is used, the order of appearance of the positional
# parameters must be monotonically increasing with no omissions.  This
# helps to prevent user errors in assignment of parmnames to parameters.
#
# A default value can also be specified on the special association line like:
#    #<parmname> = #n (=dvalue)
#  or
#    #<parmname> = #n (=dvalue optional_comment_text)

# All positional parameters used in the body of the subroutine must be
# entered -- an error occurs if an item  entry is missing when a feature
# is made with "Create Feature"

# The linuxcnc gcode language does not provide a mechanism for returning
# results so subroutines must set global parameters for results.
# Within ngcgui, _globals with names that contain a colon (:) character
# are ignored in the creation of  entry boxes.
# For example, a subroutine called from a Subfile named o<line> returns
# results in globals like: #<_line:theta>, $<_line:length>, etc.
# This feature can be used to hide globals from entry boxes for any purpose
# or for communication between routines

# Workflow (for standalone usage):

#  1) The directory location for ngc gcode files used in linuxcnc is specified
#     in the ini file by: [DISPLAY]PROGRAM_PREFIX.
#     In linuxcnc2.5, multiple directories can be specified using
#     [RS274NGC]SUBROUTINE_PATH if

#  2) Candidate subroutine files for use with this utility should contain
#     a single subroutine as described in:
#          3.7 Calling Files of the LinuxCNC ________ manual

#  3) Optionally, user supplies a Preamble file of gcode
#     No substitutions are performed on this file

#  4) User specifies a subroutine file (Subfile).
#     Entry boxes are created for each positional parameter

#  5) Optionally, user supplies a Postamble file of gcode
#     No substitutions are performed on this file

#  6) "Create Feature" Button adds feature to queue for output file.
#     The gui will verify that all positional parameters are not
#     null but makes no checks on values.

#  7) "Finalize" button prompts for filename, and writes output file
#     for all features and adds a terminating m2

#  8) After finalizing the file, the user may send the file to
#     the axis gui with the SendFileToAxis button.  If axis is not running,
#     an error is displayed.  User should verify axis state before
#     sending.  Errors detected by axis are shown within the axis
#     application.

#  9) To create a file with multiple sections from one or more
#     subroutine files:
#       a) enter values for Preamble, Subfile, Postamble
#       b) fill in positional parameters
#       d) "Create Feature" number_1

#       e) If this this the only feature, select "Finalize" to write
#          the file.  Then select "SendFileToAxis" to send the file to axis
#          or "Create Feature" to start a new file

#       f) For multiple features, continue:
#          enter different parameter values
#            or
#          specify new values for Preamble, Subfile, Postamble
#          and fill in the new entry box values
#       g) "Create Feature" number_2
#       h) Repeat f),g) for all features
#       i) "Finalize" the file (as above)

#     The Preamble and Postamble files are optional, for example one
#     might specify the Preamble only for the first subroutine and the
#     Postamble only for the last subroutine in making a output file
#     for a set of features with common parameters specified in a
#     single preamble file of features.

# Options:
#   "Retain values on Subfile read"
#        After opening a Subfile (and creating an output file) a second
#        Subfile (third,fourth, ...) may be opened while retaining values
#        for positional parameters where the names are
#        _matched_ in the subsequent file.  This is useful when
#        testing new subroutines and may be useful when combining multiple
#        feature routines if they share parameters with common names like
#        "#<zsafe>", "#<zstart>", etc.
#        Values for _numbered_ positional parameters (#n) without a name
#        association are never retained.

#   "Expand subroutine"
#        When checked, subroutines are expanded in the
#        output file.  This allows the axis_gui to highlight
#        gcode lines in the text window when paths are left-clicked in
#        the 3D window (and vice-vera) when subroutines are used.
#        In expanding subroutines, labels within are made unique
#        to avoid name collision with labels in other expansions or
#        other included subroutines.  Only one level of subroutine
#        expansion is performed.  If the interpreter detects an error, it
#        is sometimes unclear where it occurs when subroutines are called.
#        Expanding the Subfile and rerunning often gives a line number
#        as an aid in finding the problem.
#
#        When not checked, subroutines are called and not expanded.

# Button Shortcut bindings:
#   Preamble, Subfile, Postamble buttons
#     Instead of using the button and file selection dialog, enter
#     a new file name in the associated entry and <Return> to open
#     and read different file.  When the filename differs from the
#     currently laoded file, the filename text changes color.
#     This shortcut is useful when you are debugging/editing one of the
#     input files -- enter a <Return> in the corresponding entry item
#     for the filename to reload the file.

# Notes:
#   0. configuring ngcgui is simplified with linuxcnc2.5; support for
#      linuxcnc2.4 will cease when linuxcnc2.5 is released

#   1. ngcgui supports subroutine files that contain a _single_
#      subroutine in a file where the name of the subroutine
#      is the same as the name of the file.
#      ex:
#          $ cat rect.ngc
#          o<rect> sub
#          ...
#          o<rect> endsub
#      Only comments and empty lines may appear before sub or after endsub

#   2. The parameters passed to a subroutine (Postional parameters)
#      are identified as "Numbered parameters" #1,#2,...,#n with
#      n <= 30
#      ngcgui finds any instances of #1,...,#30 and identifies
#      each as a positional parameter for invocation of the subroutine.
#      So, if you have a subroutine with 3 parameters (#1,#2,#3),
#      it is not a good idea to use parameters like #4 or #30 in the
#      body of the routine since they will increase the number of
#      entry-box items in the ngcgui front-end and cause great confusion.
#
#      In the manual:
#         "O- call takes up to 30 optional arguments, which are passed
#         to the subroutine as #1, #2, ..., #N.  Parameters from #N+1 to
#          #30 have the same value as in the calling context."

#   3. LinuxCNC gcode supports labels for conditional blocks and subroutines
#      in both "Numbered" (ex: o100) and "Named" (ex: o<l101>) forms.
#      Support for the "Numbered" label format is included, but
#      it would be clearer to limit ngcgui support to:
#          Positional Parametrs --> #1, ..., #n   1<=n<=30
#          Named Labels         --> o<label_name>
#      This seems consistent with the trajectory of LinuxCNC gcode and
#      accomodation of earlier styles (numbered labels like
#      #n+1 to #30) is a small matter of editing:).

#   4. removed

#   5. If a file (subfile,preamble,postamble) is removed or modified by
#      another application (like an editor), the color for its name will
#      change to notify the ngcgui user that it should probably be reloaded.

#   6. The preamble file is provided to support simple setup actions
#      like g20/g21,g40 etc.  Similarly, the postamble file supports
#      terminating actions as required like m5.
#      The preamble and postamble file can be more complex even
#      including subroutines.  Such inclusion requires care
#      by the user if multiple files are used to make a single output
#      file with ngcgui because if a file containing subroutines
#      is included more than once, a multiple definition error is
#      flagged.  The user can avoid this by carefully selecting/deselecting
#      preamble/postamble files but a better course is to avoid
#      subroutines in these files and rely on a library of "subroutine-only"
#      files in the [DISPLAY]PROGRAM_PREFIX directory.

#   7. ngcgui inserts a special global variable named #<_feature:> that begins
#      with a value of 0 and is incremented for each added feature.  This
#      _global can be tested in subroutines; no entry box is created for it.

#      Similarly, ngcgui inserts a special global variable named #<_remaining_features:>
#      that indicates the number of features remaining after the current feature.
#      A value of zero indicates the current feature is the last feature.

#   8. entry boxes for positional parameters include key bindings
#      for keys x,y,z,a,b,c,u,v,w, and d.  When embedded in axis, typing these keys
#      cause the current value (emc_rel_act_pos) to be entered into the
#      entry box.  This function makes it simple to enter current coordinate
#      values. The d key will enter the 2*x for the diameter on a lathe)
#
#      (If there is a tcl global ::entrykeybinding proc, it will
#      be used instead for these key bindings so that other embedding
#      applications can handle these keys -- see the source for the parameters
#      passed to the proc.)

#   9. lines before the o<>sub line and after the o<>endsub line must
#      be comments (enclosed in parentheses) or begun with a semicolon (;)

#  10. each time an output file is finished, ngcgui saves a copy in
#      /tmp/ngcgui_bak/ just in case you want to see it or reuse it later
#      The /tmp directory is normally purged at restart or after
#      a number of days determined by the variable TMPTIME in
#      the system file /etc/default/rcS (ubuntu for example)

#  11. key bindings
#      Escape return to Preview page (only if embed_in_axis)
#      Ctrl-a Toggle autosend
#      Ctrl-c Clear entries
#      Ctrl-d Set entries to default values
#      Ctrl-e Open editor specified by $VISUAL on last outfile
#      Ctrl-f Create feature
#      Ctrl-F Finalize
#      Ctrl-k Show key bindings
#      Ctrl-n Restart (cancel pending)
#      Ctrl-p (re)Read Preamble
#      Ctrl-P (re)Read Postamble
#      Ctrl-r (re)Read Subfile
#      Ctrl-s Show status
#      Ctrl-S Show full status (debug info)
#      Ctrl-u Open editor specified by $VISUAL on current subfile
#      Ctrl-U Open editor specified by $VISUAL on current preamble

#  12. All entry boxes are checked for valid numbers and the entry is
#      turned red if invalid.

#  13. Emc gcode (2.3 19apr09) allows a single semicolon use for comments.
#      This gui supports semicolon comments but the syntax for special
#      association lines requires the () form:
#
#      for positional parameters 1<=n<=30:
#         #<parmname>     = #n (=defaultvalue comment_text)

#  14. Features requiring linuxcnc-2.4pre (that I can remember):
#      a) error detection when sending file to axis

#  15. Helper subroutine files that are included in the
#      [DISPLAY]PROGRAM_PREFIX (or the[RS274NGC]SUBROUTINE_PATH)
#      directory may not be suitable for use as a subfile.
#      To indicate this to a user, include a special comment line:
#          (not_a_subfile)
#      Alternatively, these files can be placed in a different
#      directory specified in the ini file [WIZARD]WIZARD_ROOT

# 16. Using a launcher (like ubuntu gnome destop launcher) doesn't
#     make it easy to pass in environmental variables like VISUAL.
#     This works for a launcher: put ngcgui.tcl in a directory
#     such as /home/yourname/bin and create script such as
#     $ cat /home/yourname/bin/launch_ngc
#     #!/bin/sh
#     export VISUAL=gedit ;# your favorite editor
#     /home/yourname/bin/ngcgui.tcl -a auto -i your inifile
#
#     make it executable:
#     $ chmod 755 /home/yourname/bin/launch_ngc
#     configure the launcher so the command is:
#     Command: /home/yourname/bin/launch_ngc
#
# 17. obsolete: xembed support removed, internal embedding works better
#
# 18. If --vwidth 0 is used and a parameter has no comment, the variable
#     name is placed in the comment field
#
# 19. For linuxcnc 2.4, the tcl proc embed_in_axis_tab will embed directly
#     in an axis tab using [DISPLAY]USER_COMMAND_FILE (or ~/.axisrc)
#     example:
#     w = widgets.right.insert("end", 'ngcgui', text='Ngcgui')
#     w.configure(borderwidth=1, highlightthickness=0)
#     f = Tkinter.Frame(w, container=0, borderwidth=0, highlightthickness=0)
#     f.pack(fill="both", expand=1, anchor="nw",side="top")
#     root_window.tk.call("source","somepath/ngcgui.tcl")
#     root_window.tk.call("::ngcgui::embed_in_axis_tab",f,"nameof_ngcgui_subfile")
#
# 20. The Preamble and Postamble entry fields may be used to insert
#     immediate gcode commands instead of reading files.  The immediate
#     syntax is signaled by a leading colon (:), commands are separated by
#     semicolons (;).  Example:
#          :t0m6;(debug, pausing);m0 (pause)
#     The commands are not validated by ngcgui but are added to the
#     output gcode file
#
# 21. When embedding in axis directly,  multiple tabpages can be specified.  Each
#     can be used independently to add multiple features from the initial or
#     newly selected subfiles.  If multiple tabpages have created features, the
#     Finalize action will offer to finalize all tabpages in left-to-right order.
#     Beware of this ordering.  If the order is incorrect, cancel and then
#     rearrange page order before finalizing.
#
# 22. Subfiles can optionally include a special comment:
#     (info: info_text)
#     The info text will be displayed (embed_in_axis only)
#
# 23. An optional image file (.png,.gif,.jpg,.pgm) can accompany a subfile.
#     The image file can help clarify the parameters; a window displaying
#     the image is popped up when the subfile is read.  The image file
#     should be in the same directory as the subfile and have the same
#     name with an appropriate image suffix, e.g. the subfile iquad.ngc
#     should be accompanied by an image file iquad.png
#
# 24. When ngcgui pages are embedded in the axis gui, options can
#     be specified:
#     NGCGUI_OPTIONS = opt1 opt2 ...
#     opt items:
#       nonew      -- disallow making   new tab page
#       noremove   -- disallow removing any tab page
#       noauto     -- noautosend (makeFile, then manually send)
#       noiframe   -- put image inside a toplevel instead of a frame
#                     so all controls are available
#       nom2       -- no m2 terminator (use %)
#
# 25. When ngcgui pages are embedded in the axis gui and the user
#     is allowed to open new subroutines, the initial starting directroy
#     for subfiles is:
#        the first directory in [RS274NGC]SUBROUTINE_PATH if
#                               [RS274NGC]SUBROUTINE_PATH is specified
#     or
#        the dir specified by [DISPLAY]PROGRAM_PREFIX if
#                             [DISPLAY]PROGRAM_PREFIX is specified
#     otherwise
#        "."

# 26. removed

# 27. Ngcgui supports .gcmc files (for gcmc the G-Code Meta Compiler)
#         http://www.vagrearg.org/content/gcmc
#     Special tags in the .gcmc file are used to:
#         1) specify the info text for the tab page (optional)
#         1) specify variable names requiring an ngcgui entry box
#         2) specify gcmc options (optional)
#
#     When creating a feature from a gcmc file, the gcmc program
#     is run with the variable values from the entry boxes and the gcmc
#     options specified.
#

#-----------------------------------------------------------------------

namespace eval ::ngcgui {
  namespace export ngcgui ;# public interface
}

#-----------------------------------------------------------------------
# Internationalization

# use the tcl-package named Emc to set up I18n support
if [catch {package require Linuxcnc} msg] {
  # if user is trying to use as standalone in an unconfigured (non-Emc)
  # environment, just continue without internationalization
  puts stdout "Internationalization not available: <$msg>"
}
# use a command or proc named "_" for ::msgcat::mc
# when embedded in axis, a command named "_" is predefined,
# since "_" is not defined for standalone usage, make a proc named "_"
if {"" == [info command "_"]} {
  package require msgcat
  proc _ {s} {return [::msgcat::mc $s]}
}

#-----------------------------------------------------------------------
proc ::ngcgui::parse_ngc {hdl ay_name filename args} {
  # return 1 for ok
  # return 0 for error and lappend to (parse,msg)
  upvar $ay_name ay
  set ay($hdl,parse,msg) ""

  # default info, supersede expected:
  set ay($hdl,info) "[_ "Current subfile: $filename"]"

  if {"$filename" == ""} {
    lappend ay($hdl,parse,msg) "[_ "Need non-null file name"]"
    return 0
  }
  if [catch {set fd [open $filename r]} msg] {
    lappend ay($hdl,parse,msg) $msg
    return 0
  }
  set basename [file tail $filename]
  set idx [string last . $basename]
  set ay($hdl,subroutine,name) [string replace $basename $idx end]
  new_image $hdl $filename

  retain_or_unset $hdl $ay_name

  set min_num 999999; set max_num -1
  set last_num 0
  set ay($hdl,label_maxwidth) 0
  set lct 0
  set lno 1

  catch {
    foreach n [array names ::ngc_sub $hdl,*] {
      unset ::ngc_sub($n)
    }
  }

  while {![eof $fd]} {
    gets $fd theline
    incr lno

    #remove blanks and tabs, use lower case (ngc rs274 format):
    set line [string map {" " "" "	" ""} $theline] ;#sp,tab to ""
    set line [string tolower $line]

    # theline: original line, may have whitespace, caps, etc.
    #    line: collapsed whitespace, lowercase
    set line_end [expr -1 + [string len $line]] ;# last index
    if {"$line" == ""} continue ;# discard empty lines
    set iscomment 0
    if {   ([string first ( $line] == 0 && [string last ) $line] == $line_end)\
        || [string first \; $line] == 0 } {
      set iscomment 1
      # match to theline for caps to find spaceFEATUREspace on a comment line
      if [string match "*\[ \]FEATURE\[ \]*"  $theline] {
        lappend emsg "[_ "Disallowed use of ngcgui generated file as Subfile"]"
        set ay($hdl,parse,msg) $emsg
        catch {unset ay($hdl,argct)} ;# make parmcheck fail
        return 0
      }
      if [string match "(not_a_subfile)"  $theline] {
        lappend emsg "[_ "File"] <$filename> [_ "marked (not_a_subfile)\nNot intended for use as a subfile"]"
        catch {unset ay($hdl,argct)} ;# make parmcheck fail
        set ay($hdl,parse,msg) $emsg
        return 0
      }
      if {[string first "(info:"  $theline] >= 0} {
        set idx [string first : $theline]
        set info [string range $theline [expr $idx +1] end]
        set ay($hdl,info) [string trim $info " )"]
      }
    }

    # disallow embedded numbered subroutines within a single-file subroutine
    if {[regexp -nocase "^o\[0-9\]*sub" $line]} {
      puts stdout "[_ "bogus"]:$lno<$theline>"
      lappend emsg \
         "[_ "can not include subroutines within ngcgui subfile"]:$theline"
      set ay($hdl,parse,msg) $emsg
      return 0
    }

    # find subroutine start:
    if [string match o<*>sub* $line] {
      if [info exists found_sub_end] {
        lappend emsg "[_ "Multiple subroutines in file not allowed"]"
        set ay($hdl,parse,msg) $emsg
        return 0
      }
      set found_sub_start 1
      set i1    [string first < $theline]
      set i2    [string first > $theline]
      set label [string range   $theline [expr $i1 + 1] [expr $i2 -1]]
      if {"$label" != "$ay($hdl,subroutine,name)"} {
        puts stdout "[_ "bogus"]:$lno<$theline>"
        lappend emsg \
          "[_ "sub label"]: o<$label> [_ "does not match subroutine file name"]"
      }
      continue ;# the sub line itself is not saved
    }

    if {[info exists found_sub_end]} {
      # allow null lines and comments after endsub
      if $iscomment {
         set ::ngc_sub($hdl,$lct) $theline
         incr lct
         continue
      } else {
        # sometimes there is an m2 after endsub, ignore it
        if {[string first m2 [string trim [string tolower $theline]]] == 0} {
          set ::ngc_sub($hdl,$lct) \
              "($::ngc(any,app): [_ "ignoring M2 after endsub"]: <$theline>)"
          puts stdout "[_ "ignoring M2 after endsub"] <$theline>"
          incr lct
          continue
        } else {
          puts stdout "[_ "bogus"]:$lno<$theline>"
          lappend emsg "[_ "file contains lines after subend"]"
        }
      }
    }

    if {![info exists found_sub_start]} {
      # allow null lines and comments before sub
      if $iscomment {
        set ::ngc_sub($hdl,$lct) $theline
        incr lct
        continue
       } else {
         puts stdout "[_ "bogus"]:$lno<$theline>"
         lappend emsg "[_ "file contains lines before sub"]"
       }
    }

    if {$iscomment} {
      set ::ngc_sub($hdl,$lct) $theline
      incr lct
      continue
    }
    # processing below for non-comments only

    # find subroutine end:
    if {   [info exists found_sub_start] \
        && [string match o<*>endsub* $line] } {
      set found_sub_end 1
      set i1    [string first < $theline]
      set i2    [string first > $theline]
      set label [string range   $theline [expr $i1 + 1] [expr $i2 -1]]
      if {"$label" != "$ay($hdl,subroutine,name)"} {
        puts stdout "[_ "bogus"]:$lno<$theline>"
        lappend emsg \
          "[_ "endsub label"]: o<$label> [_ "does not match subroutine file name"]"
      }
      continue ;# the endsub line is not saved
    }

    # find and save labels for name mangling when expanding
    if {   [info exists found_sub_start] \
       && ![info exists found_sub_end]} {
      if {$lct >= 0} {
        # save label identifiers so they can be made unique when expanding
        # multiple subroutines
        # but do not include labels for calls:
        # match to line but use theline for label to preserve user case
        if {     [string match *o<* $line] \
             && ![string match *o<*>*call* $line]} {
          set i1    [string first < $theline]
          set i2    [string first > $theline]
          set label [string range   $theline [expr $i1 + 1] [expr $i2 -1]]
          set ::ngc_sub($hdl,$lct,label) $label
          set txt [string range $theline [expr $i2+1] end]
          set ::ngc_sub($hdl,$lct) $txt
        } elseif {   [string match o\[0-9\]* $line] } {
          set tline [string trimleft $theline]
          if [regexp -nocase "(^o\[0-9\]*)(.*)" $tline v label txt] {
            set ::ngc_sub($hdl,$lct,label) $label
            set ::ngc_sub($hdl,$lct) $txt
          }
        } else {
          set ::ngc_sub($hdl,$lct) $theline
          set label ""
        }
        if {[string length $label] > $ay($hdl,label_maxwidth)} {
          set ay($hdl,label_maxwidth) [string length $label]
        }
      }
      incr lct
    }

    # find numbered parameters #1--#30 inclusive
    # in order to identify the biggest one since all
    # in this range are considered to be positional parameters
    # even if some in the range are not explicitly used
    set l $line
    while 1 {
      set i1 [string first # $l]
      if {$i1 < 0} {break}
      set i2 [expr 1 + $i1]
      set i3 [expr 2 + $i1]
      set i4 [expr 3 + $i1]
      set char2 [string range $l $i2 $i2]
      set char3 [string range $l $i3 $i3]

      set v $char2$char3[string range $l $i4 $i4]
      if {   [is_int $v] \
          && ($v > 30) } {
        break ;# ignore #nnn...
      }
      if {[is_int $char2] && ![is_int $char3]} {
        set num_var $char2
        if {$num_var < $min_num} {set min_num $num_var}
        if {$num_var > $max_num} {set max_num $num_var}
        set l [string range $l $i3 end]
        continue
      }
      if {[is_int $char2] && [is_int $char3]} {
        set num_var $char2$char3
        if { 0 < $num_var & $num_var <= 30} {
          if {$num_var < $min_num} {set min_num $num_var}
          if {$num_var > $max_num} {set max_num $num_var}
          set l [string range $l [expr 1+$i3] end]
          continue
        }
      }
      set l [string range $l $i2 end]
    }

    # find special association lines that match:
    # for positional parameters, special line is
    #      #<parmname>=#n    where 0 <= n <= 30
    #  or  #<parmname>=#n (=defaultvalue comment_text)
    if {   [string match *#<*>=#\[1-9\]*         $line] \
        || [string match *#<*>=#\[1-2\]\[0-9\]*  $line] \
        || [string match *#<*>=#30*              $line] } {

      if { [string match *#<*>=#\[3-9\]\[1-9\]*  $line] } {
        # exclude #31-#99
      } elseif {[string match *#<*>=#\[1-9\]\[0-9\]\[0-9\]*  $line] } {
        # exclude #nnn... (3 or more digit numbers)
      } else {
        set i1 [string first >=# $line]
        set parmname [string range $line 2 [expr -1+$i1]]
        set num [string range $line [expr 3+ $i1] end]

        # remove trailing comment:
        set i1 [string first ( $num]
        if {$i1 >= 0} {
          set num [string range $num 0 [expr -1 +$i1]]
        }
        set num02 [format %02d $num]
        set ay($hdl,arg,name,$num02) $parmname
        set expect_num [expr $last_num +1]
        # enforce these to appear in order to help prevent user errors
        if {$num != $expect_num && $num <= 30} {
          puts stdout "[_ "bogus"]:$lno<$theline>"
          lappend emsg \
            "[_ "out of sequence positional parameter"] $num [_ "expected"]: $expect_num "
        } else {
          set last_num $num
        }

        set i1 [string first ( $theline]
        set i2 [string last  ) $theline]
        if { $i1 >0 && $i2 > $i1} {
          set cmt [string range $theline [expr 1 + $i1] [expr -1 + $i2]]
          if [regexp -nocase "= *(\\+*-*\[0-9.\]*)(.*)" \
                 $cmt V(match) V(dvalue) V(comment)] {
            set ay($hdl,arg,dvalue,$num02)  $V(dvalue)
            set ay($hdl,arg,comment,$num02) [string trim $V(comment)]
          } else {
            set ay($hdl,arg,comment,$num02) $cmt
          }
        }

        # for --vwidth 0, make sure something exists for comment
        if {   $ay(any,width,varname) == 0 \
            && (   ![info exists ay($hdl,arg,comment,$num02)] \
                || "$ay($hdl,arg,comment,$num02)" == "")
           } {
          set ay($hdl,arg,comment,$num02) $ay($hdl,arg,name,$num02)
        }
      }
    }

  } ;# while !eof
  set ay($hdl,sublines) $lct
  close $fd

  # for args without a special name association, use #n for name
  for {set i 1} {$i <= $max_num} {incr i} {
    set num02 [format %02d $i]
    if ![info exists ay($hdl,arg,name,$num02)] {
      set ay($hdl,arg,name,$num02) #$i ;# ensure all intervening parms
    }
  }

  set ay($hdl,argct) $max_num

  # remove any notused retained items
  for {set i [expr $max_num +1]} {$i <= 30} {incr i} {
    set num02 [format %02d $i]
    catch {unset ay($hdl,arg,name,$num02)}
    catch {unset ay($hdl,arg,comment,$num02)}
  }

  # error checks
  if {![info exists found_sub_start]} {
    lappend emsg "[_ "no sub found in file"]"
  }
  if {[info exists found_sub_start] && ![info exists found_sub_end]} {
    lappend emsg "[_ "no endsub found in file"]"
  }
  if [info exists emsg] {
    set ay($hdl,parse,msg) $emsg
    return 0
  }
  return 1 ;# ok
} ;# parse

proc retain_or_unset {hdl ay_name} {
  upvar $ay_name ay
  if {$ay($hdl,retainvalues)} {
    # positional parameters: retain some
    foreach n [array names ay $hdl,arg,name,*] {
      # example:
      # exists    arg,name,03      == xloc
      #           arg,value,03     == 999
      # set       arg,byname,xloc  == 999
      set num [string range $n [expr 1+[string last , $n]] end]
      set name $ay($n)
      if ![info exists ay($hdl,arg,value,$num)] continue
      if {[string first # $name] != 0} {
        set ay($hdl,arg,byname,$name) $ay($hdl,arg,value,$num)
      }
    }
  } else {
    # retaining none
    foreach n [array names ay $hdl,arg,value*]       {unset ay($n)}
    foreach n [array names ay $hdl,arg,byname,*]     {unset ay($n)}
  }
  # always unset these
  foreach n [array names ay $hdl,arg,name,*]           {unset ay($n)}
  foreach n [array names ay $hdl,arg,comment,*]        {unset ay($n)}
  foreach n [array names ay $hdl,arg,value,*]          {unset ay($n)}
  foreach n [array names ay $hdl,arg,dvalue,*]         {unset ay($n)}
  foreach n [array names ay $hdl,arg,entrywidget,*]    {unset ay($n)}

  catch {
    foreach n [array names ::ngc_sub $hdl,*] {
      unset ::ngc_sub($n)
    }
  }
} ;# retain_or_unset
#-----------------------------------------------------------------------
proc ::ngcgui::find_gcmc {} {
  if [catch {set found [exec which gcmc]} msg] {
    puts stdout "find_gcmc:NOTfound:<$msg>"
    return ""
  } else {
    #puts stdout "find_gcmc:found:$found"
  }
  return $found
} ;# find_gcmc

proc ::ngcgui::parse_gcmc {hdl ay_name filename args} {
  # return 1 for ok
  # return 0 for error and lappend to (parse,msg)
  upvar $ay_name ay
  set ay($hdl,parse,msg) ""

  if ![info exists ::ngc(any,gcmc,executable)] {
    set result [find_gcmc]
    if {"$result" == ""} {
      lappend ay($hdl,parse,msg) "[_ "Cannot find gcmc executable"]"
      lappend ay($hdl,parse,msg) "[_ "Please Install in path"]"
      return 0
    } else {
      set ::ngc(any,gcmc,executable) [find_gcmc]
      # outdir has to be in path
      # use first dir in path as dir for temporary ofile
      if ![info exists ::ngc(any,paths)] {
        set ::ngc(any,paths) [file normalize [file dirname $filename]]
        puts "\nngcgui: [_ "not embedded, deriving outdir from:"] $filename\n"
      }

      set ::ngc(any,gcmc,outdir) [file normalize [lindex $::ngc(any,paths) 0]]
      set ::ngc(any,gcmc,funcname) tmpgcmc ;# append session id and suffix
      # clean up prior runs by moving to tmp
      if ![catch  {set flist [glob [file join $::ngc(any,gcmc,outdir) \
                             $::ngc(any,gcmc,funcname)]*] } msg] {
        file mkdir /tmp/oldgcmc
        foreach f $flist {
           #puts " file rename $f /tmp/[file tail $f]"
           file rename -force $f [file join /tmp/oldgcmc [file tail $f]]
        }
      }
    }

    set ct 1
    # catch: early versions of gcmc returns $?=1
    if [catch {set ans [exec $::ngc(any,gcmc,executable) --version]
              }  msg ] {
      puts stdout "parse_gcmc: unexpected version:<$msg>"
    } else {
      foreach line [split $ans \n] {
        set ::ngc(any,gcmc,version,line$ct) $line
        incr ct
      }
      puts stdout "gcmc    path: $::ngc(any,gcmc,executable)"
      puts stdout "gcmc version: $::ngc(any,gcmc,version,line1)"
    }
  }

  # default info, supersede expected:
  set ay($hdl,info) "[_ "Current subfile: $filename"]"
  catch {unset ::ngc($hdl,gcmc,opts)} ;# no retain on reread

  if {"$filename" == ""} {
    lappend ay($hdl,parse,msg) "[_ "Need non-null file name"]"
    return 0
  }
  if [catch {set fd [open $filename r]} msg] {
    lappend ay($hdl,parse,msg) $msg
    return 0
  }
  set basename [file tail $filename]
  set idx [string last . $basename]
  set ay($hdl,subroutine,name) [string replace $basename $idx end]
  new_image $hdl $filename

  retain_or_unset $hdl $ay_name

  set min_num 999999; set max_num -1
  set ay($hdl,label_maxwidth) 0

  set lno 1
  set num 1
  set num02 [format %02d $num]
  set names {}
  while {![eof $fd]} {
    gets $fd theline
    incr lno
    #remove blanks and tabs
    set theline [string trim $theline]
    # consider // comments only
    if {[string first "//" $theline] != 0} continue
    # The '*', '+', and '?' qualifiers are all greedy.
    #    Greedy <.*>  matches all of <H1>title</H1>
    # NonGreedy <.*?> matches the only first <H1>

    # // ngcgui : info: describing text
    set einfo "^ *\\/\\/ *ngcgui *: *info: *\(.*?\)"
    if {[regexp $einfo $theline match info]} {
      set ay($hdl,info) $info
      continue
    }

    set eopt "^ *\\/\\/ *ngcgui *: *\(-.*\)$"
    if {[regexp $eopt $theline match opt]} {
      # remove a trailing comment:
      set idx [string first '//' $opt]
      if {$idx >= 0} { set opt [string replace $opt $idx end] }
      set idx [string first \; $opt]
      if {$idx >= 0} { set opt [string replace $opt $idx end] }
      set opt [string trim $opt]

      lappend ::ngc($hdl,gcmc,opts) $opt
      continue
    }

    catch {unset name dvalue comment}
    # // ngcgui : name [= value [,comment]]
    set e1 "^ *\\/\\/ *ngcgui *: *\(.*?\) *= *\(.*?\) *\, *\(.*?\) *$"
    set e2 "^ *\\/\\/ *ngcgui *: *\(.*?\) *= *\(.*?\) *$"
    set e3 "^ *\\/\\/ *ngcgui *: *\(.*?\) *$"
    if {[regexp $e1 $theline match name dvalue comment]} {
      #puts "1_____<$name>,<$dvalue>,<$comment>"
    } elseif {[regexp $e2 $theline match name dvalue]} {
      #puts "2_____<$name>,<$dvalue>"
    } elseif {[regexp $e3 $theline match name]} {
      #puts "3_____<$name>"
    } else {
      continue
    }
    if {[lsearch $names $name] >= 0} {
      puts "duplicate name, first one wins <$name>"
      # could be an error:
      # lappend emsg "[_ "duplicate name <$name>"]"
      continue
    }
    lappend names $name
    set ay($hdl,arg,name,$num02) $name
    if [info exists dvalue] {
      # this is a convenience to make it simple to edit to
      # add a var without removing the semicolon
      #    xstart = 10;
      #    //ngcgui: xstart = 10;
      set dvalue [lindex [split $dvalue ";"] 0] ;# strip after a ";"
      set ay($hdl,arg,dvalue,$num02) $dvalue
    }
    if [info exists comment] {
      set ay($hdl,arg,comment,$num02) $comment
    } else {
      set ay($hdl,arg,comment,$num02) $name
    }
    incr num
    set num02 [format %02d $num]
  } ;# while !eof

  close $fd
  set ay($hdl,argct) [llength $names]

   # gcmc files with no args are allowed
   #  if {$ay($hdl,argct) <= 0} {
   #     lappend emsg "[_ "gcmc file with no args"]"
   #  }
  if {$ay($hdl,argct) > 30} {
     lappend emsg "[_ "gcmc file with too many args <$::ay($hdl,argct)"]"
  }

  # error checks
  if [info exists emsg] {
    set ay($hdl,parse,msg) $emsg
    return 0
  }
  return 1 ;# ok
} ;# parse_gcmc

proc ::ngcgui::dt {} {
  return [clock format [clock seconds] -format %y%m%d:%H.%M.%S]
} ;# dt

proc ::ngcgui::is_int {v} {
  if [catch {format %d $v}] { return 0 }
  return 1
} ;# is_int

proc ::ngcgui::trimprefix {s {pfx opt,} } {
  set idx [string first $pfx $s]
  if {$idx != 0} {return $s}
  return [string range $s [string length $pfx] end]
} ;# trimprefix

proc ::ngcgui::trimsuffix {s {sfx .ngc} } {
  set idx [string last $sfx $s]
  if {$idx <0} {return $s}
  return [string range $s 0 [expr -1 + $idx]]
} ;# trimsuffix

proc ::ngcgui::qid {} {
  # unique identifier
  if ![info exists ::ngc(any,qid)] { set ::ngc(any,qid) 0 }
  return [incr ::ngc(any,qid)]
} ;# qid

proc ::ngcgui::initgui {hdl} {
  if ![info exists ::ngc(embed,hdl)] {set ::ngc(embed,hdl) 0}
  if [info exists ::ngcgui($hdl,afterid)] { return ;# already done }
  # fixed initializations
  set ::ngc(any,pentries)       10 ;# number of entries in positional frame
                                   ;# 30 max positional parameters
                                   ;# 3 frames max so must have pentries >=10
  set ::ngc(any,pollms)         2000

  set ::ngc(any,color,black)    black
  set ::ngc(any,color,stdbg)    #dcdad5 ;# default gray color set
  set ::ngc(any,color,title)    lightsteelblue2
  set ::ngc(any,color,vdefault) darkseagreen2 ;# value defaults
  set ::ngc(any,color,readonly) gray

  set ::ngc(any,color,ok)       green4
  set ::ngc(any,color,single)   palegreen
  set ::ngc(any,color,multiple) cyan
  set ::ngc(any,color,feature)  lightslategray
  set ::ngc(any,color,prompt)   blue3
  set ::ngc(any,color,warn)     darkorange
  set ::ngc(any,color,notice)   lightgoldenrodyellow
  set ::ngc(any,color,override) blue3

  set ::ngc(any,color,error)    red
  set ::ngc(any,color,filegone) maroon
  set ::ngc(any,color,filenew)  darkorange
  set ::ngc(any,color,filemod)  purple
  set ::ngc(any,color,custom)   ivory2
  set ::ngc(any,color,default)  blue4

  set ::ngc(any,max_msg_len)     500 ;# limit popup msg len (gcmc)
  set ::ngc($hdl,afterid)        ""
  statemap $hdl ;# set up state transitions
} ;# initgui


proc ::ngcgui::preset {hdl ay_name} {
  # using apps call this to populate ay_name,
  # superseded items as reqd
  # all required items with defaults:
  upvar $ay_name ay

  # per-instance items:
  set ay($hdl,fname,subfile)      ""
  set ay($hdl,fname,preamble)     ""
  set ay($hdl,fname,postamble)    ""
  set ay($hdl,fname,outfile)      ""
  set ay($hdl,auto)               1
  set ay($hdl,fname,autosend)     "auto.ngc"
  set ay($hdl,dir)                ""
  set ay($hdl,retainvalues)       1
  set ay($hdl,expandsubroutine)   0
  set ay($hdl,verbose)            1
  set ay($hdl,chooser)            0
  set ay($hdl,info)               "[_ "Choose Files"]"
  set ay($hdl,standalone)         0

  # common to any instance items:
  set ay(any,app)                ngcgui
  set ay(any,entrykeys,special)  {x X y Y z Z a A b B c C u U v V w W d D}
  set ay(any,dir,just)           "/tmp/ngcgui_bak" ;# set to "" to disable
  set ay(any,aspect)             horiz
  set ay(any,font)               {Helvetica -10 normal}
  set ay(any,width,comment)      12
  set ay(any,width,varname)      12
  set ay(any,img,width,max)     320 ;# subsample image to this max size
  set ay(any,img,height,max)    240 ;# subsample image to this max size

  # options currently available with embed_in_axis only
  set ::ngc(opt,nonew)      0 ;# default allows new
  set ::ngc(opt,noremove)   0 ;# default allows remove
  set ::ngc(opt,noauto)     0 ;# default is autosend
  set ::ngc(opt,noinput)    0 ;# default is to show an input frame
  set ::ngc(opt,noiframe)   0 ;# default uses a separate toplevel for img

  set ::ngc(opt,nom2)       0 ;# default use % at start and end
                               #         instead of m2 at 3end
} ;# preset

proc ::ngcgui::gui {hdl mode args} {
  # use ::ngcgui::preset for required ::ngc($hdl,) items and defaults
  # standalone invoke: ::ngcgui::gui $hdl standalone wframe
  # embedded   invoke: ::ngcgui::gui $hdl create     wframe
  switch $mode {
    standalone {
      set ::ngc($hdl,standalone) 1
      set w [::ngcgui::gui $hdl create $args]
      return $w
    }
    create {
      if {"$hdl" == ""} {return -code error "hdl is null"}
      # mandatory arg for mode==create is a frame
      # caller packs/unpacks wframe which must be a valid name
      # but not exist yet
      set wframe [lindex $args 0]
      initgui $hdl
      set ::ngc($hdl,l,width) 10 ;# min lside width, see also tw
      if {"$::ngc(any,dir,just)" == ""} {
        unset ::ngc(any,dir,just) ;# disable feature:
      } else {
        if {   [file isdirectory $::ngc(any,dir,just)] \
            && [file writable    $::ngc(any,dir,just)] \
          } {
          # ok
        } else {
          if [catch {file mkdir $::ngc(any,dir,just)} msg] {
            puts stdout $msg ;# no such dir for example
            return "" ;# something bad happened
          }
        }
      }
      if {"$wframe" == ""} {
        return -code error "gui:create no arg for wframe"
      }
      set wframe [frame $wframe] ;# wframe specifies name, create it here
      pack $wframe -anchor nw -fill none -expand 0 ;# NB
      set ::ngc($hdl,top) [winfo toplevel $wframe]
      set ::ngc($hdl,topf) $wframe ;# ok for embed_in_axis, ok standalone

      if {"$::ngc($hdl,dir)" == ""} {set ::ngc($hdl,dir) .}

      # defaults:
      set ::ngc($hdl,id)      0
      set ::ngc($hdl,savect)  0
      conf $hdl restart,widget state disabled
      set ::ngc($hdl,ftypes,subfile) { {{GCODE,GCMC}   {.ngc .gcmc}} }
      set ::ngc($hdl,ftypes,other)   { {{NGC}   {.ngc}} }
      # initializations:
      set ::ngc($hdl,data,preamble)    ""
      set ::ngc($hdl,data,postamble)   ""

      # special frame for embed,axis
      set removable 0; set newable 0
      if {[info exists ::ngc(embed,axis)] } {
        if !$::ngc($hdl,standalone) {
          if {!$::ngc(opt,noremove) || $::ngc($hdl,chooser)} {
            set removable 1
          }
          if {!$::ngc(opt,nonew) || $::ngc($hdl,chooser)} {
            set newable 1
          }
        }
        tabmanage $::ngc($hdl,axis,page) $wframe \
                  "$::ngc(any,app)-$hdl" \
                  ::ngc($hdl,info) \
                   $removable $newable
      }
      set wframe [frame $wframe.[qid]]

      set bw 8
      set tw 10 ;# min text width (default is 20) see also l,width
      switch $::ngc(any,aspect) {
        vert {
          set wI [frame $wframe.input   -bd 1 -relief sunken] ;# input frame
          set wO [frame $wframe.output  -bd 1 -relief sunken] ;# output frame
          set wV [frame $wframe.var]                          ;# variable frame
          set wC [frame $wframe.create  -bd 1 -relief sunken] ;# create frame
          set wE [frame $wframe.exit    -bd 1 -relief sunken] ;# exit frame

          pack $wI -side top    -fill x -expand 1 -anchor n
          pack $wE -side bottom -fill x -expand 1 -anchor s
          pack $wO -side bottom -fill x -expand 1 -anchor s
          pack $wC -side bottom -fill x -expand 1 -anchor n
          pack $wV -side top    -fill x -expand 1 -anchor n
          set ::ngc($hdl,pack,positional) top
        }
        horiz {
          set wL [frame $wframe.left -bd 2 -relief ridge] ;# left frame
          set wI [frame $wL.input    -bd 0 -relief sunken] ;# input frame
          set wO [frame $wL.output   -bd 0 -relief sunken] ;# output frame
          set wC [frame $wL.create   -bd 0 -relief sunken] ;# create frame
          set wE [frame $wL.exit     -bd 0 -relief sunken] ;# exit frame
          set wV [frame $wframe.var  -bd 0 -relief flat]   ;# variable frame

          pack $wL -side left    -fill x -expand 1 -anchor nw
          pack $wI -side top     -fill x -expand 1 -anchor n
          pack $wO -side top     -fill x -expand 1 -anchor n
          pack $wE -side bottom  -fill x -expand 1 -anchor s
          pack $wC -side bottom  -fill x -expand 1 -anchor s
          pack $wV -side left -fill x -expand 1 -anchor n
          set ::ngc($hdl,pack,positional) left
          $wframe config -relief ridge -bd 2
        }
        default {return -code error ngc::gui:aspect <$aspect>}
      }
      set ::ngc($hdl,varframe) $wV
      set ::ngc($hdl,iframe)   $wI
      image_init $hdl

      set w [frame $wI.[qid]]
      pack $w -fill x -expand 1
      #pack [label $w.[qid] -anchor w -text "Input Files" \
      #    -width $::ngc($hdl,l,width)\
      #    -bg $::ngc(any,color,title) -relief groove] -fill x -expand 1
      pack [label $w.[qid] -anchor w -text "[_ "Controls"]" \
          -width $::ngc($hdl,l,width)\
          -bg $::ngc(any,color,title) -relief groove] -fill x -expand 1

# wI inputs
      set w [frame $wI.[qid]]
      pack $w -fill x -expand 1

      set b [button $w.[qid] -font $::ngc(any,font) \
            -pady 0 -width $bw -text "[_ "Preamble"]" \
            -command "::ngcgui::gui $hdl getpreamble"]
      set ::ngc($hdl,begin,widget) $b
      pack $b -side left -expand 0
      set e [entry $w.e -width $tw -font $::ngc(any,font) \
            -textvariable ::ngc($hdl,dname,preamble)]
      bind $e <Return> [list ::ngcgui::readfile $hdl preamble]
      pack $e -side left -fill x -expand 1
      set ::ngc($hdl,preamble,widget) $e

      set w [frame $wI.[qid]]
      pack $w -fill x -expand 1
      set b [button $w.[qid] -font $::ngc(any,font) \
            -pady 0 -width $bw -text "[_ "Subfile"]" \
            -command "::ngcgui::gui $hdl getsubfile"]
      pack $b -side left -expand 0
      set e [entry $w.e -width $tw -font $::ngc(any,font) \
            -textvariable ::ngc($hdl,dname,subfile)]
      bind $e <Return> [list ::ngcgui::readfile $hdl subfile]
      pack $e -side left -fill x -expand 1
      set ::ngc($hdl,subfile,widget) $e

      set w [frame $wI.[qid]]
      pack $w -fill x -expand 1
      set b [button $w.[qid] -font $::ngc(any,font) \
            -pady 0 -width $bw -text "[_ "Postamble"]" \
            -command "::ngcgui::gui $hdl  getpostamble"]
      pack $b -side left -expand 0
      set e [entry $w.e -width $tw -font $::ngc(any,font) \
            -textvariable ::ngc($hdl,dname,postamble)]
      bind $e <Return> [list ::ngcgui::readfile $hdl postamble]
      pack $e -side left -fill x -expand 1
      set ::ngc($hdl,postamble,widget) $e

#     set w [frame $wI.[qid]]
#     pack $w -fill x -expand 1
#     pack [label $w.[qid] -anchor w -text "Options" \
#          -bg $::ngc(any,color,title) -relief groove] -fill x -expand 1

      set w [frame $wI.[qid]]
      pack $w -fill x -expand 1
      set b [checkbutton $w.[qid] -anchor w -font $::ngc(any,font) \
            -text "[_ "Retain values on Subfile read"]" \
            -command [list ::ngcgui::aftertoggle $hdl retainvalues] \
            -variable ::ngc($hdl,retainvalues)]
      pack $b -side left -fill x -expand 1

      set w [frame $wI.[qid]]
      pack $w -fill x -expand 1
      set b [checkbutton $w.[qid] -anchor w -font $::ngc(any,font) \
            -text "[_ "Expand subroutine"]" \
            -command [list ::ngcgui::aftertoggle $hdl expandsubroutine] \
            -variable ::ngc($hdl,expandsubroutine)]
      pack $b -side left -fill x -expand 1
      set ::ngc($hdl,expandsubroutine,widget) $b

if {1} {
      set w [frame $wI.[qid]]
      pack $w -fill x -expand 1
      set b [checkbutton $w.[qid] -anchor w -font $::ngc(any,font) \
            -text "[_ "Autosend"]" \
            -command [list ::ngcgui::aftertoggle $hdl auto] \
            -variable ::ngc($hdl,auto)]
      pack $b -side left -fill x -expand 1
}
if {0} {
# take up too much room
      set w [frame $wI.[qid]]
      pack $w -fill x -expand 1
      set b [checkbutton $w.[qid] -anchor w -font $::ngc(any,font) \
            -text "[_ "Verbose ngcfile"]" \
            -command [list ::ngcgui::aftertoggle $hdl verbose] \
            -variable ::ngc($hdl,verbose)]
      pack $b -side left -fill x -expand 1
}

# wC create frame
      # used fixed widths so buttons stay same when text is changed
      set w [frame $wC.[qid]]
      pack $w -side top -fill x -expand 1
      set b [button $w.[qid] -text "[_ "Create Feature"]" -font $::ngc(any,font) \
            -width 14 -padx 1\
            -command "::ngcgui::gui $hdl savesection"]
      pack $b -side left -fill x -expand 1
      set ::ngc($hdl,save,widget) $b

      set text "[_ "MakeFile"]"
      if $::ngc($hdl,auto) {set text "[_ "Finalize"]"}
      set b [button $w.[qid] -state disabled -font $::ngc(any,font) \
            -fg $::ngc(any,color,prompt) \
            -width 8 -padx 1\
            -text "$text" -command "::ngcgui::gui $hdl finalize"]
      pack $b -side left -fill x -expand 1
      set ::ngc($hdl,finalize,widget) $b

      set w [frame $wC.[qid]]
      pack $w -fill x -expand 1
      pack [label $w.[qid] -width 0 -font $::ngc(any,font) \
           -pady 0 -relief flat \
           -textvariable ::ngc($hdl,savect)] -side left -fill x -expand 0

      if {!$::ngc(opt,noinput) || $::ngc($hdl,chooser)} {
        # reread notapplicable with no controls
        set b [button $w.[qid] -width 2 -font $::ngc(any,font) \
             -padx 0 -pady 0 -text "[_ "Reread"]" \
             -state disabled \
             -command [list ::ngcgui::reread $hdl] \
             ]
        pack $b -side left -fill x -expand 1
        set ::ngc($hdl,reread,widget) $b
      }

      set b [button $w.[qid] -width 2 -font $::ngc(any,font) \
           -padx 0 -pady 0 -text "[_ "Restart"]" \
           -state disabled \
           -command [list ::ngcgui::message $hdl restart] \
           ]
      pack $b -side left -fill x -expand 1
      set ::ngc($hdl,restart,widget) $b
      # sendfile,widget button is forgettable
      # use wC frame avoids problems with ctrl-a resizing app
      set b [button $wC.[qid] -state disabled -font $::ngc(any,font) \
            -pady 1 \
            -text "[_ "SendFileToAxis"]" \
            -command [list ::ngcgui::sendfile $hdl]]
      pack $b -side bottom -fill x -expand 1
      set ::ngc($hdl,sendfile,widget) $b

      if $::ngc($hdl,auto) {
        pack forget $::ngc($hdl,sendfile,widget)
        $::ngc($hdl,finalize,widget) conf -fg $::ngc(any,color,prompt)
      }

      if $::ngc($hdl,standalone) {
        set b [button $w.[qid] -takefocus 0 -font $::ngc(any,font) \
               -pady 0 -text "[_ "Exit"]" \
               -command [list ::ngcgui::bye $hdl]]
        pack $b -side left -fill none -expand 0
      }

# wO output frame
      set w [frame $wO.[qid] -bd 2]
      pack $w -side top -fill x -expand 1

      set ::ngc($hdl,msg,widget) [label $wE.[qid] \
                             -width 20\
                             -relief sunken \
                             -anchor w] ;# update with config
      pack $::ngc($hdl,msg,widget) -side left -fill x -expand 1

# wE exit frame obsoleted
#------------------------------------------------------------------------------
      if {"$::ngc($hdl,fname,preamble)" != ""} {
        set ::ngc($hdl,fname,preamble) [string trim $::ngc($hdl,fname,preamble)]
        ::ngcgui::gui $hdl readpreamble
      }
      if {"$::ngc($hdl,fname,subfile)" != ""} {
        set ::ngc($hdl,fname,subfile) [string trim $::ngc($hdl,fname,subfile)]
        ::ngcgui::gui $hdl readsubfile
      }
      if {"$::ngc($hdl,fname,postamble)" != ""} {
        set ::ngc($hdl,fname,postamble) \
          [string trim $::ngc($hdl,fname,postamble)]
        ::ngcgui::gui $hdl readpostamble
      }
      if [info exists ::ngc($hdl,fail)] {
        puts stdout "\n$::ngc(any,app):[_ "Unrecoverable problem"]:\n<$hdl>$::ngc($hdl,fail)"
        ::ngcgui::deletepage $::ngc($hdl,axis,page)
        return
      }
      update ;# ensure entry variables are updated before starting checks
      periodic_checks $hdl
      bindings $hdl init
      if ![info exists ::ngc(embed,axis)] [list updownkeys $::ngc($hdl,top)]
      after 2000 [list ::ngcgui::showmessage $hdl startup]
      return $wframe
      # ::ngcgui::gui-create-end
    }
    getpreamble {
      if {$::ngc($hdl,fname,preamble) == ""} {
        set idir $::ngc($hdl,dir)
      } else {
        set idir [file dirname $::ngc($hdl,fname,preamble)]
      }
      set filename [tk_getOpenFile \
           -title "$::ngc(any,app) Preamble file" \
           -defaultextension .ngc \
           -initialfile [file tail $::ngc($hdl,fname,preamble)] \
           -initialdir  $idir \
           -filetypes $::ngc($hdl,ftypes,other) \
           ]
      set filename [string trim $filename]
      if {"$filename" == ""} return
      check_path $filename
      set ::ngc($hdl,fname,preamble) $filename
      ::ngcgui::gui $hdl readpreamble
      return
    }
    readpreamble {
      if {   ![string match *.ngc $::ngc($hdl,fname,preamble)]\
          &&  [file readable "$::ngc($hdl,fname,preamble).ngc"]} {
        set ::ngc($hdl,fname,preamble) "$::ngc($hdl,fname,preamble).ngc"
      }
      set ::ngc($hdl,data,preamble) ""
      if {"$::ngc($hdl,fname,preamble)" == ""} {
        # message $hdl nullpreamble
        return
      } else {
        if [catch {set fpre [open $::ngc($hdl,fname,preamble) r]} msg] {
          lappend emsg $msg
          showerr $emsg
          message $hdl preambleerror
          if {$::ngc(opt,noinput) && !$::ngc($hdl,chooser)} {
            set ::ngc($hdl,fail) "preamble:$msg" ;# unrecoverable
          }
          return
        }
        set ::ngc($hdl,dname,preamble) [file tail $::ngc($hdl,fname,preamble)]
        lappend ::ngc($hdl,data,preamble) \
                "($::ngc(any,app): preamble file: $::ngc($hdl,fname,preamble))"

        # dont copy some items to preamble
        while {![eof $fpre]} {
          gets $fpre line
          set l [string map {" " "" "	" ""} $line] ;#sp,tab to ""
          if {"$l" == ""} continue
          if ![string match "(not_a_subfile)"  $line] {
            lappend ::ngc($hdl,data,preamble) $line
          }
        }
        close $fpre
        set ::ngc($hdl,fname,preamble,time) \
                     [file mtime $::ngc($hdl,fname,preamble)]
      }
      message $hdl readpreamble
      return
    }
    getpostamble {
      if {$::ngc($hdl,fname,postamble) == ""} {
        set idir $::ngc($hdl,dir)
      } else {
        set idir [file dirname $::ngc($hdl,fname,postamble)]
      }
      set filename [tk_getOpenFile \
           -title "$::ngc(any,app) [_ "Postamble file"]" \
           -defaultextension .ngc \
           -initialfile [file tail $::ngc($hdl,fname,postamble)] \
           -initialdir  $idir \
           -filetypes $::ngc($hdl,ftypes,other) \
           ]
      set filename [string trim $filename]
      if {"$filename" == ""} return
      check_path $filename
      set ::ngc($hdl,fname,postamble) $filename
      ::ngcgui::gui $hdl readpostamble
      return
    }
    readpostamble {
      if {   ![string match *.ngc $::ngc($hdl,fname,postamble)]\
          &&  [file readable "$::ngc($hdl,fname,postamble).ngc"]} {
        set ::ngc($hdl,fname,postamble) "$::ngc($hdl,fname,postamble).ngc"
      }
      set ::ngc($hdl,data,postamble) ""
      if {"$::ngc($hdl,fname,postamble)" == ""} {
        # message $hdl nullpostamble
        return
      } else {
        if [catch {set fpost [open $::ngc($hdl,fname,postamble) r]} msg] {
          lappend emsg $msg
          showerr $emsg
          message $hdl postambleerror
          return
        }
        set ::ngc($hdl,dname,postamble) [file tail $::ngc($hdl,fname,postamble)]
        lappend ::ngc($hdl,data,postamble) \
             "($::ngc(any,app): postamble file: $::ngc($hdl,fname,postamble))"
        while {![eof $fpost]} {
          gets $fpost line
          lappend ::ngc($hdl,data,postamble) "$line"
        }
        close $fpost
        set ::ngc($hdl,fname,postamble,time) \
                     [file mtime $::ngc($hdl,fname,postamble)]
      }
      message $hdl readpostamble
      return
    }
    getsubfile {
      if {$::ngc($hdl,fname,subfile) == ""} {
        set idir $::ngc($hdl,dir)
      } else {
        set idir [file dirname $::ngc($hdl,fname,subfile)]
      }
      set filename [tk_getOpenFile \
           -title "$::ngc(any,app) [_ "Subroutine file"]" \
           -defaultextension .ngc \
           -initialfile [file tail $::ngc($hdl,fname,subfile)] \
           -initialdir  $idir \
           -filetypes $::ngc($hdl,ftypes,subfile) \
           ]
      set filename [string trim $filename]
      if {"$filename" == ""} return
      check_path $filename
      set ::ngc($hdl,fname,subfile) $filename
      ::ngcgui::gui $hdl readsubfile
      return
    }
    readsubfile {
      set parsecmd ::ngcgui::parse_ngc
      if {[string match *.gcmc $::ngc($hdl,fname,subfile)] } {
        set parsecmd ::ngcgui::parse_gcmc
        set ::ngc($hdl,gcmc,file) $::ngc($hdl,fname,subfile)
        $::ngc($hdl,expandsubroutine,widget) configure -state disable
      } else {
        # in case earlier an earlier find for gcmc failed;
        catch {unset ::ngc($hdl,gcmc,file)}
        $::ngc($hdl,expandsubroutine,widget) configure -state normal
      }
      if {   ![string match *.ngc $::ngc($hdl,fname,subfile)] \
          && ![string match *.gcmc $::ngc($hdl,fname,subfile)] \
         } {
        set ::ngc($hdl,fname,subfile) "$::ngc($hdl,fname,subfile).ngc"
      }
      # uses two pack/unpack frames wP
      set ew 6; set bw 9

# wP positional parameters
      set wP $::ngc($hdl,varframe).positional ;# variable frame positional parms
      if  [winfo exists $wP] {destroy $wP}
      set wP [frame $wP -bd 2 -relief ridge]

      pack $wP -side $::ngc($hdl,pack,positional) -fill x -expand 1 -anchor n

      if {   ![string match *.ngc $::ngc($hdl,fname,subfile)]\
          &&  [file readable "$::ngc($hdl,fname,subfile).ngc"]} {
       set ::ngc($hdl,fname,subfile) "$::ngc($hdl,fname,subfile).ngc"
      }
      # read and parse the file
      set ::ngc($hdl,dname,subfile) [file tail $::ngc($hdl,fname,subfile)]
      if ![$parsecmd $hdl ::ngc $::ngc($hdl,fname,subfile)] {
        # case where user can't recover
        if {$::ngc(opt,noinput) && !$::ngc($hdl,chooser)} {
          set ::ngc($hdl,fail) "subfile:$::ngc($hdl,parse,msg)";# unrecoverable
        }
        showerr  $::ngc($hdl,parse,msg)
        # try to display name of failed file:
        message $hdl parseerror
        # 101024:09.13 leave them alone
        # set ::ngc($hdl,fname,subfile) "" ;# prevents color change
        # set ::ngc($hdl,dname,subfile) "" ;# in periodic_checks
        catch {pack forget $wP}
        return
      }
      set ::ngc($hdl,fname,subfile,time) \
                     [file mtime $::ngc($hdl,fname,subfile)]

      set w [frame $wP.[qid]]
      pack $w -side top -fill x -expand 1
      pack [label $w.[qid] -text "[_ "Positional Parameters"]" \
           -bg $::ngc(any,color,title) -anchor w -relief groove] \
           -side top -fill x -expand 1

      # Positional parameters
      # find retained values for numbered parms (#n) with
      # a byname association
      foreach n [array names ::ngc $hdl,arg,name,*] {
        # example:
        # if       ::ngc($hdl,arg,name,04)     == xloc
        # and      ::ngc($hdl,arg,byname,xloc) == 33
        # then set ::ngc($hdl,arg,value,04)       33
        # else set ::ngc($hdl,arg,value,04)       ""
        set name $::ngc($n)
        set num  [string range $n [expr 1 + [string last , $n]] end]
        if {[info exists ::ngc($hdl,arg,byname,$name)]} {
          set ::ngc($hdl,arg,value,$num) $::ngc($hdl,arg,byname,$name)
        } else {
          # use default value if available
          if [info exists ::ngc($hdl,arg,dvalue,$num)] {
            set ::ngc($hdl,arg,value,$num) $::ngc($hdl,arg,dvalue,$num)
          } else {
            set ::ngc($hdl,arg,value,$num) ""
          }
        }
      }

      # Positional parameters entries, provide two frames
      set pnamelist [lsort [array names ::ngc $hdl,arg,name,*]]
      set wP1 [frame $wP.[qid] -relief flat]
      set wP2 [frame $wP.[qid] -relief flat]
      set wP3 [frame $wP.[qid] -relief flat]
      set npos [llength $pnamelist]
      pack $wP1 -side left -anchor n -fill x -expand 1
      # a weird space is left if you dont do these separately:
      if {$npos > $::ngc(any,pentries)} {
        pack $wP2 -side left -anchor n -fill x -expand 1
        if {$npos > [expr 2*$::ngc(any,pentries)]} {
          pack $wP3 -side left -anchor n -fill x -expand 1
        }
      }
      set ct 0

      foreach v $pnamelist {
        incr ct
        if {$ct <= $::ngc(any,pentries)} {
          set fdata [frame $wP1.[qid]]
        } elseif {$ct <= [expr 2* $::ngc(any,pentries)]} {
          set fdata [frame $wP2.[qid]]
        } else {
          set fdata [frame $wP3.[qid]]
        }

        pack $fdata -side top -fill x -expand 1

        set i1 [string last , $v]
        set num [string range $v [expr 1+$i1] end]
        if [info exists ::ngc($hdl,arg,name,$num)] {
          set name $::ngc($hdl,arg,name,$num)
        } else {
          set name [format %d $num]
        }

        scan $num %d onum ;# ==>onum avoid octalinterpretation of 08,09
        set num02 [format %02d $onum]

        set l [label $fdata.[qid] -text [format %#2d $onum] -anchor e \
              -takefocus 0 -relief ridge -width 2]
        pack $l -side left -fill x -expand 0

        # use entry since it can be expanded by user to see overfill
        if {$::ngc(any,width,varname) != 0} {
          set l [entry $fdata.[qid] -state readonly  -font $::ngc(any,font) \
                -textvariable ::ngc($hdl,arg,name,$num) \
                -takefocus 0 -justify right -relief groove \
                -width $::ngc(any,width,varname)]
          pack $l -side left -fill x -expand 0
        }
        set tvar ::ngc($hdl,arg,value,$num)
        set e [entry $fdata.[qid] \
              -width $ew \
              -font $::ngc(any,font) \
              -textvariable $tvar\
              -validate     all\
              -validatecommand \
                [list ::ngcgui::validateNumber $hdl $tvar %W %s %P]]
        foreach k $::ngc(any,entrykeys,special) {
          bind $e <Key-$k> \
             [list ::ngcgui::entrykeybinding %K %W ::ngc($hdl,arg,value,$num)]
        }
        if [info exists ::ngc(embed,axis)] [list updownkeys $e]
        set ::ngc($hdl,arg,entrywidget,$num02) $e
        pack $e -side left

        set l [entry $fdata.[qid] -state readonly  -font $::ngc(any,font) \
               -textvariable ::ngc($hdl,arg,comment,$num02) \
               -takefocus 0 -relief groove \
               -width $::ngc(any,width,comment)\
               ]
        pack $l -side left -fill x -expand 1
      }

      dcheck $hdl
      set ::ngc($hdl,dir) [file dirname $::ngc($hdl,fname,subfile)]
      message $hdl readsubfile

      if [info exists ::ngc(embed,axis)] {
        set tabname $::ngc($hdl,dname,subfile)
        if {[string match *.ngc $tabname] } {
          set     idx [string last .ngc $tabname]
          set tabname [string replace $tabname $idx end ""]
        } elseif {[string match *.gcmc $tabname] } {
          set     idx [string last .gcmc $tabname]
          set tabname [string replace $tabname $idx end ""]
        }
        # show last subfile used as page name
        $::ngc(any,axis,parent) itemconfigure $::ngc($hdl,axis,page) \
              -createcmd "::ngcgui::pagecreate $hdl"\
              -raisecmd  "::ngcgui::pageraise  $hdl"\
              -leavecmd  "::ngcgui::pageleave  $hdl"\
              -text "$tabname"

        # current tab names for other hdls
        set names ""
        for {set i 0} {$i <= $::ngc(embed,hdl)} {incr i} {
          if {$i == $hdl} continue ;# exclude name for this hdl
          if [info exists ::ngc($i,axis,page)] {
             lappend names [$::ngc(any,axis,parent) \
                           itemcget $::ngc($i,axis,page) -text]
             }
        }
        if {[lsearch $names "$tabname"] >= 0} {
          # name exists, make unique name for page
          set ct 1
          while 1 {
            set tryname ${tabname}-$ct
            if {[lsearch $names "$tryname"] < 0} break
            incr ct
            if {$ct>100} {return -code error "readsubfile:problem<$trytabname>"}
          }
          $::ngc(any,axis,parent) itemconfigure $::ngc($hdl,axis,page) \
                                  -text "$tryname"
        }
      }
      return ;# readsubfile
    }
    parmcheck {
      if ![info exists ::ngc($hdl,argct)] {
        if {"$::ngc($hdl,fname,subfile)" == ""} {
          lappend err "[_ "No Subfile specified"]"
        }
        lappend err "[_ "No parameters yet"]"
      } else {
        for {set i 1} {$i <= $::ngc($hdl,argct)} {incr i} {
          set num02 [format %02d $i]
          set token $::ngc($hdl,arg,name,$num02)
          # nuisance spaces cause problems:
          set ::ngc($hdl,arg,value,$num02) \
              [string trim $::ngc($hdl,arg,value,$num02)]
          if {"$::ngc($hdl,arg,value,$num02)" == ""} {
            lappend err "[_ "Missing value for parm"] #$i ($token)"
          }
        }
      }
      if [info exists err] {
        showerr $err
        message $hdl parmerr
        return 0 ;# error
      }
      return 1 ;# ok
    }
    setoutfile {
      if {$::ngc($hdl,fname,outfile) == ""} {
        set idir $::ngc($hdl,dir)
      } else {
        set idir [file dirname $::ngc($hdl,fname,outfile)]
      }
      if {"$::ngc($hdl,fname,outfile)" == "" } {
        set ::ngc($hdl,fname,outfile) tmp
      }
      set filename [tk_getSaveFile \
           -title "$::ngc(any,app) [_ "Output file"]" \
           -defaultextension .ngc \
           -initialfile [file tail $::ngc($hdl,fname,outfile)] \
           -initialdir  $idir \
           -filetypes $::ngc($hdl,ftypes,subfile) \
           ]
      set filename [string trim $filename]
      # sometimes leading blanks get in
      set filename [string map {" " "" "	" ""} $filename] ;#sp,tab to ""
      if {$filename == ""} {
        set ::ngc($hdl,fname,outfile) "" ;# canceled
        return
      }
      set ::ngc($hdl,fname,outfile) $filename
      message $hdl setoutfile
      return
    }
    savesection {
      ::ngcgui::readfile $hdl preamble
      ::ngcgui::readfile $hdl postamble
      # save,widget has multiple presentations to steer user

      if ![::ngcgui::gui $hdl parmcheck] {
        return
      }


      if $::ngc($hdl,verbose) {
        lappend ::ngc($hdl,data,section) \
"($::ngc(any,app): files: <$::ngc($hdl,fname,preamble) $::ngc($hdl,fname,subfile) $::ngc($hdl,fname,postamble)>)"
      }
      # note: this line will be replaced on file output with a count
      # that can include multiple tab pages
      lappend ::ngc($hdl,data,section) "#<_feature:> = $::ngc($hdl,savect)"

      if {"$::ngc($hdl,fname,preamble)" == "IMMEDIATE"} {
        # indicates preamble is interpreted as
        # immediate commands separated by semicolons
        # example  ":t1m6;m1"
        set ::ngc($hdl,immediate,preamble) [string range \
                                      $::ngc($hdl,dname,preamble) 1 end]
        if $::ngc($hdl,verbose) {
          lappend ::ngc($hdl,data,section) \
                  "($::ngc(any,app): IMMEDIATE preamble:)"
        }
        foreach line [split $::ngc($hdl,immediate,preamble) \;] {
          lappend ::ngc($hdl,data,section) [string trim $line]
        }
        unset ::ngc($hdl,immediate,preamble)
      } else {
        for {set i 0} {$i < [llength $::ngc($hdl,data,preamble)]} {incr i} {
          lappend ::ngc($hdl,data,section) \
                  [lindex $::ngc($hdl,data,preamble) $i]
        }
      }


      if [info exists ::ngc($hdl,gcmc,file)] {
        if ![savesection_gcmc $hdl] {return} ;# .gcmc file
      } else {
        if ![savesection_ngc $hdl] {return} ;# conventional .ngc file
      }

     if {"$::ngc($hdl,fname,postamble)" == "IMMEDIATE"} {
       # indicates postamble is interpreted as
       # immediate commands separated by semicolons
       # example  ":t1m6;m1"
       set ::ngc($hdl,immediate,postamble) [string range \
                                      $::ngc($hdl,dname,postamble) 1 end]
       if $::ngc($hdl,verbose) {
         lappend ::ngc($hdl,data,section) \
                 "($::ngc(any,app): IMMEDIATE postamble:)"
       }
       foreach line [split $::ngc($hdl,immediate,postamble) \;] {
         lappend ::ngc($hdl,data,section) [string trim $line]
       }
       unset ::ngc($hdl,immediate,postamble)
      } else {
        for {set i 0} {$i < [llength $::ngc($hdl,data,postamble)]} {incr i} {
          lappend ::ngc($hdl,data,section) \
                  [lindex $::ngc($hdl,data,postamble) $i]
        }
      }

      message $hdl savesection
      return
    }
    finalize {
      if {$::ngc($hdl,savect) == 0} {
        return ;# silently (may be bound to key)
      }

      set doall 1 ;# default
      if {![info exists ::ngc(embed,axis)]} {
        set hdllist $hdl
      } else {
        # find all tabpages with saved features
        # order of tabpage names determines execution order
        set tnames ""
        foreach p [$::ngc(any,axis,parent) pages] {
          set h [pagetohdl $p]
          if {$h >= 0} {
            if {$::ngc($h,savect) == 0} {continue}
            lappend hdllist $h
            if [info exists ::ngc($h,axis,page)] {
              lappend tnames [$::ngc(any,axis,parent) \
                            itemcget $::ngc($h,axis,page) -text]
            }
          }
        }
        set thisone [$::ngc(any,axis,parent) \
                         itemcget $::ngc($hdl,axis,page) -text]

        if {[llength $hdllist] > 1} {
           set ans [tk_dialog .foo \
                       "[_ "Multiple Tabs with Features"]" \
                       "[_ "Finalize all Tabs?"]\n [_ "Order"]:<$tnames>" \
                       questhead 0 \
                       "[_ "No, just this page"] <$thisone>" Yes Cancel\
                   ]
           switch $ans {
              0 { set hdllist $hdl; set doall 0; #NO}
              1 {}
              2 {showmessage $hdl cancel; return}
           }
        }
      }
      set endhdl [lindex $hdllist end]

      if {$::ngc($hdl,auto) && ![sendaxis $hdl ping]} {
        set ::ngc($hdl,auto) 0
        $::ngc($hdl,finalize,widget) conf -fg $::ngc(any,color,prompt)
        lappend msg "[_ "Axis is not responding"]"
        lappend msg "[_ "Error: "]$::ngc($hdl,axis,error)"
        lappend msg ""
        lappend msg "[_ "Autosend disabled, Ctrl-A toggles autosend"]"
        lappend msg ""
        lappend msg "[_ "File saving enabled -- Finalize to save"]"
        showerr $msg nosort
        message $hdl senderror
        return
      }
      if $::ngc($hdl,auto) {
        set ::ngc($hdl,fname,outfile) $::ngc($hdl,fname,autosend)
      } else {
        # open and write fname,outfile
        title $::ngc($hdl,top) "$::ngc(any,app) <>"
        ::ngcgui::gui $hdl setoutfile
        if {"$::ngc($hdl,fname,outfile)" == ""} {
          message $hdl usercancel
          return
        }
        if {![string match *.ngc $::ngc($hdl,fname,outfile)]} {
          lappend msg "[_ "Require .ngc suffix for filename"]"
          showerr $msg
          message $hdl writeerror
          return
        }
        if {   "$::ngc($hdl,fname,outfile)" == "$::ngc($hdl,fname,subfile)" \
            || "$::ngc($hdl,fname,outfile)" == "$::ngc($hdl,fname,preamble)" \
            || "$::ngc($hdl,fname,outfile)" == "$::ngc($hdl,fname,postamble)" \
           } {
          set msg ""
          lappend msg "[_ "Disallowed overwrite of"] $::ngc($hdl,fname,outfile)"
          showerr $msg
          message $hdl writeerror
          return
        }
      }

      if [catch {set fout [open $::ngc($hdl,fname,outfile) w]} msg] {
        lappend emsg $msg
        showerr $emsg
        message $hdl writeerror
        return
      }

      if {$::ngc(opt,nom2) || [info exists ::ngcgui::control(any,nom2)]} {
        puts $fout "%"
        puts $fout "($::ngc(any,app): nom2 option)"
      }

      set featurect 0;
      set date [dt]
      set features_total 0
      foreach thdl $hdllist {
        set features_total [expr $features_total + $::ngc($thdl,savect)]
      }
      foreach thdl $hdllist {
        # the string FEATURE is used so files generated by ngcgui can
        # be detected and excluded as subfile candidates
        puts $fout "($::ngc(any,app): [_ "FEATURE"] $date)"

        for {set i 0} {$i < [llength $::ngc($thdl,data,section)]} {incr i} {
          set line [lindex $::ngc($thdl,data,section) $i]
          if {[string first "#<_feature:>" $line] >= 0} {
            # instead of current $line, output feature count (zero referenced)
            puts $fout \
              "($::ngc(any,app): [_ "feature line added"]) #<_feature:> = $featurect"
            incr featurect 1
            set remaining [expr $features_total - $featurect]
            puts -nonewline $fout \
              "($::ngc(any,app): [_ "remaining_features line added"]) "
            puts $fout "#<_remaining_features:> = [expr $features_total -$featurect]"
          } else {
            puts $fout $line
          }
        }
      } ;# for hdllist

      if {$::ngc(opt,nom2) || [info exists ::ngcgui::control(any,nom2)]} {
        puts  $fout "%"
      } else {
        if $::ngc($endhdl,verbose) {
          puts  $fout "($::ngc(any,app): m2 [_ "line added"]) m2 (g54 [_ "activated"])"
        } else {
          puts  $fout "m2 (m2 [_ "restores"] g54)"
        }
      }
      close $fout
      set ::ngc(any,gcmc,id) 0 ;# restart after finalize

      set ::ngc($hdl,last,outfile) $::ngc($hdl,fname,outfile)
      # just in case you need it later, save a dated copy in /tmp
      if [info exists ::ngc(any,dir,just)] {
        set base     [file tail $::ngc($hdl,fname,outfile)]
        set savename [file join $::ngc(any,dir,just) [dt].${base}]
        if [catch {file copy $::ngc($hdl,fname,outfile) $savename} msg] {
          lappend emsg "<$hdl>$msg"
          showerr $emsg
          message $hdl writeerror
          return
        }
      }

      if {$::ngc($hdl,auto)} {
        if ![::ngcgui::sendfile $hdl] {
          return ;# send failed, user can start axis or Ctrl-a
        }
      }

      foreach thdl $hdllist {
        set ::ngc($thdl,savect) 0
        conf $hdl restart,widget state disabled
        set ::ngc($thdl,data,section) ""
        message $thdl finalize
      } ;# for

      title $::ngc($thdl,top) "$::ngc(any,app) \
                <[file tail $::ngc($thdl,fname,outfile)]>"

      return
    }
    default {return -code error "::ngcgui::gui: unknown mode <$mode>"}
  }
  puts stdout "[_ "NOTREACHED mode"]=<$mode>"
} ;# gui

proc ::ngcgui::savesection_ngc {hdl} {
  # could check for number here using %f
  set pfmt "%12s = %s"   ;# positional
  set cfmt "(%11s = %12s = %12s)" ;# positional comment form

  if {$::ngc($hdl,expandsubroutine)} {
    # id for unique label when expanding multiple sub files
    set id $::ngc($hdl,id)
    set uwidth 3 ;# extra width for unique label 000-999
    # $uwdith characters in unique ids
    set id [format %0${uwidth}d $::ngc($hdl,id)]
    incr ::ngc($hdl,id)
    lappend ::ngc($hdl,data,section) \
             "([_ "Positional parameters for"] $::ngc($hdl,fname,subfile):)"
    for {set i 1} {$i <= $::ngc($hdl,argct)} {incr i} {
      set num02 [format %02d $i]
      set name  $::ngc($hdl,arg,value,$num02)
      lappend ::ngc($hdl,data,section) [format $pfmt #$i $name ]
    }
    # expand the subroutine in place
    lappend ::ngc($hdl,data,section) \
            "([_ "expanded file"]: $::ngc($hdl,fname,subfile))"
    for {set i 0} {$i < $::ngc($hdl,sublines)} {incr i} {
      if [info exists ::ngc_sub($hdl,$i,label)] {
        lappend ::ngc($hdl,data,section) \
                "o<$id$::ngc_sub($hdl,$i,label)> $::ngc_sub($hdl,$i)"
      } else {
        lappend ::ngc($hdl,data,section)  \
                [format %${uwidth}s%s "" " $::ngc_sub($hdl,$i)"]
      }
    }
  } else {
    # insert the subroutine call
    if $::ngc($hdl,verbose) {
      lappend ::ngc($hdl,data,section) \
           "($::ngc(any,app): [_ "call subroutine file"]: $::ngc($hdl,fname,subfile))"
      lappend ::ngc($hdl,data,section) "($::ngc(any,app): positional parameters:)"
    }
    set cline "o<$::ngc($hdl,subroutine,name)> call "
    for {set i 1} {$i <= $::ngc($hdl,argct)} {incr i} {
      set num02 [format %02d $i]
      set name  $::ngc($hdl,arg,name,$num02)
      if {[string first # $name] == 0} {set name "?"}
      # documenting comment
      if $::ngc($hdl,verbose) {
        lappend ::ngc($hdl,data,section) \
              [format $cfmt #$i $name $::ngc($hdl,arg,value,$num02)]
      }
      set cline "$cline\[$::ngc($hdl,arg,value,$num02)\]"
    }
    lappend ::ngc($hdl,data,section) "$cline"
  }
  return 1 ;# ok
} ;# savesection_ngc

proc ::ngcgui::savesection_gcmc {hdl} {
#puts =====================================
#parray ::ngc $hdl,arg,*
#parray ::ngc $hdl,gcmc,*
#parray ::ngc any,gcmc,*
#parray ::ngc $hdl,argct
#puts =====================================
  # could check for number here using %f
  set cfmt "(%12s = %12s)" ;# positional comment form

  # maybe implement later, expand after calling gcmc below
  if {$::ngc($hdl,expandsubroutine)} {
    set answer [tk_dialog .notdoneyet \
      "Not done yet"\
      "Expand subroutine not supported for gcmc files - continuing"\
      warning -1 \
      "OK"]
  }

    if ![info exists ::ngc(any,gcmc,id)] {
      set ::ngc(any,gcmc,id) 0
    }
    incr ::ngc(any,gcmc,id) ;# id for any hdl

    set funcname $::ngc(any,gcmc,funcname)
    # gcmc chars: (allowed: [a-z0-9_-])
    set funcname ${funcname}-[format %02d $::ngc(any,gcmc,id)]

    # use first one found in searchpath:
    set ifile [file normalize \
                    [pathto [file tail $::ngc($hdl,gcmc,file)]]]
    if {"$ifile" == ""} {
      return 0 ;# fail
    }
    set ::ngc($hdl,gcmc,realfile) $ifile

    set ofile [file join $::ngc(any,gcmc,outdir) $funcname.ngc]

    set cmd $::ngc(any,gcmc,executable)
    set opts ""

    if [info exists ::ngc(any,gcmc_include_path)] {
      foreach dir [split $::ngc(any,gcmc_include_path) ":"] {
        set opts "$opts --include $dir"
      }
    }
    # note: gcmc adds the current directory
    #       to the search path as last entry.
    # maybe also ?: set opts "$opts --include [file dirname $ifile]"

    set opts "$opts --output $ofile"
    set opts "$opts --gcode-function $funcname"
    if [info exists ::ngc($hdl,gcmc,opts)] {
      foreach opt $::ngc($hdl,gcmc,opts) {
        set opts "$opts $opt"
      }
    }
    if {$::ngc($hdl,argct) > 0} {
      for {set i 1} {$i <= $::ngc($hdl,argct)} {incr i} {
        set idx [format %02d $i]
        # make all entry box values explicitly floating point
        if [catch {set floatvalue [expr 1.0 * $::ngc($hdl,arg,value,$idx)]} msg] {
          set answer [tk_dialog .gcmcerror \
              "gcmc input ERROR" \
              "<$::ngc($hdl,arg,value,$idx)> must be a number" \
              error -1 \
              "OK"]
          return 0 ;# fail
        }
        set opts "$opts --define=$::ngc($hdl,arg,name,$idx)=$floatvalue"
      }
    }

#   puts stdout "     cmd=$cmd"
#   puts stdout "    opts=$opts"
#   puts stdout "   ifile=$ifile"
#   puts stdout "funcname=$funcname"
#   puts stdout "     pwd=[pwd]"
#   puts stdout "  exists=[file exists $ifile]"

    set eline "$cmd $opts $ifile"
    if $::ngc($hdl,verbose) {
      puts stdout "eline=$eline"
    }

    #tclsh considers any output on stderr as an error
    # -ignorestderr lets it pass so that --precision 2
    # would not cause an error but then there are no
    # error messages even for hard ($? !=0) errors, just
    #    "child process exited abnormally"
    # so warnings ($?=0) cause abort even though file created
    # partial file may be left on error so you cant tell by existence
    # so, parse each warning message

    # parse messages on stderr from gcmc
    set e_message ".*Runtime message\\(\\): *\(.*\)"
    set e_warning ".*Runtime warning\\(\\): *\(.*\)"
    set e_error   ".*Runtime error\\(\\): *\(.*\)"

    set m_txt ""; set w_txt ""; set e_txt ""; set compile_txt ""
    if [catch {set result [eval exec $eline]} msg] {
      if {[string length $msg] > $::ngc(any,max_msg_len)} {
         set msg [string range $msg 0 $::ngc(any,max_msg_len)]
         set msg "$msg ..."
      }
      set lmsg [split $msg \n]
      foreach line $lmsg {
        #puts l=$line
        if {[regexp $e_message $line match txt]} {
          set  m_txt "$m_txt\n$txt"
        } elseif { [regexp $e_warning $line match txt]} {
          set  w_txt "$w_txt\n$txt"
        } elseif { [regexp $e_error $line match txt]} {
          set  e_txt "$e_txt\n$txt"
        } else {
          if {"$line" != ""} {
            set  compile_txt "$compile_txt\n$line"
          }
        }
      }
      if {"$m_txt" != ""} {
        set answer [tk_dialog .gcmcinfor \
            "gcmc INFO"\
            "gcmc file:\n$ifile\n\n$m_txt"\
            info -1 \
            "OK"]
      }
      if {"$w_txt" != ""} {
        set answer [tk_dialog .gcmcwarning \
            "gcmc WARNING"\
            "gcmc file:\n$ifile\n\n$w_txt"\
            warning -1 \
            "OK"]
      }
      if {"$e_txt" != ""} {
        set answer [tk_dialog .gcmcerror \
            "gcmc ERROR"\
            "gcmc file:\n$ifile\n\n$e_txt"\
            error -1 \
            "OK"]
      }
      if {"$compile_txt" != ""} {
        set answer [tk_dialog .gcmcerror \
            "gcmc compile ERROR"\
            "gcmc file:$compile_txt"\
            error -1 \
            "OK"]
      }
      if {"$e_txt" != ""} {
        return 0 ;# fail
      }
    } else {
      #puts "savesection_gcmc OK<$result>"
    }


    # insert the subroutine call
    lappend ::ngc($hdl,data,section) \
          "\n(NOTE: $funcname is provided by a one-time, gcmc-created file:)"
    lappend ::ngc($hdl,data,section) \
            "(      $ofile)"
    lappend ::ngc($hdl,data,section) \
          "(gcmc: File: $::ngc($hdl,gcmc,realfile))"
    lappend ::ngc($hdl,data,section) \
          "(gcmc: Options:                        )"
    if [info exists ::ngc($hdl,gcmc,opts)] {
      foreach opt $::ngc($hdl,gcmc,opts) {
         lappend ::ngc($hdl,data,section) \
            "(              $opt)"
      }
    }
    lappend ::ngc($hdl,data,section) \
          "(gcmc: Variable substitions:)"
    for {set i 1} {$i <= $::ngc($hdl,argct)} {incr i} {
       set num02 [format %02d $i]
       set name  $::ngc($hdl,arg,name,$num02)
       lappend ::ngc($hdl,data,section) \
              [format $cfmt $name $::ngc($hdl,arg,value,$num02)]
    }
    lappend ::ngc($hdl,data,section) "o<$funcname> call "

    return 1 ;# ok
} ;# savesection_gcmc

proc ::ngcgui::conf {hdl wsuffix item value} {
  set w $hdl,$wsuffix
  if ![info exists ::ngc($w)] return
  $::ngc($w) conf -$item $value
} ;# conf

proc ::ngcgui::reread {hdl} {
  ::ngcgui::gui $hdl readpreamble
  ::ngcgui::gui $hdl readsubfile
  ::ngcgui::gui $hdl readpostamble
} ;# reread

proc ::ngcgui::sendfile {hdl} {
  if ![sendaxis $hdl ping] {
    showerr $::ngc($hdl,axis,error) nosort
    message $hdl senderror
    return 0 ;# err
  }
  if ![sendaxis $hdl file] {
    showerr $::ngc($hdl,axis,error) nosort
    message $hdl senderror
    return 0 ;# err
  }
  $::ngc($hdl,sendfile,widget) conf -state disabled
  message $hdl sendfile
  return 1 ;# ok
} ;# sendfile

proc ::ngcgui::readfile {hdl item} {
  # update fname,$item and readfile
  if {    ("$item" == "preamble" || "$item" == "postamble") \
       && [string first : $::ngc($hdl,dname,$item)] == 0} {
    set ::ngc($hdl,fname,$item) "IMMEDIATE"
    set ::ngc($hdl,immediate,$item) [string range \
                                       $::ngc($hdl,fname,$item) 1 end]
    return
  }
  if {"$::ngc($hdl,dname,$item)" != ""} {
    set ptype [file pathtype $::ngc($hdl,dname,$item)]
    switch $ptype {
      relative {
        set fdir [file dirname $::ngc($hdl,fname,$item)]
        if {"$fdir" == "." } {
          set fdir $::ngc($hdl,dir) ;# -D wins for this case
        }
        set ::ngc($hdl,fname,$item)  [file normalize \
            [file join  $fdir $::ngc($hdl,dname,$item)]]
      }
      absolute {set ::ngc($hdl,fname,$item) \
                    [file normalize $::ngc($hdl,dname,$item)]
      }
      default  {return -code error "::ngcgui::readfile <$hdl $ptype>"}
    }
    # simplify dname,$item to just filename
    set ::ngc($hdl,dname,$item) [file tail $::ngc($hdl,fname,$item)]
  } else {
    #note: ngc(dname,$item) is "", each readproc must init appropriately
    set ::ngc($hdl,fname,$item) ""
  }
  switch $item {
    preamble  {::ngcgui::gui $hdl readpreamble  }
    subfile   {::ngcgui::gui $hdl readsubfile   }
    postamble {::ngcgui::gui $hdl readpostamble }
  }
} ;# readfile

proc ::ngcgui::debug {hdl} {
  set t .debug-$hdl
  catch {destroy $t}
  set t [toplevel $t]
  set lw 20;set ew 12
  # hdl,$i
  foreach i {standalone auto state lastevent \
             savect dir afterid img,orig,size img,sampled,size} {
    set f [frame $t.[qid] ]
    pack [label $f.[qid] -relief ridge -anchor e -width $lw\
         -text "$i" \
         -font $::ngc(any,font)\
         ] -fill x -expand 0 -side left
    pack [entry $f.[qid] -state readonly -relief ridge -width $ew \
         -textvariable ::ngc($hdl,$i) \
         -font $::ngc(any,font)\
         ] -fill x -expand 1 -side left
    pack $f -side top -fill x -expand 1
  }
  # any,$i
  foreach i {any,font any,width,comment any,width,varname any,pollms\
             embed,axis embed,hdl} {
    set f [frame $t.[qid] ]
    pack [label $f.[qid] -relief ridge -anchor e -width $lw\
         -text "$i" \
         -font $::ngc(any,font)\
         ] -fill x -expand 0 -side left
    pack [entry $f.[qid] -state readonly -relief ridge -width $ew \
         -textvariable ::ngc($i) \
         -font $::ngc(any,font)\
         ] -fill x -expand 1 -side left
    pack $f -side top -fill x -expand 1
  }
  wm resizable $t 1 0
} ;# debug

proc ::ngcgui::statemap {hdl} {
  # form: (next,state:mode,event) --> nextstate
  set ::ngc(any,next,reset:auto,savesection)   start
  set ::ngc(any,next,reset:noauto,savesection) start
  set ::ngc(any,next,reset:auto,restart)       reset
  set ::ngc(any,next,reset:noauto,restart)     reset

  set ::ngc(any,next,start:auto,immediate)     avail
  set ::ngc(any,next,start:noauto,immediate)   avail

  # have one or more features available:
  set ::ngc(any,next,avail:auto,savesection)   avail
  set ::ngc(any,next,avail:noauto,savesection) avail
  set ::ngc(any,next,avail:auto,restart)       reset
  set ::ngc(any,next,avail:noauto,restart)     reset

  set ::ngc(any,next,avail:auto,finalize)      reset
  set ::ngc(any,next,avail:noauto,finalize)    reset2

  set ::ngc(any,next,reset2:auto,immediate)    reset
  set ::ngc(any,next,reset2:noauto,immediate)  reset

  set ::ngc($hdl,state) reset
  set ::ngc($hdl,lastevent) notsetyet

} ;# statemap

proc ::ngcgui::message {hdl event} {
  # statemachine events (and messages)
  # ::ngc(any,next,currentstateandmode,event) specifies next state for event
  switch $::ngc($hdl,auto) {
    0 {set statemode $::ngc($hdl,state):noauto}
    1 {set statemode $::ngc($hdl,state):auto}
  }
  if ![info exists ::ngc(any,next,$statemode,$event)] {
    showmessage $hdl $event
    #puts "NOEVENT $::ngc($hdl,state) $event"
    return
  }
  set ::ngc($hdl,lastevent) $event
  set ::ngc($hdl,state) $::ngc(any,next,$statemode,$event)
  #puts "$event:   $statemode ------>$::ngc($hdl,state)"
  set mw $::ngc($hdl,msg,widget)

  # entry-to-state actions:
  # note: execute switch even if state unchanged to update gui
  switch  $::ngc($hdl,state) {
    reset {
      if {"$event" == "finalize"} {
        showmessage $hdl finalize
        update idletasks
        if $::ngc($hdl,standalone) {
          after 500 ;#pause to see messages
        }
      }
      set   ::ngc($hdl,savect)         0
      conf $hdl restart,widget state disabled
      set   ::ngc($hdl,data,section)   ""
      if [info exists ::ngc(embed,axis)] {
        set bcolor $::ngc(any,color,stdbg)
        if $::ngc($hdl,chooser) {
          set bcolor $::ngc(any,color,custom)
        }
        $::ngc(any,axis,parent) itemconfigure $::ngc($hdl,axis,page) \
            -foreground $::ngc(any,color,black) \
            -background $bcolor
      }

      title $::ngc($hdl,top) "$::ngc(any,app)"
      walktree $::ngc($hdl,varframe) normal
      walktree $::ngc($hdl,iframe) normal
      # 101024:19.49 this is better:
      focus $::ngc($hdl,topf)

      # note: dont disable sendfile,widget (wanted if noauto)
      $::ngc($hdl,finalize,widget) conf -state disabled
      $::ngc($hdl,save,widget) conf -text "[_ "Create Feature"]"
      $mw conf -text "[_ "Enter parms for 1st feature"]" \
               -fg $::ngc(any,color,prompt)
    }
    uwait {
      # alternate behavior: user must select "New Outfile"
      walktree $::ngc($hdl,varframe) disabled
      walktree $::ngc($hdl,iframe) disabled
      $::ngc($hdl,save,widget)     conf -text "[_ "New Outfile"]"
      $::ngc($hdl,finalize,widget) conf -state disabled
      $mw conf -text "[_ "Ready to make New Outfile"]" \
               -fg $::ngc(any,color,prompt)
    }
    reset2 - uwait2 {
      # just make sure sendfile is made available, then go next state
      $::ngc($hdl,sendfile,widget) conf -state normal
      after 0 [list ::ngcgui::message $hdl immediate]
    }
    start {
     walktree $::ngc($hdl,varframe) normal
     walktree $::ngc($hdl,iframe) normal
     focus $::ngc($hdl,begin,widget)

     $::ngc($hdl,save,widget)     conf -text "[_ "Create Feature"]"
     $::ngc($hdl,sendfile,widget) conf -state disabled
     $::ngc($hdl,finalize,widget) conf -state normal
     $mw conf -text "[_ "Enter parms for feature "][expr 1 + $::ngc($hdl,savect)]" \
              -fg $::ngc(any,color,prompt)
     after 0 [list ::ngcgui::message $hdl immediate]
    }
    avail {
      incr ::ngc($hdl,savect)
      conf $hdl restart,widget state active

      if [info exists ::ngc(embed,axis)] {
        if {$::ngc($hdl,savect) > 1} {
          $::ngc(any,axis,parent) itemconfigure $::ngc($hdl,axis,page) \
             -foreground $::ngc(any,color,multiple) \
             -background $::ngc(any,color,feature)
        } else {
          $::ngc(any,axis,parent) itemconfigure $::ngc($hdl,axis,page) \
             -foreground $::ngc(any,color,single) \
             -background $::ngc(any,color,feature)
        }
      }

      set t "$::ngc(any,app) $::ngc($hdl,savect) [_ "feature"]"
      if {$::ngc($hdl,savect) > 1} { set t ${t}s}
      title $::ngc($hdl,top) "$t" ;# plural
      $::ngc($hdl,finalize,widget) conf -state normal
      if {$::ngc($hdl,savect) > 0} {
        $::ngc($hdl,save,widget) conf -text "[_ "Create Next"]"
      } else {
        $::ngc($hdl,save,widget) conf -text "[_ "Create Feature"]"
      }
      $::ngc($hdl,sendfile,widget) conf -state disabled
      $mw conf -text "[_ "Created feature "]$::ngc($hdl,savect)" \
              -fg $::ngc(any,color,ok)
      after 500 [list $::ngc($hdl,msg,widget) conf \
           -text "[_ "Enter parms for feature "][expr 1 + $::ngc($hdl,savect)]" \
             -fg $::ngc(any,color,prompt)
             ]
    }
  }
} ;# message

proc ::ngcgui::title {t txt} {
  if ![info exists ::ngc(embed,axis)] {
    wm title $t $txt
  }
} ;# title

proc ::ngcgui::showmessage {hdl type} {
  # if $hdl==opt          then just show $type in *,msg,widget
  # if no $hdl,msg,widget then do nothing
  # if known type         then update widgets per $type
  # else                  then just show type in *,msg,widget
  if {"$hdl" == "opt"} {
    # no message widget since opt is for all instances
    foreach w [array names ::ngc *,msg,widget] {
      $::ngc($w) conf -text "[_ "option"] :$type $::ngc($hdl,$type)" \
                      -fg $::ngc(any,color,ok)
    }
    return
  }
  if ![info exists ::ngc($hdl,msg,widget)] return
  set ::ngc($hdl,dname,outfile) [file tail $::ngc($hdl,fname,outfile)] ;#shorten

  set mw $::ngc($hdl,msg,widget)
  switch $type {
    parmerr    {
      $mw conf -text "[_ "Missing parameters"]" \
               -fg $::ngc(any,color,error)
    }
    parseerror   {
      $mw conf -text "[_ "Parse Error"]: $::ngc($hdl,dname,subfile)" \
               -fg $::ngc(any,color,error)
      $::ngc($hdl,finalize,widget)  conf -state disabled
      $::ngc($hdl,save,widget)      conf -state disabled
    }
    nullpreamble   {
      periodic_checks $hdl ;# resync
      $mw conf -text "[_ "Null Preamble"]" \
               -fg $::ngc(any,color,ok)
    }
    readpreamble   {
      periodic_checks $hdl ;# resync
      $mw conf -text "[_ "Read Preamble"]: $::ngc($hdl,dname,preamble)" \
               -fg $::ngc(any,color,ok)
    }
    preambleerror {
      $mw conf -text "[_ "Preamble Error"]: $::ngc($hdl,dname,preamble)" \
               -fg $::ngc(any,color,error)
    }
    nullpostamble   {
      periodic_checks $hdl ;# resync
      $mw conf -text "[_ "Null Postamble"]" \
               -fg $::ngc(any,color,ok)
    }
    readpostamble   {
      periodic_checks $hdl ;# resync
      $mw conf -text "[_ "Read Postamble"]: $::ngc($hdl,dname,postamble)" \
               -fg $::ngc(any,color,ok)
    }
    postambleerror {
      $mw conf -text "[_ "Postamble Error"]: $::ngc($hdl,dname,postamble)" \
               -fg $::ngc(any,color,error)
    }
    readsubfile   {
      periodic_checks $hdl ;# resync
      $mw conf -text "[_ "Read Subfile"]: $::ngc($hdl,dname,subfile)" \
               -fg $::ngc(any,color,ok)
      $::ngc($hdl,save,widget) conf -state normal ;# restore after parseerror
    }
    writeerror   {
      $mw conf -text "[_ "Write Error"]: $::ngc($hdl,dname,outfile)" \
               -fg $::ngc(any,color,error)
    }
    setoutfile {
      $mw conf -text "[_ "Outfile set"]: $::ngc($hdl,dname,outfile)" \
               -fg $::ngc(any,color,ok)
    }
    finalize   {
      $mw conf -text \
               "[_ "Finished"]: ($::ngc($hdl,savect)): $::ngc($hdl,dname,outfile)"\
               -fg $::ngc(any,color,ok)
    }
    usercancel {
      # user canceled output file spec
      $mw conf -text "[_ "Canceled"]: $::ngc($hdl,savect) pending "\
               -fg $::ngc(any,color,warn)
      walktree $::ngc($hdl,varframe) normal
      walktree $::ngc($hdl,iframe) normal
    }
    sendfile {
      $mw conf -text "[_ "Sent"]: $::ngc($hdl,dname,outfile)" \
               -fg $::ngc(any,color,ok)
    }
    senderror {
      $mw conf -text "[_ "SendFileToAxis failed"]" \
               -fg $::ngc(any,color,error)
    }
    startup {
      $mw conf -text "[_ "Ctrl-k for Key bindings"]" \
               -fg $::ngc(any,color,ok)
    }
    expandsubroutine {
      $mw conf -text "[_ "Expand sub"] $::ngc($hdl,expandsubroutine)" \
               -fg $::ngc(any,color,ok)
    }
    retainvalues {
      $mw conf -text "[_ "Retain values"] $::ngc($hdl,retainvalues)" \
               -fg $::ngc(any,color,ok)
    }
    verbose {
      $mw conf -text "[_ "Verbose"] $::ngc($hdl,verbose)" -fg $::ngc(any,color,ok)
    }
    auto {
      $mw conf -text "[_ "Autosend"] $::ngc($hdl,auto)" -fg $::ngc(any,color,ok)
    }
    cancel {
      $mw conf -text "[_ "Finalize Canceled"]" \
               -fg $::ngc(any,color,ok)
    }
    default {
      $mw conf -text "$type" -fg $::ngc(any,color,default)
    }
  }
} ;# showmessage

proc ::ngcgui::periodic_checks {hdl} {
  after cancel $::ngc($hdl,afterid)
  if {    [info exists ::ngc(embed,axis)] \
       && ([$::ngc(any,axis,parent) raise] != "$::ngc($hdl,axis,page)") } {
     # not raised, skip tests
     set ::ngc($hdl,afterid) [after $::ngc(any,pollms) \
             [list ::ngcgui::periodic_checks $hdl]] ;#reschedule
     return
  }
  # notify for modified files
  foreach i {subfile preamble postamble} {
    set f $::ngc($hdl,fname,$i)
    if {"$f" == ""} continue
    # check for widget because it can go away
    if {   [info exists ::ngc($hdl,$i,widget)] \
        && [winfo exists $::ngc($hdl,$i,widget)]} {
       # check for change in entry widget
       if {[file tail $f] != "$::ngc($hdl,dname,$i)"} {
         # new file specified in entry box
         $::ngc($hdl,$i,widget) conf -fg $::ngc(any,color,filenew)
       } else {
         $::ngc($hdl,$i,widget)     conf -fg $::ngc(any,color,ok)
         catch {unset ::ngc($hdl,$i,reread,pending)}
       }
       # check for file removal
       if ![file readable $f] {
         # file gone/perm changed notification:
         $::ngc($hdl,$i,widget) conf -fg $::ngc(any,color,filegone)
         continue
       }
      set t [file mtime $f]
      if {   [info exists ::ngc($hdl,fname,$i,time)] \
          && $t > $::ngc($hdl,fname,$i,time)\
         } {
        # file modified notification:
        conf $hdl $i,widget     fg    $::ngc(any,color,filemod)
        conf $hdl reread,widget state normal
        conf $hdl reread,widget fg    $::ngc(any,color,filemod)
        set ::ngc($hdl,$i,reread,pending) 1
      }
    }
  }
  if {[array names ::ngc $hdl,*,reread,pending] == ""} {
     conf $hdl reread,widget fg    $::ngc(any,color,black)
     conf $hdl reread,widget state disabled
  }
  ::ngcgui::dcheck $hdl
  set ::ngc($hdl,afterid) [after $::ngc(any,pollms) \
             [list ::ngcgui::periodic_checks $hdl]] ;#reschedule
  return
} ;# periodic_checks

proc ::ngcgui::dcheck {hdl} {

  # check display of default values for positional parameters
  foreach n [array names ::ngc $hdl,arg,entrywidget,*] {
    set i1 [string last , $n]
    set num02 [string range $n [expr 1 + $i1] end]
    # under some contitions, this entrywidget may be done:
    if ![winfo exists $::ngc($hdl,arg,entrywidget,$num02)] continue
    if {   [info exists ::ngc($hdl,arg,dvalue,$num02)] \
        && "$::ngc($hdl,arg,dvalue,$num02)" \
              == "$::ngc($hdl,arg,value,$num02)"} {
       $::ngc($hdl,arg,entrywidget,$num02) conf -bg $::ngc(any,color,vdefault)
    } else {
       $::ngc($hdl,arg,entrywidget,$num02) conf \
          -bg $::ngc(any,color,stdbg);# restore default
    }
  }
} ;# dcheck

proc ::ngcgui::updownkeys {w} {
  # not compatible with axis key bindings
  # make up-arrow, down-arrow behave like tab,shift-tab navigation
  bind $w <Key-Down>   [bind all <Key-Tab>]
  bind $w <Key-Up>     [bind all <<PrevWindow>>]
  # recursion:
  foreach child [winfo children $w] {
    if {$child == ""} continue
    updownkeys $child
  }
} ;# updownkeys

proc ::ngcgui::walktree {w mode} {
  # mode == normal|disabled
  # puts "w=$w mode=$mode"
  switch [winfo class $w] {
    Button      -
    Checkbutton -
    Radiobutton -
    Entry       {
      if {[$w cget -state] == "readonly"} {
        # skip
      } else {
        $w config -state $mode
      }
    }
    Toplevel    -
    Frame       {
      # recursion:
      foreach child [winfo children $w] {
        if {$child == ""} continue
        walktree $child $mode
      }
    }
  }
} ;# walktree

proc ::ngcgui::showerr {msg "opt sort" "maxerr 10"} {
  # msg is a list; default: sort msg
  set w .showerr
  catch {destroy $w}
  set w [toplevel $w]
  set l [label $w.l -justify left]
  set text ""
  if {"$opt" == "sort"} {set msg [lsort $msg]}
  set ct 0
  foreach item $msg {
    if {$ct > $maxerr} {
      set text "$text\n..."
      break ;# avoid showing too many
    } else {
      set text "$text\n$item"
    }
    incr ct
  }
  $l configure -text $text
  pack $l -side top
  set b [button $w.b -text "[_ "Dismiss"]" \
                     -command "destroy $w"]
  pack $b -side top
  focus $b
  wm withdraw $w
  wm title    $w "[_ "ngcgui Error"]"
  update idletasks
  set x [expr [winfo screenwidth $w]/2 \
            - [winfo reqwidth $w]/2  - [winfo vrootx [winfo parent $w]]]
  set y [expr [winfo screenheight $w]/2 \
            - [winfo reqheight $w]/2  - [winfo vrooty [winfo parent $w]]]
  wm geom $w +$x+$y
  wm deiconify $w
} ;# showerr

proc ::ngcgui::bye {hdl} {
  after cancel $::ngc($hdl,afterid)
  catch {destroy $::ngc($hdl,top)}   ;# for embedded usage
  set ::ngcgui::finis 1 ;# for standalone usage
} ;# bye

proc ::ngcgui::sendaxis {hdl cmd} {
  # return 1==>ok
  switch $cmd {
    ping {
      if ![catch {send axis pwd} msg] {return 1 ;#ok}
      # tk8.5 send misfeature
      if {[string first "X server insecure" $msg] >= 0} {
         puts stdout "[_ "Declining support for tk send bug in ngcgui"]"
         puts stdout "[_ "You should upgrade linuxcnc to >= linuxcnc2.5"]"
         eval exec xhost - SI:localuser:gdm
         eval exec xhost - SI:localuser:root
         # test if that worked:
         if [::ngcgui::sendaxis $hdl ping2] {return 1 ;# ok}
      }
    }
    ping2 {
      if ![catch {send axis pwd} msg] {return 1 ;#ok}
    }
    file {
      set f [file normalize $::ngc($hdl,fname,outfile)]

      if ![catch {send axis "remote open_file_name $f"} msg] {
        if {"$msg" == ""} {
          #puts sendaxis:file:ok:<$f>msg=$msg
          if [info exists ::ngc(embed,axis)] {
            $::ngc(any,axis,parent) raise preview
            focus -force .
          }
          return 1 ;# ok
        } else {
          # nonnull msg means axis-remote cmd failed, see msg
        }
      } else {
         # axis-ui-remote command not available pre2.4
         # try method that may work for axis in linuxcnc2.3.x
         return [pre2.4_send_file_to_axis $hdl $f]
      }
    }
    default {return -code error "sendaxis: unknown cmd <$cmd>"}
  }
  set ::ngc($hdl,axis,error) \{$msg\} ;# brackets needed here
  lappend ::ngc($hdl,axis,error) {Note: Ctrl-A toggles autosend}

  return 0 ;# fail
} ;# sendaxis

proc ::ngcgui::pre2.4_send_file_to_axis {hdl f} {
  # errors may be shown on axisui but NOT detected here with pre2.4
  if ![catch {send axis open_file_name $f} msg] {
    return 1 ;# ok (expect "None")
  } else {
    # notreached i suspect
    puts "[_ "pre2.4_send_file_to_axis:error"]<$msg>"
    set ::ngc($hdl,axis,error) [list $msg]
    return 0 ;# error
  }
} ;# pre2.4_send_file_to_axis

proc ::ngcgui::entrykeybinding {ax w v} {
  # if a global ::entrykeybinding proc exists, use it only:
  if {[info proc ::entrykeybinding] != ""} {
    after 0 [list ::entrykeybinding $ax $w $v]
    return
  }
  set axis [string toupper $ax]
  # these coord values may not work for some configurations:
  switch $axis {
    X {set coord X}
    Y {set coord Y}
    Z {set coord Z}
    A {set coord A}
    B {set coord B}
    C {set coord C}
    U {set coord U}
    V {set coord V}
    W {set coord W}
    D {set coord X;# for diameter}
  }
  if {![info exists coord]} return ;# silently
  # ignore errors (standalone for example)
  if [catch {
    set value [emc_rel_act_pos $coord]
    switch $axis {
      D       {set value [expr 2.0*$value] ;# diameter}
      default {}
    }
    set value [format %.4f $value]
    after 0 [list set $v $value]
    after 0 [list $w configure -fg $::ngc(any,color,override)]
  } msg] {
    # silently ignore, emc_rel_act_pos will fail in standalone
    # puts stdout "entrykeybinding:<$msg>"
  }
} ;# entrykeybinding

proc ::ngcgui::text_width_and_length {text wname lname} {
  upvar $wname maxwidth  ;#pass by ref
  upvar $lname lines     ;#pass by ref
  set linelimit 80 ;# some lines can be real long, ex ::env(LS_COLORS)
  set start 0; set end 0; set len 0
  set maxwidth 0
  set lines 0
  while {$end >= 0} {
    set end [string first \n $text $start]
    set len [expr $end - $start]
    #puts "$len $start $end [string range $text $start $end]"
    set start [expr $end +1]
    if {$len > $maxwidth} {
      # dont use len of very long lines
      if {$len < $linelimit} {
        set maxwidth $len
      }
    }
    incr lines
  }
  return
} ;# text_width_and_length

proc ::ngcgui::simple_text {top text {title ""} } {
  #note: on first cany, top should not exist
  set maxheight 20
  set tf  $top.f
  set t   $tf.txt
  set ysb $tf.ysb
  if {![winfo exists $top]} {
    toplevel $top
    pack [frame $tf] -fill both -expand 1

    text_width_and_length "$text" twidth theight
    if {$theight > $maxheight} {set theight $maxheight}
    set t   [text $t \
             -width $twidth -height $theight\
             -yscrollcommand "$ysb set" \
            ]
    set ysb [scrollbar $ysb -command "$t yview" -relief sunken]
    set  db  [button $top.b -pady 1 -text "[_ "Dismiss"]" \
                            -command "destroy $top"]
    focus $db

    pack $t   -side left   -fill both -expand 0
    pack $ysb -side right  -fill y
    pack $db -side top  -fill x    -expand 0
    # fall-thru to insert
  } else {
    wm deiconify $top
  }
  if {"$title" != ""} { wm title $top "$title" }

  #update
  #set geo [wm geometry $top]
  #set w [string range $geo 0 [expr [string first x $geo] -1]]
  #set h [string range $geo [expr [string first x $geo +1]]\
  #                         [expr [string first + $geo] -1]]

  $t configure -state normal ;# to delete/insert
  $t delete 0.0 end
  $t insert end $text
  $t configure -state disabled ;# leave disabled: insert
  wm resizable $top 0 1
  wmcenter $top
  return $top
} ;# simple_text

proc ::ngcgui::wmcenter w {
# Withdraw the window, then update all the geometry information
# so we know how big it wants to be, then center the window in the
# display and de-iconify it.
    wm withdraw $w
    update idletasks
    set x [expr [winfo screenwidth $w]/2 \
              - [winfo reqwidth $w]/2  - [winfo vrootx [winfo parent $w]]]
    set y [expr [winfo screenheight $w]/2 \
              - [winfo reqheight $w]/2  - [winfo vrooty [winfo parent $w]]]
    wm geom $w +$x+$y
    wm deiconify $w
} ;# wmcenter

proc ::ngcgui::entry_mend {w} {
  # note: entry_mend is callable by others (ttt)
  # axis creates jog bindings for the toplevel (.==dot):
  # for <KeyPress-minus> <KeyRelease-minus>  <KeyPress-equal> <KeyRelease-equal>
  # eg: bind . <KeyPress-minus> ==> {stuff}
  # thus, for entries, bindtags are: {$e Entry . all} <-- the . is a problem
  # so, limit the bindtags for entries
  if {[winfo class $w] == "Entry"} {
    bindtags $w [list $w Entry all] ;# remove the . bindtag
    bind_for_axis $w
  }
  foreach child [winfo children $w] {
    if {$child == ""} continue
    ::ngcgui::entry_mend $child
  }
} ;# entry_mend

proc ::ngcgui::recursive_bind_controlkeys {hdl w} {
  bind_controlkeys $hdl $w
  foreach child [winfo children $w] {
    if {$child == ""} continue
    ::ngcgui::recursive_bind_controlkeys $hdl $child
  }
} ;# recursive_bind_controlkeys

proc ::ngcgui::bind_controlkeys {hdl w} {
  set ::ngc(any,kbindlist) {a c d e E f F k n p P r R s S x v t U u}
  bind $w <Control-Key-a> [list ::ngcgui::toggle $hdl auto]
  bind $w <Control-Key-c> [list ::ngcgui::setentries $hdl clear]
  bind $w <Control-Key-d> [list ::ngcgui::setentries $hdl defaults]
  bind $w <Control-Key-D> [list ::ngcgui::debug $hdl]
  bind $w <Control-Key-e> [list ::ngcgui::editfile $hdl last]
  bind $w <Control-Key-E> [list ::ngcgui::toggle $hdl expandsubroutine]
  bind $w <Control-Key-f> [list ::ngcgui::gui $hdl savesection]
  bind $w <Control-Key-F> [list ::ngcgui::gui $hdl finalize]
  bind $w <Control-Key-k> [list ::ngcgui::bindings $hdl show]
  bind $w <Control-Key-n> [list ::ngcgui::message $hdl restart]
  bind $w <Control-Key-p> [list ::ngcgui::gui $hdl readpreamble]
  bind $w <Control-Key-P> [list ::ngcgui::gui $hdl readpostamble]
  bind $w <Control-Key-r> [list ::ngcgui::gui $hdl readsubfile]
  bind $w <Control-Key-R> [list ::ngcgui::toggle $hdl retainvalues]
  bind $w <Control-Key-q> [list ::ngcgui::toggle $hdl verbose]
  bind $w <Control-Key-s> [list ::ngcgui::status $hdl]
  bind $w <Control-Key-S> [list ::ngcgui::status $hdl full]
  bind $w <Control-Key-u> [list ::ngcgui::editfile $hdl source]
  bind $w <Control-Key-U> [list ::ngcgui::editfile $hdl preamble]
  # for debugging:
  bind $w <Control-Key-x> [list parray ::ngc]
  bind $w <Control-Key-v> [list parray ::env]
  bind $w <Control-Key-t> [list ::ngcgui::test]
} ;# bind_controlkeys

proc ::ngcgui::bind_for_axis {w} {
  # Escape and other special bindings for axis embedding
  bind $w <Key-Escape> "$::ngc(any,axis,parent) raise preview" ;# allow Escape too

  # axis omits return break in estopped_clicked for F1
  bind $w <Key-F1> "[bind all <Key-F1>];break"

  # Fn keys
  foreach i {2 3 4 5 6 7 8 9 10 11 12} {
    bind $w <Key-F$i> "[bind . <Key-F$i>];break"
  }
} ;# bind_for_axis

proc ::ngcgui::bindings {hdl mode} {
  set mode [string tolower $mode] ;# -nocase doesnt work tcl8.4
  switch $mode {
    show {
      set atxt "[_ "OFF"]"
      if {$::ngc($hdl,auto)} {set atxt "[_ "ON"]"}
      set msg "\
Ctrl-a [_ "Toggle autosend"]\n\
Ctrl-c [_ "Clear entries"]\n\
Ctrl-d [_ "Set entries to default values"]\n\
Ctrl-e [_ "Open editor specified by"] \$VISUAL\n\
       [_ "on last outfile"]\n\
Ctrl-E [_ "toggle expand subroutines"]\n\
Ctrl-f [_ "Create feature"]\n\
Ctrl-F [_ "Finalize (AUTO send is"] $atxt)\n\
Ctrl-k [_ "Show key bindings"]\n\
Ctrl-n [_ "Restart (cancel pending)"]\n\
Ctrl-p [_ "(re)Read Preamble"]\n\
Ctrl-P [_ "(re)Read Postamble"]\n\
Ctrl-r [_ "(re)Read Subfile"]\n\
Ctrl-R [_ "toggle retain values"]\n\
Ctrl-q [_ "toggle output file verbosity"]\n\
Ctrl-s [_ "Show status"]\n\
Ctrl-S [_ "Show full status (debug info)"]\n\
Ctrl-u [_ "Open editor specified by"] \$VISUAL\n\
       [_ "on current subfile"]\n\
Ctrl-U [_ "Open editor specified by"] \$VISUAL\n\
       [_ "on current preamble"]\
"
      if [info exists ::ngc(embed,axis)] {
        set msg "[_ " Escape Return to Preview page"]\n$msg"
      }
      # puts $msg
      ::ngcgui::simple_text .ngcguikeys $msg "$::ngc(any,app)-$hdl-keys"
    }
    init {
      # coordinate with bind_controlkeys (x,v,t for debugging)

      if [info exists ::ngc(embed,axis)] {
        ::ngcgui::bind_for_axis $::ngc($hdl,topf)
      }

      if [info exists ::ngc(embed,axis)] {
        entry_mend $::ngc($hdl,topf)
      }
      recursive_bind_controlkeys $hdl $::ngc($hdl,topf)
      bind $::ngc($hdl,topf) <Enter> [list ::ngcgui::bindings $hdl enter]
      bind $::ngc($hdl,topf) <Leave> [list ::ngcgui::bindings $hdl leave]
      set ::ngc($hdl,restore,bindtags) [bindtags $::ngc($hdl,topf)]
      set ::ngc($hdl,restore,focus) [focus -lastfor $::ngc($hdl,topf)]
    }
    enter {
      set ::ngc($hdl,restore,bindtags) [bindtags $::ngc($hdl,topf)]
      bindtags $::ngc($hdl,topf) $::ngc($hdl,topf)

      if [info exists ::ngc(embed,axis)] {
        entry_mend $::ngc($hdl,topf)
      }
      recursive_bind_controlkeys $hdl $::ngc($hdl,topf)
      set ::ngc($hdl,restore,focus) [focus -lastfor $::ngc($hdl,topf)]
      focus  $::ngc($hdl,topf)
      return
    }
    leave {
      bindtags $::ngc($hdl,topf) $::ngc($hdl,restore,bindtags)
      focus -force $::ngc($hdl,restore,focus)
      # this seems to be necesarry with notebook pages
      foreach key $::ngc(any,kbindlist) {
        bind $::ngc($hdl,topf) <Control-Key-$key> {}
      }
    }
  }
} ;# bindings

proc ::ngcgui::aftertoggle {hdl x} {
  # hdl: handle (note: opt may be used too)
  switch $x {
    auto {
      if $::ngc($hdl,auto) {
        pack forget $::ngc($hdl,sendfile,widget)
        $::ngc($hdl,sendfile,widget) conf -state normal
        $::ngc($hdl,finalize,widget) config -text "[_ "Finalize"]"
      } else {
        pack $::ngc($hdl,sendfile,widget) -fill x
        $::ngc($hdl,finalize,widget) config -text "[_ "MakeFile"]"
      }
    }
  }
  ::ngcgui::showmessage $hdl $x
} ;# aftertoggle

proc ::ngcgui::toggle {hdl x} {
  # hdl: handle (note: opt may be used too)
  set ::ngc($hdl,$x) [expr $::ngc($hdl,$x)?0:1]
  ::ngcgui::aftertoggle $hdl $x
} ;# toggle

proc ::ngcgui::test {} {
  set text "Environmental Variables:\n"
  foreach v [lsort [array names ::env]] {
    set text "$text $v [set ::env($v)]\n"
  }
  simple_text .test $text
} ;# test

proc ::ngcgui::editfile {hdl {mode last} } {
  if ![info exists ::env(VISUAL)] {
    simple_text .problem "\n[_ "Editing requires setting for environmental variable VISUAL"] \n
[_ "Trying gedit"]\n"\
      "$::ngc(any,app)-$hdl-problem"
    set ::env(VISUAL) gedit
    update
    after 5000 {destroy .problem}
  }
  # note: normalize filename to honor tilde (~)
  switch $mode {
    last {
      if {   [info exists ::ngc($hdl,last,outfile)] \
          && "$::ngc($hdl,last,outfile)" != ""} {
        eval exec $::env(VISUAL) [file normalize $::ngc($hdl,last,outfile)] &
      } else {
        simple_text .problem "[_ "No file available for editing yet"]\n"\
          "$::ngc(any,app)-$hdl-problem"
        return
      }
    }
    source {
      if {"$::ngc($hdl,fname,subfile)" != ""} {
        eval exec $::env(VISUAL) [file normalize $::ngc($hdl,fname,subfile)] &
      } else {
        simple_text .problem "[_ "No file available for editing"]\n"\
          "$::ngc(any,app)-$hdl-problem"
        return
      }
    }
    preamble {
      if {"$::ngc($hdl,fname,preamble)" != ""} {
        eval exec $::env(VISUAL) [file normalize $::ngc($hdl,fname,preamble)] &
      } else {
        simple_text .problem "[_ "No file available for editing"]\n"\
          "$::ngc(any,app)-$hdl-problem"
        return
      }
    }
  }
} ;# editfile

proc ::ngcgui::status {hdl args} {
  set items {fname,preamble fname,subfile fname,postamble\
            fname,outfile fname,autosend\
            auto dir savect font aspect retainvalues\
            expandsubroutine chooser\
            }
  set optitems {noauto nonew noremove noiframe noinput nom2}
  set anyitems {app pollms aspect width,comment width,varname qid}

  set text "[_ "Status items"]:"
  if {"$args" == "full"} {
    #parray ::ngc;return
    set bitems [lsort [array names ::ngc $hdl,*]]
    foreach i $bitems {lappend items [string trim $i $hdl,]}
    set text "Status items(all):"
  }
  set fmt "%s: %s"
  foreach i $items {
    # catch in case item gets unset
    if [catch { set line [format "$fmt" $i $::ngc($hdl,$i)]}] continue
    set text "$text\n$line"
  }
  set text "$text\n\n[_ "All-page opt items"]:"
  foreach i $optitems {
    # catch in case item gets unset
    if [catch { set line [format "$fmt" $i $::ngc(opt,$i)]}] continue
    set text "$text\n$line"
  }
  set text "$text\n\n[_ "any-items"]:"
  foreach i $anyitems {
    # catch in case item gets unset
    if [catch { set line [format "$fmt" $i $::ngc(any,$i)]}] continue
    set text "$text\n$line"
  }
  simple_text .status $text "$::ngc(any,app)-$hdl-status"
  focus .status
  bind .status <Control-Key-s> [list ::ngcgui::status $hdl $args]
  bind .status <Control-Key-S> [list ::ngcgui::status $hdl $args]
} ;# status

proc ::ngcgui::validateNumber {hdl varname widget current new} {
  # all entries must be numbers
  if ![info exists $varname] {return 1}
  if [catch  {format %g $new} ] {
    $widget configure -fg $::ngc(any,color,error)
    return 1 ;# problem but return ok (just change color)
  } else {
    if {"$current" != "$new"} {}
    $widget configure -fg $::ngc(any,color,black)
    return 1 ;# 1==>ok
  }
} ;# validateNumber

proc ::ngcgui::setentries {hdl opt} {
  # set entries per opt == defaults | clear
  switch $opt {
    defaults {
      foreach n [array names ::ngc  $hdl,arg,dvalue,*] {
        set num02 [string range $n [expr 1+[string last , $n]] end]
        set ::ngc($hdl,arg,value,$num02) $::ngc($n)
      }
      ::ngcgui::showmessage $hdl "[_ "Set defaults"]"
    }
    clear {
      foreach n [array names ::ngc $hdl,arg,value,*] {
        set num02 [string range $n [expr 1+[string last , $n]] end]
        set ::ngc($hdl,arg,value,$num02) ""
      }
      ::ngcgui::showmessage $hdl "[_ "Clear entries"]"
    }
  }
  ::ngcgui::dcheck $hdl
} ;# setentries

proc ::ngcgui::wgui {dir} {
  # for embedded applications, this proc makes a separate-window gui
  # in the current process
  # this proc is useful for testing with tkcon:
  # to debug using tkcon: source this file then % ::ngcgui::wgui dirname
  # to run ngcgui in a frame, use ::ngcgui::gui hdl create frame
  # multiple intantiations of ngcgui within the same prcess are not supported
  package require Tk
  set hdl 0
  catch {unset ::ngc}
  ::ngcgui::preset $hdl control  ;# setup control() with defaults
  set control(any,aspect) horiz
  set control(any,font)   {Helvetica -10 bold}
# set control(any,app)    [file tail $::argv0]
  set control(any,app)    ::ngcgui::wgui       ;# with tkcon argv0 not available
  set control($hdl,auto)   1                    ;# autosend with finalize
  set control($hdl,dir)    $dir
  set control($hdl,topname) .ngcgui
  eval ::ngcgui::top $hdl control
  wm withdraw .
} ;# wgui

proc ::ngcgui::findkeybinding {w {key k} } {
  # utility
  set b [bind $w <Control-Key-$key>]
  if {"$b" != ""} {
    puts "w=$w key=$key binding=<$b>"
  }
  foreach child [winfo children $w] {
    if {$child == ""} continue
    find $child $key
  }
} ;# findkeybinding

proc ::ngcgui::top {hdl ay_name} {
  # make a standalone toplevel
  upvar $ay_name ay

  foreach n [array names ay $hdl,*] { set ::ngc($n) $ay($n) }
  foreach n [array names ay any,*]  { set ::ngc($n) $ay($n) }
  if ![info exists ::ngc($hdl,topname)] {
    set ::ngc($hdl,topname) .
    focus $::ngc($hdl,topname)
  } else {
    catch {destroy $::ngc($hdl,topname)}
    toplevel    $::ngc($hdl,topname)
  }
  wm protocol $::ngc($hdl,topname) WM_DELETE_WINDOW [list ::ngcgui::bye $hdl]

  # if autosend, make sure file is writable
  if $::ngc($hdl,auto) {
    if {"$::ngc($hdl,fname,autosend)" == ""} {
      set ::ngc($hdl,fname,autosend) auto.ngc
    }
    if ![string match *.ngc $::ngc($hdl,fname,autosend)] {
      set ::ngc($hdl,fname,autosend) $::ngc($hdl,fname,autosend).ngc
    }
    set fname $::ngc($hdl,fname,autosend)
    if [file writable $fname] {
      # ok
    } else {
      if [file exists $fname] {
        puts stdout "$fname [_ "not writable"]"
        exit 1
      } else {
        if [catch {set fd [open $fname w]} msg] {
          puts stdout $msg
          exit 1
        } else {
          close $fd
          file delete $fname
        }
      }
    }
  }

  if {"$::ngc($hdl,topname)" == "."} {
    set w [::ngcgui::gui $hdl standalone .w]
  } else {
    set w [::ngcgui::gui $hdl standalone $::ngc($hdl,topname).w]
  }

  if {"$w" == ""} {exit 1} ;# "" indicates something went wrong
  pack $w -expand 0
   switch $::ngc(any,aspect) {
     vert  {wm resizable $::ngc($hdl,top) 0 1}
     horiz {wm resizable $::ngc($hdl,top) 1 0}
   }

} ;# top

proc ::ngcgui::usage {hdl ay_name} {
  upvar $ay_name ay
  set prog [file tail $::argv0]
  set dfont "\"$ay(any,font)\"" ;# avoid messing up vim colors
  set aname $ay($hdl,fname,autosend)
  puts stdout "Usage:
  $prog --help | -?
  $prog \[Options\] -D nc_files_directory_name
  $prog \[Options\] -i LinuxCNC_inifile_name
  $prog \[Options\]

  Options:
         \[-S subroutine_file\]
         \[-p preamble_file\]
         \[-P postamble_file\]
         \[-o output_file\]
         \[-a autosend_file]\            (autosend to axis default:$aname)
         \[--noauto]\                    (no autosend to axis)
         \[-N | --nom2]\                 (no m2 terminator (use %))
         \[--font \[big|small|fontspec\]\] (default: $dfont)
         \[--horiz|--vert\]              (default: --horiz)
         \[--cwidth comment_width]\      (width of comment field)
         \[--vwidth varname_width]\      (width of varname field)
         \[--quiet]\                     (fewer comments in outfile)
         \[--noiframe]\                  (default: frame displays image)

"
  exit 0
} ;# usage

proc ::ngcgui::inifind {filename stanza item} {
  # find [STANZA]ITEM value from an ini file
  set fd [open $filename r]
  set state find_stanza
  while {![eof $fd]} {
    gets $fd theline
    # remove blanks and tabs, use lower case
    set line [string map {" " "" "	" ""} $theline] ;#sp,tab to ""
    # remove trailing comment
    set i1    [string first # $line]
    if {$i1 > 0} {
      set line [string range $line 0 [expr $i1 -1]]
    }
    switch $state {
      find_stanza {
        if [regexp -nocase "^\\\[$stanza\\\]$" $line] { set state find_item }
      }
      find_item {
        if [regexp -nocase "^\\\[.*" $line] {
          break ;# new stanza found before item
        }
        if [regexp -nocase "^$item=(.*)" $line match value] {
          set thevalue $value
          # if more than one line like item=value, take the last line
        }
      }
    }
  }
  close $fd
  if [info exists thevalue] {
    return $value
  }
  return ""
} ;# inifind

proc ::ngcgui::movepage {parent lr} {
  set pages [$parent pages]
  set  page [$parent raise]
  set   idx [lsearch $pages $page]
  switch $lr {
    left  {
      if {$idx <= $::ngc(any,axis,min,idx)} {
        return
      }
      incr idx -1
    }
    right { incr idx +1 }
  }
  $parent move $page $idx
  updatepage
} ;# movepage

proc ::ngcgui::newpage {creatinghdl} {
  set subfile "" ;# newpage: user must open file

  if $::ngc(opt,noinput) {
    # there is no wI input frame, just use current file
    # file tail needed to use search path
    set subfile [file tail $::ngc($creatinghdl,fname,subfile)]
    if {"$subfile" == ""} {
      set ::ngc(opt,noinput) 0 ;# need input if no subfile to open page
    }
  }
  if $::ngc($creatinghdl,chooser) {
    set subfile "\"\""  ;# chooser starts with no specified subfile
  }

  set prefile ""
  set postfile ""
  if {"$::ngc($creatinghdl,dname,preamble)" != ""} {
    # file tail needed to use search path
    set prefile [file tail $::ngc($creatinghdl,fname,preamble)]
  }
  if {"$::ngc($creatinghdl,dname,postamble)" != ""} {
    # file tail needed to use search path
    set postfile [file tail $::ngc($creatinghdl,fname,postamble)]
  }

  set pageid ngcgui[qid]
  set w [$::ngc(any,axis,parent) insert end "$pageid" \
        -text "[_ "new"]"
        ]
  $w config -borderwidth 0 ;# not sure why this needs to be by itself
  set f [frame $w.[qid] -borderwidth 0 -highlightthickness 0]
  pack $f -fill both -expand 1 -anchor nw -side top

  # note: express font as list here is important fore embedded spaces
  set newhdl [embed_in_axis_tab $f \
              subfile=$subfile \
              preamble=$prefile \
              postamble=$postfile \
              font=$::ngc(any,font) \
              options=$::ngc(input,options) \
              gcmc_include_path=$::ngc(input,gcmc_include_path) \
             ]
  $::ngc(any,axis,parent) itemconfigure $pageid \
        -createcmd "::ngcgui::pagecreate $newhdl"\
        -raisecmd  "::ngcgui::pageraise  $newhdl"\
        -leavecmd  "::ngcgui::pageleave  $newhdl"

  # use directory from creating page
  set ::ngc($newhdl,dir) [file dir $::ngc($creatinghdl,fname,subfile)]
  $::ngc(any,axis,parent) raise $::ngc($newhdl,axis,page)
  if {$::ngc(opt,noinput) && ("$::ngc($newhdl,dname,subfile)" != "")} {
    set ::ngc($newhdl,info) "$::ngc($newhdl,dname,subfile)"
  } else {
    set ::ngc($newhdl,info) "[_ "Open a new Subfile"]"
  }
  updatepage
} ;# newpage

proc ::ngcgui::nextpage {pagename lr} {
  # next page to use after this page is deleted
  set hdl [pagetohdl $pagename]
  if {$hdl <0} {return -code error \
    "nextpage:unexpected pagename <$pagename>"
  }
  set page $::ngc($hdl,axis,page)
  set pages [$::ngc(any,axis,parent) pages]
  set lastidx [expr -1 + [llength $pages]]
  set idx [lsearch $pages $page]
  switch $lr {
    left  {
      if {$idx <= $::ngc(any,axis,min,idx)} {
        incr idx +1 ;# since idx page will be deleted
      } else {
        incr idx -1
      }
    }
    right {
      if {$idx >= $lastidx} {
        incr idx -1 ;# since idx page will be deleted
      } else {
        incr idx +1
      }
    }
  }
  set newpage [lindex $pages $idx]
  return $newpage
} ;# nextpage

proc ::ngcgui::pageexists {hdl} {
  if [info exists ::ngc($hdl,axis,page)] {return 1}
  return 0
} ;# pageexists

proc ::ngcgui::deletepage {pagename} {
  set hdl [pagetohdl $pagename]
  if {$hdl <0} {return -code error \
    "deletepage:unexpected pagename <$pagename>"
  }
  set newpage [nextpage $pagename left]
  after cancel $::ngc($hdl,afterid)
  $::ngc(any,axis,parent) delete $::ngc($hdl,axis,page)

  wm protocol $::ngc($hdl,img,top) WM_DELETE_WINDOW {}
  destroy $::ngc($hdl,img,top)

  foreach n [array names ::ngc $hdl,*] {
    unset ::ngc($n)
  }

  set idx [lsearch $::ngc(embed,pages) $pagename]
  set ::ngc(embed,pages) [lreplace $::ngc(embed,pages) $idx $idx]
  $::ngc(any,axis,parent) raise $newpage
  updatepage
} ;# deletepage

proc ::ngcgui::updatepage {} {
  set parent $::ngc(any,axis,parent)
  set allpages [$parent pages] ;# these are in tab order
  foreach page [$parent pages] {
    if {[lsearch $::ngc(embed,pages) $page] < 0} continue
    lappend orderedpages $page
  }
  if ![info exists orderedpages] return ;# can occur at start
  if {[llength $orderedpages] == 1} {
    set p $orderedpages
    foreach w {,move,l,widget move,r,widget ,remove,widget} {
      if [info exists ::ngc($p$w)] {
        $::ngc($p$w) config -state disabled
      }
    }
    return
  }
  foreach p $orderedpages {
    set idx [lsearch $orderedpages $p]
    if {$idx == 0} {
      $::ngc($p,move,l,widget) config -state disabled
      $::ngc($p,move,r,widget) config -state active
    } elseif {$idx == [expr -1 +[llength $orderedpages]]} {
      $::ngc($p,move,l,widget) config -state active
      $::ngc($p,move,r,widget) config -state disabled
    } else {
      $::ngc($p,move,l,widget) config -state active
      $::ngc($p,move,r,widget) config -state active
    }
    # remove,widget not always present
    if [info exists ::ngc($p,remove,widget)] {
      $::ngc($p,remove,widget) config -state active}
  }

  # if choosers exist, do not allow removal of last one
  set ct 0
  foreach name [array names ::ngc *,chooser] {
    if $::ngc($name) {
      incr ct
      lappend chdls [trimsuffix $name ,chooser]
    }
  }
  if {$ct == 1} {
    set chdl $chdls
    set page $::ngc($chdl,axis,page)
    $::ngc($page,remove,widget) configure -state disabled
  } elseif {$ct > 1} {
    foreach chdl $chdls {
      set page $::ngc($chdl,axis,page)
      $::ngc($page,remove,widget) configure -state active
    }
  }
} ;# updatepage

proc ::ngcgui::pagetohdl {pagename} {
  foreach name [array names ::ngc *,axis,page] {
    if {"$::ngc($name)" == "$pagename"} {
      return [trimsuffix $name ,axis,page]
      break
    }
  }
  return -1
} ;# pagetohdl

proc ::ngcgui::tabmanage {pagename wframe ident infovar \
                         {removable 0} {newable 0} } {
  # filler frame to put space below page tabs
  pack [frame $wframe.[qid] -relief flat -height 1m\
       ] -anchor n -fill both -expand 0

  set   af [frame $wframe.[qid] -relief ridge -bd 2]
  pack $af -fill x -expand 0 -anchor n ;# always pack to hold space

  # another filler frame to put space below page tabs
  pack [frame $wframe.[qid] -relief flat -height 1m\
       ] -anchor n -fill both -expand 0

  pack [label $wframe.[qid] -relief flat -anchor w \
       -textvariable $infovar \
       -fg $::ngc(any,color,prompt)\
  ] -anchor ne -fill both -expand 0

  if $removable {
    set hdl [pagetohdl $pagename]
    set b [button $af.[qid] -text "[_ "remove"]" \
                            -padx 2 -pady 1]
    $b configure -command [list ::ngcgui::deletepage $pagename]
    pack $b -side left -fill none -expand 0
    set ::ngc($pagename,remove,widget) $b
  }

  if $newable {
    set hdl [pagetohdl $pagename]
    set b [button $af.[qid] -text "[_ "new"]" \
                            -padx 2 -pady 1]
    $b configure -command [list ::ngcgui::newpage $hdl]
    pack $b -side left -fill none -expand 0
  }

  set l [label  $af.[qid] \
       -text "$ident" \
       -padx 2 -pady 1 -relief ridge\
       ]
  pack $l -side left -fill x -expand 1

  set parent $::ngc(any,axis,parent)
  set b [button $af.[qid] -text "[_ "move"]-->" \
                          -padx 2 -pady 1]
  $b configure -command [list ::ngcgui::movepage $parent right]
  pack $b -side right -fill none -expand 0
  set ::ngc($pagename,move,r,widget) $b

  set b [button $af.[qid] -text "<--[_ "move"]" \
                          -padx 2 -pady 1]
  $b configure -command [list ::ngcgui::movepage $parent left]
  pack $b -side right -fill none -expand 0
  set ::ngc($pagename,move,l,widget) $b
  updatepage
} ;# tabmanage

proc ::ngcgui::parent {} {return $::ngc(any,axis,parent)}

proc ::ngcgui::getngcgui_frame {name} {
  # utility for applications managed by ngcgui
  set wtab [dynamic_tab $name $name] ;# axis function
  set w [frame $wtab.[qid] -container 0 -borderwidth 0 -highlightthickness 0]
  pack $w -side top -fill both -expand 1 -anchor nw

  lappend ::ngc(embed,pages) $name
  return $w
} ;# getngcgui_frame

proc ::ngcgui::embed_in_axis_tab {f args} {
  # f: frame
  # args: "item=value item=value ..."

  if ![info exists ::ngc(embed,hdl)] {
    set ::ngc(embed,axis) 1
    set ::ngc(embed,hdl) 0
    set ::ngc(embed,pages) ""

    set ::ngc(any,axis,parent) [winfo parent [winfo parent $f]]

    # dont allow movement of tab to left of original location:
    set idx [lsearch [$::ngc(any,axis,parent) pages] \
                     [$::ngc(any,axis,parent) pages end]]
    if {$idx < 0} {
      set ::ngc(any,axis,min,idx)  10000
    } else {
      set ::ngc(any,axis,min,idx)  $idx
    }
  } else {
    incr ::ngc(embed,hdl)
  }
  set hdl $::ngc(embed,hdl) ;# local
  initgui $hdl

  ::ngcgui::preset $hdl ::ngc   ;# setup defaults

  set equalitems {subfile preamble postamble \
                  font \
                  startdir \
                  gcmc_include_path \
                  options \
                 }
  foreach item $equalitems {set ::ngc(input,$item) ""}
  foreach input  $args {
    set  pair [split $input =]
    set ::ngc(input,[lindex $pair 0]) [lindex $pair 1]
    # ex: input,subfile
  }
  foreach item $equalitems {set $item $::ngc(input,$item)}
  if [info exists ::ngc(input,gcmc_include_path)] {
    set ::ngc(any,gcmc_include_path) $::ngc(input,gcmc_include_path)
  }

  set ::ngc($hdl,dir) $::ngc(input,startdir)

  if {[lsearch $options nonew     ] >=0} {set ::ngc(opt,nonew)      1}
  if {[lsearch $options noremove  ] >=0} {set ::ngc(opt,noremove)   1}
  if {[lsearch $options noauto    ] >=0} {set ::ngc(opt,noauto)     1}
  if {[lsearch $options noinput   ] >=0} {set ::ngc(opt,noinput)    1}
  if {[lsearch $options noiframe  ] >=0} {set ::ngc(opt,noiframe)   1}
  if {[lsearch $options nom2      ] >=0} {set ::ngc(opt,nom2)       1}

  if {[lsearch $options expandsub ] >=0} {set ::ngc($hdl,expandsubroutine)   1}

  # special options
  if {[lsearch $options nopathcheck ] >=0} {set ::ngc($hdl,nopathcheck)   1}

  if $::ngc(opt,noauto) {
    set ::ngc($hdl,auto) 0
  } else {
    set ::ngc($hdl,auto) 1
  }
  # with image in frame there is not enough room so force noinput
  if !$::ngc(opt,noiframe) {set ::ngc(opt,noinput) 1}

  set ::ngc(any,width,comment) 0  ;# field can be as long as reqd

  set ::ngc($hdl,axis,page)   [$::ngc(any,axis,parent) pages end]
  set page $::ngc($hdl,axis,page) ;# local

  # if font has leading/trailing literal quotes, remove them
  if {   [string first \" $font] == 0 \
      && [string last  \" $font] == [expr [string len $font] -1]} {
     set font [string range $font 1 [expr [string len $font] -2]]
  }
  if {"$font" != ""} {set ::ngc(any,font) $font}

  # specific settings for embedding in axis tab:
  set ::ngc(any,aspect)        horiz
  set ::ngc(any,width,varname) 0
  if {"$subfile" != ""} {
    # detect ini file specified as ""
    # this is a chooser page -- user can open new files
    if {"$subfile" == "\"\""} {
      set ::ngc($hdl,chooser)        1
      set ::ngc($hdl,fname,subfile)  ""
      $::ngc(any,axis,parent) itemconfigure $page \
            -text "[_ "Custom"]" \
            -background $::ngc(any,color,custom)
    } else {
      if [info exists ::ngc($hdl,nopathcheck)] {
        # subfile must be a valid absolute path for this option
        # example: ttt uses /tmp directory specified with full path
        #          to avoid creating persistent files
        #          relying on purging of /tmp
        set ::ngc($hdl,fname,subfile) $subfile
        set ::ngc($hdl,dir) [file dirname $subfile]
      } else {
        set ::ngc($hdl,fname,subfile) [::ngcgui::pathto $subfile]
        set ::ngc($hdl,dir) [file dirname $::ngc($hdl,fname,subfile)]
      }
    }
  }
  if {"$preamble"  != ""} {
    set ::ngc($hdl,fname,preamble) [::ngcgui::pathto $preamble]
  }
  if {"$postamble" != ""} {
    set ::ngc($hdl,fname,postamble) [::ngcgui::pathto $postamble]
  }

  set  w [::ngcgui::gui $hdl create $f.ngc_gui]

  if {"$w" == ""} {
    puts stdout "[_ "Problem creating page"] <$hdl> <$f>"
  } else  {
    pack $w -side top -fill none -expand 1 -anchor nw
  }
  # package require Linuxcnc ;# needs linuxcnc v2.5.x, segfaults linuxcnc v2.4.x
  # just invoking emc_init works with v2.4 and v2.5
  if  [catch {emc_init} msg] {
    puts "embed_in_axis_tab: [_ "entrykeybindings not available"] <$msg>"
  }
  lappend ::ngc(embed,pages) $page
  updatepage

  return $hdl
} ;# embed_in_axis_tab

proc ::ngcgui::set_path {} {
  # set ::ngc(any,paths) on first use:
  if ![info exists ::ngc(any,paths)] {
    # expect single item, so take end item in list:
    set ::ngc(any,paths) [file normalize \
                         [lindex [inifindall DISPLAY PROGRAM_PREFIX] end]]
    set tmp [lindex [inifindall RS274NGC SUBROUTINE_PATH] end]
    foreach p [split $tmp ":"] {lappend ::ngc(any,paths) "$p"}
  }
} ;# get_path

proc ::ngcgui::pathto {fname  {mode info}} {
  # for embedded usage, find configuration file using a search path
  set fname [string trim $fname]
  if {"$fname" == ""} {return ""}

  set_path ;# if not set, will set

  if {   [string first "/" $fname] == 0
      || [string first "~" $fname] == 0
      || [string first "." $fname] == 0
     } {
    if [file exists $fname] {
      # expected usage: spcecify search path [RS274NGC]SUBROUTINE_PATH
      #            and: specify [DISPLAY]NGCGUI_SUBFILE as a file name only
      #
      # future:  maybe it should be an error to use an absolute path
      #          since the interpreter may not find the file
      # for now: only use a file if it is in search path
      set foundabsolute "$fname"
      set fname [file tail $fname] ;# to test if it is in search path
    }
  }
  foreach path $::ngc(any,paths) {
    set f [file join $path $fname]
    if {[info exists foundinpath] && [file exists $f]} {
      puts stdout "::ngcgui::pathto: [_ "Found multiple matches for"] <$fname>"
      puts stdout "[_ "using path"]: $::ngc(any,paths)"
    }
    if {![info exists foundinpath] && [file exists $f]} {set foundinpath $f}
  }

  if [info exists foundinpath] {
    if {   [info exists foundabsolute] \
        && [file normalize $foundinpath] != [file normalize $foundabsolute] } {
      puts "\nngcgui [_ "Warning"]:"
      puts "[_ "File absolute path specifier conflicts with searchpath result"]"
      puts "     [_ "Absolute Specifier"]:  $foundabsolute"
      puts "     [_ "Using Search Result"]: $foundinpath"
      puts ""
    }
    return "$foundinpath"
  } else {
    set title "[_ "File not in Search Path"]"

    set msg "<$fname> [_ "Must be in search path"]\n"
    if {[info exists foundabsolute]} {
      set msg "$msg\n[_ "(File found -- not in search path)"]"
    }
    set msg "$msg\n[_ "Current directory"]:\n[pwd]"
    set msg "$msg\n\n[_ "Search path"]:\n"
    set i 1
    foreach p $::ngc(any,paths) {
      set msg "$msg\n$i  $p"
      set fullp [file normalize $p]
      if {"$p" != "$fullp"} {
        set msg "$msg\n== $fullp"
      }
      incr i
    }
    set msg "$msg\n\n[_ "Check setting for"]: \[RS274NGC\]SUBROUTINE_PATH"
    set msg "$msg\n[_ "in ini file"]:\n$::emcini"
    set msg "$msg\n\n[_ "(Restart required after fixing ini file)"]"
    switch $mode {
      info {
        set answer [tk_dialog .notfound \
          "$title"\
          "$msg"\
          warning -1 \
          "OK"]
        set answer 0 ;# continue with warning
      }
      default {
        set answer [tk_dialog .notfound \
          "$title"\
          "$msg" \
          error 0 \
          "[_ "Try to Continue"]" "[_ "Exit"]"
        ]
      }
    }
    if $answer {return \
      -code error "[_ "Ngcgui Configuration File Not Found"] <$fname>"
    }
    if ![info exists foundabsolute] {set foundabsolute ""}
    return "$foundabsolute" ;# try to continue
  }
} ;# pathto

proc ::ngcgui::check_path filename {
  if [info exists ::ngc(embed,axis)] {
    pathto [file tail $filename] info
  }
  return
} ;# check_path

proc ::ngcgui::raiselastpage {} {
  $::ngc(any,axis,parent) raise $::ngc($::ngc(embed,hdl),axis,page)
} ;# raiselastpage

proc ::ngcgui::position {top} {
  set geo [wm geometry $top]
  return [string range $geo [string first + $geo] end]
} ;# position

proc ::ngcgui::pagecreate {hdl} {
  #puts "n:pagecreate-$hdl"
  return 1
} ;# pagecreate

proc ::ngcgui::pageraise {hdl} {
  #puts "n:pageraise-$hdl"
  set ::ngc($hdl,img,status) raised
  if {"$::ngc($hdl,fname,subfile)" != ""} {
    new_image $hdl $::ngc($hdl,fname,subfile)
  }
  return 1
} ;# pageraise

proc ::ngcgui::pageleave {hdl} {
  #puts "n:pageleave-$hdl"
  set ::ngc($hdl,img,position) [position $::ngc($hdl,img,top)]
  wm withdraw $::ngc($hdl,img,top)
  return 1 ;# important: permission to leave
} ;# pageleave

proc ::ngcgui::image_init {hdl} {
  set ::ngc($hdl,img,status)    new
  if [info exists ::ngc(embed,axis)] {
    set ::ngc($hdl,img,top) .$::ngc(any,app)-$hdl
  } else {
    set ::ngc($hdl,img,top) .$::ngc(any,app)
  }
  if [winfo exists $::ngc($hdl,img,top)] return
  wm withdraw [toplevel $::ngc($hdl,img,top)]
  wm protocol $::ngc($hdl,img,top) WM_DELETE_WINDOW \
              [list wm withdraw $::ngc($hdl,img,top)]

  if {$::ngc(opt,noinput) && !$::ngc($hdl,chooser)} {
    pack forget $::ngc($hdl,iframe) ;# wI remove the Input frame
  }
  if {   (!$::ngc(opt,noiframe) && !$::ngc($hdl,chooser) )\
      || (!$::ngc(opt,noiframe) &&  $::ngc($hdl,standalone) )\
      } {
    # use a frame for image
    set p [winfo parent $::ngc($hdl,iframe)]
    set w $p.[qid] ;# name of frame
    set ::ngc($hdl,img,widget) [image_widget $hdl $w]
    set ::ngc($hdl,img,type) frame
  } else {
    # use a toplevel for image
    set ::ngc($hdl,img,widget) [image_widget $hdl $::ngc($hdl,img,top).i]
    set ::ngc($hdl,img,type) toplevel
  }
  # note: new_image packs $::ngc($hdl,img,widget)
} ;# image_init

proc ::ngcgui::image_widget {hdl f} {
  # f is name of a frame, it should not exist at call, caller packs
  # png, pgm,ppm etc support
  if [catch {package require Img} msg] {
    tk_dialog .img \
      "[_ "Missing Tcl Package Img"] " \
      "[_ "Please install Img"]:\n $ sudo apt-get install libtk-img" \
      "" 0 \
      "ok"
    exit
  }
  if {[winfo exists $f]} {return -code error "image_widget <$w> exists"}
  frame $f ;# caller packs

  set fimg [frame $f.fimg -relief groove -borderwidth 2]
  pack $fimg -side top -expand 1 -fill both

  set ::ngc($hdl,img,canvas) [canvas $fimg.canvas -bg darkgray ]
  pack $::ngc($hdl,img,canvas) -side left -expand 1 -fill both
  return $f
} ;# image_widget

proc ::ngcgui::new_image {hdl ngcfilename} {
  set idx [string first .ngc $ngcfilename]
  if {$idx < 0} { set idx [string first .gcmc $ngcfilename]}
  if {$idx < 0}  { return -code error \
                   "new_image: unexpected filename: <$ngcfilename>"}

  set filestart [string range $ngcfilename 0 $idx]
  foreach suffix {png gif jpg pgm} {
    set f ${filestart}$suffix
    if [file readable $f] {
      set ifilename $f
      break
    }
  }
  if ![info exists ifilename] {
     catch {unset ::ngc($hdl,img,filename)}
     catch {pack forget $::ngc($hdl,img,widget)} ;# standalone
     catch {wm withdraw $::ngc($hdl,img,top)} ;# needed for standalone
     return ;# silently continue
  }

  set doimage 0
  if ![info exists ::ngc($hdl,img,filename)] {
    set ::ngc($hdl,img,status) first
    set doimage 1
  } else {
    if {"$::ngc($hdl,img,filename)" != "$ifilename"} {
      set ::ngc($hdl,img,position) [position $::ngc($hdl,img,top)]
      set ::ngc($hdl,img,status) new
      set doimage 1
    }
  }

  if {$doimage} {
    # first time for this file for this hdl
    set ::ngc($hdl,img,filename) $ifilename
    pack forget $::ngc($hdl,img,widget)
    set tmpimage [image create photo -file $ifilename]
    set ct 0
    set sw [expr [image width  $tmpimage] / $::ngc(any,img,width,max)  + 1]
    set sh [expr [image height $tmpimage] / $::ngc(any,img,height,max) + 1]
    set subsample $sw
    if  {$sh > $sw} {set subsample $sh}
    set ::ngc($hdl,img,image) [image create photo]
    $::ngc($hdl,img,image) copy $tmpimage -subsample $subsample -shrink

    set width  [image  width  $::ngc($hdl,img,image)]
    set height [image  height $::ngc($hdl,img,image)]
    # convenience only:
    set ::ngc($hdl,img,orig,size)  [image width $tmpimage]x[image height $tmpimage]
    set ::ngc($hdl,img,sampled,size)  ${width}x${height}

    $::ngc($hdl,img,canvas) delete all
    $::ngc($hdl,img,canvas) configure -width $width -height $height
    $::ngc($hdl,img,canvas) create image [expr $width/2] [expr $height/2]\
                      -anchor center \
                      -image $::ngc($hdl,img,image)
    recursive_bind_controlkeys $hdl $::ngc($hdl,img,top)
    pack $::ngc($hdl,img,widget)
  }

  # restore the image widget toplevel if applicable
  if {"$::ngc($hdl,img,type)" == "toplevel"} {
    switch $::ngc($hdl,img,status) {
       first {
         if [info exists ::ngc($hdl,img,position)] {
           wmrestore $hdl
         } else {
           wmcenter $::ngc($hdl,img,top)
         }
         if {  ![info exists ::ngc(embed,axis)] \
             || [$::ngc(any,axis,parent) raise] == $::ngc($hdl,axis,page)} {
           set ::ngc($hdl,img,status) raised ;# need for standalone
         } else {
           wm withdraw $::ngc($hdl,img,top)
         }
      }
      new -
      raised { wmrestore $hdl }
    }
    wm resizable $::ngc($hdl,img,top) 0 0
    wm title $::ngc($hdl,img,top) [trimsuffix $::ngc($hdl,dname,subfile)]
  }
} ;# new_image

proc ::ngcgui::wmrestore {hdl} {
  set w $::ngc($hdl,img,top)
  wm deiconify $w
  if [catch {
        if [info exists ::ngc($hdl,img,position)] {
          wm geometry $w $::ngc($hdl,img,position)
        }
   } msg] {
      puts stdout "wmrestore: unexpected<$msg>"
   }
} ;# wmrestore

# configure standalone usage:
proc ::ngcgui::standalone_ngcgui {args} {
    # setup ::ngcgui::control() with defaults
    set hdl 0
    ::ngcgui::preset $hdl ::ngcgui::control
    package require Tk
    # configure for standalone usage
    # map dot (.) to underline (_) to preclude window naming errors:
    set ::ngcgui::control(any,app) [string map {. _} [file tail $::argv0]]

    while {[llength $::argv] >0} {
      # beware wish handling of reserved cmdline arguments
      # to use -h: use -- -h,
      # lreplace shifts argv for no. of items for each iteration
      switch -- [lindex $::argv 0] {
        --noiframe {set ::ngc(opt,noiframe) 1
                    set ::argv [lreplace $::argv 0 0]
                    }
        -h - -? -
        --help {::ngcgui::usage $hdl ::ngcgui::control;exit 0}
        --horiz -
        -horiz {set ::ngcgui::control(any,aspect) horiz
                set ::argv [lreplace $::argv 0 0]
               }
        --vert -
        -vert  {set ::ngcgui::control(any,aspect) vert
                set ::argv [lreplace $::argv 0 0]
               }
        -q      -
        --quiet {
                 set ::ngcgui::control($hdl,verbose) 0
                 set ::argv [lreplace $::argv 0 0]
                }
        --font -
        -font  {set ::ngcgui::control(any,font) [lindex $::argv 1]
                set ::argv [lreplace $::argv 0 1]
               }
        --vwidth {set ::ngcgui::control(any,width,varname) [lindex $::argv 1]
                  set ::argv [lreplace $::argv 0 1]
                 }
        --cwidth {set ::ngcgui::control(any,width,comment) [lindex $::argv 1]
                  set ::argv [lreplace $::argv 0 1]
                 }
        -N       -
        --nom2   {set ::ngcgui::control(any,nom2) 0
                  set ::argv [lreplace $::argv 0 0]
                 }
        -S          -
        --subfile  {set ::ngcgui::control($hdl,fname,subfile) [lindex $::argv 1]
                     set ::argv [lreplace $::argv 0 1]
                   }
        -p         -
        --preamble {set ::ngcgui::control($hdl,fname,preamble) \
                          [lindex $::argv 1]
                     set ::argv [lreplace $::argv 0 1]
                   }
        -P          -
        --postamble {set ::ngcgui::control($hdl,fname,postamble) \
                           [lindex $::argv 1]
                     set ::argv [lreplace $::argv 0 1]
                    }
        -o       -
        --output {set ::ngcgui::control($hdl,fname,outfile) [lindex $::argv 1]
                  set ::argv [lreplace $::argv 0 1]
                 }
        -D          -
        --dir       {
                     # -D allows dir specification with no filenames
                     set ans [lindex $::argv 1]
                     if [file isdirectory $ans] {
                       set ::ngcgui::control($hdl,dir) $ans
                     } else {
                       set ::ngcgui::control($hdl,dir) [file dirname $ans]
                     }
                     set ::argv [lreplace $::argv 0 1]
                    }
        -a          -
        --autosend  {set ::ngcgui::control($hdl,auto) 1
                     set ::ngcgui::control($hdl,fname,autosend) \
                           [lindex $::argv 1]
                     set ::argv [lreplace $::argv 0 1]
                    }
        --noautosend -
        --noauto     {set ::ngcgui::control($hdl,auto) 0
                      set ::argv [lreplace $::argv 0 0]
                     }

        -i          -
        --ini*      {
                       set filename [lindex $::argv 1]
                       if ![file readable $filename] {
                         puts "[_ "ini file"]: <$filename> not readable"
                         exit 1
                       }
                       set ::argv [lreplace $::argv 0 1]
                       set dir [file normalize [file dirname $filename]]
                       set pdir [::ngcgui::inifind $filename \
                                           DISPLAY PROGRAM_PREFIX]
                       set pdir [file normalize $pdir]
                       if {"$pdir" == ""} {
                         puts "\[DISPLAY\]PROGRAM_PREFIX [_ "not found"] <$filename>"
                         exit 1
                       }
                       set ptype [file pathtype $pdir]
                       switch $ptype {
                         relative {set inidir [file join $dir $pdir]}
                         absolute {set inidir [file normalize $pdir]}
                         default  {puts "unhandled pathtype for $pdir <$ptype>"
                                   exit 1
                                  }
                       }
                       set ::ngcgui::control($hdl,dir) $inidir
                    }
        default {break}
      }
    }
    if {"$::ngcgui::control(any,font)" == ""} {
      set ::ngcgui::control(any,font) small
    }
    switch -- $::ngcgui::control(any,font) {
      small   {set ::ngcgui::control(any,font) {Helvetica -10 bold}}
      big     {set ::ngcgui::control(any,font) {Helvetica -16 bold}}
      default {}
    }
    # ::ngcgui::control() specifies args
    eval ::ngcgui::top $hdl ::ngcgui::control
    tkwait variable ::ngcgui::finis
    exit 0
} ;# standalone_ngcgui

if {[info exists ::argv0] && [info script] == $::argv0} ::ngcgui::standalone_ngcgui

