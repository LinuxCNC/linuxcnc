#ifndef JACOBIAN_TEST_HPP
#define JACOBIAN_TEST_HPP

#include <cppunit/extensions/HelperMacros.h>
#include <jacobian.hpp>

using namespace KDL;

class JacobianTest : public CppUnit::TestFixture
{
    CPPUNIT_TEST_SUITE( JacobianTest);
    CPPUNIT_TEST(TestChangeRefPoint);
    CPPUNIT_TEST(TestChangeRefFrame);
    CPPUNIT_TEST(TestChangeBase);
    CPPUNIT_TEST(TestConstructor);
    CPPUNIT_TEST(TestEqual);
    CPPUNIT_TEST_SUITE_END();

public:
    void setUp();
    void tearDown();

    void TestChangeRefPoint();
    void TestChangeRefFrame();
    void TestChangeBase();
    void TestConstructor();
    void TestEqual();
};

#endif
