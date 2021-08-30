import sys
# try to add a notify system so messages use the
# nice integrated pop-ups
# Ubuntu kinda wrecks this be not following the
# standard - you can't set how long the message stays up for.
# I suggest fixing this with a PPA off the net
# https://launchpad.net/~leolik/+archive/leolik?field.series_filter=lucid
# callback work around:
# http://stackoverflow.com/questions/8727937/callbacks-and-gtk-main-loop

from qtvcp.core import Status, Info
from qtvcp.lib import sys_notify

# Set up logging
from qtvcp import logger

log = logger.getLogger(__name__)

STATUS = Status()
INFO = Info()
sys_notify.init('notify')


class Notify:
    YESNO = 'yesNo'
    JOGPAUSED = 'jogPaused'
    OKCANCEL = 'okCancel'
    CLOSE = 'close'
    LASTFIVE = 'lastFive'
    CLEARALL = 'clearAll'

    def __init__(self):
        self.statusbar = None
        self.lastnum = 0
        self.notify_list = []
        self.alarmpage = []
        self.critical_message = None
        self.normal_message = None
        self.hard_limits_message = None
        STATUS.connect('shutdown', self.cleanup)

    # This prints a message in the status bar (if available)
    # the system notifier (if available)
    # adds an entry to the alarm page (if available)
    def notify(self, title, message, icon="", status_timeout=0, timeout=2):
        messageid = None
        try:
            self.show_status(message, status_timeout)
        except:
            pass
        try:
            n = self.build_general_notification(title, message, icon, timeout=timeout)
            n.show()
        except Exception as e:
            log.warning('build_generalnotification error:', exc_info=e)
        return n

    # Screenoption uses this for errors / operator messages
    # they stay up till cleared
    # self.critical_message gives reference for external controls
    def new_critical(self, icon=""):
        messageid = None
        try:
            self.critical_message = self.build_error_notification(icon)
        except Exception as e:
            log.warning('New_critical error:', exc_info=e)
        return self.critical_message

    # Screenoption uses this for hard limit errors
    # they stay up till cleared
    # self.critical_message gives reference for external controls
    def new_hard_limits(self, icon="", callback=None):
        messageid = None
        try:
            self.hard_limits_message = self.build_hard_limits_notification(icon, callback)
        except Exception as e:
            log.warning('New_critical error:', exc_info=e)
        return self.hard_limits_message

    # Screenoption uses this for errors / operator messages
    # they stay up till cleared
    # self.critical_message gives reference for external controls
    def new_normal(self, icon=""):
        messageid = None
        try:
            self.normal_message = self.build_general_notification(icon)
        except Exception as e:
            log.warning('New_critical error:', exc_info=e)
        return self.normal_message

    # messages that require yes/no response.
    def notify_yn(self, title, message, icon, timeout, function_callback):
        try:
            self.show_yn_notification(title, message, icon, timeout, function_callback)
        except Exception as e:
            log.warning('build_generalnotification_yn error:', exc_info=e)

    # message that require acknowledgement
    def notify_ok(self, title, message, icon, timeout, function_callback):
        try:
            self.show_ok_notification(title, message, icon, timeout, function_callback)
        except Exception as e:
            log.warning('build_generalnotification_ok error:', exc_info=e)

    #####################################################
    # actually build the notices
    #####################################################

    def build_error_notification(self, icon=None):
        n = sys_notify.Notification('', '', icon)
        n.setUrgency(sys_notify.Urgency.CRITICAL)
        n.setTimeout(0)
        n.addAction("action_click", "Show Last Five", self.last5_callback)
        n.onClose(self.handle_closed)
        n.addAction('destroy_clicked', 'Clear All', self.destroyClicked)
        n.addAction('close_clicked', 'Close', self.closeClicked)
        self.notify_list.append(n)
        return n

    def build_hard_limits_notification(self, icon=None, callback=None):
        self._hardLimitsCallback = callback
        n = sys_notify.Notification('', '', icon)
        n.setUrgency(sys_notify.Urgency.CRITICAL)
        n.setTimeout(0)
        n.addAction("action_click", "Override Limits", self._hardLimitsCallback)
        n.onClose(self.handle_closed)
        n.addAction('close_clicked', 'Close', self.closeClicked)
        self.notify_list.append(n)
        return n

    def build_general_notification(self, title='', message='', icon=None, timeout=2):
        n = sys_notify.Notification(title, message, icon)
        n.setUrgency(sys_notify.Urgency.NORMAL)
        n.setTimeout(int(timeout * 1000))
        n.addAction("action_click", "Show Last Five", self.last5_callback)
        n.addAction('destroy_click', 'Clear All', self.destroyClicked)
        n.addAction('close_clicked', 'Close', self.closeClicked)
        n.onClose(self.handle_closed)
        self.notify_list.append(n)
        return n

    def show_yn_notification(self, title, message, icon, timeout, callback):
        self._callback = callback
        n = sys_notify.Notification(title, message, icon)
        n.setUrgency(sys_notify.Urgency.CRITICAL)
        n.setTimeout(timeout * 1000)
        n.addAction("Yes", "Yes", self.yesClicked, callback)
        n.onClose(self.handle_closed)
        n.addAction('No', 'No', self.noClicked, callback)
        n.show()
        self.notify_list.append(n)

    def show_ok_notification(self, title, message, icon, timeout, callback):
        n = sys_notify.Notification(title, message, icon)
        n.setUrgency(sys_notify.Urgency.CRITICAL)
        n.setTimeout(timeout * 1000)
        n.addAction("Ok", "ok", self.okClicked, callback)
        n.onClose(self.handle_closed)
        n.addAction('Canel', 'cancel', self.cancelClicked, callback)
        n.show()
        self.notify_list.append(n)

    def show_toolchange_notification(self, title, message, icon, timeout, callback, jogpause=False):
        n = sys_notify.Notification(title, message, icon)
        n.setUrgency(sys_notify.Urgency.CRITICAL)
        n.setTimeout(0)
        n.addAction("action_click", "Ok", self.okClicked, callback)
        n.onClose(lambda w:self.okClicked(w, None, callback))
        if jogpause:
            n.addAction('close_clicked', 'jogPause', self.jogPauseClicked)
        n.show()
        self.notify_list.append(n)

    ################################################
    # callback mechanism
    ################################################
    def yesClicked(self, n, action, callback):
        callback(True)
        STATUS.emit('system_notify_button_pressed', Notify.YESNO, True)

    def noClicked(self, n, action, callback):
        callback(False)
        STATUS.emit('system_notify_button_pressed', Notify.YESNO, True)

    def okClicked(self, n, action, callback):
        callback(True)
        STATUS.emit('system_notify_button_pressed', Notify.OKCANCEL, True)

    def cancelClicked(self, n, action, callback):
        callback(False)
        STATUS.emit('system_notify_button_pressed', Notify.OKCANCEL, True)

    def jogPauseClicked(self, n, action, callback):
        callback(-1)
        STATUS.emit('system_notify_button_pressed', Notify.JOGPAUSE, True)

    def handle_closed(self, n):
        pass

    def closeClicked(self, n, text):
        n.close()
        STATUS.emit('system_notify_button_pressed', Notify.CLOSE, True)
        STATUS.emit('play-sound', 'SPEAK _KILL_')

    def OnClicked(self, n, signal_text):
        print('1: ' + str(n))
        print('2: ' + str(signal_text))
        n.close()

    def action_callback(self, *args, **kwds):
        print('\nAll recorded messages:')
        for num, i in enumerate(self.alarmpage):
            print(num, i)

    # pop up last five critical errors
    def last5_callback(self, n, signal_text):
        n.body = ''
        for i in range(1, 6):
            num = len(self.alarmpage) - i
            if i > len(self.alarmpage): break
            n.body = '{}\nREVIEW #{} of {}\n{}'.format(n.body,
                                                       num +1,
                                                       len(self.alarmpage),
                                                       self.alarmpage[num][1])
        n.show()
        STATUS.emit('system_notify_button_pressed', Notify.LASTFIVE, True)

    def destroyClicked(self, n, signal_text):
        self.alarmpage = []
        n.body = ''
        STATUS.emit('system_notify_button_pressed', Notify.CLEARALL, True)
        STATUS.emit('play-sound', 'SPEAK _KILL_')

    #####################################################
    # General work functions
    #####################################################
    # update the critical message display
    # this adds the new message to the old
    # show a max of 10 messages on screen
    def update(self, n, title='', message='', status_timeout=5, timeout=None, msgs=10):
        if title is not None:
           n.title = title
        if self.alarmpage ==[]:
           n.body = title + '\n' + message
        elif len(self.alarmpage) < (msgs - 1):
            n.body = ''
            for i in range(len(self.alarmpage)):
                n.body = n.body + '\n' +  self.alarmpage[i][1]
            n.body = n.body + '\n' + title + '\n' + message
        else:
            n.body = ''
            for i in range(len(self.alarmpage) - (msgs - 1), len(self.alarmpage)):
                n.body = n.body + '\n' +  self.alarmpage[i][1]
            n.body = n.body + '\n' + title + '\n' + message
        if timeout is not None:
            n.setTimeout(timeout * 1000)
        n.show()
        try:
            self.show_status(message, status_timeout)
        except:
            pass
        try:
            self.add_alarm_entry(n, message)
        except:
            pass

    # try to update a status bar if we were given reference to it
    def show_status(self, message, timeout=4):
        if self.statusbar is not None:
            try:
                messageid = self.statusbar.showMessage(message, timeout * 1000)
            except Exception as e:
                log.warning('Error adding msg to  statusbar:', exc_info=e)

    # show the previous critical messages that popped up
    # Currently alarm page doesn't keep track of what
    # notice made the alarm so we might get question dialogs too.
    def show_last(self):
        num = len(self.alarmpage) - 1 - self.lastnum
        if self.critical_message is not None:
            if self.alarmpage:
                n = self.alarmpage[num][0]
                n.body = 'Review #{} of {}\n{}'.format(self.lastnum + 1,
                                                       len(self.alarmpage),
                                                       self.alarmpage[num][1])
                n.show()
                self.show_status(n.body)
                # ready for next message if there is one, other wise reset counter
                self.lastnum += 1
                if self.lastnum >= len(self.alarmpage):
                    n.body = ''
                    self.lastnum = 0

    def external_close(self):
        for num, i in enumerate(self.alarmpage):
            print(num, i)
        if self.critical_message is not None:
            n = self.critical_message
            n.body = ''
            n.close()
        if self.normal_message is not None:
            self.normal_message.close()
        self.lastnum = 0

    # update the system alarm page, if there is one
    # this should be sent to STATUS message I think?
    def add_alarm_entry(self, mobject, message):
        if message == None: message = ''
        try:
            self.alarmpage.append((mobject, message))
        except:
            pass

    # close any remaining messages when we shutdown qtvcp
    def cleanup(self, w):
        for i in self.notify_list:
            i.close()
