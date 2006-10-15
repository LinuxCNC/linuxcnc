# parsetree.py, part of Yapps 2 - yet another python parser system
# Copyright 1999-2003 by Amit J. Patel <amitp@cs.stanford.edu>
#
# This version of the Yapps 2 Runtime can be distributed under the
# terms of the MIT open source license, either found in the LICENSE file
# included with the Yapps distribution
# <http://theory.stanford.edu/~amitp/yapps/> or at
# <http://www.opensource.org/licenses/mit-license.php>
#

"""Classes used to represent parse trees and generate output.

This module defines the Generator class, which drives the generation
of Python output from a grammar parse tree.  It also defines nodes
used to represent the parse tree; they are derived from class Node.

The main logic of Yapps is in this module.
"""

import sys, re

######################################################################
INDENT = ' '*4
class Generator:

    # TODO: many of the methods here should be class methods, not instance methods
    
    def __init__(self, name, options, tokens, rules):
        self.change_count = 0
        self.name = name
        self.options = options
        self.preparser = ''
        self.postparser = None
        
        self.tokens = {} # Map from tokens to regexps
        self.ignore = {} # List of token names to ignore in parsing, map to statements
        self.terminals = [] # List of token names (to maintain ordering)
        for t in tokens:
            if len(t) == 3:
                n,t,s = t
            else:
                n,t = t
                s = None

            if n == '#ignore':
                n = t
                self.ignore[n] = s
            if n in self.tokens.keys() and self.tokens[n] != t:
                print >>sys.stderr, 'Warning: token %s defined more than once.' % n
            self.tokens[n] = t
            self.terminals.append(n)
            
        self.rules = {} # Map from rule names to parser nodes
        self.params = {} # Map from rule names to parameters
        self.goals = [] # List of rule names (to maintain ordering)
        for n,p,r in rules:
            self.params[n] = p
            self.rules[n] = r
            self.goals.append(n)
            
        self.output = sys.stdout

    def has_option(self, name):
        return self.options.get(name, 0)
    
    def non_ignored_tokens(self):
        return [x for x in self.terminals if x not in self.ignore]
    
    def changed(self):
        """Increments the change count.

        >>> t = Generator('', [], [], [])
        >>> old_count = t.change_count
        >>> t.changed()
        >>> assert t.change_count == old_count + 1
        """
        self.change_count = 1+self.change_count

    def set_subtract(self, a, b):
        """Returns the elements of a that are not in b.

        >>> t = Generator('', [], [], [])
        >>> t.set_subtract([], [])
        []
        >>> t.set_subtract([1, 2], [1, 2])
        []
        >>> t.set_subtract([1, 2, 3], [2])
        [1, 3]
        >>> t.set_subtract([1], [2, 3, 4])
        [1]
        """
        result = []
        for x in a:
            if x not in b:
                result.append(x)
        return result
    
    def subset(self, a, b):
        """True iff all elements of sequence a are inside sequence b

        >>> t = Generator('', [], [], [])
        >>> t.subset([], [1, 2, 3])
        1
        >>> t.subset([1, 2, 3], [])
        0
        >>> t.subset([1], [1, 2, 3])
        1
        >>> t.subset([3, 2, 1], [1, 2, 3])
        1
        >>> t.subset([1, 1, 1], [1, 2, 3])
        1
        >>> t.subset([1, 2, 3], [1, 1, 1])
        0
        """
        for x in a:
            if x not in b:
                return 0
        return 1

    def equal_set(self, a, b):
        """True iff subset(a, b) and subset(b, a)

        >>> t = Generator('', [], [], [])
        >>> a_set = [1, 2, 3]
        >>> t.equal_set(a_set, a_set)
        1
        >>> t.equal_set(a_set, a_set[:])
        1
        >>> t.equal_set([], a_set)
        0
        >>> t.equal_set([1, 2, 3], [3, 2, 1])
        1
        """
        if len(a) != len(b): return 0
        if a == b: return 1
        return self.subset(a, b) and self.subset(b, a)
    
    def add_to(self, parent, additions):
        "Modify _parent_ to include all elements in _additions_"
        for x in additions:
            if x not in parent:
                parent.append(x)
                self.changed()

    def equate(self, a, b):
        """Extend (a) and (b) so that they contain each others' elements.

        >>> t = Generator('', [], [], [])
        >>> a = [1, 2]
        >>> b = [2, 3]
        >>> t.equate(a, b)
        >>> a
        [1, 2, 3]
        >>> b
        [2, 3, 1]
        """
        self.add_to(a, b)
        self.add_to(b, a)

    def write(self, *args):
        for a in args:
            self.output.write(a)

    def in_test(self, expr, full, set):
        """Generate a test of (expr) being in (set), where (set) is a subset of (full)

        expr is a string (Python expression)
        set is a list of values (which will be converted with repr)
        full is the list of all values expr could possibly evaluate to
        
        >>> t = Generator('', [], [], [])
        >>> t.in_test('x', [1,2,3,4], [])
        '0'
        >>> t.in_test('x', [1,2,3,4], [1,2,3,4])
        '1'
        >>> t.in_test('x', [1,2,3,4], [1])
        'x == 1'
        >>> t.in_test('a+b', [1,2,3,4], [1,2])
        'a+b in [1, 2]'
        >>> t.in_test('x', [1,2,3,4,5], [1,2,3])
        'x not in [4, 5]'
        >>> t.in_test('x', [1,2,3,4,5], [1,2,3,4])
        'x != 5'
        """
        
        if not set: return '0'
        if len(set) == 1: return '%s == %s' % (expr, repr(set[0]))
        if full and len(set) > len(full)/2:
            # Reverse the sense of the test.
            not_set = [x for x in full if x not in set]
            return self.not_in_test(expr, full, not_set)
        return '%s in %s' % (expr, repr(set))
    
    def not_in_test(self, expr, full, set):
        """Like in_test, but the reverse test."""
        if not set: return '1'
        if len(set) == 1: return '%s != %s' % (expr, repr(set[0]))
        return '%s not in %s' % (expr, repr(set))

    def peek_call(self, a):
        """Generate a call to scan for a token in the set 'a'"""
        assert type(a) == type([])
        a_set = (repr(a)[1:-1])
        if self.equal_set(a, self.non_ignored_tokens()): a_set = ''
        if self.has_option('context-insensitive-scanner'): a_set = ''
        if a_set: a_set += ","
        
        return 'self._peek(%s context=_context)' % a_set
    
    def peek_test(self, a, b):
        """Generate a call to test whether the next token (which could be any of
        the elements in a) is in the set b."""
        if self.subset(a, b): return '1'
        if self.has_option('context-insensitive-scanner'): a = self.non_ignored_tokens()
        return self.in_test(self.peek_call(a), a, b)

    def not_peek_test(self, a, b):
        """Like peek_test, but the opposite sense."""
        if self.subset(a, b): return '0'
        return self.not_in_test(self.peek_call(a), a, b)

    def calculate(self):
        """The main loop to compute the epsilon, first, follow sets.
        The loop continues until the sets converge.  This works because
        each set can only get larger, so when they stop getting larger,
        we're done."""
        # First we determine whether a rule accepts epsilon (the empty sequence)
        while 1:
            for r in self.goals:
                self.rules[r].setup(self)
            if self.change_count == 0: break
            self.change_count = 0

        # Now we compute the first/follow sets
        while 1:
            for r in self.goals:
                self.rules[r].update(self)
            if self.change_count == 0: break
            self.change_count = 0

    def dump_information(self):
        """Display the grammar in somewhat human-readable form."""
        self.calculate()
        for r in self.goals:
            print '    _____' + '_'*len(r)
            print ('___/Rule '+r+'\\' + '_'*80)[:79]
            queue = [self.rules[r]]
            while queue:
                top = queue[0]
                del queue[0]

                print 'Rule', repr(top), 'of class', top.__class__.__name__
                top.first.sort()
                top.follow.sort()
                eps = []
                if top.accepts_epsilon: eps = ['(null)']
                print '     FIRST:', ', '.join(top.first+eps)
                print '    FOLLOW:', ', '.join(top.follow)
                for x in top.get_children(): queue.append(x)
                
    def repr_ignore(self):
        out="{"
        for t,s in self.ignore.iteritems():
            if s is None: s=repr(s)
            out += "%s:%s," % (repr(t),s)
        out += "}"
        return out
        
    def generate_output(self):
        self.calculate()
        self.write(self.preparser)
        self.write("# Begin -- grammar generated by Yapps\n")
        self.write("import sys, re\n")
        self.write("from yapps import runtime\n")
        self.write("\n")
        self.write("class ", self.name, "Scanner(runtime.Scanner):\n")
        self.write("    patterns = [\n")
        for p in self.terminals:
            self.write("        (%s, re.compile(%s)),\n" % (
                repr(p), repr(self.tokens[p])))
        self.write("    ]\n")
        self.write("    def __init__(self, str,*args,**kw):\n")
        self.write("        runtime.Scanner.__init__(self,None,%s,str,*args,**kw)\n" %
                   self.repr_ignore())
        self.write("\n")
        
        self.write("class ", self.name, "(runtime.Parser):\n")
        self.write(INDENT, "Context = runtime.Context\n")
        for r in self.goals:
            self.write(INDENT, "def ", r, "(self")
            if self.params[r]: self.write(", ", self.params[r])
            self.write(", _parent=None):\n")
            self.write(INDENT+INDENT, "_context = self.Context(_parent, self._scanner, %s, [%s])\n" %
                       (repr(r), self.params.get(r, '')))
            self.rules[r].output(self, INDENT+INDENT)
            self.write("\n")

        self.write("\n")
        self.write("def parse(rule, text):\n")
        self.write("    P = ", self.name, "(", self.name, "Scanner(text))\n")
        self.write("    return runtime.wrap_error_reporter(P, rule)\n")
        self.write("\n")
        if self.postparser is not None:
            self.write("# End -- grammar generated by Yapps\n")
            self.write(self.postparser)
        else:
            self.write("if __name__ == '__main__':\n")
            self.write(INDENT, "from sys import argv, stdin\n")
            self.write(INDENT, "if len(argv) >= 2:\n")
            self.write(INDENT*2, "if len(argv) >= 3:\n")
            self.write(INDENT*3, "f = open(argv[2],'r')\n")
            self.write(INDENT*2, "else:\n")
            self.write(INDENT*3, "f = stdin\n")
            self.write(INDENT*2, "print parse(argv[1], f.read())\n")
            self.write(INDENT, "else: print >>sys.stderr, 'Args:  <rule> [<filename>]'\n")
            self.write("# End -- grammar generated by Yapps\n")

