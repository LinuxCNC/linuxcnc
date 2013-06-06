#!/usr/bin/python

# makefile dependency generator for pasm .p -> .bin files
# based on http://code.activestate.com/recipes/66541


import sys, os, re, string

dotOext = ".bin"

dotExtRe = re.compile("\.[a-zA-Z]+")
includeRe = re.compile("^#[ \t]*include[ \t]*")

for file in sys.argv[1:]:
    inf = open(file)
    myIncludes = [ ]
    for x in inf.readlines():
        m = includeRe.search(x)
        if m != None:
            x = x[m.regs[0][1]:-1]
            if x[0] == '"':
                x = x[1:-1]
                if x not in myIncludes:
                    myIncludes.append(x)
    inf.close()

    m = dotExtRe.search(file)
    assert m != None
    dotOFile = file[:m.regs[0][0]] + dotOext

    sys.stdout.write(dotOFile + ": " + file)
    for x in myIncludes:
        sys.stdout.write(" " + x)
    sys.stdout.write("\n")

