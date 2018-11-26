#    This is a component of AXIS, a front-end for LinuxCNC
#    Copyright 2004, 2005, 2006, 2007, 2008, 2009, 2010, 2011, 2012, 2013, 2014, 2015, 2016
#    Jeff Epler <jepler@unpythonic.net> and Chris Radek <chris@timeguy.com>
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
#    Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.


# Supply bindings missing from tcl8.6
if {[bind Scale <Left>] == ""} {
     bind Scale <Left> { tk::ScaleIncrement %W up little noRepeat }
}
if {[bind Scale <Right>] == ""} {
     bind Scale <Right> { tk::ScaleIncrement %W down little noRepeat }
}

lappend auto_path $::linuxcnc::TCL_LIB_DIR

. configure \
	-menu .menu

menu .menu \
	-cursor {}

menu .menu.file \
	-tearoff 0
menu .menu.file.recent \
	-tearoff 0
menu .menu.machine \
	-tearoff 0
menu .menu.machine.home \
	-tearoff 0
menu .menu.machine.unhome \
	-tearoff 0
menu .menu.view \
	-tearoff 0
menu .menu.help \
	-tearoff 0
menu .menu.machine.touchoff \
    -tearoff 0
menu .menu.machine.clearoffset \
    -tearoff 0

.menu.file add command \
	-accelerator O \
	-command open_file
setup_menu_accel .menu.file end [_ "_Open..."]

.menu.file add cascade \
        -menu .menu.file.recent
setup_menu_accel .menu.file end [_ "Recent _Files"]

.menu.file add command \
    -command edit_program
setup_menu_accel .menu.file end [_ "_Edit..."]

.menu.file add command \
	-accelerator [_ "Ctrl-R"] \
	-command reload_file
setup_menu_accel .menu.file end [_ "_Reload"]

.menu.file add command \
        -accelerator [_ "Ctrl-S"] \
        -command save_gcode
setup_menu_accel .menu.file end [_ "_Save gcode as..."]

.menu.file add command \
        -command gcode_properties
setup_menu_accel .menu.file end [_ "_Properties..."]

.menu.file add separator

.menu.file add command \
    -command edit_tooltable
setup_menu_accel .menu.file end [_ "Edit _tool table..."]

.menu.file add command \
	-command reload_tool_table
setup_menu_accel .menu.file end [_ "Reload tool ta_ble"]

.menu.file add separator

.menu.file add command \
        -command {exec classicladder &}
setup_menu_accel .menu.file end [_ "_Ladder Editor..."]

.menu.file add separator

.menu.file add command \
	-command {destroy .}
setup_menu_accel .menu.file end [_ "_Quit"]

# ----------------------------------------------------------------------
.menu.machine add command \
	-accelerator F1 \
	-command estop_clicked
setup_menu_accel .menu.machine end [_ "Toggle _Emergency Stop"]

.menu.machine add command \
	-accelerator F2 \
	-command onoff_clicked
setup_menu_accel .menu.machine end [_ "Toggle _Machine Power"]

.menu.machine add separator

.menu.machine add command \
	-accelerator R \
	-command task_run
setup_menu_accel .menu.machine end [_ "_Run program"]

.menu.machine add command \
	-command task_run_line
setup_menu_accel .menu.machine end [_ "Ru_n from selected line"]

.menu.machine add command \
	-accelerator T \
	-command task_step
setup_menu_accel .menu.machine end [_ "S_tep"]

.menu.machine add command \
	-accelerator P \
	-command task_pause
setup_menu_accel .menu.machine end [_ "_Pause"]

.menu.machine add command \
	-accelerator S \
	-command task_resume
setup_menu_accel .menu.machine end [_ "Re_sume"]

.menu.machine add command \
	-accelerator ESC \
	-command task_stop
setup_menu_accel .menu.machine end [_ "Stop"]

.menu.machine add checkbutton \
        -command toggle_optional_stop \
        -variable optional_stop
setup_menu_accel .menu.machine end [_ "Stop at M_1"]

.menu.machine add checkbutton \
        -command toggle_block_delete \
        -variable block_delete
setup_menu_accel .menu.machine end [_ "Skip lines with '_/'"]

.menu.machine add separator

.menu.machine add command \
	-accelerator [_ "Ctrl-M"] \
	-command clear_mdi_history
setup_menu_accel .menu.machine end [_ "Clear MDI h_istory"]
.menu.machine add command \
	-accelerator [_ "Ctrl-H"] \
	-command mdi_history_hist2clip
setup_menu_accel .menu.machine end [_ "Copy from MDI hist_ory"]
.menu.machine add command \
	-accelerator [_ "Ctrl-Shift-H"] \
	-command mdi_history_clip2hist
setup_menu_accel .menu.machine end [_ "Paste to MDI histor_y"]

.menu.machine add separator

.menu.machine add command \
        -command {exec $env(LINUXCNC_TCL_DIR)/bin/emccalib.tcl -- -ini $emcini &}
setup_menu_accel .menu.machine end [_ "_Calibration"]

.menu.machine add command \
        -command {exec $env(LINUXCNC_TCL_DIR)/bin/halshow.tcl &}
setup_menu_accel .menu.machine end [_ "Show _Hal Configuration"]

.menu.machine add command \
        -command {exec halmeter &}
setup_menu_accel .menu.machine end [_ "H_al Meter"]

.menu.machine add command \
        -command {exec halscope -- -ini $emcini &}
setup_menu_accel .menu.machine end [_ "Ha_l Scope"]

.menu.machine add command \
	-command {exec linuxcnctop -ini $emcini &}
setup_menu_accel .menu.machine end [_ "Sho_w LinuxCNC Status"]

.menu.machine add command \
	-command {exec debuglevel -ini $emcini &}
setup_menu_accel .menu.machine end [_ "Set _Debug Level"]

.menu.machine add separator

.menu.machine add cascade \
        -menu .menu.machine.home
setup_menu_accel .menu.machine end [_ "Homin_g"]

.menu.machine add cascade \
        -menu .menu.machine.unhome
setup_menu_accel .menu.machine end [_ "_Unhoming"]

.menu.machine add cascade \
    -menu .menu.machine.clearoffset
setup_menu_accel .menu.machine end [_ "_Zero coordinate system"]

.menu.machine.clearoffset add command \
    -command [list clear_offset 1]
setup_menu_accel .menu.machine.clearoffset end [_ "P1  G5_4"]

.menu.machine.clearoffset add command \
    -command [list clear_offset 2]
setup_menu_accel .menu.machine.clearoffset end [_ "P2  G5_5"]

.menu.machine.clearoffset add command \
    -command [list clear_offset 3]
setup_menu_accel .menu.machine.clearoffset end [_ "P3  G5_6"]

.menu.machine.clearoffset add command \
    -command [list clear_offset 4]
setup_menu_accel .menu.machine.clearoffset end [_ "P4  G5_7"]

.menu.machine.clearoffset add command \
    -command [list clear_offset 5]
setup_menu_accel .menu.machine.clearoffset end [_ "P5  G5_8"]

.menu.machine.clearoffset add command \
    -command [list clear_offset 6]
setup_menu_accel .menu.machine.clearoffset end [_ "P6  G5_9"]

.menu.machine.clearoffset add command \
    -command [list clear_offset 7]
setup_menu_accel .menu.machine.clearoffset end [_ "P7  G59._1"]

.menu.machine.clearoffset add command \
    -command [list clear_offset 8]
setup_menu_accel .menu.machine.clearoffset end [_ "P8  G59._2"]

.menu.machine.clearoffset add command \
    -command [list clear_offset 9]
setup_menu_accel .menu.machine.clearoffset end [_ "P9  G59._3"]

.menu.machine.clearoffset add command \
    -command [list clear_offset G92]
setup_menu_accel .menu.machine.clearoffset end [_ "_G92"]

.menu.machine add separator

.menu.machine add radiobutton \
	-variable tto_g11 \
        -value 0 \
        -command toggle_tto_g11
setup_menu_accel .menu.machine end [_ "Tool touch off to wor_kpiece"]

.menu.machine add radiobutton \
	-variable tto_g11 \
        -value 1 \
        -command toggle_tto_g11
setup_menu_accel .menu.machine end [_ "Tool touch off to _fixture"]

# ----------------------------------------------------------------------
.menu.view add radiobutton \
	-command set_view_z \
        -variable view_type \
        -value 1 \
	-accelerator V
setup_menu_accel .menu.view end [_ "_Top view"]

.menu.view add radiobutton \
	-command set_view_z2 \
        -variable view_type \
        -value 2 \
	-accelerator V
setup_menu_accel .menu.view end [_ "_Rotated Top view"]

.menu.view add radiobutton \
	-command set_view_x \
        -variable view_type \
        -value 3 \
	-accelerator V
setup_menu_accel .menu.view end [_ "_Side view"]

.menu.view add radiobutton \
	-command set_view_y \
        -variable view_type \
        -value 4 \
	-accelerator V
setup_menu_accel .menu.view end [_ "_Front view"]

.menu.view add radiobutton \
	-command set_view_p \
        -variable view_type \
        -value 5 \
	-accelerator V
setup_menu_accel .menu.view end [_ "_Perspective view"]

.menu.view add separator

.menu.view add radiobutton \
	-value 0 \
	-variable metric \
	-command redraw \
        -accelerator !
setup_menu_accel .menu.view end [_ "Display _Inches"]

.menu.view add radiobutton \
	-value 1 \
	-variable metric \
	-command redraw \
        -accelerator !
