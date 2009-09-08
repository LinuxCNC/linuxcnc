#!/usr/bin/python

import gtk
import gtk.gtkgl.widget
import gtk.gdkgl
import gtk.gdk

import glnav
import gobject
import pango

import rs274.glcanon
import rs274.interpret
import emc

import time

import tempfile
import shutil
import os

import thread

from minigl import *
import pango
import cairo
import pangocairo

def use_font(fontDesc, start, count, base):
    surface = cairo.ImageSurface(cairo.FORMAT_A8, 256, 256)
    context = pangocairo.CairoContext(cairo.Context(surface))
    layout = context.create_layout()
    fontmap = pangocairo.cairo_font_map_get_default()
    font = fontmap.load_font(fontmap.create_context(), fontDesc)
    layout.set_font_description(fontDesc)
    metrics = font.get_metrics()
    descent = metrics.get_descent()
    d = pango.PIXELS(descent)
    linespace = metrics.get_ascent() + metrics.get_descent()
    width = metrics.get_approximate_char_width()

    glPushClientAttrib(GL_CLIENT_PIXEL_STORE_BIT)
    glPixelStorei(GL_UNPACK_SWAP_BYTES, 0)
    glPixelStorei(GL_UNPACK_LSB_FIRST, 1)
    glPixelStorei(GL_UNPACK_ROW_LENGTH, 256)
    glPixelStorei(GL_UNPACK_IMAGE_HEIGHT, 256)
    glPixelStorei(GL_UNPACK_SKIP_PIXELS, 0)
    glPixelStorei(GL_UNPACK_SKIP_ROWS, 0)
    glPixelStorei(GL_UNPACK_SKIP_IMAGES, 0)
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1)
    glPixelZoom(1, -1)

    for i in range(count):
        ch = unichr(start+i)
        layout.set_text(ch)
        w, h = layout.get_size()
        context.save()
        context.new_path()
        context.rectangle(0, 0, 256, 256)
        context.set_source_rgba(0., 0., 0., 0.)
        context.set_operator (cairo.OPERATOR_SOURCE);
        context.paint()
        context.restore()

        context.save()
        context.set_source_rgba(1., 1., 1., 1.)
        context.set_operator (cairo.OPERATOR_SOURCE);
        context.move_to(0, 0)
        context.update_layout(layout)
        context.show_layout(layout)
        context.restore()

        w, h = pango.PIXELS(w), pango.PIXELS(h)
        glNewList(base+i, GL_COMPILE)
        glBitmap(0, 0, 0, 0, 0, h-d, '');
        if w and h: glDrawPixels(w, h, GL_LUMINANCE, GL_UNSIGNED_BYTE, surface.get_data())
        glBitmap(0, 0, 0, 0, w, -h+d, '');
        glEndList()

    glPopClientAttrib()
    return pango.PIXELS(width), pango.PIXELS(linespace)

class DummyProgress:
    def nextphase(self, unused): pass
    def progress(self): pass

class StatCanon(rs274.glcanon.GLCanon, rs274.interpret.StatMixin):
    def __init__(self, colors, geometry, stat):
        rs274.glcanon.GLCanon.__init__(self, colors, geometry)
        rs274.interpret.StatMixin.__init__(self, stat)
        self.progress = DummyProgress()

def W(p, k, *args, **kw):
    w = k(*args, **kw)
    w.show()
    p.add(w)
    return w

