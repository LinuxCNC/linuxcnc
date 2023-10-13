#include <kdl/kinfam/serialchain.hpp>
#include <kdl/frames.hpp>
#include <kdl/framevel.hpp>
#include <kdl/frames_io.hpp>
#include <kdl/kinfam/crs450.hpp>
#include <kdl/kinfam/kuka160.hpp>
#include <kdl/kinfam/serialchaincartpos2jnt.hpp>
#include <kdl/kinfam/lineartransmission.hpp>

using namespace KDL;



void CompareFamilies(KinematicFamily* KF1,KinematicFamily* KF2) {
	Jnt2CartPos* jnt2cartpos1 = KF1->createJnt2CartPos();
	Jnt2CartPos* jnt2cartpos2 = KF2->createJnt2CartPos();
	JointVector q(6);
   	q[0] = 0*KDL::deg2rad;
   	q[1] = 10*KDL::deg2rad;
   	q[2] = 20*KDL::deg2rad;
   	q[3] = 30*KDL::deg2rad;
   	q[4] = 40*KDL::deg2rad;
   	q[5] = 50*KDL::deg2rad;
	Frame F1,F2;
	jnt2cartpos1->evaluate(q);
	jnt2cartpos1->getFrame(F1);
	jnt2cartpos2->evaluate(q);
	jnt2cartpos2->getFrame(F2);
	if (!Equal(F1,F2,1E-7)) {
	  std::cout << "the two kinematic families do not give the same result." << std::endl;
	  std::cout << "Result of first kinematic family " << std::endl;
	  std::cout << F1 << std::endl;
	  std::cout << "Result of second kinematic family " << std::endl;
	  std::cout << F2 << std::endl;
	  exit(1);
	}
    delete jnt2cartpos1;
    delete jnt2cartpos2;
}

//
// Test whether Jnt2CartPos and CartPos2Jnt give a consistent result.
//
class TestForwardAndInverse {
	KinematicFamily*      family;
	Jnt2CartPos*          jnt2cartpos;
	Frame			      F_base_ee;
	Frame			      F_base_ee2;
	JointVector           q_solved;
	JointVector           q_initial;
public:
	CartPos2Jnt*          cartpos2jnt;
public:
	static void TestFamily(KinematicFamily* _family) {
		JointVector q_initial(6);
		q_initial[0] = 0*KDL::deg2rad;
		q_initial[1] = 0*KDL::deg2rad;
		q_initial[2] = 90*KDL::deg2rad;
		q_initial[3] = 0*KDL::deg2rad;
		q_initial[4] = 90*KDL::deg2rad;
		q_initial[5] = 0*KDL::deg2rad;
		TestForwardAndInverse testobj(_family,q_initial);
		JointVector q(6);
   		q[0] = 0*KDL::deg2rad;
   		q[1] = 10*KDL::deg2rad;
   		q[2] = 20*KDL::deg2rad;
   		q[3] = 30*KDL::deg2rad;
   		q[4] = 40*KDL::deg2rad;
   		q[5] = 50*KDL::deg2rad;
		testobj.test(q);
		//std::cout << "number of iterations " << ((SerialChainCartPos2Jnt*)testobj.cartpos2jnt)->iter << std::endl;
		q[0] = -10*KDL::deg2rad;
   		q[1] = -10*KDL::deg2rad;
   		q[2] = 40*KDL::deg2rad;
   		q[3] = -30*KDL::deg2rad;
   		q[4] = 20*KDL::deg2rad;
   		q[5] = 60*KDL::deg2rad;
		testobj.test(q);
		//std::cout << "number of iterations " << ((SerialChainCartPos2Jnt*)testobj.cartpos2jnt)->iter << std::endl;
	}


//
// Test whether Jnt2CartPos and Jnt2Jac give consistent
// results.
//
class TestForwardPosAndJac {
	KinematicFamily* family;
	Jnt2CartPos*     jnt2cartpos;
	Jnt2Jac*         jnt2jac;
    Jacobian<Frame>  FJ_base_ee;
	Frame			 F_base_ee1;
	Frame			 F_base_ee2;
public:
	static void TestFamily(KinematicFamily* _family) {
		TestForwardPosAndJac testobj(_family);
		JointVector q(6);
   	 	q[0] = 0*KDL::deg2rad;
   	 	q[1] = 10*KDL::deg2rad;
   	 	q[2] = 20*KDL::deg2rad;
   	 	q[3] = 30*KDL::deg2rad;
   	 	q[4] = 40*KDL::deg2rad;
   	 	q[5] = 50*KDL::deg2rad;
		testobj.test(q);
		q[0] = -50*KDL::deg2rad;
   	 	q[1] = -10*KDL::deg2rad;
   	 	q[2] = 20*KDL::deg2rad;
   	 	q[3] = -30*KDL::deg2rad;
   	 	q[4] = 20*KDL::deg2rad;
   	 	q[5] = 110*KDL::deg2rad;
		testobj.test(q);
	}

