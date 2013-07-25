#    This is a part of EMC2
#    Copyright 2006-2009 Jeff Epler <jepler@unpythonic.net>
#
#    This program is free software; you can redistribute it and/or modify
#    it under the terms of the GNU General Public License as published by
#    the Free Software Foundation; either version 2 of the License, or
#    (at your option) any later version.
#
#    This program is distributed in the hope that it will be useful,
#    but WITHOUT ANY WARRANTY; without even the implied warranty of
#    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#    GNU General Public License for more details.
#
#    You should have received a copy of the GNU General Public License
#    along with this program; if not, write to the Free Software
#    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA

# Load the emc package, which defines variables for various useful paths
package require Linuxcnc

proc insert_file {w title f {p {}}} {
    set f [open $f r]
    set t [read $f]
    close $f

    if {$p != {} && [regexp -all -linestop -lineanchor -indices $p $t match]} {
        set match [lindex $match 0]
        set t [string range $t $match end]
    }

    $w insert end "$title\n" title
    if {[string compare $t {}]} {
        $w insert end $t
    } else {
        $w insert end "(empty)\n"
    }
    $w insert end "\n"
}


wm ti . [msgcat::mc "LinuxCNC Errors"]
frame .f
label .f.b -bitmap error
label .f.l -justify l -wraplength 400 -text [msgcat::mc "LinuxCNC terminated with an error.  When reporting problems, please include all the information below in your message."]
pack .f.b -side left -padx 8 -pady 8
pack .f.l -side left
pack .f -side top -fill x -anchor w

frame .f2
text .f2.t -yscrollcommand [list .f2.s set] -height 24
scrollbar .f2.s -command [list .f2.t yview]
grid rowconfigure .f2 0 -weight 1
grid columnconfigure .f2 0 -weight 1
grid .f2.t -row 0 -column 0 -sticky nsew
grid .f2.s -row 0 -column 1 -sticky ns
.f2.t tag configure title -font {Helvetica 16 bold}
pack .f2 -fill both -expand 1

insert_file .f2.t "Print file information:" [lindex $argv 1]
insert_file .f2.t "Debug file information:" [lindex $argv 0]
# FIXME the SIMULATOR variable is removed.  The following should only
# be relevant for RTAI anyway.  For now, just run this all the time.
# if {$linuxcnc::SIMULATOR != "yes"} {
#     insert_file .f2.t "Kernel message information:" {|dmesg} \
# 	"^.*Adeos: Pipelining started\.|^.*I-pipe: Domain RTAI registered\."
# }
insert_file .f2.t "Kernel message information:" {|dmesg} \
	"^.*Adeos: Pipelining started\.|^.*I-pipe: Domain RTAI registered\."
# /FIXME
.f2.t configure -state disabled

frame .f3
button .f3.b1 -text [msgcat::mc "Select All"] -command {.f2.t tag add sel 0.0 end; tk_textCopy .f2.t} -width 15 -padx 4 -pady 1
button .f3.b2 -text [msgcat::mc "Close"] -command {destroy .} -width 15 -padx 4 -pady 1
pack .f3.b1 -side left -anchor e -padx 4 -pady 4
pack .f3.b2 -side left -anchor e -padx 4 -pady 4
pack .f3 -side top -anchor e
