#include "jacobiantest.hpp"
#include <kinfam_io.hpp>
#include <Eigen/Core>

CPPUNIT_TEST_SUITE_REGISTRATION(JacobianTest);

using namespace KDL;

void JacobianTest::setUp(){}
void JacobianTest::tearDown(){}

void JacobianTest::TestChangeRefPoint(){
    //Create a random jacobian
    Jacobian j1(5);
    j1.data.setRandom();
    //Create a random Vector
    Vector p;
    random(p);
    
    Jacobian j2(5);
    CPPUNIT_ASSERT(changeRefPoint(j1,p,j2));
    CPPUNIT_ASSERT(j1!=j2);
    Jacobian j3(4);
    CPPUNIT_ASSERT(!changeRefPoint(j1,p,j3));
    j3.resize(5);
    CPPUNIT_ASSERT(changeRefPoint(j2,-p,j3));
    CPPUNIT_ASSERT_EQUAL(j1,j3);

}

void JacobianTest::TestChangeRefFrame(){
    //Create a random jacobian
    Jacobian j1(5);
    j1.data.setRandom();
    //Create a random frame
    Frame f;
    random(f);
    
    Jacobian j2(5);
    CPPUNIT_ASSERT(changeRefFrame(j1,f,j2));
    CPPUNIT_ASSERT(j1!=j2);
    Jacobian j3(4);
    CPPUNIT_ASSERT(!changeRefFrame(j1,f,j3));
    j3.resize(5);
    CPPUNIT_ASSERT(changeRefFrame(j2,f.Inverse(),j3));
    CPPUNIT_ASSERT_EQUAL(j1,j3);
}

void JacobianTest::TestChangeBase(){
    //Create a random jacobian
    Jacobian j1(5);
    j1.data.setRandom();
    //Create a random rotation
    Rotation r;
    random(r);
    
    Jacobian j2(5);
    CPPUNIT_ASSERT(changeBase(j1,r,j2));
    CPPUNIT_ASSERT(j1!=j2);
    Jacobian j3(4);
    CPPUNIT_ASSERT(!changeBase(j1,r,j3));
    j3.resize(5);
    CPPUNIT_ASSERT(changeBase(j2,r.Inverse(),j3));
    CPPUNIT_ASSERT_EQUAL(j1,j3);
}

void JacobianTest::TestConstructor(){
    //Create an empty Jacobian
    Jacobian j1(2);
    //Get size
    CPPUNIT_ASSERT_EQUAL(j1.rows(),(unsigned int)6);
    CPPUNIT_ASSERT_EQUAL(j1.columns(),(unsigned int)2);
    //Create a second Jacobian from empty
    Jacobian j2(j1);
    //Get size
    CPPUNIT_ASSERT_EQUAL(j2.rows(),(unsigned int)6);
    CPPUNIT_ASSERT_EQUAL(j2.columns(),(unsigned int)2);
    Jacobian j3=j1;
    //Get size
    CPPUNIT_ASSERT_EQUAL(j3.rows(),(unsigned int)6);
    CPPUNIT_ASSERT_EQUAL(j3.columns(),(unsigned int)2);

    //Test resize
    j1.resize(5);
    //Get size
    CPPUNIT_ASSERT_EQUAL(j1.rows(),(unsigned int)6);
    CPPUNIT_ASSERT_EQUAL(j1.columns(),(unsigned int)5);

    j2=j1;
    //Get size
    CPPUNIT_ASSERT_EQUAL(j2.rows(),(unsigned int)6);
    CPPUNIT_ASSERT_EQUAL(j2.columns(),(unsigned int)5);
}

void JacobianTest::TestEqual(){
    Jacobian j1(1);
    Jacobian j2(j1);
    for (unsigned int i=0; i<6; ++i)
    {
        random(j1(i,0));
        j1(i,0) = j1(i,0)*1e-7;
    }
    CPPUNIT_ASSERT(Equal(j1,j2,1));
    CPPUNIT_ASSERT(Equal(j1,j2,1e-6));
    CPPUNIT_ASSERT(!Equal(j1,j2,1e-8));

}


