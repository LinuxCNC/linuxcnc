#include "catch.hpp"

#include <interp_testing_util.hh> // For core interp stuff and extra REQUIRE macros/ setup
#include <rs274ngc_interp.hh>
#include <interp_inspection.hh>
#include <interp_return.hh>
#include <interp_parameter_def.hh>
using namespace interp_param_global;

TEST_CASE("Interp Basics")
{
  DECL_INIT_TEST_INTERP();
  SECTION("Interp Init")
  {
    REQUIRE(currentX(settings) == 0.0);
    REQUIRE(currentY(settings) == 0.0);
    REQUIRE(currentZ(settings) == 0.0);
  }

  SECTION("Change Units")
  {
    REQUIRE_INTERP_OK(test_interp.convert_length_units(G_21, settings));
    REQUIRE(settings->length_units == CANON_UNITS_MM);
    // Magically jump 1 inch
    currentX(settings) = 25.4;
    REQUIRE_INTERP_OK(test_interp.convert_length_units(G_20, settings));
    REQUIRE(settings->length_units == CANON_UNITS_INCHES);
    REQUIRE_FUZZ(currentX(settings), 1.0);
    REQUIRE_INTERP_OK(test_interp.convert_length_units(G_21, settings));
  }

  SECTION("G52 without rotation")
  {
    // Assume all offsets and such start at zero
    REQUIRE(settings->length_units == CANON_UNITS_MM);
    REQUIRE(settings->parameters[G92_X] == 0.0);
    REQUIRE(settings->parameters[G92_Y] == 0.0);
    REQUIRE_INTERP_OK(test_interp.execute("G52 X25.4 Y0"));
    REQUIRE_FUZZ(settings->parameters[G92_X], 1.0);
    REQUIRE_FUZZ(settings->parameters[G92_Y], 0.0);
    REQUIRE_INTERP_OK(test_interp.execute("G52 Y25.4"));
    REQUIRE_FUZZ(settings->parameters[G92_X], 1.0);
    REQUIRE_FUZZ(settings->parameters[G92_Y], 1.0);
  }

  SECTION("G92 X and G5x Rotation")
  {
    // Assume all offsets and such start at zero
    REQUIRE_INTERP_OK(test_interp.execute("G20"));
    REQUIRE(settings->length_units == CANON_UNITS_INCHES);
    REQUIRE_INTERP_OK(test_interp.execute("G92.1"));
    REQUIRE(settings->parameters[G92_X] == 0.0);
    REQUIRE_INTERP_OK(test_interp.execute("G52 X1 Y0"));
    REQUIRE_INTERP_OK(test_interp.execute("G10 L2 P0 X1 Y0 Z0 R0"));
    REQUIRE_FUZZ(currentX(settings), -2.0);
    // FIXME this is the wrong behavior but what the interpreter currently expects
    REQUIRE_INTERP_OK(test_interp.execute("G10 L2 P0 X1 Y0 Z0 R90"));
    REQUIRE_FUZZ(currentX(settings), 0.0);
    REQUIRE_FUZZ(currentY(settings), 2.0);
  }

  SECTION("G92 off-axis behavior with rotation")
  {
    // Assume all offsets and such start at zero
    REQUIRE_INTERP_OK(test_interp.execute("G20"));
    REQUIRE(settings->length_units == CANON_UNITS_INCHES);
    REQUIRE_INTERP_OK(test_interp.execute("G92.1"));
    REQUIRE_INTERP_OK(test_interp.execute("G52 X1 Y0"));
    REQUIRE_INTERP_OK(test_interp.execute("G10 L2 P0 X1 Y2 Z3 A4 B5 C6 R90"));
    // FIXME this is the wrong behavior but what the interpreter currently expects
    REQUIRE_FUZZ(currentX(settings), -2.0);
    REQUIRE_FUZZ(currentY(settings), 2.0);
    // Should not be affected by rotation
    REQUIRE_FUZZ(currentZ(settings), -3.0);
    REQUIRE_FUZZ(currentA(settings), -4.0);
    REQUIRE_FUZZ(currentB(settings), -5.0);
    REQUIRE_FUZZ(currentC(settings), -6.0);
  }

  SECTION("G55 without rotation")
  {
    REQUIRE_FUZZ(currentX(settings), 0.0);
    REQUIRE_FUZZ(currentY(settings), 0.0);
    REQUIRE_FUZZ(currentZ(settings), 0.0);

    currentX(settings) = 1.0;
    currentY(settings) = 1.0;
    currentZ(settings) = 1.0;
    // KLUDGE hack in parameters directly to avoid depending on other functions
    test_interp._setup.parameters[G55_X] = 2;
    test_interp._setup.parameters[G55_Y] = 3;
    test_interp._setup.parameters[G55_Z] = 1;
    REQUIRE_INTERP_OK(test_interp.convert_coordinate_system(G_55, settings));
    REQUIRE_FUZZ(currentX(settings), -1.0);
    REQUIRE_FUZZ(currentY(settings), -2.0);
    REQUIRE_FUZZ(currentZ(settings), 0.0);
  }

  SECTION("G55 with rotation")
  {
    currentX(settings) = 0.0;
    // KLUDGE hack in parameters directly to avoid depending on other functions
    test_interp._setup.parameters[G55_X] = 2;
    test_interp._setup.parameters[G55_Y] = 3;
    test_interp._setup.parameters[G55_Z] = 1;
    test_interp._setup.parameters[G55_R] = 90;
    REQUIRE_INTERP_OK(test_interp.convert_coordinate_system(G_55, settings));
    REQUIRE_FUZZ(currentX(settings), -3.0);
    REQUIRE_FUZZ(currentY(settings), 2.0);
    REQUIRE_FUZZ(currentZ(settings), -1.0);
  }
}

