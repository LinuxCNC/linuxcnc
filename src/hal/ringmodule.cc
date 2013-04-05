// XXX: make separate classes for Record and Stream rings
// XXX: think about overwritable streams
// Python bindings for hal_ring_* methods
//
// Pavel Shramov & Michael Haberler 2/2013
#include <boost/python.hpp>
#include <string>

#include "rtapi.h"
#include "hal.h"
#include "hal_priv.h"
#include "hal_ring.h"	        /* ringbuffer declarations */

namespace bp = boost::python;

static int comp_id;

static void hal_startup(void)
{
    char msg[LINELEN];
    char name[HAL_NAME_LEN + 1];
    int retval;

    rtapi_set_msg_level(RTAPI_MSG_ALL);

    rtapi_snprintf(name, sizeof(name), "pyring%d", getpid());
    comp_id = hal_init(name);
    if (comp_id < 0) {
	rtapi_snprintf(msg, sizeof(msg), "hal_init(%s) failed: %s",
		       name, strerror(-comp_id));
        PyErr_SetString(PyExc_RuntimeError, msg);
        throw boost::python::error_already_set();
    }
    retval = hal_ready(comp_id);
    if (retval < 0) {
	rtapi_snprintf(msg, sizeof(msg), "hal_ready(%s,%d) failed: %s",
		       name, comp_id, strerror(-retval));
        PyErr_SetString(PyExc_RuntimeError, msg);
        throw boost::python::error_already_set();
    }
}

__attribute__((destructor))
static void end (void)
{
    if (comp_id > 0)
	hal_exit(comp_id);
}

class Ring : public ringbuffer_t {

private:
    ringbuffer_t rb;
    std::string rname;

public:
    bool is_stream, use_rmutex, use_wmutex;

    // attach to existing ring
    Ring(const char *name) : rname(name) {
	int retval;

	if ((retval = hal_ring_attach(name, &rb, comp_id))) {
	    char msg[LINELEN];
	    rtapi_snprintf(msg, sizeof(msg), "hal_ring_attach(): no such ring: '%s': %s",
			   name, strerror(-retval));
	    PyErr_SetString(PyExc_NameError, msg);
	    throw boost::python::error_already_set();
	}
	is_stream = rb.header->is_stream;
	use_rmutex = rtapi_ring_use_rmutex(&rb);
	use_wmutex = rtapi_ring_use_wmutex(&rb);
    }

    // create a new ring of given size and mode
    Ring(const char *name, size_t size, int mode) : rname(name)  {
	char msg[LINELEN];
	int retval;
	//printf("Ring(%s,%d,%d)\n",name,size,mode);
	if ((retval = hal_ring_new(name, size, 0, comp_id, mode)) < 0) {
	    rtapi_snprintf(msg, sizeof(msg), "hal_ring_new(): can't create ring '%s': %s",
			   name, strerror(-retval));
	    PyErr_SetString(PyExc_NameError, msg);
	    throw boost::python::error_already_set();
	}
	// and attach to it
	if (hal_ring_attach(name, &rb, comp_id) < 0) {
	    rtapi_snprintf(msg, sizeof(msg), "hal_ring_attach(): no such ring: '%s': %s",
			   name, strerror(-retval));
	    PyErr_SetString(PyExc_NameError, msg);
	    throw boost::python::error_already_set();
	}
	is_stream = mode & MODE_STREAM;
	use_rmutex = mode & USE_RMUTEX;
	use_wmutex = mode & USE_WMUTEX;
    }

    ~Ring()  {
    }

    bp::object next_size() {
	int retval;

	if (is_stream) {
	    PyErr_SetString(PyExc_IOError,
			    "next_size() is invalid for Record mode rings");
	    throw boost::python::error_already_set();
	}
	if ((retval = rtapi_record_next_size(&rb)) > -1)
	    return bp::object(retval);
	return bp::object();
    }

    int shift() {
	if (is_stream) {
	    PyErr_SetString(PyExc_IOError,
			    "shift() with no argument is invalid for Record mode rings");
	    throw boost::python::error_already_set();
	}
	return rtapi_record_shift(&rb);
    }

