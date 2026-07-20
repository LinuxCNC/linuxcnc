#!/usr/bin/env python3
#
#    halcompupdate - migrate HAL .comp components to the new HAL pin/param API
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
# The new HAL API replaces direct memory access to pins and params with
# strongly typed getter/setter access:
#
#   * declaration types change:
#       float -> real, bit -> bool, s32 -> si32, u32 -> ui32,
#       s64 -> sint, u64 -> uint, signed -> si32, unsigned -> ui32
#     ('port' is not converted yet)
#   * writing an out/io pin or a param is done with the generated
#     <name>_set(value) function instead of plain assignment
#   * reading stays transparent (the bare name keeps working)
#   * <name>_ptr is no longer dereferenceable; *<name>_ptr reads become
#     (<name>) and *<name>_ptr writes become <name>_set(...)
#
# This tool rewrites a .comp file accordingly.  By default it prints a
# unified diff; use --in-place to rewrite the file.

import argparse
import difflib
import os
import re
import sys
import tempfile

OLD2NEW = {
    'float': 'real',
    'bit': 'bool',
    's32': 'si32',
    'u32': 'ui32',
    's64': 'sint',
    'u64': 'uint',
    'signed': 'si32',
    'unsigned': 'ui32',
}

# C types used with the old HAL API and their modern replacements for
# variable use.  The volatile qualifier of the legacy hal_*_t types exists
# only because they were used for direct access to HAL memory; as variable
# types they were always wrong, so the plain rtapi_* forms are used here.
# Plain C 'float' is deliberately not mapped: it is not part of the HAL
# API and stays valid C (only 'variable' declarations are converted).
# Pointers to the legacy types are a different matter (they referenced
# HAL memory and become opaque hal_*_t references); those are warned
# about and left unchanged.
CTYPE_MAP = {
    'double': 'rtapi_real',
    'real_t': 'rtapi_real',
    'hal_float_t': 'rtapi_real',
    'hal_bit_t': 'rtapi_bool',
    'hal_s32_t': 'rtapi_s32',
    'hal_u32_t': 'rtapi_u32',
    'hal_s64_t': 'rtapi_s64',
    'hal_u64_t': 'rtapi_u64',
}

# Mapping for 'variable' declarations.  The comp grammar allows only a
# single-word type there, and instance variables are component-private
# memory, so the plain rtapi_* forms are used (no volatile qualifier).
# Old code frequently used HAL type names here ('float' for HAL floats);
# C 'float' variables holding HAL values must become rtapi_real.
# 'signed'/'unsigned' are *not* mapped: they are legitimate plain C types.
VAR_TYPE_MAP = {
    'double': 'rtapi_real',
    'real_t': 'rtapi_real',
    'float': 'rtapi_real',
    'hal_float_t': 'rtapi_real',
    'hal_bit_t': 'rtapi_bool',
    'hal_s32_t': 'rtapi_s32',
    'hal_u32_t': 'rtapi_u32',
    'hal_s64_t': 'rtapi_s64',
    'hal_u64_t': 'rtapi_u64',
}

# HAL C types/macros that will disappear at the API break; used in bodies
LEGACY_C_TYPES = [
    'hal_bit_t', 'hal_float_t', 'hal_s32_t', 'hal_u32_t',
    'hal_s64_t', 'hal_u64_t', 'hal_data_u', 'hal_pin_dir_t',
    'hal_param_dir_t',
]
LEGACY_TYPE_MACROS = [
    'HAL_BIT', 'HAL_FLOAT', 'HAL_S32', 'HAL_U32', 'HAL_S64', 'HAL_U64',
]

PIN_WRITABLE_DIRS = {'out', 'io'}
PARAM_WRITABLE_DIRS = {'rw', 'r'}


def to_c(name):
    """Replicates halcompile's to_c(): HAL name -> C identifier."""
    name = re.sub(r"[-._]*#+", "", name)
    name = name.replace("#", "").replace(".", "_").replace("-", "_")
    return re.sub(r"_+", "_", name)


class Reporter:
    def __init__(self, filename, quiet=False):
        self.filename = filename
        self.quiet = quiet

    def warn(self, msg, lineno=0):
        if not self.quiet:
            print("%s:%d: Warning: %s" % (self.filename, lineno or 0, msg),
                  file=sys.stderr)

    def error(self, msg):
        raise SystemExit("%s:0: Error: %s" % (self.filename, msg))


def lineno_of(text, pos):
    return text.count("\n", 0, pos) + 1


