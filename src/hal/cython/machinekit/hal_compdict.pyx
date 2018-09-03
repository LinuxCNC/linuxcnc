# Components pseudo dictionary

# hal_iter callback: add comp names into list
cdef int _collect_comp_names(hal_comp_t *comp,  void *userdata):
    arg =  <object>userdata
    if  isinstance(arg, list):
        arg.append(comp.name)
        return 0
    else:
        return -1

cdef list comp_names():
    names = []

    with HALMutex():
        rc = halpr_foreach_comp(NULL,  _collect_comp_names, <void *>names);
        if rc < 0:
            raise RuntimeError("comp_names: halpr_foreach_comp failed %d: %s" %
                               (rc, hal_lasterror()))
    return names

cdef int comp_count():
    with HALMutex():
        rc = halpr_foreach_comp(NULL, NULL, NULL);
        if rc < 0:
            raise RuntimeError("comp_count: halpr_foreach_comp failed %d: %s" %
                               (rc, hal_lasterror()))
    return rc


cdef class Components:
    cdef dict comps

    def __cinit__(self):
        self.comps = dict()

    def __getitem__(self, char *name):
        hal_required()

        if isinstance(name, int):
            return comp_names()[name]

        if name in self.comps:
            return self.comps[name]
        cdef hal_comp_t *comp

        with HALMutex():
            comp = halpr_find_comp_by_name(name)

        if comp == NULL:
            raise NameError, name

        c = Component(name, wrap=True)
        self.comps[name] = c
        return c

    def __contains__(self, arg):
        if isinstance(arg, Component):
            arg = arg.name
        try:
            self.__getitem__(arg)
            return True
        except NameError:
            return False

    def __len__(self):
        hal_required()
        rc = comp_count()
        if rc < 0:
            raise RuntimeError("halpr_foreach_comp failed %d" % rc)
        return rc

    def __call__(self):
        hal_required()
        return comp_names()

    def __repr__(self):
        hal_required()
        compdict = {}
        for name in comp_names():
            compdict[name] = self[name]
        return str(compdict)


components = Components()