TEST_CASE("G10 init")
{
  DECL_INIT_TEST_INTERP();

  const char *test_setup[] = {
    "g20",
    "g10 l2 p1 x0 y0 z0",
    "g54",
    "g10 l1 p1 x0 y0 z0",
    "t1 m6 g43",
  };
  execute_lines(test_interp, test_setup);
  REQUIRE_FUZZ(settings->parameters[TOOL_OFFSET_X], 0.0);
  REQUIRE_FUZZ(settings->parameters[TOOL_OFFSET_Y], 0.0);
  REQUIRE_FUZZ(settings->parameters[TOOL_OFFSET_Z], 0.0);

  SECTION("G10 L1 direct offsets")
  {
    // 2-pass loop ensures that tool offsets persist regardless of G43 / G49 state
    for (int k = 0; k < 2; ++k) {
      REQUIRE_INTERP_OK(test_interp.execute("g10 l1 p1 x0 y0 z0 "));
      REQUIRE_INTERP_OK(test_interp.execute("g10 l1 p1 x1"));
      REQUIRE_FUZZ(settings->parameters[TOOL_OFFSET_X], 1.0);
      REQUIRE_FUZZ(settings->parameters[TOOL_OFFSET_Y], 0.0);
      REQUIRE_FUZZ(settings->parameters[TOOL_OFFSET_Z], 0.0);

      REQUIRE_INTERP_OK(test_interp.execute("g10 l1 p1 y2"));
      REQUIRE_FUZZ(settings->parameters[TOOL_OFFSET_X], 1.0);
      REQUIRE_FUZZ(settings->parameters[TOOL_OFFSET_Y], 2.0);
      REQUIRE_FUZZ(settings->parameters[TOOL_OFFSET_Z], 0.0);

      REQUIRE_INTERP_OK(test_interp.execute("g10 l1 p1 z3"));
      REQUIRE_FUZZ(settings->parameters[TOOL_OFFSET_X], 1.0);
      REQUIRE_FUZZ(settings->parameters[TOOL_OFFSET_Y], 2.0);
      REQUIRE_FUZZ(settings->parameters[TOOL_OFFSET_Z], 3.0);

      REQUIRE_INTERP_OK(test_interp.execute("g49"));
    }
  }
}

