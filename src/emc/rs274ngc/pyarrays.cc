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

// (at least in boost 1.55, return_internal_reference needs a definition
// of boost::python::detail::get which comes from detail/caller.hpp.
// At first sniff it's a boost bug but what can you do...)
#define BOOST_PYTHON_MAX_ARITY 4
#include <boost/python/detail/caller.hpp>
#include <boost/python/return_internal_reference.hpp>
namespace bp = boost::python;

#include "rs274ngc.hh"
#include "interp_internal.hh"
#include "array1.hh"

namespace pp = pyplusplus::containers::static_sized;
#include "interp_array_types.hh"


void export_Arrays()
{
    using namespace boost::python;
    using namespace boost;

    pp::register_array_1< int, ACTIVE_G_CODES> ("ActiveGcodesArray" );
    pp::register_array_1< int, ACTIVE_M_CODES> ("ActiveMcodesArray" );
    pp::register_array_1< double, ACTIVE_SETTINGS> ("ActiveSettingsArray");
    pp::register_array_1< block, MAX_NESTED_REMAPS,
	bp::return_internal_reference< 1, bp::default_call_policies > > ("BlocksArray");
    pp::register_array_1< double, RS274NGC_MAX_PARAMETERS > ("ParametersArray");
    pp::register_array_1< CANON_TOOL_TABLE, CANON_POCKETS_MAX,
	bp::return_internal_reference< 1, bp::default_call_policies > > ("ToolTableArray");
    pp::register_array_1< context, INTERP_SUB_ROUTINE_LEVELS,
	bp::return_internal_reference< 1, bp::default_call_policies > > ("SubcontextArray");
    pp::register_array_1< int, 16> ("GmodesArray");
    pp::register_array_1< int, 11> ("MmodesArray");
    pp::register_array_1< double, INTERP_SUB_PARAMS> ("SubroutineParamsArray");
}
