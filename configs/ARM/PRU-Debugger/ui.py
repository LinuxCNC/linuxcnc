import hal
import glib
import pango

import struct
import string
import sys,os
import gtk
import gtksourceview2 as gtksourceview


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
            if debug: print "label: ", loffset, lname
            self.labelsbyname[lname] = loffset
            self.labelsbyaddr[loffset] = lname

    # filename record size = 64
    def decode_filenames(self):
        for i in range(self.file_count):
            off = self.file_offset + i *64
            (f,) = struct.unpack("64s",self.data[off:off+64])
            fname = nullstrip(f)
            if debug: print "fname: ", i, fname
            self.filenames[i] = fname
            self.files[i] = open(fname, "r").read().splitlines()

    # code record size = 16
    def decode_code(self):
        for i in range(self.code_count):
            off = self.code_offset + i *16
            (f, resv8,fi,l,offset,word) = struct.unpack("@bbHIII",self.data[off:off+16])
            if debug: print "code: ", f, fi,l,offset,word
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

    def __init__(self, filename):

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

    def populate_popup(self,textview,menu,data=None):
        separator = gtk.SeparatorMenuItem()
        sbp_item = gtk.MenuItem("Set breakpoint")
        sbp_item.connect("activate", self.set_breakpoint,"foo")
        rbp_item = gtk.MenuItem("Remove breakpoint")
        rbp_item.connect("activate", self.remove_breakpoint,"foo")
        menu.append(separator)
        separator.show()
        menu.append(sbp_item)
        sbp_item.show()
        menu.append(rbp_item)
        rbp_item.show()

    def set_breakpoint(self,widget,data=None):
        if self.bpline:
            self._set_bp_line(None)
        self.bpline = self.textbuffer.get_iter_at_mark(self.textbuffer.get_insert()).get_line() +1
        print "not implemented yet: setting breakpoint at line=",self.bpline
        self._set_bp_line(self.bpline)

    def remove_breakpoint(self,widget,data=None):
        if self.bpline:
            self._set_bp_line(None)
        self.bpline = None

    def _set_bp_line(self,l):
        if not l:
            if self.bpmark:
                self.textbuffer.delete_mark(self.bpmark)
                self.bpmark = None
            return
        line = self.textbuffer.get_iter_at_line(l-1)
        if not self.bpmark:
            self.bpmark = self.textbuffer.create_source_mark('breakpoint', 'breakpoint', line)
            self.bpmark.set_visible(True)
        else:
            self.textbuffer.move_mark(self.bpmark, line)
        #self.sourceview.scroll_to_mark(self.bpmark, 0, True, 0, 0.5)


    def _set_line(self,l):
        if not l:
            if self.mark:
                self.textbuffer.delete_mark(self.mark)
                self.mark = None
            return
        line = self.textbuffer.get_iter_at_line(l-1)
        if not self.mark:
            self.mark = self.textbuffer.create_source_mark('highlight', 'highlight', line)
            self.mark.set_visible(True)
        else:
            self.textbuffer.move_mark(self.mark, line)
        self.sourceview.scroll_to_mark(self.mark, 0, True, 0, 0.5)

    def load_file(self, filename):
        self.dbtxt = DebugText(filename)
        data = self.dbtxt.gen_text()
        # the reader routines leave some fluff in the strings, so clean up
        txt = data.replace('\x00', '').decode('utf-8', 'replace').encode('utf-8')
        self.textbuffer.set_text(txt)
        self._set_line(0)
        self.have_file = True


    def file_set(self,widget,data=None):
        filename = widget.get_filename()
        if debug: print "file_set",filename
        self.load_file(filename)

    def pc_changed(self,widget,data=None):
        line = widget.hal_pin.get()
        if debug: print "pc_changed: ",line

        if not self.have_file:
            return
        self._set_line(line+1)

    def __init__(self, halcomp,builder,useropts,compname):

        self.halcomp = halcomp
        self.builder = builder

        self.line = 1
        self.sourceview = builder.get_object('sourceview')
        self.textbuffer = gtksourceview.Buffer()
        self.sourceview.set_buffer(self.textbuffer)

        self.sourceview.set_editable(False)
        self.sourceview.set_show_line_numbers(False)
        self.sourceview.set_show_line_marks(True)
        self.sourceview.set_highlight_current_line(True)
        self.sourceview.modify_font(pango.FontDescription(font))

        self.sourceview.set_mark_category_background('highlight', gtk.gdk.Color('yellow'))
        self.sourceview.set_mark_category_background('breakpoint', gtk.gdk.Color('green'))
        self.mark = None
        self.bpmark = None
        self.bpline = None

        for i in range(32):
            builder.get_object(format('R%d' % (i))).modify_font(pango.FontDescription(font))
        builder.get_object("CONTROL").modify_font(pango.FontDescription(font))
        builder.get_object("PC").modify_font(pango.FontDescription(font))

        for cmd in useropts:
            exec cmd in globals()
   
        self.chooser = builder.get_object('chooser')
        self.chooser.set_current_folder(os.getcwd())

        if 'filename' in globals():
            self.load_file(filename)
            self.have_file = True
        else:
            self.have_file = False


def get_handlers(halcomp,builder,useropts,compname):

    return [HandlerClass(halcomp,builder,useropts,compname)]