# ---------------------------------------------------------------------------
# Header parsing
# ---------------------------------------------------------------------------

def split_statements(header):
    """Split the declaration part (before ';;') into statements terminated
    by ';', ignoring semicolons inside strings and triple-quoted docs.
    Returns a list of (start, end) spans, end includes the ';'."""
    spans = []
    i, start, n = 0, 0, len(header)
    while i < n:
        c = header[i]
        if header.startswith('"""', i) or header.startswith('r"""', i):
            if header[i] == 'r':
                i += 1
            i += 3
            while i < n:
                if header[i] == '\\':
                    i += 2
                    continue
                if header.startswith('"""', i):
                    i += 3
                    break
                i += 1
            continue
        if c == '"':
            i += 1
            while i < n and header[i] != '"':
                if header[i] == '\\':
                    i += 1
                i += 1
            i += 1
            continue
        if c == "'":
            i += 1
            while i < n and header[i] != "'":
                if header[i] == '\\':
                    i += 1
                i += 1
            i += 1
            continue
        if header.startswith('//', i):
            j = header.find('\n', i)
            i = n if j < 0 else j + 1
            continue
        if header.startswith('/*', i):
            j = header.find('*/', i + 2)
            i = n if j < 0 else j + 2
            continue
        if c == ';':
            spans.append((start, i + 1))
            start = i + 1
        i += 1
    if start < n:
        spans.append((start, n))
    return spans


DECL_RE = re.compile(
    r'^(\s*(?:(?://[^\n]*|/\*.*?\*/)\s*)*)'      # leading whitespace/comments
    r'(pin|param)\s+(in|out|io|rw|r)\s+'
    r'([A-Za-z][A-Za-z0-9]*)\s+'                  # HAL type
    r'([#A-Za-z_][-#A-Za-z0-9_.]*)'               # HAL name
    r'(\s*\[)?',                                  # array marker
    re.S)

VAR_RE = re.compile(
    r'^(\s*(?:(?://[^\n]*|/\*.*?\*/)\s*)*)'      # leading whitespace/comments
    r'variable\s+'
    r'([A-Za-z_][A-Za-z0-9_]*)\s+'                # C type
    r'\*?\s*[#A-Za-z_]',                          # (possibly starred) name
    re.S)


class Decl:
    def __init__(self, kind, direction, type_, name, is_array):
        self.kind = kind            # 'pin' or 'param'
        self.direction = direction
        self.type = type_
        self.name = name            # HAL name
        self.cname = to_c(name)
        self.is_array = is_array

    @property
    def writable(self):
        if self.kind == 'pin':
            return self.direction in PIN_WRITABLE_DIRS
        return self.direction in PARAM_WRITABLE_DIRS


def parse_declarations(header):
    """Find pin/param declarations, return (list of Decl, set of options)."""
    decls = []
    options = set()
    for start, end in split_statements(header):
        stmt = header[start:end]
        m = DECL_RE.match(stmt)
        if m:
            _, kind, direction, type_, name, array = m.groups()
            decls.append(Decl(kind, direction, type_, name, bool(array)))
            continue
        mo = re.match(r'\s*option\s+([A-Za-z_][A-Za-z0-9_]*)', stmt)
        if mo:
            options.add(mo.group(1))
    return decls, options


def convert_header(header, rep, c_types=True, legacy_api=False):
    """Replace old HAL types by new ones in pin/param declarations and
    modernize C types in 'variable' declarations.  Only the type token
    itself is replaced, all other text (spacing, docs) is preserved.
    Returns (new_header, changed, decls, options)."""
    decls, options = parse_declarations(header)
    if 'no_convenience_defines' in options:
        rep.error("component uses 'option no_convenience_defines'; "
                  "automatic body conversion is not possible")
    var_map = VAR_TYPE_MAP
    if legacy_api:
        # keep hal_*_t: their addresses are passed to legacy API calls
        var_map = {k: v for k, v in VAR_TYPE_MAP.items()
                   if not k.startswith('hal_')}
    edits = []          # (start, end, replacement) in header coordinates
    for start, end in split_statements(header):
        stmt = header[start:end]
        m = DECL_RE.match(stmt)
        if m:
            type_ = m.group(4)
            if type_ in OLD2NEW:
                edits.append((start + m.start(4), start + m.end(4),
                              OLD2NEW[type_]))
            continue
        if c_types:
            mv = VAR_RE.match(stmt)
            if mv and mv.group(2) in var_map:
                edits.append((start + mv.start(2), start + mv.end(2),
                              var_map[mv.group(2)]))
    if not edits:
        return header, False, decls, options
    out = []
    prev = 0
    for s0, e0, repl in edits:
        out.append(header[prev:s0])
        out.append(repl)
        prev = e0
    out.append(header[prev:])
    return ''.join(out), True, decls, options


