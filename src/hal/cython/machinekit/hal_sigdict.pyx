# Signals pseudo dictionary

# hal_iter callback: add signal names into list
cdef int _collect_sig_names(hal_sig_t *sig,  void *userdata):
    arg =  <object>userdata
    if  isinstance(arg, list):
        arg.append(sig.name)
        return 0
    else:
        return -1

cdef list sig_names():
    names = []
    with HALMutex():
        rc = halpr_foreach_sig(NULL, _collect_sig_names, <void *>names);
        if rc < 0:
            raise RuntimeError("halpr_foreach_sig failed %d" % rc)
    return names

cdef int sig_count():
    with HALMutex():
        rc = halpr_foreach_sig(NULL, NULL, NULL);
    return rc

cdef class Signals:
    cdef dict sigs

    def __cinit__(self):
        self.sigs = dict()

    def __getitem__(self, char *name):
        hal_required()

        if isinstance(name, int):
            return pin_names()[name]

        if name in self.sigs:
            return self.sigs[name]

        cdef hal_sig_t *s

        with HALMutex():
            s = halpr_find_sig_by_name(name)
            if s == NULL:
                raise NameError, name

        sig =  Signal(name)
        self.sigs[name] = sig
        return sig

    def __contains__(self, arg):
        if isinstance(arg, Signal):
            arg = arg.name
        try:
            self.__getitem__(arg)
            return True
        except NameError:
            return False

    def __len__(self):
        hal_required()
        return sig_count()

    def __delitem__(self, char *name):
        hal_required()
        del self.sigs[name]
        r = hal_signal_delete(name)
        if r:
            raise RuntimeError("hal_signal_delete %s failed: %d %s" % (name, r, hal_lasterror()))

    def __call__(self):
        hal_required()
        return sig_names()

    def __repr__(self):
        hal_required()
        sigdict = {}
        for name in sig_names():
            sigdict[name] = self[name]
        return str(sigdict)

signals = Signals()
