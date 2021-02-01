"""hal library python interface using ctypes ffi interface"""

from ctypes import*


lib = CDLL('liblinuxcnchal.so')
lib.hal_malloc.restype = c_void_p
lib.hal_malloc.argtype = [c_long]
lib.hal_comp_name.restype = c_char_p

lib.hal_pin_new.argtypes = [c_char_p, c_int, c_int, c_void_p, c_int]

lib.hal_signal_new.argtypes = [c_char_p, c_int]

lib.hal_link.argtypes = [c_char_p, c_char_p]

lib.hal_port_read.argtypes = [c_int, c_char_p, c_uint]
lib.hal_port_read.restype = c_bool

lib.hal_port_peek.argtypes = [c_int, c_char_p, c_uint]
lib.hal_port_peek.restype = c_bool

lib.hal_port_peek_commit.argtypes = [c_int, c_char_p, c_uint]
lib.hal_port_peek.restype = c_bool

lib.hal_port_write.argtypes = [c_int, c_char_p, c_uint]
lib.hal_port_write.restype = c_bool

lib.hal_port_readable.argtypes = [c_int]
lib.hal_port_readable.restype = c_uint

lib.hal_port_writable.argtypes = [c_int]
lib.hal_port_writable.restype = c_uint

lib.hal_port_buffer_size.argtypes = [c_int]
lib.hal_port_buffer_size.restype = c_uint

lib.hal_port_clear.argtypes = [c_int]

lib.hal_port_wait_readable.argtypes = [POINTER(POINTER(c_int)), c_uint, c_int]
lib.hal_port_wait_writable.argtypes = [POINTER(POINTER(c_int)), c_uint, c_int]

class HalException(Exception):
    """An exception raised by hal library functions"""
    pass


class halType:
    BIT = 1
    FLOAT = 2
    SIGNED = 3
    UNSIGNED = 4
    PORT = 5

    values = [BIT, FLOAT, SIGNED, UNSIGNED, PORT ]

    typeConversion = { BIT : c_bool,
                       FLOAT : c_double,
                       SIGNED : c_int32,
                       UNSIGNED : c_uint32,
                       PORT : c_int }

class pinDir:
    IN = 16
    OUT = 32
    IO = IN | OUT

    values =  [IN, OUT, IO]


class pin(object):
    def __init__(self, component, name, type, dir, data_ptr):
        self.comp = component
        self.name = name
        self.type = type
        self.dir = dir
        self.data_ptr = data_ptr

    @property
    def fullname(self):
        return "{0}.{1}".format(self.comp.name, self.name)

    @property
    def value(self):
        if self.type == halType.PORT:
            raise HalException("cannot get value of PORT pin")
        else:
            return self.data_ptr.contents.contents.value

    @value.setter
    def value(self, val):
        if self.type == halType.PORT:
            raise HalException("cannot set value of PORT pin")
        else:
            self.data_ptr.contents.contents.value = val


class port(pin):
    def __init__(self, component, name, type, dir, data_ptr):
        pin.__init__(self, component, name, type, dir, data_ptr)

    @property
    def __port(self):
        return self.data_ptr.contents.contents.value

    def read(self, count):
        if self.dir != pinDir.IN:
            raise HalException("cannot read output port")

        buff = create_string_buffer(count)
        if not lib.hal_port_read(self.__port, buff, count):
            return ''
        else:
            return buff.raw

    def peek(self, count):
        if self.dir != pinDir.IN:
            raise HalException("cannot peek output port")

        buff = create_string_buffer(count)
        if not lib.hal_port_peek(self.__port, buff, count):
            return ''
        else:
            return buff.raw

    def peek_commit(self, count):
        if self.dir != pinDir.IN:
            raise HalException("cannot peek commit output port")
        return lib.hal_port_peek_commit(self.__port, count)

    def write(self, buff):
        if self.dir != pinDir.OUT:
           raise HalException("cannot write input port")

        return lib.hal_port_write(self.__port, buff.encode(), len(buff))

    def readable(self):
        if self.dir != pinDir.IN:
            raise HalException("cannot read output port")

        return lib.hal_port_readable(self.__port)

    def writable(self):
        if self.dir != pinDir.OUT:
            raise HalException("cannot write input port")

        return lib.hal_port_writable(self.__port)

    def size(self):
        return lib.hal_port_buffer_size(self.__port)

    def clear(self):
        if self.dir != pinDir.IN:
            raise HalException("cannot clear output port")

        lib.hal_port_clear(self.__port)

    def waitReadable(self, count):
        if self.dir != pinDir.IN:
            raise HalException("cannot read output port")

        lib.hal_port_wait_readable(self.data_ptr, count, 0)

    def waitWritable(self, count):
        if self.dir != pinDir.OUT:
            raise HalException("cannot write input port")

        lib.hal_port_wait_writable(self.data_ptr, count, 0)

    

class component:
    def __init__(self, name):
        self.id = lib.hal_init(name)
        self.name = name
        self.pins = {}
        if self.id < 0:
            raise HalException('failed to initialized component "{0}"'.format(name))
            

    def __del__(self):
        self.exit()

          
    def exit(self):
        if self.id > 0:
            result = lib.hal_exit(self.id)
            self.id = -1
            if result < 0:
                raise HalException('hal_exit failed with code {0}'.format(result))

        
    def ready(self):
        result = lib.hal_ready(self.id)
        if result < 0:
            raise HalException('hal_ready failed with code {0}'.format(result))

    def name(self):
        return lib.hal_comp_name(self.id)


    def pinNew(self, name, type, dir):
        if not type in halType.typeConversion:
            raise HalException('failed to create pin "{0}". Unknown type {1}'.format(name, type))

        if not dir in pinDir.values:
            raise HalException('failed to create pin "{0}". Unknown dir {1}'.format(name, dir))

        ctype = halType.typeConversion[type]

        ptr = self.halMalloc(sizeof(c_void_p))
        result = lib.hal_pin_new("{0}.{1}".format(self.name, name).encode(), type, dir, ptr, self.id)
        if result < 0:
            raise HalException('failed to create pin "{0}" with code {1}'.format(name, result))

        if type == halType.PORT:
            self.pins[name] = port(self, name, type, dir,  cast(ptr, POINTER(POINTER(ctype))))
        else:
            self.pins[name] = pin(self, name, type, dir, cast(ptr, POINTER(POINTER(ctype))))

        return self.pins[name]

    def sigNew(self, name, type):
        if not type in halType.values:
            raise HalException('failed to create signal "{0}". Invalid type {1}'.format(name, type))

        result = lib.hal_signal_new(name.encode(), type)

        if result < 0:
            raise HalException('failed to create signal "{0}" with code {1}'.format(name, result))

    def sigLink(self, pin_name, sig_name):
        result = lib.hal_link(pin_name.encode(), sig_name.encode())
        if result < 0:
            raise HalException('failed to link sig "{0}" to pin "{1}" with result {2}'.format(sig_name, pin_name, result))


    def halMalloc(self, count):
        ptr = lib.hal_malloc(count)
        memset(ptr, 0, count)
        return ptr