setup_menu_accel .menu.view end [_ "Display _MM"]

.menu.view add separator

.menu.view add checkbutton \
	-variable show_program \
	-command toggle_show_program
setup_menu_accel .menu.view end [_ "S_how program"]

.menu.view add checkbutton \
	-variable show_rapids \
	-command toggle_show_rapids
setup_menu_accel .menu.view end [_ "Show program r_apids"]

.menu.view add checkbutton \
	-variable program_alpha \
	-command toggle_program_alpha
setup_menu_accel .menu.view end [_ "Alpha-_blend program"]

.menu.view add checkbutton \
	-variable show_live_plot \
	-command toggle_show_live_plot
setup_menu_accel .menu.view end [_ "Sho_w live plot"]

.menu.view add checkbutton \
	-variable show_tool \
	-command toggle_show_tool
setup_menu_accel .menu.view end [_ "Show too_l"]

.menu.view add checkbutton \
	-variable show_extents \
	-command toggle_show_extents
setup_menu_accel .menu.view end [_ "Show e_xtents"]

.menu.view add cascade \
	-menu .menu.view.grid
setup_menu_accel .menu.view end [_ "_Grid"]

.menu.view add checkbutton \
	-variable show_offsets \
	-command toggle_show_offsets
setup_menu_accel .menu.view end [_ "Show o_ffsets"]

.menu.view add checkbutton \
	-variable show_machine_limits \
	-command toggle_show_machine_limits
setup_menu_accel .menu.view end [_ "Sh_ow machine limits"]

.menu.view add checkbutton \
	-variable show_machine_speed \
	-command toggle_show_machine_speed
setup_menu_accel .menu.view end [_ "Show v_elocity"]

.menu.view add checkbutton \
	-variable show_distance_to_go \
	-command toggle_show_distance_to_go
setup_menu_accel .menu.view end [_ "Show _distance to go"]

.menu.view add checkbutton \
	-variable dro_large_font \
	-command toggle_dro_large_font
setup_menu_accel .menu.view end [_ "Large coordinate fo_nt"]

.menu.view add command \
	-accelerator [_ "Ctrl-K"] \
	-command clear_live_plot
setup_menu_accel .menu.view end [_ "_Clear live plot"]

.menu.view add checkbutton \
	-variable show_pyvcppanel \
	-accelerator [_ "Ctrl-E"] \
	-command toggle_show_pyvcppanel
setup_menu_accel .menu.view end [_ "Show pyVCP pan_el"]

.menu.view add separator

.menu.view add radiobutton \
	-value 1 \
	-variable display_type \
	-accelerator @ \
	-command redraw
setup_menu_accel .menu.view end [_ "Show commanded position"]

.menu.view add radiobutton \
	-value 0 \
	-variable display_type \
	-accelerator @ \
	-command redraw
setup_menu_accel .menu.view end [_ "Show actual position"]

.menu.view add separator

.menu.view add radiobutton \
	-value 0 \
	-variable coord_type \
	-accelerator # \
	-command redraw
setup_menu_accel .menu.view end [_ "Show machine position"]

.menu.view add radiobutton \
	-value 1 \
	-variable coord_type \
	-accelerator # \
	-command redraw
setup_menu_accel .menu.view end [_ "Show relative position"]

.menu.view add separator

.menu.view add radiobutton \
        -value 0 \
        -variable teleop_mode \
        -accelerator $ \
        -command set_teleop_mode
setup_menu_accel .menu.view end [_ "Joint mode"]

.menu.view add radiobutton \
        -value 1 \
        -variable teleop_mode \
        -accelerator $ \
        -command set_teleop_mode
setup_menu_accel .menu.view end [_ "World mode"]

menu .menu.view.grid

.menu.view.grid add radiobutton \
        -value 0 \
        -variable grid_size \
        -command set_grid_size
setup_menu_accel .menu.view.grid end [_ "_Off"]

.menu.view.grid add radiobutton \
        -value -1 \
        -variable grid_size \
        -command set_grid_size_custom
setup_menu_accel .menu.view.grid end [_ "_Custom"]


# ----------------------------------------------------------------------
.menu.help add command \
	-command {
            wm transient .about .;
            wm deiconify .about;
            show_all .about.message;
            focus .about.ok
        }
setup_menu_accel .menu.help end [_ "_About AXIS"]

.menu.help add command \
	-command {wm transient .keys .;wm deiconify .keys; focus .keys.ok}
setup_menu_accel .menu.help end [_ "Quick _Reference"]


# ----------------------------------------------------------------------
.menu add cascade \
	-menu .menu.file
setup_menu_accel .menu end [_ _File]

.menu add cascade \
	-menu .menu.machine
setup_menu_accel .menu end [_ _Machine]

.menu add cascade \
	-menu .menu.view
setup_menu_accel .menu end [_ _View]

.menu add cascade \
	-menu .menu.help
setup_menu_accel .menu end [_ _Help]

frame .toolbar \
	-borderwidth 1 \
	-relief raised

vrule .toolbar.rule16

Button .toolbar.machine_estop \
	-helptext [_ "Toggle Emergency Stop \[F1\]"] \
	-image [load_image tool_estop] \
	-relief sunken \
	-takefocus 0
bind .toolbar.machine_estop <Button-1> { estop_clicked }
setup_widget_accel .toolbar.machine_estop {}

Button .toolbar.machine_power \
	-command onoff_clicked \
	-helptext [_ "Toggle Machine power \[F2\]"] \
	-image [load_image tool_power] \
	-relief link \
	-state disabled \
	-takefocus 0
setup_widget_accel .toolbar.machine_power {}

vrule .toolbar.rule0

Button .toolbar.file_open \
	-command { open_file } \
	-helptext [_ "Open G-Code file \[O\]"] \
	-image [load_image tool_open] \
	-relief link \
	-takefocus 0
setup_widget_accel .toolbar.file_open {}

Button .toolbar.reload \
	-command { reload_file } \
	-helptext [_ "Reopen current file \[Control-R\]"] \
	-image [load_image tool_reload] \
	-relief link \
	-takefocus 0
setup_widget_accel .toolbar.reload {}

vrule .toolbar.rule4

Button .toolbar.program_run \
	-command task_run \
	-helptext [_ "Begin executing current file \[R\]"] \
	-image [load_image tool_run] \
	-relief link \
	-takefocus 0
setup_widget_accel .toolbar.program_run {}

Button .toolbar.program_step \
	-command task_step \
	-helptext [_ "Execute next line \[T\]"] \
	-image [load_image tool_step] \
	-relief link \
	-takefocus 0
setup_widget_accel .toolbar.program_step {}

Button .toolbar.program_pause \
	-command task_pauseresume \
	-helptext [_ "Pause \[P\] / resume \[S\] execution"] \
	-image [load_image tool_pause] \
	-relief link \
	-takefocus 0
setup_widget_accel .toolbar.program_pause {}

proc pause_image_normal {} {
  .toolbar.program_pause configure -image [load_image tool_pause]
}
proc pause_image_override {} {
  .toolbar.program_pause configure -image [load_image resume_inhibit]
}

Button .toolbar.program_stop \
	-command task_stop \
	-helptext [_ "Stop program execution \[ESC\]"] \
	-image [load_image tool_stop] \
	-relief link \
	-takefocus 0
setup_widget_accel .toolbar.program_stop {}

vrule .toolbar.rule8

Button .toolbar.program_blockdelete \
        -command { set block_delete [expr {!$block_delete}]; toggle_block_delete } \
        -helptext [_ "Toggle skip lines with '/' \[Alt-M /\]"] \
	-image [load_image tool_blockdelete] \
        -relief link \
        -takefocus 0

Button .toolbar.program_optpause \
        -command { set optional_stop [expr {!$optional_stop}]; toggle_optional_stop } \
        -helptext [_ "Toggle optional pause \[Alt-M 1\]"] \
	-image [load_image tool_optpause] \
        -relief link \
        -takefocus 0

vrule .toolbar.rule9
 
Button .toolbar.view_zoomin \
	-command zoomin \
	-helptext [_ "Zoom in"] \
	-image [load_image tool_zoomin] \
	-relief link \
	-takefocus 0
setup_widget_accel .toolbar.view_zoomin {}

Button .toolbar.view_zoomout \
	-command zoomout \
	-helptext [_ "Zoom out"] \
	-image [load_image tool_zoomout] \
	-relief link \
	-takefocus 0
setup_widget_accel .toolbar.view_zoomout {}

Button .toolbar.view_z \
	-command set_view_z \
	-helptext [_ "Top view"] \
	-image [load_image tool_axis_z] \
	-relief sunken \
	-takefocus 0
setup_widget_accel .toolbar.view_z {}

Button .toolbar.view_z2 \
	-command set_view_z2 \
	-helptext [_ "Rotated top view"] \
	-image [load_image tool_axis_z2] \
	-relief link \
	-takefocus 0
setup_widget_accel .toolbar.view_z2 {}

Button .toolbar.view_x \
	-command set_view_x \
	-helptext [_ "Side view"] \
	-image [load_image tool_axis_x] \
	-relief link \
	-takefocus 0
setup_widget_accel .toolbar.view_x {}

Button .toolbar.view_y \
	-command set_view_y \
	-helptext [_ "Front view"] \
	-image [load_image tool_axis_y] \
	-relief link \
	-takefocus 0
setup_widget_accel .toolbar.view_y {}

Button .toolbar.view_y2 \
	-command set_view_y2 \
	-helptext [_ "Inverted Front view"] \
	-image [load_image tool_axis_y2] \
	-relief link \
	-takefocus 0
