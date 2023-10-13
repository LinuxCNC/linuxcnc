#include "velocityprofiletest.hpp"
#include <frames_io.hpp>
CPPUNIT_TEST_SUITE_REGISTRATION( VelocityProfileTest );

using namespace KDL;

void VelocityProfileTest::setUp()
{
}

void VelocityProfileTest::tearDown()
{
}

void VelocityProfileTest::TestTrap_MaxVelocity1()
{
	// 2 second ramp up (cover 2 distance),
	// 2 second flat velocity (cover 4 distance)
	// 2 second ramp down (cover 2 distance),
	VelocityProfile_Trap	v(2, 1);
	double					time;
	v.SetProfile(2, 10);
	CPPUNIT_ASSERT_DOUBLES_EQUAL(6.0, v.Duration(),epsilon);

	// start
	time = 0;
	CPPUNIT_ASSERT_DOUBLES_EQUAL(2.0, v.Pos(time),epsilon);
	CPPUNIT_ASSERT_DOUBLES_EQUAL(0.0, v.Vel(time),epsilon);
	CPPUNIT_ASSERT_DOUBLES_EQUAL(1.0, v.Acc(time),epsilon);

	// end of ramp up
	time = 2;
	CPPUNIT_ASSERT_DOUBLES_EQUAL(4.0, v.Pos(time),epsilon);
	CPPUNIT_ASSERT_DOUBLES_EQUAL(2.0, v.Vel(time),epsilon);
	CPPUNIT_ASSERT_DOUBLES_EQUAL(0.0, v.Acc(time),epsilon);

	// middle of flat velocity
	time = 3;
	CPPUNIT_ASSERT_DOUBLES_EQUAL(6.0, v.Pos(time),epsilon);
	CPPUNIT_ASSERT_DOUBLES_EQUAL(2.0, v.Vel(time),epsilon);
	CPPUNIT_ASSERT_DOUBLES_EQUAL(0.0, v.Acc(time),epsilon);

	// end of flat velocity
	time = 4;
	CPPUNIT_ASSERT_DOUBLES_EQUAL(8.0, v.Pos(time),epsilon);
	CPPUNIT_ASSERT_DOUBLES_EQUAL(2.0, v.Vel(time),epsilon);
	CPPUNIT_ASSERT_DOUBLES_EQUAL(-1.0, v.Acc(time),epsilon);

	// middle of ramp down
	time = 5;
	CPPUNIT_ASSERT_DOUBLES_EQUAL(9.5, v.Pos(time),epsilon);
	CPPUNIT_ASSERT_DOUBLES_EQUAL(1.0, v.Vel(time),epsilon);
	CPPUNIT_ASSERT_DOUBLES_EQUAL(-1.0, v.Acc(time),epsilon);

	// end
	time = 6;
	CPPUNIT_ASSERT_DOUBLES_EQUAL(10.0, v.Pos(time),epsilon);
	CPPUNIT_ASSERT_DOUBLES_EQUAL(0.0, v.Vel(time),epsilon);
	CPPUNIT_ASSERT_DOUBLES_EQUAL(-1.0, v.Acc(time),epsilon);

	// fenceposts - before and after
	time = -1;
	CPPUNIT_ASSERT_DOUBLES_EQUAL(2.0, v.Pos(time),epsilon);
	CPPUNIT_ASSERT_DOUBLES_EQUAL(0.0, v.Vel(time),epsilon);
	CPPUNIT_ASSERT_DOUBLES_EQUAL(0.0, v.Acc(time),epsilon);
	time = 11;
	CPPUNIT_ASSERT_DOUBLES_EQUAL(10.0, v.Pos(time),epsilon);
	CPPUNIT_ASSERT_DOUBLES_EQUAL(0.0, v.Vel(time),epsilon);
	CPPUNIT_ASSERT_DOUBLES_EQUAL(0.0, v.Acc(time),epsilon);
}

void VelocityProfileTest::TestTrap_MaxVelocity2()
{
	// 2 second ramp up (cover -2 distance),
	// 2 second flat velocity (cover -4 distance)
	// 2 second ramp down (cover -2 distance),
	VelocityProfile_Trap	v(2, 1);
	v.SetProfile(2, -6);
	CPPUNIT_ASSERT_DOUBLES_EQUAL(6.0, v.Duration(),epsilon);
}

