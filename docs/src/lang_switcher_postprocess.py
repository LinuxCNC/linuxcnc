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
# Previously injected nav tree, stripped before re-inject (idempotent).
SITENAV_RE = re.compile(r'<nav class="lcnc-sitenav".*?</nav>\n?', re.DOTALL)
# A tagged topbar link (data-lcnc-link="N"); the text is replaced with the
# per-language label from build/adoc/<lang>/topbar-labels (po4a/weblate).
TOPBAR_LINK_RE = re.compile(r'(<a data-lcnc-link="(\d+)"[^>]*>)([^<]*)(</a>)')
TOPBAR_LABELS = {}      # lang -> [label, ...]; filled in main()
# Topbar logo link; retargeted to the current language's home.  Any already
# inserted <lang>/ is matched and replaced, so re-runs stay idempotent.
TOPBAR_HOME_RE = re.compile(
    r'(<a class="lcnc-topbar-home" href=")((?:\.\./)*)(?:[A-Za-z_]+/)?(index\.html")')


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


# Whole-document navigation tree mirroring the Master_*.adoc structure; each
# page gets the tree with its branch expanded and its entry active.
SRC_DIR = os.path.dirname(os.path.abspath(__file__))
SITENAV_ROOTS = []      # built once in main()
_TITLE_CACHE = {}


def _html_escape(s):
    return (s.replace('&', '&amp;').replace('<', '&lt;')
             .replace('>', '&gt;').replace('"', '&quot;'))


def _clean_title(s):
    # Drop asciidoctor index terms: (((concealed))) entirely, ((visible)) -> text.
    s = re.sub(r'\(\(\(.*?\)\)\)', '', s)
    s = re.sub(r'\(\((.*?)\)\)', r'\1', s)
    # Drop the " V{lversion}" / attribute tail and any leftover {attr}.
    s = re.sub(r'\s*V?\{[^}]*\}.*$', '', s)
    return s.strip()


def _humanize(path):
    return os.path.basename(path).replace('-', ' ').replace('_', ' ').title()


def _page_title(path):
    """First '= Heading' of the page's English source adoc, else humanized."""
    if path in _TITLE_CACHE:
        return _TITLE_CACHE[path]
    title = None
    src = os.path.join(SRC_DIR, path + '.adoc')
    try:
        with open(src, 'r', encoding='utf-8') as f:
            for line in f:
                if line.startswith('= '):
                    title = _clean_title(line[2:])
                    break
    except OSError:
        pass
    title = title or _humanize(path)
    _TITLE_CACHE[path] = title
    return title


# Books that seed the tree.  promote=True splices the book's parts in at top
# level instead of nesting them under a redundant book node.
SITENAV_MASTERS = (('Master_Documentation.adoc', True),
                   ('Master_Integrator.adoc', False),
                   ('Master_Developer.adoc', False))
_INC_RE = re.compile(r'^include::([^\[]+)\.adoc\[')
_LVL_RE = re.compile(r'^:leveloffset:\s*([+-]?\d+)')


def _parse_master(master):
    """One book node {title, path:None, children:[parts/groups/pages]}."""
    try:
        f = open(os.path.join(SRC_DIR, master), 'r', encoding='utf-8')
    except OSError:
        return None
    book = {'title': None, 'path': None, 'children': []}
    stack = [(-1, book)]          # book holds everything below level 0
    level = 0
    with f:
        for line in f:
            line = line.rstrip('\n')
            m = _LVL_RE.match(line)
            if m:
                v = m.group(1)
                level = level + int(v) if v[0] in '+-' else int(v)
                continue
            if line.startswith('= '):
                title = _clean_title(line[2:])
                if book['title'] is None:
                    book['title'] = title
                    continue
                node = {'title': title, 'path': None, 'children': []}
                while stack[-1][0] >= level:
                    stack.pop()
                stack[-1][1]['children'].append(node)
                stack.append((level, node))
                continue
            mi = _INC_RE.match(line)
            if mi:
                path = mi.group(1).strip()
                stack[-1][1]['children'].append(
                    {'title': _page_title(path), 'path': path, 'children': []})
    return book


