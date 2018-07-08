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

#define BOOST_PYTHON_MAX_ARITY 7
#include <boost/python/object.hpp>
#include <boost/python/suite/indexing/map_indexing_suite.hpp>
#include <map>

namespace bp = boost::python;

#include "rs274ngc.hh"
#include "interp_internal.hh"
#include "rs274ngc_interp.hh"
#include "array1.hh"

namespace pp = pyplusplus::containers::static_sized;
#include "interp_array_types.hh"

static  active_g_codes_array saved_g_codes_wrapper ( context &c) {
    return active_g_codes_array(c.saved_g_codes);
}

static  active_m_codes_array saved_m_codes_wrapper ( context &c) {
    return active_m_codes_array(c.saved_m_codes);
}

static  active_settings_array saved_settings_wrapper ( context &c) {
    return active_settings_array(c.saved_settings);
}

static params_array saved_params_wrapper ( context &c) {
    return params_array(c.saved_params);
}
static bp::object remap_str( remap_struct &r) {
    return  bp::object("Remap(%s argspec=%s modal_group=%d prolog=%s ngc=%s python=%s epilog=%s) " %
		       bp::make_tuple(r.name,r.argspec,r.modal_group,r.prolog_func,
				      r.remap_ngc, r.remap_py, r.epilog_func));
}

void export_Internals()
{
    using namespace boost::python;
    using namespace boost;
    class_ <context, noncopyable>("Context",no_init)
	.def_readwrite("position",&context::position)
	.def_readwrite("sequence_number",&context::sequence_number)
	.def_readwrite("filename",  &context::filename)
	.def_readwrite("subname",  &context::subName)
	.add_property( "saved_params",
		       bp::make_function( saved_params_w(&saved_params_wrapper),
					  bp::with_custodian_and_ward_postcall< 0, 1 >()))
	.add_property( "saved_g_codes",
		       bp::make_function( active_g_codes_w(&saved_g_codes_wrapper),
					  bp::with_custodian_and_ward_postcall< 0, 1 >()))
	.add_property( "saved_m_codes",
		       bp::make_function( active_m_codes_w(&saved_m_codes_wrapper),
					  bp::with_custodian_and_ward_postcall< 0, 1 >()))
	.add_property( "saved_settings",
		       bp::make_function( active_settings_w(&saved_settings_wrapper),
					  bp::with_custodian_and_ward_postcall< 0, 1 >()))
	.def_readwrite("context_status", &context::context_status)
	.def_readwrite("named_params",  &context::named_params)

	.def_readwrite("call_type",  &context::call_type)
	//.def_readwrite("tupleargs",  &context::tupleargs)
	//.def_readwrite("kwargs",  &context::kwargs)
	//.def_readwrite("py_return_type",  &context::py_return_type)
	//.def_readwrite("py_returned_double",  &context::py_returned_double)
	//.def_readwrite("py_returned_int",  &context::py_returned_int)
	//.def_readwrite("generator_next",  &context::generator_next)

	;
    // FIXME make noncopyable: class_<ParamClass, noncopyable>("Params","Interpreter parameters",no_init)
    class_ <remap_struct /*, noncopyable */>("Remap" /*, no_init*/)
	.def_readwrite("name",&remap::name)
	.def_readwrite("argspec",&remap::argspec)
	.def_readwrite("modal_group",&remap::modal_group)
	.def_readwrite("prolog_func",&remap::prolog_func)
	.def_readwrite("remap_py",&remap::remap_py)
	.def_readwrite("remap_ngc",&remap::remap_ngc)
	.def_readwrite("epilog_func",&remap::epilog_func)
	.def_readwrite("motion_code",&remap::motion_code)
	.def("__str__", &remap_str)

	;

    class_<remap_map,noncopyable>("RemapMap",no_init)
        .def(map_indexing_suite<remap_map>())
	;

    class_<parameter_value_struct /*,noncopyable */>("ParameterValue") // ,no_init)
	.def_readwrite("attr",&parameter_value_struct::attr)
	.def_readwrite("value",&parameter_value_struct::value)
	;

    class_<parameter_map,noncopyable>("ParameterMap",no_init)
        .def(map_indexing_suite<parameter_map>())
	;
}
