#!/usr/bin/env python3
"""Post-process every generated HTML page to grey out language-switcher
entries that point at a poorly-translated counterpart.

Each rendered HTML page contains a full switcher (one <li><a href="..."> per
language) emitted at asciidoctor time before per-page completeness is
known.  This pass walks the final docs/html/ tree and rewrites entries
that fall below the threshold to <li><span class="lcnc-lang-unavail">...</span></li>
so readers know at a glance which translations exist for the page they
are on.

Decision per (page, language):
  1. If no translated HTML file at the link's target, grey out (the file
     genuinely is missing, not just sparse).  This covers manpages, the
     gcode landing page, and any other page outside the AsciiDoc pipeline.
  2. Otherwise look up the page's master source in the language's .po
     file, compute (translated / total) over msgids whose location
     comment points at that master, grey out if below the threshold.
  3. Pages without a discoverable master (index.html, generated pages)
     fall back to step 1 (file existence).

Idempotent: a second run sees the same on-disk state plus the same .po
ratios, produces the same output, safe to bake into the build via a
stamp target."""

import os
import re
import sys
from collections import defaultdict

LIST_RE = re.compile(
    r'(<ul class="lcnc-lang-list">)(.*?)(</ul>)',
    re.DOTALL,
)
# Generic link entry inside <div class="details-list">.  Used by the
# manpage / docs index lists on index.html pages; on translated indexes
# the list is reused from English so some targets do not exist for that
# language.  Same class as the lang-switcher grey -- pointer-events: none
# and dim colour.
DETAILS_BLOCK_RE = re.compile(
    r'(<div class="details-list">)(.*?)(</div>)',
    re.DOTALL,
)
DETAILS_ENTRY_RE = re.compile(
    r'(\s*<li)(?:\s+class="lcnc-link-unavail")?(>)<a href="([^"]+)">([^<]+)</a></li>'
)
# Match every <li> in the list, capturing its class attr (if any), the
# href, and the visible label.  The class attr is rewritten in-place each
# run so re-running with a different threshold is reversible.
ENTRY_RE = re.compile(
    r'(\s*)<li(?:\s+class="lcnc-lang-unavail")?>'
    r'<a href="([^"]+)">([^<]+)</a></li>'
)


def parse_po(path):
    """Yield (msgid_nonempty, msgstr_nonempty, fuzzy, locations) per entry."""
    results = []
    entry = {
        'msgid': '', 'msgstr': '', 'fuzzy': False, 'locs': [],
        'in_msgid': False, 'in_msgstr': False,
    }

    def reset():
        entry['msgid'] = ''
        entry['msgstr'] = ''
        entry['fuzzy'] = False
        entry['locs'] = []
        entry['in_msgid'] = False
        entry['in_msgstr'] = False

    def emit():
        results.append((bool(entry['msgid']), bool(entry['msgstr']),
                        entry['fuzzy'], list(entry['locs'])))

    str_re = re.compile(r'^"(.*)"\s*$')
    with open(path, 'r', encoding='utf-8') as f:
        for line in f:
            line = line.rstrip('\n')
            if line == '':
                if entry['msgid'] or entry['msgstr'] or entry['locs']:
                    emit()
                    reset()
                continue
            if line.startswith('#~'):
                continue
            if line.startswith('#:'):
                entry['locs'].extend(line[2:].split())
                continue
            if line.startswith('#,'):
                if 'fuzzy' in line:
                    entry['fuzzy'] = True
                continue
            if line.startswith('#'):
                continue
            if line.startswith('msgid '):
                entry['in_msgid'] = True
                entry['in_msgstr'] = False
                m = str_re.match(line[6:])
                if m:
                    entry['msgid'] += m.group(1)
                continue
            if line.startswith('msgstr '):
                entry['in_msgid'] = False
                entry['in_msgstr'] = True
                m = str_re.match(line[7:])
                if m:
                    entry['msgstr'] += m.group(1)
                continue
            m = str_re.match(line)
            if m:
                if entry['in_msgid']:
                    entry['msgid'] += m.group(1)
                elif entry['in_msgstr']:
                    entry['msgstr'] += m.group(1)
                continue
    if entry['msgid'] or entry['msgstr'] or entry['locs']:
        emit()
    return results


def per_master_ratio(po_path):
    """Return {master_path: (total, translated)} mined from .po."""
    ratios = defaultdict(lambda: [0, 0])
    loc_re = re.compile(r'^([^:]+\.[A-Za-z0-9]+):\d+$')
    for has_id, has_str, fuzzy, locs in parse_po(po_path):
        if not has_id:
            continue
        seen = set()
        for loc in locs:
            m = loc_re.match(loc)
            if m:
                seen.add(m.group(1))
        for master in seen:
            ratios[master][0] += 1
            if has_str and not fuzzy:
                ratios[master][1] += 1
    return ratios


