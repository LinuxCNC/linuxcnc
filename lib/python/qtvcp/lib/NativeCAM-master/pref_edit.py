
# coding: utf-8
'''
Created on 2017-03-06

@author: Fernand
'''

import gtk
import sys, os
from gtk import gdk
import pygtk
pygtk.require('2.0')
import ConfigParser
import gettext, re
import pango

translate_test = True

if __name__ == '__main__':

    APP_NAME = 'nativecam'
    nativecam_locale = os.getenv('NATIVECAM_LOCALE')
    if nativecam_locale is not None :
        translate_test = True
    else :
        translate_test = False
        nativecam_locale = '/usr/share/locale'
    gettext.bindtextdomain(APP_NAME, nativecam_locale)
    gettext.textdomain(APP_NAME)
    try :
        lang = gettext.translation(APP_NAME, nativecam_locale, fallback = True)
        lang.install()
    except :
        gettext.install(APP_NAME, None, unicode = True)

def translate(fstring):
    # translate the glade file when testing translation
    txt2 = fstring.split('\n')
    fstring = ''
    for line in txt2 :
        inx = line.find('translatable="yes">')
        if inx > -1 :
            inx2 = line.find('</')
            txt = line[inx + 19:inx2]
            line = re.sub(r'%s' % txt, '%s' % _(txt), line)
        fstring += (line + '\n')
    return fstring

msizecomp = (10, 16, 20, 26, 28, 34, 48)

