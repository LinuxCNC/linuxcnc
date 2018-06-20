import sys
# try to add a notify system so messages use the
# nice intergrated pop-ups
# Ubuntu kinda wrecks this be not following the
# standard - you can't set how long the message stays up for.
# I suggest fixing this with a PPA off the net
# https://launchpad.net/~leolik/+archive/leolik?field.series_filter=lucid
    # callback work around:
    # http://stackoverflow.com/questions/8727937/callbacks-and-gtk-main-loop

from qtvcp.core import Status
from qtvcp.lib import sys_notify

# Set up logging
from qtvcp import logger
log = logger.getLogger(__name__)

STATUS = Status()
sys_notify.init('notify')


class Notify:
    def __init__(self):
        self.statusbar = None
        self.notify_list = []
        self.alarmpage = []
        STATUS.connect('shutdown', self.cleanup)

    # This prints a message in the status bar (if available)
    # the system notifier (if available)
    # adds an entry to the alarm page (if available)
    def notify(self, title, message,icon="", status_timeout=0, timeout=2):
        messageid = None
        try:
            self.show_status(message, status_timeout)
        except:
            pass
            print 'no status'
        try:
            self.add_alarm_entry(message)
        except:
            pass
        try:
            self.show_notification(title, message, icon, timeout)
        except Exception as e: print(e)

    def show_notification(self, title, message, icon=None, timeout=4):
        n = sys_notify.Notification(title, message, icon)
        n.setUrgency(sys_notify.Urgency.LOW)
        n.setTimeout(int(timeout * 1000))
        n.addAction("action_click","Show All Messages", self.action_callback)
        n.onClose(self.handle_closed)
        #self._n.add_action('You Clicked The Button', 'Remove Fire', self.OnClicked)
        n.show()
        self.notify_list.append(n)

    def handle_closed(self,n):
        pass
        #print self._n
        #print n

    def OnClicked(self, notification, signal_text):
        print '1: ' + str(notification)
        print '2: ' + str(signal_text)
        notification.close()

    def action_callback(self, *args, **kwds):
        print '\nAll recorded messages:'
        for num,i in enumerate(self.alarmpage):
            print num,i

    def show_status(self, message, timeout=4):
        try:
            messageid = self.statusbar.showMessage(message, timeout * 1000)
        except Exception as e:
            log.warning('Error adding msg to  statusbar:', exc_info=e)

    def add_alarm_entry(self, message):
        try:
            self.alarmpage.append(message)
        except:
            pass

    def cleanup(self, w):
        for i in self.notify_list:
            i.close()

