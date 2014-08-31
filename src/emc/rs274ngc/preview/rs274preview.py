#
# interpret a g-code file, and generate preview commands
#


import sys
from optparse import Option, OptionParser
import preview as gcode
import time
options = [
    Option( '-i', dest='initcode',  default="G17 G20 G40 G49 G54 G80 G90 G94"),
    Option( '-u', dest='unitcode',  default=""),
    Option( '-p', dest='parameter_file',  default=""),
    Option( '-I', dest='interface',  default="127.0.0.1"),
    Option( '-P', dest='previewport',  default="*"),
    Option( '-S', dest='statusport',  default="*"),
    ]

class Dummy:
    pass

def run(filename,canon, unitcode, initcode):

    print "running :", filename, unitcode, initcode
    result, last_sequence_number = gcode.parse(filename, canon, unitcode, initcode)

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


def main():
    parser = OptionParser()
    parser.add_options(options)

    (opts, args) = parser.parse_args()
    if not args:
        print "usage: rs274preview.py [options] <ngc file>"
        sys.exit(1)

    canon = Dummy()
    canon.parameter_file = opts.parameter_file  # eg "sim.var"

    # use to ephemeral ports
    ps = "tcp://%s:%s" % (opts.interface, opts.previewport)
    ss = "tcp://%s:%s" % (opts.interface, opts.statusport)

    # gcode.bind() takes zmq URI's and returns a tuple
    # of actually bound uris which can then be zeroconf-announced
    (preview_uri, status_uri) = gcode.bind(ps, ss)

    print "preview URI:", preview_uri
    print "status URI:", status_uri

    # let consumers connect
    time.sleep(3)


    for arg in args:
        run(arg,canon, opts.unitcode, opts.initcode)
 

if __name__ == "__main__":
    main()
    time.sleep(2) # let sockets drain