setup_widget_accel .toolbar.view_y2 {}

Button .toolbar.view_p \
	-command set_view_p \
	-helptext [_ "Perspective view"] \
	-image [load_image tool_axis_p] \
	-relief link \
	-takefocus 0
setup_widget_accel .toolbar.view_p {}

Button .toolbar.rotate \
        -image [load_image tool_rotate] \
	-helptext [_ "Toggle between Drag and Rotate Mode \[D\]"] \
        -relief link \
        -command {
            set rotate_mode [expr {!$rotate_mode}]
            if {$rotate_mode} {
                .toolbar.rotate configure -relief sunken
            } else {
                .toolbar.rotate configure -relief link
            }
        }

vrule .toolbar.rule12

Button .toolbar.clear_plot \
	-command clear_live_plot \
	-helptext [_ "Clear live plot \[Ctrl-K\]"] \
	-image [load_image tool_clear] \
	-relief link \
	-takefocus 0
setup_widget_accel .toolbar.clear_plot {}

# Pack widget .toolbar.machine_estop
pack .toolbar.machine_estop \
	-side left

# Pack widget .toolbar.machine_power
pack .toolbar.machine_power \
	-side left

# Pack widget .toolbar.rule0
pack .toolbar.rule0 \
	-fill y \
	-padx 4 \
	-pady 4 \
	-side left

# Pack widget .toolbar.file_open
pack .toolbar.file_open \
	-side left

# Pack widget .toolbar.reload
pack .toolbar.reload \
	-side left

# Pack widget .toolbar.rule4
pack .toolbar.rule4 \
	-fill y \
	-padx 4 \
	-pady 4 \
	-side left

# Pack widget .toolbar.program_run
pack .toolbar.program_run \
	-side left

# Pack widget .toolbar.program_step
pack .toolbar.program_step \
	-side left

# Pack widget .toolbar.program_pause
pack .toolbar.program_pause \
	-side left

# Pack widget .toolbar.program_stop
pack .toolbar.program_stop \
	-side left

# Pack widget .toolbar.rule8
pack .toolbar.rule8 \
	-fill y \
	-padx 4 \
	-pady 4 \
	-side left

# Pack widget .toolbar.program_blockdelete
pack .toolbar.program_blockdelete \
	-side left

# Pack widget .toolbar.program_optpause
pack .toolbar.program_optpause \
	-side left

# Pack widget .toolbar.rule9
pack .toolbar.rule9 \
	-fill y \
	-padx 4 \
	-pady 4 \
	-side left


# Pack widget .toolbar.view_zoomin
pack .toolbar.view_zoomin \
	-side left

# Pack widget .toolbar.view_zoomout
pack .toolbar.view_zoomout \
	-side left

# Pack widget .toolbar.view_z
pack .toolbar.view_z \
	-side left

# Pack widget .toolbar.view_z2
pack .toolbar.view_z2 \
	-side left

# Pack widget .toolbar.view_x
pack .toolbar.view_x \
	-side left

# Pack widget .toolbar.view_y
pack .toolbar.view_y \
	-side left

# Pack widget .toolbar.view_y2
pack .toolbar.view_y2 \
	-side left

# Pack widget .toolbar.view_p
pack .toolbar.view_p \
	-side left

# Pack widget .toolbar.rotate
pack .toolbar.rotate \
	-side left

# Pack widget .toolbar.rule12
pack .toolbar.rule12 \
	-fill y \
	-padx 4 \
	-pady 4 \
	-side left

# Pack widget .toolbar.clear_plot
pack .toolbar.clear_plot \
	-side left

panedwindow .pane \
        -borderwidth 0 \
        -handlesize 5 \
        -orient v \
        -sashpad 0 \
        -showhandle 1

set pane_top [frame .pane.top]
set pane_bottom [frame .pane.bottom]
.pane add $pane_top -sticky nsew
.pane add $pane_bottom -sticky nsew
catch {
    .pane paneconfigure $pane_top -stretch always
    .pane paneconfigure $pane_bottom -stretch never
}

NoteBook ${pane_top}.tabs \
	-borderwidth 2 \
	-arcradius 3
proc show_all_tabs w {
    upvar 0 NoteBook::$w data
    set a [winfo reqwidth $w]
    set b [expr $data(wpage) + 3]
    if {$a < $b} { $w configure -width $b }
}
after 1 after idle show_all_tabs ${pane_top}.tabs
proc set_pane_minsize {} {
    global pane_bottom pane_top
    .pane paneconfigure $pane_top -minsize [winfo reqheight $pane_top]
    .pane paneconfigure $pane_bottom -minsize [winfo reqheight $pane_bottom]
}
after 1 after idle set_pane_minsize

set _tabs_manual [${pane_top}.tabs insert end manual -text [_ "Manual Control \[F3\]"] -raisecmd {focus .; ensure_manual}]
set _tabs_mdi [${pane_top}.tabs insert end mdi -text [_ "MDI \[F5\]"]]
$_tabs_manual configure -borderwidth 2
$_tabs_mdi configure -borderwidth 2

${pane_top}.tabs itemconfigure mdi -raisecmd "[list focus ${_tabs_mdi}.command];"
#${pane_top}.tabs raise manual
after idle {
    ${pane_top}.tabs raise manual
    ${pane_top}.right raise preview 
    after idle ${pane_top}.tabs compute_size
    after idle ${pane_top}.right compute_size
}

label $_tabs_manual.axis
setup_widget_accel $_tabs_manual.axis [_ Axis:]

frame $_tabs_manual.axes

# Axis select radiobuttons
# These set the variable ja_rbutton to alphabetic value
foreach {letter} {x y z a b c u v w} {
  radiobutton $_tabs_manual.axes.axis$letter \
	-anchor w \
	-padx 0 \
	-value $letter \
	-variable ja_rbutton \
	-width 2 \
        -text [string toupper $letter] \
        -command axis_activated
}

# grid for Axis select radiobuttons
set ano 0; set aletters {x y z a b c u v w}
for {set row 0} {$row < 3} {incr row} {
  for {set col 0} {$col < 3} {incr col} {
    grid $_tabs_manual.axes.axis[lindex $aletters $ano] \
         -column $col -row $row -padx 4
    incr ano
  }
}

frame $_tabs_manual.joints

# Joints select radiobuttons
# These set the variable ja_rbutton to numeric values [0,MAX_JOINTS)
for {set num 0} {$num < $::MAX_JOINTS} {incr num} {
  radiobutton $_tabs_manual.joints.joint$num \
	-anchor w \
	-padx 0 \
	-value $num \
	-variable ja_rbutton \
	-width 2 \
        -text $num \
        -command axis_activated
}

# grid for Joint select radiobuttons
set jno 0
set rows [expr $::MAX_JOINTS / 3]
for {set row 0} {$row < 3} {incr row} {
  for {set col 0} {$col < 3} {incr col} {
    grid $_tabs_manual.joints.joint$jno \
         -column $col -row $row -padx 4
    incr jno
    if {$jno >= $::MAX_JOINTS} break
  }
  if {$jno >= $::MAX_JOINTS} break
}

frame $_tabs_manual.jogf
frame $_tabs_manual.jogf.jog

button $_tabs_manual.jogf.jog.jogminus \
	-command {if {![is_continuous]} {jog_minus 1}} \
	-padx 0 \
	-pady 0 \
	-width 2 \
        -text -
bind $_tabs_manual.jogf.jog.jogminus <Button-1> {
    if {[is_continuous]} { jog_minus }
}
bind $_tabs_manual.jogf.jog.jogminus <ButtonRelease-1> {
    if {[is_continuous]} { jog_stop }
}

button $_tabs_manual.jogf.jog.jogplus \
	-command {if {![is_continuous]} {jog_plus 1}} \
	-padx 0 \
	-pady 0 \
	-width 2 \
        -text +
bind $_tabs_manual.jogf.jog.jogplus <Button-1> {
    if {[is_continuous]} { jog_plus }
}
bind $_tabs_manual.jogf.jog.jogplus <ButtonRelease-1> {
    if {[is_continuous]} { jog_stop }
}

combobox $_tabs_manual.jogf.jog.jogincr \
	-editable 0 \
	-textvariable jogincrement \
	-value [_ Continuous] \
	-width 10
$_tabs_manual.jogf.jog.jogincr list insert end [_ Continuous] 0.1000 0.0100 0.0010 0.0001

frame $_tabs_manual.jogf.zerohome

button $_tabs_manual.jogf.zerohome.home \
	-command home_joint \
	-padx 2m \
	-pady 0
setup_widget_accel $_tabs_manual.jogf.zerohome.home [_ "Home Axis"]

button $_tabs_manual.jogf.zerohome.zero \
	-command touch_off_system \
	-padx 2m \
	-pady 0
setup_widget_accel $_tabs_manual.jogf.zerohome.zero [_ "Touch Off"]

button $_tabs_manual.jogf.zerohome.tooltouch \
	-command touch_off_tool \
	-padx 2m \
	-pady 0
setup_widget_accel $_tabs_manual.jogf.zerohome.tooltouch [_ "Tool Touch Off"]


checkbutton $_tabs_manual.jogf.override \
	-command toggle_override_limits \
	-variable override_limits
setup_widget_accel $_tabs_manual.jogf.override [_ "Override Limits"]

grid $_tabs_manual.jogf.zerohome \
	-column 0 \
	-row 1 \
	-columnspan 3 \
	-sticky w

