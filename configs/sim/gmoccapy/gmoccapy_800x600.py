from gi.repository import Pango

# reduce size of v-buttons
self.widgets["vbtb_main"].set_size_request(78, -1)
self.widgets["lbl_time"].hide()
self.widgets["vbtb_main"].set_layout(Gtk.ButtonBoxStyle.EXPAND)
vbuttons = self.widgets["vbtb_main"].get_children()
for v in vbuttons:
    v.set_size_request(60, 56)

# change layout of h-button boxes to allow smaller spacing & resize buttons
hbuttonboxes = [ "hbtb_main", "hbtb_MDI", "hbtb_auto", "hbtb_ref",
    "hbtb_touch_off", "hbtb_setup", "hbtb_edit", "hbtb_tool",
    "hbtb_load_file", "hbtb_ref_joints" ]
for b in hbuttonboxes:
    self.widgets[b].set_layout(Gtk.ButtonBoxStyle.EXPAND)
    children = self.widgets[b].get_children()
    for c in children:
        c.set_size_request(60, 56)
        if type(c) is Gtk.Button:
            btn_child = c.get_children()[0]
            if type(btn_child) is Gtk.Label:
                # btn_child.set_ellipsize(Pango.EllipsizeMode.END)
                btn_child.set_line_wrap(True)
                btn_child.set_line_wrap_mode(Pango.WrapMode.WORD_CHAR)

# optimize space in jog box
self.widgets["vbtb_jog_incr"].hide()
self.widgets["vbx_jog"].set_size_request(-1, -1)

# second row for gremlin buttons
self.widgets["box_gremlin_buttons"].remove(self.widgets["tbtn_view_dimension"])
self.widgets["box_gremlin_buttons"].remove(self.widgets["tbtn_view_tool_path"])
self.widgets["box_gremlin_buttons"].remove(self.widgets["btn_delete_view"])
self.widgets["btn_zoom_out"].set_margin_end(0)
hbox_gremlin_buttons_2 = Gtk.Box(orientation=Gtk.Orientation.HORIZONTAL)
hbox_gremlin_buttons_2.show()
hbox_gremlin_buttons_2.pack_end(self.widgets["btn_delete_view"], False, False, 0)
hbox_gremlin_buttons_2.pack_end(self.widgets["tbtn_view_tool_path"], False, False, 0)
hbox_gremlin_buttons_2.pack_end(self.widgets["tbtn_view_dimension"], False, False, 0)
self.widgets["vbox15"].pack_end(hbox_gremlin_buttons_2, False, False, 3)

# reorder settings page
self.widgets["vbox_window"].remove(self.widgets["frm_ntb_preview"]) # "On Touch off"
self.widgets["vbox_window"].remove(self.widgets["frm_keyboard"])
self.widgets["vbox_dro_settings"].remove(self.widgets["frm_preview"])
self.widgets["vbox_file"].remove(self.widgets["frm_message_position"])
self.widgets["vbox_file"].remove(self.widgets["frm_themes"])
self.widgets["vbox21"].remove(self.widgets["frm_unlock"])
self.widgets["vbox_window"].pack_start(self.widgets["frm_preview"], False, False, 3)
self.widgets["vbox_dro_settings"].pack_start(self.widgets["frm_ntb_preview"], False, False, 3)
vbox_c1 = Gtk.Box(orientation=Gtk.Orientation.VERTICAL)
vbox_c2 = Gtk.Box(orientation=Gtk.Orientation.VERTICAL)
hbox_setup_appearance_2 = Gtk.Box(orientation=Gtk.Orientation.HORIZONTAL)
hbox_setup_appearance_2.pack_start(vbox_c1, False, False, 3)
hbox_setup_appearance_2.pack_start(vbox_c2, False, False, 3)
hbox_setup_appearance_2.show()
vbox_c1.pack_start(self.widgets["frm_keyboard"], False, False, 3)
vbox_c1.pack_start(self.widgets["frm_message_position"], False, False, 3)
vbox_c2.pack_start(self.widgets["frm_themes"], False, False, 3)
vbox_c2.pack_start(self.widgets["frm_unlock"], False, False, 3)
vbox_c1.show()
vbox_c2.show()
label = Gtk.Label(label=_("Appearance") + " 2")
self.widgets["ntb_setup"].insert_page(hbox_setup_appearance_2, label, 1)

# reorder info box (feed rate, rapid override, spindle and cooling)
self.widgets["box_info"].remove(self.widgets["grid_search"])
self.widgets["box_info"].remove(self.widgets["ntb_info"])
self.widgets["ntb_info"].remove(self.widgets["hbox_main_info"])
self.widgets["hbox_main_info"].remove(self.widgets["box_tool_and_code_info"])

box_info = Gtk.Box(orientation=Gtk.Orientation.VERTICAL)
box_info.show()
box_info.pack_start(self.widgets["box_tool_and_code_info"], False, False, 0)
box_info.pack_end(self.widgets["hbox_main_info"], False, False, 0)
self.widgets["ntb_user_tabs"].append_page(box_info, Gtk.Label("Info"))

# enable user tab button to access info box
self.widgets.tbtn_user_tabs.set_sensitive(True)
self.user_tabs_enabled = True

# change default values (not necessary with correct pref file)
self.xpos = 20
self.ypos = 20
self.width = 770
self.height = 580
self.widgets.adj_x_pos.set_value(self.xpos)
self.widgets.adj_y_pos.set_value(self.ypos)
self.widgets.adj_width.set_value(self.width)
self.widgets.adj_height.set_value(self.height)