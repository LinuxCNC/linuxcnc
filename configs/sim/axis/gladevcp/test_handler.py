import hal


def get_handlers(halcomp,builder,useropts):
    return [HandlerClass(halcomp,builder,useropts)]

class HandlerClass:
    '''
    class with gladevcp callback handlers
    '''

    def on_button_press(self, widget, data=None):
        print(widget, data)
        
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

