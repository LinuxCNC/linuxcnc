import sys
import importlib
from qtvcp.core import Path, Qhal, Action, Status

# instantiate some standard libraries
PATH = Path()
QHAL = Qhal()
ACTION = Action()
STATUS = Status()

# get reference to original handler file so we can subclass it
sys.path.insert(0, PATH.SCREENDIR)
module = "{}.{}_handler".format(PATH.BASEPATH,PATH.BASEPATH)
mod = importlib.import_module(module, PATH.SCREENDIR)
sys.path.remove(PATH.SCREENDIR)
HandlerClass = mod.HandlerClass

# return our subclassed handler object
def get_handlers(halcomp, widgets, paths):
    return [UserHandlerClass(halcomp, widgets, paths)]

class UserHandlerClass(HandlerClass):
    print('Custom subclassed handler loaded\n')

    # add custom functions here
