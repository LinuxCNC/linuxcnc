import os, sys, sgmllib, cookielib, urllib, htmlentitydefs

if len(sys.argv) > 1:
    ref = sys.argv[1]
else:
    ref = "../html/gcode.html"

if len(sys.argv) > 2:
    main = sys.argv[2]
else:
    main = ref.replace("gcode", "gcode_gcode")

def get(attr, attrs, default=""):
    attr = attr.lower()
    for k, v in attrs:
        if k.lower() == attr: return v
    return default

class MetaHandler:
    def do_meta(self,  attrs):
        equiv = get("http-equiv", attrs)
        content = get("content", attrs)
        if equiv != "content-type": return
        attrs = cookielib.split_header_words([content])[0]
        encoding = get("charset", attrs)
        if encoding == "ASCII": encoding = "ISO-8859-1"
        if encoding: self.encoding = encoding

class get_refs(sgmllib.SGMLParser, MetaHandler):
    entitydefs = htmlentitydefs.entitydefs

    def __init__(self, verbose=0):
        sgmllib.SGMLParser.__init__(self, verbose)
        self.refs = set()
        self.encoding = None

    def do_a(self, attrs):
        href = get('href', attrs)
        if self.encoding:
            href = href.decode(self.encoding)
        href = urllib.unquote(href)
        if "#" in href:
            a, b = href.split("#")
            if b:
                self.refs.add(b)

class get_anchors(sgmllib.SGMLParser, MetaHandler):
    entitydefs = htmlentitydefs.entitydefs

    def __init__(self, verbose=0):
        sgmllib.SGMLParser.__init__(self, verbose)
        self.anchors = set()
        self.encoding = None

    def unknown_starttag(self, tag, attrs):
        id = get('id', attrs)
        if id:
            self.do_a([('name', id)])

    def unknown_endtag(self, tag): pass

    def do_a(self, attrs):
        name = get('name', attrs, get('id', attrs))
        if self.encoding:
            name = name.decode(self.encoding)
        name = urllib.unquote(name)
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
        print "\t%r" % i
    raise SystemExit, 1
