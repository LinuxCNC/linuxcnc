#!/usr/bin/env python3
# Report man pages whose rendered NAME drifted from their filename.
# asciidoctor takes the .TH name from the page's NAME section, the build pins the filename with -o, so a translated NAME just leaves the two disagreeing.
# Run after building the docs.  Argument: the built man tree (default docs/build/man).
# Quiet when nothing drifted.  When something has, it prints each page with the
# string to search in Weblate and its current value, so a translator can fix it.
# Never fails the build: under GitHub Actions it writes the same list to the job
# summary and emits one warning pointing at it.  The NAME lines are managed in
# Weblate, so fixes go there.

import os
import re
import sys
import glob

# The script lives in scripts/, the docs are one level up at ../docs, so it runs from anywhere.
HERE = os.path.dirname(os.path.realpath(__file__, strict=True))
DOCS = os.path.normpath(os.path.join(HERE, '..', 'docs'))
MAN = os.path.join(DOCS, 'build/man')
PO = os.path.join(DOCS, 'po')
SRC = os.path.join(DOCS, 'src/man')

def th_name(troff):
    """The .TH page name, lowercased with the roff backslash stripped, or None for an .so alias stub."""
    with open(troff, encoding='utf-8', errors='replace') as f:
        if f.readline().startswith('.so '):
            return None
        f.seek(0)
        for line in f:
            m = re.match(r'\.TH "([^"]*)"', line)
            if m:
                return m.group(1).replace('\\', '').lower()
    return None

def name_line(page):
    """The first line of the NAME section of a page's AsciiDoc source, or None for a comp page rendered straight to troff."""
    for adoc in sorted(glob.glob(f'{SRC}/man*/{page}.*.adoc')):
        lines = open(adoc, encoding='utf-8', errors='replace').read().splitlines()
        for i, line in enumerate(lines):
            if line.strip() == '== NAME':
                for rest in lines[i + 1:]:
                    if rest.strip():
                        return rest.strip()
    return None

_po = {}
def po(lang):
    """Parse docs/po/<lang>.po into {msgid: msgstr}, joining the continuation lines that wrap one string."""
    if lang not in _po:
        d = {}
        mid = mstr = mode = None
        for line in open(f'{PO}/{lang}.po', encoding='utf-8', errors='replace'):
            if line.startswith('msgid '):
                if mid is not None:
                    d[mid] = mstr or ''
                mid, mstr, mode = line[6:].strip().strip('"'), None, 'id'
            elif line.startswith('msgstr '):
                mstr, mode = line[7:].strip().strip('"'), 'str'
            elif line.startswith('"'):
                s = line.strip().strip('"')
                if mode == 'id':
                    mid += s
                elif mode == 'str':
                    mstr += s
            elif not line.strip() and mid is not None:
                d[mid] = mstr or ''
                mid = mstr = mode = None
        if mid is not None:
            d[mid] = mstr or ''
        _po[lang] = d
    return _po[lang]

def weblate_string(lang, page):
    """Return (search, current): the text to search for in Weblate and its current translation."""
    d = po(lang)
    th = page.upper()
    # A comp page's name is a troff .TH string, searched by the uppercase name.
    if d.get(th, '').strip() and d[th].strip().lower() != page.lower():
        return th, d[th]
    # An AsciiDoc page's name lives in the NAME line, searched by its text.
    nl = name_line(page)
    if nl:
        cur = d.get(nl)
        if cur is None:                                  # the source line and the msgid can differ in trailing text
            for mid, mstr in d.items():
                if mid.startswith(page) and ' - ' in mid:
                    nl, cur = mid, mstr
                    break
        return nl, cur if cur is not None else '(search the text)'
    return page, '(unknown)'

def drifted(man):
    """Yield (lang, page) for every built man page whose .TH name does not match its filename."""
    for dirpath, _dirs, files in os.walk(man):
        for fn in files:
            if not re.search(r'\.\d$', fn):
                continue
            th = th_name(os.path.join(dirpath, fn))
            page = re.sub(r'\.\d+$', '', fn)
            if th is None or th == page.lower():
                continue
            lang = os.path.relpath(dirpath, man).split(os.sep)[0]
            yield ('en' if re.fullmatch(r'man\d+', lang) else lang), page

def collect(man):
    """All drifted rows as (lang, search, current), sorted, English dropped."""
    rows = []
    for lang, page in drifted(man):
        if lang == 'en':                                 # English is the reference, it never drifts
            continue
        search, current = weblate_string(lang, page)
        rows.append((lang, search, current))
    rows.sort()
    return rows

def print_local(rows):
    print(f'# {len(rows)} man-page NAME mismatches. Search the "search" text in the Weblate docs component, language in brackets.')
    for lang, search, current in rows:
        print(f'\n[{lang}]')
        print(f'    search:  {search}')
        print(f'    current: {current}')

def write_summary(rows, fh):
    fh.write(f'## Manpage NAME mismatches: {len(rows)}\n\n')
    fh.write('Translated NAME lines that drifted from the command name.\n')
    fh.write('Search the "search" text in the Weblate docs component for the language and fix it there.\n\n')
    fh.write('| lang | search | current |\n')
    fh.write('| ---- | ------ | ------- |\n')
    for lang, search, current in rows:
        cells = (c.replace('|', '\\|') for c in (lang, search, current))
        fh.write('| %s | %s | %s |\n' % tuple(cells))

def main(argv):
    man = argv[1] if len(argv) > 1 else MAN
    # A built tree has troff pages; without them the docs are not built and a
    # silent "all clean" would read as success when nothing was actually scanned.
    if not any(re.search(r'\.\d$', f) for _root, _dirs, files in os.walk(man) for f in files):
        print(f'{man}: no built man pages found, build the docs first (make manpages docs).', file=sys.stderr)
        return 1
    rows = collect(man)
    if not rows:
        return 0                                         # quiet when nothing drifted
    print_local(rows)
    summary = os.environ.get('GITHUB_STEP_SUMMARY')
    if summary:
        with open(summary, 'a', encoding='utf-8') as fh:
            write_summary(rows, fh)
    if os.environ.get('GITHUB_ACTIONS'):
        print(f'::warning title=manpage NAME drift::{len(rows)} manpage NAME line(s) disagree with their filename, see the job summary for the list to fix in Weblate')
    return 0

if __name__ == '__main__':
    sys.exit(main(sys.argv))