    void consume(int nbytes) {
	size_t avail;

	if (!is_stream) {
	    PyErr_SetString(PyExc_IOError,
			    "shift(int) is invalid for Stream mode rings");
	    throw boost::python::error_already_set();
	}
	avail = rtapi_stream_read_space(rb.header);
	if (nbytes > (int) avail) {
	    char msg[LINELEN];
	    rtapi_snprintf(msg, sizeof(msg),
			   "shift(%d): argument larger than bytes available (%d)",
			   nbytes, avail);
	    PyErr_SetString(PyExc_NameError, msg);
	    throw boost::python::error_already_set();
	}
	rtapi_stream_read_advance(&rb, nbytes);
    }

    int write(char *buf, size_t size) {
	int retval;
	char msg[LINELEN];

	if (is_stream) {
	    unsigned rsize = rtapi_stream_write(&rb, buf, size);
	    return  (rsize != size) ? rsize : 0;
	} else {
	    if ((retval = rtapi_record_write(&rb, buf, size)) == ERANGE) {
		// ERANGE: record greater than ring buffer (fatal)
		rtapi_snprintf(msg, sizeof(msg),
			       "write: record size %d greater than buffer size %d",
			       size, rb.header->size);
		PyErr_SetString( PyExc_IOError, msg);
		throw boost::python::error_already_set();
	    }
	    // may return EAGAIN:  currently not enough space in ring buffer (temporary)
	    return retval;
	}
    }

    size_t available() const {
	if (is_stream)
	    return rtapi_stream_write_space(rb.header);
	else
	    return rtapi_record_write_space(rb.header);
    }

    void flush() {
	if (is_stream)
	    rtapi_stream_flush(&rb);
	else
	    rtapi_record_flush(&rb);
    }

    bp::object next_buffer()
    {
	if (is_stream) {
	    ringvec_t vec[2];

	    rtapi_stream_get_read_vector(&rb, vec);
	    if (vec[0].rv_len) {
		bp::handle<> h(PyString_FromStringAndSize((const char *)vec[0].rv_base,
							  vec[0].rv_len));
		if (vec[1].rv_len == 0) {
		    rtapi_stream_read_advance(&rb, vec[0].rv_len);
		    return bp::object(h);
		} else {
		    bp::handle<> h2(PyString_FromStringAndSize((const char *)vec[1].rv_base,
							       vec[1].rv_len));
		    rtapi_stream_read_advance(&rb, vec[0].rv_len + vec[1].rv_len);
		    return bp::object(h) + bp::object(h2);
		}
	    } else
		return bp::object();
	} else {
	    ring_size_t size = rtapi_record_next_size(&rb);
	    if (size < 0)
		return bp::object();
	    bp::handle<> h(PyString_FromStringAndSize((const char *)rtapi_record_next(&rb), size));
	    return bp::object(h);
	}
    }

    bp::object scratchpad()
    {
	if (rb.header->scratchpad_size == 0)
	    return bp::object();
	bp::handle<> h(PyString_FromStringAndSize((const char *)rb.scratchpad,
						  rb.header->scratchpad_size));
	return bp::object(h);
    }

    int get_reader()       { return rb.header->reader; }
    void set_reader(int r) { rb.header->reader = r; }
    int get_writer()       { return rb.header->writer; }
    void set_writer(int w) { rb.header->writer = w; }
    int get_size()       { return rb.header->size; }
    int get_spsize()       { return rb.header->scratchpad_size; }

    bool get_stream_mode() { return is_stream; }
    bool get_rmutex_mode() { return use_rmutex; }
    bool get_wmutex_mode() { return use_wmutex; }
    std::string get_name() { return rname; }
};

using namespace boost::python;

static  Ring *ring_attach(const char *name)
{
    return new Ring(name);
}

static  Ring *ring_create(const char *name, size_t size, int mode)
{
    return new Ring(name, size, mode);
}

static int ring_detach(const char *name)
{
    return hal_ring_detach(name);
}

static list ring_names (void)
{
    bp::list result;
    hal_ring_t *ring __attribute__((cleanup(halpr_autorelease_mutex)));
    int next;

    rtapi_mutex_get(&(hal_data->mutex));
    next = hal_data->ring_list_ptr;
    while (next != 0) {
	ring = (hal_ring_t *) SHMPTR(next);
	result.append(ring->name);
	next = ring->next_ptr;
    }
    return result;
}

