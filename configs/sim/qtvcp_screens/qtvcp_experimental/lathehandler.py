#!/usr/bin/env python3
# vim: sts=4 sw=4 et
#    This is a component of EMC
#    savestate.py copyright 2013 Andy Pugh
#    based on code from
#    probe.py Copyright 2010 Michael Haberler
#
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
#    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA''''''

import os,sys
from gladevcp.persistence import IniFile,widget_defaults,set_debug,select_widgets
import hal
import hal_glib
import gtk
import glib
import linuxcnc
import rsvg
import cairo
import signal
import pango

debug = 0

class HandlerClass:
    active = False
    tab_num = 0

    def on_expose(self,nb,data=None):
        tab_num = nb.get_current_page()
        tab = nb.get_nth_page(tab_num)
        cr = tab.window.cairo_create()
        cr.set_operator(cairo.OPERATOR_OVER)
        x, y, w, h = tab.allocation
        sx, sy, sw, sh =  self.svg.get_dimension_data()
        cr.translate(0, y)
        cr.scale(1.0 *w / sw, 1.0*h/sh)
        self.svg.render_cairo(cr = cr, id = '#layer%i' % tab_num)

    # This catches our messages from another program
    def event(self,w,event):
        print(event.message_type,event.data)
        if event.message_type == 'Gladevcp':
            if event.data[:7] == 'Visible':
                self.active = True
                self.visible.set(True)
            else:
                self.active = False
                self.visible.set(False)

    # We connect to client-events from the new toplevel widget
    def on_map_event(self, widget, data=None):
        top = widget.get_toplevel()
        print("map event")
        top.connect('client-event', self.event)

    def on_destroy(self,obj,data=None):
        self.ini.save_state(self)

    def on_restore_defaults(self,button,data=None):
        '''
        example callback for 'Reset to defaults' button
        currently unused
        '''
        self.ini.create_default_ini()
        self.ini.restore_state(self)

    def __init__(self, halcomp,builder,useropts):
        self.halcomp = halcomp
        self.builder = builder
        self.ini_filename = 'savestate.sav'
        self.defaults = {  IniFile.vars: dict(),
                           IniFile.widgets : widget_defaults(select_widgets(self.builder.get_objects(), hal_only=False,output_only = True))
                        }
        self.ini = IniFile(self.ini_filename,self.defaults,self.builder)
        self.ini.restore_state(self)

        # A pin to use a physical switch to start the cycle
        self.cycle_start = hal_glib.GPin(halcomp.newpin('cycle-start', hal.HAL_BIT, hal.HAL_IN))
        self.cycle_start.connect('value-changed', self.cycle_pin)

        # A pin to mask cycle-start so that Touchy doesn't run auto.
        self.visible = hal_glib.GPin(halcomp.newpin('visible', hal.HAL_BIT, hal.HAL_OUT))

        # This catches the signal from Touchy to say that the tab is exposed
        t = self.builder.get_object('eventbox1')
        t.connect('map-event',self.on_map_event)
        t.add_events(gtk.gdk.STRUCTURE_MASK)
        self.cmd = linuxcnc.command()

        # This connects the expose event to re-draw and scale the SVG frames
        t = self.builder.get_object('tabs1')
        t.connect_after("expose_event", self.on_expose)
        t.connect("destroy", gtk.main_quit)
        t.add_events(gtk.gdk.STRUCTURE_MASK)
        self.svg = rsvg.Handle(file='LatheMacro.svg', )

    def show_keyb(self, obj, data=None):
        self.active_ctrl = obj
        self.keyb = self.builder.get_object('keyboard')
        self.entry = self.builder.get_object('entry1')
        self.entry.modify_font(pango.FontDescription("courier 42"))
        self.entry.set_text("")
        resp = self.keyb.run()

    def keyb_prev_click(self, obj, data=None):
        self.entry.set_text(self.active_ctrl.get_text())

    def keyb_number_click(self, obj, data=None):
        data = self.entry.get_text()
        data = data + obj.get_label()
        if data[-2:] in [ '/2', '/4', '/8']:
            v = data[:-2].split('.')
            if len(v) == 2:
                data = '%6.3f'%(float(v[0]) + float(v[1]) / float(data[-1:]))
            elif len(v) == 1:
                data = '%6.3f'%(float(v[0]) / float(data[-1:]))
        elif data[-3:] in [ '/16', '/32', '/64']:
            v = data[:-3].split('.')
            if len(v) == 2:
                data = '%6.3f'%(float(v[0]) + float(v[1]) / float(data[-2:]))
            elif len(v) == 1:
                data = '%6.3f'%(float(v[0]) / float(data[-2:]))
        self.entry.set_text(data)

    def keyb_pm_click(self, obj, data=None):
        data = self.entry.get_text()
        if data[0] == '-':
            data = data[1:]
        else:
            data = '-' + data
        self.entry.set_text(data)

    def keyb_convert_click(self, obj, data=None):
        v = float(self.entry.get_text())
        op = obj.get_label()
        if op == 'in->mm':
            v = v * 25.4
        elif op == 'mm->in':
            v = v / 25.4
        elif op == 'tpi->pitch':
            v = 25.4 / v
        elif op == 'pitch->tpi':
            v = 25.4 / v
        self.entry.set_text('%6.4f'%v)

    def keyb_del_click(self, obj, data=None):
        data = self.entry.get_text()
        data = data[:-1]
        self.entry.set_text(data)

    def keyb_clear_click(self, obj, data=None):
        self.entry.set_text('')

    def keyb_cancel_click(self, obj, data=None):
        self.keyb.hide()

    def keyb_ok_click(self, obj, data=None):
        if self.entry.get_text() != '':
            self.active_ctrl.set_value(float(self.entry.get_text()))
        self.keyb.hide()

    def set_alpha(self, obj, data = None):
        cr = obj.window.cairo_create()
        cr.set_source_rgba(1.0, 1.0, 1.0, 0.0)

    def cycle_pin(self, pin, data = None):
        if pin.get() == 0:
            return
        print('cycle pin')
        if self.active:
            nb = self.builder.get_object('tabs1')
            print('current tab', nb.get_current_page())
            tab = nb.get_nth_page(nb.get_current_page())
            for c in tab.get_children():
                if c.name.partition('.')[2] == 'action':
                    self.cmd.abort()
                    self.cmd.mode(linuxcnc.MODE_MANUAL)
                    self.cmd.wait_complete()
                    c.emit('clicked')

    def gash(self, obj, data = None):
        print('event', data)

def get_handlers(halcomp,builder,useropts):

    global debug
    for cmd in useropts:
        exec cmd in globals()

    set_debug(debug)
    return [HandlerClass(halcomp,builder,useropts)]



