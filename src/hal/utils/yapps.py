#!/usr/bin/python

#
# Yapps 2 - yet another python parser system
# Copyright 1999-2003 by Amit J. Patel <amitp@cs.stanford.edu>
#
# This version of Yapps 2 can be distributed under the
# terms of the MIT open source license, either found in the LICENSE file
# included with the Yapps distribution
# <http://theory.stanford.edu/~amitp/yapps/> or at
# <http://www.opensource.org/licenses/mit-license.php>
#

import sys, os, re
BASE = os.path.abspath(os.path.join(os.path.dirname(sys.argv[0]), ".."))
sys.path.insert(0, os.path.join(BASE, "lib", "python"))

from yapps import runtime, parsetree

def generate(inputfilename, outputfilename='', dump=0, **flags):
    """Generate a grammar, given an input filename (X.g)
    and an output filename (defaulting to X.py)."""

    if not outputfilename:
        if inputfilename.endswith('.g'):
            outputfilename = inputfilename[:-2] + '.py'
        else:
            raise Exception('Must specify output filename if input filename is not *.g')
        
    DIVIDER = '\n%%\n' # This pattern separates the pre/post parsers
    preparser, postparser = None, None # Code before and after the parser desc

    # Read the entire file
    s = open(inputfilename,'r').read()

    # See if there's a separation between the pre-parser and parser
    f = s.find(DIVIDER)
    if f >= 0: preparser, s = s[:f]+'\n\n', s[f+len(DIVIDER):]

    # See if there's a separation between the parser and post-parser
    f = s.find(DIVIDER)
    if f >= 0: s, postparser = s[:f], '\n\n'+s[f+len(DIVIDER):]

    # Create the parser and scanner and parse the text
    scanner = grammar.ParserDescriptionScanner(s, filename=inputfilename)
    if preparser: scanner.del_line += preparser.count('\n')

    parser = grammar.ParserDescription(scanner)
    t = runtime.wrap_error_reporter(parser, 'Parser')
    if t is None: return 1 # Failure
    if preparser is not None: t.preparser = preparser
    if postparser is not None: t.postparser = postparser

    # Check the options
    for f in t.options.keys():
        for opt,_,_ in yapps_options:
            if f == opt: break
        else:
            print >>sys.stderr, 'Warning: unrecognized option', f
    # Add command line options to the set
    for f in flags.keys(): t.options[f] = flags[f]
            
    # Generate the output
    if dump:
        t.dump_information()
    else:
        t.output = open(outputfilename, 'w')
        t.generate_output()
    return 0

if __name__ == '__main__':
    import doctest
    doctest.testmod(sys.modules['__main__'])
    doctest.testmod(parsetree)

    # Someday I will use optparse, but Python 2.3 is too new at the moment.
    yapps_options = [
        ('context-insensitive-scanner',
         'context-insensitive-scanner',
         'Scan all tokens (see docs)'),
        ]

    import getopt
    optlist, args = getopt.getopt(sys.argv[1:], 'f:', ['help', 'dump', 'use-devel-grammar'])
    if not args or len(args) > 2:
        print >>sys.stderr, 'Usage:'
        print >>sys.stderr, '  python', sys.argv[0], '[flags] input.g [output.py]'
        print >>sys.stderr, 'Flags:'
        print >>sys.stderr, ('  --dump' + ' '*40)[:35] + 'Dump out grammar information'
        print >>sys.stderr, ('  --use-devel-grammar' + ' '*40)[:35] + 'Use the devel grammar parser from yapps_grammar.py instead of the stable grammar from grammar.py'
        for flag, _, doc in yapps_options:
            print >>sys.stderr, ('  -f' + flag + ' '*40)[:35] + doc
    else:
        # Read in the options and create a list of flags
        flags = {}
        use_devel_grammar = 0
        for opt in optlist:
            for flag, name, _ in yapps_options:
                if opt == ('-f', flag):
                    flags[name] = 1
                    break
            else:
                if opt == ('--dump', ''):
                    flags['dump'] = 1
                elif opt == ('--use-devel-grammar', ''):
                    use_devel_grammar = 1
                else:
                    print >>sys.stderr, 'Warning: unrecognized option', opt[0], opt[1]

        if use_devel_grammar:
            import yapps_grammar as grammar
        else:
            from yapps import grammar
            
        sys.exit(generate(*tuple(args), **flags))
