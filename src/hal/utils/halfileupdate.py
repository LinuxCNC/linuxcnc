#!/usr/bin/env python3
#
#    halfileupdate - update HAL configuration files after component,
#                    pin and parameter renames
#    Copyright 2026 Luca Toniolo
#
#    This program is free software; you can redistribute it and/or modify
#    it under the terms of the GNU General Public License as published by
#    the Free Software Foundation; either version 2 of the License, or
#    (at your option) any later version.
#
#    This program is distributed in the hope that it will be useful,
#    but WITHOUT ANY WARRANTY; without even the implied warranty of
#    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#    GNU General Public License for more details.
#
#    You should have received a copy of the GNU General Public License
#    along with this program; if not, write to the Free Software
#    Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
#
# Killing off the 32-bit HAL types renames components, pins and
# parameters whose name spells a type that no longer exists
# ('conv_s32_float' -> 'conv_sint_real', 'mux-gen.00.in-s32-00' ->
# 'mux-gen.00.in-sint-00').  Configurations using those names stop
# working.  This tool rewrites them.
#
# Nothing is renamed by pattern-matching a name.  A name is rewritten
# only when it resolves to an instance of a component that a 'loadrt'
# in the processed files created, so 'names=' and 'count=' instances,
# and components whose HAL prefix differs from the module name, are
# handled without guessing.  A name that cannot be resolved is left
# alone and reported.
#
# By default a unified diff is printed and nothing is written; --in-place
# rewrites, keeping a .bak.
#
# The tool is a migration aid.  It carries the rename table itself, no
# part of HAL knows about the old names, and both are meant to be
# dropped once the release that renamed things is a few releases old.

import argparse
import difflib
import os
import re
import sys
import tempfile

# ---------------------------------------------------------------------------
# The renames.  The 32-bit HAL types are gone, so the type words in a name
# follow the types that are left: bit -> bool, float -> real, s32 -> sint,
# u32 -> uint.  Regenerate these tables when the rename set changes;
# nothing else in HAL knows the old names.
# ---------------------------------------------------------------------------

# Renamed components (the loadrt/loadusr module name).  None means the
# component is gone with no replacement: two old converters map to one new
# one (s32 and s64 both become sint) and a converter whose two sides
# collapse to the same type is not a conversion any more.  Uses of such a
# component are reported and left unchanged.
MODULE_RENAMES = {
    'conv_bit_float':   'conv_bool_real',
    'conv_bit_s32':     'conv_bool_sint',
    'conv_bit_s64':     'conv_bool_sint',
    'conv_bit_u32':     'conv_bool_uint',
    'conv_bit_u64':     'conv_bool_uint',
    'conv_float_s32':   'conv_real_sint',
    'conv_float_s64':   'conv_real_sint',
    'conv_float_u32':   'conv_real_uint',
    'conv_float_u64':   'conv_real_uint',
    'conv_s32_bit':     'conv_sint_bool',
    'conv_s32_float':   'conv_sint_real',
    'conv_s32_u32':     'conv_sint_uint',
    'conv_s32_u64':     'conv_sint_uint',
    'conv_s32_s64':     None,
    'conv_s64_bit':     'conv_sint_bool',
    'conv_s64_float':   'conv_sint_real',
    'conv_s64_u32':     'conv_sint_uint',
    'conv_s64_u64':     'conv_sint_uint',
    'conv_s64_s32':     None,
    'conv_u32_bit':     'conv_uint_bool',
    'conv_u32_float':   'conv_uint_real',
    'conv_u32_s32':     'conv_uint_sint',
    'conv_u32_s64':     'conv_uint_sint',
    'conv_u32_u64':     None,
    'conv_u64_bit':     'conv_uint_bool',
    'conv_u64_float':   'conv_uint_real',
    'conv_u64_s32':     'conv_uint_sint',
    'conv_u64_s64':     'conv_uint_sint',
    'conv_u64_u32':     None,
}

# Components whose HAL instance name is not the module name with the
# underscores turned into dashes.  Both spellings of a renamed component
# belong here, the instances are built from this.
HAL_PREFIXES = {
    'mux_generic':      'mux-gen',
    'demux_generic':    'demux-gen',
}

