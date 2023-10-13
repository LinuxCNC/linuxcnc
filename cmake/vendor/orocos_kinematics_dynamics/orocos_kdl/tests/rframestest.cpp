#include <kdl/framevel.hpp> 
#include <kdl/frameacc.hpp> 
#include <kdl/framevel_io.hpp>
#include <kdl/frameacc_io.hpp>
#include <kdl/test_macros.h>


using namespace KDL;

void TestVector() {
    KDL_CTX;
	Vector   v(3,4,5);
	Vector   v2;				
	Vector    vt(-5,-6,-3);	
	KDL_DIFF( 2*v-v, v		);	
	KDL_DIFF( v*2-v, v		);	
	KDL_DIFF( v+v+v-2*v, v	);
	v2=v;
	KDL_DIFF( v,	v2	);
	v2+=v;	
	KDL_DIFF( 2*v,	v2	);
	v2-=v;
	KDL_DIFF( v,v2);
	v2.ReverseSign();
	KDL_DIFF( v,-v2);
	KDL_DIFF( v*vt,-vt*v);
	v2 = Vector(-5,-6,-3);
	KDL_DIFF( v*v2,-v2*v);
}

void TestVectorVel() {
    KDL_CTX;
	VectorVel   v(Vector(3,-4,5),Vector(6,3,-5));	
	VectorVel   v2;
	Vector    vt(-4,-6,-8);	
	KDL_DIFF( 2*v-v, v		);
	KDL_DIFF( v*2-v, v		);
	KDL_DIFF( v+v+v-2*v, v	);
	v2=v;
	KDL_DIFF( v,	v2	);
	v2+=v;	
	KDL_DIFF( 2*v,	v2	);
	v2-=v;
	KDL_DIFF( v,v2);
	v2.ReverseSign();
	KDL_DIFF( v,-v2);
	KDL_DIFF( v*vt,-vt*v);
	v2 = VectorVel(Vector(-5,-6,-3),Vector(3,4,5));
	KDL_DIFF( v*v2,-v2*v);
}

void TestVectorAcc() {
    KDL_CTX;
	VectorAcc   v(Vector(3,-4,5),Vector(6,3,-5),Vector(-4,-3,-6));
	VectorAcc   v2;				
	Vector    vt(-4,-6,-8);	
	KDL_DIFF( 2*v-v, v		);	
	KDL_DIFF( v*2-v, v		);	
	KDL_DIFF( v+v+v-2*v, v	);
	v2=v;
	KDL_DIFF( v,	v2	);
	v2+=v;
	KDL_DIFF( 2*v,	v2	);
	v2-=v;
	KDL_DIFF( v,v2);
	v2.ReverseSign();
	KDL_DIFF( v,-v2);
	KDL_DIFF( v*vt,-vt*v);
	v2 = VectorAcc(Vector(-5,-6,-3),Vector(-3,-4,-1),Vector(10,12,9));
	KDL_DIFF( v*v2,-v2*v);
}

void TestRotation() {
    KDL_CTX;
	Vector   v(Vector(9,4,-2));								
	Vector   v2;											
	Vector    vt(2,3,4);							
	Rotation R;										
	Rotation R2;						
	double a;
	double b;				
	double c;			
	a= -15*deg2rad;	
	b= 20*deg2rad;
	c= -80*deg2rad;	
	R = Rotation(Rotation::RPY(a,b,c));	
	R2=R;							
	KDL_DIFF( R,R2						);
	KDL_DIFF( (R*v).Norm(),	v.Norm()								);
	KDL_DIFF( R.Inverse(R*v),	v										);
	KDL_DIFF( R*R.Inverse(v),	v										);
	KDL_DIFF( R*Rotation::Identity(),	R								);
	KDL_DIFF( Rotation::Identity()*R,	R								);
	KDL_DIFF( R*(R*(R*v)),	(R*R*R)*v									);
	KDL_DIFF( R*(R*(R*vt)),	(R*R*R)*vt								);
	KDL_DIFF( R*R.Inverse(),	Rotation::Identity()					);
	KDL_DIFF( R.Inverse()*R,	Rotation::Identity()					);
	KDL_DIFF( R.Inverse()*v,	R.Inverse(v)							);
	v2=v*v-2*v;	
	KDL_DIFF( (v2).Norm(),	::sqrt(dot(v2,v2))							);
}


