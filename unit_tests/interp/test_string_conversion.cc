#include "catch.hpp"

#include <interp_testing_util.hh> // For core interp stuff and extra REQUIRE macros/ setup
#include <rs274ngc_interp.hh>
#include <interp_inspection.hh>
#include <interp_return.hh>
#include <saicanon.hh>
#include <interp_parameter_def.hh>
#include <interp_internal.hh>

using namespace interp_param_global;

TEST_CASE("convert G-codes to string")
{
    CHECK_THAT(toString(G_0), Catch::Matchers::Matches("G0"));
    CHECK_THAT(toString(G_5_1), Catch::Matchers::Matches("G5.1"));
    CHECK_THAT(toString(G_64), Catch::Matchers::Matches("G64"));
    CHECK_THAT(toString(G_61_1), Catch::Matchers::Matches("G61.1"));
}
