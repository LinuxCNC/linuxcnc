import interpreter
EMC_DEBUG_CONFIG            = 0x00000002
EMC_DEBUG_VERSIONS          = 0x00000008
EMC_DEBUG_TASK_ISSUE        = 0x00000010
EMC_DEBUG_NML               = 0x00000040
EMC_DEBUG_MOTION_TIME       = 0x00000080
EMC_DEBUG_INTERP            = 0x00000100
EMC_DEBUG_RCS               = 0x00000200
EMC_DEBUG_INTERP_LIST       = 0x00000800
EMC_DEBUG_IOCONTROL         = 0x00001000
EMC_DEBUG_OWORD             = 0x00002000
EMC_DEBUG_REMAP             = 0x00004000
EMC_DEBUG_PYTHON            = 0x00008000
EMC_DEBUG_NAMEDPARAM        = 0x00010000
EMC_DEBUG_GDBONSIGNAL       = 0x00020000

i = interpreter.this

print "debug: ", i.debugmask,"logging:",i.loggingLevel
## i.debugmask |= EMC_DEBUG_PYTHON
## i.loggingLevel = 9

def introspect(*args):
    global i

    print "argc=", len(args),"args=",args, "call_level=",i.call_level

    # the low-level access within the block
    for n in range(len(args)):
        print "param #",n ,"=", i.blocks[0].params[n]

    # this is a low-level interface.
    for x in i.sub_context[1].named_params:
        print "name:",x.key(),"value=",x.data().value, "attr=",x.data().attr

    print "current oword subname=", i.blocks[0].o_name
    print "m_modes[0]=", i.blocks[0].m_modes[0]
    print "g_modes[0]=", i.blocks[0].g_modes[0]

    # this is the high level named & numbered parameter interface
    print "current tool=",i.params[5400],i.params["_current_tool"]

    print "feed=",i.params['_feed']
    print "speed=",i.params['_rpm']

    print "global parameter set in test.ngc:",i.params['_a_global_set_in_test_dot_ngc']
    print "parameter set via test.ini:",i.params['_[example]variable']
    assert i.params['_[example]variable'] == args[3]

    i.params["a_new_local"] = 321.0
    i.params["_a_new_global"] = 456.0

    print "locals: ",i.params.locals()
    print "globals: ",i.params.globals()
    print "params(): ",i.params()
    return 2.71828
