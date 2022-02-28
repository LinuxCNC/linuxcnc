# GladeVcp HAL lighted button
#
# Copyright (c) 2015  Moses McKnight <moses@texband.net>
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

import gi
gi.require_version("Gtk","3.0")
from gi.repository import Gtk
from gi.repository import Gdk
from gi.repository import GObject
from gi.repository import Pango
import cairo
import math

# This creates the custom lighted button widget
# A lighted button has a HAL_OUT or HAL_IO pin for the button.  This pin indicates if the button is pressed or toggled or not.
# If it is an HAL_IO pin, the pin is set True when the button is clicked and must be set False externally.
#    This gives an external component a chance to "see" the pin state.
# If the pin is a HAL_OUT pin, the button acts as a toggle button and the pin state is toggled with each click.
# A HAL pin to enable/disable the button can be created.  One use is to connect it to halui.machine.is-on to enable a button
# when the machine is on and disable it when the machine is off.
# There is also a HAL IN pin for the light, which operates independently of the button state.
#
# If has_hal_pins is true, the button creates several pins with the base name being the name of the widget
#    widgetname-button        OUT or IO   - indicates button state
#    widgetname-button-not    OUT         - only created if 'button_halio' is NOT true, inverse of widgetname-button
#    widgetname-enable        IN          - only created if 'create_enable_pin' is true, allows button to be enabled and disabled by a HAL signal
#    widgetname-light         IN          - controls light on/off
#
# If 'has_hal_pins' is false, no hal pins are created and the button can be completely controlled in code.  It emits the 'clicked'
# signal when pressed.  You can also leave 'has_hal_pins' true, and connect the -light pin to a HAL signal while handling the
# button 'clicked' signal in code.  For this use you want to make sure 'button_halio_pin' is false or the button will remain 'active'
# after the first click, and calls to get_active() will always return True.

if __name__ == "__main__":
    from hal_widgets import _HalWidgetBase, hal, hal_pin_changed_signal
else:
    from .hal_widgets import _HalWidgetBase, hal, hal_pin_changed_signal

clicked_signal = ('clicked', (GObject.SignalFlags.RUN_FIRST, GObject.TYPE_NONE, (GObject.TYPE_OBJECT,)))

