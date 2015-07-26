from machinekit import rtapi
import inspect

# helper for logging in nosetests scripts to rtapi
# to correlate test location with HAL/RTAPI log messages

# see also:
# http://stackoverflow.com/questions/6810999/how-to-determine-file-function-and-line-number

class Log:
    def __init__(self, level=rtapi.MSG_DBG,tag="logger"):
        self.l = None
        self.level = level
        self.tag = tag

    def log(self, *args):
        # defer init until RTAPI known to be up
        if not self.l:
            self.l = rtapi.RTAPILogger(level=self.level,tag=self.tag)

        # 0 represents this line
        # 1 represents line at caller

        callerframerecord = inspect.stack()[1]
        frame = callerframerecord[0]
        info = inspect.getframeinfo(frame)
        print >> self.l, "%s:%s:%s %s" % (info.filename,
                                          info.function,
                                          info.lineno,
                                          " ".join(args)),

if __name__ == "__main__":
    l = Log(level=rtapi.MSG_ERR,tag="testrun")
    l.log("foo","bar","baz")