# Renamed pins and parameters, per component, relative to the instance
# name.  '##' matches an index and is carried over to the new name.
# 'sel-bit-NN' is not in here: it selects with bits, it is not a HAL bit.
PIN_RENAMES = {
    'mux_generic': [
        ('in-bit-##',       'in-bool-##'),
        ('in-float-##',     'in-real-##'),
        ('in-s32-##',       'in-sint-##'),
        ('in-u32-##',       'in-uint-##'),
        ('out-bit',         'out-bool'),
        ('out-float',       'out-real'),
        ('out-s32',         'out-sint'),
        ('out-u32',         'out-uint'),
    ],
    'demux_generic': [
        ('in-bit',          'in-bool'),
        ('in-float',        'in-real'),
        ('in-s32',          'in-sint'),
        ('in-u32',          'in-uint'),
        ('out-bit-##',      'out-bool-##'),
        ('out-float-##',    'out-real-##'),
        ('out-s32-##',      'out-sint-##'),
        ('out-u32-##',      'out-uint-##'),
    ],
    'demux': [
        ('sel-u32',         'sel-uint'),
    ],
    'reset': [
        ('out-bit',         'out-bool'),
        ('out-float',       'out-real'),
        ('out-s32',         'out-sint'),
        ('out-u32',         'out-uint'),
        ('reset-bit',       'reset-bool'),
        ('reset-float',     'reset-real'),
        ('reset-s32',       'reset-sint'),
        ('reset-u32',       'reset-uint'),
    ],
}

# Renamed functions, per component: the name after the instance in
# addf/delf.  A function called '_' is exported as the instance name
# itself and has nothing to rename.
FUNC_RENAMES = {
}

# Commands that carry HAL object names, and which of their arguments do.
# Argument 0 is the command itself.  Signal names are never rewritten,
# they are the user's own.
PIN_ARGS = {
    'linkps': (1,),         # linkps pin [signal]
    'linksp': (2,),         # linksp signal pin
    'linkpp': (1, 2),       # linkpp pin pin, no longer a halcmd command
    'unlinkp': (1,),
    'setp': (1,),
    'getp': (1,),
    'ptype': (1,),
}
FUNC_ARGS = {
    'addf': (1,),           # addf function thread [position]
    'delf': (1,),
}
MODULE_CMDS = ('loadrt', 'unloadrt', 'unload', 'unloadusr')
ARROWS = ('<=', '=>', '<=>', '<==', '==>')

# The old type words and what they are called now.  Used for the type on a
# 'newsig' line, which is converted, and for a user-chosen instance name
# that spells a type ('names=cvt_s32'), which keeps working and is only
# noted.
TYPE_WORDS = {
    'bit': 'bool', 'float': 'real',
    's32': 'sint', 's64': 'sint', 'u32': 'uint', 'u64': 'uint',
}

# Tcl substitutions and INI/environment references make a token
# unresolvable; they are reported instead of rewritten.
UNRESOLVABLE_RE = re.compile(r'[$\[\]{}]')

INDEXED_INSTANCE_RE = re.compile(r'^(?P<prefix>.+?)\.(?P<idx>\d+)(?:\.(?P<leaf>.+))?$')


def compile_renames(renames):
    """Compile the ## index patterns once: {module: [(pattern, template)]},
    where the pattern matches a whole pin, parameter or function name."""
    out = {}
    for module, rules in renames.items():
        for old, new in rules:
            parts, newparts = old.split('##'), new.split('##')
            pattern = re.compile(r'\A%s\Z'
                                 % r'(\d+)'.join(re.escape(p) for p in parts))
            template = ''
            for n, part in enumerate(newparts):
                if n:
                    template += '\\%d' % n
                template += part
            out.setdefault(module, []).append((pattern, template))
    return out


PIN_RULES = compile_renames(PIN_RENAMES)
FUNC_RULES = compile_renames(FUNC_RENAMES)


def prefix_of(module):
    """HAL instance prefix of a component: the module name with the
    underscores turned into dashes, as halcompile does, unless the
    component says otherwise."""
    return HAL_PREFIXES.get(module, module.replace('_', '-'))


