import hal

# Import core libraries
from gladevcp.core import Info, Action

# Instantiate libraries
INFO = Info()
ACTION = Action()

def get_handlers(halcomp,builder,useropts):
    return [HandlerClass(halcomp,builder,useropts)]

class HandlerClass:
    '''
    class with gladevcp callback handlers
    '''

    # general button callback function with some sample code
    def on_button_press(self, widget, data=None):
        print(widget, data)
        print('Max Feed Override INI setting:',INFO.MAX_FEED_OVERRIDE)
        ACTION.SET_DISPLAY_MESSAGE('Handler defined button Pressed')

    def __init__(self, halcomp,builder,useropts):
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