    TestForwardPosAndJac(KinematicFamily* _family) :
		family(_family),
		jnt2cartpos(_family->createJnt2CartPos()),
		jnt2jac(_family->createJnt2Jac()),
        FJ_base_ee(_family->nrOfJoints())
    {
		// the transformations should exist :
		assert( jnt2jac != 0);
		assert( jnt2cartpos != 0);
    }

    int test(JointVector& q) {
        double deltaq = 1E-4;
        double epsJ   = 1E-4;
		if (jnt2jac->evaluate(q)!=0) return 1;
		jnt2jac->getJacobian(FJ_base_ee);
        for (int i=0; i< q.size() ;i++) {
            // test the derivative of J towards qi
            double oldqi = q[i];
            q[i] = oldqi+deltaq;
			if (jnt2cartpos->evaluate(q)!=0) return 1;
			jnt2cartpos->getFrame(F_base_ee2);
            q[i] = oldqi-deltaq;
			if (jnt2cartpos->evaluate(q)!=0) return 1;
			jnt2cartpos->getFrame(F_base_ee1);
            q[i] = oldqi;
            // check Jacobian :
            Twist Jcol = diff(F_base_ee1,F_base_ee2,2*deltaq);
            if (!Equal(Jcol,FJ_base_ee.deriv(i),epsJ)) {
                std::cout << "Difference between symbolic and numeric calculation of Jacobian for column "
                          << i << std::endl;
                std::cout << "Numeric " << Jcol << std::endl;
                std::cout << "Symbolic " << FJ_base_ee.deriv(i) << std::endl;
                exit(1);
            }
        }
    }

    ~TestForwardPosAndJac() {
		delete jnt2cartpos;
		delete jnt2jac;
    }
};


//
// Test whether Jnt2CartVel and Jnt2Jac give consistent
// results.
//
class TestCartVelAndJac {
	KinematicFamily* family;
	Jnt2CartVel*     jnt2cartvel;
	Jnt2Jac*         jnt2jac;
    Jacobian<Frame>  FJ_base_ee;
	FrameVel		 F_base_ee;
public:
	static void TestFamily(KinematicFamily* _family) {
		TestCartVelAndJac testobj(_family);
		JointVector qdot(6);
        qdot[0] = 0.1;
        qdot[1] = 0.2;
        qdot[2] = -0.3;
        qdot[3] = 0.4;
        qdot[4] = -0.5;
        qdot[5] = 0.6;
		JointVector q(6);
   	 	q[0] = 0*KDL::deg2rad;
   	 	q[1] = 10*KDL::deg2rad;
   	 	q[2] = 20*KDL::deg2rad;
   	 	q[3] = 30*KDL::deg2rad;
   	 	q[4] = 40*KDL::deg2rad;
   	 	q[5] = 50*KDL::deg2rad;
        std::cout << "q[1] = " << q[1] << std::endl;
		testobj.test(q,qdot);
		q[0] = -50*KDL::deg2rad;
   	 	q[1] = -10*KDL::deg2rad;
   	 	q[2] = 20*KDL::deg2rad;
   	 	q[3] = -30*KDL::deg2rad;
   	 	q[4] = 20*KDL::deg2rad;
   	 	q[5] = 110*KDL::deg2rad;
		testobj.test(q,qdot);
	}

