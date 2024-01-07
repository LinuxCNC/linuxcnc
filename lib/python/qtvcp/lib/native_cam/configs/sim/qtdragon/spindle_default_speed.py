print('\nUser command file for update spubdke default found\n')

# import the handlerfile to get reference to it's libraries.
# use <screenname>_handler
import qtdragon_handler as SELF

# needed to instance patch
# reference: https://ruivieira.dev/python-monkey-patching-for-readability.html
import types

def spindle_setting():
    SELF.STATUS.stat.poll()
    if not SELF.STATUS.is_auto_running():
        s = SELF.STATUS.stat.settings[2]
        if s>0:
            # hack to change default spindle speed
            SELF.INFO['DEFAULT_SPINDLE_0_SPEED'] = s

###############################################
# add hook to add a status request
###############################################

def after_override__(self):
    try:
        SELF.STATUS.connect('s-code-changed', lambda w, data:spindle_setting())
       #SELF.STATUS.connect('periodic', lambda w:spindle_setting())
    except Exception as e:
        print(e)

# Here we are instance patching the original handler file to add a new
# function that calls our new function (of the same name)
# defined in this file
self.after_override__ = types.MethodType(after_override__, self)


