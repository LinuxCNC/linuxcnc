import gtk
import gobject
import cairo
import math
import gtk.glade

# This creates the custom LED widget


class HAL_LED(gtk.DrawingArea):
    __gtype_name__ = 'HAL_LED'
    __HAL_LED_RED__ = 0
    __HAL_LED_BLUE__ = 1
    __HAL_LED_GREEN__ = 2
    __qwerty__ = 2
    def post_create(self, obj, reason):
                print "\nhola\n"
    def is_frobnicator(self):
        print "hi"
    def qwerty(self):
        pass

    def __init__(self):
        print "LED init"
        super(HAL_LED, self).__init__()
        self.qwerty = 5
        self._dia = 10
        self._state = 0
        self._shape = 1
        self._blink_active = False
        self._blink_state = False
        self._blink_rate = 500
        self.is_frobnicator = 0
        self._on_color = [0.3, 0.4, 0.6]
        self._off_color = [0.9, 0.1, 0.1]
        self.set_size_request(25, 25)
        self.connect("expose-event", self.expose)

    def qwerty(self):
        pass
    # This method draws our widget
    # depending on self.state, self.blink_active, self.blink_state and the sensitive state of the parent
    # sets the fill as the on or off colour.
    def expose(self, widget, event):
        cr = widget.window.cairo_create()
        sensitive = self.flags() & gtk.PARENT_SENSITIVE
        if not sensitive: alpha = .3
        else: alpha = 1
        cr.set_line_width(3)
        cr.set_source_rgba(0, 0, 0.0,alpha)
        # square led
        if self._shape == 2:
            self.set_size_request(self._dia*2+5, self._dia*2+5)
            w = self.allocation.width
            h = self.allocation.height
            cr.translate(w/2, h/2)
            cr.rectangle(-self._dia, -self._dia, self._dia*2,self._dia*2)
        # oval led
        elif self._shape == 1:
            self.set_size_request(self._dia*2+5, self._dia*2+5)
            w = self.allocation.width
            h = self.allocation.height
            cr.translate(w/2, h/2)
            cr.scale( 1, 0.7);
            cr.arc(0, 0, self._dia, 0, 2*math.pi)
        # round led
        else:            
            self.set_size_request(self._dia*2+5, self._dia*2+5)           
            w = self.allocation.width
            h = self.allocation.height
            cr.translate(w/2, h/2)
            lg2 = cairo.RadialGradient(0, 0, 0,  0, 0, self._dia*2)
            if self._state == True:
                r = self._on_color[0]
                g = self._on_color[1]
                b = self._on_color[2]
            else:
                r = self._off_color[0]
                g = self._off_color[1]
                b = self._off_color[2]
            lg2.add_color_stop_rgba(1, r/.25,g/.25,b/.25,1)
            lg2.add_color_stop_rgba(.5, r,g,b, .5)
            cr.arc(0, 0, self._dia, 0, 2*math.pi)

        cr.stroke_preserve()        
        if self._state == True:
            if self._blink_active == False or self._blink_active == True and self._blink_state == True:
                cr.set_source_rgba(self._on_color[0],self._on_color[1],self._on_color[2],alpha)
            elif self._blink_state == False:
                cr.set_source_rgba(self._off_color[0],self._off_color[1],self._off_color[2],alpha)
        else:
            cr.set_source_rgba(self._off_color[0],self._off_color[1],self._off_color[2],alpha)
        cr.fill()    
        return False
      
    # This sets the LED on or off color
    # and then redraws it
    # Usage: ledname.set_active(True) 
    def set_active(self, data ):
        self._state = data
        self.queue_draw()

    def set_sensitive(self, data ):
        print data

    def set_blink_active(self, data):
        self.set_blink_rate(self._blink_rate)
        if data == True:
            self._blink_active = True
            self.set_blink_rate(self._blink_rate)
        else:
            print "LED's that blink, cannot be made unblinked at runtime"
            return

    #FIXME the gobject timers are never explicly destroyed
    def set_blink_rate(self,rate):
        self._blink_timer = gobject.timeout_add(rate, self.blink)

    def blink(self):
        if self._blink_state == True:
            self._blink_state = False
        else: self._blink_state = True
        return True # keep running this event

    # This allows setting of the on and off colour
    # red,green and blue are float numbers beteen 0 and 1
    # if color = None uses colorname. only a few names supported
    # Usage: ledname.set_color("off",[r,g,b],"colorname")

    def set_color(self, state, color = [0,0,0],colorname = "black"):
        if color == None:
            if colorname =="black": 
                color = [0,0,0]
            elif colorname =="blue": 
                color = [0,0,1]
            elif colorname =="green": 
                color = [0,1,0]
            elif colorname =="red": 
                color = [.9,0,.2]
            elif colorname =="yellow": 
                color = [1,1,0]
            elif colorname =="white": 
                color = [1,1,1]
            elif colorname =="dark": 
                r = self._on_color[0] *.4
                g = self._on_color[1] *.4
                b = self._on_color[2] *.4
                color = [r,g,b]
            else:
                color = [1,1,0]
        if state == "off":
            self._off_color = color
        elif state == "on":
            self._on_color = color
        else:
            return

    # This alows setting the diameter of the LED
    # Usage: ledname.set_dia(10)
    def set_dia(self, dia):
        self._dia = dia
        self.queue_draw()

    # This sets the shape round oval or square
    def set_shape(self, shape):
        self._shape = shape
        self.queue_draw()