    TestCartVelAndJac(KinematicFamily* _family) :
		family(_family),
		jnt2cartvel(_family->createJnt2CartVel()),
		jnt2jac(_family->createJnt2Jac()),
        FJ_base_ee(_family->nrOfJoints())
    {
		// the transformations should exist :
		assert( jnt2jac != 0);
		assert( jnt2cartvel != 0);
    }

    int test(JointVector& q,JointVector& qdot) {
        std::cout << "Testing whether Jnt2CartVel and Jnt2Jac are consistent " << std::endl;
        std::cout << "q[1] = " << q[1] << std::endl;
        double deltaq = 1E-4;
        double epsJ   = 1E-4;
		int result;
        result = jnt2jac->evaluate(q);
        assert(result==0);
		jnt2jac->getJacobian(FJ_base_ee);
        result = jnt2cartvel->evaluate(q,qdot);
        jnt2cartvel->getFrameVel(F_base_ee);
        assert(result==0);
        Twist t = Twist::Zero();
        for (int i=0; i< q.size() ;i++) {
            t += FJ_base_ee.deriv(i)*qdot[i];
        }
        if (!Equal(t,F_base_ee.GetTwist(),1E-6)) {
                std::cout << "Difference between the resuls"<< std::endl;
                std::cout << "via the Jacobian  " << t << std::endl;
                std::cout << "via the jnt2cartvel transformation  " << F_base_ee.GetTwist() << std::endl;
                exit(1);
        }
    }

    ~TestCartVelAndJac() {
		delete jnt2cartvel;
		delete jnt2jac;
    }
};



//
// Test whether Jnt2CartVel and CartVel2Jnt give consistent
// results.
//
class TestCartVelAndInverse {
	KinematicFamily* family;
	Jnt2CartVel*     jnt2cartvel;
	CartVel2Jnt*     cartvel2jnt;
	FrameVel		 F_base_ee;
    JointVector      qdot2;
public:
	static void TestFamily(KinematicFamily* _family) {
		TestCartVelAndInverse testobj(_family);
		JointVector qdot(6);
        qdot[0] = 0.1;
        qdot[1] = 0.2;
        qdot[2] = -0.3;
        qdot[3] = 0.4;
        qdot[4] = -0.5;
        qdot[5] = 0.6;
        JointVector q(6);
   	 	q[0] = 0*KDL::deg2rad;
   	 	q[1] = 10*KDL::deg2rad;
   	 	q[2] = 20*KDL::deg2rad;
   	 	q[3] = 30*KDL::deg2rad;
   	 	q[4] = 40*KDL::deg2rad;
   	 	q[5] = 50*KDL::deg2rad;
        testobj.test(q,qdot);
		q[0] = -50*KDL::deg2rad;
   	 	q[1] = -10*KDL::deg2rad;
   	 	q[2] = 20*KDL::deg2rad;
   	 	q[3] = -30*KDL::deg2rad;
   	 	q[4] = 20*KDL::deg2rad;
   	 	q[5] = 110*KDL::deg2rad;
		testobj.test(q,qdot);
	}

    TestCartVelAndInverse(KinematicFamily* _family) :
		family(_family),
		jnt2cartvel(_family->createJnt2CartVel()),
		cartvel2jnt(_family->createCartVel2Jnt()),
        qdot2(_family->nrOfJoints())
    {
		// the transformations should exist :
		assert( cartvel2jnt != 0);
		assert( jnt2cartvel != 0);
    }