def new_module_of(module):
    """(new name, renamed?); the new name is None when the component is
    gone with no replacement."""
    if module not in MODULE_RENAMES:
        return module, False
    return MODULE_RENAMES[module], True


def rename_name(rules, module, name):
    for pattern, template in rules.get(module, ()):
        m = pattern.match(name)
        if m:
            return m.expand(template)
    return None


def is_renamed(module):
    """Whether anything about this component changed."""
    return (module in MODULE_RENAMES or module in PIN_RULES
            or module in FUNC_RULES)


def renamed_prefixes():
    """{HAL prefix: module} of everything that changed, for recognizing a
    name whose 'loadrt' was not seen."""
    modules = set(MODULE_RENAMES) | set(PIN_RULES) | set(FUNC_RULES)
    return dict((prefix_of(module), module) for module in modules)


def in_hallib(fname):
    """(found, searched) for a HAL file that is not next to the INI file:
    whether LinuxCNC would take it from its HAL library, and whether the
    library could be looked at at all (HALLIB_PATH and HALLIB_DIR are set
    by the linuxcnc script, not necessarily in the shell running this)."""
    path = os.environ.get('HALLIB_PATH')
    dirs = [d for d in (path.split(':') if path else []) if d]
    hallib = os.environ.get('HALLIB_DIR')
    if hallib:
        dirs.append(hallib)
    if not dirs:
        return False, False
    return (any(os.path.exists(os.path.join(d, fname)) for d in dirs), True)


def rejoin(original, words):
    """Put a split name back together with its own separators."""
    seps = re.findall(r'[-_.]', original)
    out = words[0]
    for sep, word in zip(seps, words[1:]):
        out += sep + word
    return out


class Reporter:
    def __init__(self, quiet=False):
        self.quiet = quiet
        self.warnings = 0       # names that need a human
        self.notes = 0          # things worth a look, no action implied
        self.edits = 0          # mechanical changes applied
        self.filename = '-'
        self.lineno = 0

    def at(self, filename, lineno=0):
        self.filename = filename
        self.lineno = lineno

    def warn(self, msg, lineno=None):
        self.warnings += 1
        if not self.quiet:
            print("%s:%d: Warning: %s"
                  % (self.filename, self.lineno if lineno is None else lineno,
                     msg), file=sys.stderr)

    def note(self, msg, lineno=None):
        self.notes += 1
        if not self.quiet:
            print("%s:%d: Note: %s"
                  % (self.filename, self.lineno if lineno is None else lineno,
                     msg), file=sys.stderr)

    def info(self, msg):
        if not self.quiet:
            print("halfileupdate: %s" % msg, file=sys.stderr)


# ---------------------------------------------------------------------------
# HAL file lexing
# ---------------------------------------------------------------------------

def code_of(line):
    """Length of the part of the line that is not a comment, following
    halcmd's strip_comments(): '#' outside quotes starts a comment."""
    state = None
    for i, c in enumerate(line):
        if state is None:
            if c == '#':
                return i
            if c in '"\'':
                state = c
        elif c == state:
            state = None
    return len(line)


def tokens_of(code):
    """[(start, end, text)] for the whitespace separated tokens."""
    return [(m.start(), m.end(), m.group())
            for m in re.finditer(r'\S+', code)]


def splice(line, edits):
    """Apply [(start, end, text)] to a line, rightmost first."""
    for start, end, text in sorted(edits, reverse=True):
        line = line[:start] + text + line[end:]
    return line


# ---------------------------------------------------------------------------
# What is loaded
# ---------------------------------------------------------------------------

class Instance:
    """One HAL component instance, and how its name is built."""
    def __init__(self, module, prefix, index=None):
        self.module = module
        self.prefix = prefix        # instance name, or the base of it
        self.index = index          # None for a names= instance


