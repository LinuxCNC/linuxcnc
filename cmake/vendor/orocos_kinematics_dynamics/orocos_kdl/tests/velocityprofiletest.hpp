#ifndef VELOCITYPROFILETEST_HPP
#define VELOCITYPROFILETEST_HPP

#include <cppunit/extensions/HelperMacros.h>
#include <velocityprofile_trap.hpp>
#include <velocityprofile_traphalf.hpp>
#include <velocityprofile_dirac.hpp>

class VelocityProfileTest : public CppUnit::TestFixture
{
    CPPUNIT_TEST_SUITE(VelocityProfileTest);
    CPPUNIT_TEST(TestTrap_MaxVelocity1);
    CPPUNIT_TEST(TestTrap_MaxVelocity2);
    CPPUNIT_TEST(TestTrap_MaxVelocity3);
    CPPUNIT_TEST(TestTrap_SetDuration1);

    CPPUNIT_TEST(TestTrapHalf_SetProfile_Start);
    CPPUNIT_TEST(TestTrapHalf_SetProfile_End);
    CPPUNIT_TEST(TestTrapHalf_SetDuration_Start);
    CPPUNIT_TEST(TestTrapHalf_SetDuration_End);

    CPPUNIT_TEST(TestDirac_SetProfile);
    CPPUNIT_TEST(TestDirac_SetProfileDuration);

    CPPUNIT_TEST_SUITE_END();

public:
    void setUp();
    void tearDown();

    void TestTrap_MaxVelocity1();
    void TestTrap_MaxVelocity2();
    void TestTrap_MaxVelocity3();
    void TestTrap_SetDuration1();

    void TestTrapHalf_SetProfile_Start();
    void TestTrapHalf_SetProfile_End();
    void TestTrapHalf_SetDuration_Start();
    void TestTrapHalf_SetDuration_End();

    void TestDirac_SetProfile();
    void TestDirac_SetProfileDuration();
};

#endif