void TestRotationVel() {
    KDL_CTX;
	VectorVel   v(Vector(9,4,-2),Vector(-5,6,-2));			
	VectorVel   v2;							
	Vector    vt(2,3,4);	
	RotationVel R;		
	RotationVel R2;	
	double a;
	double b;					
	double c;	
	a= -15*deg2rad;					
	b= 20*deg2rad;		
	c= -80*deg2rad;	
	R = RotationVel(Rotation::RPY(a,b,c),Vector(2,4,1));	
	R2=R;						
	KDL_DIFF( R,R2														);
	KDL_DIFF( (R*v).Norm(),	v.Norm()								);
	KDL_DIFF( R.Inverse(R*v),	v										);
	KDL_DIFF( R*R.Inverse(v),	v										);
	KDL_DIFF( R*Rotation::Identity(),	R								);
	KDL_DIFF( Rotation::Identity()*R,	R								);
	KDL_DIFF( R*(R*(R*v)),	(R*R*R)*v									);
	KDL_DIFF( R*(R*(R*vt)),	(R*R*R)*vt								);
	KDL_DIFF( R*R.Inverse(),	RotationVel::Identity()					);
	KDL_DIFF( R.Inverse()*R,	RotationVel::Identity()					);
	KDL_DIFF( R.Inverse()*v,	R.Inverse(v)							);
	v2=v*v-2*v;	
	KDL_DIFF( (v2).Norm(),	sqrt(dot(v2,v2))							);
}

void TestRotationAcc() {
    KDL_CTX;
	VectorAcc   v(Vector(9,4,-2),Vector(-5,6,-2),Vector(2,-3,-3));
	VectorAcc   v2;										
	Vector    vt(2,3,4);				
	RotationAcc R;					
	RotationAcc R2;			
	double a;						
	double b;				
	double c;	
	a= -15*deg2rad;					
	b= 20*deg2rad;			
	c= -80*deg2rad;							
	R = RotationAcc(Rotation::RPY(a,b,c),Vector(2,4,1),Vector(-3,-2,-1));
	R2=R;														
	KDL_DIFF( R,R2																);
	KDL_DIFF( (R*v).Norm(),	v.Norm()										);
	KDL_DIFF( R.Inverse(R*v),	v												);
	KDL_DIFF( R*R.Inverse(v),	v												);
	KDL_DIFF( R*Rotation::Identity(),	R										);
	KDL_DIFF( Rotation::Identity()*R,	R										);
	KDL_DIFF( R*(R*(R*v)),	(R*R*R)*v											);
	KDL_DIFF( R*(R*(R*vt)),	(R*R*R)*vt										);
	KDL_DIFF( R*R.Inverse(),	RotationAcc::Identity()							);
	KDL_DIFF( R.Inverse()*R,	RotationAcc::Identity()							);
	KDL_DIFF( R.Inverse()*v,	R.Inverse(v)									);
	v2=v*v-2*v;
	KDL_DIFF( (v2).Norm(),	sqrt(dot(v2,v2))									);
}

void TestFrame() {
    KDL_CTX;
	Vector   v(3,4,5);						
	Vector    vt(-1,0,-10);
	Rotation R;				
	Frame F;			
	Frame F2;
	F = Frame(Rotation::EulerZYX(10*deg2rad,20*deg2rad,-10*deg2rad),Vector(4,-2,1));
			
	F2=F;									
	KDL_DIFF( F,	F2													);
	KDL_DIFF( F.Inverse(F*v),	v										);
	KDL_DIFF( F.Inverse(F*vt), vt										);
	KDL_DIFF( F*F.Inverse(v),	v										);
	KDL_DIFF( F*F.Inverse(vt),	vt										);
	KDL_DIFF( F*Frame::Identity(),	F									);
	KDL_DIFF( Frame::Identity()*F,	F									);
	KDL_DIFF( F*(F*(F*v)),	(F*F*F)*v									);
	KDL_DIFF( F*(F*(F*vt)),	(F*F*F)*vt								);
	KDL_DIFF( F*F.Inverse(),	Frame::Identity()						);
	KDL_DIFF( F.Inverse()*F,	Frame::Identity()						);
	KDL_DIFF( F.Inverse()*vt,	F.Inverse(vt)							);
}

