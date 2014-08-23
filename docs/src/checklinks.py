import os, sys, sgmllib, cookielib, urllib, htmlentitydefs

if len(sys.argv) > 1:
    ref = sys.argv[1]
else:
    ref = "../html/gcode.html"

if len(sys.argv) > 2:
    targets = sys.argv[2:]
else:
    targets = None

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
	self.refs.add(href)

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

_anchors = {}
def get_anchors_cached(filename):
    if filename not in _anchors:
	a = get_anchors()
	a.feed(open(filename).read())
	_anchors[filename] = a.anchors
    return _anchors[filename]

def resolve_file(src, target):
    if "#" in target:
	a, b = target.split("#", 1)
    else:
	a, b = target, None

    a = a or src

    return os.path.join(os.path.dirname(ref), a), b

def resolve(target, anchor):
    if not anchor: return True

    anchors = get_anchors_cached(target)
    return anchor in anchors

refs = get_refs()
refs.feed(open(ref).read())
refs = refs.refs

missing_anchor = set()
missing_file = set()
unlisted_targets = set()
good = set()
for r in refs:
    target, anchor = resolve_file(ref, r)
    if targets and not target in targets:
	unlisted_targets.add(target)
    elif not os.path.exists(target):
	missing_file.add(r)
    elif not resolve(target, anchor):
	missing_anchor.add(r)
    else:
	good.add(r)

if missing_file:
    print "Files linked to in %s but could not be found:" % (
        os.path.basename(ref),)
    for i in sorted(missing_file):
        print "\t%r" % i
if missing_anchor:
    print "Anchors used in %s but not defined in linked file:" % (
        os.path.basename(ref),)
    for i in sorted(missing_anchor):
        print "\t%r" % i
if unlisted_targets:
    print "Links to files not listed as targets:"
    for i in sorted(unlisted_targets):
	print "\t%r" % i
    print "If all link targets are not listed in the Submakefile, then the results of this program is unreliable."
print "Good links: %d/%d" % (len(good), len(refs))
if missing_anchor or missing_file or unlisted_targets:
    raise SystemExit, 1
