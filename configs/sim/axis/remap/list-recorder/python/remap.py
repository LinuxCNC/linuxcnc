import copy

import recorder

from interpreter import *
# raises InterpreterException if execute() or read() fails
throw_exceptions = 1


#from stdglue import  init_stdglue

def g01(self,**words):
    print "LINE= ",words['n']
    words["code"] = "g0"
    recorder.add(words)

def g11(self,**words):
    words["code"] = "g1"
    recorder.add(words)

def g21(self,**words):
    words["code"] = "g2"
    recorder.add(words)

def g31(self,**words):
    words["code"] = "g3"
    recorder.add(words)

def m200(self,**words):
    profile = int(words["q"])
    recorder.profiles[profile] = list()
    
def m201(self,**words):
    print "m201 words=",words
    profile = int(words["q"])
    print "profile",profile
    print recorder.profiles[profile]

def g70(self,**words):
    q = int(words["q"])

    if not  recorder.profiles.has_key(q):
        return "G70: profile %d not defined" % q

    for cmd in  recorder.profiles[q]:
        line = ""
        stmt = copy.deepcopy(cmd)
        line += stmt["code"] + " "
        del stmt["code"]
        lineno = int(stmt["n"])
        del stmt["n"]
        for word in stmt.iterkeys():
            line += word + str(stmt[word]) + " "
        try:
            print "executing %d: %s" % (lineno,line)
            self.execute(line,lineno)
            lineno += 1

        except InterpreterException,e:
            msg = "%d: '%s' - %s" % (e.line_number,e.line_text, e.error_message)
            print "----------------",msg
            return msg
    return INTERP_OK

