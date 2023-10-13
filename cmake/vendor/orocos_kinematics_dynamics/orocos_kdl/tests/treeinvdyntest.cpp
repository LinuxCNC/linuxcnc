#include "treeinvdyntest.hpp"
#include <kinfam_io.hpp>
#include <frames_io.hpp>
#include <chainidsolver_recursive_newton_euler.hpp>
#include <treeidsolver_recursive_newton_euler.hpp>
#include <time.h>
#include <cmath>


CPPUNIT_TEST_SUITE_REGISTRATION( TreeInvDynTest );

using namespace KDL;
using std::cout;
using std::endl;

void TreeInvDynTest::setUp()
{
    srand( (unsigned)time( NULL ));

    //spatial inertia (just to test dynamics)
    RigidBodyInertia inertia(0.3, Vector(0.0, 0.1, 0.0), RotationalInertia(0.005, 0.001, 0.001));

    //create chain #1
    chain1.addSegment(Segment("Segment 11", Joint("Joint 11", Joint::RotZ),
                              Frame(Vector(0.0,0.0,0.0)), inertia));
    chain1.addSegment(Segment("Segment 12", Joint("Joint 12", Joint::RotX),
                              Frame(Vector(0.0,0.0,0.9)), inertia));
    chain1.addSegment(Segment("Segment 13", Joint("Joint 13", Joint::None),
                              Frame(Vector(-0.4,0.0,0.0))));
    chain1.addSegment(Segment("Segment 14", Joint("Joint 14", Joint::RotX),
                              Frame(Vector(0.0,0.0,1.2)), inertia));
    chain1.addSegment(Segment("Segment 15", Joint("Joint 15", Joint::None),
                              Frame(Vector(0.4,0.0,0.0)), inertia));
    chain1.addSegment(Segment("Segment 16", Joint("Joint 16", Joint::RotZ),
                              Frame(Vector(0.0,0.0,1.4)), inertia));
    chain1.addSegment(Segment("Segment 17", Joint("Joint 17", Joint::RotX),
                              Frame(Vector(0.0,0.0,0.0)), inertia));
    chain1.addSegment(Segment("Segment 18", Joint("Joint 18", Joint::RotZ),
                              Frame(Vector(0.0,0.0,0.4)), inertia));
    chain1.addSegment(Segment("Segment 19", Joint("Joint 19", Joint::None),
                              Frame(Vector(0.0,0.0,0.0)), inertia));

    //create chain #2
    chain2.addSegment(Segment("Segment 21", Joint("Joint 21", Joint::RotZ),
                              Frame(Vector(0.0,0.0,0.5)), inertia));
    chain2.addSegment(Segment("Segment 22", Joint("Joint 22", Joint::RotX),
                              Frame(Vector(0.0,0.0,0.4)), inertia));
    chain2.addSegment(Segment("Segment 23", Joint("Joint 23", Joint::RotX),
                              Frame(Vector(0.0,0.0,0.3)), inertia));
    chain2.addSegment(Segment("Segment 24", Joint("Joint 24", Joint::RotX),
                              Frame(Vector(0.0,0.0,0.2)), inertia));
    chain2.addSegment(Segment("Segment 25", Joint("Joint 25", Joint::RotZ),
                              Frame(Vector(0.0,0.0,0.1)), inertia));

    //create tree as two chains attached to the root
    tree = Tree();
    tree.addChain(chain1, tree.getRootSegment()->first);
    tree.addChain(chain2, tree.getRootSegment()->first);

    //create a simple "y-shaped" tree
    m=1.0, Iz=0.05, L=0.5; //geometric and dynamic parameters
    Segment s1("S1", Joint("q1", Joint::RotZ), Frame(), RigidBodyInertia(m, Vector(L,0,0), RotationalInertia(0,0,Iz)));
    Segment s2("S2", Joint("q2", Vector(L,0,0), Vector(0,0,1), Joint::RotAxis), Frame(Vector(L,0,0)), RigidBodyInertia(m, Vector(L/2,0,0), RotationalInertia(0,0,Iz)));
    Segment s3("S3", Joint("q3", Vector(2*L,0,0), Vector(0,0,1), Joint::RotAxis), Frame(Vector(2*L,0,0)), RigidBodyInertia(m, Vector(L/2,0,0), RotationalInertia(0,0,Iz)));
    ytree = Tree("root");
    ytree.addSegment(s1, "root");
    ytree.addSegment(s2, "S1");
    ytree.addSegment(s3, "S1");
}


void TreeInvDynTest::tearDown() { }


void TreeInvDynTest::UpdateTreeTest() {
  // std::cout << "Tree: " << endl << tree2str(tree) << endl; //NOTE: tree2str is not available as I implemented it in another "parallel" commit
  std::cout << "\nTree: " << endl << tree << endl;

  Tree temp_tree(tree);
  TreeIdSolver_RNE solver(temp_tree, Vector::Zero());
  JntArray q, qd, qdd, tau;

  CPPUNIT_ASSERT_EQUAL((int)SolverI::E_SIZE_MISMATCH, solver.CartToJnt(q, qd, qdd, WrenchMap(), tau));
  q.resize(temp_tree.getNrOfJoints());
  CPPUNIT_ASSERT_EQUAL((int)SolverI::E_SIZE_MISMATCH, solver.CartToJnt(q, qd, qdd, WrenchMap(), tau));
  qd.resize(temp_tree.getNrOfJoints());
  CPPUNIT_ASSERT_EQUAL((int)SolverI::E_SIZE_MISMATCH, solver.CartToJnt(q, qd, qdd, WrenchMap(), tau));
  qdd.resize(temp_tree.getNrOfJoints());
  CPPUNIT_ASSERT_EQUAL((int)SolverI::E_SIZE_MISMATCH, solver.CartToJnt(q, qd, qdd, WrenchMap(), tau));
  tau.resize(temp_tree.getNrOfJoints());
  CPPUNIT_ASSERT_EQUAL((int)SolverI::E_NOERROR, solver.CartToJnt(q, qd, qdd, WrenchMap(), tau));

  temp_tree.addSegment(Segment("extra"), temp_tree.getRootSegment()->first);
  CPPUNIT_ASSERT_EQUAL((int)SolverI::E_NOT_UP_TO_DATE, solver.CartToJnt(q, qd, qdd, WrenchMap(), tau));
  solver.updateInternalDataStructures();
  CPPUNIT_ASSERT_EQUAL((int)SolverI::E_NOERROR, solver.CartToJnt(q, qd, qdd, WrenchMap(), tau));
}