# ---------------------------------------------------------------------------
# Body rewriting
# ---------------------------------------------------------------------------

def skip_ws_comments(s, i):
    n = len(s)
    while i < n:
        if s[i] in ' \t\r\n':
            i += 1
        elif s.startswith('//', i):
            j = s.find('\n', i)
            i = n if j < 0 else j + 1
        elif s.startswith('/*', i):
            j = s.find('*/', i + 2)
            i = n if j < 0 else j + 2
        else:
            break
    return i


def scan_string(s, i):
    quote = s[i]
    i += 1
    n = len(s)
    while i < n:
        if s[i] == '\\':
            i += 2
            continue
        if s[i] == quote:
            return i + 1
        i += 1
    return n


def match_parens(s, i):
    """i points at '('.  Return index just past the matching ')'."""
    depth = 0
    n = len(s)
    while i < n:
        c = s[i]
        if c == '"' or c == "'":
            i = scan_string(s, i)
            continue
        if s.startswith('//', i):
            j = s.find('\n', i)
            i = n if j < 0 else j + 1
            continue
        if s.startswith('/*', i):
            j = s.find('*/', i + 2)
            i = n if j < 0 else j + 2
            continue
        if c in '([{':
            depth += 1
        elif c in ')]}':
            depth -= 1
            if depth == 0:
                return i + 1
        i += 1
    return n


def read_assign_op(s, i):
    """At position i (after optional whitespace already skipped), return
    (op, end) if an assignment operator follows, else None."""
    for op in ('<<=', '>>='):
        if s.startswith(op, i):
            return op, i + 3
    c = s[i] if i < len(s) else ''
    if c in '+-*/%&|^':
        if s[i + 1:i + 2] == '=':
            return c + '=', i + 2
        return None
    if c == '=':
        if s[i + 1:i + 2] == '=':
            return None
        return '=', i + 1
    return None


def read_expr(s, i):
    """Read a C expression starting at i up to (not including) the
    terminating ';', ',', unmatched ')'/'}' or ternary ':' at nesting
    level zero.  Returns (expr_text, end_index)."""
    start = i
    depth = 0
    ternary = 0
    n = len(s)
    while i < n:
        c = s[i]
        if c == '"' or c == "'":
            i = scan_string(s, i)
            continue
        if s.startswith('//', i):
            j = s.find('\n', i)
            i = n if j < 0 else j + 1
            continue
        if s.startswith('/*', i):
            j = s.find('*/', i + 2)
            i = n if j < 0 else j + 2
            continue
        if c in '([{':
            depth += 1
        elif c in ')]}':
            if depth == 0:
                break
            depth -= 1
        elif depth == 0:
            if c == '?':
                ternary += 1
            elif c == ':':
                if ternary:
                    ternary -= 1
                else:
                    break
            elif c == ';' or c == ',':
                break
        i += 1
    return s[start:i].strip(), i


IDENT_RE = re.compile(r'[A-Za-z_][A-Za-z0-9_]*')

# Index expressions that are guaranteed side-effect-free (plain identifier
# or numeric literal); required where the index is emitted more than once.
SIMPLE_IDX_RE = re.compile(r'([A-Za-z_][A-Za-z0-9_]*|0x[0-9a-fA-F]+|[0-9]+)$')

# Legacy pin/param creation calls used directly in hand-written C code.
# Components using these need manual conversion; converting the hal_*_t C
# types would break compilation of the calls themselves.
LEGACY_API_RE = re.compile(
    r'\bhal_(?:pin|param)_(?:bit|float|s32|u32|s64|u64|port)_newf?\s*\('
    r'|\bhal_(?:pin|param)_new\s*\(')

DEFINE_RE = re.compile(
    r'#\s*define\s+([A-Za-z_][A-Za-z0-9_]*)(\(([^)]*)\))?')


