import cairo
from math import pi


pngfile = 'vortex.me.png'

class HandlerClass:

    def on_draw(self, widget, cr):
        print("on_draw")

        # Sets the operator to clear which deletes everything below
        # where an object is drawn
        cr.set_operator(cairo.OPERATOR_CLEAR)
        width = widget.get_allocated_width()
        height = widget.get_allocated_height()
        # Makes the mask fill the entire window
        cr.rectangle(0.0, 0.0, width, height)
        # Deletes everything in the window (since the compositing
        # operator is clear and mask fills the entire window
        cr.fill()
        # Set the compositing operator back to the default
        cr.set_operator(cairo.OPERATOR_OVER)

        if self.scale:
            w, h = widget.get_allocated_width(), widget.get_allocated_height()
            cr.scale(1.0 *w / self.width, 1.0*h/self.height)

        cr.set_source_surface(self.img, 0, 0)
        cr.paint()



    def __init__(self, halcomp,builder,useropts):

        self.builder = builder
        win = self.builder.get_object("window1")

        #win.set_decorated(False)

        self.img = cairo.ImageSurface.create_from_png(pngfile)
        self.width = self.img.get_width()
        self.height = self.img.get_height()

        # Makes the window paintable, so we can draw directly on it
        win.set_app_paintable(True)
        win.set_size_request(self.width, self.height)

        # This sets the windows colormap, so it supports transparency.
        # This will only work if the wm support alpha channel
        screen = win.get_screen()
        rgba = screen.get_rgba_visual()
        win.set_visual(rgba)

        # scaling the bitmap is possible by turning this on
        # however the fixed widget does NOT do proportional scaling
        self.scale = 1

def get_handlers(halcomp,builder,useropts):

    return [HandlerClass(halcomp,builder,useropts)]
