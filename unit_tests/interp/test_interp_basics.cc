#include "catch.hpp"

#include <interp_testing_util.hh> // For core interp stuff and extra REQUIRE macros/ setup
#include <rs274ngc_interp.hh>
#include <interp_inspection.hh>
#include <interp_return.hh>
#include <saicanon.hh>
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
    CHECK_FUZZ(currentX(settings), 1.0);
    REQUIRE_INTERP_OK(test_interp.convert_length_units(G_21, settings));
  }

  SECTION("G52 without rotation")
  {
    // Assume all offsets and such start at zero
    REQUIRE(settings->length_units == CANON_UNITS_MM);
    REQUIRE(settings->parameters[G92_X] == 0.0);
    REQUIRE(settings->parameters[G92_Y] == 0.0);
    REQUIRE_INTERP_OK(test_interp.execute("G52 X25.4 Y0"));
    CHECK_FUZZ(settings->parameters[G92_X], 1.0);
    CHECK_FUZZ(settings->parameters[G92_Y], 0.0);
    REQUIRE_INTERP_OK(test_interp.execute("G52 Y25.4"));
    CHECK_FUZZ(settings->parameters[G92_X], 1.0);
    CHECK_FUZZ(settings->parameters[G92_Y], 1.0);
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
    CHECK_FUZZ(currentX(settings), -2.0);
    // FIXME this is the wrong behavior but what the interpreter currently expects
    REQUIRE_INTERP_OK(test_interp.execute("G10 L2 P0 X1 Y0 Z0 R90"));
    CHECK_FUZZ(currentX(settings), -0.0);
    CHECK_FUZZ(currentY(settings), 2.0);
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
    CHECK_FUZZ(currentX(settings), -2.0);
    CHECK_FUZZ(currentY(settings), 2.0);
    // Should not be affected by rotation
    CHECK_FUZZ(currentZ(settings), -3.0);
    CHECK_FUZZ(currentA(settings), -4.0);
    CHECK_FUZZ(currentB(settings), -5.0);
    CHECK_FUZZ(currentC(settings), -6.0);
  }

  SECTION("G55 without rotation")
  {
    REQUIRE_INTERP_OK(test_interp.execute("G20"));
    CHECK_FUZZ(currentX(settings), 0.0);
    CHECK_FUZZ(currentY(settings), 0.0);
    CHECK_FUZZ(currentZ(settings), 0.0);

    currentX(settings) = 1.0;
    currentY(settings) = 1.0;
    currentZ(settings) = 1.0;
    // KLUDGE hack in parameters directly to avoid depending on other functions
    test_interp._setup.parameters[G55_X] = 2;
    test_interp._setup.parameters[G55_Y] = 3;
    test_interp._setup.parameters[G55_Z] = 1;
    REQUIRE_INTERP_OK(test_interp.convert_coordinate_system(G_55, settings));
    CHECK_FUZZ(currentX(settings), -1.0);
    CHECK_FUZZ(currentY(settings), -2.0);
    CHECK_FUZZ(currentZ(settings), 0.0);
  }

  SECTION("G55 with rotation")
  {
    REQUIRE_INTERP_OK(test_interp.execute("G20"));
    currentX(settings) = 0.0;
    // KLUDGE hack in parameters directly to avoid depending on other functions
    test_interp._setup.parameters[G55_X] = 2;
    test_interp._setup.parameters[G55_Y] = 3;
    test_interp._setup.parameters[G55_Z] = 1;
    test_interp._setup.parameters[G55_R] = 90;
    REQUIRE_INTERP_OK(test_interp.convert_coordinate_system(G_55, settings));
    CHECK_FUZZ(currentX(settings), -3.0);
    CHECK_FUZZ(currentY(settings), 2.0);
    CHECK_FUZZ(currentZ(settings), -1.0);
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
  CHECK_FUZZ(settings->parameters[TOOL_OFFSET_X], 0.0);
  CHECK_FUZZ(settings->parameters[TOOL_OFFSET_Y], 0.0);
  CHECK_FUZZ(settings->parameters[TOOL_OFFSET_Z], 0.0);

  SECTION("G10 L1 direct offsets")
  {
    // 2-pass loop ensures that tool offsets persist regardless of G43 / G49 state
    for (int k = 0; k < 2; ++k) {
      REQUIRE_INTERP_OK(test_interp.execute("g10 l1 p1 x0 y0 z0 "));
      REQUIRE_INTERP_OK(test_interp.execute("g10 l1 p1 x1"));
      CHECK_FUZZ(settings->parameters[TOOL_OFFSET_X], 1.0);
      CHECK_FUZZ(settings->parameters[TOOL_OFFSET_Y], 0.0);
      CHECK_FUZZ(settings->parameters[TOOL_OFFSET_Z], 0.0);

      REQUIRE_INTERP_OK(test_interp.execute("g10 l1 p1 y2"));
      CHECK_FUZZ(settings->parameters[TOOL_OFFSET_X], 1.0);
      CHECK_FUZZ(settings->parameters[TOOL_OFFSET_Y], 2.0);
      CHECK_FUZZ(settings->parameters[TOOL_OFFSET_Z], 0.0);

      REQUIRE_INTERP_OK(test_interp.execute("g10 l1 p1 z3"));
      CHECK_FUZZ(settings->parameters[TOOL_OFFSET_X], 1.0);
      CHECK_FUZZ(settings->parameters[TOOL_OFFSET_Y], 2.0);
      CHECK_FUZZ(settings->parameters[TOOL_OFFSET_Z], 3.0);

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
  CHECK_FUZZ(settings->parameters[TOOL_OFFSET_X], -0.9);
  CHECK_FUZZ(settings->parameters[TOOL_OFFSET_Y], 0.0);
  CHECK_FUZZ(settings->parameters[TOOL_OFFSET_Z], 0.0);
  REQUIRE_INTERP_OK(test_interp.execute("g10 l10 p1 y2"));
  CHECK_FUZZ(settings->parameters[TOOL_OFFSET_X], -0.9);
  CHECK_FUZZ(settings->parameters[TOOL_OFFSET_Y], -1.8);
  CHECK_FUZZ(settings->parameters[TOOL_OFFSET_Z], 0.0);

  REQUIRE_INTERP_OK(test_interp.execute("g10 l10 p1 z3"));
  CHECK_FUZZ(settings->parameters[TOOL_OFFSET_X], -0.9);
  CHECK_FUZZ(settings->parameters[TOOL_OFFSET_Y], -1.8);
  CHECK_FUZZ(settings->parameters[TOOL_OFFSET_Z], -2.7);
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
  CHECK_FUZZ(settings->parameters[TOOL_OFFSET_X], -sqrt(2.0)/2.0);
  CHECK_FUZZ(settings->parameters[TOOL_OFFSET_Y], -sqrt(2.0)/2.0);
  CHECK_FUZZ(settings->parameters[TOOL_OFFSET_Z], 0.0);
  CHECK_FUZZ(currentX(settings), 1.0);
  CHECK_FUZZ(currentY(settings), 0.0);

  REQUIRE_INTERP_OK(test_interp.execute("g10 l10 p1 y1"));
  REQUIRE_INTERP_OK(test_interp.execute("g43"));
  CHECK_FUZZ(settings->parameters[TOOL_OFFSET_X], 0.0);
  CHECK_FUZZ(settings->parameters[TOOL_OFFSET_Y], -sqrt(2));
  CHECK_FUZZ(settings->parameters[TOOL_OFFSET_Z], 0.0);
  CHECK_FUZZ(currentX(settings), 1.0);
  CHECK_FUZZ(currentY(settings), 1.0);
  REQUIRE_INTERP_OK(test_interp.execute("g49"));
  CHECK_FUZZ(settings->parameters[TOOL_OFFSET_X], 0.0);
  CHECK_FUZZ(settings->parameters[TOOL_OFFSET_Y], -sqrt(2));
  CHECK_FUZZ(settings->parameters[TOOL_OFFSET_Z], 0.0);
  CHECK_FUZZ(currentX(settings), 0.0);
  CHECK_FUZZ(currentY(settings), 0.0);
}
}

SCENARIO("Call G10 with G92 active (based on runtest g10-with-g92)")
{
  GIVEN("No G92 offsets applied")
  {
    DECL_INIT_TEST_INTERP();
    REQUIRE_INTERP_OK(test_interp.execute("g20"));
    REQUIRE(settings->length_units == CANON_UNITS_INCHES);
    WHEN("No offsets are applied") {
      THEN("Both current position and current offset should be zero") {
        CHECK_FUZZ(currentX(settings), 0.0);
        CHECK_FUZZ(currentY(settings), 0.0);
        CHECK_FUZZ(currentZ(settings), 0.0);
        CHECK_FUZZ(currentWorkOffsetX(settings), 0);
        CHECK_FUZZ(currentWorkOffsetY(settings), 0);
        CHECK_FUZZ(currentWorkOffsetZ(settings), 0);
        CHECK_FUZZ(currentWorkOffsetA(settings), 0);
        CHECK_FUZZ(currentWorkOffsetB(settings), 0);
        CHECK_FUZZ(currentWorkOffsetC(settings), 0);
      }
    }
    WHEN("Work offsets are specified via G10 L2 for the active coordinate system")
    {
      REQUIRE_INTERP_OK(test_interp.execute("g10 l2 p0 x7 y8 z9 a10 b11 c12"));
      THEN ("New offset values are stored and immediately applied")
      {
        CHECK_FUZZ(settings->origin_index, 1); // Confirm it's G54
        CHECK_FUZZ(settings->parameters[G54_X], 7);
        CHECK_FUZZ(settings->parameters[G54_Y], 8);
        CHECK_FUZZ(settings->parameters[G54_Z], 9);
        CHECK_FUZZ(settings->parameters[G54_A], 10);
        CHECK_FUZZ(settings->parameters[G54_B], 11);
        CHECK_FUZZ(settings->parameters[G54_C], 12);
        CHECK_FUZZ(currentWorkOffsetX(settings), 7);
        CHECK_FUZZ(currentWorkOffsetY(settings), 8);
        CHECK_FUZZ(currentWorkOffsetZ(settings), 9);
        CHECK_FUZZ(currentWorkOffsetA(settings), 10);
        CHECK_FUZZ(currentWorkOffsetB(settings), 11);
        CHECK_FUZZ(currentWorkOffsetC(settings), 12);

        CHECK_FUZZ(currentX(settings), -7);
        CHECK_FUZZ(currentY(settings), -8);
        CHECK_FUZZ(currentZ(settings), -9);
        CHECK_FUZZ(currentA(settings), -10);
        CHECK_FUZZ(currentB(settings), -11);
        CHECK_FUZZ(currentC(settings), -12);
      }
    }
    WHEN("Work offsets are specified via G10 L2 for inactive coordinate systems")
    {
      REQUIRE_INTERP_OK(test_interp.execute("G54"));
      REQUIRE_INTERP_OK(test_interp.execute("g10 l2 p2 x7 y8 z9 a10 b11 c12"));
      CHECK_FUZZ(settings->origin_index, 1); // Confirm it's G54
      THEN("Work offset parameters are updated but the current offset is not (nor is the current position)") {
        CHECK_FUZZ(settings->parameters[G55_X], 7);
        CHECK_FUZZ(settings->parameters[G55_Y], 8);
        CHECK_FUZZ(settings->parameters[G55_Z], 9);
        CHECK_FUZZ(settings->parameters[G55_A], 10);
        CHECK_FUZZ(settings->parameters[G55_B], 11);
        CHECK_FUZZ(settings->parameters[G55_C], 12);

        CHECK_FUZZ(currentWorkOffsetX(settings), 0);
        CHECK_FUZZ(currentWorkOffsetY(settings), 0);
        CHECK_FUZZ(currentWorkOffsetZ(settings), 0);
        CHECK_FUZZ(currentWorkOffsetA(settings), 0);
        CHECK_FUZZ(currentWorkOffsetB(settings), 0);
        CHECK_FUZZ(currentWorkOffsetC(settings), 0);

        CHECK_FUZZ(currentX(settings), 0);
        CHECK_FUZZ(currentY(settings), 0);
        CHECK_FUZZ(currentZ(settings), 0);
        CHECK_FUZZ(currentA(settings), 0);
        CHECK_FUZZ(currentB(settings), 0);
        CHECK_FUZZ(currentC(settings), 0);
      }
    }
    WHEN("Work offsets are specified via G10 L2 for the active coordinate system")
    {
      REQUIRE_INTERP_OK(test_interp.execute("g10 l2 p0 x7 y8 z9 a10 b11 c12"));
      THEN ("New offset values are stored and immediately applied")
      {
        CHECK_FUZZ(settings->origin_index, 1); // Confirm it's G54
        CHECK_FUZZ(settings->parameters[G54_X], 7);
        CHECK_FUZZ(settings->parameters[G54_Y], 8);
        CHECK_FUZZ(settings->parameters[G54_Z], 9);
        CHECK_FUZZ(settings->parameters[G54_A], 10);
        CHECK_FUZZ(settings->parameters[G54_B], 11);
        CHECK_FUZZ(settings->parameters[G54_C], 12);

        CHECK_FUZZ(currentWorkOffsetX(settings), 7);
        CHECK_FUZZ(currentWorkOffsetY(settings), 8);
        CHECK_FUZZ(currentWorkOffsetZ(settings), 9);
        CHECK_FUZZ(currentWorkOffsetA(settings), 10);
        CHECK_FUZZ(currentWorkOffsetB(settings), 11);
        CHECK_FUZZ(currentWorkOffsetC(settings), 12);

        CHECK_FUZZ(currentX(settings), -7);
        CHECK_FUZZ(currentY(settings), -8);
        CHECK_FUZZ(currentZ(settings), -9);
        CHECK_FUZZ(currentA(settings), -10);
        CHECK_FUZZ(currentB(settings), -11);
        CHECK_FUZZ(currentC(settings), -12);
      }
    }
  }
  GIVEN("G92 offsets active")
  {
    DECL_INIT_TEST_INTERP();
    REQUIRE_INTERP_OK(test_interp.execute("g20"));
    REQUIRE(settings->length_units == CANON_UNITS_INCHES);
    REQUIRE_INTERP_OK(test_interp.execute("g92 x3 y4 z5"));
    constexpr double axis_offset_x = -3;
    constexpr double axis_offset_y = -4;

    WHEN("No work offsets are applied") {
      THEN("Both current position and current offset should only be due to G92") {
        CHECK_FUZZ(currentX(settings), -axis_offset_x);
        CHECK_FUZZ(currentY(settings), -axis_offset_y);
        CHECK_FUZZ(currentZ(settings), 5);

        CHECK_FUZZ(currentWorkOffsetX(settings), 0);
        CHECK_FUZZ(currentWorkOffsetY(settings), 0);
        CHECK_FUZZ(currentWorkOffsetZ(settings), 0);
        CHECK_FUZZ(currentWorkOffsetA(settings), 0);
        CHECK_FUZZ(currentWorkOffsetB(settings), 0);
        CHECK_FUZZ(currentWorkOffsetC(settings), 0);
      }
    }
    WHEN("Work offsets are specified via G10 L2 for the active coordinate system with rotation")
    {
      constexpr double work_offset_x = 7;
      constexpr double work_offset_y = 8;

      REQUIRE_INTERP_OK(test_interp.execute("g10 l2 p0 x7 y8 z9 a10 b11 c12 R45"));
      THEN ("Current position will be rotated in X/Y")
      {
        CHECK_FUZZ(settings->origin_index, 1); // Confirm it's G54
        CHECK_FUZZ(settings->parameters[G54_X], work_offset_x);
        CHECK_FUZZ(settings->parameters[G54_Y], work_offset_y);
        CHECK_FUZZ(settings->parameters[G54_Z], 9);
        CHECK_FUZZ(settings->parameters[G54_A], 10);
        CHECK_FUZZ(settings->parameters[G54_B], 11);
        CHECK_FUZZ(settings->parameters[G54_C], 12);

        CHECK_FUZZ(currentWorkOffsetX(settings), work_offset_x);
        CHECK_FUZZ(currentWorkOffsetY(settings), work_offset_y);
        CHECK_FUZZ(currentWorkOffsetZ(settings), 9);
        CHECK_FUZZ(currentWorkOffsetA(settings), 10);
        CHECK_FUZZ(currentWorkOffsetB(settings), 11);
        CHECK_FUZZ(currentWorkOffsetC(settings), 12);

        constexpr double xy_rotation = M_PI / 4;
        // FIXME this math is wrong but what the current interpreter expects
        const double total_offset_x = axis_offset_x + work_offset_x;
        const double total_offset_y = axis_offset_y + work_offset_y;
        const double expect_x = (cos(-xy_rotation) * -total_offset_x + -sin(-xy_rotation) * -total_offset_y);
        const double expect_y = (sin(-xy_rotation) * -total_offset_x + cos(-xy_rotation) * -total_offset_y);
        CHECK_FUZZ(currentX(settings), expect_x);
        CHECK_FUZZ(currentY(settings), expect_y);
        CHECK_FUZZ(currentZ(settings), -4);
        CHECK_FUZZ(currentA(settings), -10);
        CHECK_FUZZ(currentB(settings), -11);
        CHECK_FUZZ(currentC(settings), -12);
      }
    }
  }
}

SCENARIO("Save / restore of G92 parameters")
{
  GIVEN("Active G92 Offsets")
  {
    DECL_INIT_TEST_INTERP();
    REQUIRE_INTERP_OK(test_interp.execute("g20"));
    REQUIRE(settings->length_units == CANON_UNITS_INCHES);
    REQUIRE_INTERP_OK(test_interp.execute("g92 x3 y4 z5"));
    constexpr double axis_offset_x = -3;
    constexpr double axis_offset_y = -4;
    constexpr double axis_offset_z = -5;
    CHECK_FUZZ(settings->parameters[G92_X], axis_offset_x);
    CHECK_FUZZ(settings->parameters[G92_Y], axis_offset_y);
    CHECK_FUZZ(settings->parameters[G92_Z], axis_offset_z);

    WHEN("Offsets are disabled with G92.2") {
      REQUIRE_INTERP_OK(test_interp.execute("g92.2"));
      THEN("Current position changes but internal offset parameters are unaffected") {
        CHECK_FUZZ(currentX(settings), 0);
        CHECK_FUZZ(currentY(settings), 0);
        CHECK_FUZZ(currentZ(settings), 0);

        CHECK_FUZZ(settings->parameters[G92_X], axis_offset_x);
        CHECK_FUZZ(settings->parameters[G92_Y], axis_offset_y);
        CHECK_FUZZ(settings->parameters[G92_Z], axis_offset_z);

        WHEN("Offsets are re-enabled with G92.3") {
          REQUIRE_INTERP_OK(test_interp.execute("g92.3"));
          THEN("Original position and offsets are restored") {
            CHECK_FUZZ(currentX(settings), -axis_offset_x);
            CHECK_FUZZ(currentY(settings), -axis_offset_y);
            CHECK_FUZZ(currentZ(settings), -axis_offset_z);

            CHECK_FUZZ(settings->parameters[G92_X], axis_offset_x);
            CHECK_FUZZ(settings->parameters[G92_Y], axis_offset_y);
            CHECK_FUZZ(settings->parameters[G92_Z], axis_offset_z);
          }
        }
      }
    }
  }
}


SCENARIO("Convert G20 / G21 ")
{
  GIVEN("Starting in G20")
  {
    DECL_INIT_TEST_INTERP();
    REQUIRE_INTERP_OK(test_interp.execute("g20"));
    REQUIRE_INTERP_OK(test_interp.execute("g0 x1 y2 z3 a4 b5 c6"));

    CHECK_FUZZ(currentX(settings), 1);
    CHECK_FUZZ(currentY(settings), 2);
    CHECK_FUZZ(currentZ(settings), 3);
    CHECK_FUZZ(currentA(settings), 4);
    CHECK_FUZZ(currentB(settings), 5);
    CHECK_FUZZ(currentC(settings), 6);
    REQUIRE(settings->length_units == CANON_UNITS_INCHES);
    WHEN("Convert current position to mm") {
      REQUIRE_INTERP_OK(test_interp.execute("g21"));
      REQUIRE(settings->length_units == CANON_UNITS_MM);
      THEN("urrent position should be in new program units") {
        CHECK_FUZZ(currentX(settings), 1 * 25.4);
        CHECK_FUZZ(currentY(settings), 2 * 25.4);
        CHECK_FUZZ(currentZ(settings), 3 * 25.4);
        CHECK_FUZZ(currentA(settings), 4);
        CHECK_FUZZ(currentB(settings), 5);
        CHECK_FUZZ(currentC(settings), 6);
      }
      WHEN("Convert to back to inches") {
        REQUIRE_INTERP_OK(test_interp.execute("g20"));
        REQUIRE(settings->length_units == CANON_UNITS_INCHES);
        THEN("current position returns to inch value") {
          CHECK_FUZZ(currentX(settings), 1);
          CHECK_FUZZ(currentY(settings), 2);
          CHECK_FUZZ(currentZ(settings), 3);
          CHECK_FUZZ(currentA(settings), 4);
          CHECK_FUZZ(currentB(settings), 5);
          CHECK_FUZZ(currentC(settings), 6);
        }
      }
    }

    WHEN("Switch to G21") {
      REQUIRE_INTERP_OK(test_interp.execute("g10 L2 P1 X1 Y2 Z3 A4 B5 C6"));
      REQUIRE_INTERP_OK(test_interp.execute("g54"));
      REQUIRE_INTERP_OK(test_interp.execute("g0 x0.5 y0.5"));
      REQUIRE_INTERP_OK(test_interp.execute("g21"));
      REQUIRE(settings->length_units == CANON_UNITS_MM);
      THEN("Current work offset in mm") {
        CHECK_FUZZ(currentWorkOffsetX(settings), 1 * 25.4);
        CHECK_FUZZ(currentWorkOffsetY(settings), 2 * 25.4);
        CHECK_FUZZ(currentWorkOffsetZ(settings), 3 * 25.4);
        CHECK_FUZZ(currentWorkOffsetA(settings), 4);
        CHECK_FUZZ(currentWorkOffsetB(settings), 5);
        CHECK_FUZZ(currentWorkOffsetC(settings), 6);
      }
      WHEN("Switch to back to G20") {
        REQUIRE_INTERP_OK(test_interp.execute("g20"));
        REQUIRE(settings->length_units == CANON_UNITS_INCHES);
        THEN("current work offset returns to inches") {
          CHECK_FUZZ(currentWorkOffsetX(settings), 1);
          CHECK_FUZZ(currentWorkOffsetY(settings), 2);
          CHECK_FUZZ(currentWorkOffsetZ(settings), 3);
          CHECK_FUZZ(currentWorkOffsetA(settings), 4);
          CHECK_FUZZ(currentWorkOffsetB(settings), 5);
          CHECK_FUZZ(currentWorkOffsetC(settings), 6);
        }
      }
    }
  }

  GIVEN("Starting in with an axis offset")
  {
    DECL_INIT_TEST_INTERP();
    REQUIRE_INTERP_OK(test_interp.execute("g20"));
    REQUIRE_INTERP_OK(test_interp.execute("g92 x1 y2 z3 a4 b5 c6"));

    THEN("Initial axis offsets should be in inches") {
      CHECK_FUZZ(currentAxisOffsetX(settings), -1);
      CHECK_FUZZ(currentAxisOffsetY(settings), -2);
      CHECK_FUZZ(currentAxisOffsetZ(settings), -3);
      CHECK_FUZZ(currentAxisOffsetA(settings), -4);
      CHECK_FUZZ(currentAxisOffsetB(settings), -5);
      CHECK_FUZZ(currentAxisOffsetC(settings), -6);
    }
    WHEN("Switch to G21") {
      REQUIRE_INTERP_OK(test_interp.execute("g21"));
      REQUIRE(settings->length_units == CANON_UNITS_MM);
      THEN("Axis offsets in mm") {
        CHECK_FUZZ(currentAxisOffsetX(settings), -1 * 25.4);
        CHECK_FUZZ(currentAxisOffsetY(settings), -2 * 25.4);
        CHECK_FUZZ(currentAxisOffsetZ(settings), -3 * 25.4);
        CHECK_FUZZ(currentAxisOffsetA(settings), -4);
        CHECK_FUZZ(currentAxisOffsetB(settings), -5);
        CHECK_FUZZ(currentAxisOffsetC(settings), -6);
      }
      WHEN("Switch to back to G20") {
        REQUIRE_INTERP_OK(test_interp.execute("g20"));
        REQUIRE(settings->length_units == CANON_UNITS_INCHES);
        THEN("Axis offsets in inches again") {
          CHECK_FUZZ(currentAxisOffsetX(settings), -1);
          CHECK_FUZZ(currentAxisOffsetY(settings), -2);
          CHECK_FUZZ(currentAxisOffsetZ(settings), -3);
          CHECK_FUZZ(currentAxisOffsetA(settings), -4);
          CHECK_FUZZ(currentAxisOffsetB(settings), -5);
          CHECK_FUZZ(currentAxisOffsetC(settings), -6);
        }
      }
    }
  }
}

SCENARIO("Applying a work offset while active")
{
  GIVEN("A 45 deg rotated coordinate system with no offsets")
  {
    DECL_INIT_TEST_INTERP();
    REQUIRE_INTERP_OK(test_interp.execute("g20"));
    REQUIRE(settings->length_units == CANON_UNITS_INCHES);
    REQUIRE_INTERP_OK(test_interp.execute("g54"));
    REQUIRE_INTERP_OK(test_interp.execute("g10 l2 p1 x0 y0 z0 r45"));
    REQUIRE_INTERP_OK(test_interp.execute("g0 g53 x0 y0 z0"));
    REQUIRE(currentX(settings) == 0.0);
    REQUIRE(currentY(settings) == 0.0);
    REQUIRE(currentZ(settings) == 0.0);

    WHEN("Request L20 for an X offset") {
      REQUIRE_INTERP_OK(test_interp.execute("g10 l20 p1 x-1"));
      THEN("Actual work offsets are rotated") {
          CHECK_FUZZ(currentWorkOffsetX(settings), sqrt(2)/2);
          CHECK_FUZZ(currentWorkOffsetY(settings), sqrt(2)/2);
          CHECK_FUZZ(currentWorkOffsetZ(settings), 0);
      }

      WHEN("Move to X0 and request a Y offset") {
        REQUIRE_INTERP_OK(test_interp.execute("g0 x0"));
        REQUIRE_INTERP_OK(test_interp.execute("g10 l20 p1 y-1"));
        THEN("Actual work offsets are rotated") {
          CHECK_FUZZ(currentWorkOffsetX(settings), 0.0);
          CHECK_FUZZ(currentWorkOffsetY(settings), sqrt(2));
          CHECK_FUZZ(currentWorkOffsetZ(settings), 0);
        }
      }
    }
  }
}