void VelocityProfileTest::TestTrap_MaxVelocity3()
{
	// 2 second ramp up (cover 4 distance),
	// 0 second flat velocity (cover 0 distance)
	// 2 second ramp down (cover 4 distance),
	VelocityProfile_Trap	v(4, 2);
	v.SetProfile(2, 10);
	CPPUNIT_ASSERT_DOUBLES_EQUAL(4.0, v.Duration(),epsilon);

	// new profile
	v.SetProfile(2, -6);
	CPPUNIT_ASSERT_DOUBLES_EQUAL(4.0, v.Duration(),epsilon);

	// another new profile : ramp + 2 sec + ramp
	v.SetProfile(13, 13 + 4 + 8 + 4);
	CPPUNIT_ASSERT_DOUBLES_EQUAL(6.0, v.Duration(),epsilon);
}

void VelocityProfileTest::TestTrap_SetDuration1()
{
	// same as first max velocity test, but twice as
	// long (max velocity gives 6 seconds)
	VelocityProfile_Trap	v(2, 1);
	double					time;
	v.SetProfileDuration(2, 10, 12.0);
	CPPUNIT_ASSERT_DOUBLES_EQUAL(12.0, v.Duration(),epsilon);

	// start
	time = 0;
	CPPUNIT_ASSERT_DOUBLES_EQUAL(2.0, v.Pos(time),epsilon);
	CPPUNIT_ASSERT_DOUBLES_EQUAL(0.0, v.Vel(time),epsilon);
	CPPUNIT_ASSERT_DOUBLES_EQUAL(0.25, v.Acc(time),epsilon);

	// end of ramp up
	time = 4;
	CPPUNIT_ASSERT_DOUBLES_EQUAL(4.0, v.Pos(time),epsilon);
	CPPUNIT_ASSERT_DOUBLES_EQUAL(1.0, v.Vel(time),epsilon);
	CPPUNIT_ASSERT_DOUBLES_EQUAL(0.0, v.Acc(time),epsilon);

	// middle of flat velocity
	time = 6;
	CPPUNIT_ASSERT_DOUBLES_EQUAL(6.0, v.Pos(time),epsilon);
	CPPUNIT_ASSERT_DOUBLES_EQUAL(1.0, v.Vel(time),epsilon);
	CPPUNIT_ASSERT_DOUBLES_EQUAL(0.0, v.Acc(time),epsilon);

	// end of flat velocity
	time = 8;
	CPPUNIT_ASSERT_DOUBLES_EQUAL(8.0, v.Pos(time),epsilon);
	CPPUNIT_ASSERT_DOUBLES_EQUAL(1.0, v.Vel(time),epsilon);
	CPPUNIT_ASSERT_DOUBLES_EQUAL(-0.25, v.Acc(time),epsilon);

	// middle of ramp down
	time = 10;
	CPPUNIT_ASSERT_DOUBLES_EQUAL(9.5, v.Pos(time),epsilon);
	CPPUNIT_ASSERT_DOUBLES_EQUAL(0.5, v.Vel(time),epsilon);
	CPPUNIT_ASSERT_DOUBLES_EQUAL(-0.25, v.Acc(time),epsilon);

	// end
	time = 12;
	CPPUNIT_ASSERT_DOUBLES_EQUAL(10.0, v.Pos(time),epsilon);
	CPPUNIT_ASSERT_DOUBLES_EQUAL(0.0, v.Vel(time),epsilon);
	CPPUNIT_ASSERT_DOUBLES_EQUAL(-0.25, v.Acc(time),epsilon);
}