grid $_tabs_manual.jogf.jog \
	-column 0 \
	-row 0 \
	-columnspan 3 \
	-sticky w

# Grid widget $_tabs_manual.jogf.zerohome.home
grid $_tabs_manual.jogf.zerohome.home \
	-column 0 \
	-row 0 \
	-ipadx 2 \
	-pady 2 \
	-sticky w

# Grid widget $_tabs_manual.jogf.zerohome.zero
grid $_tabs_manual.jogf.zerohome.zero \
	-column 1 \
	-row 0 \
	-ipadx 2 \
	-pady 2 \
	-sticky w

# Grid widget $_tabs_manual.jogf.zerohome.tooltouch
grid $_tabs_manual.jogf.zerohome.tooltouch \
	-column 1 \
	-row 2 \
	-ipadx 2 \
	-pady 2 \
	-sticky w

# Grid widget $_tabs_manual.jogf.override
grid $_tabs_manual.jogf.override \
	-column 0 \
	-row 3 \
	-columnspan 3 \
	-pady 2 \
	-sticky w

# Grid widget $_tabs_manual.jogf.jog.jogminus
grid $_tabs_manual.jogf.jog.jogminus \
	-column 0 \
	-row 0 \
	-pady 2 \
	-sticky nsw

# Grid widget $_tabs_manual.jogf.jog.jogplus
grid $_tabs_manual.jogf.jog.jogplus \
	-column 1 \
	-row 0 \
	-pady 2 \
	-sticky nsw

# Grid widget $_tabs_manual.jogf.jog.jogincr
grid $_tabs_manual.jogf.jog.jogincr \
	-column 2 \
	-row 0 \
	-pady 2 \
        -sticky nsw

vspace $_tabs_manual.space1 \
	-height 12

label $_tabs_manual.spindlel
setup_widget_accel $_tabs_manual.spindlel [_ Spindle:]

frame $_tabs_manual.spindlef
frame $_tabs_manual.spindlef.row1
frame $_tabs_manual.spindlef.row2

radiobutton $_tabs_manual.spindlef.ccw \
	-borderwidth 2 \
	-command spindle \
	-image [load_image spindle_ccw] \
	-indicatoron 0 \
	-selectcolor [systembuttonface] \
	-value -1 \
	-variable spindledir
setup_widget_accel $_tabs_manual.spindlef.ccw {}

radiobutton $_tabs_manual.spindlef.stop \
	-borderwidth 2 \
	-command spindle \
	-indicatoron 0 \
	-selectcolor [systembuttonface] \
	-value 0 \
	-variable spindledir
setup_widget_accel $_tabs_manual.spindlef.stop [_ Stop]

radiobutton $_tabs_manual.spindlef.cw \
	-borderwidth 2 \
	-command spindle \
	-image [load_image spindle_cw] \
	-indicatoron 0 \
	-selectcolor [systembuttonface] \
	-value 1 \
	-variable spindledir
setup_widget_accel $_tabs_manual.spindlef.cw {}

button $_tabs_manual.spindlef.spindleminus \
	-padx 0 \
	-pady 0 \
	-width 2
bind $_tabs_manual.spindlef.spindleminus <Button-1> {
	if {[%W cget -state] == "disabled"} { continue }
	spindle_decrease
}
bind $_tabs_manual.spindlef.spindleminus <ButtonRelease-1> {
	if {[%W cget -state] == "disabled"} { continue }
	spindle_constant
}
setup_widget_accel $_tabs_manual.spindlef.spindleminus [_ -]

button $_tabs_manual.spindlef.spindleplus \
	-padx 0 \
	-pady 0 \
	-width 2
bind $_tabs_manual.spindlef.spindleplus <Button-1> {
	if {[%W cget -state] == "disabled"} { continue }
	spindle_increase
}
bind $_tabs_manual.spindlef.spindleplus <ButtonRelease-1> {
	if {[%W cget -state] == "disabled"} { continue }
	spindle_constant
}
setup_widget_accel $_tabs_manual.spindlef.spindleplus [_ +]

checkbutton $_tabs_manual.spindlef.brake \
	-command brake \
	-variable brake
setup_widget_accel $_tabs_manual.spindlef.brake [_ Brake]

# Grid widget $_tabs_manual.spindlef.brake
grid $_tabs_manual.spindlef.brake \
	-column 0 \
	-row 3 \
	-pady 2 \
	-sticky w

grid $_tabs_manual.spindlef.row1 -row 1 -column 0 -sticky nw
grid $_tabs_manual.spindlef.row2 -row 2 -column 0 -sticky nw

# Grid widget $_tabs_manual.spindlef.ccw
pack $_tabs_manual.spindlef.ccw  \
        -in $_tabs_manual.spindlef.row1 \
        -side left \
        -pady 2

# Grid widget $_tabs_manual.spindlef.stop
pack $_tabs_manual.spindlef.stop \
        -in $_tabs_manual.spindlef.row1 \
        -side left \
        -pady 2 \
        -ipadx 8

# Grid widget $_tabs_manual.spindlef.cw
pack $_tabs_manual.spindlef.cw \
        -in $_tabs_manual.spindlef.row1 \
        -side left \
        -pady 2

# Grid widget $_tabs_manual.spindlef.spindleminus
pack $_tabs_manual.spindlef.spindleminus \
        -in $_tabs_manual.spindlef.row2 \
        -side left \
        -pady 2

# Grid widget $_tabs_manual.spindlef.spindleplus
pack $_tabs_manual.spindlef.spindleplus \
        -in $_tabs_manual.spindlef.row2 \
        -side left \
        -pady 2

vspace $_tabs_manual.space2 \
	-height 12

label $_tabs_manual.coolant
setup_widget_accel $_tabs_manual.coolant [_ Coolant:]

checkbutton $_tabs_manual.mist \
	-command mist \
	-variable mist
setup_widget_accel $_tabs_manual.mist [_ Mist]

checkbutton $_tabs_manual.flood \
	-command flood \
	-variable flood
setup_widget_accel $_tabs_manual.flood [_ Flood]

grid rowconfigure $_tabs_manual 99 -weight 1
grid columnconfigure $_tabs_manual 99 -weight 1
# Grid widget $_tabs_manual.axes
grid $_tabs_manual.axes \
	-column 1 \
	-row 0 \
	-padx 0 \
	-sticky w

# Grid widget $_tabs_manual.axis
grid $_tabs_manual.axis \
	-column 0 \
	-row 0 \
	-pady 1 \
	-sticky nw

# Grid widget $_tabs_manual.coolant
grid $_tabs_manual.coolant \
	-column 0 \
	-row 5 \
	-sticky w

# Grid widget $_tabs_manual.flood
grid $_tabs_manual.flood \
	-column 1 \
	-row 6 \
	-columnspan 2 \
	-padx 4 \
	-sticky w

# Grid widget $_tabs_manual.jogf
grid $_tabs_manual.jogf \
	-column 1 \
	-row 1 \
	-padx 4 \
	-sticky w

# Grid widget $_tabs_manual.mist
grid $_tabs_manual.mist \
	-column 1 \
	-row 5 \
	-columnspan 2 \
	-padx 4 \
	-sticky w

# Grid widget $_tabs_manual.space1
grid $_tabs_manual.space1 \
	-column 0 \
	-row 2

# Grid widget $_tabs_manual.space2
grid $_tabs_manual.space2 \
	-column 0 \
	-row 4

# Grid widget $_tabs_manual.spindlef
grid $_tabs_manual.spindlef \
	-column 1 \
	-row 3 \
	-padx 4 \
	-sticky w

# Grid widget $_tabs_manual.spindlel
grid $_tabs_manual.spindlel \
	-column 0 \
	-row 3 \
	-pady 2 \
	-sticky nw

label $_tabs_mdi.historyl
setup_widget_accel $_tabs_mdi.historyl [_ History:]

# MDI-history listbox
listbox $_tabs_mdi.history \
    -width 40 \
    -height 8 \
    -exportselection 0 \
    -selectmode extended \
    -relief flat \
    -highlightthickness 0 \
    -takefocus 0 \
    -yscrollcommand "$_tabs_mdi.history.sby set"
# always have an empty element at the end
$_tabs_mdi.history insert end ""

scrollbar $_tabs_mdi.history.sby -borderwidth 0  -command "$_tabs_mdi.history yview"
pack $_tabs_mdi.history.sby -side right -fill y
grid rowconfigure $_tabs_mdi.history 0 -weight 1

vspace $_tabs_mdi.vs1 \
	-height 12

label $_tabs_mdi.commandl
setup_widget_accel $_tabs_mdi.commandl [_ "MDI Command:"]

entry $_tabs_mdi.command \
	-textvariable mdi_command

button $_tabs_mdi.go \
	-command send_mdi \
	-padx 1m \
	-pady 0
setup_widget_accel $_tabs_mdi.go [_ Go]

vspace $_tabs_mdi.vs2 \
	-height 12

label $_tabs_mdi.gcodel
setup_widget_accel $_tabs_mdi.gcodel [_ "Active G-Codes:"]

text $_tabs_mdi.gcodes \
	-height 2 \
	-width 40 \
	-undo 0 \
	-wrap word

$_tabs_mdi.gcodes insert end {}
$_tabs_mdi.gcodes configure -state disabled

vspace $_tabs_mdi.vs3 \
	-height 12

# Grid widget $_tabs_mdi.command
grid $_tabs_mdi.command \
	-column 0 \
	-row 4 \
	-sticky ew

# Grid widget $_tabs_mdi.commandl
grid $_tabs_mdi.commandl \
	-column 0 \
	-row 3 \
	-sticky w

