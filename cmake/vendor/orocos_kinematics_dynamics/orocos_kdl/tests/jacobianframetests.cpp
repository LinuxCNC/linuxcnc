#include <kdl/frames.hpp>
#include <kdl/frames_io.hpp>
#include <kdl/framevel.hpp>
#include <kdl/framevel_io.hpp>
#include <kdl/jacobianexpr.hpp>
#include <kdl/jacobianframe.hpp>
#include <kdl/jacobianframevel.hpp>
#include "jacobianframetests.hpp"

namespace KDL {


void checkDiffs() {
    KDL_CTX;
	double adouble,bdouble;
	Vector avector,bvector;
	Twist  atwist,btwist;
	Rotation arot,brot;
	Frame  aframe,bframe;
	// large deviations :
	random(adouble);random(bdouble);
	random(avector);random(bvector);
	random(atwist);random(btwist);
	random(arot);random(brot);
	random(aframe);random(bframe);
	double dt=0.1;
	double eps = 1E-10;
	checkEqual(bdouble,addDelta(adouble,diff(adouble,bdouble,dt),dt),eps);
	checkEqual(bvector,addDelta(avector,diff(avector,bvector,dt),dt),eps);
	checkEqual(btwist,addDelta(atwist,diff(atwist,btwist,dt),dt),eps);
	checkEqual(brot,addDelta(arot,diff(arot,brot,dt),dt),eps);
	checkEqual(bframe,addDelta(aframe,diff(aframe,bframe,dt),dt),eps);

	// small deviations
	dt = 0.00001;
	double ddouble;
	Vector dvector;
	Twist  dtwist;    
	Vector drot;	// there is no error in the naming ...
	Twist  dframe;
	random(ddouble);random(dvector);random(dtwist);random(drot);random(dframe);
	checkEqual(ddouble,diff(adouble,addDelta(adouble,ddouble,dt),dt),eps);
	checkEqual(dvector,diff(avector,addDelta(avector,dvector,dt),dt),eps);
	checkEqual(dtwist,diff(atwist,addDelta(atwist,dtwist,dt),dt),eps);
	checkEqual(drot,diff(arot,addDelta(arot,drot,dt),dt),eps);
	checkEqual(dframe,diff(aframe,addDelta(aframe,dframe,dt),dt),eps);
}


void checkEulerZYX() {
	// Take care of the order of the arguments :
    KDL_CTX;
	int nrofcol=3;
	Jacobian<Rotation> R(nrofcol);
	Jacobian<Rotation> R2(nrofcol);
	Jacobian<double> alpha(nrofcol);
	random(alpha);
	Jacobian<double> beta(nrofcol);
	random(beta);
	Jacobian<double> gamma(nrofcol);
	random(gamma);
	SetEulerZYX(gamma,beta,alpha,R);	
	// now we have a random frame R.
	int result=GetEulerZYX(R,gamma,beta,alpha);
	assert( result == 0);
	SetEulerZYX(gamma,beta,alpha,R2);	
	checkEqual(R.value(),R2.value(),0.0001);
	for (int i=0;i<nrofcol;i++) {
		checkEqual(R.deriv(i),R2.deriv(i),0.0001);
	}
	double dt= 1E-8;
	double eps = 1E-4;
	std::cout << "Tests with numerical derivatives for EulerZYX " << std::endl;
	for (int i=0;i<nrofcol;i++) {
		Rotation R2 = Rotation::EulerZYX(alpha.value(),beta.value(),gamma.value());
		checkEqual( R.deriv(i), 
					diff(
						R.value(), 
						Rotation::EulerZYX(
							alpha.value()+ alpha.deriv(i)*dt, 
							beta.value() + beta.deriv(i)*dt, 
							gamma.value()+ gamma.deriv(i)*dt),
						dt),
					eps);
	}		
}


void checkFrameOps() {
    KDL_CTX;
	checkDiffs();

	checkUnary<OpInverse,Frame>::check();
	checkUnary<OpNegate,Vector>::check();
	checkUnary<OpNorm,Vector>::check();
    checkUnary<OpRotX,double>::check();
    checkUnary<OpRotY,double>::check();
    checkUnary<OpRotZ,double>::check();
    checkUnary<OpUnitX,Rotation>::check();
    checkUnary<OpUnitY,Rotation>::check();
    checkUnary<OpUnitZ,Rotation>::check();
    checkUnary<OpInverse,Rotation>::check();
    checkUnary<OpNegate,Twist>::check();

	checkBinary<OpMult,Frame,Frame>::check();
	checkBinary<OpMult,Frame,Vector>::check();
	checkBinary<OpDot,Vector,Vector>::check();
	checkBinary<OpMult,Vector,Vector>::check();
	checkBinary<OpAdd,Vector,Vector>::check();
	checkBinary<OpSub,Vector,Vector>::check();
    checkBinary<OpMult,Rotation,Rotation>::check();
    checkBinary<OpMult,Rotation,Vector>::check();    
    checkBinary<OpMult,Rotation,Twist>::check();    
    checkBinary<OpAdd,Twist,Twist>::check();    
    checkBinary<OpSub,Twist,Twist>::check();    
    checkBinary<OpMult,Twist,double>::check();    
    checkBinary<OpMult,double,Twist>::check();    
    checkBinary<OpRefPoint,Twist,Vector>::check();
    checkBinary<OpAdd,Wrench,Wrench>::check();    
    checkBinary<OpSub,Wrench,Wrench>::check();    
    checkBinary<OpMult,Wrench,double>::check();    
    checkBinary<OpMult,double,Wrench>::check();    
    checkBinary<OpRefPoint,Wrench,Vector>::check();	
	checkBinary<OpDiff,Vector,Vector>::check();

	//checkBinary_displ<OpDiff,Rotation,Rotation>::check(); 


	checkEulerZYX();
}

void checkFrameVelOps() {
    KDL_CTX;
	checkDiffs();

	checkUnaryVel<OpNegate,VectorVel>::check();
	checkUnaryVel<OpNorm,VectorVel>::check();
	checkUnaryVel<OpInverse,FrameVel>::check();
	checkUnaryVel<OpRotation,FrameVel>::check();
	checkUnaryVel<OpOrigin,FrameVel>::check();
	checkUnaryVel<OpUnitX,RotationVel>::check();
	checkUnaryVel<OpUnitY,RotationVel>::check();
	checkUnaryVel<OpUnitZ,RotationVel>::check();
	checkUnaryVel<OpCoordX,VectorVel>::check();
	checkUnaryVel<OpCoordY,VectorVel>::check();
	checkUnaryVel<OpCoordZ,VectorVel>::check();
	checkUnaryVel<OpRotation,FrameVel>::check();

    //checkUnary<OpRotX,double>::check();
    //checkUnary<OpRotY,double>::check();
    //checkUnary<OpRotZ,double>::check();
    checkUnaryVel<OpInverse,RotationVel>::check();
    checkUnaryVel<OpNegate,TwistVel>::check();

	checkBinaryVel<OpMult,FrameVel,FrameVel>::check();
	checkBinaryVel<OpMult,FrameVel,VectorVel>::check();
	checkBinaryVel<OpDot,VectorVel,VectorVel>::check();
	checkBinaryVel<OpMult,VectorVel,VectorVel>::check();
	checkBinaryVel<OpAdd,VectorVel,VectorVel>::check();
	checkBinaryVel<OpSub,VectorVel,VectorVel>::check();
	checkBinaryVel<OpMult,doubleVel,VectorVel>::check();
	checkBinaryVel<OpMult,VectorVel,doubleVel>::check();
    checkBinaryVel<OpMult,RotationVel,RotationVel>::check();
    checkBinaryVel<OpMult,RotationVel,VectorVel>::check();    
    checkBinaryVel<OpMult,RotationVel,TwistVel>::check();    
    checkBinaryVel<OpAdd,TwistVel,TwistVel>::check();    
    checkBinaryVel<OpSub,TwistVel,TwistVel>::check();    
    checkBinaryVel<OpMult,TwistVel,doubleVel>::check();    
    checkBinaryVel<OpMult,doubleVel,TwistVel>::check(); 
    checkBinaryVel<OpRefPoint,TwistVel,VectorVel>::check();
    /*
    checkBinary<OpAdd,Wrench,Wrench>::check();    
    checkBinary<OpSub,Wrench,Wrench>::check();    
    checkBinary<OpMult,Wrench,double>::check();    
    checkBinary<OpMult,double,Wrench>::check();    
    checkBinary<OpRefPoint,Wrench,Vector>::check();	*/
	checkBinaryVel<OpDiff,VectorVel,VectorVel>::check();
	//checkBinaryVel<OpDiff,RotationVel,RotationVel>::check(); WHY ? 
	//checkBinaryVel<OpDiff,FrameVel,FrameVel>::check();  WHY ?
	

	//checkEulerZYX();
}


} // namespace ORO_Geometry