# Map: page's docs/html-relative subpath -> master source path used by .po
# location comments.  e.g. de/config/ini-config.html -> src/config/ini-config.adoc
def subpath_to_master(subpath):
    # Strip leading language dir if present (de/foo -> foo).
    parts = subpath.split('/')
    if parts and parts[0] in LANGUAGES:
        parts = parts[1:]
    flat = '/'.join(parts)
    if flat.endswith('.html'):
        flat = flat[:-5] + '.adoc'
    # Manpages: docs/html/man/man1/foo.1.html corresponds to src/man/man1/foo.1.adoc
    # Other docs: docs/html/config/foo.html corresponds to src/config/foo.adoc
    return 'src/' + flat


LANGUAGES = []  # populated from CLI


def rewrite_block(html_path, html_dir, html_root, po_ratios, threshold, block_body):
    def repl(m):
        indent, href, label = m.group(1), m.group(2), m.group(3)
        target = os.path.normpath(os.path.join(html_dir, href))
        unavail = False
        if target.startswith(html_root):
            if not os.path.exists(target):
                unavail = True
            else:
                rel = os.path.relpath(target, html_root)
                parts = rel.split('/')
                if parts and parts[0] in LANGUAGES:
                    lang = parts[0]
                    master = subpath_to_master(rel)
                    ratios = po_ratios.get(lang, {})
                    tot, tr = ratios.get(master, (0, 0))
                    if tot > 0 and (100.0 * tr / tot) < threshold:
                        unavail = True
        cls = ' class="lcnc-lang-unavail"' if unavail else ''
        return f'{indent}<li{cls}><a href="{href}">{label}</a></li>'
    return ENTRY_RE.sub(repl, block_body)


def rewrite_details_block(html_dir, block_body):
    """Toggle the lcnc-link-unavail class on every <li><a> entry in a
    details-list block based on whether the link target exists on disk.
    Used so translated index pages do not surface dead manpage links."""
    def repl(m):
        before, after, href, label = m.group(1), m.group(2), m.group(3), m.group(4)
        target = os.path.normpath(os.path.join(html_dir, href))
        if os.path.exists(target):
            cls = ''
        else:
            cls = ' class="lcnc-link-unavail"'
        return f'{before}{cls}{after}<a href="{href}">{label}</a></li>'
    return DETAILS_ENTRY_RE.sub(repl, block_body)


def process(html_path, html_root, po_ratios, threshold):
    with open(html_path, 'r', encoding='utf-8') as f:
        content = f.read()
    if 'lcnc-lang-list' not in content and 'details-list' not in content:
        return False
    html_dir = os.path.dirname(html_path)
    new_content = content

    if 'lcnc-lang-list' in new_content:
        def list_repl(m):
            return m.group(1) + rewrite_block(html_path, html_dir, html_root,
                                              po_ratios, threshold, m.group(2)) + m.group(3)
        new_content = LIST_RE.sub(list_repl, new_content)

    if 'details-list' in new_content:
        def details_repl(m):
            return m.group(1) + rewrite_details_block(html_dir, m.group(2)) + m.group(3)
        new_content = DETAILS_BLOCK_RE.sub(details_repl, new_content)

    if new_content == content:
        return False
    with open(html_path, 'w', encoding='utf-8') as f:
        f.write(new_content)
    return True


def main(html_root, po_dir, threshold, languages):
    global LANGUAGES
    LANGUAGES = languages
    html_root = os.path.abspath(html_root)
    po_ratios = {}
    for lang in languages:
        po_path = os.path.join(po_dir, f'{lang}.po')
        if os.path.exists(po_path):
            po_ratios[lang] = per_master_ratio(po_path)
    changed = 0
    seen = 0
    for dirpath, _dirs, files in os.walk(html_root):
        for name in files:
            if not name.endswith('.html'):
                continue
            seen += 1
            if process(os.path.join(dirpath, name), html_root, po_ratios, threshold):
                changed += 1
    print(f'lang_switcher_postprocess: scanned {seen} HTML files, rewrote {changed} '
          f'(threshold {threshold}%)')


if __name__ == '__main__':
    if len(sys.argv) < 5:
        sys.stderr.write('Usage: lang_switcher_postprocess.py <html-root> <po-dir> <threshold> <lang1> [lang2 ...]\n')
        sys.exit(2)
    main(sys.argv[1], sys.argv[2], float(sys.argv[3]), sys.argv[4:])
