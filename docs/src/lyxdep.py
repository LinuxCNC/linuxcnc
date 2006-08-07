#!/usr/bin/python
import re, sys, os

include_re = re.compile("\\\\(input|include){(?P<filename>[^}]*)}")
begin_re = re.compile("\\\\begin_inset Graphics")
end_re = re.compile("\\\\end_inset")
filename_re = re.compile("filename (?P<filename>.*)$")

processed = []

def process_file(name):
    if name in processed: return
    print name,
    processed.append(name)

    r = os.path.dirname(name)

    f = open(name)
    st = 0
    for line in f.readlines():
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
print "%s.pdf %s.d:" % (o, o),

process_file(sys.argv[1])
print
