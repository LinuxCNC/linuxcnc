#include "solvertest.hpp"
#include <frames_io.hpp>
#include <framevel_io.hpp>
#include <kinfam_io.hpp>
#include <time.h>
#include <utilities/utility.h>

CPPUNIT_TEST_SUITE_REGISTRATION( SolverTest );

using namespace KDL;

void SolverTest::setUp()
{
    srand( (unsigned)time( NULL ));

    chain1.addSegment(Segment("Segment 1", Joint("Joint 1", Joint::RotZ),
                              Frame(Vector(0.0,0.0,0.0))));
    chain1.addSegment(Segment("Segment 2", Joint("Joint 2", Joint::RotX),
                              Frame(Vector(0.0,0.0,0.9))));
    chain1.addSegment(Segment("Segment 3", Joint("Joint 3", Joint::None),
                              Frame(Vector(-0.4,0.0,0.0))));
    chain1.addSegment(Segment("Segment 4", Joint("Joint 4", Joint::RotX),
                              Frame(Vector(0.0,0.0,1.2))));
    chain1.addSegment(Segment("Segment 5", Joint("Joint 5", Joint::None),
                              Frame(Vector(0.4,0.0,0.0))));
    chain1.addSegment(Segment("Segment 6", Joint("Joint 6", Joint::RotZ),
                              Frame(Vector(0.0,0.0,1.4))));
    chain1.addSegment(Segment("Segment 7", Joint("Joint 7", Joint::RotX),
                              Frame(Vector(0.0,0.0,0.0))));
    chain1.addSegment(Segment("Segment 8", Joint("Joint 8", Joint::RotZ),
                              Frame(Vector(0.0,0.0,0.4))));
    chain1.addSegment(Segment("Segment 9", Joint("Joint 9", Joint::None),
                              Frame(Vector(0.0,0.0,0.0))));

    chain2.addSegment(Segment("Segment 1", Joint("Joint 1", Joint::RotZ),
                              Frame(Vector(0.0,0.0,0.5))));
    chain2.addSegment(Segment("Segment 2", Joint("Joint 2", Joint::RotX),
                              Frame(Vector(0.0,0.0,0.4))));
    chain2.addSegment(Segment("Segment 3", Joint("Joint 3", Joint::RotX),
                              Frame(Vector(0.0,0.0,0.3))));
    chain2.addSegment(Segment("Segment 4", Joint("Joint 4", Joint::RotX),
                              Frame(Vector(0.0,0.0,0.2))));
    chain2.addSegment(Segment("Segment 5", Joint("Joint 5", Joint::RotZ),
                              Frame(Vector(0.0,0.0,0.1))));


    chain3.addSegment(Segment("Segment 1", Joint("Joint 1", Joint::RotZ),
                              Frame(Vector(0.0,0.0,0.0))));
    chain3.addSegment(Segment("Segment 2", Joint("Joint 2", Joint::RotX),
                              Frame(Vector(0.0,0.0,0.9))));
    chain3.addSegment(Segment("Segment 3", Joint("Joint 3", Joint::RotZ),
                              Frame(Vector(-0.4,0.0,0.0))));
    chain3.addSegment(Segment("Segment 4", Joint("Joint 4", Joint::RotX),
                              Frame(Vector(0.0,0.0,1.2))));
    chain3.addSegment(Segment("Segment 5", Joint("Joint 5", Joint::None),
                              Frame(Vector(0.4,0.0,0.0))));
    chain3.addSegment(Segment("Segment 6", Joint("Joint 6", Joint::RotZ),
                              Frame(Vector(0.0,0.0,1.4))));
    chain3.addSegment(Segment("Segment 7", Joint("Joint 7", Joint::RotX),
                              Frame(Vector(0.0,0.0,0.0))));
    chain3.addSegment(Segment("Segment 8", Joint("Joint 8", Joint::RotZ),
                              Frame(Vector(0.0,0.0,0.4))));
    chain3.addSegment(Segment("Segment 9", Joint("Joint 9", Joint::RotY),
                              Frame(Vector(0.0,0.0,0.0))));


    chain4.addSegment(Segment("Segment 1", Joint("Joint 1", Vector(10,0,0), Vector(1,0,1),Joint::RotAxis),
                              Frame(Vector(0.0,0.0,0.5))));
    chain4.addSegment(Segment("Segment 2", Joint("Joint 2", Vector(0,5,0), Vector(1,0,0),Joint::RotAxis),
                              Frame(Vector(0.0,0.0,0.4))));
    chain4.addSegment(Segment("Segment 3", Joint("Joint 3", Vector(0,0,5), Vector(1,0,4),Joint::RotAxis),
                              Frame(Vector(0.0,0.0,0.3))));
    chain4.addSegment(Segment("Segment 4", Joint("Joint 4", Vector(0,0,0), Vector(1,0,0),Joint::RotAxis),
                              Frame(Vector(0.0,0.0,0.2))));
    chain4.addSegment(Segment("Segment 5", Joint("Joint 5", Vector(0,0,0), Vector(0,0,1),Joint::RotAxis),
                              Frame(Vector(0.0,0.0,0.1))));



    //chain definition for vereshchagin's dynamic solver
    Joint rotJoint0 = Joint(Joint::RotZ, 1, 0, 0.01);
    Joint rotJoint1 = Joint(Joint::RotZ, 1, 0, 0.01);

    Frame refFrame(Rotation::RPY(0.0, 0.0, 0.0), Vector(0.0, 0.0, 0.0));
    Frame frame1(Rotation::RPY(0.0, 0.0, 0.0), Vector(0.0, -0.4, 0.0));
    Frame frame2(Rotation::RPY(0.0, 0.0, 0.0), Vector(0.0, -0.4, 0.0));

    //chain segments
    Segment segment1 = Segment(rotJoint0, frame1);
    Segment segment2 = Segment(rotJoint1, frame2);

    //rotational inertia around symmetry axis of rotation
    RotationalInertia rotInerSeg1(0.0, 0.0, 0.0, 0.0, 0.0, 0.0);

    //spatial inertia
    RigidBodyInertia inerSegment1(0.3, Vector(0.0, -0.4, 0.0), rotInerSeg1);
    RigidBodyInertia inerSegment2(0.3, Vector(0.0, -0.4, 0.0), rotInerSeg1);
    segment1.setInertia(inerSegment1);
    segment2.setInertia(inerSegment2);

    //chain
    chaindyn.addSegment(segment1);
    chaindyn.addSegment(segment2);

	// Motoman SIA10 Chain (for IK singular value tests)
	motomansia10.addSegment(Segment(Joint(Joint::None),
									Frame::DH_Craig1989(0.0, 0.0, 0.36, 0.0)));
	motomansia10.addSegment(Segment(Joint(Joint::RotZ),
									Frame::DH_Craig1989(0.0, PI_2, 0.0, 0.0)));
	motomansia10.addSegment(Segment(Joint(Joint::RotZ),
									Frame::DH_Craig1989(0.0, -PI_2, 0.36, 0.0)));
	motomansia10.addSegment(Segment(Joint(Joint::RotZ),
									Frame::DH_Craig1989(0.0, PI_2, 0.0, 0.0)));
	motomansia10.addSegment(Segment(Joint(Joint::RotZ),
									Frame::DH_Craig1989(0.0, -PI_2, 0.36, 0.0)));
	motomansia10.addSegment(Segment(Joint(Joint::RotZ),
									Frame::DH_Craig1989(0.0, PI_2, 0.0, 0.0)));
	motomansia10.addSegment(Segment(Joint(Joint::RotZ),
									Frame::DH_Craig1989(0.0, -PI_2, 0.0, 0.0)));
	motomansia10.addSegment(Segment(Joint(Joint::RotZ),
									Frame(Rotation::Identity(),Vector(0.0,0.0,0.155))));

    // Motoman SIA10 Chain with Mass Parameters (for forward dynamics tests)

    //  effective motor inertia is included as joint inertia
    static const double scale=1;
    static const double offset=0;
    static const double inertiamotorA=5.0;      // effective motor inertia kg-m^2
    static const double inertiamotorB=3.0;      // effective motor inertia kg-m^2
    static const double inertiamotorC=1.0;      // effective motor inertia kg-m^2
    static const double damping=0;
    static const double stiffness=0;

    //  Segment Inertias
    KDL::RigidBodyInertia inert1(15.0, KDL::Vector(0.0, -0.02, 0.0),                       // mass, CM
                                 KDL::RotationalInertia(0.1, 0.05, 0.1, 0.0, 0.0, 0.0));   // inertia
    KDL::RigidBodyInertia inert2(5.0, KDL::Vector(0.0, -0.02, -0.1),
                                 KDL::RotationalInertia(0.01, 0.1, 0.1, 0.0, 0.0, 0.0));
    KDL::RigidBodyInertia inert3(5.0, KDL::Vector(0.0, -0.05, 0.02),
                                 KDL::RotationalInertia(0.05, 0.01, 0.05, 0.0, 0.0, 0.0));
    KDL::RigidBodyInertia inert4(3.0, KDL::Vector(0.0, 0.02, -0.15),
                                 KDL::RotationalInertia(0.1, 0.1, 0.01, 0.0, 0.0, 0.0));
    KDL::RigidBodyInertia inert5(3.0, KDL::Vector(0.0, -0.05, 0.01),
                                 KDL::RotationalInertia(0.02, 0.01, 0.02, 0.0, 0.0, 0.0));
    KDL::RigidBodyInertia inert6(3.0, KDL::Vector(0.0, -0.01, -0.1),
                                 KDL::RotationalInertia(0.1, 0.1, 0.01, 0.0, 0.0, 0.0));
    KDL::RigidBodyInertia inert7(1.0, KDL::Vector(0.0, 0.0, 0.05),
                                 KDL::RotationalInertia(0.01, 0.01, 0.1, 0.0, 0.0, 0.0));

    motomansia10dyn.addSegment(Segment(Joint(Joint::RotZ, scale, offset, inertiamotorA, damping, stiffness),
                               Frame::DH(0.0, PI_2, 0.36, 0.0),
                               inert1));
    motomansia10dyn.addSegment(Segment(Joint(Joint::RotZ, scale, offset, inertiamotorA, damping, stiffness),
                               Frame::DH(0.0, -PI_2, 0.0, 0.0),
                               inert2));
    motomansia10dyn.addSegment(Segment(Joint(Joint::RotZ, scale, offset, inertiamotorB, damping, stiffness),
                               Frame::DH(0.0, PI_2, 0.36, 0.0),
                               inert3));
    motomansia10dyn.addSegment(Segment(Joint(Joint::RotZ, scale, offset, inertiamotorB, damping, stiffness),
                               Frame::DH(0.0, -PI_2, 0.0, 0.0),
                               inert4));
    motomansia10dyn.addSegment(Segment(Joint(Joint::RotZ, scale, offset, inertiamotorC, damping, stiffness),
                               Frame::DH(0.0, PI_2, 0.36, 0.0),
                               inert5));
    motomansia10dyn.addSegment(Segment(Joint(Joint::RotZ, scale, offset, inertiamotorC, damping, stiffness),
                               Frame::DH(0.0, -PI_2, 0.0, 0.0),
                               inert6));
    motomansia10dyn.addSegment(Segment(Joint(Joint::RotZ, scale, offset, inertiamotorC, damping, stiffness),
                               Frame::DH(0.0, 0.0, 0.0, 0.0),
                               inert7));
    motomansia10dyn.addSegment(Segment(Joint(Joint::None),
                                       Frame(Rotation::Identity(),Vector(0.0,0.0,0.155))));
}

