# Load the emc.tcl file, which defines variables for various useful paths
source [file join [file dirname [info script]] emc.tcl]

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


wm ti . [msgcat::mc "EMC2 Errors"]
frame .f
label .f.b -bitmap error
label .f.l -justify l -wraplength 400 -text [msgcat::mc "EMC2 terminated with an error.  When reporting problems, please include the information below in your message."]
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
insert_file .f2.t "Kernel message information:" {|dmesg} \
    "^.*Adeos: Pipelining started."
insert_file .f2.t "Debug file information:" [lindex $argv 0]
.f2.t configure -state disabled

frame .f3
button .f3.b1 -text [msgcat::mc "Select All"] -command {.f2.t tag add sel 0.0 end; tk_textCopy .f2.t} -width 15 -padx 4 -pady 1
button .f3.b2 -text [msgcat::mc "Close"] -command {destroy .} -width 15 -padx 4 -pady 1
pack .f3.b1 -side left -anchor e -padx 4 -pady 4
pack .f3.b2 -side left -anchor e -padx 4 -pady 4
pack .f3 -side top -anchor e