######################################################################
class Node:
    """This is the base class for all components of a grammar."""
    def __init__(self, rule):
        self.rule = rule # name of the rule containing this node
        self.first = []
        self.follow = []
        self.accepts_epsilon = 0
        
    def setup(self, gen):
        # Setup will change accepts_epsilon,
        # sometimes from 0 to 1 but never 1 to 0.
        # It will take a finite number of steps to set things up
        pass
    
    def used(self, vars):
        "Return two lists: one of vars used, and the other of vars assigned"
        return vars, []

    def get_children(self):
        "Return a list of sub-nodes"
        return []
    
    def __repr__(self):
        return str(self)
    
    def update(self, gen):
        if self.accepts_epsilon:
            gen.add_to(self.first, self.follow)

    def output(self, gen, indent):
        "Write out code to _gen_ with _indent_:string indentation"
        gen.write(indent, "assert 0 # Invalid parser node\n")
    
class Terminal(Node):
    """This class stores terminal nodes, which are tokens."""
    def __init__(self, rule, token):
        Node.__init__(self, rule)
        self.token = token
        self.accepts_epsilon = 0

    def __str__(self):
        return self.token

    def update(self, gen):
        Node.update(self, gen)
        if self.first != [self.token]:
            self.first = [self.token]
            gen.changed()

    def output(self, gen, indent):
        gen.write(indent)
        if re.match('[a-zA-Z_][a-zA-Z_0-9]*$', self.token):
            gen.write(self.token, " = ")
        gen.write("self._scan(%s, context=_context)\n" % repr(self.token))
        