# Grid widget $_tabs_mdi.gcodel
grid $_tabs_mdi.gcodel \
	-column 0 \
	-row 6 \
	-sticky w

# Grid widget $_tabs_mdi.gcodes
grid $_tabs_mdi.gcodes \
	-column 0 \
	-row 7 \
	-columnspan 2 \
	-sticky new

# Grid widget $_tabs_mdi.go
grid $_tabs_mdi.go \
	-column 1 \
	-row 4

# Grid widget $_tabs_mdi.history
grid $_tabs_mdi.history \
	-column 0 \
	-row 1 \
	-columnspan 2 \
	-sticky nesw

# Grid widget $_tabs_mdi.historyl
grid $_tabs_mdi.historyl \
	-column 0 \
	-row 0 \
	-sticky w

# Grid widget $_tabs_mdi.vs1
grid $_tabs_mdi.vs1 \
	-column 0 \
	-row 2

# Grid widget $_tabs_mdi.vs2
grid $_tabs_mdi.vs2 \
	-column 0 \
	-row 5

# Grid widget $_tabs_mdi.vs3
grid $_tabs_mdi.vs3 \
	-column 0 \
	-row 8
grid columnconfigure $_tabs_mdi 0 -weight 1
grid rowconfigure $_tabs_mdi 1 -weight 1

NoteBook ${pane_top}.right \
        -borderwidth 2 \
        -arcradius 3
after 1 after idle show_all_tabs ${pane_top}.right

set _tabs_preview [${pane_top}.right insert end preview -text [_ "Preview"]]
set _tabs_numbers [${pane_top}.right insert end numbers -text [_ "DRO"]]
$_tabs_preview configure -borderwidth 1
$_tabs_numbers configure -borderwidth 1

text ${_tabs_numbers}.text -width 1 -height 1 -wrap none \
	-background [systembuttonface] \
	-borderwidth 0 \
	-undo 0 \
	-relief flat
pack ${_tabs_numbers}.text -fill both -expand 1
bindtags ${_tabs_numbers}.text [list ${_tabs_numbers}.text . all]

frame .info

label .info.task_state \
	-anchor w \
	-borderwidth 2 \
	-relief sunken \
	-textvariable task_state_string \
	-width 14
setup_widget_accel .info.task_state {}

label .info.tool \
	-anchor w \
	-borderwidth 2 \
	-relief sunken \
	-textvariable ::tool \
	-width 30

label .info.position \
	-anchor w \
	-borderwidth 2 \
	-relief sunken \
	-textvariable ::position \
	-width 25

# Pack widget .info.task_state
pack .info.task_state \
	-side left

# Pack widget .info.tool
pack .info.tool \
	-side left \
	-fill x -expand 1

# Pack widget .info.position
pack .info.position \
	-side left

frame ${pane_bottom}.t \
	-borderwidth 2 \
	-relief sunken \
	-highlightthickness 1

text ${pane_bottom}.t.text \
	-borderwidth 0 \
	-exportselection 0 \
	-height 9 \
	-highlightthickness 0 \
	-relief flat \
	-takefocus 0 \
	-undo 0 \
	-yscrollcommand [list ${pane_bottom}.t.sb set]
${pane_bottom}.t.text insert end {}
bind ${pane_bottom}.t.text <Configure> { goto_sensible_line }

scrollbar ${pane_bottom}.t.sb \
	-borderwidth 0 \
	-command [list ${pane_bottom}.t.text yview] \
	-highlightthickness 0

# Pack widget ${pane_bottom}.t.text
pack ${pane_bottom}.t.text \
	-expand 1 \
	-fill both \
	-side left

# Pack widget ${pane_bottom}.t.sb
pack ${pane_bottom}.t.sb \
	-fill y \
	-side left

frame ${pane_top}.ajogspeed
label ${pane_top}.ajogspeed.l0 -text [_ "Jog Speed:"]
label ${pane_top}.ajogspeed.l1
scale ${pane_top}.ajogspeed.s -bigincrement 0 -from .06 -to 1 -resolution .020 -showvalue 0 -variable ajog_slider_val -command update_ajog_slider_vel -orient h -takefocus 0
label ${pane_top}.ajogspeed.l -textv jog_aspeed -width 6 -anchor e
pack ${pane_top}.ajogspeed.l0 -side left
pack ${pane_top}.ajogspeed.l -side left
pack ${pane_top}.ajogspeed.l1 -side left
pack ${pane_top}.ajogspeed.s -side right
bind . <less> [regsub %W [bind Scale <Left>] ${pane_top}.ajogspeed.s]
bind . <greater> [regsub %W [bind Scale <Right>] ${pane_top}.ajogspeed.s]


frame ${pane_top}.jogspeed
label ${pane_top}.jogspeed.l0 -text [_ "Jog Speed:"]
label ${pane_top}.jogspeed.l1
scale ${pane_top}.jogspeed.s -bigincrement 0 -from .06 -to 1 -resolution .020 -showvalue 0 -variable jog_slider_val -command update_jog_slider_vel -orient h -takefocus 0
label ${pane_top}.jogspeed.l -textv jog_speed -width 6 -anchor e
pack ${pane_top}.jogspeed.l0 -side left
pack ${pane_top}.jogspeed.l -side left
pack ${pane_top}.jogspeed.l1 -side left
pack ${pane_top}.jogspeed.s -side right
bind . , [regsub %W [bind Scale <Left>] ${pane_top}.jogspeed.s]
bind . . [regsub %W [bind Scale <Right>] ${pane_top}.jogspeed.s]

frame ${pane_top}.maxvel
label ${pane_top}.maxvel.l0 -text [_ "Max Velocity:"]
label ${pane_top}.maxvel.l1
scale ${pane_top}.maxvel.s -bigincrement 0 -from .06 -to 1 -resolution .020 -showvalue 0 -variable maxvel_slider_val -command update_maxvel_slider_vel -orient h -takefocus 0
label ${pane_top}.maxvel.l -textv maxvel_speed -width 6 -anchor e
pack ${pane_top}.maxvel.l0 -side left
pack ${pane_top}.maxvel.l -side left
pack ${pane_top}.maxvel.l1 -side left
pack ${pane_top}.maxvel.s -side right
bind . <semicolon> [regsub %W [bind Scale <Left>] ${pane_top}.maxvel.s]
bind . ' [regsub %W [bind Scale <Right>] ${pane_top}.maxvel.s]

frame ${pane_top}.spinoverride

label ${pane_top}.spinoverride.foentry \
	-textvariable spindlerate \
	-width 3 \
        -anchor e
setup_widget_accel ${pane_top}.spinoverride.foentry 0

scale ${pane_top}.spinoverride.foscale \
	-command set_spindlerate \
	-orient horizontal \
	-resolution 1.0 \
	-showvalue 0 \
	-takefocus 0 \
	-to 120.0 \
	-variable spindlerate

label ${pane_top}.spinoverride.l
setup_widget_accel ${pane_top}.spinoverride.l [_ "Spindle Override:"]
label ${pane_top}.spinoverride.m -width 1
setup_widget_accel ${pane_top}.spinoverride.m [_ "%"]

# Pack widget ${pane_top}.spinoverride.l
pack ${pane_top}.spinoverride.l \
	-side left

# Pack widget ${pane_top}.spinoverride.foscale
pack ${pane_top}.spinoverride.foscale \
	-side right

# Pack widget ${pane_top}.spinoverride.foentry
pack ${pane_top}.spinoverride.m \
	-side right

# Pack widget ${pane_top}.spinoverride.foentry
pack ${pane_top}.spinoverride.foentry \
	-side right



frame ${pane_top}.feedoverride

label ${pane_top}.feedoverride.foentry \
	-textvariable feedrate \
	-width 4 \
        -anchor e
setup_widget_accel ${pane_top}.feedoverride.foentry 0

scale ${pane_top}.feedoverride.foscale \
	-command set_feedrate \
	-orient horizontal \
	-resolution 1.0 \
	-showvalue 0 \
	-takefocus 0 \
	-to 120.0 \
	-variable feedrate

label ${pane_top}.feedoverride.l
setup_widget_accel ${pane_top}.feedoverride.l [_ "Feed Override:"]
label ${pane_top}.feedoverride.m -width 1
setup_widget_accel ${pane_top}.feedoverride.m [_ "%"]

# Pack widget ${pane_top}.feedoverride.l
pack ${pane_top}.feedoverride.l \
	-side left

# Pack widget ${pane_top}.feedoverride.foscale
pack ${pane_top}.feedoverride.foscale \
	-side right

# Pack widget ${pane_top}.feedoverride.foentry
pack ${pane_top}.feedoverride.m \
	-side right

# Pack widget ${pane_top}.feedoverride.foentry
pack ${pane_top}.feedoverride.foentry \
	-side right

frame ${pane_top}.rapidoverride

label ${pane_top}.rapidoverride.foentry \
	-textvariable rapidrate \
	-width 4 \
        -anchor e
setup_widget_accel ${pane_top}.rapidoverride.foentry 0

scale ${pane_top}.rapidoverride.foscale \
	-command set_rapidrate \
	-orient horizontal \
	-resolution 1.0 \
	-showvalue 0 \
	-takefocus 0 \
	-to 120.0 \
	-variable rapidrate

label ${pane_top}.rapidoverride.l
setup_widget_accel ${pane_top}.rapidoverride.l [_ "Rapid Override:"]
label ${pane_top}.rapidoverride.m -width 1
setup_widget_accel ${pane_top}.rapidoverride.m [_ "%"]

