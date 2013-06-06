# a tour of accessing interpreter internals

def call_stack(self,*args):
    print "------- interpreter call stack: "
    for i in range(self.call_level):
        s = self.sub_context[i]
        print "%d: position=%d sequence_number=%d filename=%s subname=%s context_status=%x" % (i, s.position, s.sequence_number,s.filename,s.subname,s.context_status),
        print "named_params=",s.named_params

def remap_stack(self, *args):
    print "------- interpreter remap stack: "
    for i in range(self.remap_level):
        r = self.blocks[i].executing_remap
        print "%d: name=%s argspec=%s prolog_func=%s ngc=%s py=%s epilog=%s modal_group=%d" % (r.name,r.argspec,r.prolog_func,r.ngc,r.epilog_func,r.modal_group)

def tooltable(self, *args):
    print "------- tool table:"
    for i in range(len(self.tool_table)):
        t = self.tool_table[i]
        if t.toolno != -1: print str(t)
    print "tool in spindle=%d pocketPrepped=%d" % (self.current_tool,self.selected_pocket)


def show_block(self,*args):
    if len(args) > 0:
        n = int(args[0])
    else:
        n = 0
    b = self.blocks[n]
    print "-- blocks[%d]" % (n)
    print "line_number=%d o_name=%s p_flag=%d p_number%g q_flag=%d q_number=%g comment=%s" % (b.line_number,b.o_name,b.p_flag,b.p_number,b.q_flag,b.q_number,b.comment)


    
def show(self,*args):
    print "dir(interpreter)=",dir(self)
    tooltable(self)
    show_block(self,0)
    if self.remap_level: show_block(self,self.remap_level)
    call_stack(self)
    remap_stack(self)
    print "active G codes:",self.active_g_codes
    print "active M codes:",self.active_m_codes
    print "active settings:",self.active_settings
    print "parameters:",self.parameters