void TreeInvDynTest::TwoChainsTest() {
  Vector gravity(0,0,-9.8);
  TreeIdSolver_RNE solver(tree, gravity);
  ChainIdSolver_RNE solver1(chain1, gravity);
  ChainIdSolver_RNE solver2(chain2, gravity);

  unsigned int nt = tree.getNrOfJoints();
  unsigned int n1 = chain1.getNrOfJoints();
  unsigned int n2 = chain2.getNrOfJoints();

  //Check that sizes are consistent -- otherwise following code does not make sense!
  CPPUNIT_ASSERT_EQUAL(nt, n1+n2);

  JntArray q(nt), qd(nt), qdd(nt), tau(nt); //data related to tree
  JntArray q1(n1), qd1(n1), qdd1(n1), tau1(n1); //data related to chain1
  JntArray q2(n2), qd2(n2), qdd2(n2), tau2(n2); //data related to chain2

  unsigned int iterations = 100;
  while(iterations-- > 0) {
    //Randomize joint vectors
    for(unsigned int i=0; i<nt; i++) random(q(i)), random(qd(i)), random(qdd(i));
    for(unsigned int i=0; i<n1; i++) q1(i)=q(i), qd1(i)=qd(i), qdd1(i)=qdd(i);
    for(unsigned int i=0; i<n2; i++) q2(i)=q(i+n1), qd2(i)=qd(i+n1), qdd2(i)=qdd(i+n1);

    //Evaluate efforts
    CPPUNIT_ASSERT_EQUAL((int)SolverI::E_NOERROR, solver.CartToJnt(q, qd, qdd, WrenchMap(), tau));
    CPPUNIT_ASSERT_EQUAL((int)SolverI::E_NOERROR, solver1.CartToJnt(q1, qd1, qdd1, Wrenches(chain1.getNrOfSegments()), tau1));
    CPPUNIT_ASSERT_EQUAL((int)SolverI::E_NOERROR, solver2.CartToJnt(q2, qd2, qdd2, Wrenches(chain2.getNrOfSegments()), tau2));

    //compare efforts
    JntArray tau12(tau.rows());
    for(unsigned int i=0; i<n1; i++) tau12(i) = tau1(i);
    for(unsigned int i=0; i<n2; i++) tau12(i+n1) = tau2(i);
    CPPUNIT_ASSERT_EQUAL(tau, tau12);
  }
}


void TreeInvDynTest::YTreeTest() {
  std::cout << "\nY-shaped tree: " << endl << ytree << endl;

  double g = 9.8;
  TreeIdSolver_RNE solver(ytree, Vector(0,-g,0));

  //Following is just a check in case the model is modified. In this case the
  //analytic model derived using Euler-Lagrange equations would not be valid.
  unsigned int dof = ytree.getNrOfJoints();
  CPPUNIT_ASSERT_EQUAL(dof, (unsigned int)3); // The analytic euler-lagrange model was derived for a specific 3-dof model

  JntArray q(dof), qd(dof), qdd(dof), tau(dof), eff(dof);

  double mLL = m*L*L;
  double Ie = Iz + 0.25*mLL;

  unsigned int iterations = 100;
  while(iterations-- > 0) {
    //Randomize joint vectors
    for(unsigned int i=0; i<dof; i++)
      random(q(i)), random(qd(i)), random(qdd(i));

    //Evaluate efforts using the numerical solver
    CPPUNIT_ASSERT_EQUAL((int)SolverI::E_NOERROR, solver.CartToJnt(q, qd, qdd, WrenchMap(), tau));

    //Evaluate efforts using the analytic model derived using Euler-Lagrange equations
    double s2 = std::sin(q(1)), c2 = std::cos(q(1));
    double s3 = std::sin(q(2)), c3 = std::cos(q(2));
    double c1 = std::cos(q(0)), c12 = std::cos(q(0)+q(1)), c13 = std::cos(q(0)+q(2));
    eff(0) = (3*Iz+6.5*mLL+mLL*c2+2*mLL*c3) * qdd(0) + (Ie+0.5*mLL*c2) * qdd(1)
            + (Ie+mLL*c3) * qdd(2) - 0.5*mLL*s2*qd(1)*qd(1) - mLL*s3*qd(2)*qd(2)
            - mLL*s2*qd(0)*qd(1) - 2*mLL*s3*qd(0)*qd(2) + m*g*(4*L*c1+0.5*L*c12+0.5*L*c13);
    eff(1) = (Ie+0.5*mLL*c2) * qdd(0) + Ie * qdd(1) + 0.5*mLL*s2*qd(0)*qd(0) + 0.5*m*g*L*c12;
    eff(2) = (Ie+mLL*c3) * qdd(0) + Ie * qdd(2) + mLL*s3*qd(0)*qd(0) + 0.5*m*g*L*c13;

    //compare efforts
    CPPUNIT_ASSERT_EQUAL(tau, eff);
  }

}
