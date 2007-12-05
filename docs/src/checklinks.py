import os, sys, sgmllib

if len(sys.argv) > 1:
    ref = sys.argv[1]
else:
    ref = "../html/gcode.html"

if len(sys.argv) > 2:
    main = sys.argv[2]
else:
    main = ref.replace("gcode", "gcode_main")

def get(attr, attrs, default=""):
    for k, v in attrs:
        if k == attr: return v
    return default

class get_refs(sgmllib.SGMLParser):
    def __init__(self, verbose=0):
        sgmllib.SGMLParser.__init__(self, verbose)
        self.refs = set()

    def do_a(self, attrs):
        href = get('href', attrs)
        if "#" in href:
            a, b = href.split("#")
            if b:
                self.refs.add(b)


class get_anchors(sgmllib.SGMLParser):
    def __init__(self, verbose=0):
        sgmllib.SGMLParser.__init__(self, verbose)
        self.anchors = set()

    def do_a(self, attrs):
        name = get('name', attrs)
        if name:
            self.anchors.add(name)


r = get_refs()
r.feed(open(ref).read())
r = r.refs

a = get_anchors()
a.feed(open(main).read())
a = a.anchors

missing = r - a
if missing:
    print "Anchors used in %s but not defined in %s:" % (
        os.path.basename(ref), os.path.basename(main))
    for i in missing:
        print "\t%s" % i
    raise SystemExit, 1