def build_sitenav(masters=SITENAV_MASTERS):
    """Nested nav model.  Node dicts: group/book = {title, path:None,
    children:[...]}, page = {title, path, children:[]}."""
    roots = []
    for master, promote in masters:
        book = _parse_master(master)
        if not book or not book['children']:
            continue
        if promote:
            roots.extend(book['children'])
        else:
            roots.append(book)
    return roots


def _has_active(node, active):
    if node['path'] == active:
        return True
    return any(_has_active(c, active) for c in node['children'])


def _active_leaf(title, href, page_toc):
    """The active page entry.  With its own sections it becomes the
    disclosure itself: the name is the summary (toggles the section list
    rather than linking back to the page you are on), collapsed so the
    sections do not push sibling pages out of view."""
    if not page_toc:
        return (f'<a class="lcnc-sn-active" tabindex="-1" autofocus '
                f'href="{href}">{_html_escape(title)}</a>')
    return (f'<details class="lcnc-sn-page"><summary class="lcnc-sn-active" '
            f'autofocus>{_html_escape(title)}</summary>{page_toc}</details>')


def _render_node(node, active, prefix, page_toc):
    if node['path'] is not None and not node['children']:
        href = prefix + node['path'] + '.html'
        if node['path'] == active:
            return _active_leaf(node['title'], href, page_toc)
        return f'<a href="{href}">{_html_escape(node["title"])}</a>'
    inner = ''.join(_render_node(c, active, prefix, page_toc)
                    for c in node['children'])
    # Open only the branch leading to the active page; everything else folds.
    op = ' open' if _has_active(node, active) else ''
    return (f'<details{op}><summary>{_html_escape(node["title"])}</summary>'
            f'{inner}</details>')


def render_sitenav(active, prefix, page_toc='', orphan_title=None):
    body = ''
    # A page absent from the master tree (orphan_title set) still gets its own
    # entry + section list at the top, so it is never left without a TOC.
    if orphan_title is not None:
        href = prefix + active + '.html'
        body += _active_leaf(orphan_title, href, page_toc)
    body += ''.join(_render_node(n, active, prefix, page_toc)
                    for n in SITENAV_ROOTS)
    return f'<nav class="lcnc-sitenav" aria-label="Documentation">{body}</nav>'


def _extract_sectlevel1(html):
    """The page's own asciidoctor TOC list (balanced <ul class=sectlevel1>)."""
    start = html.find('<ul class="sectlevel1">')
    if start < 0:
        return ''
    depth = 0
    for m in re.finditer(r'<ul\b|</ul>', html[start:]):
        depth += 1 if m.group() == '<ul' else -1
        if depth == 0:
            return html[start:start + m.end()]
    return ''


def _toc2_bounds(content):
    """(inner_start, inner_end, close_end) of the toc2 div, or None."""
    open_tag = '<div id="toc" class="toc2">'
    i = content.find(open_tag)
    if i < 0:
        return None
    j = i + len(open_tag)
    depth = 1
    for m in re.finditer(r'<div\b|</div>', content[j:]):
        depth += 1 if m.group() == '<div' else -1
        if depth == 0:
            return (j, j + m.start(), j + m.end())
    return None


def _scaffold_sidebar(content, nav):
    """Section-less pages have no toc2 container; add the body classes and
    insert the sidebar div after the page <h1>."""
    def body_cls(m):
        attrs, cls = m.group(1), m.group(2)
        if 'toc2' in cls:
            return m.group(0)
        return f'<body{attrs} class="{cls} toc2 toc-left"'
    content = re.sub(r'<body\b([^>]*?) class="([^"]*)"', body_cls, content, count=1)
    div = f'<div id="toc" class="toc2">{nav}</div>\n'
    return re.sub(r'(<div id="header">.*?</h1>\s*)',
                  lambda m: m.group(1) + div, content, count=1, flags=re.DOTALL)


