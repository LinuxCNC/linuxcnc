#!/usr/bin/env python

# QTVcp Notification Module
# Provides a consistent and easy to use facility for showing system notifications.

#   Copyright (c) 2018 Kurt Jacobson
#      <kurtcjacobson@gmail.com>
#
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 2 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.

import dbus
from collections import OrderedDict

# Set up logging
from qtvcp import logger
log = logger.getLogger(__name__)

DBusQtMainLoop = None
try:
    from dbus.mainloop.pyqt5 import DBusQtMainLoop
except ImportError:
    log.warning("Could not import DBusQtMainLoop, is package 'python-dbus.mainloop.pyqt5' installed?")

APP_NAME = ''
DBUS_IFACE = None
NOTIFICATIONS = {}

class Urgency:
    """freedesktop.org notification urgency levels"""
    LOW, NORMAL, CRITICAL = range(3)

class UninitializedError(RuntimeError):
    """Error raised if you try to show an error before initializing"""
    pass

def init(app_name):
    """Initializes the DBus connection"""
    global APP_NAME, DBUS_IFACE
    APP_NAME = app_name

    name = "org.freedesktop.Notifications"
    path = "/org/freedesktop/Notifications"
    interface = "org.freedesktop.Notifications"

    mainloop = None
    if DBusQtMainLoop is not None:
        mainloop = DBusQtMainLoop(set_as_default=True)

    bus = dbus.SessionBus(mainloop)
    proxy = bus.get_object(name, path)
    DBUS_IFACE = dbus.Interface(proxy, interface)

    if mainloop is not None:
        # We have a mainloop, so connect callbacks
        DBUS_IFACE.connect_to_signal('ActionInvoked', _onActionInvoked)
        DBUS_IFACE.connect_to_signal('NotificationClosed', _onNotificationClosed)

def _onActionInvoked(nid, action):
    """Called when a notification action is clicked"""
    nid, action = int(nid), str(action)
    try:
        notification = NOTIFICATIONS[nid]
    except KeyError:
        # must have been created by some other program
        return
    notification._onActionInvoked(action)

def _onNotificationClosed(nid, reason):
    """Called when the notification is closed"""
    nid, reason = int(nid), int(reason)
    try:
        notification = NOTIFICATIONS[nid]
    except KeyError:
        # must have been created by some other program
        return
    notification._onNotificationClosed(notification)
    del NOTIFICATIONS[nid]

class Notification(object):
    """Notification object"""

    id = 0
    timeout = -1
    _onNotificationClosed = lambda *args: None

    def __init__(self, title, body='', icon='', timeout=-1):
        """Initializes a new notification object.

        Args:
            title (str):              The title of the notification
            body (str, optional):     The body text of the notification
            icon (str, optional):     The icon to display with the notification
            timeout (TYPE, optional): The time in ms before the notification hides, -1 for default, 0 for never
        """
        self.title = title              # title of the notification
        self.body = body                # the body text of the notification
        if icon is None:
            icon = ''                   # Fix for legacy use
        self.icon = icon                # the path to the icon to use
        self.timeout = timeout          # time in ms before the notification disappears
        self.hints = {}                 # dict of various display hints
        self.actions = OrderedDict()    # actions names and their callbacks
        self.data = {}                  # arbitrary user data

    def show(self):
        if DBUS_IFACE is None:
            raise UninitializedError("You must call 'notify.init()' before 'notify.show()'")

        """Asks the notification server to show the notification"""
        nid = DBUS_IFACE.Notify(APP_NAME,
                           self.id,
                           self.icon,
                           self.title,
                           self.body,
                           self._makeActionsList(),
                           self.hints,
                           self.timeout,
                        )

        self.id = int(nid)

        NOTIFICATIONS[self.id] = self
        return True

    def close(self):
        """Ask the notification server to close the notification"""
        if self.id != 0:
            DBUS_IFACE.CloseNotification(self.id)

    def onClose(self, callback):
        """Set the callback called when the notification is closed"""
        self._onNotificationClosed = callback

    def setUrgency(self, value):
        """Set the freedesktop.org notification urgency level"""
        if value not in range(3):
            raise ValueError("Unknown urgency level '%s' specified" % level)
        self.hints['urgency'] = dbus.Byte(value)

    def setSoundFile(self, sound_file):
        """Sets a sound file to play when the notification shows"""
        self.hints['sound-file'] = sound_file

    def setSoundName(self, sound_name):
        """Set a freedesktop.org sound name to play when notification shows"""
        self.hints['sound-name'] = sound_name

    def setIconPath(self, icon_path):
        """Set the URI of the icon to display in the notification"""
        self.hints['image-path'] = 'file://' + icon_path

    def setQIcon(self, q_icon):
        # FixMe this would be convenient, but may not be possible
        raise NotImplementedError

    def setLocation(self, x_pos, y_pos):
        """Sets the location to display the notification"""
        self.hints['x'] = int(x_pos)
        self.hints['y'] = int(y_pos)

    def setCategory(self, category):
        """Sets the the freedesktop.org notification category"""
        self.hints['category'] = category

    def setTimeout(self, timeout):
        """Set the display duration in milliseconds, -1 for default"""
        if not isinstance(timeout, int):
            raise TypeError("Timeout value '%s' was not int" % timeout)
        self.timeout = timeout

    def setHint(self, key, value):
        """Set one of the other hints"""
        self.hints[key] = value

    def addAction(self, action, label, callback, user_data=None):
        """Add an action to the notification.

        Args:
            action (str):               A sort key identifying the action
            label (str):                The text to display on the action button
            callback (bound method):    The method to call when the action is activated
            user_data (any, optional):  Any user data to be passed to the action callback
        """
        self.actions[action] = (label, callback, user_data)

    def _makeActionsList(self):
        """Make the actions array to send over DBus"""
        arr = []
        for action, (label, callback, user_data) in self.actions.items():
            arr.append(action)
            arr.append(label)
        return arr

    def _onActionInvoked(self, action):
        """Called when the user activates a notification action"""
        try:
            label, callback, user_data = self.actions[action]
        except KeyError:
            return

        if user_data is None:
            callback(self, action)
        else:
            callback(self, action, user_data)


# ----------------------- E X A M P L E -----------------------

def onHelp(n, action):
    assert(action == "help"), "Action was not help!"
    print "You clicked Help action"
    n.close()

def onIgnore(n, action, data):
    assert(action == "ignore"), "Action was not ignore!"
    print "You clicked Ignore action"
    print "Passed user data was: ", data
    n.close()

def onClose(n):
    print "Notification closed"
    app.quit()

if __name__ == "__main__":
    import sys
    from PyQt5.QtCore import QCoreApplication
    app = QCoreApplication(sys.argv)

    # Initialize the DBus connection to the notification server
    init("demo")

    # Initialize a new notification object
    n = Notification("Demo Notification",
                     "This notification is very important as it " +
                     "notifies you that notifications are working.",
                     timeout=3000
                    )
    n.setUrgency(Urgency.NORMAL)
    n.setCategory("device")
    n.setIconPath("/usr/share/icons/Tango/scalable/status/dialog-error.svg")
    # no user data
    n.addAction("help", "Help", onHelp)
    # passing arbitrary user data to the callback
    n.addAction("ignore", "Ignore", onIgnore, 12345)
    n.onClose(onClose)

    n.show()
    app.exec_()