class Loaded:
    """The components the processed files have loaded so far."""
    def __init__(self):
        self.named = {}         # instance name -> module, from names=
        self.defaults = {}      # HAL prefix -> module, from a plain loadrt
        self.counts = {}        # HAL prefix -> count, when it was given

    def add(self, module, count=None, names=None):
        if names:
            for name in names:
                self.named[name] = module
        else:
            self.defaults[prefix_of(module)] = module
            if count is not None:
                self.counts[prefix_of(module)] = count

    def resolve(self, token):
        """(Instance, leaf) for a HAL name, or (None, None).  The leaf is
        '' for the instance itself (an 'addf' of a '_' function)."""
        # names= instances first, longest match: an instance name may
        # contain dots ('names=laser.motion.type-conv')
        best = None
        for name, module in self.named.items():
            if token == name or token.startswith(name + '.'):
                if best is None or len(name) > len(best[0]):
                    best = (name, module)
        if best:
            name, module = best
            return Instance(module, name), token[len(name) + 1:]
        m = INDEXED_INSTANCE_RE.match(token)
        if m and m.group('prefix') in self.defaults:
            prefix = m.group('prefix')
            return (Instance(self.defaults[prefix], prefix, m.group('idx')),
                    m.group('leaf') or '')
        return None, None


# ---------------------------------------------------------------------------
# Conversion
# ---------------------------------------------------------------------------

