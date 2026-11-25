#!/usr/bin/env python3
# Flag section references relying on auto-generated AsciiDoc ids:
# <<_derived_id>> xrefs and link:...html#_derived_id URLs. Derived ids come
# from the section title, so the reference breaks when the title is
# translated or retitled. Pin the target with an explicit [[anchor]] instead.
# Pass 1 collects explicit anchor definitions in docs/src, pass 2 reports
# derived-id references with no explicit target.
# Warn-only unless --enforce or DOCS_ANCHOR_CHECK_ENFORCE is set.

import os
import re
import sys
import glob

# The script lives in scripts/, the docs are one level up at ../docs, so it runs from anywhere.
HERE = os.path.dirname(os.path.realpath(__file__, strict=True))
DOCS = os.path.normpath(os.path.join(HERE, '..', 'docs'))
SRC = os.path.join(DOCS, 'src')

# Explicit anchor definition forms, mirroring ANCHOR_DEF in
# docs/src/extensions/xref_resolver.rb, plus the anchor: macro.
ANCHOR_DEF = re.compile(r"""
    \[\[ ([A-Za-z_][\w:.-]*) (?:,[^\]]*)? \]\]     |   # [[id]] or [[id,reftext]]
    \[\# ([A-Za-z_][\w:.-]*) (?:[.%][^\]]*)? \]    |   # [#id]
    \[ (?:[^,\]]*,\s*)* id\s*=\s*["']? ([A-Za-z_][\w:.-]*) ["']? [,\]] |   # [id="foo"]
    ^anchor: ([A-Za-z_][\w:.-]*) \[\]              |   # anchor:id[]
    ^:id:\s* ([A-Za-z_][\w:.-]*)                       # :id: foo
""", re.X | re.M)

# Derived-id references: <<_foo>> / <<_foo,Title>> xrefs and
# link:...html#_foo[...] URL fragments.  Namespaced anchors carry a ':', so a
# target that starts with '_' and has no ':' is a derived id by convention.
XREF = re.compile(r'<<(_[A-Za-z0-9][\w.-]*)(?:,.*?)?>>')
LINKURL = re.compile(r'link:[^\s\[]*#(_[A-Za-z0-9][\w.-]*)\[')

def collect(adoc_files):
    defined = {}  # anchor -> file
    for path in adoc_files:
        text = open(path, encoding='utf-8', errors='replace').read()
        for m in ANCHOR_DEF.finditer(text):
            anchor = next(g for g in m.groups() if g)
            defined.setdefault(anchor, path)
    return defined

def find_derived_refs(adoc_files, defined):
    problems = []
    for path in adoc_files:
        lines = open(path, encoding='utf-8', errors='replace').read().splitlines()
        for lineno, line in enumerate(lines, 1):
            for regex in (XREF, LINKURL):
                for m in regex.finditer(line):
                    target = m.group(1)
                    if target not in defined:
                        rel = os.path.relpath(path, DOCS)
                        problems.append((rel, lineno, m.group(0)[:60], target))
    return problems

def main():
    enforce = '--enforce' in sys.argv or os.environ.get('DOCS_ANCHOR_CHECK_ENFORCE')
    adoc_files = sorted(glob.glob(f'{SRC}/**/*.adoc', recursive=True))
    defined = collect(adoc_files)
    problems = find_derived_refs(adoc_files, defined)
    if not problems:
        return 0
    out = ['Derived-id section references with no explicit anchor target',
           '(these break in translated docs when the target title is translated;',
           'pin the target section with an explicit [[anchor]] and reference that):',
           '']
    for rel, lineno, ref, target in problems:
        out.append(f'{rel}:{lineno}: {ref}')
        out.append(f'    target `[[{target}]]` is not explicitly defined anywhere in docs/src')
    text = '\n'.join(out)
    print(text)
    if os.environ.get('GITHUB_ACTIONS'):
        summary = os.environ.get('GITHUB_STEP_SUMMARY')
        if summary:
            with open(summary, 'a', encoding='utf-8') as f:
                f.write('## Derived-id anchor check\n\n```\n' + text + '\n```\n')
        level = 'error' if enforce else 'warning'
        print(f'::{level} title=Derived-id anchor check::{len(problems)} reference(s) rely on auto-generated section ids, see job summary')
    return 1 if enforce else 0

if __name__ == '__main__':
    sys.exit(main())