void SolverTest::tearDown()
{
//     delete fksolverpos;
//     delete jacsolver;
//     delete fksolvervel;
//     delete iksolvervel;
//     delete iksolverpos;
}

void SolverTest::UpdateChainTest()
{
    ChainFkSolverPos_recursive fksolverpos(chain2);
    ChainFkSolverVel_recursive fksolvervel(chain2);
    ChainJntToJacSolver jacsolver1(chain2);
    ChainJntToJacDotSolver jacdotsolver1(chain2);
    ChainIkSolverVel_pinv iksolver2(chain2);
    ChainIkSolverVel_pinv_givens iksolver_pinv_givens2(chain2);
    ChainIkSolverVel_pinv_nso  iksolver_pinv_nso(chain2);
    ChainIkSolverVel_wdls iksolver_wdls(chain2,1e-6,30);
    ChainIkSolverPos_NR iksolverpos(chain2,fksolverpos,iksolver2);
    ChainIkSolverPos_NR_JL iksolverpos2(chain2,fksolverpos,iksolver2);
    ChainIkSolverPos_LMA iksolverpos3(chain2);
    ChainDynParam dynparam(chain2, Vector::Zero());
    ChainIdSolver_RNE idsolver1(chain2, Vector::Zero());
    unsigned int nr_of_constraints = 4;
    ChainIdSolver_Vereshchagin idsolver2(chain2,Twist::Zero(),4);

    JntArray q_in(chain2.getNrOfJoints());
    JntArray q_in2(chain2.getNrOfJoints());
    JntArrayVel q_in3(chain2.getNrOfJoints());
    for(unsigned int i=0; i<chain2.getNrOfJoints(); i++)
    {
        random(q_in(i));
        random(q_in2(i));
    }
    JntArray q_out(chain2.getNrOfJoints());
    JntArray q_out2(chain2.getNrOfJoints());
    Jacobian jac(chain2.getNrOfJoints());
    Frame T;
    Twist t;
    FrameVel T2;
    Wrenches wrenches(chain2.getNrOfSegments());
    JntSpaceInertiaMatrix m(chain2.getNrOfJoints());

    Jacobian alpha(nr_of_constraints - 1);
    JntArray beta(nr_of_constraints - 1);
    CPPUNIT_ASSERT_EQUAL((int)SolverI::E_OUT_OF_RANGE,fksolverpos.JntToCart(q_in, T, chain2.getNrOfSegments()+1));
    CPPUNIT_ASSERT_EQUAL((int)SolverI::E_OUT_OF_RANGE,fksolvervel.JntToCart(q_in3, T2, chain2.getNrOfSegments()+1));
    CPPUNIT_ASSERT_EQUAL((int)SolverI::E_OUT_OF_RANGE, jacsolver1.JntToJac(q_in, jac, chain2.getNrOfSegments()+1));
    CPPUNIT_ASSERT_EQUAL((int)SolverI::E_OUT_OF_RANGE, jacdotsolver1.JntToJacDot(q_in3, t, chain2.getNrOfSegments()+1));
    CPPUNIT_ASSERT_EQUAL((int)SolverI::E_OUT_OF_RANGE, jacdotsolver1.JntToJacDot(q_in3, jac, chain2.getNrOfSegments()+1));
    chain2.addSegment(Segment("Segment 6", Joint("Joint 6", Joint::RotX),
            Frame(Vector(0.0,0.0,0.1))));
    CPPUNIT_ASSERT_EQUAL((int)SolverI::E_NOT_UP_TO_DATE, jacsolver1.JntToJac(q_in, jac, chain2.getNrOfSegments()));
    CPPUNIT_ASSERT_EQUAL((int)SolverI::E_NOT_UP_TO_DATE, jacdotsolver1.JntToJacDot(q_in3, jac, chain2.getNrOfSegments()));
    CPPUNIT_ASSERT_EQUAL((int)SolverI::E_NOT_UP_TO_DATE, jacdotsolver1.JntToJacDot(q_in3, t, chain2.getNrOfSegments()));
    CPPUNIT_ASSERT_EQUAL((int)SolverI::E_NOT_UP_TO_DATE, iksolver2.CartToJnt(q_in,t,q_out));
    CPPUNIT_ASSERT_EQUAL((int)SolverI::E_NOT_UP_TO_DATE, iksolver_pinv_givens2.CartToJnt(q_in,t,q_out));
    CPPUNIT_ASSERT_EQUAL((int)SolverI::E_NOT_UP_TO_DATE, iksolver_pinv_nso.CartToJnt(q_in,t,q_out));
    CPPUNIT_ASSERT_EQUAL((int)SolverI::E_NOT_UP_TO_DATE, iksolver_wdls.CartToJnt(q_in,t,q_out));
    CPPUNIT_ASSERT_EQUAL((int)SolverI::E_NOT_UP_TO_DATE, iksolverpos.CartToJnt(q_in,T,q_out));
    CPPUNIT_ASSERT_EQUAL((int)SolverI::E_NOT_UP_TO_DATE, iksolverpos2.CartToJnt(q_in,T,q_out));
    CPPUNIT_ASSERT_EQUAL((int)SolverI::E_NOT_UP_TO_DATE, iksolverpos3.CartToJnt(q_in,T,q_out));
    CPPUNIT_ASSERT_EQUAL((int)SolverI::E_NOT_UP_TO_DATE, idsolver1.CartToJnt(q_in,q_in2,q_out,wrenches,q_out2));
    CPPUNIT_ASSERT_EQUAL((int)SolverI::E_NOT_UP_TO_DATE, idsolver2.CartToJnt(q_in,q_in2,q_out,alpha,beta,wrenches,q_out2));
    CPPUNIT_ASSERT_EQUAL((int)SolverI::E_NOT_UP_TO_DATE, dynparam.JntToCoriolis(q_in, q_in2, q_out));
    CPPUNIT_ASSERT_EQUAL((int)SolverI::E_NOT_UP_TO_DATE, dynparam.JntToGravity(q_in, q_out));
    CPPUNIT_ASSERT_EQUAL((int)SolverI::E_NOT_UP_TO_DATE, dynparam.JntToMass(q_in, m));

    jacsolver1.updateInternalDataStructures();
    jacdotsolver1.updateInternalDataStructures();
    iksolver2.updateInternalDataStructures();
    iksolver_pinv_givens2.updateInternalDataStructures();
    iksolver_pinv_nso.updateInternalDataStructures();
    iksolver_wdls.updateInternalDataStructures();
    iksolverpos.updateInternalDataStructures();
    iksolverpos2.updateInternalDataStructures();
    iksolverpos3.updateInternalDataStructures();
    idsolver1.updateInternalDataStructures();
    idsolver2.updateInternalDataStructures();
    dynparam.updateInternalDataStructures();

    CPPUNIT_ASSERT_EQUAL((int)SolverI::E_SIZE_MISMATCH,fksolverpos.JntToCart(q_in, T, chain2.getNrOfSegments()));
    CPPUNIT_ASSERT_EQUAL((int)SolverI::E_SIZE_MISMATCH,fksolvervel.JntToCart(q_in3, T2, chain2.getNrOfSegments()));
    CPPUNIT_ASSERT_EQUAL((int)SolverI::E_SIZE_MISMATCH, jacsolver1.JntToJac(q_in, jac, chain2.getNrOfSegments()));
    CPPUNIT_ASSERT_EQUAL((int)SolverI::E_SIZE_MISMATCH, jacdotsolver1.JntToJacDot(q_in3, t, chain2.getNrOfSegments()));
    CPPUNIT_ASSERT_EQUAL((int)SolverI::E_SIZE_MISMATCH, jacdotsolver1.JntToJacDot(q_in3, jac, chain2.getNrOfSegments()));
    CPPUNIT_ASSERT_EQUAL((int)SolverI::E_SIZE_MISMATCH, iksolver2.CartToJnt(q_in,t,q_out));
    CPPUNIT_ASSERT_EQUAL((int)SolverI::E_SIZE_MISMATCH, iksolver_pinv_givens2.CartToJnt(q_in,t,q_out));
    CPPUNIT_ASSERT_EQUAL((int)SolverI::E_SIZE_MISMATCH, iksolver_pinv_nso.CartToJnt(q_in,t,q_out));
    CPPUNIT_ASSERT_EQUAL((int)SolverI::E_SIZE_MISMATCH, iksolver_wdls.CartToJnt(q_in,t,q_out));
    CPPUNIT_ASSERT_EQUAL((int)SolverI::E_SIZE_MISMATCH, iksolverpos.CartToJnt(q_in,T,q_out));
    CPPUNIT_ASSERT_EQUAL((int)SolverI::E_SIZE_MISMATCH, iksolverpos2.CartToJnt(q_in,T,q_out));
    CPPUNIT_ASSERT_EQUAL((int)SolverI::E_SIZE_MISMATCH, iksolverpos3.CartToJnt(q_in,T,q_out));
    CPPUNIT_ASSERT_EQUAL((int)SolverI::E_SIZE_MISMATCH, idsolver1.CartToJnt(q_in,q_in2,q_out,wrenches,q_out2));
    CPPUNIT_ASSERT_EQUAL((int)SolverI::E_SIZE_MISMATCH, idsolver2.CartToJnt(q_in,q_in2,q_out,alpha,beta,wrenches,q_out2));
    CPPUNIT_ASSERT_EQUAL((int)SolverI::E_SIZE_MISMATCH, dynparam.JntToCoriolis(q_in, q_in2, q_out));
    CPPUNIT_ASSERT_EQUAL((int)SolverI::E_SIZE_MISMATCH, dynparam.JntToGravity(q_in, q_out));
    CPPUNIT_ASSERT_EQUAL((int)SolverI::E_SIZE_MISMATCH, dynparam.JntToMass(q_in, m));

    q_in.resize(chain2.getNrOfJoints());
    CPPUNIT_ASSERT_EQUAL((int)SolverI::E_SIZE_MISMATCH, jacsolver1.JntToJac(q_in, jac, chain2.getNrOfSegments()));
    CPPUNIT_ASSERT_EQUAL((int)SolverI::E_SIZE_MISMATCH, iksolver2.CartToJnt(q_in,t,q_out));
    CPPUNIT_ASSERT_EQUAL((int)SolverI::E_SIZE_MISMATCH, iksolver_pinv_givens2.CartToJnt(q_in,t,q_out));
    CPPUNIT_ASSERT_EQUAL((int)SolverI::E_SIZE_MISMATCH, iksolver_pinv_nso.CartToJnt(q_in,t,q_out));
    CPPUNIT_ASSERT_EQUAL((int)SolverI::E_SIZE_MISMATCH, iksolver_wdls.CartToJnt(q_in,t,q_out));
    CPPUNIT_ASSERT_EQUAL((int)SolverI::E_SIZE_MISMATCH, iksolverpos.CartToJnt(q_in,T,q_out));
    CPPUNIT_ASSERT_EQUAL((int)SolverI::E_SIZE_MISMATCH, iksolverpos2.CartToJnt(q_in,T,q_out));
    CPPUNIT_ASSERT_EQUAL((int)SolverI::E_SIZE_MISMATCH, iksolverpos3.CartToJnt(q_in,T,q_out));
    CPPUNIT_ASSERT_EQUAL((int)SolverI::E_SIZE_MISMATCH, iksolverpos2.CartToJnt(q_in,T,q_out));
    CPPUNIT_ASSERT_EQUAL((int)SolverI::E_SIZE_MISMATCH, iksolverpos3.CartToJnt(q_in,T,q_out));
    CPPUNIT_ASSERT_EQUAL((int)SolverI::E_SIZE_MISMATCH, idsolver1.CartToJnt(q_in,q_in2,q_out,wrenches,q_out2));
    CPPUNIT_ASSERT_EQUAL((int)SolverI::E_SIZE_MISMATCH, idsolver2.CartToJnt(q_in,q_in2,q_out,alpha,beta,wrenches,q_out2));
    CPPUNIT_ASSERT_EQUAL((int)SolverI::E_SIZE_MISMATCH, dynparam.JntToCoriolis(q_in, q_in2, q_out));
    CPPUNIT_ASSERT_EQUAL((int)SolverI::E_SIZE_MISMATCH, dynparam.JntToGravity(q_in, q_out));
    CPPUNIT_ASSERT_EQUAL((int)SolverI::E_SIZE_MISMATCH, dynparam.JntToMass(q_in, m));
    q_in2.resize(chain2.getNrOfJoints());
    CPPUNIT_ASSERT_EQUAL((int)SolverI::E_SIZE_MISMATCH, dynparam.JntToCoriolis(q_in, q_in2, q_out));
    CPPUNIT_ASSERT_EQUAL((int)SolverI::E_SIZE_MISMATCH, idsolver1.CartToJnt(q_in,q_in2,q_out,wrenches,q_out2));
    CPPUNIT_ASSERT_EQUAL((int)SolverI::E_SIZE_MISMATCH, idsolver2.CartToJnt(q_in,q_in2,q_out,alpha,beta,wrenches,q_out2));
    wrenches.resize(chain2.getNrOfSegments());
    CPPUNIT_ASSERT_EQUAL((int)SolverI::E_SIZE_MISMATCH, idsolver1.CartToJnt(q_in,q_in2,q_out,wrenches,q_out2));
    CPPUNIT_ASSERT_EQUAL((int)SolverI::E_SIZE_MISMATCH, idsolver2.CartToJnt(q_in,q_in2,q_out,alpha,beta,wrenches,q_out2));
    q_out2.resize(chain2.getNrOfSegments());
    CPPUNIT_ASSERT_EQUAL((int)SolverI::E_SIZE_MISMATCH, idsolver1.CartToJnt(q_in,q_in2,q_out,wrenches,q_out2));
    CPPUNIT_ASSERT_EQUAL((int)SolverI::E_SIZE_MISMATCH, idsolver2.CartToJnt(q_in,q_in2,q_out,alpha,beta,wrenches,q_out2));
    alpha.resize(nr_of_constraints);
    CPPUNIT_ASSERT_EQUAL((int)SolverI::E_SIZE_MISMATCH, idsolver2.CartToJnt(q_in,q_in2,q_out,alpha,beta,wrenches,q_out2));
    beta.resize(nr_of_constraints);
    CPPUNIT_ASSERT_EQUAL((int)SolverI::E_SIZE_MISMATCH, idsolver2.CartToJnt(q_in,q_in2,q_out,alpha,beta,wrenches,q_out2));
    jac.resize(chain2.getNrOfJoints());
    CPPUNIT_ASSERT_EQUAL((int)SolverI::E_SIZE_MISMATCH, jacdotsolver1.JntToJacDot(q_in3, jac, chain2.getNrOfSegments()));
    q_out.resize(chain2.getNrOfJoints());
    q_in3.resize(chain2.getNrOfJoints());
    m.resize(chain2.getNrOfJoints());
    CPPUNIT_ASSERT_EQUAL((int)SolverI::E_NOERROR,fksolverpos.JntToCart(q_in, T, chain2.getNrOfSegments()));
    CPPUNIT_ASSERT_EQUAL((int)SolverI::E_NOERROR,fksolvervel.JntToCart(q_in3, T2, chain2.getNrOfSegments()));
    CPPUNIT_ASSERT_EQUAL((int)SolverI::E_NOERROR, jacsolver1.JntToJac(q_in, jac, chain2.getNrOfSegments()));
    CPPUNIT_ASSERT_EQUAL((int)SolverI::E_NOERROR, jacdotsolver1.JntToJacDot(q_in3, jac, chain2.getNrOfSegments()));
    CPPUNIT_ASSERT_EQUAL((int)SolverI::E_NOERROR, jacdotsolver1.JntToJacDot(q_in3, t, chain2.getNrOfSegments()));
    CPPUNIT_ASSERT((int)SolverI::E_NOERROR <= iksolver2.CartToJnt(q_in,t,q_out));
    CPPUNIT_ASSERT((int)SolverI::E_NOERROR <= iksolver_pinv_givens2.CartToJnt(q_in,t,q_out));
    CPPUNIT_ASSERT((int)SolverI::E_NOERROR <= iksolver_pinv_nso.CartToJnt(q_in,t,q_out));
    CPPUNIT_ASSERT((int)SolverI::E_NOERROR <= iksolver_wdls.CartToJnt(q_in,t,q_out));
    CPPUNIT_ASSERT((int)SolverI::E_NOERROR <= iksolverpos.CartToJnt(q_in,T,q_out));
    CPPUNIT_ASSERT((int)SolverI::E_NOERROR <= iksolverpos2.CartToJnt(q_in,T,q_out));
    CPPUNIT_ASSERT((int)SolverI::E_NOERROR <= iksolverpos3.CartToJnt(q_in,T,q_out));
    CPPUNIT_ASSERT((int)SolverI::E_NOERROR <= iksolverpos2.CartToJnt(q_in,T,q_out));
    CPPUNIT_ASSERT((int)SolverI::E_NOERROR <= iksolverpos3.CartToJnt(q_in,T,q_out));
    CPPUNIT_ASSERT((int)SolverI::E_NOERROR <= idsolver1.CartToJnt(q_in,q_in2,q_out,wrenches,q_out2));
    CPPUNIT_ASSERT((int)SolverI::E_NOERROR <= idsolver2.CartToJnt(q_in,q_in2,q_out,alpha,beta,wrenches,q_out2));
    CPPUNIT_ASSERT((int)SolverI::E_NOERROR <= dynparam.JntToCoriolis(q_in, q_in2, q_out));
    CPPUNIT_ASSERT((int)SolverI::E_NOERROR <= dynparam.JntToGravity(q_in, q_out));
    CPPUNIT_ASSERT((int)SolverI::E_NOERROR <= dynparam.JntToMass(q_in, m));
}
void SolverTest::FkPosAndJacTest()
{
    ChainFkSolverPos_recursive fksolver1(chain1);
    ChainJntToJacSolver jacsolver1(chain1);
    FkPosAndJacLocal(chain1,fksolver1,jacsolver1);
    ChainFkSolverPos_recursive fksolver2(chain2);
    ChainJntToJacSolver jacsolver2(chain2);
    FkPosAndJacLocal(chain2,fksolver2,jacsolver2);
    ChainFkSolverPos_recursive fksolver3(chain3);
    ChainJntToJacSolver jacsolver3(chain3);
    FkPosAndJacLocal(chain3,fksolver3,jacsolver3);
    ChainFkSolverPos_recursive fksolver4(chain4);
    ChainJntToJacSolver jacsolver4(chain4);
    FkPosAndJacLocal(chain4,fksolver4,jacsolver4);
}

