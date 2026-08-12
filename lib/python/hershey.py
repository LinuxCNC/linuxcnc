#    This is a component of AXIS, a front-end for emc
#    Copyright 2005, 2006 Chris Radek <chris@timeguy.com>
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

import itertools

translate = {'0': 0, '1': 1, '2': 2, '3': 3, '4': 4, '5': 5, '6': 6, '7': 7, '8': 8, '9': 9, '-': 10, '.': 11, 'X': 12, 'Y': 13, 'Z': 14, 'G': 15,
        'U': 16, 'V': 17, 'W': 18}

class Hershey:
    def __init__(self):
        self.hershey = (
    # 0
        [[(240.0, 20.0), (180.0, 40.0), (140.0, 100.0), (120.0, 200.0),
          (120.0, 260.0), (140.0, 360.0), (180.0, 420.0), (240.0, 440.0),
          (280.0, 440.0), (340.0, 420.0), (380.0, 360.0), (400.0, 260.0),
          (400.0, 200.0), (380.0, 100.0), (340.0, 40.0), (280.0, 20.0),
          (240.0, 20.0)]], 
    # 1
        [[(120.0, 100.0), (160.0, 80.0), (220.0, 20.0), (220.0, 440.0)]], 
    # 2
        [[(140.0, 120.0), (140.0, 100.0), (160.0, 60.0), (180.0, 40.0),
          (220.0, 20.0), (300.0, 20.0), (340.0, 40.0), (360.0, 60.0),
          (380.0, 100.0), (380.0, 140.0), (360.0, 180.0), (320.0, 240.0),
          (120.0, 440.0), (400.0, 440.0)]], 
    # 3
        [[(160.0, 20.0), (380.0, 20.0), (260.0, 180.0), (320.0, 180.0),
          (360.0, 200.0), (380.0, 220.0), (400.0, 280.0), (400.0, 320.0),
          (380.0, 380.0), (340.0, 420.0), (280.0, 440.0), (220.0, 440.0),
          (160.0, 420.0), (140.0, 400.0), (120.0, 360.0)]], 
    # 4
        [[(320.0, 20.0), (120.0, 300.0), (420.0, 300.0)], [(320.0, 20.0),
          (320.0, 440.0)]], 
    # 5
        [[(360.0, 20.0), (160.0, 20.0), (140.0, 200.0), (160.0, 180.0),
          (220.0, 160.0), (280.0, 160.0), (340.0, 180.0), (380.0, 220.0),
          (400.0, 280.0), (400.0, 320.0), (380.0, 380.0), (340.0, 420.0),
          (280.0, 440.0), (220.0, 440.0), (160.0, 420.0), (140.0, 400.0),
          (120.0, 360.0)]], 
    # 6
        [[(380.0, 80.0), (360.0, 40.0), (300.0, 20.0), (260.0, 20.0),
          (200.0, 40.0), (160.0, 100.0), (140.0, 200.0), (140.0, 300.0),
          (160.0, 380.0), (200.0, 420.0), (260.0, 440.0), (280.0, 440.0),
          (340.0, 420.0), (380.0, 380.0), (400.0, 320.0), (400.0, 300.0),
          (380.0, 240.0), (340.0, 200.0), (280.0, 180.0), (260.0, 180.0),
          (200.0, 200.0), (160.0, 240.0), (140.0, 300.0)]], 
    # 7
        [[(400.0, 20.0), (200.0, 440.0)], [(120.0, 20.0), (400.0, 20.0)]], 
    # 8
        [[(220.0, 20.0), (160.0, 40.0), (140.0, 80.0), (140.0, 120.0),
          (160.0, 160.0), (200.0, 180.0), (280.0, 200.0), (340.0, 220.0),
          (380.0, 260.0), (400.0, 300.0), (400.0, 360.0), (380.0, 400.0),
          (360.0, 420.0), (300.0, 440.0), (220.0, 440.0), (160.0, 420.0),
          (140.0, 400.0), (120.0, 360.0), (120.0, 300.0), (140.0, 260.0),
          (180.0, 220.0), (240.0, 200.0), (320.0, 180.0), (360.0, 160.0),
          (380.0, 120.0), (380.0, 80.0), (360.0, 40.0), (300.0, 20.0),
          (220.0, 20.0)]], 
    # 9
        [[(380.0, 160.0), (360.0, 220.0), (320.0, 260.0), (260.0, 280.0),
          (240.0, 280.0), (180.0, 260.0), (140.0, 220.0), (120.0, 160.0),
          (120.0, 140.0), (140.0, 80.0), (180.0, 40.0), (240.0, 20.0),
          (260.0, 20.0), (320.0, 40.0), (360.0, 80.0), (380.0, 160.0),
          (380.0, 260.0), (360.0, 360.0), (320.0, 420.0), (260.0, 440.0),
          (220.0, 440.0), (160.0, 420.0), (140.0, 380.0)]], 
    # -
        [[(80, 260), (440, 260)]], 
    # .
        [[(120, 400), (100, 420), (120, 440), (140, 420), (120, 400)]], 
    # X
        [[(60, 20), (340, 440)], [(340, 20), (60, 440)]], 
    # Y
        [[(40, 20), (200, 220), (200, 440)], [(360, 20), (200, 220)]], 
    # Z
        [[(340, 20), (60, 440)], [(60, 20), (340, 20)], 
         [(60, 440), (340, 440)]],
    # G
        [[(380.0, 80.0), (360.0, 40.0), (300.0, 20.0), (260.0, 20.0),
          (200.0, 40.0), (160.0, 100.0), (140.0, 200.0), (140.0, 300.0),
          (160.0, 380.0), (200.0, 420.0), (260.0, 440.0), (280.0, 440.0),
          (340.0, 420.0), (380.0, 380.0), (400.0, 320.0),
          (400.0, 280.0), (270.0, 280.0)]],
    # U
        [[(60, 20), (60, 400), (95, 410), (130, 420), (165, 430),
          (200, 440), (200, 440), (235, 430), (270, 420), (305, 410),
          (340, 400), (340, 20)]],
    # V
        [[(60, 20), (200, 440), (340, 20)]],
    # W
        [[(60, 20), (60, 400), (100, 440), (160, 440), (200, 400),
          (240, 440), (300, 440), (340, 400), (340, 20)],
         [(200, 400), (200, 300)]],
       )
        # The preview renderer draws Hershey glyphs through the line shader via
        # string_polylines(); the legacy per-glyph GL display lists (glGenLists/
        # glNewList) and the plot_digit/plot_string draw helpers that used them
        # are gone (they are removed from OpenGL core profiles).

    def string_polylines(self, s, frac=0.0, flip_y=False, flip_z=False,
                         bbox=False):
        """Return a string's strokes as polylines in the local text frame.

        GL-free equivalent of ``plot_string``'s geometry: glyphs are laid out in
        the post-(1/440)-scale frame (each ~1 unit tall) with the same advance
        widths and ``frac`` offset, and the two readability flips applied. The
        caller decides ``flip_y``/``flip_z`` from ``modelview[2][2] < -0.001`` /
        ``modelview[1][1] < -0.001`` (the diagonal terms plot_string tests), and
        supplies the outer positioning transform itself. Returns a list of
        polylines, each a list of ``(x, y)`` points.
        """
        frac_final = frac
        if flip_y:
            frac_final = 1.0 - frac_final
        if flip_z:
            frac_final = 1.0 - frac_final
        slen = self.string_len(s)

        def place(x440, y440):
            gx = x440 / 440.0 - slen * frac_final
            gy = y440 / 440.0
            if flip_z:
                gx, gy = -gx, 1.0 - gy
            if flip_y:
                gx = -gx
            return (gx, gy)

        polylines = []
        if bbox:
            # Same rectangle plot_string draws around the string (in 440-space).
            right = slen * 440.0 + 140.0
            polylines.append([place(-140.0, -140.0), place(right, -140.0),
                              place(right, 580.0), place(-140.0, 580.0),
                              place(-140.0, -140.0)])
        advance = 0.0
        for c in s:
            digit = self.hershey[translate[c]]
            for stroke in digit:
                polylines.append([place(x + advance, 440.0 - y)
                                  for x, y in stroke])
            if c == '1':
                advance += 260.0
            elif c == '.':
                advance += 180.0
            else:
                advance += 400.0
        return polylines

    def string_len(self, s):
        l = 0.0
        for c in s:
            if c == '1':
                l += 260.0
            elif c == '.':
                l += 180.0
            else:
                l += 400.0

        return l/440.0