class Converter:
    def __init__(self, rep):
        self.rep = rep
        self.loaded = Loaded()
        self.known_prefixes = renamed_prefixes()
        self.unresolved = set()

    # -- names ------------------------------------------------------------

    def convert_name(self, token, is_func=False, report=True):
        """The new spelling of a HAL object name, or None to leave it."""
        inst, leaf = self.loaded.resolve(token)
        if inst is None:
            if report:
                self.check_unresolved(token)
            return None
        newmodule, renamed = new_module_of(inst.module)
        if renamed and newmodule is None:
            if report:
                self.rep.warn("'%s' belongs to component '%s', which is "
                              "removed with no replacement; connect the "
                              "signal directly and delete the component"
                              % (token, inst.module))
            return None
        if inst.index is None:
            # a names= instance keeps the name the user gave it
            newname = inst.prefix
        else:
            newname = '%s.%s' % (prefix_of(newmodule if renamed
                                           else inst.module), inst.index)
            count = self.loaded.counts.get(inst.prefix)
            if report and count is not None and int(inst.index) >= count:
                self.rep.warn("'%s' is instance %s of '%s', which was loaded "
                              "with count=%d; the name is converted, but check "
                              "it" % (token, inst.index, inst.module, count))
        if leaf:
            # 'addf <instance>' with no leaf addresses a '_' function,
            # whose HAL name is the instance name itself
            newleaf = rename_name(FUNC_RULES if is_func else PIN_RULES,
                                  inst.module, leaf)
            newname += '.' + (newleaf or leaf)
        return newname if newname != token else None

    def note_legacy_name(self, name):
        """A names= instance keeps whatever the user called it, but a name
        that spells a type which no longer exists is worth pointing out."""
        words = re.split(r'[-_.]', name)
        for n, word in enumerate(words):
            new = TYPE_WORDS.get(word)
            if new is None:
                continue
            suggestion = list(words)
            suggestion[n] = new
            self.rep.note("the instance name '%s' spells '%s', a type that "
                          "is going away; the name keeps working, it is "
                          "yours, but '%s' may read better"
                          % (name, word,
                             rejoin(name, suggestion)))
            return

    def check_unresolved(self, token):
        """Report a name that looks like it belongs to a renamed component
        but whose loadrt was not in the files processed."""
        m = INDEXED_INSTANCE_RE.match(token)
        prefix = m.group('prefix') if m else token
        module = self.known_prefixes.get(prefix)
        if module is None or prefix in self.unresolved:
            return
        self.unresolved.add(prefix)
        self.rep.warn("'%s' looks like an instance of '%s', but no 'loadrt' "
                      "for it was seen in the files processed; nothing is "
                      "converted for it - run halfileupdate on the INI file "
                      "so the loadrt is read first" % (token, module))

    def convert_module(self, token):
        newmodule, renamed = new_module_of(token)
        if not renamed:
            return None
        if newmodule is None:
            self.rep.warn("component '%s' is removed with no replacement; "
                          "the conversion it did is not needed any more, "
                          "connect the signal directly" % token)
            return None
        return newmodule

    # -- lines --------------------------------------------------------------

    def convert_command(self, segs):
        """Rewrite one command.  A haltcl command and an INI value can be
        spread over continuation lines, so segs is a list of (lineno, body,
        code start, code end, continued) and the command is dispatched on
        all of it at once.  Returns {segment index: new body} and the files
        it sources."""
        pieces, mapping = [], []
        for n, (lineno, body, start, end, cont) in enumerate(segs):
            code = body[start:end]
            if cont:
                # drop the trailing backslash, it is not part of the command
                code = code[:code.rstrip().rfind('\\')]
            mapping.append((sum(len(p) + 1 for p in pieces), n, start))
            pieces.append(code)
        logical = ' '.join(pieces)
        # a multi-line command is reported at the line it starts on
        self.rep.lineno = segs[0][0]
        toks = tokens_of(logical)
        edits = []
        includes = []
        if toks and toks[0][2] == 'hal' and len(toks) > 1:
            # the Tcl 'hal' command takes the HAL command as its first
            # argument: 'hal setp name value'
            toks = toks[1:]
        if toks:
            cmd = toks[0][2]
            if cmd in MODULE_CMDS:
                self.do_loadrt(cmd, toks, edits)
            elif cmd == 'loadusr':
                self.do_loadusr(toks, edits)
            elif cmd == 'net':
                self.do_net(toks, edits)
            elif cmd in PIN_ARGS:
                if cmd == 'linkpp':
                    self.rep.warn("'linkpp' is not a halcmd command any "
                                  "more; the line will fail whatever its "
                                  "names are, replace it with 'net'")
                self.do_args(toks, PIN_ARGS[cmd], edits)
            elif cmd in FUNC_ARGS:
                self.do_args(toks, FUNC_ARGS[cmd], edits, is_func=True)
            elif cmd == 'newsig':
                self.do_newsig(toks, edits)
            elif cmd in ('alias', 'unalias'):
                self.do_alias(toks, edits)
            elif cmd == 'source' and len(toks) > 1:
                includes.append(toks[1][2])
        for lineno, body, _, end, _cont in segs:
            self.note_comment(body[end:], lineno)
        if not edits:
            return {}, includes
        self.rep.edits += len(edits)
        per_line = {}
        for start, endpos, text in edits:
            base, n, offset = [m for m in mapping if m[0] <= start][-1]
            per_line.setdefault(n, []).append((start - base + offset,
                                               endpos - base + offset, text))
        return dict((n, splice(segs[n][1], e)) for n, e in per_line.items()), \
            includes

    def note_comment(self, comment, lineno):
        """A comment that still spells a renamed name is a trap for the
        next reader, but rewriting text nobody can verify is not this
        tool's job, so it is only reported."""
        if not comment:
            return
        for word in re.findall(r'[A-Za-z0-9_.-]+', comment):
            new = self.convert_name(word, report=False)
            if new is None and word in MODULE_RENAMES:
                new = MODULE_RENAMES[word]
            if new and new != word:
                self.rep.note("the comment still says '%s' ('%s' now); "
                              "comments are not rewritten, check it"
                              % (word, new), lineno)
                return

    def do_loadrt(self, cmd, toks, edits):
        if len(toks) < 2:
            return
        start, end, module = toks[1]
        if UNRESOLVABLE_RE.search(module):
            self.rep.note("'%s %s' uses a substitution; the component it "
                          "loads cannot be determined, nothing is converted "
                          "for it" % (cmd, module))
            return
        if cmd == 'loadrt':
            count, names = None, None
            for _, _, arg in toks[2:]:
                if arg.startswith('count='):
                    try:
                        count = int(arg.split('=', 1)[1])
                    except ValueError:
                        count = None
                elif arg.startswith('names='):
                    names = [n for n in arg.split('=', 1)[1].split(',') if n]
            self.loaded.add(module, count, names)
            for name in names or ():
                self.note_legacy_name(name)
        new = self.convert_module(module)
        if new:
            edits.append((start, end, new))

    def do_loadusr(self, toks, edits):
        """loadusr [flags] program [args]; only a component the table knows
        is touched, the rest of the command line is left alone."""
        skip_next = False
        for start, end, tok in toks[1:]:
            if skip_next:
                skip_next = False
                continue
            if tok.startswith('-'):
                skip_next = tok in ('-Wn', '-n')
                continue
            if is_renamed(tok):
                self.loaded.add(tok)
                new = self.convert_module(tok)
                if new:
                    edits.append((start, end, new))
            break

    def do_net(self, toks, edits):
        """net signal [arrow] pin [arrow] pin ...; the signal name is the
        first argument and is never touched."""
        seen_signal = False
        for start, end, tok in toks[1:]:
            if tok in ARROWS:
                continue
            if not seen_signal:
                seen_signal = True
                continue
            self.rewrite_arg(start, end, tok, edits)

    def do_args(self, toks, positions, edits, is_func=False):
        for pos in positions:
            if pos >= len(toks):
                continue
            start, end, tok = toks[pos]
            self.rewrite_arg(start, end, tok, edits, is_func)

    def do_newsig(self, toks, edits):
        """newsig <signal> <type>; the signal name is the user's, but the
        type word is one of the ones that changed."""
        if len(toks) < 3:
            return
        start, end, tok = toks[2]
        new = TYPE_WORDS.get(tok.lower())
        if new:
            edits.append((start, end, new))

    def do_alias(self, toks, edits):
        """alias pin|param <name> <alias>, unalias pin|param <name>; only
        the real name is rewritten, the alias belongs to the user."""
        if len(toks) < 3 or toks[1][2] not in ('pin', 'param'):
            return
        start, end, tok = toks[2]
        self.rewrite_arg(start, end, tok, edits)

    def rewrite_arg(self, start, end, tok, edits, is_func=False):
        if UNRESOLVABLE_RE.search(tok):
            literal = re.split(r'[$\[{]', tok)[0].rstrip('.-_')
            inst, _ = self.loaded.resolve(literal)
            module = (inst.module if inst is not None
                      else self.loaded.defaults.get(literal))
            if module is not None and is_renamed(module):
                self.rep.warn("'%s' is built with a substitution; it is left "
                              "unchanged, convert it by hand" % tok)
            return
        new = self.convert_name(tok, is_func)
        if new:
            edits.append((start, end, new))

    # -- files --------------------------------------------------------------

    def convert_text(self, text, filename, tcl=False):
        """Rewrite a whole HAL file; returns (new text, files it sources)."""
        self.rep.at(filename)
        lines = []
        for line in text.splitlines(True):
            eol = ''
            body = line
            while body and body[-1] in '\r\n':
                eol = body[-1] + eol
                body = body[:-1]
            lines.append([body, eol])
        includes = []
        i = 0
        while i < len(lines):
            segs = []
            while True:
                body = lines[i][0]
                end = code_of(body)
                # only haltcl continues a command on the next line; halcmd
                # reads a HAL file one line at a time
                cont = tcl and body[:end].rstrip().endswith('\\')
                segs.append((i + 1, body, 0, end, cont))
                if not cont or i + 1 >= len(lines):
                    break
                i += 1
            newbodies, inc = self.convert_command(segs)
            for n, newbody in newbodies.items():
                lines[segs[n][0] - 1][0] = newbody
            includes.extend(inc)
            i += 1
        return ''.join(body + eol for body, eol in lines), includes