TEST_CASE("G10 L10 ")
{
  DECL_INIT_TEST_INTERP();

  SECTION("tool offsets relative to position no rotation")
  {
  const char *test_setup[] = {
      "g20",
      "g10 l2 p1 x0 y0 z0",
      "g54",
      "g10 l1 p1 x0 y0 z0",
      "t1 m6 g43",
  };
  REQUIRE_INTERP_OK(execute_lines(test_interp, test_setup));

  REQUIRE_INTERP_OK(test_interp.execute("g0 x.1 y.2 z.3"));
  REQUIRE_INTERP_OK(test_interp.execute("g10 l10 p1 x1"));
  REQUIRE_FUZZ(settings->parameters[TOOL_OFFSET_X], -0.9);
  REQUIRE_FUZZ(settings->parameters[TOOL_OFFSET_Y], 0.0);
  REQUIRE_FUZZ(settings->parameters[TOOL_OFFSET_Z], 0.0);
  REQUIRE_INTERP_OK(test_interp.execute("g10 l10 p1 y2"));
  REQUIRE_FUZZ(settings->parameters[TOOL_OFFSET_X], -0.9);
  REQUIRE_FUZZ(settings->parameters[TOOL_OFFSET_Y], -1.8);
  REQUIRE_FUZZ(settings->parameters[TOOL_OFFSET_Z], 0.0);

  REQUIRE_INTERP_OK(test_interp.execute("g10 l10 p1 z3"));
  REQUIRE_FUZZ(settings->parameters[TOOL_OFFSET_X], -0.9);
  REQUIRE_FUZZ(settings->parameters[TOOL_OFFSET_Y], -1.8);
  REQUIRE_FUZZ(settings->parameters[TOOL_OFFSET_Z], -2.7);
}

SECTION("G10 L10 tool offsets relative to position + 45 deg rotation")
{

  const char *test_setup[] = {
      "g20",
      "g10 l2 p1 x0 y0 z0 r45",
      "g54",
      "g10 l1 p1 x0 y0 z0",
      "t1 m6 g43",
  };
  REQUIRE(execute_lines(test_interp, test_setup));

  REQUIRE_INTERP_OK(test_interp.execute("g0 x0 y0 z0"));
  REQUIRE_INTERP_OK(test_interp.execute("g10 l10 p1 x1"));
  REQUIRE_INTERP_OK(test_interp.execute("g43"));
  REQUIRE_FUZZ(settings->parameters[TOOL_OFFSET_X], -sqrt(2.0)/2.0);
  REQUIRE_FUZZ(settings->parameters[TOOL_OFFSET_Y], -sqrt(2.0)/2.0);
  REQUIRE_FUZZ(settings->parameters[TOOL_OFFSET_Z], 0.0);
  REQUIRE_FUZZ(currentX(settings), 1.0);
  REQUIRE_FUZZ(currentY(settings), 0.0);

  REQUIRE_INTERP_OK(test_interp.execute("g10 l10 p1 y1"));
  REQUIRE_INTERP_OK(test_interp.execute("g43"));
  REQUIRE_FUZZ(settings->parameters[TOOL_OFFSET_X], 0.0);
  REQUIRE_FUZZ(settings->parameters[TOOL_OFFSET_Y], -sqrt(2));
  REQUIRE_FUZZ(settings->parameters[TOOL_OFFSET_Z], 0.0);
  REQUIRE_FUZZ(currentX(settings), 1.0);
  REQUIRE_FUZZ(currentY(settings), 1.0);
  REQUIRE_INTERP_OK(test_interp.execute("g49"));
  REQUIRE_FUZZ(settings->parameters[TOOL_OFFSET_X], 0.0);
  REQUIRE_FUZZ(settings->parameters[TOOL_OFFSET_Y], -sqrt(2));
  REQUIRE_FUZZ(settings->parameters[TOOL_OFFSET_Z], 0.0);
  REQUIRE_FUZZ(currentX(settings), 0.0);
  REQUIRE_FUZZ(currentY(settings), 0.0);
}
}