class PrefEditor():

    def read_float(self, cf, section, key, default):
        try :
            if cf == 'p' :
                return self.config_pref.getfloat(section, key)
            else :
                return self.config_def.getfloat(section, key)
        except :
            return default

    def read_boolean(self, cf, section, key, default):
        try :
            if cf == 'p' :
                return self.config_pref.getboolean(section, key)
            else :
                return self.config_def.getboolean(section, key)
        except :
            return default

    def read_str(self, cf, section, key, default):
        try :
            if cf == 'p' :
                val = self.config_pref.get(section, key)
            else :
                val = self.config_def.get(section, key)
            if val is None :
                return default
            else :
                return val
        except :
            return default

    def read_int(self, cf, section, key, default):
        return int(round(self.read_float(cf, section, key, default), 0))

    def getint(self, value):
        return int(round(value, 0))


    def __init__(self, ncam, is_metric, catalog, path, pre_amble, post_amble, sysdir) :
        global translate_test

        self.path = path
        self.default_metric = is_metric
        self.cfg_file = os.path.join(path, 'catalogs', 'ncam.conf')
        self.config_pref = ConfigParser.ConfigParser()
        self.config_pref.read(self.cfg_file)

        self.pref_file = os.path.join(path, 'catalogs', catalog, 'default.conf')
        self.config_def = ConfigParser.ConfigParser()
        self.config_def.read(self.pref_file)
        self.catalog = catalog

        try :
            gf = open(os.path.join(sysdir, "ncam_pref.glade")).read()
        except :
            raise IOError(_("Expected file not found : %s") % 'ncam_pref.glade')

        builder = gtk.Builder()
        if translate_test :
            gf = translate(gf)
        else :
            builder.set_translation_domain('nativecam')

        builder.add_from_string(gf)

        prefdlg = builder.get_object("prefdlg")
        if ncam is not None :
            parent = ncam.main_box.get_toplevel()
            builder.get_object("hscaleWindowWidth").set_adjustment(ncam.w_adj)
            builder.get_object("hscaleTVWidth").set_adjustment(ncam.tv_w_adj)
            builder.get_object("hscaleNameColWidth").set_adjustment(ncam.col_width_adj)
        else :
            parent = None

        builder.get_object("valueLbl").modify_font(pango.FontDescription('sans 16'))
        self.adj_WindowWidth = builder.get_object("hscaleWindowWidth").get_adjustment()
        self.adj_tvWidth = builder.get_object("hscaleTVWidth").get_adjustment()
        self.adj_nameColWidth = builder.get_object("hscaleNameColWidth").get_adjustment()

        self.table11 = builder.get_object("table11")

        if ncam is None :
            self.adj_WindowWidth.set_value(self.read_int('p', 'display', 'width', 550))
            self.adj_tvWidth.set_value(self.read_int('p', 'display', 'master_tv_width', 175))
            self.adj_nameColWidth.set_value(self.read_int('p', 'display', 'name_col_width', 160))

        self.ellip_name_combo = builder.get_object("ellip_name_combo")
        self.ellip_name_combo.set_active(self.read_int('p', 'display', 'name-ellipsis', 2))

        self.tv2_expandable = builder.get_object("tv2_expandable")
        self.tv2_expandable.set_active(self.read_boolean('p', 'display', 'tv2_expandable', False))

        self.tv_expandable = builder.get_object("tv_expandable")
        self.tv_expandable.set_active(self.read_boolean('p', 'display', 'tv_expandable', False))

        self.adj_vkbwidth = builder.get_object("adj_vkbwidth")
        self.adj_vkbwidth.set_value(self.read_int('p', 'virtual_kb', 'minimum_width', 260))
        self.adj_vkbwidth.connect('value-changed', self.set_vkb_height)
        self.adj_vkbheight = builder.get_object("adj_vkbheight")
        self.adj_vkbheight.connect('value-changed', self.set_vkb_height)
        self.adj_vkbheight.set_value(self.read_int('p', 'virtual_kb', 'height', 260))
        self.vkb_cancel = builder.get_object("vkb_cancel")
        self.vkb_cancel.set_active(self.read_boolean('p', 'virtual_kb', 'cancel_on_focus_out', True))

        self.restore_tvstate = builder.get_object("restore_tvstate")
        self.restore_tvstate.set_active(self.read_boolean('p', 'display', 'restore_expand_state', True))

        self.imgMenu = builder.get_object("imgMenu")
        self.adj_menuiconsize = builder.get_object("adj_menuiconsize")
        self.adj_menuiconsize.set_value(self.read_int('p', 'icons_size', 'menu', 4))
        self.adj_menuiconsize.connect("value-changed", self.menu_isize)
        self.adj_menuiconsize.connect("value-changed", self.menu_isize1)

        self.imgAddMenu = builder.get_object("imgAddMenu")
        self.adj_addmenuiconsize = builder.get_object("adj_addmenuiconsize")
        self.adj_addmenuiconsize.set_value(self.read_int('p', 'icons_size', 'add_menu', 24))
        self.adj_addmenuiconsize.connect("value-changed", self.addmenu_isize)

        self.imgToolbar = builder.get_object("imgMToolBar")
        self.adj_tbIconSize = builder.get_object("adj_tbIconSize")
        self.adj_tbIconSize.set_value(self.read_int('p', 'icons_size', 'toolbar', 5))
        self.adj_tbIconSize.connect("value-changed", self.toolbar_isize)
        self.adj_tbIconSize.connect("value-changed", self.toolbar_isize1)

        self.imgHistTB = builder.get_object("imgHistTB")
        self.adj_histiconsize = builder.get_object("adj_histiconsize")
        self.adj_histiconsize.set_value(self.read_int('p', 'icons_size', 'ncam_toolbar', 30))
        self.adj_histiconsize.connect("value-changed", self.imgHist_isize)

        self.imgTV = builder.get_object("imgTV")
        self.adj_tviconsize = builder.get_object("adj_tviconsize")
        self.adj_tviconsize.set_value(self.read_int('p', 'icons_size', 'treeview', 28))
        self.adj_tviconsize.connect("value-changed", self.tv_isize)

        self.imgAddDlg = builder.get_object("imgAddDlg")
        self.adj_adddlgimgsize = builder.get_object("adj_adddlgimgsize")
        self.adj_adddlgimgsize.set_value(self.read_int('p', 'icons_size', 'add_dlg', 70))
        self.adj_adddlgimgsize.connect("value-changed", self.adddlg_isize)

        self.comboProbe = builder.get_object("comboProbe")
        self.comboProbe.set_active(self.read_int('d', 'probe', 'probe_func', 4) - 2)

        self.adj_probelatch = builder.get_object("adj_probelatch")
        self.adj_probelatchfeed = builder.get_object("adj_probelatchfeed")
        self.adj_probefeed = builder.get_object("adj_probefeed")
        self.adj_probesafe = builder.get_object("adj_probesafe")
        self.adj_probedia = builder.get_object("adj_probedia")
        self.adj_probeheight = builder.get_object("adj_probeheight")

        self.adj_centerdrill_depth = builder.get_object("adj_centerdrill_depth")

        if self.default_metric :
            self.adj_probelatch.set_value(self.read_float('d', 'probe_mm', 'probe_latch', -1.0))
            self.adj_probelatchfeed.set_value(self.read_float('d', 'probe_mm', 'probe_latch_feed', 50.0))
            self.adj_probefeed.set_value(self.read_float('d', 'probe_mm', 'probe_feed', 200.0))
            self.adj_probesafe.set_value(self.read_float('d', 'probe_mm', 'probe_safe', 5.0))
            self.adj_probedia.set_value(self.read_float('d', 'probe_mm', 'probe_tip_dia', 3.0))
            self.adj_probeheight.set_value(self.read_float('d', 'probe_mm', 'probe_height', 0.0))

            self.adj_centerdrill_depth.set_value(self.read_float('d', 'drill_mm', 'center_drill_depth', -3.0))
        else :
            self.adj_probelatch.set_value(self.read_float('d', 'probe', 'probe_latch', -0.05))
            self.adj_probelatchfeed.set_value(self.read_float('d', 'probe', 'probe_latch_feed', 2.0))
            self.adj_probefeed.set_value(self.read_float('d', 'probe', 'probe_feed', 8.0))
            self.adj_probesafe.set_value(self.read_float('d', 'probe', 'probe_safe', 0.2))
            self.adj_probedia.set_value(self.read_float('d', 'probe', 'probe_tip_dia', 0.125))
            self.adj_probeheight.set_value(self.read_float('d', 'probe', 'probe_height', 0.0))

            self.adj_centerdrill_depth.set_value(self.read_float('d', 'drill', 'center_drill_depth', -0.125))

        self.finalcut_chk = builder.get_object("finalcut_chk")
        self.finalcut_chk.set_active(self.read_boolean('d', 'general', 'show_final_cut', True))

        self.autosave_chk = builder.get_object("autosave_chk")
        self.autosave_chk.set_active(self.read_boolean('d', 'general', 'autosave', False))

        self.finalbottom_chk = builder.get_object("finalbottom_chk")
        self.finalbottom_chk.set_active(self.read_boolean('d', 'general', 'show_bottom_cut', True))
        self.finalcut_chk.connect("toggled", self.ref_clicked)
        self.finalbottom_lbl = builder.get_object("label29")

        self.comboCoords = builder.get_object("comboCoords")
        self.comboCoords.set_active(self.read_int('d', 'ngc', 'off_rot_coord_system', 2))

        self.adj_timeout_value = builder.get_object("adj_timeout_value")
        self.adj_timeout_value.set_value(self.read_float('d', 'general', 'time_out', 0.300))
        self.digits_combo = builder.get_object("digits_combo")
        self.digits_combo.set_active(self.read_int('d', 'general', 'digits', 3) - 1)
        self.adj_spindledelay = builder.get_object("adj_spindledelay")

        self.units_combo = builder.get_object("units_combo")
        self.units_combo.set_active(self.read_int('d', 'general', 'default_metric', 0))

        self.adj_spindledelay.set_value(self.read_float('d', 'ngc', 'spindle_acc_time', 0.0))

        self.spindle_always_chk = builder.get_object("spindle_always_chk")
        self.spindle_always_chk.set_active(self.read_boolean('d', 'ngc', 'spindle_all_time', True))

        self.adj_gmoccapy = builder.get_object("adj_gmoccapy")
        self.adj_gmoccapy.set_value(self.read_float('d', 'general', 'gmoccapy_time_out', 0.15))

        self.tlo_chk = builder.get_object("tlo_chk")
        self.tlo_chk.set_active(self.read_boolean('d', 'probe', 'probe_tool_len_comp', True))

        self.entryInit = builder.get_object("entryInit")
        self.entryInit.set_text(self.read_str('d', 'ngc', 'init_str', pre_amble))
        self.entryPost = builder.get_object("entryPost")
        self.entryPost.set_text(self.read_str('d', 'ngc', 'post_amble', post_amble))
        self.use_pct_signs_chk = builder.get_object("use_pct_signs_chk")
        self.use_pct_signs_chk.set_active(self.read_boolean('d', 'ngc', 'use_pct_signs', False))

        self.combo_pck = builder.get_object("combo_pck")
        self.combo_pck.set_active(self.read_int('d', 'pocket', 'mode', 0))

        self.adj_opt_eng1 = builder.get_object("adj_opt_eng1")
        self.adj_opt_eng1.set_value(self.read_float('d', 'optimizing', 'engagement1', 0.20))
        self.adj_opt_eng2 = builder.get_object("adj_opt_eng2")
        self.adj_opt_eng2.set_value(self.read_float('d', 'optimizing', 'engagement2', 0.30))
        self.adj_opt_eng3 = builder.get_object("adj_opt_eng3")
        self.adj_opt_eng3.set_value(self.read_float('d', 'optimizing', 'engagement3', 0.80))

        self.adj_opt_ff1 = builder.get_object("adj_opt_ff1")
        self.adj_opt_ff1.set_value(self.read_float('d', 'optimizing', 'feedfactor1', 1.60))
        self.adj_opt_ff2 = builder.get_object("adj_opt_ff2")
        self.adj_opt_ff2.set_value(self.read_float('d', 'optimizing', 'feedfactor2', 1.40))
        self.adj_opt_ff3 = builder.get_object("adj_opt_ff3")
        self.adj_opt_ff3.set_value(self.read_float('d', 'optimizing', 'feedfactor3', 1.25))
        self.adj_opt_ff4 = builder.get_object("adj_opt_ff4")
        self.adj_opt_ff4.set_value(self.read_float('d', 'optimizing', 'feedfactor4', 1.00))
        self.adj_opt_ff0 = builder.get_object("adj_opt_ff0")
        self.adj_opt_ff0.set_value(self.read_float('d', 'optimizing', 'feedfactor0', 1.00))

        self.adj_opt_sf1 = builder.get_object("adj_opt_sf1")
        self.adj_opt_sf1.set_value(self.read_float('d', 'optimizing', 'speedfactor1', 1.25))
        self.adj_opt_sf2 = builder.get_object("adj_opt_sf2")
        self.adj_opt_sf2.set_value(self.read_float('d', 'optimizing', 'speedfactor2', 1.25))
        self.adj_opt_sf3 = builder.get_object("adj_opt_sf3")
        self.adj_opt_sf3.set_value(self.read_float('d', 'optimizing', 'speedfactor3', 1.25))
        self.adj_opt_sf4 = builder.get_object("adj_opt_sf4")
        self.adj_opt_sf4.set_value(self.read_float('d', 'optimizing', 'speedfactor4', 1.00))
        self.adj_opt_sf0 = builder.get_object("adj_opt_sf0")
        self.adj_opt_sf0.set_value(self.read_float('d', 'optimizing', 'speedfactor0', 1.00))

        self.plasma_tmode_chk = builder.get_object("plasma_tmode_chk")
        self.plasma_tmode_chk.set_active(self.read_boolean('d', 'plasma', 'test_mode', True))

        builder.get_object("spinbutton31").set_icon_from_pixbuf(0, self.get_pixbuf('zoom-fit.png', 22))
        builder.get_object("spinbutton16").set_icon_from_pixbuf(0, self.get_pixbuf('zoom-fit.png', 22))
        builder.get_object("spinbutton28").set_icon_from_pixbuf(0, self.get_pixbuf('zoom-fit.png', 22))
        builder.get_object("entry1").set_icon_from_pixbuf(0, self.get_pixbuf('zoom-100.png', 22))

        builder.get_object("spinbutton17").set_icon_from_pixbuf(0, self.get_pixbuf('feed-fact.png', 22))
        builder.get_object("spinbutton18").set_icon_from_pixbuf(0, self.get_pixbuf('feed-fact.png', 22))
        builder.get_object("spinbutton19").set_icon_from_pixbuf(0, self.get_pixbuf('feed-fact.png', 22))
        builder.get_object("spinbutton20").set_icon_from_pixbuf(0, self.get_pixbuf('feed-fact.png', 22))
        builder.get_object("spinbutton21").set_icon_from_pixbuf(0, self.get_pixbuf('feed-fact.png', 22))

        builder.get_object("spinbutton22").set_icon_from_pixbuf(0, self.get_pixbuf('spindle-sf.png', 22))
        builder.get_object("spinbutton24").set_icon_from_pixbuf(0, self.get_pixbuf('spindle-sf.png', 22))
        builder.get_object("spinbutton25").set_icon_from_pixbuf(0, self.get_pixbuf('spindle-sf.png', 22))
        builder.get_object("spinbutton26").set_icon_from_pixbuf(0, self.get_pixbuf('spindle-sf.png', 22))
        builder.get_object("spinbutton27").set_icon_from_pixbuf(0, self.get_pixbuf('spindle-sf.png', 22))

        builder.get_object("spin_autorefresh").set_icon_from_pixbuf(0, self.get_pixbuf('time-out.png', 22))
        builder.get_object("spinbutton14").set_icon_from_pixbuf(0, self.get_pixbuf('time-out.png', 22))
        builder.get_object("spin_gmoccapy").set_icon_from_pixbuf(0, self.get_pixbuf('time-out.png', 22))

        builder.get_object("entryInit").set_icon_from_pixbuf(0, self.get_pixbuf('pre-amble.png', 22))
        builder.get_object("entryPost").set_icon_from_pixbuf(0, self.get_pixbuf('post-amble.png', 22))

        builder.get_object("spinbutton9").set_icon_from_pixbuf(0, self.get_pixbuf('feed-fact.png', 22))
        builder.get_object("spinbutton10").set_icon_from_pixbuf(0, self.get_pixbuf('feed-fact.png', 22))
        builder.get_object("spinbutton8").set_icon_from_pixbuf(0, self.get_pixbuf('feed-fact.png', 22))
        builder.get_object("spinbutton11").set_icon_from_pixbuf(0, self.get_pixbuf('feed-fact.png', 22))
        builder.get_object("spinbutton12").set_icon_from_pixbuf(0, self.get_pixbuf('feed-fact.png', 22))

        builder.get_object("spinbutton13").set_icon_from_pixbuf(0, self.get_pixbuf('feed-fact.png', 22))

        builder.get_object("buttonSave").connect('clicked', self.save_click)
        prefdlg.set_transient_for(parent)
        prefdlg.set_keep_above(True)
        prefdlg.show_all()
        builder.get_object('spinbutton14').set_visible(catalog in ['mill', 'lathe'])
        builder.get_object('label53').set_visible(catalog in ['mill', 'lathe'])
        builder.get_object('alignment5').set_visible(catalog == 'mill')
        builder.get_object('alignment9').set_visible(catalog == 'mill')
        builder.get_object('alignment1').set_visible(catalog == 'mill')
        builder.get_object('alignment7').set_visible(catalog == 'plasma')
        builder.get_object('label28').set_visible(catalog in ['mill', 'plasma'])
        builder.get_object('label29').set_visible(catalog in ['mill', 'plasma'])
        builder.get_object('finalcut_chk').set_visible(catalog in ['mill', 'plasma'])
        builder.get_object('finalbottom_chk').set_visible(catalog in ['mill', 'plasma'])
        self.ref_clicked()
        self.menu_isize()
        self.tv_isize()
        self.adddlg_isize()
        self.imgHist_isize()
        self.addmenu_isize()
        self.toolbar_isize()
        self.set_vkb_height()

        self.saved = False
        prefdlg.run()
        prefdlg.destroy()

    def get_pixbuf(self, icon, size) :
        if size < 16 :
            size = 16
        imgfile = os.path.join(self.path, 'graphics', icon)
        if imgfile is not None :
            try :
                return gdk.pixbuf_new_from_file_at_size(imgfile, size, size)
            except gdk.PixbufError as err :
                print(err)
        return None

    def tv_isize(self, *args):
        self.imgTV.set_from_pixbuf(self.get_pixbuf('circle.png', self.getint(self.adj_tviconsize.get_value())))

    def adddlg_isize(self, *args):
        self.imgAddDlg.set_from_pixbuf(self.get_pixbuf('circle.png', self.getint(self.adj_adddlgimgsize.get_value())))

    def imgHist_isize(self, *args):
        self.imgHistTB.set_from_pixbuf(self.get_pixbuf('circle.png', self.getint(self.adj_histiconsize.get_value())))

    def addmenu_isize(self, *args):
        self.imgAddMenu.set_from_pixbuf(self.get_pixbuf('circle.png', self.getint(self.adj_addmenuiconsize.get_value())))

    def toolbar_isize(self, *args):
        self.imgToolbar.set_from_stock('gtk-save', self.getint(self.adj_tbIconSize.get_value()))

    def toolbar_isize1(self, *args):
        self.adj_histiconsize.set_value(msizecomp[self.getint(self.adj_tbIconSize.get_value())])

    def menu_isize(self, *args):
        self.imgMenu.set_from_stock('gtk-open', self.getint(self.adj_menuiconsize.get_value()))

    def menu_isize1(self, *args):
        self.adj_addmenuiconsize.set_value(msizecomp[self.getint(self.adj_menuiconsize.get_value())])

    def ref_clicked(self, *args):
        self.finalbottom_chk.set_sensitive(self.finalcut_chk.get_active())
        self.finalbottom_lbl.set_sensitive(self.finalcut_chk.get_active())

    def set_vkb_height(self, *arg):
        self.table11.set_size_request(self.getint(self.adj_vkbwidth.get_value()), self.getint(self.adj_vkbheight.get_value()))

    def save_click(self, *args):
        # preferences first
        if not self.config_pref.has_section('display') :
            self.config_pref.add_section('display')
        self.config_pref.set('display', 'width', self.getint(self.adj_WindowWidth.get_value()))
        self.config_pref.set('display', 'name-ellipsis', self.ellip_name_combo.get_active())
        self.config_pref.set('display', 'name_col_width', self.getint(self.adj_nameColWidth.get_value()))
        self.config_pref.set('display', 'master_tv_width', self.getint(self.adj_tvWidth.get_value()))
        self.config_pref.set('display', 'restore_expand_state', self.restore_tvstate.get_active())
        self.config_pref.set('display', 'tv2_expandable', self.tv2_expandable.get_active())
        self.config_pref.set('display', 'tv_expandable', self.tv_expandable.get_active())

        if not self.config_pref.has_section('icons_size') :
            self.config_pref.add_section('icons_size')
        self.config_pref.set('icons_size', 'treeview', self.getint(self.adj_tviconsize.get_value()))
        self.config_pref.set('icons_size', 'add_menu', self.getint(self.adj_addmenuiconsize.get_value()))
        self.config_pref.set('icons_size', 'menu', self.getint(self.adj_menuiconsize.get_value()))
        self.config_pref.set('icons_size', 'toolbar', self.getint(self.adj_tbIconSize.get_value()))
        self.config_pref.set('icons_size', 'add_dlg', self.getint(self.adj_adddlgimgsize.get_value()))
        self.config_pref.set('icons_size', 'ncam_toolbar', self.getint(self.adj_histiconsize.get_value()))

        if not self.config_pref.has_section('virtual_kb') :
            self.config_pref.add_section('virtual_kb')
        self.config_pref.set('virtual_kb', 'minimum_width', self.getint(self.adj_vkbwidth.get_value()))
        self.config_pref.set('virtual_kb', 'height', self.getint(self.adj_vkbheight.get_value()))
        self.config_pref.set('virtual_kb', 'cancel_on_focus_out', self.vkb_cancel.get_active())

        with open(self.cfg_file, 'wb') as configfile:
            self.config_pref.write(configfile)

        # defaults now
        if not self.config_def.has_section('general') :
            self.config_def.add_section('general')
        self.config_def.set('general', 'time_out', self.adj_timeout_value.get_value())
        self.config_def.set('general', 'digits', int(self.digits_combo.get_active()) + 1)
        self.config_def.set('general', 'default_metric', int(self.units_combo.get_active()))
        if self.catalog in ['mill', 'plasma'] :
            self.config_def.set('general', 'show_final_cut', self.finalcut_chk.get_active())
            self.config_def.set('general', 'show_bottom_cut', self.finalbottom_chk.get_active())
        self.config_def.set('general', 'gmoccapy_time_out', self.adj_gmoccapy.get_value())
        self.config_def.set('general', 'autosave', self.autosave_chk.get_active())

        if not self.config_def.has_section('ngc') :
            self.config_def.add_section('ngc')
        self.config_def.set('ngc', 'init_str', self.entryInit.get_text())
        self.config_def.set('ngc', 'post_amble', self.entryPost.get_text())
        self.config_def.set('ngc', 'use_pct_signs', self.use_pct_signs_chk.get_active())
        if self.catalog == 'mill' :
            self.config_def.set('ngc', 'off_rot_coord_system', self.comboCoords.get_active())
            self.config_def.set('ngc', 'spindle_all_time', self.spindle_always_chk.get_active())
        if self.catalog in ['mill', 'lathe'] :
            self.config_def.set('ngc', 'spindle_acc_time', self.adj_spindledelay.get_value())

        if self.catalog == 'mill' :
            if not self.config_def.has_section('probe') :
                self.config_def.add_section('probe')
            self.config_def.set('probe', 'probe_func', self.comboProbe.get_active() + 2)
            self.config_def.set('probe', 'probe_tool_len_comp', self.tlo_chk.get_active())

            if self.default_metric :
                probe_section = 'probe_mm'
                drill_section = 'drill_mm'
            else :
                probe_section = 'probe'
                drill_section = 'drill'

            if not self.config_def.has_section(probe_section) :
                self.config_def.add_section(probe_section)
            if not self.config_def.has_section(drill_section) :
                self.config_def.add_section(drill_section)

            self.config_def.set(probe_section, 'probe_feed', self.adj_probefeed.get_value())
            self.config_def.set(probe_section, 'probe_latch', self.adj_probelatch.get_value())
            self.config_def.set(probe_section, 'probe_latch_feed', self.adj_probelatchfeed.get_value())
            self.config_def.set(probe_section, 'probe_tip_dia', self.adj_probedia.get_value())
            self.config_def.set(probe_section, 'probe_safe', self.adj_probesafe.get_value())
            self.config_def.set(probe_section, 'probe_height', self.adj_probeheight.get_value())

            self.config_def.set(drill_section, 'center_drill_depth', self.adj_centerdrill_depth.get_value())

            if not self.config_def.has_section('pocket') :
                self.config_def.add_section('pocket')
            self.config_def.set('pocket', 'mode', self.combo_pck.get_active())

            if not self.config_def.has_section('optimizing') :
                self.config_def.add_section('optimizing')
            self.config_def.set('optimizing', 'engagement1', self.adj_opt_eng1.get_value())
            self.config_def.set('optimizing', 'engagement2', self.adj_opt_eng2.get_value())
            self.config_def.set('optimizing', 'engagement3', self.adj_opt_eng3.get_value())

            self.config_def.set('optimizing', 'feedfactor1', self.adj_opt_ff1.get_value())
            self.config_def.set('optimizing', 'feedfactor2', self.adj_opt_ff2.get_value())
            self.config_def.set('optimizing', 'feedfactor3', self.adj_opt_ff3.get_value())
            self.config_def.set('optimizing', 'feedfactor4', self.adj_opt_ff4.get_value())
            self.config_def.set('optimizing', 'feedfactor0', self.adj_opt_ff0.get_value())

            self.config_def.set('optimizing', 'speedfactor1', self.adj_opt_sf1.get_value())
            self.config_def.set('optimizing', 'speedfactor2', self.adj_opt_sf2.get_value())
            self.config_def.set('optimizing', 'speedfactor3', self.adj_opt_sf3.get_value())
            self.config_def.set('optimizing', 'speedfactor4', self.adj_opt_sf4.get_value())
            self.config_def.set('optimizing', 'speedfactor0', self.adj_opt_sf0.get_value())

        if self.catalog == 'plasma' :
            if not self.config_def.has_section('plasma') :
                self.config_def.add_section('plasma')
            self.config_def.set('plasma', 'test_mode', self.plasma_tmode_chk.get_active())

        with open(self.pref_file, 'wb') as configfile:
            self.config_def.write(configfile)

        self.saved = True

def edit_preferences(ncam = None, default_metric = True, catalog = 'mill', path = '', \
        pre_amble = '', post_amble = 'M2', sysdir = ''):
    ed = PrefEditor(ncam, default_metric, catalog, path, pre_amble.strip(), \
        post_amble.strip(), sysdir)
    return ed.saved

if __name__ == '__main__':
    sys.exit(edit_preferences())
