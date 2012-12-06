import hal
import glib
import pango

import struct
import string
import sys,os

debug = 0
font = 'courier bold 12'

def nullstrip(s):
    try:
        s = s[:s.index('\x00')]
    except ValueError:
        pass
    return s

class DebugText:
    # decode structure of PASM .dbg output

    # label record size = 68
    def decode_labels(self):
        for i in range(self.label_count):
            off = self.label_offset + i *68
            (loffset,lname) = struct.unpack("@I64s",self.data[off:off+68])
            lname.rstrip('\0')
            if self.debug: print "label: ", loffset, lname
            self.labelsbyname[lname] = loffset
            self.labelsbyaddr[loffset] = lname

    # filename record size = 64
    def decode_filenames(self):
        for i in range(self.file_count):
            off = self.file_offset + i *64
            (f,) = struct.unpack("64s",self.data[off:off+64])
            fname = nullstrip(f)
            if self.debug: print "fname: ", i, fname
            self.filenames[i] = fname
            self.files[i] = open(fname, "r").read().splitlines()

    # code record size = 16
    def decode_code(self):
        for i in range(self.code_count):
            off = self.code_offset + i *16
            (f, resv8,fi,l,offset,word) = struct.unpack("@bbHIII",self.data[off:off+16])
            if self.debug: print "code: ", f, fi,l,offset,word
            self.code [offset ] = word
            self.line [offset ] = l
            self.fileindex [offset ] = fi
            self.cflags [offset] = f

    def gen_text(self):
        self.text = ""
        for adr in sorted(self.code.iterkeys()):
            l = ""
            l = format("%-3.3s: %8.8x %3.3d " % (adr, self.code[adr],self.line[adr]))
            src = self.files[self.fileindex[adr]]
            if self.labelsbyaddr.has_key(adr):
                l += self.labelsbyaddr[adr]
                l += ":"
            l += src[self.line[adr]-1]
            self.text += l
            self.text += "\n"
        return self.text

    def __init__(self, filename,debug=False):
        self.debug = debug

        # labels by labelname, returns code address
        self.labelsbyname = dict()
        # labels by address, returns label name
        self.labelsbyaddr = dict()
        # other stuff by address
        self.code = dict()
        self.fileindex = dict()
        self.line = dict()
        self.cflags = dict()
        # lists of lines, indexed by fileindex
        self.files = dict()
        # lists of files, indexed by fileindex
        self.filenames = dict()

        self.data = open(filename, "r").read()
        (self.file_id,self.label_count,
         self.label_offset,self.file_count,self.file_offset,self.code_count,
            self.code_offset,self.entrypoint,selfflags) = struct.unpack("@IIIIIIIII", self.data[0:36])
        if self.file_id !=  (0x10150000 | 0x03): # see pasm source
            sys.stderr.write("file id doesnt match: %x/%x", fid, file_id)

        self.decode_labels()
        self.decode_filenames()
        self.decode_code()

class HandlerClass:

    def file_set(self,widget,data=None):
        print "file_set"
        filename = widget.get_filename()

        self.dbtxt = DebugText(filename,True)
        data = self.dbtxt.gen_text()
        txt = data.replace('\x00', '').decode('utf-8', 'replace').encode('utf-8')
        self.sourcetextbuffer.set_text(txt)

        self.have_file = True
        self.iter1 = self.sourcetextbuffer.get_iter_at_line(1)
        self.iter2 = self.sourcetextbuffer.get_iter_at_line(2)

    def pc_changed(self,widget,data=None):
        print "pc_changed"
        line = widget.hal_pin.get()
        print "pc_changed: ",line

        if not self.have_file:
            return
        # this doesnt work reliably yet - please fix
        self.sourcetextbuffer.apply_tag_by_name("default", self.iter1, self.iter2)
        self.iter1 = self.sourcetextbuffer.get_iter_at_line(line)
        self.iter2 = self.sourcetextbuffer.get_iter_at_line(line+1)
        self.sourcetextbuffer.apply_tag_by_name("highlighted", self.iter1, self.iter2)
        self.sourcetextbuffer.place_cursor(self.iter1)

    def __init__(self, halcomp,builder,useropts):

        self.halcomp = halcomp
        self.builder = builder
        self.sourcetext = builder.get_object('sourcetext')
        self.sourcetextbuffer = self.sourcetext.get_buffer()
        self.sourcetext.modify_font(pango.FontDescription(font))
        for i in range(32):
            builder.get_object(format('R%d' % (i))).modify_font(pango.FontDescription(font))
        builder.get_object("CONTROL").modify_font(pango.FontDescription(font))
        builder.get_object("PC").modify_font(pango.FontDescription(font))

        self.have_file = False
        self.sourcetextbuffer.create_tag("highlighted",  background = "yellow")
        self.sourcetextbuffer.create_tag("default", background = "white")

        self.chooser = builder.get_object('chooser')
        self.chooser.set_current_folder(os.getcwd())

def get_handlers(halcomp,builder,useropts):

    return [HandlerClass(halcomp,builder,useropts)]
