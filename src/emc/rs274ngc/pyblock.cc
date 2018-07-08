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
#include <boost/python/class.hpp>
#include <boost/python/suite/indexing/map_indexing_suite.hpp>
#include <map>

namespace bp = boost::python;

#include "rs274ngc.hh"
#include "interp_internal.hh"
#include "rs274ngc_interp.hh"
#include "array1.hh"

namespace pp = pyplusplus::containers::static_sized;
#include "interp_array_types.hh"

static const char *get_comment(block &b) { return b.comment; };
static const char *get_o_name(block &b) { return b.o_name; };

static  g_modes_array g_modes_wrapper ( block & b) {
    return g_modes_array(b.g_modes);
}

static  m_modes_array m_modes_wrapper ( block & b) {
    return m_modes_array(b.m_modes);
}

static params_array params_wrapper ( block & b) {
    return params_array(b.params);
}

void export_Block()
{
    using namespace boost::python;
    using namespace boost;
    class_ <block, noncopyable>("Block",no_init)
	.def_readwrite("f_flag",&block::f_flag)
	.def_readwrite("p_flag",&block::p_flag)
	.def_readwrite("p_number",&block::p_number)
	.def_readwrite("a_flag",&block::a_flag)
	.def_readwrite("a_number",&block::a_number)
	.def_readwrite("b_flag",&block::b_flag)
	.def_readwrite("b_number",&block::b_number)
	.def_readwrite("c_flag",&block::c_flag)
	.def_readwrite("c_number",&block::c_number)
	.def_readwrite("d_number_float",&block::d_number_float)
	.def_readwrite("d_flag",&block::d_flag)
	.def_readwrite("e_flag",&block::e_flag)
	.def_readwrite("e_number",&block::e_number)
	.def_readwrite("f_flag",&block::f_flag)
	.def_readwrite("f_number",&block::f_number)
	.def_readwrite("h_flag",&block::h_flag)
	.def_readwrite("h_number",&block::h_number)
	.def_readwrite("i_flag",&block::i_flag)
	.def_readwrite("i_number",&block::i_number)
	.def_readwrite("j_flag",&block::j_flag)
	.def_readwrite("j_number",&block::j_number)
	.def_readwrite("k_flag",&block::k_flag)
	.def_readwrite("k_number",&block::k_number)
	.def_readwrite("l_number",&block::l_number)
	.def_readwrite("l_flag",&block::l_flag)
	.def_readwrite("line_number",&block::line_number)
	.def_readwrite("saved_line_number",&block::line_number)
	.def_readwrite("n_number",&block::n_number)
	.def_readwrite("motion_to_be",&block::motion_to_be)
	.def_readwrite("m_count",&block::m_count)
	.def_readwrite("user_m",&block::user_m)
	.def_readwrite("p_number",&block::p_number)
	.def_readwrite("p_flag",&block::p_flag)
	.def_readwrite("q_number",&block::q_number)
	.def_readwrite("q_flag",&block::q_flag)
	.def_readwrite("r_flag",&block::r_flag)
	.def_readwrite("r_number",&block::r_number)
	.def_readwrite("s_flag",&block::s_flag)
	.def_readwrite("s_number",&block::s_number)
	.def_readwrite("t_flag",&block::t_flag)
	.def_readwrite("t_number",&block::t_number)
	.def_readwrite("u_flag",&block::u_flag)
	.def_readwrite("u_number",&block::u_number)
	.def_readwrite("v_flag",&block::v_flag)
	.def_readwrite("v_number",&block::v_number)
	.def_readwrite("w_flag",&block::w_flag)
	.def_readwrite("w_number",&block::w_number)
	.def_readwrite("x_flag",&block::x_flag)
	.def_readwrite("x_number",&block::x_number)
	.def_readwrite("y_flag",&block::y_flag)
	.def_readwrite("y_number",&block::y_number)
	.def_readwrite("z_flag",&block::z_flag)
	.def_readwrite("z_number",&block::z_number)
	.def_readwrite("radius_flag",&block::radius_flag)
	.def_readwrite("radius",&block::radius)
	.def_readwrite("theta_flag",&block::theta_flag)
	.def_readwrite("theta",&block::theta)

	.def_readwrite("offset",&block::offset)
	.def_readwrite("o_type",&block::o_type)

	// I hope someday I really understand this
	.add_property("executing_remap",
		      make_getter(&block::executing_remap,
				  return_value_policy<reference_existing_object>()),
		      make_setter(&block::executing_remap,
				  return_value_policy<reference_existing_object>()))

	.def_readwrite("call_type",&block::call_type)
	.def_readwrite("breadcrumbs",&block::breadcrumbs)
	.def_readwrite("phase",&block::phase)
	.def_readwrite("builtin_used",&block::builtin_used)

	//  read-only
	.add_property("comment",  &get_comment)
	.add_property("o_name",   &get_o_name)

	.add_property( "params",
		       bp::make_function( params_w(&params_wrapper),
					  bp::with_custodian_and_ward_postcall< 0, 1 >()))
	// arrays
	.add_property( "m_modes",
		       bp::make_function( m_modes_w(&m_modes_wrapper),
					  bp::with_custodian_and_ward_postcall< 0, 1 >()))
	.add_property( "g_modes",
		       bp::make_function( g_modes_w(&g_modes_wrapper),
					  bp::with_custodian_and_ward_postcall< 0, 1 >()))

	;

}
