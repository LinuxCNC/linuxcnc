#!/usr/bin/env python3
# Build a small TrueType (glyf-based) subset of a CFF-based OTF/TTC for
# prawn-pdf (used by asciidoctor-pdf) to embed.  Debian ships
# Noto Serif CJK only as a CFF/OTF TrueType Collection and prawn 2.4
# (the version on bookworm) corrupts the PDF when asked to embed CFF
# outlines directly, so we convert the curves with cu2qu first.
#
# Subsetting before conversion keeps the build under a second: a full
# Noto Serif CJK face is ~65 k glyphs (~45 s convert); the docs use a
# few hundred CJK characters at most.
#
# Usage:
#   otf2ttf.py <input.{otf,ttc}> <output.ttf> [--ttc-index N] [--text-from DIR]
#
# Built for the docs build only.  Not a general font tool.

import argparse, os, re, sys, time
from fontTools.ttLib import TTCollection, TTFont, newTable
from fontTools.pens.ttGlyphPen import TTGlyphPen
from fontTools.pens.cu2quPen import Cu2QuPen
from fontTools.subset import Subsetter, Options

# Characters we want available in the fallback font.  Defaults to BMP CJK
# unified ideographs + halfwidth / fullwidth forms.  ASCII is excluded
# since the base font already covers it.
CJK_RE = re.compile(r'[　-鿿＀-￯]')


def scan_cjk(root):
    seen = set()
    for dirpath, _, files in os.walk(root):
        for f in files:
            if not (f.endswith('.adoc') or f.endswith('.po')):
                continue
            try:
                t = open(os.path.join(dirpath, f), encoding='utf-8', errors='ignore').read()
            except OSError:
                continue
            seen.update(CJK_RE.findall(t))
    return seen


def cff_to_ttf(font):
    glyph_set = font.getGlyphSet()
    glyf = newTable('glyf')
    glyf.glyphs = {}
    glyf.glyphOrder = font.getGlyphOrder()
    for name in font.getGlyphOrder():
        pen = TTGlyphPen(None)
        try:
            glyph_set[name].draw(Cu2QuPen(pen, max_err=1.0, reverse_direction=True))
        except Exception:
            pass
        glyf.glyphs[name] = pen.glyph()
    font['glyf'] = glyf
    for t in ('CFF ', 'CFF2', 'VORG'):
        if t in font:
            del font[t]
    font['head'].indexToLocFormat = 1
    font['loca'] = newTable('loca')
    font.sfntVersion = '\x00\x01\x00\x00'


def convert(src, dst, ttc_index=None, text=None):
    t0 = time.time()
    if ttc_index is not None:
        font = TTCollection(src).fonts[ttc_index]
    else:
        font = TTFont(src)

    # `is not None`, not `if text:`: an empty scan result must still subset
    # (to notdef) rather than convert the full ~65k-glyph face (~200s).
    if text is not None:
        opts = Options()
        opts.layout_features = ['*']
        opts.notdef_outline = True
        opts.recommended_glyphs = True
        opts.name_IDs = ['*']
        opts.name_legacy = True
        opts.name_languages = ['*']
        sub = Subsetter(options=opts)
        sub.populate(text=text)
        sub.subset(font)

    if 'CFF ' in font or 'CFF2' in font:
        cff_to_ttf(font)

    font.save(dst)
    print(f'{dst}: {time.time()-t0:.1f}s ({font["maxp"].numGlyphs} glyphs)')


def main():
    p = argparse.ArgumentParser()
    p.add_argument('input')
    p.add_argument('output')
    p.add_argument('--ttc-index', type=int, default=None)
    p.add_argument('--text-from', action='append', default=[], metavar='DIR',
                   help='directory scanned for adoc/po sources; subset output '
                        'to characters found there.  Repeatable.')
    a = p.parse_args()
    if a.text_from:
        seen = set()
        for d in a.text_from:
            seen.update(scan_cjk(d))
        text = ''.join(sorted(seen))
    else:
        text = None
    convert(a.input, a.output, a.ttc_index, text)


if __name__ == '__main__':
    main()
