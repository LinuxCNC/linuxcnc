#ifndef RS274NGC_INTERP_PYTHON
#define RS274NGC_INTERP_PYTHON
#ifndef BOOST_PYTHON_NAX_ARITY
#define BOOST_PYTHON_MAX_ARITY 4
#endif
#include <boost/python/object.hpp>
struct pycontext_impl {
    boost::python::object tupleargs; // the args tuple for Py functions
    boost::python::object kwargs; // the args dict for Py functions
    int py_return_type;   // type of Python return value - enum retopts 
    double py_returned_double;
    int py_returned_int;
    // generator object next method as returned if Python function contained a yield statement
    boost::python::object generator_next; 
};
#endif
