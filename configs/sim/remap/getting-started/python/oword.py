from util import call_pydevd

def foo(*args):
    print "foo!"


# make debugger available as oword procedure
def pydevd(self,*args):
    call_pydevd()
