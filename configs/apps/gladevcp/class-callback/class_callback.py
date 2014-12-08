import hal
import glib
import time

from inspect import stack

class OtherClass:
    '''
    methods in this class are not exposed as callback handlers
    since the object instance is not passed to gladevcp in get_handlers()
    '''
    def invisible(selfself):
        pass


class HandlerClass:
    '''
    class with gladevcp callback handlers
    '''

    def on_button_press(self,widget,data=None):
        '''
        a callback method
        parameters are:
            the generating object instance, likte a GtkButton instance
            user data passed if any - this is currently unused but
            the convention should be retained just in case
        '''
        self.nhits += 1
        laptime = time.time() - self.start
        self.builder.get_object('message').set_label("time lap#%d: %.2f seconds" % (self.nhits,laptime))

    def _on_timer_tick(self,userdata=None):
        '''
        the full glib functionality is available if needed.
        here's a timer function which will be called periodically
        returning True restarts the timer
        returning False makes it a one-shot
        '''
        self.ticks += 1
        self.halcomp['value'] = self.ticks
        return True


    def __init__(self, halcomp,builder,useropts,compname):
        '''
        Handler classes are instantiated in the following state:
        - the widget tree is created, but not yet realized (no toplevel window.show() executed yet)
        - the halcomp HAL component is set up and the widhget tree's HAL pins have already been added to it
        - it is safe to add more hal pins because halcomp.ready() has not yet been called at this point.

        after all handlers are instantiated in command line and get_handlers() order, callbacks will be
        connected with connect_signals()/signal_autoconnect()

        The builder may be either of libglade or GtkBuilder type depending on the glade file format.
        '''

        self.halcomp = halcomp
        self.builder = builder

        self.halcomp.newpin("value", hal.HAL_FLOAT, hal.HAL_OUT)
        self.start = time.time()
        self.nhits = 0
        self.ticks = 0

        # demonstrate a slow background timer - granularity is one second
        # for a faster timer, use this:
        # glib.timeout_add(5000,  self._on_timer_tick)
        glib.timeout_add_seconds(1, self._on_timer_tick)




other = OtherClass()  # executed at import time


def get_handlers(halcomp,builder,useropts,compname):
    '''
    this function is called by gladevcp at import time (when this module is passed with '-u <modname>.py')

    return a list of object instances whose methods should be connected as callback handlers
    any method whose name does not begin with an underscore ('_') is a  callback candidate

    the 'get_handlers' name is reserved - gladevcp expects it, so do not change
    '''
    return [HandlerClass(halcomp,builder,useropts,compname)]
