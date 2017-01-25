import sys
# try to add a notify system so messages use the
# nice intergrated pop-ups
# Ubuntu kinda wrecks this be not following the
# standard - you can't set how long the message stays up for.
# I suggest fixing this with a PPA off the net
# https://launchpad.net/~leolik/+archive/leolik?field.series_filter=lucid
try:
    NOTIFY_AVAILABLE = False
    import pynotify
    if not pynotify.init("notify"):
        print "**** Notify INFO: There was a problem initializing the pynotify module"
    else:
        NOTIFY_AVAILABLE = True
except:
    print "**** Notify INFO: You don't seem to have pynotify installed"
    sys.exit()

class Notify:
    def __init__(self):
        self.statusbar = None
        self.alarmpage = None
    # This prints a message in the status bar (if available)
    # the system notifier (if available)
    # adds an entry to the alarm page (if available)
    # Ubuntu screws with the system notification program so it does follow timeouts
    # There is a ppa on the net to fix this I suggest it.
    # https://launchpad.net/~leolik/+archive/leolik?field.series_filter=lucid
    def notify(self,title,message,icon="",timeout=2):
        messageid = None
        try:
            self.show_status(message, timeout)
        except:
            pass
        try:
            self.add_alarm_entry(message)
        except:
            pass
        try:
            self.show_notification(title, message, icon, timeout)
        except:
            pass

    def show_notification(self, title, message, icon=None, timeout=4):
        uri = ""
        if icon:
            uri = "file://" + icon
        n = pynotify.Notification(title, message, uri)
        n.set_hint_string("x-canonical-append","True")
        n.set_urgency(pynotify.URGENCY_CRITICAL)
        n.set_timeout(int(timeout * 1000) )
        n.show()

    def show_status(self, message, timeout=4):
        try:
            messageid = self.statusbar.showMessage(message, timeout * 1000)
        except:
            pass

    def add_alarm_entry(self, message):
        try:
            self.alarmpage.append(message)
        except:
            pass