void VelocityProfileTest::TestTrapHalf_SetProfile_Start()
{
	// 2 second ramp up (cover 2 distance),
	// 2 second flat velocity (cover 4 distance)
	VelocityProfile_TrapHalf	v(2, 1, true);
	double						time;
	v.SetProfile(2, 2+6);
	CPPUNIT_ASSERT_DOUBLES_EQUAL(4.0, v.Duration(),epsilon);

	// start
	time = 0;
	CPPUNIT_ASSERT_DOUBLES_EQUAL(2.0, v.Pos(time),epsilon);
	CPPUNIT_ASSERT_DOUBLES_EQUAL(0.0, v.Vel(time),epsilon);
	CPPUNIT_ASSERT_DOUBLES_EQUAL(1.0, v.Acc(time),epsilon);

	// end of ramp up
	time = 2;
	CPPUNIT_ASSERT_DOUBLES_EQUAL(4.0, v.Pos(time),epsilon);
	CPPUNIT_ASSERT_DOUBLES_EQUAL(2.0, v.Vel(time),epsilon);
	CPPUNIT_ASSERT_DOUBLES_EQUAL(0.0, v.Acc(time),epsilon);

	// middle of flat velocity
	time = 3;
	CPPUNIT_ASSERT_DOUBLES_EQUAL(6.0, v.Pos(time),epsilon);
	CPPUNIT_ASSERT_DOUBLES_EQUAL(2.0, v.Vel(time),epsilon);
	CPPUNIT_ASSERT_DOUBLES_EQUAL(0.0, v.Acc(time),epsilon);

	// end
	time = 4;
	CPPUNIT_ASSERT_DOUBLES_EQUAL(8.0, v.Pos(time),epsilon);
	CPPUNIT_ASSERT_DOUBLES_EQUAL(2.0, v.Vel(time),epsilon);
	CPPUNIT_ASSERT_DOUBLES_EQUAL(0.0, v.Acc(time),epsilon);

	// fenceposts - before and after
	time = -1;
	CPPUNIT_ASSERT_DOUBLES_EQUAL(2.0, v.Pos(time),epsilon);
	CPPUNIT_ASSERT_DOUBLES_EQUAL(0.0, v.Vel(time),epsilon);
	CPPUNIT_ASSERT_DOUBLES_EQUAL(0.0, v.Acc(time),epsilon);
	time = 5;
	CPPUNIT_ASSERT_DOUBLES_EQUAL(8.0, v.Pos(time),epsilon);
	CPPUNIT_ASSERT_DOUBLES_EQUAL(0.0, v.Vel(time),epsilon);
	CPPUNIT_ASSERT_DOUBLES_EQUAL(0.0, v.Acc(time),epsilon);
}

void VelocityProfileTest::TestTrapHalf_SetProfile_End()
{
	// 2 second flat velocity (cover 4 distance)
	// 2 second ramp up (cover 2 distance),
	VelocityProfile_TrapHalf	v(2, 1, false);
	double						time;
	v.SetProfile(9, 9-6);
	CPPUNIT_ASSERT_DOUBLES_EQUAL(4.0, v.Duration(),epsilon);

	// start - flat velocity
	time = 0;
	CPPUNIT_ASSERT_DOUBLES_EQUAL(9.0, v.Pos(time),epsilon);
	CPPUNIT_ASSERT_DOUBLES_EQUAL(-2.0, v.Vel(time),epsilon);
	CPPUNIT_ASSERT_DOUBLES_EQUAL(0.0, v.Acc(time),epsilon);

	// end of flat velocity
	time = 2;
	CPPUNIT_ASSERT_DOUBLES_EQUAL(5.0, v.Pos(time),epsilon);
	CPPUNIT_ASSERT_DOUBLES_EQUAL(-2.0, v.Vel(time),epsilon);
	CPPUNIT_ASSERT_DOUBLES_EQUAL(1.0, v.Acc(time),epsilon);

	// middle of ramp down
	time = 3;
	CPPUNIT_ASSERT_DOUBLES_EQUAL(3.5, v.Pos(time),epsilon);
	CPPUNIT_ASSERT_DOUBLES_EQUAL(-1.0, v.Vel(time),epsilon);
	CPPUNIT_ASSERT_DOUBLES_EQUAL(1.0, v.Acc(time),epsilon);

	// end
	time = 4;
	CPPUNIT_ASSERT_DOUBLES_EQUAL(3.0, v.Pos(time),epsilon);
	CPPUNIT_ASSERT_DOUBLES_EQUAL(0.0, v.Vel(time),epsilon);
	CPPUNIT_ASSERT_DOUBLES_EQUAL(0.0, v.Acc(time),epsilon);

	// fenceposts - before and after
	time = -1;
	CPPUNIT_ASSERT_DOUBLES_EQUAL(9.0, v.Pos(time),epsilon);
	CPPUNIT_ASSERT_DOUBLES_EQUAL(0.0, v.Vel(time),epsilon);
	CPPUNIT_ASSERT_DOUBLES_EQUAL(0.0, v.Acc(time),epsilon);
	time = 5;
	CPPUNIT_ASSERT_DOUBLES_EQUAL(3.0, v.Pos(time),epsilon);
	CPPUNIT_ASSERT_DOUBLES_EQUAL(0.0, v.Vel(time),epsilon);
	CPPUNIT_ASSERT_DOUBLES_EQUAL(0.0, v.Acc(time),epsilon);
}

