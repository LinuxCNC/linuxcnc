#include "catch.hpp"

#include <interp_testing_util.hh> // For core interp stuff and extra REQUIRE macros/ setup
#include <rs274ngc_interp.hh>
#include <interp_inspection.hh>
#include <interp_return.hh>
#include <saicanon.hh>

#include <interp_parameter_def.hh>
using namespace interp_param_global;

/** Reads and parses a G-code command (and checks for errors along the way). */
static block_struct read_and_parse_command(Interp &interp, const char *command)
{
  char rawline[1024]="";
  char blocktext[1024]="";
  int line_length = 0;
  REQUIRE_INTERP_OK(interp.read_text(
                      command,
                      nullptr,
                      rawline,
                      blocktext,
                      &line_length));

  // Ensure it actually does something
  if (command && command[0]) {
    REQUIRE(line_length);
  }

  block_struct eblock{};
  REQUIRE_INTERP_OK(interp.init_block(&eblock));
  REQUIRE_INTERP_OK(interp.read_items(&eblock, blocktext, interp._setup.parameters));
  return eblock;
}

SCENARIO("Movement commands")
{
  GIVEN("Linear Feed along all axes ")
  {
    DECL_INIT_TEST_INTERP();
    REQUIRE_INTERP_OK(test_interp.execute("g20"));
    REQUIRE(settings->length_units == CANON_UNITS_INCHES);

    THEN("Block should have G1 word and axis words") {
      const char *command = "G1 X1 Y2 Z3 A4 C6";

      block_struct eblock = read_and_parse_command(test_interp, command);
      REQUIRE(eblock.x_flag);
      REQUIRE(eblock.y_flag);
      REQUIRE(eblock.z_flag);
      REQUIRE(eblock.a_flag);
      REQUIRE(!eblock.b_flag);
      REQUIRE(eblock.c_flag);

      REQUIRE_FUZZ(eblock.x_number, 1);
      REQUIRE_FUZZ(eblock.y_number, 2);
      REQUIRE_FUZZ(eblock.z_number, 3);
      REQUIRE_FUZZ(eblock.a_number, 4);
      REQUIRE_FUZZ(eblock.c_number, 6);
    }
  }
}
