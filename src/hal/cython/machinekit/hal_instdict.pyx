# Instances pseudo dictionary

# hal_iter callback: add inst names into list
cdef int _collect_inst_names(hal_inst_t *inst,  void *userdata):
    arg =  <object>userdata
    if  isinstance(arg, list):
        arg.append(inst.name)
        return 0
    else:
        return -1

cdef list inst_names():
    names = []
    with HALMutex():
        rc = halpr_foreach_inst(NULL,  _collect_inst_names, <void *>names);
        if rc < 0:
            raise RuntimeError("inst_names: halpr_foreach_inst failed %d: %s" % (rc,hal_lasterror()))
    return names

cdef int inst_count():
    with HALMutex():
        rc = halpr_foreach_inst(NULL, NULL, NULL);
        if rc < 0:
            raise RuntimeError("inst_count: halpr_foreach_inst failed %d: %s" % (rc,hal_lasterror()))
    return rc


cdef class Instances:
    cdef dict insts

    def __cinit__(self):
        self.insts = dict()

    def __getitem__(self, char *name):
        hal_required()

        if isinstance(name, int):
            return inst_names()[name]

        if name in self.insts:
            return self.insts[name]
        cdef hal_inst_t *p
        with HALMutex():
            p = halpr_find_inst_by_name(name)
        if p == NULL:
            raise NameError, "no such inst: %s" % (name)
        inst =  Instance(name)
        self.insts[name] = inst
        return inst

    def __contains__(self, arg):
        if isinstance(arg, Instance):
            arg = arg.name
        try:
            self.__getitem__(arg)
            return True
        except NameError:
            return False

    def __len__(self):
        hal_required()
        return inst_count()

    def __call__(self):
        hal_required()
        return inst_names()

    def __repr__(self):
        hal_required()
        instdict = {}
        for name in inst_names():
            instdict[name] = self[name]
        return str(instdict)

instances = Instances()
