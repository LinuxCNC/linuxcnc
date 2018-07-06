#!/usr/bin/env python

#------------------------------------------------------------------------------
# Copyright: 2013
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
#------------------------------------------------------------------------------

"""gremlin_view
Provide class: GremlinView for gremlin with buttons for simpler embedding
Standalone functionality if linuxcnc running

A default ui file (gremlin_view.ui) is provided for a default
button arrangement but a user may provide their own by supplying
the glade_file argument.

The following objects are mandatory:

  'gremlin_view_window'      toplevel window
  'gremlin_view_hal_gremlin' hal_gremlin
  'gremlin_view_box'         HBox or VBox' containing hal_gremlin

Optional radiobutton group names:
  'select_p_view'
  'select_x_view'
  'select_y_view'
  'select_z_view'
  'select_z2_view'

Optional Checkbuttons names:
  'enable_dro'
  'show_machine_speed
  'show_distance_to_go'
  'show_limits'
  'show_extents'
  'show_tool'
  'show_metric'

Callbacks are provided for the following buttons actions
  on_clear_live_plotter_clicked
  on_enable_dro_clicked
  on_zoomin_pressed
  on_zoomout_pressed
  on_pan_x_minus_pressed
  on_pan_x_plus_pressed
  on_pan_y_minus_pressed
  on_pan_y_plus_pressed
  on_show_tool_clicked
  on_show_metric_clicked
  on_show_extents_clicked
  on_select_p_view_clicked
  on_select_x_view_clicked
  on_select_y_view_clicked
  on_select_z_view_clicked
  on_select_z2_view_clicked
  on_show_distance_to_go_clicked
  on_show_machine_speed_clicked
  on_show_limits_clicked
"""

import os
import sys
import gtk
import gladevcp.hal_actions  # reqd for Builder
import linuxcnc
import time
import subprocess
import gettext
import datetime
import gobject
import glib # for glib.GError

g_ui_dir          = linuxcnc.SHARE + "/linuxcnc"
g_periodic_secs   = 1 # integer
g_delta_pixels    = 10
g_move_delay_secs = 0.2
g_progname        = os.path.basename(sys.argv[0])
g_verbose         = False

LOCALEDIR = linuxcnc.SHARE + "/locale"
gettext.install("linuxcnc", localedir=LOCALEDIR, unicode=True)

def ini_check ():
    """set environmental variable and change directory"""
    # Note:
    #   hal_gremlin gets inifile from os.environ (only)
    #   hal_gremlin expects cwd to be same as ini file
    ini_filename = get_linuxcnc_ini_file()
    if ini_filename is not None:
        os.putenv('INI_FILE_NAME',ini_filename)    # ineffective
        os.environ['INI_FILE_NAME'] = ini_filename # need for hal_gremlin
        os.chdir(os.path.dirname(ini_filename))
        if g_verbose:
            print('ini_check: INI_FILENAME= %s' % ini_filename)
            print('ini_check:       curdir= %s' % os.path.curdir)
        return True # success
    print(_('%s:linuxcnc ini file  not available') % g_progname)
    return False # exit here crashes glade-gtk2

def get_linuxcnc_ini_file():
    """find linuxcnc ini file with pgrep"""
    ps   = subprocess.Popen('ps -C linuxcncsvr --no-header -o args'.split(),
                             stdout=subprocess.PIPE
                           )
    p,e = ps.communicate()

    if ps.returncode:
        print(_('get_linuxcnc_ini_file: stdout= %s') % p)
        print(_('get_linuxcnc_ini_file: stderr= %s') % e)
        return None

    ans = p.split()[p.split().index('-ini')+1]
    return ans

