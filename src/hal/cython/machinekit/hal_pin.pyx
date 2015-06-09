from .hal_priv cimport MAX_EPSILON, hal_data_u
from .hal_util cimport shmptr, pin_linked, linked_signal


def describe_hal_type(haltype):
    if haltype == HAL_FLOAT:
        return 'float'
    elif haltype == HAL_BIT:
        return 'bit'
    elif haltype == HAL_U32:
        return 'u32'
    elif haltype == HAL_S32:
        return 's32'
    else:
        return 'unknown'

def describe_hal_dir(haldir):
    if haldir == HAL_IN:
        return 'in'
    elif haldir == HAL_OUT:
        return 'out'
    elif haldir == HAL_IO:
        return 'io'
    else:
        return 'unknown'


cdef class _Pin:
    cdef hal_data_u **_storage
    cdef hal_pin_t *_pin

    def __cinit__(self, *args,  init=None, eps=0):
        hal_required()
        self._storage = NULL
        if len(args) == 1:
            #  wrapping existing pin, args[0] = name
            with HALMutex():
                self._pin = halpr_find_pin_by_name(args[0])
                if self._pin == NULL:
                    raise RuntimeError("no such pin %s" % args[0])
        else:
            # create a new pin and wrap it
            comp = args[0]
            name = args[1]
            t = args[2]
            dir = args[3]
            if (eps < 0) or (eps > MAX_EPSILON-1):
                raise RuntimeError("pin %s : epsilon index out of range" % (name, eps))

            self._storage = <hal_data_u **>hal_malloc(sizeof(hal_data_u *))
            if self._storage == NULL:
                raise RuntimeError("Fail to allocate HAL memory for pin %s" % name)

            name = "{}.{}".format(comp.name, name)
            r = hal_pin_new(name, t, dir, <void **>(self._storage), (<Component>comp)._comp.comp_id)
            if r:
                raise RuntimeError("Fail to create pin %s: %d %s" % (name, r, hal_lasterror()))
            with HALMutex():
                self._pin = halpr_find_pin_by_name(name)
                self._pin.eps_index = eps

    property linked:
        def __get__(self): return pin_linked(self._pin)

    property signame:
        def __get__(self):
            if not  pin_linked(self._pin): return None  # raise exception?
            return linked_signal(self._pin).name

    property signal:
        def __get__(self):
            if not  pin_linked(self._pin): return None  # raise exception?
            return signals[self.signame]

    property type:
        def __get__(self): return self._pin.type

    property epsilon:
        def __get__(self): return hal_data.epsilon[self._pin.eps_index]

    property eps:
        def __get__(self): return self._pin.eps_index
        def __set__(self, int eps):
            if (eps < 0) or (eps > MAX_EPSILON-1):
                raise RuntimeError("pin %s : epsilon index out of range" % (self._pin.name, eps))
            self._pin.eps_index = eps

    property handle:
        def __get__(self): return self._pin.handle

    property name:
        def __get__(self): return self._pin.name

    property dir:
        def __get__(self): return self._pin.dir

    def link(self, arg):
        # check if we have a pin or a list of pins
        if isinstance(arg, Pin) \
           or (isinstance(arg, str) and (arg in pins)) \
           or hasattr(arg, '__iter__'):
            return net(self, arg)  # net is more verbose than link

        # we got a signal or a new signal
        return net(arg, self)

    def __iadd__(self, pins):
        return self.link(pins)

    def unlink(self):
        r = hal_unlink(self._pin.name)
        if r:
            raise RuntimeError("Failed to unlink pin %s: %d - %s" %
                               (self._pin.name, r, hal_lasterror()))

    def _set(self, v):
        cdef hal_data_u *_dptr
        if self._storage == NULL: # an existing pin, wrapped

            if pin_linked(self._pin):
                raise RuntimeError("cannot set value of linked pin %s:" % self._pin.name)

            # retrieve address of dummy signal
            _dptr = <hal_data_u *>&self._pin.dummysig
            return py2hal(self._pin.type, _dptr, v)
        else:
            # a pin we created
            return py2hal(self._pin.type, self._storage[0], v)

    def _get(self):
        cdef hal_data_u *_dptr
        cdef hal_sig_t *_sig
        if self._storage == NULL: # an existing pin, wrapped
            if pin_linked(self._pin):
                # get signal's data address
                _sig = <hal_sig_t *>shmptr(self._pin.signal);
                _dptr = <hal_data_u *>shmptr(_sig.data_ptr);
            else:
                # retrieve address of dummy signal
                _dptr = <hal_data_u *>&self._pin.dummysig
            return hal2py(self._pin.type, _dptr)
        else:
            # a pin we allocated storage for
            return hal2py(self._pin.type, self._storage[0])


class Pin(_Pin):
    def __init__(self, *args, init=None,eps=0):
        if len(args) == 1: # wrapped existing pin
            t = self.type
            dir = self.dir
            name = self.name
        else:
            t = args[2]   # created new pin
            dir = args[3]

        if relaxed or dir != HAL_IN:
            self.set = self._set
        if relaxed or dir != HAL_OUT:
            self.get = self._get
        if init:
            self.set(init)

    def set(self, v): raise NotImplementedError("Pin is read-only")
    def get(self): raise NotImplementedError("Pin is write-only")

    def __repr__(self):
        return "<hal.Pin %s %s %s %s>" % (self.name,
                                          describe_hal_type(self.type),
                                          describe_hal_dir(self.dir),
                                          self.get())
