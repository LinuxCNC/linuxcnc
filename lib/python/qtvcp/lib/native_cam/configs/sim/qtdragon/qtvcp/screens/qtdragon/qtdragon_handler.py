from qtvcp.core import Path
path = Path()
HandlerClass = path.importDefaultHandler()

def get_handlers(halcomp, widgets, paths):
    return [UserHandlerClass(halcomp, widgets, paths)]

class UserHandlerClass(HandlerClass):
    print('user class loaded\n')

    def on_keycall_ABORT(self,event,state,shift,cntrl):
        super().on_keycall_ABORT(event,state,shift,cntrl)
        if state:
            print('abort')

    def before_loop__(self):
        try:
            self.w.led_at_speed.setProperty('halpin_option',False)   
            self.w.led_at_speed.setProperty('follow_halpin_state',True)
            self.w.led_at_speed.setProperty('halpin_name','spindle.0.at-speed')
            self.w.led_at_speed. _hal_init()
        except Exception as e:
            pass