# ---------------------------------------------------------------------------
# INI files
# ---------------------------------------------------------------------------

INI_KEY_RE = re.compile(r'^\s*([A-Za-z0-9_]+)\s*=[ \t]*')

# INI keys naming a HAL file or carrying a HAL command, in the order
# LinuxCNC runs them
INI_FILE_KEYS = ('HALFILE', 'POSTGUI_HALFILE', 'SHUTDOWN')
INI_CMD_KEYS = ('HALCMD', 'POSTGUI_HALCMD')
# A user interface may read a HAL file through a key of its own, such as
# CUSTOM_HALFILE.  Any other entry naming a HAL file is converted too,
# after the known ones, so that everything a loadrt could be in has been
# read by then.
INI_HAL_SUFFIXES = ('.hal', '.tcl')


def read_ini(lines):
    """[(key, value, segments)] of the entries naming a HAL file or
    carrying a HAL command, in file order.  A value continued over
    several lines with a trailing backslash is one entry; its segments
    are (lineno, body, value start, value end, continued), as
    convert_command wants them."""
    out = []
    i = 0
    while i < len(lines):
        body = lines[i]
        m = INI_KEY_RE.match(body)
        if not m or body.lstrip()[:1] in ('#', ';'):
            i += 1
            continue
        key = m.group(1)
        segs, pieces, start = [], [], m.end()
        while True:
            end = len(body.rstrip())
            cont = body.rstrip().endswith('\\')
            segs.append((i + 1, body, start, end, cont))
            pieces.append(body[start:end - 1 if cont else end].strip())
            if not cont or i + 1 >= len(lines):
                break
            i += 1
            body, start = lines[i], 0
        value = ' '.join(p for p in pieces if p)
        if key in INI_FILE_KEYS + INI_CMD_KEYS:
            out.append((key, value, segs))
        elif value.split() and value.split()[0].endswith(INI_HAL_SUFFIXES):
            out.append(('CUSTOM', value, segs))
        i += 1
    return out


