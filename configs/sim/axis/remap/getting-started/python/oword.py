from util import call_pydevd

def foo(*args):
    print "foo!"


# make debugger available as oword procedure
def pydevd(self,*args):
    call_pydevd()



# this would be defined in the oword module
def mysub(self, *args):
    print "number of parameters passed:", len(args)
    for a in args:
	print a