class Eval(Node):
    """This class stores evaluation nodes, from {{ ... }} clauses."""
    def __init__(self, rule, expr):
        Node.__init__(self, rule)
        self.expr = expr

    def setup(self, gen):
        Node.setup(self, gen)
        if not self.accepts_epsilon:
            self.accepts_epsilon = 1
            gen.changed()

    def __str__(self):
        return '{{ %s }}' % self.expr.strip()

    def output(self, gen, indent):
        gen.write(indent, self.expr.strip(), '\n')
        
class NonTerminal(Node):
    """This class stores nonterminal nodes, which are rules with arguments."""
    def __init__(self, rule, name, args):
        Node.__init__(self, rule)
        self.name = name
        self.args = args

    def setup(self, gen):
        Node.setup(self, gen)
        try:
            self.target = gen.rules[self.name]
            if self.accepts_epsilon != self.target.accepts_epsilon:
                self.accepts_epsilon = self.target.accepts_epsilon
                gen.changed()
        except KeyError: # Oops, it's nonexistent
            print >>sys.stderr, 'Error: no rule <%s>' % self.name
            self.target = self
            
    def __str__(self):
        return '%s' % self.name

    def update(self, gen):
        Node.update(self, gen)
        gen.equate(self.first, self.target.first)
        gen.equate(self.follow, self.target.follow)

    def output(self, gen, indent):
        gen.write(indent)
        gen.write(self.name, " = ")
        args = self.args
        if args: args += ', '
        args += '_context'
        gen.write("self.", self.name, "(", args, ")\n")
        
