import os

# Set up logging
from qtvcp import logger
log = logger.getLogger(__name__)

# Set the log level for this module
log.setLevel(logger.ERROR) # One of DEBUG, INFO, WARNING, ERROR, CRITICAL

class Icon:
    def __init__(self):
        self.ICONDIR = os.path.join(os.path.abspath(os.path.dirname(__file__)), 'widget_icons')
        self.LCNC_ICON = os.path.join(self.ICONDIR, 'linuxcnc-wizard.gif')
        if not os.path.isfile(self.LCNC_ICON):
            log.warning('error no icon in: {}'.format(self.LCNC_ICON))

    def get_path(self, widgetname):
        for i in ('.png', '.gif', 'error'):
            path = os.path.join(self.ICONDIR, widgetname + i)
            if os.path.isfile(path):
                return path
            elif i == 'error':
                log.warning("Missing icon for '{}' widget".format(widgetname))
                return self.LCNC_ICON
