import sys
# try to add a notify system so messages use the
# nice intergrated pop-ups
# Ubuntu kinda wrecks this be not following the
# standard - you can't set how long the message stays up for.
# I suggest fixing this with a PPA off the net
# https://launchpad.net/~leolik/+archive/leolik?field.series_filter=lucid
    # callback work around:
    # http://stackoverflow.com/questions/8727937/callbacks-and-gtk-main-loop
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
        self._n=[]
        self.statusbar = None
        self.alarmpage = []

    # This prints a message in the status bar (if available)
    # the system notifier (if available)
    # adds an entry to the alarm page (if available)
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
        except Exception as e: print(e)

    def show_notification(self, title, message, icon=None, timeout=4):
        uri = ""
        try:
            if icon:
                uri = "file://" + icon
        except:
            print 'ERROR Notify - Icon filename error - %s'% icon
        n =  pynotify.Notification(title,message)
        self._n.append(n)
        #n.set_hint_string("x-canonical-append","True")
        n.set_urgency(pynotify.URGENCY_LOW)
        n.set_timeout(int(timeout * 1000) )
        n.add_action("action_click","Show All Messages",self.action_callback,None) # Arguments
        n.connect('closed', self.handle_closed)
        #self._n.add_action('You Clicked The Button', 'Remove Fire', self.OnClicked)
        n.show()

    def handle_closed(self,n):
        pass
        #print self._n
        #print n

    def OnClicked(self,notification, signal_text):
        print '1: ' + str(notification)
        print '2: ' + str(signal_text)
        notification.close()

    def action_callback(self,*args,**kwds):
        print '\nAll recorded messages:'
        for num,i in enumerate(self.alarmpage):
            print num,i

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