class BodyRewriter:
    def __init__(self, body, decls, rep, c_types=True, legacy_api=False):
        self.s = body
        self.rep = rep
        self.c_types = c_types
        # When the code calls the legacy creation API directly, the hal_*_t
        # types must stay (their addresses are passed to those functions);
        # such components need manual conversion.
        if legacy_api:
            self.ctype_map = {'double': 'rtapi_real'}
        else:
            self.ctype_map = CTYPE_MAP
        self.out = []
        # Only converted (old-type) decls need rewriting; new-type decls
        # are already fine and 'port' keeps direct access.
        conv = [d for d in decls if d.type in OLD2NEW]
        self.writable = {d.cname: d for d in conv if d.writable}
        self.readonly = {d.cname: d for d in conv if not d.writable}
        # all converted decls (pins and params), for &-address checks
        self.allconv = dict(self.readonly)
        self.allconv.update(self.writable)
        # _ptr only ever existed for pins, never for params
        self.pinconv = {d.cname: d for d in conv if d.kind == 'pin'}
        self.changed = False
        self.last_sig = ''      # last significant char copied to output
        self.last_sig2 = ''     # the one before it
        self.last_ident = None  # last identifier copied to output
        self.fragment = False   # True when rewriting a sub-expression
        self.line_offset = 0

    @classmethod
    def for_fragment(cls, parent, text, exclude=(), line_offset=0):
        """Create a rewriter for an expression fragment extracted from
        parent, sharing its pin/param tables.  'exclude' lists identifiers
        that shadow pins/params in the fragment (macro parameters)."""
        sub = cls.__new__(cls)
        sub.s = text
        sub.rep = parent.rep
        sub.c_types = parent.c_types
        sub.ctype_map = parent.ctype_map
        sub.out = []
        sub.writable = {k: v for k, v in parent.writable.items()
                        if k not in exclude}
        sub.readonly = {k: v for k, v in parent.readonly.items()
                        if k not in exclude}
        sub.allconv = {k: v for k, v in parent.allconv.items()
                       if k not in exclude}
        sub.pinconv = {k: v for k, v in parent.pinconv.items()
                       if k not in exclude}
        sub.changed = False
        sub.last_sig = ''
        sub.last_sig2 = ''
        sub.last_ident = None
        sub.fragment = True
        sub.line_offset = line_offset
        return sub

    def rewrite_expr(self, expr):
        """Recursively rewrite an extracted expression (handles chained
        assignments like 'x = y = z')."""
        sub = BodyRewriter.for_fragment(self, expr)
        out = sub.run()
        self.changed = self.changed or sub.changed
        return out

    def emit(self, text):
        """Emit significant code, tracking the last tokens seen."""
        self.out.append(text)
        for c in text:
            if c not in ' \t\r\n':
                self.last_sig2 = self.last_sig
                self.last_sig = c

    def emit_raw(self, text):
        """Emit text that must not influence token tracking (comments,
        strings, whitespace, preprocessor lines)."""
        self.out.append(text)

    def pop_star(self):
        """Remove a trailing '*' (the dereference operator) from the output,
        keeping surrounding whitespace.  Walks the output chunks backwards
        and only touches the affected tail chunk.  Returns False if the
        last significant output char is not a plain single '*'."""
        for idx in range(len(self.out) - 1, -1, -1):
            chunk = self.out[idx]
            stripped = chunk.rstrip()
            if not stripped:
                continue                    # whitespace-only chunk
            if not stripped.endswith('*'):
                return False
            if len(stripped) > 1 and stripped[-2] == '*':   # '**' double deref
                return False
            pos = len(stripped) - 1
            self.out[idx] = chunk[:pos] + chunk[pos + 1:]
            return True
        return False

    def idx_has_pin_write(self, idx):
        """True if the array index expression assigns to, or increments/
        decrements, a converted writable pin/param (side effect whose
        semantics cannot be preserved by automatic rewriting)."""
        for name in self.writable:
            m = re.search(r'\b%s\b' % re.escape(name), idx)
            while m:
                k = skip_ws_comments(idx, m.end())
                if (read_assign_op(idx, k) or idx.startswith('++', k)
                        or idx.startswith('--', k)):
                    return True
                m = re.search(r'\b%s\b' % re.escape(name), idx, m.end())
        return False

    def line(self, pos):
        return self.line_offset + lineno_of(self.s, pos)

    def run(self):
        s = self.s
        n = len(s)
        i = 0
        at_line_start = True
        while i < n:
            c = s[i]
            if s.startswith('//', i):
                j = s.find('\n', i)
                j = n if j < 0 else j
                self.emit_raw(s[i:j])
                i = j
                continue
            if s.startswith('/*', i):
                j = s.find('*/', i + 2)
                j = n if j < 0 else j + 2
                self.emit_raw(s[i:j])
                i = j
                continue
            if c == '\n':
                self.emit_raw(c)
                i += 1
                at_line_start = True
                continue
            if c in ' \t\r':
                self.out.append(c)
                i += 1
                continue
            if c == '#' and at_line_start:
                j = i
                while True:
                    e = s.find('\n', j)
                    e = n if e < 0 else e
                    if e > j and s[e - 1] == '\\' and e < n:
                        j = e + 1
                        continue
                    break
                line = s[i:e]
                i = e
                at_line_start = False
                # #define bodies may contain pin/param writes that must be
                # converted; macro parameters shadow same-named pins.
                dm = DEFINE_RE.match(line)
                if dm:
                    self.handle_define(line, dm, i)
                else:
                    self.emit_raw(line)
                continue
            at_line_start = False
            if c == '"' or c == "'":
                j = scan_string(s, i)
                self.emit_raw(s[i:j])
                i = j
                continue
            m = IDENT_RE.match(s, i)
            if m:
                ident = m.group(0)
                # struct member access (.name, ->name) is never a pin
                if self.last_sig == '.' or (self.last_sig == '>' and self.last_sig2 == '-'):
                    self.emit(ident)
                    self.last_ident = ident
                    i = m.end()
                    continue
                if self.c_types and ident in self.ctype_map:
                    # keep 'long double' intact
                    if ident == 'double' and self.last_ident == 'long':
                        self.emit(ident)
                    elif ident.startswith('hal_') and s[skip_ws_comments(s, m.end()):].startswith('*'):
                        self.rep.warn("pointer to legacy HAL type '%s'; pointers "
                                      "into HAL memory become opaque references "
                                      "in the new API - left as-is, convert to "
                                      "hal_get_*/hal_set_* manually" % ident,
                                      self.line(i))
                        self.emit(ident)
                    else:
                        self.emit(self.ctype_map[ident])
                        self.changed = True
                    self.last_ident = ident
                    i = m.end()
                    continue
                if ident.endswith('_ptr') and ident[:-4] in self.pinconv:
                    i = self.handle_ptr(ident[:-4], i, m.end())
                    self.last_ident = None
                    continue
                if ident in self.writable:
                    i = self.handle_writable(ident, i, m.end())
                    self.last_ident = None
                    continue
                if ident in self.readonly:
                    k = skip_ws_comments(s, m.end())
                    if read_assign_op(s, k) or s.startswith('++', k) or s.startswith('--', k):
                        self.rep.warn("assignment to input pin '%s' left as-is "
                                      "(was already invalid)" % ident, self.line(i))
                    self.emit(ident)
                    self.last_ident = ident
                    i = m.end()
                    continue
                self.emit(ident)
                self.last_ident = ident
                i = m.end()
                continue
            if s.startswith('++', i) or s.startswith('--', i):
                i = self.handle_prefix_incdec(i)
                self.last_ident = None
                continue
            if c == '&' and not s.startswith('&&', i) and not s.startswith('&=', i):
                k = skip_ws_comments(s, i + 1)
                mi = IDENT_RE.match(s, k)
                if mi and mi.group(0) in self.allconv:
                    self.rep.warn("address of pin/param '%s' is taken; with the new "
                                  "API '%s_ptr' is an opaque reference - convert the "
                                  "called code to hal_get_*/hal_set_* manually"
                                  % (mi.group(0), mi.group(0)), self.line(i))
                self.emit(c)
                self.last_ident = None
                i += 1
                continue
            self.emit(c)
            if c not in ' \t\r\n':
                self.last_ident = None
            i += 1
        if not self.fragment:
            self.check_legacy_refs()
        return ''.join(self.out)

    # -- token handlers ----------------------------------------------------

    def emit_gap(self, a, b):
        """Preserve a skipped whitespace/comment region if it contains a
        comment (pure whitespace may be dropped when rewriting)."""
        if a < b:
            gap = self.s[a:b]
            if '//' in gap or '/*' in gap:
                self.emit_raw(gap)

    def value_discarded(self):
        """True when the expression value at the current position is
        discarded (statement context), so postfix ++/-- may be rewritten
        with prefix semantics."""
        return self.last_sig in ('', ';', '{', '}')

    def rewrite_index(self, ident, idx, pos):
        """Rewrite an array index expression.  Returns the rewritten
        index, or None when it contains a pin/param write whose semantics
        cannot be preserved (a warning is emitted then)."""
        if self.idx_has_pin_write(idx):
            self.rep.warn("array index of '%s' modifies a pin/param; "
                          "left as-is - convert manually" % ident, pos)
            return None
        return self.rewrite_expr(idx)

    def handle_writable(self, ident, start, end):
        """A writable pin/param identifier.  Convert writes to _set()."""
        s = self.s
        decl = self.writable[ident]
        k = skip_ws_comments(s, end)
        idx = None
        after = k
        gaps = []
        if decl.is_array and k < len(s) and s[k] == '(':
            close = match_parens(s, k)
            k2 = skip_ws_comments(s, close)
            if (read_assign_op(s, k2) or s.startswith('++', k2)
                    or s.startswith('--', k2)):
                idx = s[k + 1:close - 1].strip()
                gaps = [(end, k), (close, k2)]
                after = k2
            else:
                # read access name(index): keep as-is; the index expression
                # is processed normally by the main loop
                self.emit(s[start:end])
                self.emit_raw(s[end:k])
                return k
        else:
            gaps = [(end, k)]
        op = read_assign_op(s, after)
        if op:
            opstr, opend = op
            if idx is not None:
                idx = self.rewrite_index(ident, idx, self.line(start))
                if idx is None:
                    self.emit(s[start:opend])
                    return opend
                if (opstr != '=' and not SIMPLE_IDX_RE.match(idx)):
                    self.rep.warn("compound assignment to '%s' has an index "
                                  "with side effects; left as-is - convert "
                                  "manually" % ident, self.line(start))
                    self.emit(s[start:opend])
                    return opend
            expr, exprend = read_expr(s, skip_ws_comments(s, opend))
            if not expr:
                self.rep.warn("could not parse expression in assignment to '%s'; "
                              "left as-is" % ident, self.line(start))
                self.emit(s[start:opend])
                return opend
            expr = self.rewrite_expr(expr)
            for a, b in gaps:
                self.emit_gap(a, b)
            target = "%s(%s)" % (ident, idx) if idx is not None else ident
            args = "%s, " % idx if idx is not None else ""
            if opstr == '=':
                new = "%s_set(%s%s)" % (ident, args, expr)
            else:
                new = "%s_set(%s%s %s (%s))" % (ident, args, target,
                                                opstr[0], expr)
            self.emit(new)
            self.changed = True
            return exprend
        if s.startswith('++', after) or s.startswith('--', after):
            sign = '+' if s.startswith('++', after) else '-'
            if not self.value_discarded():
                self.rep.warn("postfix %s on pin/param '%s' changes value "
                              "semantics (setter yields the new value); "
                              "left as-is - convert manually"
                              % (s[after:after + 2], ident), self.line(start))
                self.emit(s[start:after + 2])
                return after + 2
            if idx is not None:
                idx = self.rewrite_index(ident, idx, self.line(start))
                if idx is None:
                    self.emit(s[start:after + 2])
                    return after + 2
                if not SIMPLE_IDX_RE.match(idx):
                    self.rep.warn("increment/decrement of '%s' has an index "
                                  "with side effects; left as-is - convert "
                                  "manually" % ident, self.line(start))
                    self.emit(s[start:after + 2])
                    return after + 2
            for a, b in gaps:
                self.emit_gap(a, b)
            target = "%s(%s)" % (ident, idx) if idx is not None else ident
            args = "%s, " % idx if idx is not None else ""
            self.emit("%s_set(%s%s %s 1)" % (ident, args, target, sign))
            self.changed = True
            return after + 2
        # plain read access: unchanged
        self.emit(s[start:end])
        self.emit_raw(s[end:after])
        return after

    def handle_prefix_incdec(self, i):
        s = self.s
        sign = '+' if s.startswith('++', i) else '-'
        k = skip_ws_comments(s, i + 2)
        m = IDENT_RE.match(s, k)
        if m and m.group(0) in self.writable:
            ident = m.group(0)
            decl = self.writable[ident]
            after = skip_ws_comments(s, m.end())
            idx = None
            if decl.is_array and after < len(s) and s[after] == '(':
                close = match_parens(s, after)
                idx = s[after + 1:close - 1].strip()
                after = close
            target = "%s(%s)" % (ident, idx) if idx is not None else ident
            args = "%s, " % idx if idx is not None else ""
            self.emit("%s_set(%s%s %s 1)" % (ident, args, target, sign))
            self.changed = True
            return after
        self.emit(s[i:i + 2])
        return i + 2

    def handle_ptr(self, base, start, end):
        """'<name>_ptr' for a converted pin.  Only '*name_ptr' dereferences
        can be converted automatically; anything else needs manual work."""
        s = self.s
        n = len(s)
        deref = self.last_sig == '*'
        if not deref:
            self.rep.warn("'%s_ptr' used as a value; with the new API it is an "
                          "opaque reference, not a pointer - convert manually"
                          % base, self.line(start))
            self.emit(s[start:end])
            return end
        k = skip_ws_comments(s, end)
        idx = None
        after = k
        decl = self.pinconv[base]
        if decl.is_array and k < n and s[k] == '(':
            close = match_parens(s, k)
            idx = s[k + 1:close - 1].strip()
            after = skip_ws_comments(s, close)
        op = read_assign_op(s, after)
        if op and decl.writable:
            opstr, opend = op
            if idx is not None:
                idx = self.rewrite_index(base, idx, self.line(start))
                if idx is None:
                    self.emit(s[start:opend])
                    return opend
                if opstr != '=' and not SIMPLE_IDX_RE.match(idx):
                    self.rep.warn("compound assignment through '%s_ptr' has an "
                                  "index with side effects; left as-is - "
                                  "convert manually" % base, self.line(start))
                    self.emit(s[start:opend])
                    return opend
            expr, exprend = read_expr(s, skip_ws_comments(s, opend))
            if not expr:
                self.rep.warn("could not parse expression in assignment through "
                              "'%s_ptr'; left as-is" % base, self.line(start))
                self.emit(s[start:opend])
                return opend
            if not self.pop_star():
                self.rep.warn("cannot rewrite dereference of '%s_ptr' here; "
                              "left as-is" % base, self.line(start))
                self.emit(s[start:opend])
                return opend
            expr = self.rewrite_expr(expr)
            self.emit_gap(end, k)
            target = "%s(%s)" % (base, idx) if idx is not None else base
            args = "%s, " % idx if idx is not None else ""
            if opstr == '=':
                new = "%s_set(%s%s)" % (base, args, expr)
            else:
                new = "%s_set(%s%s %s (%s))" % (base, args, target,
                                                opstr[0], expr)
            self.emit(new)
            self.changed = True
            return exprend
        if op:
            self.rep.warn("write through '%s_ptr' of read-only pin; left as-is"
                          % base, self.line(start))
            self.emit(s[start:end])
            self.emit_raw(s[end:after])
            return after
        # postfix ++/-- directly after the dereference increments the old
        # pointer itself; parenthesized derefs followed by an operator are
        # ambiguous - neither can be converted automatically
        if s.startswith('++', after) or s.startswith('--', after):
            self.rep.warn("postfix %s on '%s_ptr' cannot be converted "
                          "automatically; left as-is - convert manually"
                          % (s[after:after + 2], base), self.line(start))
            self.emit(s[start:after + 2])
            return after + 2
        if after < n and s[after] == ')':
            k2 = skip_ws_comments(s, after + 1)
            op2 = read_assign_op(s, k2)
            if op2 or s.startswith('++', k2) or s.startswith('--', k2):
                self.rep.warn("parenthesized dereference of '%s_ptr' followed "
                              "by assignment or increment/decrement; left "
                              "as-is - convert manually" % base, self.line(start))
                if op2:
                    pend = op2[1]
                else:
                    pend = k2 + 2
                self.emit(s[start:pend])
                return pend
        # dereference read: *name_ptr -> (name), *name_ptr(i) -> (name(i))
        if idx is not None:
            idx = self.rewrite_index(base, idx, self.line(start))
            if idx is None:
                self.emit(s[start:after])
                return after
        if not self.pop_star():
            self.rep.warn("cannot rewrite dereference of '%s_ptr' here; "
                          "left as-is" % base, self.line(start))
            self.emit(s[start:end])
            self.emit_raw(s[end:after])
            return after
        target = "%s(%s)" % (base, idx) if idx is not None else base
        self.emit("(%s)" % target)
        self.emit_raw(s[end:after])
        self.changed = True
        return after

    # -- checks --------------------------------------------------------------

    def handle_define(self, line, dm, pos):
        """Rewrite the body of a #define directive.  The directive head
        (#define NAME(params)) is preserved; macro parameters shadow
        same-named pins/params inside the body."""
        params = []
        if dm.group(2):
            params = [p.strip() for p in dm.group(3).split(',') if p.strip()]
        head = line[:dm.end()]
        code = line[dm.end():]
        if not code.strip():
            self.emit_raw(line)
            return
        for name in list(self.writable) + list(self.readonly):
            if re.search(r'\b%s\b' % re.escape(name), head):
                self.rep.warn("macro '%s' shadows or redefines pin/param '%s'; "
                              "not modified - check manually"
                              % (dm.group(1), name), self.line(pos))
                self.emit_raw(line)
                return
        sub = BodyRewriter.for_fragment(self, code, exclude=params,
                                        line_offset=self.line(pos) - 1)
        rewritten = sub.run()
        self.changed = self.changed or sub.changed
        self.emit_raw(head)
        self.emit(rewritten)

    def check_legacy_refs(self):
        for t in LEGACY_C_TYPES:
            if re.search(r'\b%s\b' % t, self.s):
                self.rep.warn("uses legacy HAL C type '%s'; it is deprecated and "
                              "will be removed at the HAL API break" % t)
        for t in LEGACY_TYPE_MACROS:
            if re.search(r'\b%s\b' % t, self.s):
                self.rep.warn("uses legacy HAL type constant '%s'; it will be "
                              "removed at the HAL API break" % t)


