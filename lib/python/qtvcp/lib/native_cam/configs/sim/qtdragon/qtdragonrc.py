print('\nUser command file for following spindle.0.at_speed\n')

# needed to instance patch
# reference: https://ruivieira.dev/python-monkey-patching-for-readability.html
import types

# import the handlerfile to get reference to it's libraries.
# use <screenname>_handler
import qtdragon_handler as SELF

###############################################
# add hook to add a status request
###############################################
def before_loop__(self):
    try:
        self.w.led_at_speed.setProperty('halpin_option',False)   
        self.w.led_at_speed.setProperty('follow_halpin_state',True)
        self.w.led_at_speed.setProperty('halpin_name','spindle.0.at-speed')
        self.w.led_at_speed. _hal_init()
    except Exception as e:
        pass

# Here we are instance patching the original handler file to add a new
# function that calls our new function (of the same name)
# defined in this file
self.before_loop__ = types.MethodType(before_loop__, self)