void SolverTest::FkVelAndJacTest()
{
    ChainFkSolverVel_recursive fksolver1(chain1);
    ChainJntToJacSolver jacsolver1(chain1);
    FkVelAndJacLocal(chain1,fksolver1,jacsolver1);
    ChainFkSolverVel_recursive fksolver2(chain2);
    ChainJntToJacSolver jacsolver2(chain2);
    FkVelAndJacLocal(chain2,fksolver2,jacsolver2);
    ChainFkSolverVel_recursive fksolver3(chain3);
    ChainJntToJacSolver jacsolver3(chain3);
    FkVelAndJacLocal(chain3,fksolver3,jacsolver3);
    ChainFkSolverVel_recursive fksolver4(chain4);
    ChainJntToJacSolver jacsolver4(chain4);
    FkVelAndJacLocal(chain4,fksolver4,jacsolver4);
}

void SolverTest::FkVelAndIkVelTest()
{
    //Chain1
    std::cout<<"square problem"<<std::endl;
    ChainFkSolverVel_recursive fksolver1(chain1);
    ChainIkSolverVel_pinv iksolver1(chain1);
    ChainIkSolverVel_pinv_givens iksolver_pinv_givens1(chain1);
    std::cout<<"KDL-SVD-HouseHolder"<<std::endl;
    FkVelAndIkVelLocal(chain1,fksolver1,iksolver1);
    std::cout<<"KDL-SVD-Givens"<<std::endl;
    FkVelAndIkVelLocal(chain1,fksolver1,iksolver_pinv_givens1);

    //Chain2
    std::cout<<"underdetermined problem"<<std::endl;
    ChainFkSolverVel_recursive fksolver2(chain2);
    ChainIkSolverVel_pinv iksolver2(chain2);
    ChainIkSolverVel_pinv_givens iksolver_pinv_givens2(chain2);
    std::cout<<"KDL-SVD-HouseHolder"<<std::endl;
    FkVelAndIkVelLocal(chain2,fksolver2,iksolver2);
    std::cout<<"KDL-SVD-Givens"<<std::endl;
    FkVelAndIkVelLocal(chain2,fksolver2,iksolver_pinv_givens2);

    //Chain3
    std::cout<<"overdetermined problem"<<std::endl;
    ChainFkSolverVel_recursive fksolver3(chain3);
    ChainIkSolverVel_pinv iksolver3(chain3);
    ChainIkSolverVel_pinv_givens iksolver_pinv_givens3(chain3);
    std::cout<<"KDL-SVD-HouseHolder"<<std::endl;
    FkVelAndIkVelLocal(chain3,fksolver3,iksolver3);
    std::cout<<"KDL-SVD-Givens"<<std::endl;
    FkVelAndIkVelLocal(chain3,fksolver3,iksolver_pinv_givens3);

    //Chain4
    std::cout<<"overdetermined problem"<<std::endl;
    ChainFkSolverVel_recursive fksolver4(chain4);
    ChainIkSolverVel_pinv iksolver4(chain4);
    ChainIkSolverVel_pinv_givens iksolver_pinv_givens4(chain4);
    std::cout<<"KDL-SVD-HouseHolder"<<std::endl;
    FkVelAndIkVelLocal(chain4,fksolver4,iksolver4);
    std::cout<<"KDL-SVD-Givens"<<std::endl;
    FkVelAndIkVelLocal(chain4,fksolver4,iksolver_pinv_givens4);
}

