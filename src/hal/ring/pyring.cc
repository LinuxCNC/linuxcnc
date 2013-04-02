#include <boost/python.hpp>

#include "ring.h"

namespace py = boost::python;

class Ring : public ringbuffer_t {
public:
	Ring(size_t size) { ring_init(this, size, 0); }
	Ring(void* ptr) { ring_init(this, 0, ptr); }

	ring_size_t next_size() { return ring_next_size(this); }
	const void* next() { return ring_next(this); }

	int write(char *buf, size_t size) { return ring_write(this, buf, size); }
	void shift() { return ring_shift(this); }
	size_t available() const { return ring_available(this); }

	py::object read_buffer() const
	{
		const void *data;
		size_t size;

		if (ring_read(this, &data, &size))
			return py::object();
		py::handle<> h(PyBuffer_FromMemory ((void *) data, size));
		return py::object(h);
	}
};

class RingIter : public ringiter_t {
	const Ring &_ring;
public:
	RingIter(const Ring &ring) : _ring(ring) { ring_iter_init(&ring, this); }
	int shift() { return ring_iter_shift(this);}
	py::object read_buffer() const
	{
		const void *data;
		size_t size;

		if (ring_iter_read(this, &data, &size))
			return py::object();
		py::handle<> h(PyBuffer_FromMemory ((void *) data, size));
		return py::object(h);
	}
};

BOOST_PYTHON_MODULE(ring) {
	using namespace boost::python;

	scope().attr("__doc__") = "Simple ringbuffer implementation\n";

	class_<Ring>("Ring", init<size_t>())
		.def("next", &Ring::read_buffer)
		.def("next_size", &Ring::next_size)
		.def("read", &Ring::read_buffer)
		.def("available", &Ring::available)
		.def("shift", &Ring::shift)
		.def("write", &Ring::write);
	class_<RingIter>("RingIter", init<const Ring &>())
		.def("shift", &RingIter::shift)
		.def("read", &RingIter::read_buffer);
}
