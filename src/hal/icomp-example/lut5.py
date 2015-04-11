#!/usr/bin/python

# script to determine function value for lut5.9 given a
# boolean expression using inputs 0-4 (named i0..i4)

# examples from manpage:
# lut5 'i0 &  i1 &  i2 &  i3 &  i4'
# lut5 'i0 |  i1 |  i2 |  i3 |  i4'
# lut5 -n2 'i0 ^  i1'
#
# a two-line mux:
# lut5 -n3 '(i2 and i1) or (not i2 and i0)'
#
# Note that the "~" operator may not be used for negation, because
# in Python, ~True = -2(!)

import sys,os
from optparse import Option, OptionParser
options = [ Option( '-n', dest='inputs', metavar='<number of inputs>',default=5
                    , help="Set number of inputs (default 5, named i0..i4)")
                    ]

def main():
    usage = "usage: %prog [options] expression"
    parser = OptionParser(usage=usage)
    parser.disable_interspersed_args()
    parser.add_options(options)

    (opts, args) = parser.parse_args()

    if not args:
        parser.print_help()
        print "examples:"
        print "  lut5 'i0 &  i1 &  i2 &  i3 &  i4'"
        print "  lut5 'i0 |  i1 |  i2 |  i3 |  i4'"
        print "  lut5 -n2 'i0 ^  i1'"
        print "# a two-line mux:"
        print "  lut5 -n3 '(i2 and i1) or (not i2 and i0)'"
        sys.exit(1)
    expression = args[0]

    print "# expression = %s" % (expression)
    print "#in: i4 i3 i2 i1 i0 out weight"

    function = 0
    for i in range(1 << int(opts.inputs)):
        weight = 1 << i
        i0 = True if i & 1 else False
        i1 = True if i & 2 else False
        i2 = True if i & 4 else False
        i3 = True if i & 8 else False
        i4 = True if i & 16 else False
        result = eval(expression)
        print "#%2d:  %d  %d  %d  %d  %d  %d" % (i,int(i4),int(i3),int(i2),int(i1),int(i0),int(result)),
        if result:
            function += weight
            print "  0x%x" % (weight)
        else:
            print
    print "# setp lut5.N.function 0x%x" % (function)


if __name__ == '__main__':
    main()