void SolverTest::FkPosAndIkPosTest()
{
    std::cout<<"square problem"<<std::endl;
    ChainFkSolverPos_recursive fksolver1(chain1);
    ChainIkSolverVel_pinv iksolver1v(chain1);
    ChainIkSolverVel_pinv_givens iksolverv_pinv_givens1(chain1);
    ChainIkSolverPos_NR iksolver1(chain1,fksolver1,iksolver1v);
    ChainIkSolverPos_NR iksolver1_givens(chain1,fksolver1,iksolverv_pinv_givens1,1000);

    std::cout<<"KDL-SVD-HouseHolder"<<std::endl;
    FkPosAndIkPosLocal(chain1,fksolver1,iksolver1);
    std::cout<<"KDL-SVD-Givens"<<std::endl;
    FkPosAndIkPosLocal(chain1,fksolver1,iksolver1_givens);

    std::cout<<"underdetermined problem"<<std::endl;
    ChainFkSolverPos_recursive fksolver2(chain2);
    ChainIkSolverVel_pinv iksolverv2(chain2);
    ChainIkSolverVel_pinv_givens iksolverv_pinv_givens2(chain2);
    ChainIkSolverPos_NR iksolver2(chain2,fksolver2,iksolverv2);
    ChainIkSolverPos_NR iksolver2_givens(chain2,fksolver2,iksolverv_pinv_givens2,1000);

    std::cout<<"KDL-SVD-HouseHolder"<<std::endl;
    FkPosAndIkPosLocal(chain2,fksolver2,iksolver2);
    std::cout<<"KDL-SVD-Givens"<<std::endl;
    FkPosAndIkPosLocal(chain2,fksolver2,iksolver2_givens);

    std::cout<<"overdetermined problem"<<std::endl;
    ChainFkSolverPos_recursive fksolver3(chain3);
    ChainIkSolverVel_pinv iksolverv3(chain3);
    ChainIkSolverVel_pinv_givens iksolverv_pinv_givens3(chain3);
    ChainIkSolverPos_NR iksolver3(chain3,fksolver3,iksolverv3);
    ChainIkSolverPos_NR iksolver3_givens(chain3,fksolver3,iksolverv_pinv_givens3,1000);

    std::cout<<"KDL-SVD-HouseHolder"<<std::endl;
    FkPosAndIkPosLocal(chain3,fksolver3,iksolver3);
    std::cout<<"KDL-SVD-Givens"<<std::endl;
    FkPosAndIkPosLocal(chain3,fksolver3,iksolver3_givens);

    std::cout<<"underdetermined problem with WGs segment constructor"<<std::endl;
    ChainFkSolverPos_recursive fksolver4(chain4);
    ChainIkSolverVel_pinv iksolverv4(chain4);
    ChainIkSolverVel_pinv_givens iksolverv_pinv_givens4(chain4);
    ChainIkSolverPos_NR iksolver4(chain4,fksolver4,iksolverv4,1000);
    ChainIkSolverPos_NR iksolver4_givens(chain4,fksolver4,iksolverv_pinv_givens4,1000);

    std::cout<<"KDL-SVD-HouseHolder"<<std::endl;
    FkPosAndIkPosLocal(chain4,fksolver4,iksolver4);
    std::cout<<"KDL-SVD-Givens"<<std::endl;
    FkPosAndIkPosLocal(chain4,fksolver4,iksolver4_givens);
}

