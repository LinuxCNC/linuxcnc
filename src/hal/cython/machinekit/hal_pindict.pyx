# Pins pseudo dictionary


# hal_iter callback: add pin names into list
cdef int _collect_pin_names(hal_pin_t *pin,  void *userdata):
    arg =  <object>userdata
    if  isinstance(arg, list):
        arg.append(pin.name)
        return 0
    else:
        return -1

cdef list pin_names():
    names = []
    with HALMutex():
        rc = halpr_foreach_pin(NULL,  _collect_pin_names, <void *>names);
        if rc < 0:
            raise RuntimeError("pin_names: halpr_foreach_pin failed %d: %s" % (rc,hal_lasterror()))
    return names

cdef int pin_count():
    with HALMutex():
        rc = halpr_foreach_pin(NULL, NULL, NULL);
        if rc < 0:
            raise RuntimeError("pin_count: halpr_foreach_pin failed %d: %s" % (rc,hal_lasterror()))
    return rc


cdef class Pins:
    cdef dict pins

    def __cinit__(self):
        self.pins = dict()

    def __getitem__(self, name):
        hal_required()

        if isinstance(name, int):
            return pin_names()[name]

        if name in self.pins:
            return self.pins[name]
        cdef hal_pin_t *p
        with HALMutex():
            p = halpr_find_pin_by_name(name)
        if p == NULL:
            raise NameError, "no such pin: %s" % (name)
        pin =  Pin(name)
        self.pins[name] = pin
        return pin

    def __contains__(self, arg):
        if isinstance(arg, Pin):
            arg = arg.name
        try:
            self.__getitem__(arg)
            return True
        except NameError:
            return False

    def __len__(self):
        hal_required()
        return pin_count()

    def __call__(self):
        hal_required()
        return pin_names()

    def __repr__(self):
        hal_required()
        pindict = {}
        for name in pin_names():
            pindict[name] = self[name]
        return str(pindict)

pins = Pins()