BOOST_PYTHON_MEMBER_FUNCTION_OVERLOADS( Ring_shift_overloads,
					Ring::shift, 0,1)

BOOST_PYTHON_MODULE(ring) {

    scope().attr("__doc__") = "Simple ringbuffer implementation\n";

    scope().attr("MODE_STREAM") = MODE_STREAM;
    scope().attr("USE_RMUTEX") = USE_RMUTEX;
    scope().attr("USE_WMUTEX") = USE_WMUTEX;

    class_<Ring,boost::noncopyable>("Ring", no_init)
	.def("next", &Ring::next_buffer,
	     "in Record mode, returns the size of the next record , "
	     "or the number of bytes readable in Stream mode. "
	     "both return -1 if no data is available. Note than "
	     "in Record mode, 0 is a legit record size, so test for -1"
	     " in code which can use either mode.")

	.def("write", &Ring::write,
	     "write to ring. Returns 0 on success."
	     "in Stream mode, a nozero return value indicates the number"
	     " of bytes actually written. In record mode, a non-zero"
	     " return value indicates the write failed due to lack of "
	     "buffer space, and should be retried later. An oversized "
	     "record (larger than buffer size) will raise an IOError "
	     "exception.")

	.def("flush", &Ring::flush,
	     "clear the buffer contents. Note this is not thread-safe"
	     " unless all readers and writers use a r/w mutex.")

	.def("available", &Ring::available,
	     "in Stream mode, return number of bytes available to write. "
	     "in Record mode, return the size of the largest record which"
	     " safely be written.")

	.def("next_buffer", &Ring::next_buffer,
	     "Return the next record, or all available bytes"
	     " as a Buffer  object, or None. "
	     "this is a 'peek read' - "
	     "data is not actually removed from the buffer "
	     "until shift (Record mode) or shift(number of bytes) "
	     "is executed.")

	// record-mode reading operations
	.def("next_size", &Ring::next_size,
	     "applicable in Record mode only."
	     "Return size of the next record. Int. "
	     "Zero is a valid record length. "
	     "If the buffer is empty, return None.")

	.def("shift", &Ring::shift,
	     "applicable in Record mode only. "
	     "consume the current record.")

	// stream mode operations
	.def("consume", &Ring::consume,
	     "applicable in Stream mode only. "
	     "remove argument number of bytes from stream. "
	     "May raise IOError if more than the number of "
	     "available bytes are consumed,")

	.add_property("reader", &Ring::get_reader, &Ring::set_reader)
	.add_property("writer", &Ring::get_writer, &Ring::set_writer)
	.add_property("size",  &Ring::get_size)
	.add_property("spsize",  &Ring::get_spsize,
		      "return the scratchpad size")
	.add_property("name",  &Ring::get_name)
	.add_property("is_stream",  &Ring::get_stream_mode)
	.add_property("use_rmutex",  &Ring::get_rmutex_mode)
	.add_property("use_wmutex",  &Ring::get_wmutex_mode)
	;

    hal_startup();

    scope().attr("comp_id") = comp_id;
    def("rings", ring_names);
    def("attach", ring_attach, return_value_policy<manage_new_object>());
    def("create", ring_create, return_value_policy<manage_new_object>());
    def("detach", ring_detach);
}

// Future: migrate to Memoryview. See:
// http://stackoverflow.com/questions/8123121/how-to-get-back-a-valid-object-from-python-in-c-while-this-object-has-been-con


// PyObject* Allocator()
// {
//     void* buff = my_alloc_function(char, size);
//     Py_buffer pybuffer;
//     int res = PyBuffer_FillInfo(&pybuffer, 0, buff, size, false, PyBUF_CONTIG);
//     if (res == -1)
//         return NULL;
//     return PyMemoryView_FromBuffer(&pybuffer);
// }

// void Destructor()(object pyMemoryView_object) const
// {
//     Py_buffer* py_buffer = PyMemoryView_GET_BUFFER(pyMemoryView_object.ptr());
//     my_free_function(py_buffer->buf);
//     PyBuffer_Release(py_buffer);
// }