void VelocityProfileTest::TestTrapHalf_SetDuration_Start()
{
	// same as TestTrapHalf__SetProfile_Start() but twice as slow
	// Lingers at start position with zero velocity for a period of time,
	// as does not scale the velocity; only scales the acceleration!?
	VelocityProfile_TrapHalf	v(2, 1, true);
	double						time;
	v.SetProfileDuration(2, 2+6, 8);
	CPPUNIT_ASSERT_DOUBLES_EQUAL(8.0, v.Duration(),epsilon);

	// start - no motion
	time = 0;
	CPPUNIT_ASSERT_DOUBLES_EQUAL(2.0, v.Pos(time),epsilon);
	CPPUNIT_ASSERT_DOUBLES_EQUAL(0.0, v.Vel(time),epsilon);
	CPPUNIT_ASSERT_DOUBLES_EQUAL(0.0, v.Acc(time),epsilon);

	// no motion
	time = 1.9;
	CPPUNIT_ASSERT_DOUBLES_EQUAL(2.0, v.Pos(time),epsilon);
	CPPUNIT_ASSERT_DOUBLES_EQUAL(0.0, v.Vel(time),epsilon);
	CPPUNIT_ASSERT_DOUBLES_EQUAL(0.0, v.Acc(time),epsilon);

	// begin ramp at scaled acceleration
	time = 2;
	CPPUNIT_ASSERT_DOUBLES_EQUAL(2.0, v.Pos(time), 0.001);
	CPPUNIT_ASSERT_DOUBLES_EQUAL(0.0, v.Vel(time),epsilon);
	CPPUNIT_ASSERT_DOUBLES_EQUAL(0.333, v.Acc(time), 0.001);

	// middle of ramp up
	time = 5;
	CPPUNIT_ASSERT_DOUBLES_EQUAL(3.5, v.Pos(time), 0.001);
	CPPUNIT_ASSERT_DOUBLES_EQUAL(1.0, v.Vel(time), 0.001);
	CPPUNIT_ASSERT_DOUBLES_EQUAL(0.3333, v.Acc(time), 0.001);

	// end - continue with given velocity
	time = 8;
	CPPUNIT_ASSERT_DOUBLES_EQUAL(8.0, v.Pos(time),epsilon);
	CPPUNIT_ASSERT_DOUBLES_EQUAL(2.0, v.Vel(time),epsilon);
	CPPUNIT_ASSERT_DOUBLES_EQUAL(0.0, v.Acc(time),epsilon);

	// fenceposts - before and after
	time = -1;
	CPPUNIT_ASSERT_DOUBLES_EQUAL(2.0, v.Pos(time),epsilon);
	CPPUNIT_ASSERT_DOUBLES_EQUAL(0.0, v.Vel(time),epsilon);
	CPPUNIT_ASSERT_DOUBLES_EQUAL(0.0, v.Acc(time),epsilon);
	time = 9;
	CPPUNIT_ASSERT_DOUBLES_EQUAL(8.0, v.Pos(time),epsilon);
	CPPUNIT_ASSERT_DOUBLES_EQUAL(0.0, v.Vel(time),epsilon);
	CPPUNIT_ASSERT_DOUBLES_EQUAL(0.0, v.Acc(time),epsilon);

}

