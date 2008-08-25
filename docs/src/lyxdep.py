#!/usr/bin/python
import re, sys, os

include_re = re.compile("\\\\(input|include|verbatiminput){(?P<filename>[^}]*)}")
begin_re = re.compile("\\\\begin_inset Graphics")
end_re = re.compile("\\\\end_inset")
filename_re = re.compile("filename (?P<filename>.*)$")
format_re = re.compile("\\\\lyxformat (\d+)$")

processed = []

def process_file(name):
    if name in processed: return
    print name,
    processed.append(name)

    r = os.path.dirname(name)

    f = open(name)
    st = 0
    for line in f.readlines():
        m = format_re.search(line)
        if m:
            if m.group(1) != "221":
                raise SystemExit, """\
LyX documentation must be written with lyxformat 221 (the format written by
LyX 1.3.7 on Ubuntu 6.06 Dapper Drake).  For more information, see README
or http://wiki.linuxcnc.org/cgi-bin/emcinfo.pl?BeyondWiki"""
        m = include_re.search(line)
        if m:
            f = os.path.join(r, m.group("filename"))
            process_file(f)
            continue
        m = begin_re.search(line)
        if m:
            st = 1
            continue
        m = end_re.search(line)
        if m:
            st = 0
            continue
        m = filename_re.search(line)
        if st and m:
            print os.path.join(r, m.group("filename")),

o = os.path.splitext(sys.argv[1])[0]
if len(sys.argv) > 2:
	print " ".join(sys.argv[2:]),
print "%s.pdf:" % o,

process_file(sys.argv[1])
print
