#include <boost/python.hpp>

namespace bp = boost::python;

#include <stdio.h>
#include <string.h>

#include "emcglb.h"
#include "rs274ngc.hh"
#include "interp_return.hh"
#include "interp_internal.hh"
#include "rs274ngc_interp.hh"
#include "units.h"

BOOST_PYTHON_MODULE(EmcConfig) {
    using namespace boost::python;
    using namespace boost;

    scope().attr("__doc__") =
        "EMC configuration access\n"
        ;

    scope().attr("EMC_INIFILE") = (char *) _parameter_file_name;
}
