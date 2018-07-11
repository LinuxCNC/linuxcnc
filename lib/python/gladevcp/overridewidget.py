#!/usr/bin/python2
# GladeVcp Widget - override widget
# This widgets controls linuxcnc's override rate
#
# Copyright (c) 2015 Chris Morley
#
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 2 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.

import sys,os,pango
import linuxcnc

try:
    import gobject,gtk
except:
    print('GTK not available')
    sys.exit(1)

INIPATH = os.environ.get('INI_FILE_NAME', '/dev/null')

class Override(gtk.HScale):
    __gtype_name__ = 'Override'
    __gproperties__ = {
        'override_type' : ( gobject.TYPE_INT, 'Override Type', '0: Feed  1: Rapid  2: Spindle 3: Max velocity',
                    0, 3, 0, gobject.PARAM_READWRITE | gobject.PARAM_CONSTRUCT),
    }
    __gproperties = __gproperties__

    def __init__(self, *a, **kw):
        gtk.HScale.__init__(self, *a, **kw)
        self.emc = linuxcnc
        self.status = self.emc.stat()
        self.cmd = linuxcnc.command()
        self.override_type = 0
        self.override = 1.0
        self.max_vel_convert = 100
        self.adjustment = gtk.Adjustment(value=100, lower=0, upper=200, step_incr=1, page_incr=0, page_size=0)
        self.set_type(self.override_type)
        self.set_adjustment(self.adjustment)
        self.set_digits(0)
        self.set_value_pos(gtk.POS_LEFT)
        #self.add_mark(100.0,gtk.POS_RIGHT,'')
        self.connect('value-changed',self.update_value)
        # The update time: every 100 milliseonds
        gobject.timeout_add(100, self.periodic)

    # we set the adjustment limits based on the INI entries
    def set_type(self, data=0):
        adjustment = self.get_adjustment()
        self.override_type = data
        self.inifile = self.emc.ini(INIPATH)
        if self.override_type == 0:
            MAXFEED = float(self.inifile.find("DISPLAY","MAX_FEED_OVERRIDE") or 2.0)
            #print 'feed',MAXFEED
            adjustment.set_upper(MAXFEED*100)
            adjustment.set_lower(0)
        elif self.override_type == 1:
            #print 'rapid'
            adjustment.set_upper(100)
            adjustment.set_lower(0)
        elif self.override_type == 2:
            MINSPINDLE = float(self.inifile.find("DISPLAY","MIN_SPINDLE_OVERRIDE") or .5)
            #print 'mins',MINSPINDLE
            MAXSPINDLE = float(self.inifile.find("DISPLAY","MAX_SPINDLE_OVERRIDE") or 1.5)
            #print 'maxs',MAXSPINDLE
            adjustment.set_upper(MAXSPINDLE*100)
            adjustment.set_lower(MINSPINDLE*100)
        elif self.override_type == 3:
            MAXVEL = float(self.inifile.find("TRAJ","MAX_VELOCITY") or 100)
            #print 'maxv',MAXVEL,MAXVEL/100.0
            self.max_vel_convert = MAXVEL/100.0
            adjustment.set_upper(100)
            adjustment.set_lower(0)

    # This is a signal callback that commands linuxcnc based on the current
    # scale position
    def update_value(self, widget):
        data = widget.get_value()
        if not self.override_type == 3:
            data=data/100
        else:
            data=data*self.max_vel_convert
        #print data,self.override_type ,self.adjustment
        if self.override_type == 0:
            self.cmd.feedrate(data)
        elif self.override_type == 1:
            self.cmd.rapidrate(data)
        elif self.override_type == 2:
            self.cmd.spindleoverride(data)
        elif self.override_type == 3:
            self.cmd.maxvel(data)
        return True

    # This runs runs at the gooject timeout rate
    # it polls linuxcnc to get the current data
    # and updates the scale to refleck the current value.
    # in this way if eg HALUI is used to set an override the
    # scale will track it.
    def periodic(self):
        try:
            self.status.poll()
            if self.override_type == 0:
                self.override = self.status.feedrate
            elif self.override_type == 1:
                self.override = self.status.rapidrate
            elif self.override_type == 2:
                self.override = self.status.spindlerate
            elif self.override_type == 3:
                self.override = self.status.max_velocity
            # max velocity is not based on % so must be converted
            if not self.override_type == 3:
                self.set_value(self.override*100)
            else:
                self.set_value(self.override/self.max_vel_convert)
        except:
            pass
        return True

    # This is so GLADE can get the values for the editor
    # A user can use this too using goobject 
    def do_get_property(self, property):
        name = property.name.replace('-', '_')
        if name in self.__gproperties.keys():
            return getattr(self, name)
        else:
            raise AttributeError('unknown property %s' % property.name)

    # This is used by GLADE editor to set values
    def do_set_property(self, property, value):
        name = property.name.replace('-', '_')
        if name in self.__gproperties.keys():
            setattr(self, name, value)
        if name == 'override_type':
            self.set_type(value)
        else:
            raise AttributeError('unknown property %s' % property.name)

# for testing without glade editor:
def main():
    window = gtk.Dialog("My dialog",
                   None,
                   gtk.DIALOG_MODAL | gtk.DIALOG_DESTROY_WITH_PARENT,
                   (gtk.STOCK_CANCEL, gtk.RESPONSE_REJECT,
                    gtk.STOCK_OK, gtk.RESPONSE_ACCEPT))
    t = gtk.Table(rows=4, columns=1, homogeneous=True)
    window.vbox.add(t)

    offset1 = Override()
    offset1.set_type(0)
    t.attach(offset1, 0, 1, 0, 1)

    offset2 = Override()
    offset2.set_type(1)
    t.attach(offset2, 0, 1, 1, 2)

    offset3 = Override()
    offset3.set_type(2)
    t.attach(offset3, 0, 1, 2, 3)

    offset4 = Override()
    offset4.set_type(3)
    t.attach(offset4, 0, 1, 3, 4)

    window.connect("destroy", gtk.main_quit)

    window.show_all()
    response = window.run()
    if response == gtk.RESPONSE_ACCEPT:
       print "ok"
    else:
       print "cancel"

if __name__ == "__main__":	
    main()