void TestFrameVel() {
    KDL_CTX;
	VectorVel   v(Vector(3,4,5),Vector(-2,-4,-1));		
	Vector    vt(-1,0,-10);						
	RotationVel R;						
	FrameVel F;					
	FrameVel F2;				;
	F = FrameVel(
			Frame(Rotation::EulerZYX(10*deg2rad,20*deg2rad,-10*deg2rad),Vector(4,-2,1)),
			Twist(Vector(2,-2,-2),Vector(-5,-3,-2))
		);					
	F2=F;				
	KDL_DIFF( F,	F2													);
	KDL_DIFF( F.Inverse(F*v),	v										);
	KDL_DIFF( F.Inverse(F*vt), vt										);
	KDL_DIFF( F*F.Inverse(v),	v										);
	KDL_DIFF( F*F.Inverse(vt),	vt										);
	KDL_DIFF( F*Frame::Identity(),	F									);
	KDL_DIFF( Frame::Identity()*F,	F									);
	KDL_DIFF( F*(F*(F*v)),	(F*F*F)*v									);
	KDL_DIFF( F*(F*(F*vt)),	(F*F*F)*vt								);
	KDL_DIFF( F*F.Inverse(),	FrameVel::Identity()						);
	KDL_DIFF( F.Inverse()*F,	Frame::Identity()						);
	KDL_DIFF( F.Inverse()*vt,	F.Inverse(vt)							);
}

void TestFrameAcc() {
    KDL_CTX;
    VectorAcc   v(Vector(3,4,5),Vector(-2,-4,-1),Vector(6,4,2));	
	Vector    vt(-1,0,-10);					
	RotationAcc R;					
	FrameAcc F;					
	FrameAcc F2;			
	F = FrameAcc(
			Frame(Rotation::EulerZYX(10*deg2rad,20*deg2rad,-10*deg2rad),Vector(4,-2,1)),
			Twist(Vector(2,-2,-2),Vector(-5,-3,-2)),
			Twist(Vector(5,6,2),Vector(-2,-3,1))
		);		
	F2=F;									
	KDL_DIFF( F,	F2													);
	KDL_DIFF( F.Inverse(F*v),	v										);
	KDL_DIFF( F.Inverse(F*vt), vt										);
	KDL_DIFF( F*F.Inverse(v),	v										);
	KDL_DIFF( F*F.Inverse(vt),	vt										);
	KDL_DIFF( F*Frame::Identity(),	F									);
	KDL_DIFF( Frame::Identity()*F,	F									);
	KDL_DIFF( F*(F*(F*v)),	(F*F*F)*v									);
	KDL_DIFF( F*(F*(F*vt)),	(F*F*F)*vt								);
	KDL_DIFF( F*F.Inverse(),	FrameAcc::Identity()						);
	KDL_DIFF( F.Inverse()*F,	Frame::Identity()						);
	KDL_DIFF( F.Inverse()*vt,	F.Inverse(vt)							);
}

