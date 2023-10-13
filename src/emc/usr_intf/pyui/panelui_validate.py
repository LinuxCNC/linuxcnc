import sys,os
from configobj import ConfigObj, flatten_errors
from validate import Validator
from validate import ValidateError
import shutil

# path to the configuration the user requested
BASE = os.path.abspath(os.path.join(os.path.dirname(sys.argv[0]), ".."))
LIBPATH = os.path.join(BASE, "lib/python/pyui","panelui_spec.ini")
try:
    CONFIGPATH = os.environ['CONFIG_DIR']
    CONFIGDIR = os.path.join(CONFIGPATH, '_panelui.ini')
except:
    print('**** PANEL COMMAND: no panelui.ini file in config directory')
    CONFIGPATH = os.path.expanduser("~")
    CONFIGDIR = os.path.join(CONFIGPATH, '_panelui.ini')
print("Validating panelui INI file from: ",CONFIGDIR)

def list_check(value):
    if not isinstance(value, list):
        if value.lower() == 'none':
            return ['NONE','NONE']
        if isinstance(value, str):
            return [value,'NONE']
        else:
            raise ValidateError('"%s" is not a list' % value)

    return value

def numerical_check(value):
    try:
        float(value)
        return value
    except ValueError:
        raise ValidateError('"%s" is not a number' % value)


config = ConfigObj(CONFIGDIR, configspec=LIBPATH, indent_type='    ')

validator = Validator({'list':list_check,'number':numerical_check})
result = config.validate(validator, preserve_errors=True,copy=True)

if result != True:
    for entry in flatten_errors(config, result):
        # each entry is a tuple
        section_list, key, error = entry
        if key is not None:
           section_list.append(key)
        else:
            section_list.append('[missing section]')
        section_string = ', '.join(section_list)
        if error == False:
            error = 'Missing value or section.'
        print('PANELUI:',section_string, ' = ', error)
else:
    print('PANELUI: validation found no obvious errors')
shutil.copy2(CONFIGDIR, os.path.join(CONFIGPATH, '_panelui.ini~'))
config.write()