class HAL_LightButton(Gtk.DrawingArea, _HalWidgetBase):
    __gtype_name__ = 'HAL_LightButton'
    __gsignals__ = dict([clicked_signal])
    __gproperties__ = {
        'has_hal_pins' : ( GObject.TYPE_BOOLEAN, 'Has HAL pins', 'Set false if this button will not be controlled using HAL',
                    True, GObject.ParamFlags.READWRITE | GObject.ParamFlags.CONSTRUCT),
        'is_momentary' : ( GObject.TYPE_BOOLEAN, 'Is momentary',
                        'Set True if this button will be momentary rather then toggle',
                    False, GObject.ParamFlags.READWRITE | GObject.ParamFlags.CONSTRUCT),
        'button_halio_pin' : ( GObject.TYPE_BOOLEAN, 'Button pin is HAL_IO', 'If HAL_IO, pin is set true on button press; if HAL_OUT, pin state is toggled by button press',
                    True, GObject.ParamFlags.READWRITE | GObject.ParamFlags.CONSTRUCT),
        'create_enable_pin' : ( GObject.TYPE_BOOLEAN, 'Create enable pin', 'Creates an enable pin which enables the button if True, and disables when False',
                    False, GObject.ParamFlags.READWRITE | GObject.ParamFlags.CONSTRUCT),
        'dual_color' : ( GObject.TYPE_BOOLEAN, 'Dual Color Light', 'If true, light is on always, but changes color for OFF state',
                    False, GObject.ParamFlags.READWRITE | GObject.ParamFlags.CONSTRUCT),
        #'use_bitmaps' : ( GObject.TYPE_BOOLEAN, 'Use Bitmaps', 'If true, you must select bitmaps for each button state',
        #            False, GObject.ParamFlags.READWRITE | GObject.ParamFlags.CONSTRUCT),
        'light_is_on' : ( GObject.TYPE_BOOLEAN, 'Light is on', 'Turns light on - for testing in glade',
                    False, GObject.ParamFlags.READWRITE | GObject.ParamFlags.CONSTRUCT),
        'light_on_color' : ( Gdk.Color.__gtype__, 'Light ON color', "Set color for light ON state",
                        GObject.ParamFlags.READWRITE),
        'light_off_color' : ( Gdk.Color.__gtype__, 'Light OFF color', "Set color for light OFF state",
                        GObject.ParamFlags.READWRITE),
        'border_width' : ( GObject.TYPE_INT, 'Border width', 'Number of pixels extra border around label',
                    0, 50, 6, GObject.ParamFlags.READWRITE | GObject.ParamFlags.CONSTRUCT),
        'corner_radius' : ( GObject.TYPE_INT, 'Corner radius', 'Radius of the button corners',
                    1, 20, 4, GObject.ParamFlags.READWRITE | GObject.ParamFlags.CONSTRUCT),
        'button_text' : ( GObject.TYPE_STRING, 'Button default/off text', 'Text shown when light is off, or all the time if \"Button ON text\" is blank',
                    "Button", GObject.ParamFlags.READWRITE|GObject.ParamFlags.CONSTRUCT),
        'button_on_text' : ( GObject.TYPE_STRING, 'Button ON text', 'If not blank, this text will be shown on the button when light is on',
                    "", GObject.ParamFlags.READWRITE|GObject.ParamFlags.CONSTRUCT),
        'font_on_color' : ( Gdk.Color.__gtype__, 'Font ON color', "Set color for button text when light is ON",
                        GObject.ParamFlags.READWRITE),
        'font_off_color' : ( Gdk.Color.__gtype__, 'Font OFF color', "Set color for button text when light is OFF",
                        GObject.ParamFlags.READWRITE),
        'font_face' : ( GObject.TYPE_STRING, 'Font name', 'Button text',
                    "Sans", GObject.ParamFlags.READWRITE|GObject.ParamFlags.CONSTRUCT),
        'font_bold' : ( GObject.TYPE_BOOLEAN, 'Bold font', 'Set button font to bold',
                    False, GObject.ParamFlags.READWRITE | GObject.ParamFlags.CONSTRUCT),
        'font_size' : ( GObject.TYPE_INT, 'Font size', 'Number of pixels extra border around label',
                    0, 100, 10, GObject.ParamFlags.READWRITE | GObject.ParamFlags.CONSTRUCT),
    }
    __gproperties = __gproperties__
    _size_request = (35, 35)

    def __init__(self):
        super(HAL_LightButton, self).__init__()

        self.has_hal_pins = True
        
        # When True, button_pin is HAL_IO.
        # Pressing the button sets the pin True, and it must be reset to False externally
        # When False, button_pin is HAL_OUT, and it's state is toggled by a button press,
        # which means this button becomes a togglebutton.
        self.button_halio_pin = True
        
        self.dual_color = False
        self.use_bitmaps = False  #this feature is not implemented yet
        self.light_on_color = Gdk.Color.parse('green')
        self.light_off_color = Gdk.Color.parse('gray')
        self.border_width = 6
        self.corner_radius = 4
        self.button_text = 'Button'
        self.button_on_text = ''
        self.font_face = 'Sans'
        self.font_bold = False
        self.font_size = 10
        self.font_on_color = Gdk.Color.parse('black')
        self.font_off_color = Gdk.Color.parse('black')
        self.create_enable_pin = False
        
        self.active = False
        self.light_is_on = False
        self.mouseover = False
        
        self.default_pangolayout = self.create_pango_layout(self.button_text)
        self.on_pangolayout = self.create_pango_layout(self.button_on_text)
        
        self.set_size_request(*self._size_request)
        self.set_events( Gdk.EventMask.EXPOSURE_MASK
                       | Gdk.EventMask.ENTER_NOTIFY_MASK
                       | Gdk.EventMask.LEAVE_NOTIFY_MASK
                       | Gdk.EventMask.BUTTON_PRESS_MASK
                       | Gdk.EventMask.BUTTON_RELEASE_MASK
                       | Gdk.EventMask.POINTER_MOTION_MASK
                       | Gdk.EventMask.POINTER_MOTION_HINT_MASK)
 
        self.connect("draw", self.expose)
        #self.connect('button-press-event', self.pressed)
        #self.connect('button-release-event', self.released)
        #self.connect('state-changed', self._on_state_changed)
        
    def do_enter_notify_event(self, event):
        self.mouseover = True
        self.queue_draw()
    def do_leave_notify_event(self, event):
        self.mouseover = False
        self.queue_draw()
        
    def do_button_press_event(self, event):
        if event.button == 1:
            if self.is_momentary:
                active = True
            else:
                active = not self.active

            if (self.has_hal_pins):
                if (self.button_halio_pin):
                    self.set_active(True)
                    try:
                        self.button_pin.set(True)
                    except:
                        pass
                else:
                    self.set_active(active)
                    try:
                        self.button_pin.set(active)
                        self.button_pin_not.set(not active)
                    except:
                        pass

            else:
                self.set_active(active)

            self.emit("clicked", self)
        return True

    def do_button_release_event(self, event):
        if event.button == 1:
            if not self.is_momentary:
                return True
            if (self.has_hal_pins):
                    self.set_active(False)
                    try:
                        self.button_pin.set(False)
                        self.button_pin_not.set(True)
                    except:
                        pass

            else:
                self.set_active(False)

            self.emit("clicked", self)
        return True


    def expose(self, widget, event):
        if self.is_sensitive():
            alpha = 1
        else:
            alpha = 0.3

        w = self.get_allocated_width()
        h = self.get_allocated_height()

        cr = widget.get_property('window').cairo_create()
        def set_color(c):
            return cr.set_source_rgba(c.red_float, c.green_float, c.blue_float, alpha)

        if (self.use_bitmaps == False):
            linewidth = 4
            x = y = linewidth/2
            w2 = w - linewidth
            h2 = h - linewidth
            x_center = x + w2/2
            y_center = y + h2/2
            degrees = math.pi / 180.0
            
            cr.new_sub_path()
            cr.arc(x + w2 - self.corner_radius, y + self.corner_radius, self.corner_radius, -90 * degrees, 0 * degrees)
            cr.arc(x + w2 - self.corner_radius, y + h2 - self.corner_radius, self.corner_radius, 0 * degrees, 90 * degrees)
            cr.arc(x + self.corner_radius, y + h2 - self.corner_radius, self.corner_radius, 90 * degrees, 180 * degrees)
            cr.arc(x + self.corner_radius, y + self.corner_radius, self.corner_radius, 180 * degrees, 270 * degrees)
            cr.close_path()
            
            if (self.light_is_on):
                color = self.light_on_color
                if self.mouseover:
                    color = Gdk.color_from_hsv(color.hue, color.saturation * .5, color.value * 1.5)
                color2 = Gdk.color_from_hsv(color.hue, color.saturation * .25, color.value)
                linecolor = Gdk.color_from_hsv(color.hue, color.saturation, color.value * .95)
            else:
                color = self.light_off_color
                if self.mouseover:
                    color = Gdk.color_from_hsv(color.hue, color.saturation * .5, color.value * 1.5)
                if (self.dual_color):
                    color2 = Gdk.color_from_hsv(color.hue, color.saturation * .25, color.value)
                    linecolor = Gdk.color_from_hsv(color.hue, color.saturation, color.value * .95)
                else:
                    color1 = Gdk.color_from_hsv(color.hue, color.saturation * .50, color.value * 2)
                    color2 = Gdk.color_from_hsv(color.hue, color.saturation, color.value * .50)
                    linecolor = Gdk.color_from_hsv(color.hue, color.saturation * .50, color.value * .75)
            
            cr.set_line_width(linewidth)
            set_color(linecolor)
            cr.stroke_preserve()
            
            if (self.light_is_on == True) or (self.dual_color == True):
                gradient_radius = (w2 + h2)
                g1 = cairo.RadialGradient(x_center, y_center, 0, x_center, y_center, gradient_radius)
                g1.add_color_stop_rgb(0.0, color2.red_float, color2.green_float, color2.blue_float)
                g1.add_color_stop_rgb(1.0, color.red_float, color.green_float, color.blue_float)
            else:
                g1 = cairo.LinearGradient(x, y, x, y + h2)
                g1.add_color_stop_rgb(0.0, color1.red_float, color1.green_float, color1.blue_float)
                g1.add_color_stop_rgb(0.0, color.red_float, color.green_float, color.blue_float)
                g1.add_color_stop_rgb(1.0, color2.red_float, color2.green_float, color2.blue_float)
            cr.set_source(g1)
            cr.fill()
        
        #Using bitmaps is not implemented. Bitmaps should not be scaled unless you figure out a way to make them scale nicely
        #The main reason to use bitmaps would be if someone wants a different look from the default one
        #The code below is the basic way to do it.  update_widget_size() should use the button size instead of text size
        #and there should be properties to set image filenames for each state (light on, light off, mouseover, etc)
        else:
            cr.save()
            image = cairo.ImageSurface.create_from_png('resources/k_green.png')
            img_w = image.get_width()
            img_h = image.get_height()
            print(float(w)/img_w, float(h)/img_h)
            cr.set_source_surface(image, 0, 0)
            cr.paint()
            cr.restore()

        # write text
        _layout = self.default_pangolayout
        if (self.light_is_on):
            set_color(self.font_on_color)
            if (not self.button_on_text == ""):
                _layout = self.on_pangolayout
        else:
            set_color(self.font_off_color)
 
        fontw, fonth = _layout.get_pixel_size()
        cr.move_to((w - fontw)/2, (h - fonth)/2)
        cr.update_layout(_layout)
        cr.show_layout(_layout)
        return False
    
    def update_font(self):
        if self.font_bold == True:
            fontweight = "bold"
        else:
            fontweight = ""
        self.default_pangolayout.set_font_description(Pango.FontDescription(self.font_face + ' ' + fontweight + ' ' + str(self.font_size)))
        self.on_pangolayout.set_font_description(Pango.FontDescription(self.font_face + ' ' + fontweight + ' ' + str(self.font_size)))
        self.update_widget_size()
    
    def update_widget_size(self):
        w1, h1 = self.default_pangolayout.get_pixel_size()
        w2, h2 = self.on_pangolayout.get_pixel_size()
        width = max(w1 + self.border_width*2, w2 + self.border_width*2)
        height = max(h1 + self.border_width*2, h2 + self.border_width*2)
        self.set_size_request(int(width), int(height))
        
    # These set_* functions are called by the hal pin callbacks.
    # Set self.has_hal_pins = False if you want to control state with these functions directly
    #**************************************************************
    
    def set_active(self, active):
        self.active = active
        self.queue_draw()
    def get_active(self):
        return self.active
    
    def set_light_on(self, state):
        self.light_is_on = state
        self.queue_draw()
    def get_light_on(self):
        return self.light_is_on
        
    def set_text(self, text):
        self.button_text = text
        self.default_pangolayout.set_text(text)
        self.update_widget_size()
        
    def set_on_text(self, text):
        self.button_on_text = text
        self.on_pangolayout.set_text(text)
        self.update_widget_size()

    #**************************************************************

    def do_get_property(self, property):
        name = property.name.replace('-', '_')
        if name in list(self.__gproperties.keys()):
            return getattr(self, name)
        else:
            raise AttributeError('unknown property %s' % property.name)

    def do_set_property(self, property, value):
        name = property.name.replace('-', '_')
        if name == 'button_text':
            self.set_text(value)
        elif name == 'button_on_text':
            self.set_on_text(value)
        elif name in list(self.__gproperties.keys()):
            setattr(self, name, value)
            if name in ['font_face', 'font_bold', 'font_size']:
                self.update_font()
            if name == 'border_width':
                self.update_widget_size()
            self.queue_draw()
        else:
            raise AttributeError('unknown property %s' % property.name)
        return True

    def _hal_init(self):
        if (self.has_hal_pins):
            _HalWidgetBase._hal_init(self)

            self.set_active(False)
            if (self.button_halio_pin):
                self.button_pin = self.hal.newpin(self.hal_name+'-button', hal.HAL_BIT, hal.HAL_IO)
                self.button_pin.connect('value-changed', self.button_pin_update)
            else:
                self.button_pin = self.hal.newpin(self.hal_name+'-button', hal.HAL_BIT, hal.HAL_OUT)
                self.button_pin_not = self.hal.newpin(self.hal_name+'-button-not', hal.HAL_BIT, hal.HAL_OUT)
            if (self.create_enable_pin):
                self.enable_pin = self.hal.newpin(self.hal_name+'-enable', hal.HAL_BIT, hal.HAL_IN)
                self.enable_pin.connect('value-changed', self.enable_pin_update)

            self.light_pin = self.hal.newpin(self.hal_name+'-light', hal.HAL_BIT, hal.HAL_IN)
            self.light_pin.connect('value-changed', self.light_pin_update)
            
    def button_pin_update(self, hal_pin, data=None):
        self.set_active(bool(self.button_pin.get()))
    def enable_pin_update(self, hal_pin, data=None):
        self.set_sensitive(hal_pin.get())
    def light_pin_update(self, hal_pin, data=None):
        self.set_light_on(bool(self.light_pin.get()))
    
GObject.type_register(HAL_LightButton)