class Sequence(Node):
    """This class stores a sequence of nodes (A B C ...)"""
    def __init__(self, rule, *children):
        Node.__init__(self, rule)
        self.children = children

    def setup(self, gen):
        Node.setup(self, gen)
        for c in self.children: c.setup(gen)
        
        if not self.accepts_epsilon:
            # If it's not already accepting epsilon, it might now do so.
            for c in self.children:
                # any non-epsilon means all is non-epsilon
                if not c.accepts_epsilon: break
            else:
                self.accepts_epsilon = 1
                gen.changed()

    def get_children(self):
        return self.children
    
    def __str__(self):
        return '( %s )' % ' '.join(map(str, self.children))

    def update(self, gen):
        Node.update(self, gen)
        for g in self.children:
            g.update(gen)

        empty = 1
        for g_i in range(len(self.children)):
            g = self.children[g_i]

            if empty: gen.add_to(self.first, g.first)
            if not g.accepts_epsilon: empty = 0
            
            if g_i == len(self.children)-1:
                next = self.follow
            else:
                next = self.children[1+g_i].first
            gen.add_to(g.follow, next)

        if self.children:
            gen.add_to(self.follow, self.children[-1].follow)

    def output(self, gen, indent):
        if self.children:
            for c in self.children:
                c.output(gen, indent)
        else:
            # Placeholder for empty sequences, just in case
            gen.write(indent, 'pass\n')
            
class Choice(Node):
    """This class stores a choice between nodes (A | B | C | ...)"""
    def __init__(self, rule, *children):
        Node.__init__(self, rule)
        self.children = children

    def setup(self, gen):
        Node.setup(self, gen)
        for c in self.children: c.setup(gen)
            
        if not self.accepts_epsilon:
            for c in self.children:
                if c.accepts_epsilon:
                    self.accepts_epsilon = 1
                    gen.changed()

    def get_children(self):
        return self.children
    
    def __str__(self):
        return '( %s )' % ' | '.join(map(str, self.children))

    def update(self, gen):
        Node.update(self, gen)
        for g in self.children:
            g.update(gen)

        for g in self.children:
            gen.add_to(self.first, g.first)
            gen.add_to(self.follow, g.follow)
        for g in self.children:
            gen.add_to(g.follow, self.follow)
        if self.accepts_epsilon:
            gen.add_to(self.first, self.follow)

    def output(self, gen, indent):
        test = "if"
        gen.write(indent, "_token = ", gen.peek_call(self.first), "\n")
        tokens_seen = []
        tokens_unseen = self.first[:]
        if gen.has_option('context-insensitive-scanner'):
            # Context insensitive scanners can return ANY token,
            # not only the ones in first.
            tokens_unseen = gen.non_ignored_tokens()
        for c in self.children:
            testset = c.first[:]
            removed = []
            for x in testset:
                if x in tokens_seen:
                    testset.remove(x)
                    removed.append(x)
                if x in tokens_unseen: tokens_unseen.remove(x)
            tokens_seen = tokens_seen + testset
            if removed:
                if not testset:
                    print >>sys.stderr, 'Error in rule', self.rule+':'
                else:
                    print >>sys.stderr, 'Warning in rule', self.rule+':'
                print >>sys.stderr, ' *', self
                print >>sys.stderr, ' * These tokens could be matched by more than one clause:'
                print >>sys.stderr, ' *', ' '.join(removed)
                
            if testset:
                if not tokens_unseen: # context sensitive scanners only!
                    if test == 'if':
                        # if it's the first AND last test, then
                        # we can simply put the code without an if/else
                        c.output(gen, indent)
                    else:
                        gen.write(indent, "else:")
                        t = gen.in_test('', [], testset)
                        if len(t) < 70-len(indent):
                            gen.write(' #', t)
                        gen.write("\n")
                        c.output(gen, indent+INDENT)
                else:
                    gen.write(indent, test, " ",
                              gen.in_test('_token', tokens_unseen, testset),
                              ":\n")
                    c.output(gen, indent+INDENT)
                test = "elif"

        if tokens_unseen:
            gen.write(indent, "else:\n")
            gen.write(indent, INDENT, "raise runtime.SyntaxError(_token[0], ")
            gen.write("'Could not match ", self.rule, "')\n")
        
