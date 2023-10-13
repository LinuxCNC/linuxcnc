#ifndef FRAMES_TEST_HPP
#define FRAMES_TEST_HPP

#include <cppunit/extensions/HelperMacros.h>
#include <frames.hpp>
#include <jntarray.hpp>

using namespace KDL;

class FramesTest : public CppUnit::TestFixture
{
    CPPUNIT_TEST_SUITE( FramesTest);
    CPPUNIT_TEST(TestVector);
    CPPUNIT_TEST(TestVector2DNorm);
    CPPUNIT_TEST(TestTwist);
    CPPUNIT_TEST(TestWrench);
    CPPUNIT_TEST(TestRotation);
    CPPUNIT_TEST(TestQuaternion);
    CPPUNIT_TEST(TestFrame);
    CPPUNIT_TEST(TestJntArray);
    CPPUNIT_TEST(TestRotationDiff);
    CPPUNIT_TEST(TestEuler);
    CPPUNIT_TEST(TestGetRotAngle);
    CPPUNIT_TEST_SUITE_END();

public:
    void setUp();
    void tearDown();

    void TestVector();
    void TestVector2DNorm();
    void TestTwist();
    void TestWrench();
    void TestRotation();
    void TestQuaternion();
    void TestFrame();
    void TestJntArray();
    void TestJntArrayWhenEmpty();
	void TestRotationDiff();
	void TestEuler();
	void TestGetRotAngle();

private:
    void TestVector2(Vector& v);
    void TestTwist2(Twist& t);
    void TestWrench2(Wrench& w);
    void TestRotation2(const Vector& v,double a,double b,double c);
    void TestOneRotation(const std::string& msg,
                         const KDL::Rotation& R,
                         const double expectedAngle,
                         const KDL::Vector& expectedAxis);
    void TestArbitraryRotation(const std::string& msg,
                               const KDL::Vector& v,
                               const double angle,
                               const double expectedAngle,
                               const KDL::Vector& expectedVector);
    void TestRangeArbitraryRotation(const std::string& msg,
                                    const KDL::Vector& v,
                                    const KDL::Vector& expectedVector);
    void TestOneRotationDiff(const std::string& msg,
                             const KDL::Rotation& R_a_b1,
                             const KDL::Rotation& R_a_b2,
                             const KDL::Vector& expectedDiff);
 
};

#endif