# ---------------------------------------------------------------------------
# Driver
# ---------------------------------------------------------------------------

def split_comp(text, rep):
    m = re.search(r'\n;;\n', text)
    if not m:
        rep.error("not a .comp file (no ';;' separator found)")
    return text[:m.start()], text[m.end():]


def convert(text, filename, quiet=False, c_types=True):
    rep = Reporter(filename, quiet)
    header, body = split_comp(text, rep)
    legacy_api = bool(LEGACY_API_RE.search(body))
    if legacy_api:
        rep.warn("uses the legacy hal_pin_*_new/hal_param_*_new creation API "
                 "directly; the calls and the data they export must be "
                 "converted to hal_pin_new_<type>/hal_param_new_<type> "
                 "manually")
    newheader, hchanged, decls, _ = convert_header(header, rep, c_types,
                                                   legacy_api)
    rewriter = BodyRewriter(body, decls, rep, c_types, legacy_api)
    newbody = rewriter.run()
    return newheader + "\n;;\n" + newbody, (hchanged or rewriter.changed)


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
    raise SystemExit("halcompupdate: cannot create backup for %s "
                     "(too many .bak files)" % fname)


def write_atomic(fname, text):
    """Write text to fname atomically (temp file + rename), preserving the
    original file mode."""
    st = os.stat(fname)
    fd, tmp = tempfile.mkstemp(dir=os.path.dirname(os.path.abspath(fname)),
                               prefix='.halcompupdate-', suffix='.tmp')
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
        prog='halcompupdate',
        description="Migrate HAL .comp files to the new HAL pin/param API "
                    "(real/bool/sint/uint/si32/ui32 types and <name>_set() "
                    "accessors). Without options a unified diff is printed.")
    p.add_argument('--in-place', '-i', action='store_true',
                   help="rewrite files in place (a .bak backup is kept unless "
                        "--no-backup is given)")
    p.add_argument('--no-backup', action='store_true',
                   help="with --in-place, do not keep a .bak backup")
    p.add_argument('--check', action='store_true',
                   help="do not write anything; exit 1 if a file would change")
    p.add_argument('--no-c-types', action='store_true',
                   help="only convert pin/param declarations and accesses, do "
                        "not modernize C types (double -> rtapi_real, "
                        "hal_float_t -> rtapi_real, ...)")
    p.add_argument('--quiet', '-q', action='store_true',
                   help="suppress warnings on stderr")
    p.add_argument('files', nargs='+', metavar='file.comp')
    args = p.parse_args(argv)

    errors = 0
    changed_any = False
    for fname in args.files:
        try:
            with open(fname, 'r', newline='') as f:
                text = f.read()
        except OSError as e:
            print("halcompupdate: %s" % e, file=sys.stderr)
            errors = 1
            continue
        newtext, changed = convert(text, fname, args.quiet,
                                   not args.no_c_types)
        if not changed:
            continue
        changed_any = True
        if args.check:
            continue
        if args.in_place:
            bak = None
            if not args.no_backup:
                bak = make_backup(fname)
            write_atomic(fname, newtext)
            if not args.quiet:
                print("halcompupdate: updated %s%s" %
                      (fname, "" if bak is None else " (backup: %s)" % bak),
                      file=sys.stderr)
        else:
            sys.stdout.writelines(difflib.unified_diff(
                text.splitlines(True), newtext.splitlines(True),
                fromfile=fname, tofile=fname + ".new"))
    if errors:
        return 2
    return 1 if (args.check and changed_any) else 0


if __name__ == '__main__':
    sys.exit(main())

# vim:sw=4:sts=4:et:syn=python
