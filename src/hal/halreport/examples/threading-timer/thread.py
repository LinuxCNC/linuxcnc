# http://stackoverflow.com/questions/4152969/genrate-timer-in-python/4153314#4153314
#
# something like this will be useful for session timers
#
import threading
import time

class TimerClass(threading.Thread):
    def __init__(self):
        threading.Thread.__init__(self)
        self.event = threading.Event()

    def run(self):
        while not self.event.is_set():
            print "do something"
            self.event.wait( 1 )

    def stop(self):
        self.event.set()

tmr = TimerClass()
tmr.setDaemon(True)
tmr.start()
time.sleep( 10 )
tmr.stop()