    int test(const JointVector& q,const JointVector& qdot) {
        std::cout << "Testing whether Jnt2CartVel and CartVel2Jnt are consistent " << std::endl;
        double epsJ   = 1E-7;
		int result;

        result = jnt2cartvel->evaluate(q,qdot);
        assert(result==0);
        jnt2cartvel->getFrameVel(F_base_ee);

        cartvel2jnt->setTwist(F_base_ee.GetTwist());
        result = cartvel2jnt->evaluate(q, qdot2);
        assert(result==0);

        for (int i=0;i<qdot.size();++i) {
           if (fabs(qdot[i]-qdot2[i])>epsJ)  {
               std::cout << " original joint velocities and calculated joint velocities do not match" << std::endl;
               //std::cerr << " original joint velocities and calculated joint velocities do not match" << std::endl;
               for (int j=0;j<qdot.size();j++) {
                    std::cout << "qdot["<<j<<"]="<<qdot[j]<<"  and qdot2["<<j<<"]="<<qdot2[j] << std::endl;
                }
                std::cout << "Frame : " << F_base_ee.GetFrame() << std::endl;
                std::cout << "Twist : " << F_base_ee.GetTwist() << std::endl;
               exit(1);
           }
        }
    }

    ~TestCartVelAndInverse() {
		delete jnt2cartvel;
		delete cartvel2jnt;
    }
};








/**
 * a kinematic family class  with some non-trivial linear transmission.
 */
class CRS450_exp: public SerialChain {
	public:
	explicit CRS450_exp(int jointoffset=0) :
		SerialChain("CRS450", 6, jointoffset,new LinearTransmission(6) )
	{
		double L1 = 0.33;
		double L2 = 0.305;
		double L3 = 0.33;
		double L4 = 0.176;
		LinearTransmission* tr = (LinearTransmission*)transmission;
		tr->setTransmission(2,2.0,1.0);
		tr->setTransmission(3,3.0,2.0);
        addJoint(new JointRotZ(Frame::Identity())); // j1
        addJoint(new JointRotY(Frame(Rotation::Identity(),Vector(0,0,L1))));// j2
        addJoint(new JointRotY(Frame(Rotation::Identity(),Vector(0,0,L2))));// j3
        addJoint(new JointRotZ(Frame(Rotation::Identity(),Vector(0,0,0)))); // j4
        addJoint(new JointRotY(Frame(Rotation::Identity(),Vector(0,0,L3))));// j5
        addJoint(new JointRotZ(Frame(Rotation::Identity(),Vector(0,0,0)))); // j6
        setLastJointToEE(Frame(Rotation::Identity(),Vector(0,0,L4)));
	};
};


int main(int argc,char* argv[]) {
	KinematicFamily* family1,*family2,*family3,*family4;
	std::cout << std::endl << "Tests on CRS450 " << std::endl;
	family1 = new CRS450();
	TestForwardAndInverse::TestFamily(family1);
	TestForwardPosAndJac::TestFamily(family1);
    TestCartVelAndJac::TestFamily(family1);
    TestCartVelAndInverse::TestFamily(family1);//
	std::cout <<std::endl <<  "Tests on CRS450_exp " << std::endl;
	family2 = new CRS450_exp();
	TestForwardAndInverse::TestFamily(family2);
	TestForwardPosAndJac::TestFamily(family2);
    TestCartVelAndJac::TestFamily(family2);
    TestCartVelAndInverse::TestFamily(family2);//
	std::cout << std::endl << "Tests on CRS450Feath " << std::endl;
	family3 = new CRS450Feath();
	TestForwardAndInverse::TestFamily(family3);
	TestForwardPosAndJac::TestFamily(family3);
    TestCartVelAndJac::TestFamily(family3);
    TestCartVelAndInverse::TestFamily(family3);//
	std::cout << std::endl << "Comparing the kinematic families "<< std::endl;
	CompareFamilies(family1,family3);
	std::cout << std::endl << "Tests on CRS450Feath->createSerialChain " << std::endl;
    family4 = ((ZXXZXZ*)family3)->createSerialChain();
	TestForwardAndInverse::TestFamily(family4);
	TestForwardPosAndJac::TestFamily(family4);
    TestCartVelAndJac::TestFamily(family4);
    TestCartVelAndInverse::TestFamily(family4);//



    // testing the clone functionality (valgrind)
	KinematicFamily* family1b,*family2b,*family3b,*family4b;
    family1b = family1->clone();
    family2b = family2->clone();
    family3b = family3->clone();
    family4b = family4->clone();

    delete family4;
	delete family3;
	delete family2;
	delete family1;

    delete family4b;
	delete family3b;
	delete family2b;
	delete family1b;

	return 0;
}