def ini_order(entries):
    """The [HAL] entries in the order LinuxCNC executes them: the HAL
    files first, then the HALCMD lines, then the same for postgui, then
    the shutdown file."""
    rank = {'HALFILE': 0, 'HALCMD': 1, 'POSTGUI_HALFILE': 2,
            'POSTGUI_HALCMD': 3, 'SHUTDOWN': 4, 'CUSTOM': 5}
    return sorted(entries, key=lambda e: (rank[e[0]], e[2][0][0]))


# ---------------------------------------------------------------------------
# Driver
# ---------------------------------------------------------------------------

class Session:
    """One conversion run: the files, in the order they are executed."""

    def __init__(self, rep):
        self.conv = Converter(rep)
        self.rep = rep
        self.results = {}       # path -> (old text, new text)
        self.done = set()
        self.errors = 0

    def read(self, path):
        with open(path, 'r', newline='') as f:
            return f.read()

    def hal_file(self, path, required=True):
        real = os.path.realpath(path)
        if real in self.done:
            return
        self.done.add(real)
        try:
            text = self.read(path)
        except OSError as e:
            if required:
                self.errors += 1
                print("halfileupdate: %s" % e, file=sys.stderr)
            return
        tcl = path.endswith('.tcl')
        if tcl and re.search(r'(?m)^\s*(proc|for|foreach|while|if)\b', text):
            self.rep.at(path)
            self.rep.note("haltcl file that builds HAL names in code; the "
                          "plain commands are converted, a name a proc or a "
                          "loop builds is not, and has to be converted by "
                          "hand")
        newtext, includes = self.conv.convert_text(text, path, tcl)
        if newtext != text:
            self.results[path] = (text, newtext)
        for inc in includes:
            if UNRESOLVABLE_RE.search(inc):
                continue
            self.hal_file(os.path.join(os.path.dirname(path), inc),
                          required=False)

    def ini_file(self, path):
        try:
            text = self.read(path)
        except OSError as e:
            self.errors += 1
            print("halfileupdate: %s" % e, file=sys.stderr)
            return
        bodies, eols = [], []
        for line in text.splitlines(True):
            eol = ''
            while line and line[-1] in '\r\n':
                eol = line[-1] + eol
                line = line[:-1]
            bodies.append(line)
            eols.append(eol)
        entries = ini_order(read_ini(bodies))
        if not entries:
            self.rep.at(path)
            self.rep.note("no HAL files and no HAL commands found; is this "
                          "an INI file?")
            return
        changed = False
        for key, value, segs in entries:
            lineno = segs[0][0]
            self.rep.at(path, lineno)
            if key in INI_CMD_KEYS:
                newbodies, _ = self.conv.convert_command(segs)
                for n, newbody in newbodies.items():
                    bodies[segs[n][0] - 1] = newbody
                    changed = True
                continue
            # a file entry may carry arguments after the file name
            fname = value.split()[0] if value.split() else ''
            if not fname:
                continue
            if fname.startswith('LIB:'):
                self.rep.note("%s is a LinuxCNC library file; the library is "
                              "updated with LinuxCNC, so it is not converted"
                              % fname)
                continue
            local = os.path.join(os.path.dirname(path),
                                 os.path.expanduser(fname))
            if os.path.exists(local):
                self.hal_file(local)
                continue
            found, searched = in_hallib(fname)
            if found or not searched:
                self.rep.note("%s is not next to the INI file; LinuxCNC "
                              "takes it from the HAL library, which is "
                              "updated with LinuxCNC, so it is not converted"
                              % fname)
            else:
                self.rep.note("%s: no such file next to the INI file and "
                              "none in the HAL library; not converted"
                              % fname)
        if changed:
            self.results[path] = (text, ''.join(b + e for b, e
                                                in zip(bodies, eols)))

    def add(self, path):
        if path.endswith('.ini'):
            self.ini_file(path)
        else:
            self.hal_file(path)


