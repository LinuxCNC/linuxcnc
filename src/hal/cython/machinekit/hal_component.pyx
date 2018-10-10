from .hal_util cimport pypin_value

class ComponentExit(Exception):
    pass

cdef class Component(HALObject):
    cdef hal_compiled_comp_t *_cc
    cdef dict _itemdict
    cdef int _handle

    def __cinit__(self, char *name, mode=TYPE_USER, int userarg1=0, int userarg2=0,
                  wrap=False, noexit=False, lock=True):
        global _comps
        hal_required()
        self._itemdict = dict()
        with HALMutexIf(lock):
            self._cc = NULL
            if not wrap:
                self._o.comp = halg_xinitf(0, mode, userarg1, userarg2, NULL, NULL, "%s", name)
                if self._o.comp == NULL:
                    raise RuntimeError("Failed to create component '%s': - %s" %
                                       (name, hal_lasterror()))
                if not noexit:
                    _comps.append(hh_get_id(&self._o.comp.hdr))  # to exit list
            else:
                self._o.comp = halg_find_object_by_name(0, hal_const.HAL_COMPONENT,
                                                        name).comp
            if self._o.comp == NULL:
                raise RuntimeError("halpr_find_comp_by_name(%s) failed" % name)

    def newpin(self, *a, **kw):
        if self._o.comp.state != COMP_INITIALIZING:
            raise RuntimeError("component %s: cannot add pin in state %d" %
                               (self.name ,self._o.comp.state))
        p =  Pin(self, *a,**kw)
        self._itemdict[a[0]] = p
        return p

    def __getitem__(self, name):
        if name in self._itemdict:
            return self._itemdict[name].get()
        raise KeyError("component %s: nonexistent pin %s" %
                       (self.name, name))

    def __setitem__(self, name, value):
        if name in self._itemdict:
            self._itemdict[name].set(value)
        else:
            raise KeyError("component %s: nonexistent pin %s" %
                           (self.name, name))

    def pins(self, lock=True):
        ''' return a list of Pin objects owned by this component, which includes all instance pins'''
        with HALMutexIf(lock):
            # collect pin names
            pinnames = comp_owned_names(0, hal_const.HAL_PIN, hh_get_id(&self._o.comp.hdr))
            # now the wrapped objects, all under the HAL mutex held:
            pinlist = []
            for n in pinnames:
                pinlist.append(pins.__getitem_unlocked__(n))
            return pinlist

    def pin(self, name, base=None):
        ''' return component Pin object, base does not need to be supplied if pin name matches component name '''
        if base == None:
            base = self.name
        return Pin('%s.%s' % (base, name))

    def exit(self):
        if self._cc != NULL:
            hal_ccomp_free(self._cc)
        if hh_get_id(&self._o.comp.hdr) > 0:
            hal_exit(hh_get_id(&self._o.comp.hdr))

    def ready(self):
        rc = hal_ready(hh_get_id(&self._o.comp.hdr))
        if rc:
            raise RuntimeError("Failed to ready component '%s' - %d : %d - %s" %
                               (self.name,
                                hh_get_id(&self._o.comp.hdr),
                                rc, hal_lasterror()))

    def bind(self):
        rc = hal_bind(self.name)
        if rc < 0:
            raise RuntimeError("Failed to bind component '%s' - %d : %d - %s" %
                               (self.name,hh_get_id(&self._o.comp.hdr), rc, hal_lasterror()))

    def unbind(self):
        rc = hal_unbind(hh_get_name(&self._o.comp.hdr))
        if rc < 0:
           raise RuntimeError("Failed to unbind component '%s' - %d : %d - %s" %
                               (self.name,hh_get_id(&self._o.comp.hdr), rc, hal_lasterror()))

    def acquire(self, int pid):
        rc = hal_acquire(hh_get_name(&self._o.comp.hdr), pid)
        if rc < 0:
            raise RuntimeError("Failed to acquire component '%s' - %d : %d - %s" %
                               (self.name,hh_get_id(&self._o.comp.hdr), rc, hal_lasterror()))

    def release(self):
        rc = hal_release(hh_get_name(&self._o.comp.hdr))
        if rc < 0:
            raise RuntimeError("Failed to release component '%s' - %d : %d - %s" %
                               (self.name,hh_get_id(&self._o.comp.hdr), rc, hal_lasterror()))

    property pid:
        def __get__(self): return self._o.comp.pid

    property type:
        def __get__(self): return self._o.comp.type

    property state:
        def __get__(self): return self._o.comp.state

    property has_ctor:
        def __get__(self): return self._o.comp.ctor != NULL

    property instantiable: # same as has_ctor
        def __get__(self): return self._o.comp.ctor != NULL

    property has_dtor:
        def __get__(self): return self._o.comp.dtor != NULL

    property last_update:
        def __get__(self): return self._o.comp.last_update
        def __set__(self,int value):  self._o.comp.last_update = value

    property last_bound:
        def __get__(self): return self._o.comp.last_bound
        def __set__(self,int value):  self._o.comp.last_bound = value

    property last_unbound:
        def __get__(self): return self._o.comp.last_unbound
        def __set__(self,int value):  self._o.comp.last_unbound = value

    property userarg1:
        def __get__(self): return self._o.comp.userarg1
        def __set__(self, int value): self._o.comp.userarg1 = value

    property userarg2:
        def __get__(self): return self._o.comp.userarg2
        def __set__(self, int value): self._o.comp.userarg2 = value

    def changed(self,  userdata=None, report_all=False):
        if self._cc == NULL:
            rc = halg_compile_comp(1, self.name, &self._cc)
            if rc < 0:
                raise RuntimeError("Failed to compile component '%s' - %d : %d - %s" %
                                   (self.name, self.oid,
                                    rc, hal_lasterror()))
        nchanged = hal_ccomp_match(self._cc)
        if nchanged < 0:
                raise RuntimeError("hal_ccomp_match failed '%s' - %d : %d - %s" %
                                   (self.name, self.oid,
                                    rc, hal_lasterror()))
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
cdef int comp_callback(const int phase,
                       const hal_compiled_comp_t * cc,
                       hal_pin_t *p,
                       const hal_data_u *value,
                       void *userdata):
    arg =  <object>userdata
    if callable(arg):
        if phase == REPORT_BEGIN:
            (arg)(phase, None, None)
        elif phase == REPORT_PIN:
            (arg)(phase, hh_get_name(&p.hdr),pypin_value(p))
        elif phase == REPORT_END:
            (arg)(phase, None, None)
        else:
            raise RuntimeError("invalid phase %d" % phase)
        return 0

    if  isinstance(arg, list):
        if phase == REPORT_PIN:
            arg.append((hh_get_name(&p.hdr),pypin_value(p)))
        elif phase == REPORT_BEGIN:
            del arg[0:len(arg)]  # clear result list
        return 0

    if arg is None:
        return 0
    return -1

_wrapdict[hal_const.HAL_COMPONENT] = Component
components = HALObjectDict(hal_const.HAL_COMPONENT)
