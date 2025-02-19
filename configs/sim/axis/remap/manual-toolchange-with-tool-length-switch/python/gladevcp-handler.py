#!/usr/bin/env python3

import hal

class HandlerClass:

    def on_led_change(self,hal_led,data=None):
        '''
        the gladevcp.change led had a transition
        '''
        if hal_led.hal_pin.get():
            if self.halcomp["number"] > 0.0:
                self.change_text.set_label("Insert tool number %d" % (int(self.halcomp["number"])))
            else:
                self.change_text.set_label("Remove tool")
        else:
            self.change_text.set_label("No change requested")         

    def __init__(self, halcomp,builder,useropts):
        self.halcomp = halcomp
        self.change_text = builder.get_object("change-text")
        self.halcomp.newpin("number", hal.HAL_FLOAT, hal.HAL_IN)

def get_handlers(halcomp,builder,useropts):
    return [HandlerClass(halcomp,builder,useropts)]
