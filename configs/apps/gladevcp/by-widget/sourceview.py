
import gtk
import gtksourceview2 as gtksourceview

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
        print "file_set",filename
        self.textbuffer.set_text(open(filename).read())
        self.line = 1
        self._set_line(self.line)    

    def on_down(self,widget,data=None):
        self.line += 1
        self._set_line(self.line)

        
    def on_up(self,widget,data=None):
        self.line -= 1
        self._set_line(self.line)
   
    def __init__(self, halcomp,builder,useropts,compname):
        self.halcomp = halcomp
        self.builder = builder

        self.line = 1
        self.sourceview = builder.get_object('gtksourceview1')
        self.textbuffer = gtksourceview.Buffer()
        self.sourceview.set_buffer(self.textbuffer)

        self.sourceview.set_show_line_numbers(True)
        self.sourceview.set_show_line_marks(True)
        self.sourceview.set_highlight_current_line(True)
        self.sourceview.set_mark_category_icon_from_icon_name('highlight', 'gtk-forward')
        self.sourceview.set_mark_category_background('highlight', gtk.gdk.Color('yellow'))
        self.mark = None

        
def get_handlers(halcomp,builder,useropts,compname):

    return [HandlerClass(halcomp,builder,useropts,compname)]