void VelocityProfileTest::TestTrapHalf_SetDuration_End()
{
	// same as TestTrapHalf__SetProfile_Start() but twice as slow
	// Lingers at start position with zero velocity for a period of time,
	// as does not scale the velocity; only scales the acceleration!?
	VelocityProfile_TrapHalf	v(2, 1, true);
	double						time;
	v.SetProfileDuration(2+6, 2, 8);
	CPPUNIT_ASSERT_DOUBLES_EQUAL(8.0, v.Duration(),epsilon);

	// start - no motion
	time = 0;
	CPPUNIT_ASSERT_DOUBLES_EQUAL(8.0, v.Pos(time),epsilon);
	CPPUNIT_ASSERT_DOUBLES_EQUAL(0.0, v.Vel(time),epsilon);
	CPPUNIT_ASSERT_DOUBLES_EQUAL(0.0, v.Acc(time),epsilon);

	// no motion
	time = 1.9;
	CPPUNIT_ASSERT_DOUBLES_EQUAL(8.0, v.Pos(time),epsilon);
	CPPUNIT_ASSERT_DOUBLES_EQUAL(0.0, v.Vel(time),epsilon);
	CPPUNIT_ASSERT_DOUBLES_EQUAL(0.0, v.Acc(time),epsilon);

	// begin ramp at scaled acceleration
	time = 2;
	CPPUNIT_ASSERT_DOUBLES_EQUAL(8.0, v.Pos(time), 0.001);// WRONG, backwards!
	CPPUNIT_ASSERT_DOUBLES_EQUAL(0.0, v.Vel(time),epsilon);
	CPPUNIT_ASSERT_DOUBLES_EQUAL(-0.333, v.Acc(time), 0.001);

	// middle of ramp up
	time = 5;
	CPPUNIT_ASSERT_DOUBLES_EQUAL(6.5, v.Pos(time), 0.001);
	CPPUNIT_ASSERT_DOUBLES_EQUAL(-1.0, v.Vel(time), 0.001);
	CPPUNIT_ASSERT_DOUBLES_EQUAL(-0.3333, v.Acc(time), 0.001);

	// end - continue with given velocity
	time = 8;
	CPPUNIT_ASSERT_DOUBLES_EQUAL(2.0, v.Pos(time),epsilon);
	CPPUNIT_ASSERT_DOUBLES_EQUAL(-2.0, v.Vel(time),epsilon);
	CPPUNIT_ASSERT_DOUBLES_EQUAL(0.0, v.Acc(time),epsilon);

	// fenceposts - before and after
	time = -1;
	CPPUNIT_ASSERT_DOUBLES_EQUAL(8.0, v.Pos(time),epsilon);
	CPPUNIT_ASSERT_DOUBLES_EQUAL(0.0, v.Vel(time),epsilon);
	CPPUNIT_ASSERT_DOUBLES_EQUAL(0.0, v.Acc(time),epsilon);
	time = 9;
	CPPUNIT_ASSERT_DOUBLES_EQUAL(2.0, v.Pos(time),epsilon);
	CPPUNIT_ASSERT_DOUBLES_EQUAL(0.0, v.Vel(time),epsilon);
	CPPUNIT_ASSERT_DOUBLES_EQUAL(0.0, v.Acc(time),epsilon);

}

void VelocityProfileTest::TestDirac_SetProfile()
{
    double time;
    double pos1 = 10.0;
    double pos2 = -1.0;

    VelocityProfile_Dirac v;
    v.SetProfile(pos1, pos2);

    time = 0;
    CPPUNIT_ASSERT_DOUBLES_EQUAL(pos1, v.Pos(time), epsilon);

    time = 1.0;
    CPPUNIT_ASSERT_DOUBLES_EQUAL(pos2, v.Pos(time), epsilon);

    time = -1.0;
    CPPUNIT_ASSERT_DOUBLES_EQUAL(pos1, v.Pos(time), epsilon);
}

void VelocityProfileTest::TestDirac_SetProfileDuration()
{
    double time;
    double pos1 = 10.0;
    double pos2 = -1.0;
    double duration = 5.0;
    VelocityProfile_Dirac v;
    v.SetProfileDuration(pos1, pos2, duration);

    time = -1.0;
    CPPUNIT_ASSERT_DOUBLES_EQUAL(pos1, v.Pos(time), epsilon);

    time = duration/2;
    CPPUNIT_ASSERT_DOUBLES_EQUAL((pos1 + pos2)/2, v.Pos(time), epsilon);
    CPPUNIT_ASSERT_DOUBLES_EQUAL((pos2-pos1)/duration, v.Vel(time), epsilon);

    time = duration;
    CPPUNIT_ASSERT_DOUBLES_EQUAL(pos2, v.Pos(time), epsilon);

    time = duration + 1.0;
    CPPUNIT_ASSERT_DOUBLES_EQUAL(pos2, v.Pos(time), epsilon);
}