class Wrapper(Node):
    """This is a base class for nodes that modify a single child."""
    def __init__(self, rule, child):
        Node.__init__(self, rule)
        self.child = child

    def setup(self, gen):
        Node.setup(self, gen)
        self.child.setup(gen)

    def get_children(self):
        return [self.child]
    
    def update(self, gen):
        Node.update(self, gen)
        self.child.update(gen)
        gen.add_to(self.first, self.child.first)
        gen.equate(self.follow, self.child.follow)

class Option(Wrapper):
    """This class represents an optional clause of the form [A]"""
    def setup(self, gen):
        Wrapper.setup(self, gen)
        if not self.accepts_epsilon:
            self.accepts_epsilon = 1
            gen.changed()

    def __str__(self):
        return '[ %s ]' % str(self.child)

    def output(self, gen, indent):
        if self.child.accepts_epsilon:
            print >>sys.stderr, 'Warning in rule', self.rule+': contents may be empty.'
        gen.write(indent, "if %s:\n" %
                  gen.peek_test(self.first, self.child.first))
        self.child.output(gen, indent+INDENT)

        if gen.has_option('context-insensitive-scanner'):
            gen.write(indent, "if %s:\n" %
                    gen.not_peek_test(gen.non_ignored_tokens(), self.follow))
            gen.write(indent+INDENT, "raise runtime.SyntaxError(pos=self._scanner.get_pos(), context=_context, msg='Need one of ' + ', '.join(%s))\n" %
                    repr(self.first))

        
class Plus(Wrapper):
    """This class represents a 1-or-more repetition clause of the form A+"""
    def setup(self, gen):
        Wrapper.setup(self, gen)
        if self.accepts_epsilon != self.child.accepts_epsilon:
            self.accepts_epsilon = self.child.accepts_epsilon
            gen.changed()

    def __str__(self):
        return '%s+' % str(self.child)

    def update(self, gen):
        Wrapper.update(self, gen)
        gen.add_to(self.child.follow, self.child.first)
        
    def output(self, gen, indent):
        if self.child.accepts_epsilon:
            print >>sys.stderr, 'Warning in rule', self.rule+':'
            print >>sys.stderr, ' * The repeated pattern could be empty.  The resulting parser may not work properly.'
        gen.write(indent, "while 1:\n")
        self.child.output(gen, indent+INDENT)
        union = self.first[:]
        gen.add_to(union, self.follow)
        gen.write(indent+INDENT, "if %s: break\n" %
                  gen.not_peek_test(union, self.child.first))

        if gen.has_option('context-insensitive-scanner'):
            gen.write(indent, "if %s:\n" %
                    gen.not_peek_test(gen.non_ignored_tokens(), self.follow))
            gen.write(indent+INDENT, "raise runtime.SyntaxError(pos=self._scanner.get_pos(), context=_context, msg='Need one of ' + ', '.join(%s))\n" %
                    repr(self.first))


class Star(Wrapper):
    """This class represents a 0-or-more repetition clause of the form A*"""
    def setup(self, gen):
        Wrapper.setup(self, gen)
        if not self.accepts_epsilon:
            self.accepts_epsilon = 1
            gen.changed()

    def __str__(self):
        return '%s*' % str(self.child)

    def update(self, gen):
        Wrapper.update(self, gen)
        gen.add_to(self.child.follow, self.child.first)
        
    def output(self, gen, indent):
        if self.child.accepts_epsilon:
            print >>sys.stderr, 'Warning in rule', self.rule+':'
            print >>sys.stderr, ' * The repeated pattern could be empty.  The resulting parser probably will not work properly.'
        gen.write(indent, "while %s:\n" %
                  gen.peek_test(self.follow, self.child.first))
        self.child.output(gen, indent+INDENT)

        # TODO: need to generate tests like this in lots of rules
        if gen.has_option('context-insensitive-scanner'):
            gen.write(indent, "if %s:\n" %
                    gen.not_peek_test(gen.non_ignored_tokens(), self.follow))
            gen.write(indent+INDENT, "raise runtime.SyntaxError(pos=self._scanner.get_pos(), context=_context, msg='Need one of ' + ', '.join(%s))\n" %
                    repr(self.first))

