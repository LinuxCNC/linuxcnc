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
        try:
            n = self.show_notification(title, message, icon, timeout)
        except Exception as e:
               log.warning('show_noficication error:', exc_info=e)
        return n

    # 
    def new_critical(self, icon=""):
        messageid = None
        try:
            n = self.build_error_notification(icon)
        except Exception as e:
               log.warning('New_critical error:', exc_info=e)
        return n


    def notify_yn(self, title, message,icon, timeout, function_callback):
        try:
            self.show_yn_notification(title, message, icon, timeout,function_callback)
        except Exception as e:
               log.warning('show_noficication_yn error:', exc_info=e)

    def notify_ok(self, title, message,icon, timeout, function_callback):
        try:
            self.show_ok_notification(title, message, icon, timeout,function_callback)
        except Exception as e:
               log.warning('show_noficication_ok error:', exc_info=e)

    def build_error_notification(self, icon=None):
        n = sys_notify.Notification('', '', icon)
        n.setUrgency(sys_notify.Urgency.CRITICAL)
        n.setTimeout(0)
        n.addAction("action_click","Show All Messages", self.action_callback)
        n.onClose(self.handle_closed)
        n.addAction('Clear Messages', 'Clear', self.clearClicked)
        self.notify_list.append(n)
        return n

    def show_notification(self, title, message, icon=None, timeout=4):
        n = sys_notify.Notification(title, message, icon)
        n.setUrgency(sys_notify.Urgency.NORMAL)
        n.setTimeout(int(timeout * 1000))
        n.addAction("action_click","Show All Messages", self.action_callback)
        n.onClose(self.handle_closed)
        n.addAction('You Clicked The Button', 'Remove Fire', self.OnClicked)
        n.show()
        self.notify_list.append(n)
        return n

    def show_yn_notification(self, title, message, icon, timeout,callback):
        self._callback=callback
        n = sys_notify.Notification(title, message, icon)
        n.setUrgency(sys_notify.Urgency.CRITICAL)
        n.setTimeout(timeout* 1000)
        n.addAction("Yes", "Yes", self.yesClicked)
        n.onClose(self.handle_closed)
        n.addAction('No', 'No', self.noClicked)
        n.show()
        self.notify_list.append(n)

    def show_ok_notification(self, title, message, icon, timeout, callback):
        n = sys_notify.Notification(title, message, icon)
        n.setUrgency(sys_notify.Urgency.CRITICAL)
        n.setTimeout(timeout* 1000)
        n.addAction("Ok", "ok", self.okClicked, callback)
        n.onClose(self.handle_closed)
        n.addAction('Canel', 'canel', self.cancelClicked, callback)
        n.show()
        self.notify_list.append(n)

    def yesClicked(self, n, action, callback):
        callback(True)

    def noClicked(self, n, action, callback):
        callback(False)

    def okClicked(self, n, action, callback):
        callback(True)

    def cancelClicked(self, n, action, callack):
        callback(False)

    def handle_closed(self,n):
        pass
        #print self._n
        #print n

    def clearClicked(self, n, text):
        n.body = ''
        n.close()

    def OnClicked(self, n, signal_text):
        print '1: ' + str(n)
        print '2: ' + str(signal_text)
        n.close()

    def action_callback(self, *args, **kwds):
        print '\nAll recorded messages:'
        for num,i in enumerate(self.alarmpage):
            print num,i

    def show_status(self, message, timeout=4):
        if self.statusbar is not None:
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

    def update(self, n, title='', message=''):
        if title is not None:
            n.title = title
        n.body = n.body +'\n'+ title+'\n'+ message
        n.show()
        try:
            self.show_status(message, 5)
        except:
            pass
        try:
            self.add_alarm_entry(message)
        except:
            pass

