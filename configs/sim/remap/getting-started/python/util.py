import inspect
import emccanon

# O-word procedure to trap into the Pydevd debugger
# start debug server in Eclipse, then
# call as 'O<pydevd> call' from MDI

# example setup for debugging embedded Python code
# see http://pydev.org/manual_adv_remote_debugger.html
# if this points to a valid directory,

def call_pydevd():
    """ trap into the pydevd debugger"""
    
    import os,sys
    
    pydevdir= '/home/mah/.eclipse/org.eclipse.platform_3.5.0_155965261/plugins/org.python.pydev.debug_2.0.0.2011040403/pysrc/'

    # the 'emctask' module is present only in the milltask instance, otherwise both the UI and
    # milltask would try to connect to the debug server.

    if os.path.isdir(pydevdir) and  'emctask' in sys.builtin_module_names:
        sys.path.append(pydevdir)
        sys.path.insert(0,pydevdir)
        try:
            import pydevd
            emccanon.MESSAGE("pydevd imported, connecting to Eclipse debug server...")
            pydevd.settrace()
        except:
            emccanon.MESSAGE("no pydevd module found")
            pass



def lineno():
    """ return line number in the current Python script """
    return inspect.currentframe().f_back.f_lineno

def error_stack(self):
    """ print the Interpreters error stack (function names) """
    print "error stack level=%d" % (self.stack_index)
    for s in self.stack():
        print "--'%s'" % (s)

def callstack(self):
    """ print the O-Word call stack """
    for i in range(self.call_level):
        c = self.sub_context[i]
        print "%d: pos=%d seq=%d filename=%s sub=%s" % (i,c.position, c.sequence_number,c.filename,c.subname)
        

   