class Gremlin(gtk.gtkgl.widget.DrawingArea, glnav.GlNavBase,
              rs274.glcanon.GlCanonDraw):
    rotation_vectors = [(1.,0.,0.), (0., 0., 1.)]

    def __init__(self, inifile):

        display_mode = ( gtk.gdkgl.MODE_RGB | gtk.gdkgl.MODE_DEPTH |
                         gtk.gdkgl.MODE_DOUBLE )
        glconfig = gtk.gdkgl.Config(mode=display_mode)

        gtk.gtkgl.widget.DrawingArea.__init__(self, glconfig)
        glnav.GlNavBase.__init__(self)
        def C(s):
            a = self.colors[s + "_alpha"]
            s = self.colors[s]
            return [int(x * 255) for x in s + (a,)]


        self.logger = emc.positionlogger(emc.stat(),
            C('backplotjog'),
            C('backplottraverse'),
            C('backplotfeed'),
            C('backplotarc'),
            C('backplottoolchange'),
            C('backplotprobing'),
            self.get_geometry()
        )
        thread.start_new_thread(self.logger.start, (.01,))

        rs274.glcanon.GlCanonDraw.__init__(self, emc.stat(), self.logger)
        self.inifile = inifile

        self.select_primed = None

        self.connect_after('realize', self.realize)
        self.connect('configure_event', self.reshape)
        self.connect('map_event', self.map)
        self.connect('expose_event', self.expose)

        self.connect('motion-notify-event', self.motion)
        self.connect('button-press-event', self.pressed)
        self.connect('button-release-event', self.select_fire)
        self.connect('scroll-event', self.scroll)

        self.add_events(gtk.gdk.POINTER_MOTION_MASK)
        self.add_events(gtk.gdk.POINTER_MOTION_HINT_MASK)
        self.add_events(gtk.gdk.BUTTON_MOTION_MASK)
        self.add_events(gtk.gdk.BUTTON_PRESS_MASK)
        self.add_events(gtk.gdk.BUTTON_RELEASE_MASK)

        self.fingerprint = ()

        self.lat = 0
        self.minlat = -90
        self.maxlat = 90

        self.highlight_line = None

	self.a_axis_wrapped = inifile.find("AXIS_3", "WRAPPED_ROTARY")
	self.b_axis_wrapped = inifile.find("AXIS_4", "WRAPPED_ROTARY")
	self.c_axis_wrapped = inifile.find("AXIS_5", "WRAPPED_ROTARY")

	live_axis_count = 0
	for i,j in enumerate("XYZABCUVW"):
	    if self.stat.axis_mask & (1<<i) == 0: continue
	    live_axis_count += 1
	self.num_joints = int(inifile.find("TRAJ", "JOINTS") or live_axis_count)

    def activate(self):
        glcontext = gtk.gtkgl.widget_get_gl_context(self)
        gldrawable = gtk.gtkgl.widget_get_gl_drawable(self)

        return gldrawable and glcontext and gldrawable.gl_begin(glcontext)

    def swapbuffers(self):
        gldrawable = gtk.gtkgl.widget_get_gl_drawable(self)
        gldrawable.swap_buffers()

    def deactivate(self):
        gldrawable = gtk.gtkgl.widget_get_gl_drawable(self)
        gldrawable.gl_end()

    def winfo_width(self):
        return self.width

    def winfo_height(self):
        return self.height

    def reshape(self, widget, event):
        self.width = event.width
        self.height = event.height

    def expose(self, widget=None, event=None):
        if not self.initialised: return
        if self.perspective: self.redraw_perspective()
        else: self.redraw_ortho()

        return True

    def _redraw(self): self.expose()

    def map(self, *args):
        gobject.idle_add(self.poll)

    def poll(self):
        time.sleep(.02)
        s = self.stat
        s.poll()
        fingerprint = (self.logger.npts, self.soft_limits(),
            s.actual_position, s.joint_actual_position,
            s.homed, s.origin, s.limit, s.tool_in_spindle,
            s.motion_mode, s.current_vel)

        if fingerprint != self.fingerprint:
            self.fingerprint = fingerprint
            self.expose()

        # return self.visible
        return True

    @rs274.glcanon.with_context
    def realize(self, widget):
        self.set_view_z()
        s = self.stat
        s.poll()
        if s.file: self.load(s.file)

        fontDesc = pango.FontDescription('courier bold 16')
        self.font_base = glGenLists(128)
        width, linespace = use_font(fontDesc, 0, 128, self.font_base)
        self.font_linespace = linespace
        self.font_charwidth = width
        rs274.glcanon.GlCanonDraw.realize(self)

    def load(self, filename):
        s = self.stat
        s.poll()

        td = tempfile.mkdtemp()
        try:
            canon = StatCanon(self.colors, self.get_geometry(), s)
            parameter = self.inifile.find("RS274NGC", "PARAMETER_FILE")
            temp_parameter = os.path.join(td, os.path.basename(parameter))
            shutil.copy(parameter, temp_parameter)
            canon.parameter_file = temp_parameter

            unitcode = "G%d" % (20 + (s.linear_units == 1))
            initcode = self.inifile.find("RS274NGC", "RS274NGC_STARTUP_CODE") or ""
            self.load_preview(filename, canon, unitcode, initcode)
        finally:
            shutil.rmtree(td)

        self.set_view_z()

    def get_num_joints(self): return self.num_joints
    def get_geometry(self): return 'XYZ'
    def get_joints_mode(self): return False
    def get_show_commanded(self): return True
    def get_show_extents(self): return True
    def get_show_limits(self): return True
    def get_show_live_plot(self): return True
    def get_show_machine_speed(self): return True
    def get_show_metric(self): return True
    def get_show_program(self): return True
    def get_show_rapids(self): return True
    def get_show_relative(self): return True
    def get_show_tool(self): return True
    def get_show_distance_to_go(self): return True
    def get_view(self): return 'z'
    def is_lathe(self): return False
    def get_current_tool(self):
        for i in self.stat.tool_table:
            if i[0] == self.stat.tool_in_spindle:
                return i
    def get_highlight_line(self): return self.highlight_line

    def get_a_axis_wrapped(self): return self.a_axis_wrapped
    def get_b_axis_wrapped(self): return self.b_axis_wrapped
    def get_c_axis_wrapped(self): return self.c_axis_wrapped

    def get_font_info(self):
        return self.font_charwidth, self.font_linespace, self.font_base

    def select_prime(self, x, y):
        self.select_primed = x, y

    @rs274.glcanon.with_context
    def select_fire(self, widget, event):
        if not self.select_primed: return
        x, y = self.select_primed
        self.select_primed = None
        self.select(x, y)

    def select_cancel(self, widget=None, event=None):
        self.select_primed = None

    def pressed(self, widget, event):
        button1 = event.button == 1
        button2 = event.button == 2
        button3 = event.button == 3
        if button1:
            self.select_prime(event.x, event.y)
            self.recordMouse(event.x, event.y)
        elif button2:
            self.recordMouse(event.x, event.y)
        elif button3:
            self.startZoom(event.y)

    def motion(self, widget, event):
        button1 = event.state & gtk.gdk.BUTTON1_MASK
        button2 = event.state & gtk.gdk.BUTTON2_MASK
        button3 = event.state & gtk.gdk.BUTTON3_MASK
        shift = event.state & gtk.gdk.SHIFT_MASK
        if button1 and self.select_primed:
            x, y = self.select_primed
            distance = max(abs(event.x - x), abs(event.y - y))
            if distance > 8: self.select_cancel()
        if button1 and not self.select_primed:
            if shift:
                self.translateOrRotate(event.x, event.y)
            else:
                self.rotateOrTranslate(event.x, event.y)
        elif button2:
            self.translateOrRotate(event.x, event.y)
        elif button3:
            self.continueZoom(event.y)

    def scroll(self, widget, event):
        if event.direction == gtk.gdk.SCROLL_UP: self.zoomin()
        elif event.direction == gtk.gdk.SCROLL_DOWN: self.zoomout()

class GremlinApp(gtk.Window):
    def __init__(self, inifile):
        inifile = emc.ini(inifile)
        gtk.Window.__init__(self)

        self.vbox = W(self, gtk.VBox)
        self.gremlin = W(self.vbox, Gremlin, inifile)
        self.gremlin.set_size_request(400, 400)

        self.connect("destroy", self.quit)

        self.show()
    def quit(self, event):
        gtk.main_quit()

def main():
    from sys import argv
    g = GremlinApp(argv[1])
    gtk.main()

if __name__ == '__main__': raise SystemExit, main()
