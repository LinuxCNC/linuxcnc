#    Copyright 2007 Jeff Epler <jepler@unpythonic.net>
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
#
# Info about Xilinx bitfiles:
#
# The format consists of several variable length chunks, preceded by what
# seems to be a constant header (a magic number).
#
# After the header, each "chunk" consists of a one byte "tag", a two or
# four byte length, and "length" bytes of data (the body).
#
# In some chunks, the body is a zero terminated printable string.  In
# others it is a blob of binary data.  The file format doesn't care.
#
# Standard Xilinx files use 5 chunks: 'a' through 'd' are zero-terminated
# strings with information about the file.  'e' is a large binary blob
# with the actual bitstream in it.  Xilinx uses 2 byte lengths for chunks
# 'a' thru 'd', and 4 bytes for chunk 'e'.  This library allows other
# chunks, and assume that all have 4 byte lengths, except 'a' thru 'd'.
#
import struct

class BitFile:
    MAXCHUNKS = 50
    SMALLCHUNKS = "abcd"
    MAGIC = "\x00\x09" \
	    "\x0f\xf0\x0f\xf0" \
	    "\x0f\xf0\x0f\xf0" \
	    "\x00\x00\x01"
    ORDER = "abcde"

    def __init__(self, chunks={}):
	self.chunks = dict(chunks)

    def __getitem__(self, item):
	return self.chunks[item]
    def __setitem__(self, item, value):
	self.chunks[item] = value
    def __delitem__(self, item):
	del self.chunks[item]

    def chunkorder(self, (tag, value)):
	if tag in self.ORDER:
	    return self.ORDER.index(tag)
	return 256 + ord(tag)

    @classmethod
    def fromstring(cls, data):
	if not data.startswith(cls.MAGIC):
	    raise ValueError, "data does not start with the magic number"
	i = len(cls.MAGIC)
	chunks = {}
	while i < len(data):
	    tag = data[i]
	    if tag in cls.SMALLCHUNKS:
		chunksize = struct.unpack(">H", data[i+1:i+3])[0]
		i = i + 3
	    else:
		chunksize = struct.unpack(">I", data[i+1:i+5])[0]
		i = i + 5
	    chunkdata = data[i:i+chunksize]
	    if tag in chunks:
		raise ValueError, "bitfile has chunk %r more than once" % tag
	    chunks[tag] = chunkdata
	    i = i + chunksize
	return cls(chunks)

    @classmethod
    def fromfile(cls, file):
	return cls.fromstring(file.read())

    @classmethod
    def fromfilename(cls, filename):
	return cls.fromstring(open(filename, "rb").read())

    def tostring(self):
	result = self.MAGIC
	for tag, chunkdata in sorted(self.chunks.items(), key=self.chunkorder):
	    if len(tag) != 1:
		raise ValueError, "Tag %r must be a length-1 string" % tag
	    result = result + tag
	    if tag in self.SMALLCHUNKS:
		result += struct.pack(">H", len(chunkdata))
	    else:
		result += struct.pack(">I", len(chunkdata))
	    result += chunkdata

	return result
    
    def tofile(self, file):
	return file.write(self.tostring())

    def tofilename(self, filename):
	return open(filename, "wb").write(self.tostring())

if __name__ == '__main__':
    import sys

    if len(sys.argv) < 2:
	c = BitFile()
	c['a'] = "hello world"
	c['e'] = "goodbye"
	c['A'] = "shazam"
	s = c.tostring()
	print repr(s)
	d = BitFile.fromstring(s)
	print d.chunks
	assert d.tostring() == s

    for bitfile in sys.argv[1:]:
	bits = open(bitfile, "rb").read()
	c = BitFile.fromstring(bits)
	for k, v in sorted(c.chunks.items()):
	    if k in 'abcd':
		print k, v
	    else:
		print k, len(v)
	newbits = c.tostring()
	assert bits == newbits  # Assuming the original is in canonical order!
	print