class GremlinView():
    """Implement a standalone gremlin with some buttons
       and provide means to embed using a glade ui file"""
    def __init__(self
                ,glade_file=None #None: use default ui
                ,parent=None
                ,width=None
                ,height=None
                ,alive=True
                ,gtk_theme_name="Follow System Theme"
                ):

        self.alive = alive
        linuxcnc_running = False
        if ini_check():
            linuxcnc_running = True

        if (glade_file == None):
            glade_file = os.path.join(g_ui_dir,'gremlin_view.ui')

        bldr = gtk.Builder()
        try:
            bldr.add_from_file(glade_file)
        except glib.GError,detail:
            print('\nGremlinView:%s\n' % detail)
            raise glib.GError,detail # re-raise

        # required objects:
        self.topwindow = bldr.get_object('gremlin_view_window')
        self.gbox      = bldr.get_object('gremlin_view_box')
        self.halg      = bldr.get_object('gremlin_view_hal_gremlin')

        #self.halg.show_lathe_radius = 1 # for test, hal_gremlin default is Dia

        if not linuxcnc_running:
            # blanks display area:
            self.halg.set_has_window(False)

        # radiobuttons for selecting view: (expect at least one)
        select_view_letters = ['p','x','y','z','z2']
        found_view = None
        for vletter in select_view_letters:
            try:
                obj = bldr.get_object('select_' + vletter + '_view')
            except:
                continue
            if obj is not None:
                setattr(self,vletter + '_view',obj)
                if found_view is None:
                    found_view = obj
                    self.my_view = vletter
                    obj.set_group(None)
                    obj.set_active(True)
                else:
                    obj.set_group(found_view)
        if found_view is None:
            print('%s:Expected to find "select_*_view"' % __file__)

        check_button_objects = ['enable_dro'
                               ,'show_machine_speed'
                               ,'show_distance_to_go'
                               ,'show_limits'
                               ,'show_extents'
                               ,'show_tool'
                               ,'show_metric'
                               ]
        for objname in check_button_objects:
            obj = bldr.get_object(objname)
            if obj is not None:
                setattr(self,'objname',obj)
                obj.set_active(True)
            else:
                if g_verbose:
                    print('%s: Optional object omitted <%s>'
                          % (__file__,objname))

        # show_metric: use ini file
