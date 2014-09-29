#
# interpret a g-code file, and generate preview commands
#
# run as python rs274preview.py <ngcfile>
# to abort after first second:
# python rs274preview.py -a <verybigngcfile>

import sys
import getopt
import preview as gcode
import time
import threading

class Dummy:
   def __init__(self):
       self.aborted = False

   def do_cancel(self):
       print "setting abort flag"
       self.aborted = True

   def check_abort(self):
       print "check_abort"
       return self.aborted

class Preview(threading.Thread):
    def __init__(self,filename,canon, unitcode, initcode):
        self.filename = filename
        self.canon = canon
        self.unitcode = unitcode
        self.initcode = initcode

        threading.Thread.__init__(self)
        self.daemon = True

    def run(self):

        try:

            result, last_sequence_number = gcode.parse(self.filename,
                                                       self.canon, self.unitcode,
                                                       self.initcode)

            # XXX mystery attributes
            # print "gcodes", gcode.linecode.gcodes
            # print "sequence_number", gcode.linecode.sequence_number

            if result > gcode.MIN_ERROR:
                print " gcode error: %s " % (gcode.strerror(result))
                print " last_sequence_number ",last_sequence_number
            else:
                pass
                # # XXX: unclear how this is supposed to work
                # minxt,maxxt,min_t_xt,max_t_xt = gcode.calc_extents()
                # print "X extent: %.2f .. %.2f" % (minxt[0],maxxt[0])
                # print "Y extent: %.2f .. %.2f" % (minxt[1],maxxt[1])
                # print "Z extent: %.2f .. %.2f" % (minxt[0],maxxt[2])
                # print "X extent w/tool: %.2f .. %.2f" % (min_t_xt[0],max_t_xt[0])
                # print "Y extent w/tool: %.2f .. %.2f" % (min_t_xt[1],max_t_xt[1])
                # print "Z extent w/tool: %.2f .. %.2f" % (min_t_xt[0],max_t_xt[2])
        except KeyboardInterrupt,e:
            print "exception:", e
        print "Preview exiting"

def main():
    canon = Dummy()
    # canon expects a 'parameter_file' attribute
    #canon.parameter_file = "sim.var"
    # corresponds to Axis ini RS274NGC PARAMETER_FILE value
    canon.parameter_file = ""

    # XXX mystery parameter
    # executed before startupcode - to set machine units (G20,G21)?
    unitcode = ""
    # corresponds to Axis ini RS274NGC RS274NGC_STARTUP_CODE value
    initcode = "G17 G20 G40 G49 G54 G80 G90 G94"

    try:
        opts, args = getopt.getopt(sys.argv[1:], "s:i:f:a")
    except getopt.error, msg:
        print msg
        sys.exit(2)
    t = 0
    for o, a in opts:
        if o in ("-h", "--help"):
            print __doc__
            sys.exit(0)
        if o in ("-s"):
            startupcode = a
        if o in ("-i"):
            initcode = a
        if o in ("-p"):
            canon.parameter_file = a
        if o in ("-a"):
            # will trigger abort after 1 second (checking interval in previewmodule.cc)
            canon.do_cancel()

    for arg in args:
        p = Preview(arg,canon, unitcode, initcode)
        p.start()

    p.join()

if __name__ == "__main__":
    main()
    time.sleep(2) # let sockets drain