/*    This is a component of LinuxCNC
 *    Copyright 2013 Michael Haberler <git@mah.priv.at>
 *
 *    This program is free software; you can redistribute it and/or modify
 *    it under the terms of the GNU General Public License as published by
 *    the Free Software Foundation; either version 2 of the License, or
 *    (at your option) any later version.
 *
 *    This program is distributed in the hope that it will be useful,
 *    but WITHOUT ANY WARRANTY; without even the implied warranty of
 *    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *    GNU General Public License for more details.
 *
 *    You should have received a copy of the GNU General Public License
 *    along with this program; if not, write to the Free Software
 *    Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */
// Interpreter internals - Python bindings
// Michael Haberler 7/2011
//

#define BOOST_PYTHON_MAX_ARITY 4
#include <boost/python/extract.hpp>
#include <boost/python/suite/indexing/map_indexing_suite.hpp>
#include <map>

namespace bp = boost::python;
extern int _task;  // zero in gcodemodule, 1 in milltask

#include <stdio.h>
#include <string.h>
#include <assert.h>

#include "rs274ngc.hh"
#include "interp_return.hh"
#include "interp_internal.hh"
#include "rs274ngc_interp.hh"
#include "paramclass.hh"

#define IS_STRING(x) (PyObject_IsInstance(x.ptr(), (PyObject*)&PyString_Type))
#define IS_INT(x) (PyObject_IsInstance(x.ptr(), (PyObject*)&PyInt_Type))

// access to named and numbered parameters via a pseudo-dictionary
// either params["paramname"] or params[5400] is valid

ParamClass::ParamClass(Interp &i) : interp(i) {};

double ParamClass::getitem( bp::object sub)
{
    double retval = 0;
    if (IS_STRING(sub)) {
	char const* varname = bp::extract < char const* > (sub);
	int status;
	interp.find_named_param(varname, &status, &retval);
	if (!status)
	    throw std::runtime_error("parameter does not exist: "
				     + std::string(varname));
    } else
	if (IS_INT(sub)) {
	    int index = bp::extract < int > (sub);
	    retval = interp._setup.parameters[index];
	} else {
	    throw std::runtime_error("params subscript type must be integer or string");
	}
    return retval;
}

double ParamClass::setitem(bp::object sub, double dvalue)
{
    if (IS_STRING(sub)) {
	char const* varname = bp::extract < char const* > (sub);
	int status = interp.add_named_param(varname, varname[0] == '_' ? PA_GLOBAL :0);
	status = interp.store_named_param(&interp._setup,varname, dvalue, 0);
	if (status != INTERP_OK)
	    throw std::runtime_error("cant assign value to parameter: " +
				     std::string(varname));

    } else
	if (IS_INT(sub)) {
	    int index = bp::extract < int > (sub);
	    if ((index < 0) || (index > RS274NGC_MAX_PARAMETERS -1)) {
		std::stringstream sstr;
		sstr << "params subscript out of range : "
		     << index << " - must be between 0 and "
		     << RS274NGC_MAX_PARAMETERS;
		throw std::runtime_error(sstr.str());
	    }
	    interp._setup.parameters[index] = dvalue;
	    return dvalue;
	} else
	    throw std::runtime_error("params subscript type must be integer or string");
    return dvalue;
}

bp::list ParamClass::namelist(context &c) const {
    bp::list result;
    for(parameter_map::iterator it = c.named_params.begin();
	it != c.named_params.end(); ++it) {
	result.append( it->first);
    }
    return result;
}

bp::list ParamClass::locals() {
    return namelist(interp._setup.sub_context[interp._setup.call_level]);
}

bp::list ParamClass::globals() {
    return namelist(interp._setup.sub_context[0]);
}

bp::list ParamClass::operator()() const
{
    bp::list result = namelist(interp._setup.sub_context[interp._setup.call_level]);
    result.extend(namelist(interp._setup.sub_context[0]));
    return result;
};

int ParamClass::length() { return RS274NGC_MAX_PARAMETERS;}

void export_ParamClass()
{
    using namespace boost::python;
    using namespace boost;

    class_<ParamClass>("Params","Interpreter parameters",no_init)
	.def("__getitem__", &ParamClass::getitem)
        .def("__setitem__", &ParamClass::setitem)
        .def("__len__", &ParamClass::length)
        .def("globals", &ParamClass::globals)
        .def("locals", &ParamClass::locals)
	.def("__call__", &ParamClass::operator());
    ;
}