void SolverTest::IkSingularValueTest()
{
	unsigned int maxiter = 30;
	double	eps = 1e-6 ;
	int maxiter_vel = 30;
	double	eps_vel = 0.1 ;
    Frame F, dF, F_des,F_solved;
	KDL::Twist F_error ;

	std::cout<<"KDL-IK Solver Tests for Near Zero SVs"<<std::endl;

    ChainFkSolverPos_recursive fksolver(motomansia10);
    ChainIkSolverVel_pinv ikvelsolver1(motomansia10,eps_vel,maxiter_vel);
    ChainIkSolverPos_NR iksolver1(motomansia10,fksolver,ikvelsolver1,maxiter,eps);
	unsigned int nj = motomansia10.getNrOfJoints();
    JntArray q(nj), q_solved(nj) ;


	std::cout<<"norminal case:  convergence"<<std::endl;

	q(0) = 0. ;
	q(1) = 0.5 ;
	q(2) = 0.4 ;
	q(3) = -PI_2 ;
	q(4) = 0. ;
	q(5) = 0. ;
	q(6) = 0. ;

	dF.M = KDL::Rotation::RPY(0.1, 0.1, 0.1) ;
	dF.p = KDL::Vector(0.01,0.01,0.01) ;

	CPPUNIT_ASSERT_EQUAL(0, fksolver.JntToCart(q,F));
	F_des = F * dF ;

	CPPUNIT_ASSERT_EQUAL((int)SolverI::E_NOERROR,
                         iksolver1.CartToJnt(q, F_des, q_solved));	// converges
    CPPUNIT_ASSERT_EQUAL((int)SolverI::E_NOERROR,
                         ikvelsolver1.getError());
	CPPUNIT_ASSERT_EQUAL((unsigned int)1,
                         ikvelsolver1.getNrZeroSigmas()) ;		//	1 singular value

	CPPUNIT_ASSERT_EQUAL(0, fksolver.JntToCart(q_solved,F_solved));
	F_error = KDL::diff(F_solved,F_des);
	CPPUNIT_ASSERT_EQUAL(F_des,F_solved);

	std::cout<<"nonconvergence:  pseudoinverse singular"<<std::endl;

	q(0) = 0. ;
	q(1) = 0.2 ;
	q(2) = 0.4 ;
	q(3) = -PI_2 ;
	q(4) = 0. ;
	q(5) = 0. ;
	q(6) = 0. ;

	dF.M = KDL::Rotation::RPY(0.1, 0.1, 0.1) ;
	dF.p = KDL::Vector(0.01,0.01,0.01) ;

	CPPUNIT_ASSERT_EQUAL(0, fksolver.JntToCart(q,F));
	F_des = F * dF ;

	CPPUNIT_ASSERT_EQUAL((int)SolverI::E_MAX_ITERATIONS_EXCEEDED,
                         iksolver1.CartToJnt(q,F_des,q_solved)); // no converge
	CPPUNIT_ASSERT_EQUAL((int)ChainIkSolverVel_pinv::E_CONVERGE_PINV_SINGULAR,
                         ikvelsolver1.getError());        	// truncated SV solution
	CPPUNIT_ASSERT_EQUAL((unsigned int)2,
                         ikvelsolver1.getNrZeroSigmas()) ;		//	2 singular values (jac pseudoinverse singular)

	std::cout<<"nonconvergence:  large displacement, low iterations"<<std::endl;

	q(0) = 0. ;
	q(1) = 0.5 ;
	q(2) = 0.4 ;
	q(3) = -PI_2 ;
	q(4) = 0. ;
	q(5) = 0. ;
	q(6) = 0. ;

	// big displacement
	dF.M = KDL::Rotation::RPY(0.2, 0.2, 0.2) ;
	dF.p = KDL::Vector(-0.2,-0.2, -0.2) ;

	// low iterations
	maxiter = 5 ;
    ChainIkSolverPos_NR iksolver2(motomansia10,fksolver,ikvelsolver1,maxiter,eps);

	CPPUNIT_ASSERT_EQUAL(0, fksolver.JntToCart(q,F));
	F_des = F * dF ;

    CPPUNIT_ASSERT_EQUAL((int)SolverI::E_MAX_ITERATIONS_EXCEEDED,
                         iksolver2.CartToJnt(q,F_des,q_solved));	//  does not converge
    CPPUNIT_ASSERT_EQUAL((int)SolverI::E_NOERROR,
                        ikvelsolver1.getError());
	CPPUNIT_ASSERT_EQUAL((unsigned int)1,
                         ikvelsolver1.getNrZeroSigmas()) ;		//	1 singular value (jac pseudoinverse exists)

	std::cout<<"nonconvergence:  fully singular"<<std::endl;

    q(0) = 0. ;
    q(1) = 0. ;
    q(2) = 0. ;
    q(3) = 0. ;
    q(4) = 0. ;
    q(5) = 0. ;
    q(6) = 0. ;

    dF.M = KDL::Rotation::RPY(0.1, 0.1, 0.1) ;
    dF.p = KDL::Vector(0.01,0.01,0.01) ;

    CPPUNIT_ASSERT_EQUAL((int)SolverI::E_NOERROR, fksolver.JntToCart(q,F));
    F_des = F * dF ;

    CPPUNIT_ASSERT_EQUAL((int)SolverI::E_MAX_ITERATIONS_EXCEEDED,
                         iksolver1.CartToJnt(q,F_des,q_solved)); // no converge
    CPPUNIT_ASSERT_EQUAL((int)ChainIkSolverVel_pinv::E_CONVERGE_PINV_SINGULAR,
                         ikvelsolver1.getError());        	// truncated SV solution
    CPPUNIT_ASSERT_EQUAL((unsigned int)3,
                         ikvelsolver1.getNrZeroSigmas());
}


void SolverTest::IkVelSolverWDLSTest()
{
	int maxiter = 30;
	double	eps = 0.1 ;
	double lambda = 0.1 ;

	std::cout<<"KDL-IK WDLS Vel Solver Tests for Near Zero SVs"<<std::endl;

	KDL::ChainIkSolverVel_wdls ikvelsolver(motomansia10,eps,maxiter) ;
	ikvelsolver.setLambda(lambda) ;
	unsigned int nj = motomansia10.getNrOfJoints();
    JntArray q(nj), dq(nj) ;

	KDL::Vector	v05(0.05,0.05,0.05) ;
	KDL::Twist dx(v05,v05) ;

	std::cout<<"smallest singular value is above threshold (no WDLS)"<<std::endl;

	q(0) = 0. ;
	q(1) = 0.5 ;
	q(2) = 0.4 ;
	q(3) = -PI_2 ;
	q(4) = 0. ;
	q(5) = 0. ;
	q(6) = 0. ;

	CPPUNIT_ASSERT_EQUAL((int)SolverI::E_NOERROR,
                         ikvelsolver.CartToJnt(q, dx, dq)) ;	// wdls mode
	CPPUNIT_ASSERT(1==ikvelsolver.getNrZeroSigmas()) ;		//	1 singular value


	std::cout<<"smallest singular value is below threshold (lambda is scaled)"<<std::endl;

	q(1) = 0.2 ;

	CPPUNIT_ASSERT_EQUAL((int)ChainIkSolverVel_wdls::E_CONVERGE_PINV_SINGULAR,
                         ikvelsolver.CartToJnt(q, dx, dq)) ;	// wdls mode
	CPPUNIT_ASSERT_EQUAL((unsigned int)2,ikvelsolver.getNrZeroSigmas()) ;		//	2 singular values
	CPPUNIT_ASSERT_EQUAL(ikvelsolver.getLambdaScaled(),
                         sqrt(1.0-(ikvelsolver.getSigmaMin()/eps)*(ikvelsolver.getSigmaMin()/eps))*lambda) ;

	std::cout<<"smallest singular value is zero (lambda_scaled=lambda)"<<std::endl;

	q(1) = 0.0 ;

    CPPUNIT_ASSERT_EQUAL((int)ChainIkSolverVel_wdls::E_CONVERGE_PINV_SINGULAR,
                         ikvelsolver.CartToJnt(q, dx, dq)) ;	// wdls mode
	CPPUNIT_ASSERT_EQUAL((unsigned int)2,ikvelsolver.getNrZeroSigmas()) ;		//	2 singular values
	CPPUNIT_ASSERT_EQUAL(ikvelsolver.getLambdaScaled(),lambda) ;	// full value

	// fully singular
	q(2) = 0.0 ;
	q(3) = 0.0 ;

    CPPUNIT_ASSERT_EQUAL((int)ChainIkSolverVel_wdls::E_CONVERGE_PINV_SINGULAR,
                         ikvelsolver.CartToJnt(q, dx, dq)) ;	// wdls mode
	CPPUNIT_ASSERT_EQUAL(4,(int)ikvelsolver.getNrZeroSigmas()) ;
	CPPUNIT_ASSERT_EQUAL(ikvelsolver.getLambdaScaled(),lambda) ;	// full value
}


void SolverTest::FkPosAndJacLocal(Chain& chain,ChainFkSolverPos& fksolverpos,ChainJntToJacSolver& jacsolver)
{
    double deltaq = 1E-4;

    Frame F1,F2;

    JntArray q(chain.getNrOfJoints());
    Jacobian jac(chain.getNrOfJoints());

    for(unsigned int i=0; i<chain.getNrOfJoints(); i++)
    {
        random(q(i));
    }

    jacsolver.JntToJac(q,jac);

    for (unsigned int i=0; i< q.rows() ; i++)
    {
        // test the derivative of J towards qi
        double oldqi = q(i);
        q(i) = oldqi+deltaq;
        CPPUNIT_ASSERT_EQUAL((int)SolverI::E_NOERROR, fksolverpos.JntToCart(q,F2));
        q(i) = oldqi-deltaq;
        CPPUNIT_ASSERT_EQUAL((int)SolverI::E_NOERROR, fksolverpos.JntToCart(q,F1));
        q(i) = oldqi;
        // check Jacobian :
        Twist Jcol1 = diff(F1,F2,2*deltaq);
        Twist Jcol2(Vector(jac(0,i),jac(1,i),jac(2,i)),
                    Vector(jac(3,i),jac(4,i),jac(5,i)));

        //CPPUNIT_ASSERT_EQUAL(true,Equal(Jcol1,Jcol2,epsJ));
        CPPUNIT_ASSERT_EQUAL(Jcol1,Jcol2);
    }
}

void SolverTest::FkVelAndJacLocal(Chain& chain, ChainFkSolverVel& fksolvervel, ChainJntToJacSolver& jacsolver)
{
    JntArray q(chain.getNrOfJoints());
    JntArray qdot(chain.getNrOfJoints());

    for(unsigned int i=0; i<chain.getNrOfJoints(); i++)
    {
        random(q(i));
        random(qdot(i));
    }
    JntArrayVel qvel(q,qdot);
    Jacobian jac(chain.getNrOfJoints());

    FrameVel cart;
    Twist t;

    jacsolver.JntToJac(qvel.q,jac);
    CPPUNIT_ASSERT_EQUAL((int)SolverI::E_NOERROR, fksolvervel.JntToCart(qvel,cart));
    MultiplyJacobian(jac,qvel.qdot,t);
    CPPUNIT_ASSERT_EQUAL(cart.deriv(),t);
}

void SolverTest::FkVelAndIkVelLocal(Chain& chain, ChainFkSolverVel& fksolvervel, ChainIkSolverVel& iksolvervel)
{

    JntArray q(chain.getNrOfJoints());
    JntArray qdot(chain.getNrOfJoints());

    for(unsigned int i=0; i<chain.getNrOfJoints(); i++)
    {
        random(q(i));
        random(qdot(i));
    }
    JntArrayVel qvel(q,qdot);
    JntArray qdot_solved(chain.getNrOfJoints());

    FrameVel cart;

    CPPUNIT_ASSERT_EQUAL((int)SolverI::E_NOERROR, fksolvervel.JntToCart(qvel,cart));

    CPPUNIT_ASSERT_EQUAL((int)SolverI::E_NOERROR, iksolvervel.CartToJnt(qvel.q,cart.deriv(),qdot_solved));

    qvel.deriv()=qdot_solved;

    if(chain.getNrOfJoints()<=6)
        CPPUNIT_ASSERT(Equal(qvel.qdot,qdot_solved,1e-5));
    else
    {
        FrameVel cart_solved;
        CPPUNIT_ASSERT_EQUAL((int)SolverI::E_NOERROR,fksolvervel.JntToCart(qvel,cart_solved));
        CPPUNIT_ASSERT(Equal(cart.deriv(),cart_solved.deriv(),1e-5));
    }
}


void SolverTest::FkPosAndIkPosLocal(Chain& chain,ChainFkSolverPos& fksolverpos, ChainIkSolverPos& iksolverpos)
{
    JntArray q(chain.getNrOfJoints());
    for(unsigned int i=0; i<chain.getNrOfJoints(); i++)
    {
        random(q(i));
    }
    JntArray q_init(chain.getNrOfJoints());
    double tmp;
    for(unsigned int i=0; i<chain.getNrOfJoints(); i++)
    {
        random(tmp);
        q_init(i)=q(i)+0.1*tmp;
    }
    JntArray q_solved(q);

    Frame F1,F2;

    CPPUNIT_ASSERT_EQUAL((int)SolverI::E_NOERROR, fksolverpos.JntToCart(q,F1));
    CPPUNIT_ASSERT_EQUAL((int)SolverI::E_NOERROR, iksolverpos.CartToJnt(q_init,F1,q_solved));
    CPPUNIT_ASSERT_EQUAL((int)SolverI::E_NOERROR, fksolverpos.JntToCart(q_solved,F2));

    CPPUNIT_ASSERT_EQUAL(F1,F2);
    //CPPUNIT_ASSERT_EQUAL(q,q_solved);

}


void SolverTest::VereshchaginTest()
{

    Vector constrainXLinear(1.0, 0.0, 0.0);
    Vector constrainXAngular(0.0, 0.0, 0.0);
    Vector constrainYLinear(0.0, 0.0, 0.0);
    Vector constrainYAngular(0.0, 0.0, 0.0);
    // Vector constrainZLinear(0.0, 0.0, 0.0);
    //Vector constrainZAngular(0.0, 0.0, 0.0);
    Twist constraintForcesX(constrainXLinear, constrainXAngular);
    Twist constraintForcesY(constrainYLinear, constrainYAngular);
    //Twist constraintForcesZ(constrainZLinear, constrainZAngular);
    Jacobian alpha(1);
    //alpha.setColumn(0, constraintForcesX);
    alpha.setColumn(0, constraintForcesX);
    //alpha.setColumn(0, constraintForcesZ);

    //Acceleration energy at  the end-effector
    JntArray betha(1); //set to zero
    betha(0) = 0.0;
    //betha(1) = 0.0;
    //betha(2) = 0.0;

    //arm root acceleration
    Vector linearAcc(0.0, 10, 0.0); //gravitational acceleration along Y
    Vector angularAcc(0.0, 0.0, 0.0);
    Twist twist1(linearAcc, angularAcc);

    //external forces on the arm
    Vector externalForce1(0.0, 0.0, 0.0);
    Vector externalTorque1(0.0, 0.0, 0.0);
    Vector externalForce2(0.0, 0.0, 0.0);
    Vector externalTorque2(0.0, 0.0, 0.0);
    Wrench externalNetForce1(externalForce1, externalTorque1);
    Wrench externalNetForce2(externalForce2, externalTorque2);
    Wrenches externalNetForce;
    externalNetForce.push_back(externalNetForce1);
    externalNetForce.push_back(externalNetForce2);
    //~Definition of constraints and external disturbances
    //-------------------------------------------------------------------------------------//


    //Definition of solver and initial configuration
    //-------------------------------------------------------------------------------------//
    int numberOfConstraints = 1;
    ChainIdSolver_Vereshchagin constraintSolver(chaindyn, twist1, numberOfConstraints);

    //These arrays of joint values contain actual and desired values
    //actual is generated by a solver and integrator
    //desired is given by an interpolator
    //error is the difference between desired-actual
    //in this test only the actual values are printed.
    const int k = 1;
    JntArray jointPoses[k];
    JntArray jointRates[k];
    JntArray jointAccelerations[k];
    JntArray jointTorques[k];
    for (int i = 0; i < k; i++)
    {
        JntArray jointValues(chaindyn.getNrOfJoints());
        jointPoses[i] = jointValues;
        jointRates[i] = jointValues;
        jointAccelerations[i] = jointValues;
        jointTorques[i] = jointValues;
    }

    // Initial arm position configuration/constraint
    JntArray jointInitialPose(chaindyn.getNrOfJoints());
    jointInitialPose(0) = 0.0; // initial joint0 pose
    jointInitialPose(1) = PI/6.0; //initial joint1 pose, negative in clockwise
    //j0=0.0, j1=pi/6.0 correspond to x = 0.2, y = -0.7464
    //j0=2*pi/3.0, j1=pi/4.0 correspond to x = 0.44992, y = 0.58636

    //actual
    jointPoses[0](0) = jointInitialPose(0);
    jointPoses[0](1) = jointInitialPose(1);

    //~Definition of solver and initial configuration
    //-------------------------------------------------------------------------------------//


    //Definition of process main loop
    //-------------------------------------------------------------------------------------//
    //Time required to complete the task move(frameinitialPose, framefinalPose)
    double taskTimeConstant = 0.1;
    double simulationTime = 1*taskTimeConstant;
    double timeDelta = 0.01;

    const std::string msg = "Assertion failed, check matrix and array sizes";

    for (double t = 0.0; t <=simulationTime; t = t + timeDelta)
    {
        CPPUNIT_ASSERT_EQUAL((int)SolverI::E_NOERROR, constraintSolver.CartToJnt(jointPoses[0], jointRates[0], jointAccelerations[0], alpha, betha, externalNetForce, jointTorques[0]));

        //Integration(robot joint values for rates and poses; actual) at the given "instanteneous" interval for joint position and velocity.
        jointRates[0](0) = jointRates[0](0) + jointAccelerations[0](0) * timeDelta; //Euler Forward
        jointPoses[0](0) = jointPoses[0](0) + (jointRates[0](0) - jointAccelerations[0](0) * timeDelta / 2.0) * timeDelta; //Trapezoidal rule
        jointRates[0](1) = jointRates[0](1) + jointAccelerations[0](1) * timeDelta; //Euler Forward
        jointPoses[0](1) = jointPoses[0](1) + (jointRates[0](1) - jointAccelerations[0](1) * timeDelta / 2.0) * timeDelta;
        //printf("time, j0_pose, j1_pose, j0_rate, j1_rate, j0_acc, j1_acc, j0_constraintTau, j1_constraintTau \n");
        printf("%f          %f      %f       %f     %f       %f      %f     %f      %f\n", t, jointPoses[0](0), jointPoses[0](1), jointRates[0](0), jointRates[0](1), jointAccelerations[0](0), jointAccelerations[0](1), jointTorques[0](0), jointTorques[0](1));
    }
}

void SolverTest::FkPosVectTest()
{
    ChainFkSolverPos_recursive fksolver1(chain1);
    std::vector<Frame> v_out(chain1.getNrOfSegments());
    
    JntArray q(chain1.getNrOfJoints());
    JntArray qdot(chain1.getNrOfJoints());

    for(unsigned int i=0; i<chain1.getNrOfJoints(); i++)
    {
        random(q(i));
        random(qdot(i));
    }
    Frame f_out;
    fksolver1.JntToCart(q,v_out);
    fksolver1.JntToCart(q,f_out);
     
    CPPUNIT_ASSERT(Equal(v_out[chain1.getNrOfSegments()-1],f_out,1e-5));
}

void SolverTest::FkVelVectTest()
{
    ChainFkSolverVel_recursive fksolver1(chain1);
    std::vector<FrameVel> v_out(chain1.getNrOfSegments());
    
    JntArray q(chain1.getNrOfJoints());
    JntArray qdot(chain1.getNrOfJoints());

    for(unsigned int i=0; i<chain1.getNrOfJoints(); i++)
    {
        random(q(i));
        random(qdot(i));
    }
    JntArrayVel qvel(q,qdot);
    FrameVel f_out;
    fksolver1.JntToCart(qvel,v_out);
    fksolver1.JntToCart(qvel,f_out);
    
    CPPUNIT_ASSERT(Equal(v_out[chain1.getNrOfSegments()-1],f_out,1e-5));
}

void SolverTest::FdSolverDevelopmentTest()
{
    int ret;
    double eps=1.e-3;

    std::cout<<"KDL FD Solver Development Test for Motoman SIA10"<<std::endl;

    //  NOTE:  This is prototype code for the KDL forward dynamics solver class
    //         based on the Recurse Newton Euler Method:  ChainFdSolver_RNE

    //  Dynamics Solver
    Vector gravity(0.0, 0.0, -9.81);  // base frame
    ChainDynParam DynSolver = KDL::ChainDynParam(motomansia10dyn, gravity);

    unsigned int nj = motomansia10dyn.getNrOfJoints();
    unsigned int ns = motomansia10dyn.getNrOfSegments();

    // Joint position, velocity, and acceleration
    JntArray q(nj);
    JntArray qd(nj);
    JntArray qdd(nj);

    //  random
    q(0) = 0.2;
    q(1) = 0.6;
    q(2) = 1.;
    q(3) = 0.5;
    q(4) = -1.4;
    q(5) = 0.3;
    q(6) = -0.8;

    qd(0) = 1.;
    qd(1) = -2.;
    qd(2) = 3.;
    qd(3) = -4.;
    qd(4) = 5.;
    qd(5) = -6.;
    qd(6) = 7.;

    // Validate FK
    ChainFkSolverPos_recursive fksolver(motomansia10dyn);
    Frame f_out;
    fksolver.JntToCart(q,f_out);
    CPPUNIT_ASSERT(Equal(-0.547, f_out.p(0), eps));
    CPPUNIT_ASSERT(Equal(-0.301, f_out.p(1), eps));
    CPPUNIT_ASSERT(Equal(0.924, f_out.p(2), eps));
    CPPUNIT_ASSERT(Equal(0.503, f_out.M(0,0), eps));
    CPPUNIT_ASSERT(Equal(0.286, f_out.M(0,1), eps));
    CPPUNIT_ASSERT(Equal(-0.816, f_out.M(0,2), eps));
    CPPUNIT_ASSERT(Equal(-0.859, f_out.M(1,0), eps));
    CPPUNIT_ASSERT(Equal(0.269, f_out.M(1,1), eps));
    CPPUNIT_ASSERT(Equal(-0.436, f_out.M(1,2), eps));
    CPPUNIT_ASSERT(Equal(0.095, f_out.M(2,0), eps));
    CPPUNIT_ASSERT(Equal(0.920, f_out.M(2,1), eps));
    CPPUNIT_ASSERT(Equal(0.381, f_out.M(2,2), eps));

    // Validate Jacobian
    ChainJntToJacSolver jacsolver(motomansia10dyn);
    Jacobian jac(nj);
    jacsolver.JntToJac(q, jac);
    double Jac[6][7] =
        {{0.301,-0.553,0.185,0.019,0.007,-0.086,0.},
        {-0.547,-0.112,-0.139,-0.376,-0.037,0.063,0.},
        {0.,-0.596,0.105,-0.342,-0.026,-0.113,0.},
        {0.,0.199,-0.553,0.788,-0.615,0.162,-0.816},
        {0.,-0.980,-0.112,-0.392,-0.536,-0.803,-0.436},
        {1.,0.,0.825,0.475,0.578,-0.573,0.381}};
    for ( unsigned int i=0; i<6; i++ ) {
        for ( unsigned int j=0; j<nj; j++ ) {
            CPPUNIT_ASSERT(Equal(jac(i,j), Jac[i][j], eps));
        }
    }

    // Return values
    JntArray taugrav(nj);
    JntArray taucor(nj);
    JntSpaceInertiaMatrix H(nj), Heff(nj);

    // Compute Dynamics (torque in N-m)
    ret = DynSolver.JntToGravity(q, taugrav);
    if (ret < 0) std::cout << "KDL: inverse dynamics ERROR: " << ret << std::endl;
    CPPUNIT_ASSERT(Equal(0.000, taugrav(0), eps));
    CPPUNIT_ASSERT(Equal(-36.672, taugrav(1), eps));
    CPPUNIT_ASSERT(Equal(4.315, taugrav(2), eps));
    CPPUNIT_ASSERT(Equal(-11.205, taugrav(3), eps));
    CPPUNIT_ASSERT(Equal(0.757, taugrav(4), eps));
    CPPUNIT_ASSERT(Equal(1.780, taugrav(5), eps));
    CPPUNIT_ASSERT(Equal(0.000, taugrav(6), eps));

    ret = DynSolver.JntToCoriolis(q, qd, taucor);
    if (ret < 0) std::cout << "KDL: inverse dynamics ERROR: " << ret << std::endl;
    CPPUNIT_ASSERT(Equal(-15.523, taucor(0), eps));
    CPPUNIT_ASSERT(Equal(24.250, taucor(1), eps));
    CPPUNIT_ASSERT(Equal(-6.862, taucor(2), eps));
    CPPUNIT_ASSERT(Equal(6.303, taucor(3), eps));
    CPPUNIT_ASSERT(Equal(0.110, taucor(4), eps));
    CPPUNIT_ASSERT(Equal(-4.898, taucor(5), eps));
    CPPUNIT_ASSERT(Equal(-0.249, taucor(6), eps));

    ret = DynSolver.JntToMass(q, H);
    if (ret < 0) std::cout << "KDL: inverse dynamics ERROR: " << ret << std::endl;
    double Hexp[7][7] =
        {{6.8687,-0.4333,0.4599,0.6892,0.0638,-0.0054,0.0381},
        {-0.4333,8.8324,-0.5922,0.7905,0.0003,-0.0242,0.0265},
        {0.4599,-0.5922,3.3496,-0.0253,0.1150,-0.0243,0.0814},
        {0.6892,0.7905,-0.0253,3.9623,-0.0201,0.0087,-0.0291},
        {0.0638,0.0003,0.1150,-0.0201,1.1234,0.0029,0.0955},
        {-0.0054,-0.0242,-0.0243,0.0087,0.0029,1.1425,0},
        {0.0381,0.0265,0.0814,-0.0291,0.0955,0,1.1000}};
    for ( unsigned int i=0; i<nj; i++ ) {
        for ( unsigned int j=0; j<nj; j++ ) {
            CPPUNIT_ASSERT(Equal(H(i,j), Hexp[i][j], eps));
        }
    }

    // Inverse Dynamics:
    //   T = H * qdd + Tcor + Tgrav - J^T * Fext
    // Forward Dynamics
    //   1. Call JntToMass from ChainDynParam to get H
    //   2. Call ID with qdd=0 to get T=Tcor+Tgrav+J^T*Fext
    //   3. Calculate qdd = H^-1 * T
    KDL::ChainIdSolver_RNE IdSolver = KDL::ChainIdSolver_RNE(motomansia10dyn, gravity);

    // In tool coordinates
    Vector f(10,-20,30) ;
    Vector n(3,-4,5) ;
    Wrench f_tool(f,n);
    // In local link coordinates
    Wrenches f_ext(ns);
    for(unsigned int i=0;i<ns;i++){
        SetToZero(f_ext[i]);
    }
    f_ext[ns-1]=f_tool;

    JntArray Tnoninertial(nj);
    JntArray jntarraynull(nj);
    SetToZero(jntarraynull);
    IdSolver.CartToJnt(q, qd, jntarraynull, f_ext, Tnoninertial);
    CPPUNIT_ASSERT(Equal(-21.252, Tnoninertial(0), eps));
    CPPUNIT_ASSERT(Equal(-37.933, Tnoninertial(1), eps));
    CPPUNIT_ASSERT(Equal(-2.497, Tnoninertial(2), eps));
    CPPUNIT_ASSERT(Equal(-15.289, Tnoninertial(3), eps));
    CPPUNIT_ASSERT(Equal(-4.646, Tnoninertial(4), eps));
    CPPUNIT_ASSERT(Equal(-9.201, Tnoninertial(5), eps));
    CPPUNIT_ASSERT(Equal(-5.249, Tnoninertial(6), eps));

    // get acceleration using inverse symmetric matrix times vector
    Eigen::MatrixXd H_eig(nj,nj), L(nj,nj);
    Eigen::VectorXd Tnon_eig(nj), D(nj), r(nj), acc_eig(nj);
    for(unsigned int i=0;i<nj;i++){
        Tnon_eig(i) =  -Tnoninertial(i);
        for(unsigned int j=0;j<nj;j++){
            H_eig(i,j) =  H(i,j);
        }
    }
    ldl_solver_eigen(H_eig, Tnon_eig, L, D, r, acc_eig);
    for(unsigned int i=0;i<nj;i++){
        qdd(i) = acc_eig(i);
    }
    CPPUNIT_ASSERT(Equal(2.998, qdd(0), eps));
    CPPUNIT_ASSERT(Equal(4.289, qdd(1), eps));
    CPPUNIT_ASSERT(Equal(0.946, qdd(2), eps));
    CPPUNIT_ASSERT(Equal(2.518, qdd(3), eps));
    CPPUNIT_ASSERT(Equal(3.530, qdd(4), eps));
    CPPUNIT_ASSERT(Equal(8.150, qdd(5), eps));
    CPPUNIT_ASSERT(Equal(4.254, qdd(6), eps));
}

void SolverTest::FdSolverConsistencyTest()
{
    int ret;
    double eps=1.e-3;

    std::cout<<"KDL FD Solver Consistency Test for Motoman SIA10"<<std::endl;

    //  NOTE:  Compute the forward and inverse dynamics and test for consistency

    //  Forward Dynamics Solver
    Vector gravity(0.0, 0.0, -9.81);  // base frame
    KDL::ChainFdSolver_RNE FdSolver = KDL::ChainFdSolver_RNE(motomansia10dyn, gravity);

    unsigned int nj = motomansia10dyn.getNrOfJoints();
    unsigned int ns = motomansia10dyn.getNrOfSegments();

    // Joint position, velocity, and acceleration
    KDL::JntArray q(nj);
    KDL::JntArray qd(nj);
    KDL::JntArray qdd(nj);
    KDL::JntArray tau(nj);

    //  random
    q(0) = 0.2;
    q(1) = 0.6;
    q(2) = 1.;
    q(3) = 0.5;
    q(4) = -1.4;
    q(5) = 0.3;
    q(6) = -0.8;

    qd(0) = 1.;
    qd(1) = -2.;
    qd(2) = 3.;
    qd(3) = -4.;
    qd(4) = 5.;
    qd(5) = -6.;
    qd(6) = 7.;

    // actuator torques
    tau(0) = 50.;
    tau(1) = -20.;
    tau(2) = 10.;
    tau(3) = 40.;
    tau(4) = -60.;
    tau(5) = 15.;
    tau(6) = -10.;

    KDL::Vector f(10,-20,30) ;
    KDL::Vector n(3,-4,5) ;
    KDL::Wrench f_tool(f,n);
    // In local link coordinates
    KDL::Wrenches f_ext(ns);
    for(unsigned int i=0;i<ns;i++){
        SetToZero(f_ext[i]);
    }
    f_ext[ns-1]=f_tool;

    // Call FD function
    ret = FdSolver.CartToJnt(q, qd, tau, f_ext, qdd);
    if (ret < 0) std::cout << "KDL: forward dynamics ERROR: " << ret << std::endl;
    CPPUNIT_ASSERT(Equal(9.486, qdd(0), eps));
    CPPUNIT_ASSERT(Equal(1.830, qdd(1), eps));
    CPPUNIT_ASSERT(Equal(4.726, qdd(2), eps));
    CPPUNIT_ASSERT(Equal(11.665, qdd(3), eps));
    CPPUNIT_ASSERT(Equal(-50.108, qdd(4), eps));
    CPPUNIT_ASSERT(Equal(21.403, qdd(5), eps));
    CPPUNIT_ASSERT(Equal(-0.381, qdd(6), eps));

    // Check against ID solver for consistency
    KDL::ChainIdSolver_RNE IdSolver = KDL::ChainIdSolver_RNE(motomansia10dyn, gravity);
    KDL::JntArray torque(nj);
    IdSolver.CartToJnt(q, qd, qdd, f_ext, torque);
    for ( unsigned int i=0; i<nj; i++ )
    {
        CPPUNIT_ASSERT(Equal(torque(i), tau(i), eps));
    }

    return;
}

void SolverTest::LDLdecompTest()
{
    std::cout<<"LDL Solver Test"<<std::endl;
    double eps=1.e-6;

    //  Given A and b, solve Ax=b for x, where A is a symmetric real matrix
    //  https://en.wikipedia.org/wiki/Cholesky_decomposition
    Eigen::MatrixXd A(3,3), Aout(3,3);
    Eigen::VectorXd b(3);
    Eigen::MatrixXd L(3,3), Lout(3,3);
    Eigen::VectorXd d(3), dout(3);
    Eigen::VectorXd x(3), xout(3);
    Eigen::VectorXd r(3);  // temp variable used internally by ldl solver
    Eigen::MatrixXd Dout(3,3);  // diagonal matrix

    // Given
    A <<  4, 12, -16,
         12, 37, -43,
        -16, -43, 98;
    b << 28, 117, 98;
    // Results to verify
    L <<  1, 0, 0,
          3, 1, 0,
          -4, 5, 1;
    d << 4, 1, 9;
    x << 3, 8, 5;

    ldl_solver_eigen(A, b, Lout, dout, r, xout);

    for(int i=0;i<3;i++){
        for(int j=0;j<3;j++){
            CPPUNIT_ASSERT(Equal(L(i,j), Lout(i,j), eps));
        }
    }

    Dout.setZero();
    for(int i=0;i<3;i++){
        Dout(i,i) = dout(i);
    }

    // Verify solution for x
    for(int i=0;i<3;i++){
        CPPUNIT_ASSERT(Equal(xout(i), x(i), eps));
    }

    // Test reconstruction of A from LDL^T decomposition
    Aout = Lout * Dout * Lout.transpose();
    for(int i=0;i<3;i++){
        for(int j=0;j<3;j++){
            CPPUNIT_ASSERT(Equal(A(i,j), Aout(i,j), eps));
        }
    }

    return;
}