void TestTwistVel() {
    KDL_CTX;
	// Twist
	TwistVel t(VectorVel(
				Vector(6,3,5),
				Vector(1,4,2)
			 ),VectorVel(
			 		Vector(4,-2,7),
			 		Vector(-1,-2,-3)
			 )
		);
	TwistVel t2;
	RotationVel  R(Rotation::RPY(10*deg2rad,20*deg2rad,-15*deg2rad),Vector(-1,5,3));
	FrameVel F = FrameVel(
		Frame(
			Rotation::EulerZYX(-17*deg2rad,13*deg2rad,-16*deg2rad),
			Vector(4,-2,1)
		),
		Twist(
			Vector(2,-2,-2),
			Vector(-5,-3,-2)
		)
	);

	KDL_DIFF(2.0*t-t,t);
	KDL_DIFF(t*2.0-t,t);
	KDL_DIFF(t+t+t-2.0*t,t);
	t2=t;
	KDL_DIFF(t,t2);
	t2+=t;
	KDL_DIFF(2.0*t,t2);
	t2-=t;
	KDL_DIFF(t,t2);
	t.ReverseSign();
	KDL_DIFF(t,-t2);
	KDL_DIFF(R.Inverse(R*t),t);
	KDL_DIFF(R*t,R*R.Inverse(R*t));
	KDL_DIFF(F.Inverse(F*t),t);
	KDL_DIFF(F*t,F*F.Inverse(F*t));
	KDL_DIFF(doubleVel(3.14,2)*t,t*doubleVel(3.14,2));
	KDL_DIFF(t/doubleVel(3.14,2),t*(1.0/doubleVel(3.14,2)));
	KDL_DIFF(t/3.14,t*(1.0/3.14));
	KDL_DIFF(-t,-1.0*t);
	VectorVel p1(Vector(5,1,2),Vector(4,2,1)) ;
	VectorVel p2(Vector(2,0,5),Vector(-2,7,-1)) ;
	KDL_DIFF(t.RefPoint(p1+p2),t.RefPoint(p1).RefPoint(p2));
	KDL_DIFF(t,t.RefPoint(-p1).RefPoint(p1));
}

void TestTwistAcc() {
    KDL_CTX;
	// Twist
	TwistAcc     t( VectorAcc(Vector(6,3,5),Vector(1,4,2),Vector(5,2,1)),
		              VectorAcc(Vector(4,-2,7),Vector(-1,-2,-3),Vector(5,2,9) )
					);
	TwistAcc    t2; 
	RotationAcc  R(Rotation::RPY(10*deg2rad,20*deg2rad,-15*deg2rad),
		             Vector(-1,5,3),
					 Vector(2,1,3)
					 ) ;
	FrameAcc F = FrameAcc(
			Frame(Rotation::EulerZYX(-17*deg2rad,13*deg2rad,-16*deg2rad),Vector(4,-2,1)),
			Twist(Vector(2,-2,-2),Vector(-5,-3,-2)),
			Twist(Vector(5,4,-5),Vector(12,13,17))
		    );	

	KDL_DIFF(2.0*t-t,t);
	KDL_DIFF(t*2.0-t,t);
	KDL_DIFF(t+t+t-2.0*t,t);
	t2=t; 
	KDL_DIFF(t,t2);
	t2+=t;
	KDL_DIFF(2.0*t,t2);
	t2-=t;
	KDL_DIFF(t,t2);
	t.ReverseSign();
	KDL_DIFF(t,-t2);
	KDL_DIFF(R.Inverse(R*t),t);
	KDL_DIFF(R*t,R*R.Inverse(R*t));
	KDL_DIFF(F.Inverse(F*t),t);
	KDL_DIFF(F*t,F*F.Inverse(F*t));
	KDL_DIFF(doubleAcc(3.14,2,3)*t,t*doubleAcc(3.14,2,3));
	KDL_DIFF(t/doubleAcc(3.14,2,7),t*(1.0/doubleAcc(3.14,2,7)));
	KDL_DIFF(t/3.14,t*(1.0/3.14));
	KDL_DIFF(-t,-1.0*t);
	VectorAcc p1(Vector(5,1,2),Vector(4,2,1),Vector(2,1,3));
	VectorAcc p2(Vector(2,0,5),Vector(-2,7,-1),Vector(-3,2,-1));
	KDL_DIFF(t.RefPoint(p1+p2),t.RefPoint(p1).RefPoint(p2));
	KDL_DIFF(t,t.RefPoint(-p1).RefPoint(p1));
}

int main() {
    KDL_CTX;
	TestVector();		
	TestRotation();	
	TestFrame();	
	TestVectorVel();		
	TestRotationVel();	
	TestFrameVel();	
	TestVectorAcc();
	TestRotationAcc();
	TestFrameAcc();	
	TestTwistVel();
	TestTwistAcc();
	return 0;
}