#FIXME  show_metric,lunits s/b mandatory?
        try:
            objname = 'show_metric'
            self.show_metric = bldr.get_object('show_metric')
            lunits = self.halg.inifile.find('TRAJ','LINEAR_UNITS')
        except AttributeError:
            if g_verbose:
                print('%s: Problem for <%s>' % (__file__,objname))

        if linuxcnc_running:
            if   lunits == 'inch':
                self.halg.metric_units = False
            elif lunits == 'mm':
                self.halg.metric_units = True
            else:
                raise AttributeError,('%s: unknown [TRAJ]LINEAR_UNITS] <%s>'
                                     % (__file__,lunits))

        if self.halg.get_show_metric():
            self.show_metric.set_active(True)
        else:
            self.show_metric.set_active(False)

        if alive:
            bldr.connect_signals(self)
            # todo: to remove other signals on halg:
            # bldr.disconnect(integer_handle_id)
            # bldr.disconnect_by_func('func_name')
            # bldr.handler_disconnect()

        minwidth  = 300 # smallest size
        minheight = 300 # smallest size

        if (width  is None):
            width = minwidth
        else:
            width = int(width)

        if (height is None):
            height = minheight
        else:
            height = int(height)

        if width  < minwidth:
            width  = minwidth
        if height < minheight:
            height = minheight

        # err from gremlin if omit this
        self.halg.width  = width
        self.halg.height = height
        self.halg.set_size_request(width,height)

        # self.x,self.y used in conjunction with pan buttons
        # but using mouse may change internal values in gremlin
        # resulting in unexpected movement if both mouse and
        # pan buttons are used
        self.x = 0
        self.y = 0

        #  prevent flashing topwindow
        self.topwindow.iconify()
        self.topwindow.show_all()
        self.topwindow.hide()

        self.preview_file(None)
        if linuxcnc_running:
            try:
                self.preview_file(None)
            except linuxcnc.error,detail:
                print('linuxcnc.error')
                print('        detail=',detail)

        try:
            self.last_file = self.halg._current_file
        except AttributeError:
            self.last_file = None
        self.last_file_mtime = None

        self.parent = parent
        if self.parent is None:
            # topwindow (standalone) application
            # print "TOP:",gtk_theme_name
            screen   = self.topwindow.get_screen()
        else:
            # print "REPARENT:",gtk_theme_name
            screen   = self.halg.get_screen()

        settings = gtk.settings_get_for_screen(screen)
        systname = settings.get_property("gtk-theme-name")
        if (   (gtk_theme_name is None)
            or (gtk_theme_name == "")
            or (gtk_theme_name == "Follow System Theme")):
            gtk_theme_name = systname
        settings.set_string_property('gtk-theme-name',gtk_theme_name,"")

        self.topwindow.connect('destroy',self._topwindowquit)
        self.topwindow.show_all()
        self.running = True

        if self.last_file is not None:
            self.topwindow.set_title(g_progname
                      + ': ' + os.path.basename(self.last_file))
            self.last_file_mtime = datetime.datetime.fromtimestamp(
                                   os.path.getmtime(self.last_file))

        self.ct = 0
        if self.parent is None: self.topwindow.deiconify()
        self._periodic('BEGIN')
        gobject.timeout_add_seconds(g_periodic_secs,self._periodic,'Continue')
        # or use gobject.timeout_add() interval units in mS

    def _periodic(self,arg):
        # print "_periodic:",self.ct,arg
        self.ct +=1
        self.halg.poll()

        if (self.parent is not None) and (self.ct) == 2:
            # not sure why delay is needed for reparenting
            # but without, the display of the (rgb) axes
            # and the cone to not appear in gremlin
            # print "REPARENT:",self.gbox, self.parent
            #-----------------------------------------------------------------------------
            # determine if glade interface designer is running
            # to avoid assertion error:
            # gtk_widget_reparent_fixup_child: assertion failed: (client_data != NULL)
            is_glade = False
            if 'glade' in sys.argv[0] and 'gladevcp' not in sys.argv[0]:
                for d in os.environ['PATH'].split(':'):
                    f = os.path.join(d,sys.argv[0])
                    if (    os.path.isfile(f)
                        and os.access(f, os.X_OK)):
                        is_glade = True
                        break
            #-----------------------------------------------------------------------------
            if (not is_glade):
                self.gbox.reparent(self.parent)
            self.gbox.show_all()
            self.gbox.connect('destroy',self._gboxquit)
            return True

        try:
            current_file = self.halg._current_file
        except AttributeError:
            current_file = None
        if current_file is None:
            return True # keep trying _periodic()
        current_file_mtime = datetime.datetime.fromtimestamp(
                               os.path.getmtime(current_file))
        if (   current_file       != self.last_file
            or current_file_mtime != self.last_file_mtime):
            # print('old,new',self.last_file_mtime,current_file_mtime)
            self.last_file       = current_file
            self.last_file_mtime = current_file_mtime
            self.halg.hide()
            self.halg.load()
            getattr(self.halg,'set_view_%s' % self.my_view)()
            self.halg.show()
            if self.topwindow is not None:
                self.topwindow.set_title(g_progname
                           + ': ' + os.path.basename(self.last_file))
        return True # repeat _periodic()

    def preview_file(self,filename):
        self.halg.hide()
        # handle exception in case glade is running
        try:
            self.halg.load(filename or None)
        except Exception, detail:
            if self.alive:
                print "file load fail:",Exception,detail
            pass
        getattr(self.halg,'set_view_%s' % self.my_view)()
        self.halg.show()

    def _gboxquit(self,w):
        self.running = False # stop periodic checks

    def _topwindowquit(self,w):
        self.running = False # stop periodic checks
        gtk.main_quit()

    def expose(self):
        self.halg.expose()

    def on_zoomin_pressed(self,w):
        while w.get_state() == gtk.STATE_ACTIVE:
            self.halg.zoomin()
            time.sleep(g_move_delay_secs)
            gtk.main_iteration_do()

    def on_zoomout_pressed(self,w):
        while w.get_state() == gtk.STATE_ACTIVE:
            self.halg.zoomout()
            time.sleep(g_move_delay_secs)
            gtk.main_iteration_do()

    def on_pan_x_minus_pressed(self,w):
        while w.get_state() == gtk.STATE_ACTIVE:
            self.x -= g_delta_pixels
            self.halg.translate(self.x,self.y)
            time.sleep(g_move_delay_secs)
            gtk.main_iteration_do()

    def on_pan_x_plus_pressed(self,w):
        while w.get_state() == gtk.STATE_ACTIVE:
            self.x += g_delta_pixels
            self.halg.translate(self.x,self.y)
            time.sleep(g_move_delay_secs)
            gtk.main_iteration_do()

    def on_pan_y_minus_pressed(self,w):
        while w.get_state() == gtk.STATE_ACTIVE:
            self.y += g_delta_pixels
            self.halg.translate(self.x,self.y)
            time.sleep(g_move_delay_secs)
            gtk.main_iteration_do()

    def on_pan_y_plus_pressed(self,w):
        while w.get_state() == gtk.STATE_ACTIVE:
            self.y -= g_delta_pixels
            self.halg.translate(self.x,self.y)
            time.sleep(g_move_delay_secs)
            gtk.main_iteration_do()

    def on_clear_live_plotter_clicked(self,w):
        self.halg.clear_live_plotter()

    def on_enable_dro_clicked(self,w):
        if w.get_active():
            self.halg.enable_dro = True
        else:
            self.halg.enable_dro = False
        self.expose()

    def on_show_machine_speed_clicked(self,w):
        if w.get_active():
            self.halg.show_velocity = True
        else:
            self.halg.show_velocity = False
        self.expose()

    def on_show_distance_to_go_clicked(self,w):
        if w.get_active():
            self.halg.show_dtg = True
        else:
            self.halg.show_dtg = False
        self.expose()

    def on_show_limits_clicked(self,w):
        if w.get_active():
            self.halg.show_limits = True
        else:
            self.halg.show_limits = False
        self.expose()

    def on_show_extents_clicked(self,w):
        if w.get_active():
            self.halg.show_extents_option = True
        else:
            self.halg.show_extents_option = False
        self.expose()

    def on_show_tool_clicked(self,w):
        if w.get_active():
            self.halg.show_tool = True
        else:
            self.halg.show_tool = False
        self.expose()

    def on_show_metric_clicked(self,w):
        if w.get_active():
            self.halg.metric_units = True
        else:
            self.halg.metric_units = False
        self.expose()

    def on_select_p_view_clicked(self,w):
        self.set_view_per_w(w,'p')

    def on_select_x_view_clicked(self,w):
        self.set_view_per_w(w,'x')

    def on_select_y_view_clicked(self,w):
        self.set_view_per_w(w,'y')

    def on_select_z_view_clicked(self,w):
        self.set_view_per_w(w,'z')

    def on_select_z2_view_clicked(self,w):
        self.set_view_per_w(w,'z2')

    def set_view_per_w(self,w,vletter):
        if not w.get_active(): return
        self.halg.hide()
        getattr(self.halg,'set_view_%s' % vletter)()
        self.my_view = vletter
        self.halg.show()