def inject_sitenav(html_path, html_root, content):
    """Replace asciidoctor's page-only TOC sidebar with the whole-document
    tree; scaffold a sidebar for section-less pages that have none."""
    # Recover the page's section list before stripping any prior nav (on a
    # re-run it lives nested inside it), so it is never lost.
    pre = _toc2_bounds(content)
    page_toc = _extract_sectlevel1(content[pre[0]:pre[1]]) if pre else ''
    content = SITENAV_RE.sub('', content)
    if not SITENAV_ROOTS:
        return content
    rel = os.path.relpath(html_path, html_root).split('/')
    if rel and rel[0] in (['en'] + list(LANGUAGES)):
        rel = rel[1:]
    page_rel = '/'.join(rel)
    active = page_rel[:-5] if page_rel.endswith('.html') else page_rel
    prefix = '../' * page_rel.count('/')
    in_tree = any(_has_active(n, active) for n in SITENAV_ROOTS)
    bounds = _toc2_bounds(content)
    if bounds is not None:
        orphan = None if in_tree else _page_title(active)
        nav = render_sitenav(active, prefix, page_toc, orphan_title=orphan)
        inner_start, inner_end, _close = bounds
        return content[:inner_start] + nav + content[inner_end:]
    # No toc2: scaffold only when the page is in the tree (orphans stay bare).
    if not in_tree:
        return content
    return _scaffold_sidebar(content, render_sitenav(active, prefix, page_toc))


def _page_lang(html_path, html_root):
    rel = os.path.relpath(html_path, html_root).split('/')
    return rel[0] if rel else None


def process(html_path, html_root, po_ratios):
    with open(html_path, 'r', encoding='utf-8') as f:
        content = f.read()
    if ('lcnc-lang-list' not in content and 'details-list' not in content
            and 'id="toc" class="toc2"' not in content):
        return False
    html_dir = os.path.dirname(html_path)
    new_content = content
    lang = _page_lang(html_path, html_root)

    # Point the topbar logo at this language's home, not the generic redirect.
    if lang in (['en'] + list(LANGUAGES)):
        new_content = TOPBAR_HOME_RE.sub(
            lambda m: m.group(1) + m.group(2) + lang + '/' + m.group(3),
            new_content, count=1)

    # Localize the topbar site links for translated pages (English is the
    # template default).  Labels come from the po4a-translated strings file.
    labels = TOPBAR_LABELS.get(lang)
    if labels and 'data-lcnc-link=' in new_content:
        def link_repl(m):
            i = int(m.group(2))
            text = labels[i] if i < len(labels) and labels[i] else m.group(3)
            return m.group(1) + _html_escape(text) + m.group(4)
        new_content = TOPBAR_LINK_RE.sub(link_repl, new_content)

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

    # Whole-document navigation tree in the left sidebar.
    new_content = inject_sitenav(html_path, html_root, new_content)

    if new_content == content:
        return False
    with open(html_path, 'w', encoding='utf-8') as f:
        f.write(new_content)
    return True


def _load_topbar_labels(html_root, languages):
    # build/adoc/<lang>/topbar-labels (po4a output), sibling of build/html.
    adoc = os.path.join(os.path.dirname(html_root), 'adoc')
    for lang in languages:
        path = os.path.join(adoc, lang, 'topbar-labels')
        try:
            with open(path, 'r', encoding='utf-8') as f:
                # po4a separates the strings by blank lines; keep the order.
                TOPBAR_LABELS[lang] = [ln.strip() for ln in f if ln.strip()]
        except OSError:
            pass


def main(html_root, po_dir, languages):
    global LANGUAGES, SITENAV_ROOTS
    LANGUAGES = languages
    SITENAV_ROOTS = build_sitenav()
    html_root = os.path.abspath(html_root)
    _load_topbar_labels(html_root, languages)
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
    # argv[3] is the legacy POKEEP slot, accepted and ignored.  The language
    # list may be empty (English-only build): sidebar/topbar still inject.
    if len(sys.argv) < 4:
        sys.stderr.write('Usage: lang_switcher_postprocess.py <html-root> <po-dir> <threshold> [lang1 ...]\n')
        sys.exit(2)
    main(sys.argv[1], sys.argv[2], sys.argv[4:])
