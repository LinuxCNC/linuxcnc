from .hal_util cimport pin_value

class ComponentExit(Exception):
    pass

cdef class Component:
    cdef hal_compiled_comp_t *_cc
    cdef hal_comp_t *_comp
    cdef dict _itemdict
    cdef int _handle

    def __cinit__(self, name, mode=TYPE_USER, userarg1=0, int userarg2=0,
                  wrap=False, noexit=False):
        global _comps
        self._itemdict = dict()
        if not wrap:
            if name in components:
                raise RuntimeError("component with name '%s' already exists" % name)
            id = hal_xinit(mode, userarg1, userarg2, NULL, NULL, name)
            if id < 0:
                raise RuntimeError("Failed to create component '%s': %d - %s" % (name,id, hal_lasterror()))
            if not noexit:
                _comps.append(id)  # to exit list

        self._cc = NULL

        with HALMutex():
            self._comp = halpr_find_comp_by_name(name)

        if self._comp == NULL:
            raise RuntimeError("halpr_find_comp_by_name(%s) failed" % name)

    def newpin(self, *a, **kw):
        if self._comp.state != COMP_INITIALIZING:
            raise RuntimeError("component %s: cannot add pin in state %d" %
                               (self._comp.name,self._comp.state))
        p =  Pin(self, *a,**kw)
        self._itemdict[a[0]] = p
        return p

    def __getitem__(self, name):
        if name in self._itemdict:
            return self._itemdict[name].get()
        raise KeyError("component %s: nonexistent pin %s" % (self._comp.name, name))

    def __setitem__(self, name, value):
        if name in self._itemdict:
            self._itemdict[name].set(value)
        else:
            raise KeyError("component %s: nonexistent pin %s" % (self._comp.name, name))

    def pins(self):
        ''' return list of Pin objects owned by this component, which includes all instance pins'''
        cdef hal_pin_t *p
        p = NULL

        pinnames = []
        with HALMutex():
            p = halpr_find_pin_by_owner_id(self._comp.comp_id, p)
            while p != NULL:
                pinnames.append(p.name)
                p = halpr_find_pin_by_owner_id(self._comp.comp_id, p)

        pinlist = []
        for n in pinnames:
            pinlist.append(pins[n])
        return pinlist

    def pin(self, name, base=None):
        ''' return component Pin object, base does not need to be supplied if pin name matches component name '''
        if base == None:
            base = self.name
        return Pin('%s.%s' % (base, name))

    def exit(self):
        if self._cc != NULL:
            hal_ccomp_free(self._cc)
        if self._comp.comp_id > 0:
            hal_exit(self._comp.comp_id)
        # not sure this is a good idea:
        #raise ComponentExit("component %s exited" % self._comp.name)

    def ready(self):
        rc = hal_ready(self._comp.comp_id)
        if rc:
            raise RuntimeError("Failed to ready component '%s' - %d : %d - %s" %
                               (self._comp.name,self._comp.comp_id, rc, hal_lasterror()))

    def bind(self):
        rc = hal_bind(self._comp.name)
        if rc < 0:
            raise RuntimeError("Failed to bind component '%s' - %d : %d - %s" %
                               (self._comp.name,self._comp.comp_id, rc, hal_lasterror()))

    def unbind(self):
        rc = hal_unbind(self._comp.name)
        if rc < 0:
           raise RuntimeError("Failed to unbind component '%s' - %d : %d - %s" %
                               (self._comp.name,self._comp.comp_id, rc, hal_lasterror()))

    def acquire(self, int pid):
        rc = hal_acquire(self._comp.name, pid)
        if rc < 0:
            raise RuntimeError("Failed to acquire component '%s' - %d : %d - %s" %
                               (self._comp.name,self._comp.comp_id, rc, hal_lasterror()))

    def release(self):
        rc = hal_release(self._comp.name)
        if rc < 0:
            raise RuntimeError("Failed to release component '%s' - %d : %d - %s" %
                               (self._comp.name,self._comp.comp_id, rc, hal_lasterror()))

    property name:
        def __get__(self): return self._comp.name

    property type:
        def __get__(self): return self._comp.type

    property id:
        def __get__(self): return self._comp.comp_id

    property pid:
        def __get__(self): return self._comp.pid

    property state:
        def __get__(self): return self._comp.state

    property has_ctor:
        def __get__(self): return self._comp.ctor != NULL

    property instantiable: # same as has_ctor
        def __get__(self): return self._comp.ctor != NULL

    property has_dtor:
        def __get__(self): return self._comp.dtor != NULL

    property last_update:
        def __get__(self): return self._comp.last_update
        def __set__(self,int value):  self._comp.last_update = value

    property last_bound:
        def __get__(self): return self._comp.last_bound
        def __set__(self,int value):  self._comp.last_bound = value

    property last_unbound:
        def __get__(self): return self._comp.last_unbound
        def __set__(self,int value):  self._comp.last_unbound = value

    property userarg1:
        def __get__(self): return self._comp.userarg1
        def __set__(self, int value): self._comp.userarg1 = value

    property userarg2:
        def __get__(self): return self._comp.userarg2
        def __set__(self, int value): self._comp.userarg2 = value

    def changed(self,  userdata=None, report_all=False):
        if self._cc == NULL:
            rc = hal_compile_comp(self._comp.name, &self._cc)
            if rc < 0:
                raise RuntimeError("Failed to compile component '%s' - %d : %d - %s" %
                                   (self._comp.name,self._comp.comp_id, rc, hal_lasterror()))
        nchanged = hal_ccomp_match(self._cc)
        if nchanged < 0:
                raise RuntimeError("hal_ccomp_match failed '%s' - %d : %d - %s" %
                                   (self._comp.name,self._comp.comp_id, rc, hal_lasterror()))
        if nchanged == 0 and not report_all:
            return 0
        rc = hal_ccomp_report(self._cc, comp_callback, <void *>userdata, int(report_all))
        if rc:
            raise RuntimeError("component report: invalid userdata - must be callable or list: %d" % rc)
        return nchanged

# userdata may be:
# NULL (no argument)
# a callable
# a list
# fail changed() if not one of the above
cdef int comp_callback(int phase, hal_compiled_comp_t * cc, hal_pin_t *p,hal_data_u *value,void *userdata):
    arg =  <object>userdata
    if callable(arg):
        if phase == REPORT_BEGIN:
            (arg)(phase, None, None)
        elif phase == REPORT_PIN:
            (arg)(phase, p.name,pin_value(p))
        elif phase == REPORT_END:
            (arg)(phase, None, None)
        else:
            raise RuntimeError("invalid phase %d" % phase)
        return 0

    if  isinstance(arg, list):
        if phase == REPORT_PIN:
            arg.append((p.name,pin_value(p)))
        elif phase == REPORT_BEGIN:
            del arg[0:len(arg)]  # clear result list
        return 0

    if arg is None:
        return 0
    return -1
