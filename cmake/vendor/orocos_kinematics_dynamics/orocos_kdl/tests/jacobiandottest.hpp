#ifndef JACOBIANDOT_TEST_HPP
#define JACOBIANDOT_TEST_HPP

#include <cppunit/extensions/HelperMacros.h>
#include "chainjnttojacdotsolver.hpp"
#include "frames_io.hpp"
#include "kinfam_io.hpp"

class JacobianDotTest : public CppUnit::TestFixture
{
    CPPUNIT_TEST_SUITE( JacobianDotTest);
    CPPUNIT_TEST(testD2DiffHybrid);
    CPPUNIT_TEST(testD6DiffHybrid);
    CPPUNIT_TEST(testKukaDiffHybrid);
    
    CPPUNIT_TEST(testD2DiffInertial);
    CPPUNIT_TEST(testD6DiffInertial);
    CPPUNIT_TEST(testKukaDiffInertial);
    
    CPPUNIT_TEST(testD2DiffBodyFixed);
    CPPUNIT_TEST(testD6DiffBodyFixed);
    CPPUNIT_TEST(testKukaDiffBodyFixed);
    
    CPPUNIT_TEST(testD2Symbolic);

    CPPUNIT_TEST_SUITE_END();

public:
    void setUp();
    void tearDown();

    void testD2DiffHybrid();
    void testD6DiffHybrid();
    void testKukaDiffHybrid();

    void testD2DiffInertial();
    void testD6DiffInertial();
    void testKukaDiffInertial();
    
    void testD2DiffBodyFixed();
    void testD6DiffBodyFixed();
    void testKukaDiffBodyFixed();
    
    void testD2Symbolic();
};

#endif
