import gi
gi.require_version('Gtk', '3.0')
from gi.repository import Gtk
from gi.repository import Gdk
from gi.repository import GtkSource

class HandlerClass:

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
        
    def file_set(self,widget,data=None):
        filename = widget.get_filename()
        print("file_set",filename)
        self.textbuffer.set_text(open(filename).read())
        self.line = 1
        self._set_line(self.line)

    def on_down(self,widget,data=None):
        self.line += 1
        self._set_line(self.line)

    def on_up(self,widget,data=None):
        self.line -= 1
        self._set_line(self.line)

    def __init__(self, halcomp,builder,useropts):
        self.halcomp = halcomp
        self.builder = builder

        self.line = 1
        self.sourceview = builder.get_object('gtksourceview1')
        self.textbuffer = GtkSource.Buffer()
        self.sourceview.set_buffer(self.textbuffer)

        self.sourceview.set_show_line_numbers(True)
        self.sourceview.set_show_line_marks(True)
        self.sourceview.set_highlight_current_line(True)
        
        att = GtkSource.MarkAttributes()
        color = Gdk.RGBA()
        color.parse('yellow')
        att.set_background(color)
        self.sourceview.set_mark_attributes('highlight', att, 1)
        self.mark = None

def get_handlers(halcomp,builder,useropts):

    return [HandlerClass(halcomp,builder,useropts)]
