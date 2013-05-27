# decode structure of PASM .dbg output

import struct
import string
import sys,os

debug = 0


# labels by labelname, returns code address
labelsbyname = dict()
# labels by address, returns label name
labelsbyaddr = dict()
# other stuff by address
code = dict()
fileindex = dict()
line = dict()
cflags = dict()
# lists of lines, indexed by fileindex
files = dict()
# lists of files, indexed by fileindex
filenames = dict()


def nullstrip(s):
    """Return a string truncated at the first null character."""
    try:
        s = s[:s.index('\x00')]
    except ValueError:  # No nulls were found, which is okay.
        pass
    return s

# label record size = 68
def decode_labels():
    for i in range(label_count):
        off = label_offset + i *68
        (loffset,lname) = struct.unpack("@I64s",data[off:off+68])
        lname.rstrip('\0')
        if debug: print "label: ", loffset, lname
        labelsbyname[lname] = loffset
        labelsbyaddr[loffset] = lname

# filename record size = 64
def decode_filenames():
    for i in range(file_count):
        off = file_offset + i *64
        (f,) = struct.unpack("64s",data[off:off+64])
        fname = nullstrip(f)
        if debug: print "fname: ", i, fname
        filenames[i] = fname
        files[i] = open(fname, "r").read().splitlines()

# code record size = 16
def decode_code():
    for i in range(code_count):
        off = code_offset + i *16
        (f, resv8,fi,l,offset,word) = struct.unpack("@bbHIII",data[off:off+16])
        if debug: print "code: ", f, fi,l,offset,word
        code [offset ] = word
        line [offset ] = l
        fileindex [offset ] = fi
        cflags [offset] = f



if len(sys.argv) < 2:
    sys.stderr.write('Usage: sys.argv[0] ')
    sys.exit(1)

if not os.path.exists(sys.argv[1]):
    sys.stderr.write('ERROR: pasm .dbg file not found')
    sys.exit(1)

fileName = sys.argv[1]

fid = 0x10150000 | 0x03
data = open(fileName, "r").read()
if debug: print len(data),data
(file_id,label_count, label_offset,file_count,file_offset,code_count,code_offset,entrypoint,flags) = struct.unpack("@IIIIIIIII", data[0:36])

if file_id != fid:
    sys.stderr.write("file id doesnt match: %x/%x", fid, file_id

if debug: print file_id,label_count, label_offset,file_count,file_offset,code_count,code_offset,entrypoint,flags

decode_labels()
decode_filenames()
decode_code()

print "entry point: %8.8x" % (entrypoint),
if labelsbyaddr.has_key(entrypoint):
    print labelsbyaddr[entrypoint],
print ", %d labels from %d files, %d code records" % (label_count,file_count,code_count)
print "source files: ",
for i in sorted(filenames.iterkeys()): print "\"%s\" " % (filenames[i]),
print

for adr in sorted(code.iterkeys()):
    print "%-3.3s: %8.8x" % (adr, code[adr]),
    lines = files[fileindex[adr]]
    print line[adr],
    if labelsbyaddr.has_key(adr):
        print labelsbyaddr[adr],":",
    print lines[line[adr]-1]
