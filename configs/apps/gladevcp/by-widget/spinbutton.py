'''
    HAL spinbutton
    demo for Gtk bug workaround
    see Gladevcp Manual Section FAQ item 6
'''

class HandlerClass:

    def on_spinbutton_realize(self,widget,data=None):
        widget.set_value(3.14)


def get_handlers(halcomp,builder,useropts,compname):

    return [HandlerClass()]

