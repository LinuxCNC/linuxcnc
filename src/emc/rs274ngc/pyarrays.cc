// Interpreter internals - Python bindings
// Michael Haberler 7/2011

#include <boost/python.hpp>
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
