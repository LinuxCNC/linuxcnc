#ifndef INERTIA_TEST_HPP
#define INERTIA_TEST_HPP

#include <cppunit/extensions/HelperMacros.h>

class InertiaTest : public CppUnit::TestFixture
{
    CPPUNIT_TEST_SUITE( InertiaTest);
    CPPUNIT_TEST(TestRotationalInertia);
    CPPUNIT_TEST(TestRigidBodyInertia);
    CPPUNIT_TEST(TestArticulatedBodyInertia);
    CPPUNIT_TEST_SUITE_END();

public:
    void setUp();
    void tearDown();

    void TestRotationalInertia();
    void TestRigidBodyInertia();
    void TestArticulatedBodyInertia();

};

#endif
