import interpreter

def init_stdglue(self):
    pass

def check_coords(self, axis, wanted):
    actual = getattr(self, "current_%s" % axis)
    wanted = float(wanted)
    if abs(actual - wanted) > 0.000001:
        self.execute("(ERROR  %s:  %0.1f != %0.1f)" % (axis, actual, wanted))
    else:
        self.execute("(%s: %0.1f = %0.1f)" % (axis, actual, wanted))

def body_405(self,**words):

    self.execute("(body_M405 begin)")

    self.execute("G0 Y1")
    yield interpreter.INTERP_EXECUTE_FINISH
    check_coords(self, 'y', 1)

    self.execute("G0 Y2")
    yield interpreter.INTERP_EXECUTE_FINISH
    check_coords(self, 'y', 2)

    self.execute("G0 Y3")
    yield interpreter.INTERP_EXECUTE_FINISH
    check_coords(self, 'y', 3)

    self.execute("(body_M405 end)")
    yield interpreter.INTERP_OK

def prolog_406(self,**words):
    self.execute("(prolog_M406 begin)")

    self.execute("G0 X1")
    yield interpreter.INTERP_EXECUTE_FINISH
    check_coords(self, 'x', 1)

    self.execute("G0 X2")
    yield interpreter.INTERP_EXECUTE_FINISH
    check_coords(self, 'x', 2)

    self.execute("G0 X3")
    yield interpreter.INTERP_EXECUTE_FINISH
    check_coords(self, 'x', 3)

    self.execute("(prolog_M406 end)")
    yield interpreter.INTERP_OK


def epilog_406(self,**words):
    self.execute("(epilog_M406 begin)")

    self.execute("G0 Z1")
    yield interpreter.INTERP_EXECUTE_FINISH
    check_coords(self, 'z', 1)

    self.execute("G0 Z2")
    yield interpreter.INTERP_EXECUTE_FINISH
    check_coords(self, 'z', 2)

    self.execute("G0 Z3")
    yield interpreter.INTERP_EXECUTE_FINISH
    check_coords(self, 'z', 3)

    self.execute("(epilog_M406 end)")
    yield interpreter.INTERP_OK