def make_backup(fname):
    """Create a backup of fname next to it, refusing to follow symlinks or
    to clobber an existing backup.  Returns the backup path."""
    flags = os.O_WRONLY | os.O_CREAT | os.O_EXCL | getattr(os, 'O_NOFOLLOW', 0)
    for n in range(100):
        cand = fname + '.bak' if n == 0 else "%s.bak.%d" % (fname, n)
        try:
            fd = os.open(cand, flags, 0o644)
        except FileExistsError:
            continue
        with os.fdopen(fd, 'w', newline='') as f:
            with open(fname, 'r', newline='') as src:
                f.write(src.read())
        return cand
    raise SystemExit("halfileupdate: cannot create backup for %s "
                     "(too many .bak files)" % fname)


def write_atomic(fname, text):
    """Write text to fname atomically (temp file + rename), preserving the
    original file mode."""
    st = os.stat(fname)
    fd, tmp = tempfile.mkstemp(dir=os.path.dirname(os.path.abspath(fname)),
                               prefix='.halfileupdate-', suffix='.tmp')
    try:
        with os.fdopen(fd, 'w', newline='') as f:
            f.write(text)
            f.flush()
            os.fsync(f.fileno())
        os.chmod(tmp, st.st_mode & 0o7777)
        os.replace(tmp, fname)
    except BaseException:
        try:
            os.unlink(tmp)
        except OSError:
            pass
        raise


def main(argv=None):
    p = argparse.ArgumentParser(
        prog='halfileupdate',
        description="Update HAL configuration files after component, pin and "
                    "parameter renames. Given an INI file, every HAL file it "
                    "lists is converted, in the order LinuxCNC runs them. "
                    "Without options a unified diff is printed and nothing "
                    "is written.")
    p.add_argument('--in-place', '-i', action='store_true',
                   help="rewrite files in place (a .bak backup is kept unless "
                        "--no-backup is given)")
    p.add_argument('--no-backup', action='store_true',
                   help="with --in-place, do not keep a .bak backup")
    p.add_argument('--quiet', '-q', action='store_true',
                   help="suppress warnings on stderr")
    p.add_argument('files', nargs='+', metavar='file.hal|file.ini')
    args = p.parse_args(argv)

    rep = Reporter(args.quiet)
    session = Session(rep)
    for fname in args.files:
        session.add(fname)

    for fname, (text, newtext) in session.results.items():
        if args.in_place:
            bak = None
            if not args.no_backup:
                bak = make_backup(fname)
            write_atomic(fname, newtext)
            rep.info("updated %s%s"
                     % (fname, "" if bak is None else " (backup: %s)" % bak))
        else:
            sys.stdout.writelines(difflib.unified_diff(
                text.splitlines(True), newtext.splitlines(True),
                fromfile=fname, tofile=fname + ".new"))

    if not args.quiet and (session.results or rep.warnings or rep.notes):
        summary = ("halfileupdate: %d change(s) in %d file(s), %d name(s) "
                   "left for manual review" % (rep.edits, len(session.results),
                                               rep.warnings))
        if rep.notes:
            summary += ", %d note(s)" % rep.notes
        print(summary + " - review the diff and test the configuration "
              "before use", file=sys.stderr)
    return 2 if session.errors else 0


if __name__ == '__main__':
    sys.exit(main())

# vim:sw=4:sts=4:et:syn=python
