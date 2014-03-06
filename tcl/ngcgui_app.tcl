# read ini items, source ngcgui.tcl, create axis tabs
# provide tcl package Ngcgui

#-----------------------------------------------------------------------
# Copyright: 2010-2012
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
#-----------------------------------------------------------------------

proc ngcgui_app_init {} {
  if {[info command inifindall] == ""} {
    return -code error "ngcgui_app_init: [_ "requires command inifindall
from axis.py (LinuxCNC 2.5) or"] \[DISPLAY\]USER_COMMAND_FILE (LinuxCNC 2.4)"
  }

  # DISPLAY:NGCGUI specifies the file normally named ngcgui.tcl
  #                not required if tcl packages are used
  #                but it could be specified for testing to
  #                supersede the installed ngcgui.tcl
  set ngcgui           [lindex [inifindall DISPLAY NGCGUI]           end]

  # DIAPLAY:NGCGUI_*  expect these inifile items to be specified as a list
  set ngcgui_subfile   [inifindall DISPLAY NGCGUI_SUBFILE]
  # optional (expect single item, so take last item in list):
  set ngcgui_preamble  [lindex [inifindall DISPLAY  NGCGUI_PREAMBLE]  end]
  set ngcgui_postamble [lindex [inifindall DISPLAY  NGCGUI_POSTAMBLE] end]
  set ngcgui_font      [lindex [inifindall DISPLAY  NGCGUI_FONT]      end]
  set ngcgui_options   [lindex [inifindall DISPLAY  NGCGUI_OPTIONS]   end]
  set program_prefix   [lindex [inifindall DISPLAY  PROGRAM_PREFIX]   end]
  set subroutine_path  [lindex [inifindall RS274NGC SUBROUTINE_PATH]  end]
  set gcmc_include_path [lindex [inifindall DISPLAY GCMC_INCLUDE_PATH]  end]


  # allow specification of DISPLAY:NGCGUI for:
  #  1) compatibility with use of DISPLAY:USER_COMMAND_FILE
  #  2) allow substitition of alternate for testing
  set libngcgui [file join [file dirname [info script]] ngcgui.tcl]
  if {"$ngcgui" == ""} {
    set ngcgui $libngcgui ;# normally, use the library supplied ngcgui.tcl
  } else {
    if {"$ngcgui" != "$libngcgui"} {
      puts stderr "ngcgui_app_init: [_ "Substituting"] $ngcgui [_ "for"] $libngcgui"
    }
  }
  if ![file readable $ngcgui] {
    return -code error "ngcgui_app.tcl: <$ngcgui> [_ "not readable"]"
  }
  if {[info procs ::ngcui::gui] == ""} {
    source $ngcgui ;# main ngcgui code
  } else {
    # this can occur due to obsolete USER_COMMAND_FILE
    puts "[_ "Unexpected: multiple startups for ngcgui"] <$ngcgui>"
    puts "\n[_ LinuxCNC version"] = $::version"
    puts "[_ "for linuxCNC 2.5.xxx, Do not include tkapp.py in the ini file"]\n"
    return -code error "[_ "Unexpected: multiple startups for ngcgui"] <$ngcgui>"
  }

  if {"$subroutine_path" != ""} {
    # first choice is first element of subroutine_path
    set startdir [lindex [split $subroutine_path :] 0]
  } elseif {"$program_prefix" != ""} {
    set startdir $program_prefix
  } else {
    set startdir "."
  }

  set ::ngcguict 0
  foreach fname $ngcgui_subfile {
    set wtab [dynamic_tab ngcgui$::ngcguict [file tail $fname] ] ;# axis func
    if {"$wtab" == "None"} {
      # functions of tkapp.py were incorporated in LinuxCNC  2.5 27dec10 af6ae9907e1c0
      #puts "\nerrorInfo: $::errorInfo"
      puts "\n[_ "LinuxCNC version"] = $::version"
      puts "[_ "for LinuxCNC 2.5.xxx, Do not include tkapp.py in the ini file"]\n"
      continue
    }
    set w [frame $wtab.tframe -container 0 -borderwidth 0 -highlightthickness 0]
    pack $w -side top -fill both -expand 1 -anchor nw

    ::ngcgui::embed_in_axis_tab $w \
              subfile=$fname \
              preamble=$ngcgui_preamble \
              postamble=$ngcgui_postamble \
              font=$ngcgui_font \
              startdir=$startdir \
              options=$ngcgui_options \
              gcmc_include_path=$gcmc_include_path
    incr ::ngcguict
  }
  return
} ;# ngcgui_app_init

ngcgui_app_init  ;# sources ngcgui.tcl
rename ngcgui_app_init {} ;# no longer needed
package provide Ngcgui 1.0
