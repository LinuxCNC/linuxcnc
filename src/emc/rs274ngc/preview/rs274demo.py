
import sys
import getopt
import gcode   # stock gcode module
import time

class DumpCanon:
    '''
    coordinates are already scaled to inches
    '''
    def p(self,args):
        r = ""
        for axisbit in range(0,8):
            if self.axismask & (1 << axisbit):
                r += ("%s%.4f ") % ("XYZABCUVW"[axisbit],args[axisbit])
        return r

    def set_g5x_offset(self, g5x_index,*axes):
        '''
        set offset for current coordinate system
        '''
        print "set_g5x_offset ", g5x_index,self.p(axes)

    def set_g92_offset(self, *axes):
        print "set_g92_offset", self.p(axes)

    def set_plane(self, plane):
        '''
        G17, G18, G19, G17.1, G18.1, G19.1 Plane Selection
        XY - 1, YZ - 2, XZ -3, UV - 4, VW - 5, UW - 6
        '''
        print "set plane %d" % plane

    def set_feed_rate(self, rate):
        '''
        see 1.2.5 Feed Rate
        standard case: normalized to inch/min
        '''
        print "set feed rate %.4f" % rate

    def straight_traverse(self, *axes):
        ''' move at rapid rate to programmed point '''
        print "straight_traverse", self.p(axes)

    def straight_feed(self, *axes):
        ''' move at feed rate to programmed point '''
        print "straight_feed", self.p(axes)

    def dwell(self, arg):
        if arg < .1:
            print "dwell %f ms" % (1000 * arg)
        else:
            print "dwell %f seconds" % arg

    def arc_feed(self, first_end, second_end, first_axis, second_axis,
                 rotation, axis_end_point,
                 a_position, b_position, c_position,
                 u_position, v_position, w_position):
        '''
        arc at feed rate(G2,G3)
        first_end, second_end: end points of first and second axis
        first_axis, second_axis: arc center first/second axis
        rotation: number of full turns in arc
        axis_end_point: end point of third axis
        a_position, b_position, c_position,
        u_position, v_position, w_position: values at end of arc
        '''
        print "arc_feed e1=%.4f e2%.4f  a1=%.4f a2=%.4f rot=%d  end=%.4f "  \
            " a=%.4f b=%.4f c=%.4f u=%.4f v=%.4f w%.4f " \
                % ( first_end, second_end, first_axis, second_axis,
                 rotation, axis_end_point,
                 a_position, b_position, c_position,
                 u_position, v_position, w_position)

    def straight_probe(self,*axes):
        ''' probe to programmed point'''
        print "straight_probe",self.p(axes)

    def rigid_tap(self, x,y,z):
        ''' tap to programmed point'''
        print "rigid_tap X%.4f Y%.4f Z%.4f" %(x,y,z)

    def user_defined_function(self, num, arg1, arg2):
        print "user_defined_function %d %.4f %.4f" % (num,arg1,arg2)

    def change_tool(self, pocket):
        print "change_tool",pocket

    def set_xy_rotation(self,rotation):
        '''
        rotation of the XY axis around the Z
        '''
        print "set_xy_rotation %.4f" % (rotation)

    def comment(self, text):
        print "#", text

    def message(self,text):
        print "message:", text

    def get_external_angular_units(self):
        '''
        conversion factor - internal deg units to external units
        '''
        return 1.0

    def get_external_length_units(self):
        '''
        conversion factor - external units to internal mm units
        '''
        return 1.0

    def get_axis_mask(self):
        '''
        or of the machine's axes:
        AXIS_MASK_X =   1, AXIS_MASK_Y =   2, AXIS_MASK_Z =   4,
        AXIS_MASK_A =   8, AXIS_MASK_B =  16, AXIS_MASK_C =  32,
        AXIS_MASK_U =  64, AXIS_MASK_V = 128, AXIS_MASK_W = 256,
        '''
        return self.axismask

    def get_tool(self, tool):
        '''
        return tool table entry for 'tool'
        toolno, offset.tran.x, offset.tran.y, offset.tran.z,
        offset.a, offset.b, offset.c, offset.u, offset.v, offset.w,
        diameter, frontangle, backangle, orientation
        '''
        return tool, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0

    def tool_offset(self,*axes):
        '''
        G43, G43.1: Activate Tool length compensation
        '''
        print "tool_offset",self.p(axes)

    def is_lathe(self):
        ''' referred to by GLCanon'''
        return False

    def check_abort(self):
        ''' check for abort condition before executing a line'''
        return False

    def get_block_delete(self):
        ''' return True if blocks starting with '/' are to be skipped '''
        return False

    def next_line(self, state):
        '''
        indicate a new line parsed, also for blocks extending
        over several lines
        '''
        print "next_line", state.sequence_number
        self.state = state
        self.lineno = self.state.sequence_number


    def __init__(self,axismask = 7):
        self.axismask = axismask # x,y,z
        self.lineno = 0


def run(filename,canon, unitcode, initcode):
    result, last_sequence_number = gcode.parse(filename, canon, unitcode, initcode)

    # XXX mystery attributes
    print "gcodes", gcode.linecode.gcodes
    print "sequence_number", gcode.linecode.sequence_number

    if result > gcode.MIN_ERROR:
        print " gcode error, line %d: %s " % (canon.lineno, gcode.strerror(result))
        print " last_sequence_number ",last_sequence_number
    else:
        # XXX: unclear how this is supposed to work
        minxt,maxxt,min_t_xt,max_t_xt = gcode.calc_extents()
        print "X extent: %.2f .. %.2f" % (minxt[0],maxxt[0])
        print "Y extent: %.2f .. %.2f" % (minxt[1],maxxt[1])
        print "Z extent: %.2f .. %.2f" % (minxt[0],maxxt[2])
        print "X extent w/tool: %.2f .. %.2f" % (min_t_xt[0],max_t_xt[0])
        print "Y extent w/tool: %.2f .. %.2f" % (min_t_xt[1],max_t_xt[1])
        print "Z extent w/tool: %.2f .. %.2f" % (min_t_xt[0],max_t_xt[2])


def main():
    canon = DumpCanon()
    # canon expects a 'parameter_file' attribute
    #canon.parameter_file = "sim.var"
    # corresponds to Axis ini RS274NGC PARAMETER_FILE value
    canon.parameter_file = ""

    # XXX mystery parameter
    # executed before startupcode - to set machine units (G20,G21)?
    unitcode = ""
    # corresponds to Axis ini RS274NGC RS274NGC_STARTUP_CODE value
    initcode = "G17 G20 G40 G49 G54 G80 G90 G94"
    filename = "test.ngc"

    try:
        opts, args = getopt.getopt(sys.argv[1:], "s:i:f:")
    except getopt.error, msg:
        print msg
        sys.exit(2)

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


    for arg in args:
        run(arg,canon, unitcode, initcode)
    else:
        run(filename,canon, unitcode, initcode)

if __name__ == "__main__":
    main()
    time.sleep(2) # let sockets drain
