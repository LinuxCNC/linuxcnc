#!/usr/bin/env python3
"""Post-process every generated HTML page for translation status.

Two passes, both reversible so a second run is a no-op (safe to bake into
the build via a stamp target):

  1. Language switcher: grey a <li><a> only when its target file is
     genuinely absent (so the switcher never offers a 404).  Sparse but
     present translations stay clickable -- completeness is conveyed by
     the banner, not by hiding the link.
  2. Per-page banner: on a translated page that is not fully translated,
     inject a no-JS notice just below the topbar stating "this page is
     N% translated", tinted red(0%)..yellow..green(100%).  N comes from
     the language's .po: msgids whose location comment points at the
     page's master source, counted translated / total.

Pages without a discoverable master (English, generated index pages) get
no banner.  Dead links in the manpage index lists are still greyed by
file existence (separate concern, see rewrite_details_block)."""

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
# Topbar header element (HTML5 <header>, distinct from asciidoctor's
# <div id="header">); the banner is injected right after it.
PAGE_HEADER_RE = re.compile(r'(<header id="lcnc-topbar".*?</header>)', re.DOTALL)
# A previously injected banner, removed before re-injecting so the pass
# stays idempotent and reflects the current .po ratio each run.
BANNER_RE = re.compile(r'\n?[ \t]*<div class="lcnc-trans-banner".*?</div>', re.DOTALL)


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


def page_coverage(html_path, html_root, po_ratios):
    """(translated, total) for this page's own language, or None when the
    page has no discoverable master (English, generated index pages)."""
    rel = os.path.relpath(html_path, html_root)
    parts = rel.split('/')
    if not parts or parts[0] not in LANGUAGES:
        return None
    master = subpath_to_master(rel)
    tot, tr = po_ratios.get(parts[0], {}).get(master, (0, 0))
    return (tr, tot) if tot > 0 else None


def banner_html(pct):
    """No-JS translation-status banner.  Only the hue (red 0..green 120)
    is emitted inline as a custom property; the stylesheet picks the
    lightness so the light and dark themes each tint it their own way."""
    hue = int(round(1.2 * pct))  # 0->red, 50->60 yellow, 100->120 green
    return (f'<div class="lcnc-trans-banner" style="--lcnc-pct-hue:{hue}">'
            f'This page is {pct}% translated. '
            f'Untranslated text is shown in English.</div>')


def rewrite_block(html_dir, html_root, block_body):
    def repl(m):
        indent, href, label = m.group(1), m.group(2), m.group(3)
        target = os.path.normpath(os.path.join(html_dir, href))
        # Grey only entries whose target file is genuinely absent (avoids a
        # 404 from the switcher).  Sparse-but-present translations stay
        # clickable; the per-page banner conveys completeness instead.
        unavail = target.startswith(html_root) and not os.path.exists(target)
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


def process(html_path, html_root, po_ratios):
    with open(html_path, 'r', encoding='utf-8') as f:
        content = f.read()
    if 'lcnc-lang-list' not in content and 'details-list' not in content:
        return False
    html_dir = os.path.dirname(html_path)
    new_content = content

    if 'lcnc-lang-list' in new_content:
        def list_repl(m):
            return m.group(1) + rewrite_block(html_dir, html_root, m.group(2)) + m.group(3)
        new_content = LIST_RE.sub(list_repl, new_content)

    if 'details-list' in new_content:
        def details_repl(m):
            return m.group(1) + rewrite_details_block(html_dir, m.group(2)) + m.group(3)
        new_content = DETAILS_BLOCK_RE.sub(details_repl, new_content)

    # Translation-status banner: drop any prior one, re-derive from the .po.
    new_content = BANNER_RE.sub('', new_content)
    cov = page_coverage(html_path, html_root, po_ratios)
    if cov is not None:
        tr, tot = cov
        if tr < tot:
            pct = tr * 100 // tot  # floor: 99.6% never reads as 100
            new_content = PAGE_HEADER_RE.sub(
                lambda m: m.group(1) + '\n' + banner_html(pct),
                new_content, count=1)

    if new_content == content:
        return False
    with open(html_path, 'w', encoding='utf-8') as f:
        f.write(new_content)
    return True


def main(html_root, po_dir, languages):
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
            if process(os.path.join(dirpath, name), html_root, po_ratios):
                changed += 1
    print(f'lang_switcher_postprocess: scanned {seen} HTML files, rewrote {changed}')


if __name__ == '__main__':
    # argv[3] is the legacy POKEEP threshold slot; banners now show for any
    # incomplete page, so it is accepted for call compatibility and ignored.
    if len(sys.argv) < 5:
        sys.stderr.write('Usage: lang_switcher_postprocess.py <html-root> <po-dir> <threshold> <lang1> [lang2 ...]\n')
        sys.exit(2)
    main(sys.argv[1], sys.argv[2], sys.argv[4:])
