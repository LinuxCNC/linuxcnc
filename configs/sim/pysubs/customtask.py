import emctask


# int test(int arg);
# int emcToolPrepare(int p, int tool);
# int emcToolLoad();
# int emcToolUnload();
# int emcToolSetNumber(int number);

# virtual but not yet wrapped
# void load_tool(int pocket);
# int saveToolTable(const char *filename,			      CANON_TOOL_TABLE toolTable[]);

class CustomTask(emctask.Task):
    def __init__(self):
        emctask.Task.__init__(self)
        print "Py CustomTask.init"


    # example Python pre & post-call hooks
    def test(self, arg):
        # do your pre-call Python thing
        print "Py pre test(%d)" % (arg)
        # call the default ethod (C)
        rc = super(CustomTask,self).test(arg)
        # do your post-call Python thing
        print "Py post test(%d)" % (arg)
        return rc

    def emcToolPrepare(self,p,tool):
        print "--Py pre-hook: emcToolPrepare()",p, tool
        emctask.operator_text("foo!",id=4711)
        rc = super(CustomTask,self).emcToolPrepare(p,tool)
        emctask.operator_error("bar!")
        print "--Py post-hook: emcToolPrepare()",p, tool
        return rc

    def emcToolLoad(self):
        print "--Py pre-hook: emcToolLoad()"
        rc = super(CustomTask,self).emcToolLoad()
        return rc

    def emcToolUnload(self):
        print "--Py pre-hook: emcToolUnload()"
        rc = super(CustomTask,self).emcTooUnload()
        return rc


    def emcToolSetNumber(self,n):
        print "--Py pre-hook: emcToolSetNumber()",n
        rc = super(CustomTask,self).emcToolSetNumber(n)
        return rc
