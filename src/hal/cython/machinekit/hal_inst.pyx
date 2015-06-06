

cdef class Instance:
    cdef hal_inst_t *_inst
    cdef hal_comp_t *_comp

    def __cinit__(self, name):
        hal_required()
        with HALMutex():
            self._inst = halpr_find_inst_by_name(name)
            if self._inst == NULL:
                raise RuntimeError("instance %s does not exist" % name)

    # def delete(self):
    #     r = hal_inst_delete(self._inst.name)
    #     if (r < 0):
    #         raise RuntimeError("Fail to delete instance %s: %s" % (self._name, hal_lasterror()))

    def owner(self):

        with HALMutex():
            self._comp = halpr_find_owning_comp(self._inst.comp_id)

        if self._comp == NULL:
            raise RuntimeError("BUG: Failed to find owning comp %d of instance %s: %s" %
                               (self._inst.comp_id, self._inst.name, hal_lasterror()))
        return Component(self._comp.name, wrap=True)

    property size:
        def __get__(self): return self._inst.inst_size

    property name:
        def __get__(self): return self._inst.name

    property id:
        def __get__(self): return self._inst.inst_id

    property comp_id:
        def __get__(self): return self._inst.comp_id

    def pins(self):
        ''' return a list of Pin objects owned by this instance'''
        cdef hal_pin_t *p
        p = NULL

        pinnames = []
        with HALMutex():
            p = halpr_find_pin_by_owner_id(self._inst.inst_id, p)
            while p != NULL:
                pinnames.append(p.name)
                p = halpr_find_pin_by_owner_id(self._inst.inst_id, p)

        pinlist = []
        for n in pinnames:
            pinlist.append(pins[n])
        return pinlist

    def pin(self, name, base=None):
        ''' return component Pin object, base does not need to be supplied if pin name matches component name '''
        if base == None:
            base = self.name
        return Pin('%s.%s' % (base, name))


    # TODO: add a bufferview of the shm area
