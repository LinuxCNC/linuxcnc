#ifndef KDL_TREE_INV_DYN_TEST_HPP
#define KDL_TREE_INV_DYN_TEST_HPP

#include <cppunit/extensions/HelperMacros.h>

#include <chain.hpp>
#include <tree.hpp>


using namespace KDL;

class TreeInvDynTest : public CppUnit::TestFixture
{
    CPPUNIT_TEST_SUITE(TreeInvDynTest);
    CPPUNIT_TEST(UpdateTreeTest);
    CPPUNIT_TEST(TwoChainsTest);
    CPPUNIT_TEST(YTreeTest);
    CPPUNIT_TEST_SUITE_END();

public:
    void setUp();
    void tearDown();

    void UpdateTreeTest();
    void TwoChainsTest();
    void YTreeTest();

private:
    Chain chain1,chain2;
    Tree tree, ytree;
    double m, Iz, L;
};
#endif