# Pack widget ${pane_top}.rapidoverride.l
pack ${pane_top}.rapidoverride.l \
	-side left

# Pack widget ${pane_top}.rapidoverride.foscale
pack ${pane_top}.rapidoverride.foscale \
	-side right

# Pack widget ${pane_top}.rapidoverride.foentry
pack ${pane_top}.rapidoverride.m \
	-side right

# Pack widget ${pane_top}.rapidoverride.foentry
pack ${pane_top}.rapidoverride.foentry \
	-side right

toplevel .about
bind .about <Key-Return> { wm wi .about }
bind .about <Key-Escape> { wm wi .about }

text .about.message \
	-background [systembuttonface] \
	-borderwidth 0 \
	-relief flat \
	-width 40 \
	-height 11 \
	-wrap word \
	-cursor {}

.about.message tag configure link \
	-underline 1 -foreground blue
.about.message tag bind link <Leave> {
	.about.message configure -cursor {}
	.about.message tag configure link -foreground blue}
.about.message tag bind link <Enter> {
	.about.message configure -cursor hand2
	.about.message tag configure link -foreground red}
.about.message tag bind link <ButtonPress-1><ButtonRelease-1> {launch_website}
.about.message insert end [subst [_ "LinuxCNC/AXIS version \$version\n\nCopyright (C) 2004, 2005, 2006, 2007, 2008, 2009, 2010, 2011, 2012, 2013, 2014, 2015, 2016 Jeff Epler and Chris Radek.\n\nThis is free software, and you are welcome to redistribute it under certain conditions.  See the file COPYING, included with LinuxCNC.\n\nVisit the LinuxCNC web site: "]] {} {http://www.linuxcnc.org/} link
.about.message configure -state disabled

button .about.ok \
	-command {wm wi .about} \
	-default active \
	-padx 0 \
	-pady 0 \
	-width 10
setup_widget_accel .about.ok [_ OK]

label .about.image \
	-borderwidth 0 \
	-image [load_image banner]
setup_widget_accel .about.image {}

# Pack widget .about.image
pack .about.image

# Pack widget .about.message
pack .about.message \
	-expand 1 \
	-fill both

# Pack widget .about.ok
pack .about.ok

# Configure widget .about
wm title .about [_ "About AXIS"]
wm iconname .about {}
wm resiz .about 0 0
wm minsize .about 1 1
wm protocol .about WM_DELETE_WINDOW {wm wi .about}

toplevel .keys
bind .keys <Key-Return> { wm withdraw .keys }
bind .keys <Key-Escape> { wm withdraw .keys }

frame .keys.text \

button .keys.ok \
	-command {wm wi .keys} \
	-default active \
	-padx 0 \
	-pady 0 \
	-width 10
setup_widget_accel .keys.ok [_ OK]

# Pack widget .keys.text
pack .keys.text \
	-expand 1 \
	-fill y

# Pack widget .keys.ok
pack .keys.ok

# Configure widget .keys
wm title .keys [_ "AXIS Quick Reference"]
wm iconname .keys {}
wm resiz .keys 0 0
wm minsize .keys 1 1
wm protocol .keys WM_DELETE_WINDOW {wm wi .keys}

# Grid widget ${pane_top}.feedoverride
grid ${pane_top}.feedoverride \
	-column 0 \
	-row 2 \
	-sticky new

# Grid widget ${pane_top}.rapidoverride
grid ${pane_top}.rapidoverride \
	-column 0 \
	-row 3 \
	-sticky new

# Grid widget ${pane_top}.spinoverride
grid ${pane_top}.spinoverride \
	-column 0 \
	-row 4 \
	-sticky new

grid ${pane_top}.jogspeed \
	-column 0 \
	-row 5 \
	-sticky new

grid ${pane_top}.ajogspeed \
	-column 0 \
	-row 6 \
	-sticky new

grid ${pane_top}.maxvel \
	-column 0 \
	-row 7 \
	-sticky new

# Grid widget .info
grid .info \
	-column 0 \
	-row 6 \
	-columnspan 2 \
	-sticky ew

# Grid widget ${pane_top}.right
grid ${pane_top}.right \
	-column 1 \
	-row 1 \
	-columnspan 2 \
	-padx 2 \
	-pady 2 \
	-rowspan 99 \
	-sticky nesw

grid ${pane_top}.tabs \
	-column 0 \
	-row 1 \
	-sticky nesw \
	-padx 2 \
	-pady 2
grid rowconfigure ${pane_top} 1 -weight 1
grid columnconfigure ${pane_top} 1 -weight 1
grid ${pane_bottom}.t \
	-column 1 \
	-row 1 \
	-sticky nesw
grid rowconfigure ${pane_bottom} 1 -weight 1
grid columnconfigure ${pane_bottom} 1 -weight 1

grid .pane -column 0 -row 1 -sticky nsew -rowspan 2

# Grid widget .toolbar
grid .toolbar \
	-column 0 \
	-row 0 \
	-columnspan 3 \
	-sticky nesw
grid columnconfigure . 0 -weight 1
grid rowconfigure . 1 -weight 1

# vim:ts=8:sts=8:noet:sw=8

set manualgroup [concat [winfo children $_tabs_manual.axes] \
    $_tabs_manual.jogf.zerohome.home \
    $_tabs_manual.jogf.jog.jogminus \
    $_tabs_manual.jogf.jog.jogplus \
    $_tabs_manual.spindlef.cw $_tabs_manual.spindlef.ccw \
    $_tabs_manual.spindlef.stop $_tabs_manual.spindlef.brake \
    $_tabs_manual.flood $_tabs_manual.mist]

set mdigroup [concat $_tabs_mdi.command $_tabs_mdi.go $_tabs_mdi.history]

proc disable_group {ws} { foreach w $ws { $w configure -state disabled } }
proc enable_group {ws} { foreach w $ws { $w configure -state normal } }

proc state {e args} {
    set e [uplevel \#0 [list expr $e]]
    if {$e} { set newstate normal } else {set newstate disabled}
    foreach w $args {
        if {[llength $w] > 1} {
            set m [lindex $w 0]
            for {set i 1} {$i < [llength $w]} {incr i} {
                set idx [extract_text [_ [lindex $w $i]]]
                set oldstate [$m entrycget $idx -state]
                if {$oldstate != $newstate} {
                    $m entryconfigure $idx -state $newstate
                }
            }
        } else {
            set oldstate [$w cget -state]
            if {$oldstate != $newstate} {
                $w configure -state $newstate
            }
        }
    }
}
proc relief {e args} {
    set e [uplevel \#0 [list expr $e]]
    if {$e} { set newstate sunken } else {set newstate link }
    foreach w $args { $w configure -relief $newstate }
}

proc update_title {args} {
    set basetitle [subst [_ "AXIS \$::version on \$::machine"]]
    if {$::taskfile == ""} {
        set nofile [_ "(no file)"]
        wm ti . "$basetitle $nofile"
        wm iconname . "AXIS"
    } else {
        wm ti . "[lindex [file split $::taskfile] end] - $basetitle"
        wm iconname . "[lindex [file split $::taskfile] end]"
    }
}

proc update_state {args} {
    # (The preferred exactly-two-argument form of switch cannot be used
    # as the patterns must undergo $-expansion)
    switch -- $::task_state \
        $::STATE_ESTOP { set ::task_state_string [_ "ESTOP"] } \
        $::STATE_ESTOP_RESET { set ::task_state_string [_ "OFF"] } \
        $::STATE_ON { set ::task_state_string [_ "ON"] }

    relief {$task_state == $STATE_ESTOP} .toolbar.machine_estop
    state  {$task_state != $STATE_ESTOP} \
        .toolbar.machine_power {.menu.machine "Toggle _Machine Power"}
    relief {$task_state == $STATE_ON}    .toolbar.machine_power 

    state  {$interp_state == $INTERP_IDLE && $taskfile != ""} \
        .toolbar.reload {.menu.file "_Reload"}
    state  {$taskfile != ""} \
        {.menu.file "_Save gcode as..."}
    state  {$interp_state == $INTERP_IDLE && $taskfile != "" && $::has_editor} \
        {.menu.file "_Edit..."}
    state  {$taskfile != ""} {.menu.file "_Properties..."}
    state  {$interp_state == $INTERP_IDLE} .toolbar.file_open \
        {.menu.file "_Open..." "_Quit" "Recent _Files"} \
        {.menu.machine "Skip lines with '_/'"} .toolbar.program_blockdelete
    state  {$task_state == $STATE_ON && $interp_state == $INTERP_IDLE } \
        .toolbar.program_run {.menu.machine "_Run program"} \
        {.menu.file "Reload tool ta_ble"}
    state  {$interp_state == $INTERP_IDLE} \
        {.menu.file "Edit _tool table..."}

    state  {$task_state == $STATE_ON && $interp_state == $INTERP_IDLE} \
        {.menu.machine "Homin_g" "_Unhoming" "_Zero coordinate system"}

    relief {$interp_state != $INTERP_IDLE} .toolbar.program_run
    state  {$task_state == $STATE_ON && $taskfile != ""} \
                .toolbar.program_step {.menu.machine "S_tep"}
    state  {$task_state == $STATE_ON && \
      ($interp_state == $INTERP_READING || $interp_state == $INTERP_WAITING) } \
                {.menu.machine "_Pause"}
    state  {$task_state == $STATE_ON && $interp_state == $INTERP_PAUSED } \
                {.menu.machine "Re_sume"}
    state  {$task_state == $STATE_ON && $interp_state != $INTERP_IDLE} \
                .toolbar.program_pause
    relief {$interp_pause != 0} \
                .toolbar.program_pause
    relief {$block_delete != 0} \
                .toolbar.program_blockdelete
    relief {$optional_stop != 0} \
                .toolbar.program_optpause
    state  {$task_state == $STATE_ON && $interp_state != $INTERP_IDLE} \
                .toolbar.program_stop {.menu.machine "Stop"}
    relief {$interp_state == $INTERP_IDLE} \
                .toolbar.program_stop
    state  {$::has_ladder} {.menu.file "_Ladder Editor..."}

    state {$task_state == $STATE_ON \
            && $interp_state == $INTERP_IDLE && $highlight_line != -1} \
                {.menu.machine "Ru_n from selected line"}

    state {$::task_state == $::STATE_ON && $::interp_state == $::INTERP_IDLE\
            && $spindledir != 0} \
                $::_tabs_manual.spindlef.spindleminus \
                $::_tabs_manual.spindlef.spindleplus

    if {   $::motion_mode     == $::TRAJ_MODE_FREE
        && $::kinematics_type != $::KINEMATICS_IDENTITY
       } {
        set ::position [concat [_ "Position:"] Joint]
    } else {
        set coord_str [lindex [list [_ Machine] [_ Relative]] $::coord_type]
        set display_str [lindex [list [_ Actual] [_ Commanded]] $::display_type]

        set ::position [concat [_ "Position:"] $coord_str $display_str]
    }

    if {$::task_state == $::STATE_ON && $::interp_state == $::INTERP_IDLE} {
        if {   ($::last_interp_state != $::INTERP_IDLE || $::last_task_state != $::task_state) \
            && $::task_mode == $::TASK_MODE_AUTO} {
            set_mode_from_tab
        }
        enable_group $::manualgroup
    } else {
        disable_group $::manualgroup
    }

    if {$::task_state == $::STATE_ON && $::queued_mdi_commands < $::max_queued_mdi_commands } {
        enable_group $::mdigroup
    } else {
        disable_group $::mdigroup
    }

    if {   $::task_state == $::STATE_ON
        && $::interp_state == $::INTERP_IDLE
        && (   $::motion_mode != $::TRAJ_MODE_FREE
            || $::kinematics_type == $::KINEMATICS_IDENTITY
           )} {
        $::_tabs_manual.jogf.zerohome.zero configure -state normal
    } else {
        $::_tabs_manual.jogf.zerohome.zero configure -state disabled
    }

    if {    $::task_state   == $::STATE_ON
         && $::interp_state == $::INTERP_IDLE
         && (   $::motion_mode != $::TRAJ_MODE_FREE
             || $::kinematics_type == $::KINEMATICS_IDENTITY
            )
         && ("$::tool" != "" && "$::tool" != [_ "No tool"])
       } {
        $::_tabs_manual.jogf.zerohome.tooltouch configure -state normal
    } else {
        $::_tabs_manual.jogf.zerohome.tooltouch configure -state disabled
    }


    set ::last_interp_state $::interp_state
    set ::last_task_state $::task_state

    if {$::on_any_limit} {
        $::_tabs_manual.jogf.override configure -state normal
    } else {
        $::_tabs_manual.jogf.override configure -state disabled
    }
}

proc set_mode_from_tab {} {
    set page [${::pane_top}.tabs raise]
    switch $page {
        mdi { ensure_mdi }
        default { ensure_manual
                  focus $::pane_top.tabs.fmanual
                }
    }

}

# trace var: motion_mode
proc joint_mode_switch {args} {
    # note: test for existence of ::trajcoordinates because it is not avail first timeo
    # todo: save and restore last value of ::ja_rbutton for joints,axes
    if {   $::motion_mode     == $::TRAJ_MODE_FREE
        && $::kinematics_type != $::KINEMATICS_IDENTITY
       } {
        grid forget $::_tabs_manual.axes
        grid $::_tabs_manual.joints -column 1 -row 0 -padx 0 -pady 0 -sticky w
        setup_widget_accel $::_tabs_manual.axis [_ Joint:]
        # Joints: need number for ja_rbutton
        if {[info exists ::trajcoordinates]} {
          # make first (leftmost) radiobutton active
          for {set i 0} {$i < $::MAX_AXIS} {incr i} {lappend anums $i}
          # typ anums is {0 ... 8}
          if { [lsearch $anums $::ja_rbutton] < 0 } {
            set ::ja_rbutton 0
          }
        }
    } else {
        grid forget $::_tabs_manual.joints
        grid $::_tabs_manual.axes -column 1 -row 0 -padx 0 -pady 0 -sticky w
        setup_widget_accel $::_tabs_manual.axis [_ Axis:]
        # Axes: need letter for ja_rbutton
        if {[info exists ::trajcoordinates]} {
          # make first (leftmost) radiobutton active
          for {set i 0} {$i < $::MAX_JOINTS} {incr i} {lappend jnums $i}
          # typ jnums is {0 ... 8}
          if { [lsearch $jnums $::ja_rbutton] >= 0 } { 
            set ::ja_rbutton [string range $::trajcoordinates 0 0]
          }
        }
    }    
}

proc queue_update_state {args} { 
    after cancel update_state
    after idle update_state
}

set rotate_mode 0
set taskfile ""
set machine ""
set task_state -1
set has_editor 1
set has_ladder 0
set last_task_state 0
set task_mode -1
set task_paused 0
set optional_stop 0
set block_delete 0
set interp_pause 0
set last_interp_state 0
set interp_state 0
set running_line -1
set highlight_line -1
set coord_type 1
set display_type 0
set spindledir {}
set motion_mode 0
set kinematics_type -1
set metric 0
set max_speed 1
set queued_mdi_commands 0
set max_queued_mdi_commands 10
trace variable taskfile w update_title
trace variable machine w update_title
trace variable taskfile w queue_update_state
trace variable task_state w queue_update_state
trace variable task_mode w queue_update_state
trace variable task_paused w queue_update_state
trace variable optional_stop w queue_update_state
trace variable block_delete w queue_update_state
trace variable interp_pause w queue_update_state
trace variable interp_state w queue_update_state
trace variable running_line w queue_update_state
trace variable highlight_line w queue_update_state
trace variable spindledir w queue_update_state
trace variable coord_type w queue_update_state
trace variable display_type w queue_update_state
trace variable motion_mode w queue_update_state
trace variable kinematics_type w queue_update_state
trace variable on_any_limit w queue_update_state
trace variable motion_mode w joint_mode_switch
trace variable queued_mdi_commands  w queue_update_state

set editor_deleted 0

bind . <Control-Tab> {
    set page [${pane_top}.tabs raise]
    switch $page {
        mdi { ${pane_top}.tabs raise manual }
        default { ${pane_top}.tabs raise mdi }
    }
    break
}

# Handle Tk 8.6+ where virtual events are used for cursor motion
foreach {k v} {
    Left    PrevChar        Right   NextChar
    Up      PrevLine        Down    NextLine
    Home    LineStart       End     LineEnd
} {
    set b [bind Entry <<$v>>]
    if {$b != {}} { bind Entry <$k> $b }
}

# any key that causes an entry or spinbox action should not continue to perform
# a binding on "."
foreach c {Entry Spinbox} {
        foreach b [bind $c] {
            switch -glob $b {
                <*-Key-*> {
                    bind $c $b {+if {[%W cget -state] == "normal"} break}
                }
            }
    }

    foreach b { Left Right
            Up Down Prior Next Home
            End } {
        bind $c <KeyPress-$b> {+if {[%W cget -state] == "normal"} break}
        bind $c <KeyRelease-$b> {+if {[%W cget -state] == "normal"} break}
    }
    bind $c <Control-KeyPress-Home> {+if {[%W cget -state] == "normal"} break}
    bind $c <Control-KeyRelease-Home> {+if {[%W cget -state] == "normal"} \
                                                                        break}
    bind $c <Control-KeyPress-KP_Home> {+if {[%W cget -state] == "normal"} \
                                                                        break}
    bind $c <Control-KeyRelease-KP_Home> {+if {[%W cget -state] == "normal"} \
                                                                        break}
    set bb [bind $c <KeyPress>]
    foreach k { Left Right Up Down Prior Next
                Home End } {
        set b [bind $c <$k>]
        if {$b == {}} { set b $bb }
        bind $c <KeyPress-KP_$k> "if {%A == \"\"} { $b } { $bb; break }"
        bind $c <KeyRelease-KP_$k> {+if {[%W cget -state] == "normal"} break}
    }

    foreach k {0 1 2 3 4 5 6 7 8 9} {
        bind $c <KeyPress-KP_$k> "$bb; break"
        bind $c <KeyRelease-KP_$k> {+if {[%W cget -state] == "normal"} break}
    }

    bind $c <Key> {+if {[%W cget -state] == "normal" && [string length %A]} \
                                                                        break}
}

proc is_continuous {} {
    expr {"[$::_tabs_manual.jogf.jog.jogincr get]" == [_ "Continuous"]}
}

proc show_all text {
    $text yview moveto 0.0
    update
    set fy [lindex [$text yview] 1]
    set ch [$text cget -height]
    $text configure -height [expr {ceil($ch/$fy)}]
}

proc delete_all text {
    set nl [lindex [split [$text index end] .] 0]
    while {$nl >= 1500} {
      $text delete 1.0 1000.end
      incr nl -1000
    }

    $text delete 1.0 end
}

proc size_combobox_to_entries c {
    set fo [$c cget -font]
    set wi [font measure $fo 0]
    set sz 4
    foreach i [$c list get 0 end] {
        set li [expr ([font measure $fo $i] + $wi - 1)/$wi]
        if {$li > $sz} { set sz $li }
    }
    $c configure -width $sz
}

proc size_label_to_strings {w args} {
    set fo [$w cget -font]
    set wi [font measure $fo 0]
    set sz 4
    foreach i args {
        set li [expr ([font measure $fo $i] + $wi - 1)/$wi]
        if {$li > $sz} { set sz $li }
    }
    $w configure -width $sz
}

proc size_menubutton_to_entries {w} {
    set m $w.menu
    set fo [$w cget -font]
    set wi [font measure $fo 0]
    set sz 4
    for {set i 0} {$i <= [$m index end]} {incr i} {
        set type [$m type $i]
        if {$type == "separator" || $type == "tearoff"} continue
        set text [$m entrycget $i -label]
        set li [expr ([font measure $fo $text] + $wi - 1)/$wi]
        if {$li > $sz} { set sz $li }
    }
    $w configure -width $sz
}

size_combobox_to_entries $_tabs_manual.jogf.jog.jogincr
size_label_to_strings $_tabs_manual.axis [_ Joint:] [_ Axis:]

proc setval {vel max_speed} {
    if {$vel == 0} { return 0 }
    if {$vel >= 60*$max_speed} { set vel [expr 60*$max_speed] }
    set x [expr {-1/(log($vel/60./$max_speed)-1)}]
    expr {round($x * 200.) / 200.}
}

proc val2vel {val max_speed} {
    if {$val == 0} { return 0 }
    if {$val == 1} { format "%32.5f" [expr {$max_speed * 60.}]
    } else { format "%32.5f" [expr {60 * $max_speed * exp(-1/$val + 1)}] }
}

proc places {s1 s2} {
    if {$s1 > 1 && int($s1) != int($s2)} {
        return [expr {[string first . $s1]-1}]
    }
    set l1 [string length $s1]
    set l2 [string length $s2]
    for {set i 15} {$i < $l1 && $i < $l2} {incr i} {
        set c1 [string index $s1 $i]
        set c2 [string index $s2 $i]
        if {$c1 != "0" && $c1 != "." && $c1 != $c2} { return $i } 
    }
    return [string length $s1]
}

proc val2vel_show {val maxvel} {
    set this_vel [val2vel $val $maxvel]
    set next_places 0
    set last_places 0
    if {$val > .005} {
        set next_vel [val2vel [expr {$val - .005}] $maxvel]
        set next_places [places $this_vel $next_vel]
    }
    if {$val < .995} {
        set prev_vel [val2vel [expr {$val + .005}] $maxvel]
        set prev_places [places $this_vel $prev_vel]
    }
    if {$next_places > $last_places} {
        string trim [string range $this_vel 0 $next_places]
    } {
        string trim [string range $this_vel 0 $last_places]
    }
}

proc set_slider_min {minval} {
    global pane_top
    global max_speed
    ${pane_top}.jogspeed.s configure -from [setval $minval $max_speed]
}

proc set_aslider_min {minval} {
    global pane_top
    global max_aspeed
    ${pane_top}.ajogspeed.s configure -from [setval $minval $max_aspeed]
}

proc update_jog_slider_vel {newval} {
    global jog_speed max_speed
    set max_speed_units [to_internal_linear_unit $max_speed]
    if {$max_speed_units == {None}} return
    if {$::metric} { set max_speed_units [expr {25.4 * $max_speed_units}] }
    set speed [val2vel_show $newval $max_speed_units];
    set jog_speed $speed
}

proc update_maxvel_slider_vel {newval} {
    global maxvel_speed max_maxvel
    set max_speed_units [to_internal_linear_unit $max_maxvel]
    if {$max_speed_units == {None}} return
    if {$::metric} { set max_speed_units [expr {25.4 * $max_speed_units}] }
    set speed [val2vel_show $newval $max_speed_units];
    set maxvel_speed $speed
    set_maxvel $speed
}

proc update_maxvel_slider {} {
    global maxvel_speed max_maxvel maxvel_slider_val
    set max_speed_units [to_internal_linear_unit $max_maxvel]
    if {$max_speed_units == {None}} return
    if {$::metric} { set max_speed_units [expr {25.4 * $max_speed_units}] }
    set maxvel_slider_val [setval $maxvel_speed $max_speed_units]
}

proc update_units {args} {
    if {$::metric} {
        ${::pane_top}.jogspeed.l1 configure -text mm/min
        ${::pane_top}.maxvel.l1 configure -text mm/min
    } else {
        ${::pane_top}.jogspeed.l1 configure -text in/min
        ${::pane_top}.maxvel.l1 configure -text in/min
    }
    update_jog_slider_vel $::jog_slider_val
    update_maxvel_slider_vel $::maxvel_slider_val
}

proc update_ajog_slider_vel {newval} {
    global jog_aspeed max_aspeed
    set jog_aspeed [val2vel_show $newval $max_aspeed];
}

proc update_recent {args} {
    .menu.file.recent delete 0 end
    set i 1
    foreach f $args {
        if {$i < 10} { set und 0 } \
        elseif {$i == 10} { set und 1 } \
        else { set und -1 }
        .menu.file.recent add command -underline $und \
            -label "$i: [file tail $f]" \
            -command [list open_file_name $f]
        incr i
    }
}


bind . <Configure> {
    if {"%W" == "."} {
        set msz [wm minsize %W]
        set nmsz [list [winfo reqwidth %W] [expr [winfo reqheight %W]+4]]
        if {$msz != $nmsz} { eval wm minsize %W $nmsz }
    }
}

bind . <Alt-v> [bind all <Alt-Key>]
bind . <Alt-v> {+break}

bind . <Key-Return> {focus .}

wm withdraw .about
wm withdraw .keys

DynamicHelp::add $_tabs_manual.spindlef.ccw -text [_ "Turn spindle counterclockwise \[F10\]"]
DynamicHelp::add $_tabs_manual.spindlef.cw -text [_ "Turn spindle clockwise \[F9\]"]
DynamicHelp::add $_tabs_manual.spindlef.stop -text [_ "Stop spindle \[F9/F10\]"]
DynamicHelp::add $_tabs_manual.spindlef.spindleplus -text [_ "Turn spindle Faster \[F12\]"]
DynamicHelp::add $_tabs_manual.spindlef.spindleminus -text [_ "Turn spindle Slower \[F11\]"]
DynamicHelp::add $_tabs_manual.spindlef.brake -text [_ "Turn spindle brake on \[Shift-B\] or off \[B\]"]
DynamicHelp::add $_tabs_manual.flood -text [_ "Turn flood on or off \[F8\]"]
DynamicHelp::add $_tabs_manual.mist -text [_ "Turn mist on or off \[F7\]"]
DynamicHelp::add $_tabs_manual.jogf.zerohome.home -text [_ "Send active axis home \[Home\]"]
DynamicHelp::add $_tabs_manual.jogf.zerohome.zero -text [_ "Set G54 offset for active axis \[End\]"]
DynamicHelp::add $_tabs_manual.jogf.zerohome.tooltouch -text [_ "Set tool offset for loaded tool \[Control-End\]"]
DynamicHelp::add $_tabs_manual.axes.axisx -text [_ "Activate axis \[X\]"]
DynamicHelp::add $_tabs_manual.axes.axisy -text [_ "Activate axis \[Y\]"]
DynamicHelp::add $_tabs_manual.axes.axisz -text [_ "Activate axis \[Z\]"]
DynamicHelp::add $_tabs_manual.axes.axisa -text [_ "Activate axis \[A\]"]
DynamicHelp::add $_tabs_manual.axes.axisb -text [_ "Activate axis \[4\]"]
DynamicHelp::add $_tabs_manual.axes.axisc -text [_ "Activate axis \[5\]"]
DynamicHelp::add $_tabs_manual.jogf.jog.jogminus -text [_ "Jog selected axis"]
DynamicHelp::add $_tabs_manual.jogf.jog.jogplus -text [_ "Jog selected axis"]
DynamicHelp::add $_tabs_manual.jogf.jog.jogincr -text [_ "Select jog increment"]
DynamicHelp::add $_tabs_manual.jogf.override -text [_ "Temporarily allow jogging outside machine limits \[L\]"]

# On at least some versions of Tk (tk8.4 on ubuntu 6.06), this hides files
# beginning with "." from the open dialog.  Who knows what it does on other
# versions.
catch {
    auto_load ::tk::dialog::file:: 
    namespace eval ::tk::dialog::file {}
    set ::tk::dialog::file::showHiddenBtn 1
    set ::tk::dialog::file::showHiddenVar 0
}

# Show what alphabetic letters are left for a specific menu
proc show_menu_available {m} {
    for {set i 0} {$i < [$m index end]} {incr i} {
        set t [$m type $i]
        if {$t == "separator" || $t == "tearoff"} {continue}
        set u [$m entrycget $i -underline]
        if {$u == -1} {continue}
        set l [$m entrycget $i -label]
        set c [string tolower [string range $l $u $u]]
        if {[info exists used($c)]} { puts "Duplicate: $c" }
        set used($c) {}
    }

    foreach i {a b c d e f g h i j k l m n o p q r s t u v w x y z} {
        if {![info exists used($i)]} { puts "Available: $i" }
    }
}

# vim:ts=8:sts=4:et:sw=4:
