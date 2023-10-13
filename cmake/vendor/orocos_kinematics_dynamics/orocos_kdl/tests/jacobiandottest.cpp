#include "jacobiandottest.hpp"

CPPUNIT_TEST_SUITE_REGISTRATION(JacobianDotTest);

using namespace KDL;

void JacobianDotTest::setUp(){}
void JacobianDotTest::tearDown(){}


namespace KDL{
    static const double L0 = 1.0;
    static const double L1 = 0.5;
    static const double L2 = 0.4;
    static const double L3 = 0;
    static const double L4 = 0;
    static const double L5 = 0;
    Chain d2(){
        Chain d2;
        d2.addSegment(Segment(Joint(Joint::RotZ),Frame(Vector(L0,0,0))));
        d2.addSegment(Segment(Joint(Joint::RotZ),Frame(Vector(L1,0,0))));
        return d2;
    }
    Chain d6(){
        Chain d6;
        d6.addSegment(Segment(Joint(Joint::RotZ),Frame(Vector(L0,0,0))));
        d6.addSegment(Segment(Joint(Joint::RotX),Frame(Vector(L1,0,0))));
        d6.addSegment(Segment(Joint(Joint::RotX),Frame(Vector(L2,0,0))));
        d6.addSegment(Segment(Joint(Joint::RotZ),Frame(Vector(L3,0,0))));
        d6.addSegment(Segment(Joint(Joint::RotX),Frame(Vector(L4,0,0))));
        d6.addSegment(Segment(Joint(Joint::RotZ),Frame(Vector(L5,0,0))));
        return d6;
    }
    Chain KukaLWR_DHnew(){
        Chain kukaLWR_DHnew;
        
        //joint 0
        kukaLWR_DHnew.addSegment(Segment(Joint(Joint::None),
                                  Frame::DH_Craig1989(0.0, 0.0, 0.31, 0.0)
                                  ));
        //joint 1
        kukaLWR_DHnew.addSegment(Segment(Joint(Joint::RotZ),
                                  Frame::DH_Craig1989(0.0, 1.5707963, 0.0, 0.0),
                                  Frame::DH_Craig1989(0.0, 1.5707963, 0.0, 0.0).Inverse()*RigidBodyInertia(2,
                                                                                                 Vector::Zero(),
                                                                                                 RotationalInertia(0.0,0.0,0.0115343,0.0,0.0,0.0))));
                                   
        //joint 2 
        kukaLWR_DHnew.addSegment(Segment(Joint(Joint::RotZ),
                                  Frame::DH_Craig1989(0.0, -1.5707963, 0.4, 0.0),
                                  Frame::DH_Craig1989(0.0, -1.5707963, 0.4, 0.0).Inverse()*RigidBodyInertia(2,
                                                                                                   Vector(0.0,-0.3120511,-0.0038871),
                                                                                                   RotationalInertia(-0.5471572,-0.0000302,-0.5423253,0.0,0.0,0.0018828))));
                                  
        //joint 3
        kukaLWR_DHnew.addSegment(Segment(Joint(Joint::RotZ),
                                  Frame::DH_Craig1989(0.0, -1.5707963, 0.0, 0.0),
                                  Frame::DH_Craig1989(0.0, -1.5707963, 0.0, 0.0).Inverse()*RigidBodyInertia(2,
                                                                                                   Vector(0.0,-0.0015515,0.0),
                                                                                                   RotationalInertia(0.0063507,0.0,0.0107804,0.0,0.0,-0.0005147))));
                                  
        //joint 4
        kukaLWR_DHnew.addSegment(Segment(Joint(Joint::RotZ),
                                  Frame::DH_Craig1989(0.0, 1.5707963, 0.39, 0.0),
                                  Frame::DH_Craig1989(0.0, 1.5707963, 0.39, 0.0).Inverse()*RigidBodyInertia(2,
                                                                                                   Vector(0.0,0.5216809,0.0),
                                                                                                   RotationalInertia(-1.0436952,0.0,-1.0392780,0.0,0.0,0.0005324))));
                                  
        //joint 5
        kukaLWR_DHnew.addSegment(Segment(Joint(Joint::RotZ),
                                  Frame::DH_Craig1989(0.0, 1.5707963, 0.0, 0.0),
                                  Frame::DH_Craig1989(0.0, 1.5707963, 0.0, 0.0).Inverse()*RigidBodyInertia(2,
                                                                                                   Vector(0.0,0.0119891,0.0),
                                                                                                   RotationalInertia(0.0036654,0.0,0.0060429,0.0,0.0,0.0004226))));
                                  
        //joint 6
        kukaLWR_DHnew.addSegment(Segment(Joint(Joint::RotZ),
                                  Frame::DH_Craig1989(0.0, -1.5707963, 0.0, 0.0),
                                  Frame::DH_Craig1989(0.0, -1.5707963, 0.0, 0.0).Inverse()*RigidBodyInertia(2,
                                                                                                   Vector(0.0,0.0080787,0.0),
                                                                                                   RotationalInertia(0.0010431,0.0,0.0036376,0.0,0.0,0.0000101))));
        //joint 7
        kukaLWR_DHnew.addSegment(Segment(Joint(Joint::RotZ),
                                   Frame::Identity(),
                                   RigidBodyInertia(2,
                                                                                                   Vector::Zero(),
                                                                                                   RotationalInertia(0.000001,0.0,0.0001203,0.0,0.0,0.0))));
        return kukaLWR_DHnew;
    }    
}


void changeRepresentation(Jacobian& J,const Frame& F_bs_ee,const int& representation)
{
    switch(representation)
    {
        case ChainJntToJacDotSolver::HYBRID:
            break;
        case ChainJntToJacDotSolver::BODYFIXED:
            // Ref Frame {ee}, Ref Point {ee}
            J.changeBase(F_bs_ee.M.Inverse());
            break;
        case ChainJntToJacDotSolver::INERTIAL:
            // Ref Frame {bs}, Ref Point {bs}
            J.changeRefPoint(-F_bs_ee.p);
            break;
    }
}
void Jdot_diff(const Jacobian& J_q,
                     const Jacobian& J_qdt,
                     const double& dt,
                     Jacobian& Jdot)
{
    assert(J_q.columns() == J_qdt.columns());
    assert(J_q.columns() == Jdot.columns());
    for(unsigned int l=0;l<6;l++)
    for(unsigned int c=0;c<J_q.columns();c++)
        Jdot(l,c) = (J_qdt(l,c) - J_q(l,c))/dt;
}

Jacobian Jdot_d2_symbolic(const JntArray& q,const JntArray& qdot)
{
    // Returns Jdot for the simple 2DOF arm 
    Jacobian Jdot(q.rows());
    SetToZero(Jdot);
    Jdot(0,0) =  -L1 * (qdot(0) + qdot(1))*cos(q(0)+q(1))-L0*cos(q(0))*qdot(0);
    Jdot(0,1) =  -L1 * (qdot(0) + qdot(1))*cos(q(0)+q(1));
    Jdot(1,0) =  -L1 * (qdot(0) + qdot(1))*sin(q(0)+q(1))-L0*sin(q(0))*qdot(0);
    Jdot(1,1) =  -L1 * (qdot(0) + qdot(1))*sin(q(0)+q(1));
    return Jdot;
}

Jacobian J_d2_symbolic(const JntArray& q,const JntArray& /*qdot*/)
{
    // Returns J for the simple 2DOF arm
    Jacobian J(q.rows());
    SetToZero(J);
    J(0,0) =  -L1 * sin(q(0)+q(1))-L0*sin(q(0));
    J(0,1) =  -L1 * sin(q(0)+q(1));
    J(1,0) =   L1 * cos(q(0)+q(1))+L0*cos(q(0));
    J(1,1) =   L1 * cos(q(0)+q(1));
    J(5,0) = J(5,1) = 1;
    return J;
}

JntArray diff(const JntArray& q,const JntArray& qdot,const double& dt)
{
    JntArray q_qdqt(q);
    for(unsigned int i=0; i<q.rows(); i++)
        q_qdqt(i) += dt*qdot(i);
    return q_qdqt;
}

void random(JntArray& q)
{
    for(unsigned int i=0; i<q.rows(); i++)
        random(q(i));
}

double compare_Jdot_Diff_vs_Solver(const Chain& chain,const double& dt,const int& representation,bool verbose)
{
    // This test verifies if the solvers gives approx. the same result as [ J(q+qdot*dot) - J(q) ]/dot
    JntArray q(chain.getNrOfJoints());
    JntArray qdot(chain.getNrOfJoints());
    JntArray q_dqdt(chain.getNrOfJoints());


    random(q);
    random(qdot);
    q_dqdt = diff(q,qdot,dt);
    
    ChainJntToJacDotSolver jdot_solver(chain);
    ChainJntToJacSolver j_solver(chain);
    ChainFkSolverPos_recursive fk_solver(chain);

    Frame F_bs_ee_q,F_bs_ee_q_dqdt;
    Jacobian jac_q(chain.getNrOfJoints()),
                jac_q_dqdt(chain.getNrOfJoints()),
                jdot_by_diff(chain.getNrOfJoints());
                
    j_solver.JntToJac(q,jac_q);
    j_solver.JntToJac(q_dqdt,jac_q_dqdt);  
    
    fk_solver.JntToCart(q,F_bs_ee_q);
    fk_solver.JntToCart(q_dqdt,F_bs_ee_q_dqdt);
    
    changeRepresentation(jac_q,F_bs_ee_q,representation);
    changeRepresentation(jac_q_dqdt,F_bs_ee_q_dqdt,representation);
    
    Jdot_diff(jac_q,jac_q_dqdt,dt,jdot_by_diff);

    Jacobian jdot_by_solver(chain.getNrOfJoints());
    jdot_solver.setRepresentation(representation);
    jdot_solver.JntToJacDot(JntArrayVel(q_dqdt,qdot),jdot_by_solver);

    Twist jdot_qdot_by_solver;
    MultiplyJacobian(jdot_by_solver,qdot,jdot_qdot_by_solver);

    Twist jdot_qdot_by_diff;
    MultiplyJacobian(jdot_by_diff,qdot,jdot_qdot_by_diff);
    
    if(verbose){
        std::cout << "Jdot diff:\n" << jdot_by_diff << "\n"
                  << "Jdot solver:\n" << jdot_by_solver << "\n"
                  << "Error:\n"
                  << jdot_qdot_by_diff-jdot_qdot_by_solver << "\n"
                  << q << "\n"
                  << qdot <<std::endl;
    }
    double err = jdot_qdot_by_diff.vel.Norm() - jdot_qdot_by_solver.vel.Norm()
                  + jdot_qdot_by_diff.rot.Norm() - jdot_qdot_by_solver.rot.Norm();
    return std::abs(err);
}

double compare_d2_Jdot_Symbolic_vs_Solver(bool verbose)
{
    Chain chain=d2();
    JntArray q(chain.getNrOfJoints());
    JntArray qdot(chain.getNrOfJoints());

    random(q);
    random(qdot);
    
    ChainJntToJacDotSolver jdot_solver(chain);
               
    Jacobian jdot_sym = Jdot_d2_symbolic(q,qdot);

    Jacobian jdot_by_solver(chain.getNrOfJoints());
    jdot_solver.JntToJacDot(JntArrayVel(q,qdot),jdot_by_solver);

    Twist jdot_qdot_by_solver;
    MultiplyJacobian(jdot_by_solver,qdot,jdot_qdot_by_solver);

    Twist jdot_qdot_sym;
    MultiplyJacobian(jdot_sym,qdot,jdot_qdot_sym);
    
    if(verbose){
        std::cout << "Jdot symbolic:\n" << jdot_sym << "\n"
                  << "Jdot solver:\n" << jdot_by_solver << "\n"
                  << "Error:\n" << jdot_qdot_sym-jdot_qdot_by_solver << "\n"
                  << q << "\n"
                  << qdot << std::endl;
    }
    double err = jdot_qdot_sym.vel.Norm() - jdot_qdot_by_solver.vel.Norm()
                  + jdot_qdot_sym.rot.Norm() - jdot_qdot_by_solver.rot.Norm();
    return std::abs(err);    
}

bool runTest(const Chain& chain,const int& representation)
{
    bool success = true;
    bool verbose = false;
    double err;
    bool print_err = false;
    
    for(double dt=1e-6;dt<0.1;dt*=10)
    {
        double eps_diff_vs_solver = 4.0*dt; // Apparently :)

        for(int i=0; i<100 ; ++i)
        {
            err = compare_Jdot_Diff_vs_Solver(chain,dt,representation,verbose);

            success &= err<=eps_diff_vs_solver;

            if(!success || print_err){
                std::cout << "dt: "<< dt << "\n"
                          << "err: "<< err << "\n"
                          << "eps_diff_vs_solver: " << eps_diff_vs_solver << std::endl;
                if(!success)
                    break;
            }
        }
    }

    return success;
}

void JacobianDotTest::testD2DiffHybrid(){
    CPPUNIT_ASSERT(runTest(d2(),0));
}
void JacobianDotTest::testD6DiffHybrid(){
    CPPUNIT_ASSERT(runTest(d6(),0));
}
void JacobianDotTest::testKukaDiffHybrid(){
    CPPUNIT_ASSERT(runTest(KukaLWR_DHnew(),0));
}

void JacobianDotTest::testD2DiffInertial(){
    CPPUNIT_ASSERT(runTest(d2(),2));
}
void JacobianDotTest::testD6DiffInertial(){
    CPPUNIT_ASSERT(runTest(d6(),2));
}
void JacobianDotTest::testKukaDiffInertial(){
    CPPUNIT_ASSERT(runTest(KukaLWR_DHnew(),2));
}
void JacobianDotTest::testD2DiffBodyFixed(){
    CPPUNIT_ASSERT(runTest(d2(),1));
}
void JacobianDotTest::testD6DiffBodyFixed(){
    CPPUNIT_ASSERT(runTest(d6(),1));
}
void JacobianDotTest::testKukaDiffBodyFixed(){
    CPPUNIT_ASSERT(runTest(KukaLWR_DHnew(),1));
}

void JacobianDotTest::testD2Symbolic(){
    // This test verifies if the solvers gives the same result as the symbolic Jdot (Hybrid only)
    bool success = true;
    bool verbose = false;
    double err_d2_sym;
    bool print_err = false;
    
    double eps_sym_vs_solver = 1e-10;
    
    for(int i=0; i<100; ++i)
    {
        err_d2_sym = compare_d2_Jdot_Symbolic_vs_Solver(verbose);
        
        success &= err_d2_sym <= eps_sym_vs_solver;
        
        if(!success || print_err){
            std::cout << "err_d2_sym: " << err_d2_sym << "\n"
            << "eps_sym_vs_solver: "<< eps_sym_vs_solver <<std::endl;
            if(!success)
                break;
        }
            
    }
    
    CPPUNIT_ASSERT(success);
}
