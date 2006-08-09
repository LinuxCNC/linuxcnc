# Copyright 2005 Jeff Epler
# All Rights Reserved
# 
# Permission to use, copy, modify, and distribute this software and its
# documentation for any purpose and without fee is hereby granted, provided that
# the above copyright notice appear in all copies and that both that copyright
# notice and this permission notice appear in supporting documentation, 
# 
# JEFF EPLER DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE, INCLUDING ALL
# IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN NO EVENT SHALL JEFF EPLER
# BE LIABLE FOR ANY SPECIAL, INDIRECT OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
# WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF
# CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION
# WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.

import struct, sys, Image

icon = []

for f in sys.argv[1:]:
    img = Image.open(f).convert("RGBA")
    icon.append(struct.pack("II", *img.size))
    icon.append(img.tostring())

icon = "".join(icon)

icon = icon.encode("hex")
print "icon = ("
for i in range(0, len(icon), 72):
	print repr(icon[i:i+72])
print ").decode('hex')"