#-----------------------------------------------------------------------------
# Standalone (and demo) usage:
def standalone_gremlin_view():

    import getopt
    #---------------------------------------
    def usage(msg=None):

        print("""\n
Usage:   %s [options]\n
Options: [-h | --help]
         [-v | --verbose]
         [-W | --width]  width
         [-H | --height] height
         [-f | --file]   glade_file

Note: linuxcnc must be running on same machine
""") % g_progname
        if msg:
            print('\n%s' % msg)
    #---------------------------------------

    glade_file  = None
    width       = None
    height      = None
    vbose       = False
    try:
        options,remainder = getopt.getopt(sys.argv[1:]
                                         , 'f:hH:vW:'
                                         , ['file='
                                           ,'help'
                                           ,'width='
                                           ,'height='
                                           ]
                                         )
    except getopt.GetoptError,msg:
        usage()
        print('GetoptError: %s' % msg)
        sys.exit(1)
    for opt,arg in options:
        if opt in ('-h','--help'):
            usage(),sys.exit(0)
        if opt in ('-v','--verbose'):
            g_verbose = True
            continue
        if opt in ('-W','--width' ): width=arg
        if opt in ('-H','--height'): height=arg
        if opt in ('-f','--file'):   glade_file=arg
    if remainder:
        usage('unknown argument:%s' % remainder)
        sys.exit(1)

    try:
        g = GremlinView(glade_file=glade_file
                       ,width=width
                       ,height=height
                       )
        gtk.main()
    except linuxcnc.error,detail:
        gtk.main()
        print('linuxcnc.error:',detail)
        usage()

# vim: sts=4 sw=4 et
