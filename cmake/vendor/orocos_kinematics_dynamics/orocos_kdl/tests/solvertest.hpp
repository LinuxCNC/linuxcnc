#ifndef KDL_SOLVER_TEST_HPP
#define KDL_SOLVER_TEST_HPP

#include <cppunit/extensions/HelperMacros.h>

#include <chain.hpp>
#include <chainfksolverpos_recursive.hpp>
#include <chainfksolvervel_recursive.hpp>
#include <chainiksolvervel_pinv.hpp>
#include <chainiksolvervel_pinv_givens.hpp>
#include <chainiksolvervel_pinv_nso.hpp>
#include <chainiksolvervel_wdls.hpp>
#include <chainiksolverpos_nr.hpp>
#include <chainiksolverpos_lma.hpp>
#include <chainiksolverpos_nr_jl.hpp>
#include <chainjnttojacsolver.hpp>
#include <chainjnttojacdotsolver.hpp>
#include <chainidsolver_vereshchagin.hpp>
#include <chainidsolver_recursive_newton_euler.hpp>
#include <chaindynparam.hpp>
#include <chainidsolver_recursive_newton_euler.hpp>
#include <chainfdsolver_recursive_newton_euler.hpp>
#include <utilities/ldl_solver_eigen.hpp>


using namespace KDL;

class SolverTest : public CppUnit::TestFixture
{
    CPPUNIT_TEST_SUITE( SolverTest);
    CPPUNIT_TEST(FkPosAndJacTest );
    CPPUNIT_TEST(FkVelAndJacTest );
    CPPUNIT_TEST(FkVelAndIkVelTest );
    CPPUNIT_TEST(FkPosAndIkPosTest );
    CPPUNIT_TEST(VereshchaginTest );
    CPPUNIT_TEST(IkSingularValueTest );
    CPPUNIT_TEST(IkVelSolverWDLSTest );
    CPPUNIT_TEST(FkPosVectTest );
    CPPUNIT_TEST(FkVelVectTest );
    CPPUNIT_TEST(FdSolverDevelopmentTest );
    CPPUNIT_TEST(FdSolverConsistencyTest );
    CPPUNIT_TEST(LDLdecompTest);
    CPPUNIT_TEST(UpdateChainTest );
    CPPUNIT_TEST_SUITE_END();

public:
    void setUp();
    void tearDown();

    void FkPosAndJacTest();
    void FkVelAndJacTest();
    void FkVelAndIkVelTest();
    void FkPosAndIkPosTest();
    void VereshchaginTest();
    void IkSingularValueTest() ;
    void IkVelSolverWDLSTest();
    void FkPosVectTest();
    void FkVelVectTest();
    void FdSolverDevelopmentTest();
    void FdSolverConsistencyTest();
    void LDLdecompTest();
    void UpdateChainTest();

private:

  Chain chain1, chain2, chain3, chain4, chaindyn, motomansia10, motomansia10dyn;

    void FkPosAndJacLocal(Chain& chain,ChainFkSolverPos& fksolverpos,ChainJntToJacSolver& jacsolver);
    void FkVelAndJacLocal(Chain& chain, ChainFkSolverVel& fksolvervel, ChainJntToJacSolver& jacsolver);
    void FkVelAndIkVelLocal(Chain& chain, ChainFkSolverVel& fksolvervel, ChainIkSolverVel& iksolvervel);
    void FkPosAndIkPosLocal(Chain& chain,ChainFkSolverPos& fksolverpos, ChainIkSolverPos& iksolverpos);

};
#endif

