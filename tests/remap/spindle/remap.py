from __future__ import print_function

def m500(self, **words):
    print("M500 P{} {{".format(int(words['p'])))
    print("  active spindle: ", self.active_spindle)
    print("  spindle speeds: ", list(self.speed))
    print("}")
