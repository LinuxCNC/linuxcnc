#include <frames.hpp>
#include <frames_io.hpp>
#include <utilities/utility.h>

int main()
{
    //Creating Vectors
    KDL::Vector v1;//Default constructor
    KDL::Vector v2(1.0,2.0,3.0);//Most used constructor
    KDL::Vector v3(v2);//Copy constructor
    KDL::Vector v4 = KDL::Vector::Zero();//Static member

    //Use operator << to print the values of your vector
    std::cout<<"v1 ="<<v1<<std::endl;
    std::cout<<"v2 = "<<v2<<std::endl;
    std::cout<<"v3 = "<<v3<<std::endl;
    std::cout<<"v4 = "<<v4<<std::endl;

    //Get/Set values of a vector
    v1[0]=4.0;
    v1[1]=5.0;
    v1[2]=6.0;
    v2(0)=7.0;
    v2(1)=8.0;
    v2(2)=9.0;
    v3.x(10.0);
    v3.y(11.0);
    v3.z(12.0);

    std::cout<<"v1: "<<v1[0]<<", "<<v1[1]<<", "<<v1[2]<<std::endl;
    std::cout<<"v2: "<<v2(0)<<", "<<v2(1)<<", "<<v2(2)<<std::endl;
    std::cout<<"v3: "<<v3.x()<<", "<<v3.y()<<", "<<v3.z()<<std::endl;

    //double - vector operators
    std::cout<<"2*v2 = "<<2*v2<<std::endl;
    std::cout<<"v1*2 = "<<v1*2<<std::endl;
    std::cout<<"v1/2 = "<<v1/2<<std::endl;

    //vector - vector operators
    std::cout<<"v1+v2 = "<<v1+v2<<std::endl;
    std::cout<<"v3-v1 = "<<v3-v1<<std::endl;

    v3-=v1;
    v2+=v1;
    std::cout<<"v3-=v1; v3 = "<<v3<<std::endl;
    std::cout<<"v2+=v1; v2 = "<<v2<<std::endl;

    //cross and dot product between two vectors
    std::cout<<"cross(v1,v2) =  "<<v1*v2<<std::endl;
    std::cout<<"dot(v1,v2) = "<<dot(v1,v2)<<std::endl;

    //Inversing the sign of a vector
    v1=-v2;
    std::cout<<"v1=-v2; v1="<<v1<<std::endl;
    v1.ReverseSign();
    std::cout<<"v1.ReverseSign(); v1 = "<<v1<<std::endl;

    //Equal operators
    std::cout<<"v1==v2 ? "<<(v1==v2)<<std::endl;
    std::cout<<"v1!=v2 ? "<<(v1!=v2)<<std::endl;
    std::cout<<"Equal(v1,v2,1e-6) ? "<<Equal(v1,v2,1e-6)<<std::endl;

    //Calculating the norm and normalising your vector
    std::cout<<"norm(v3): "<<v3.Norm()<<std::endl;
    v3.Normalize();
    std::cout<<"Normalize(v3)"<<v3<<std::endl;

    //Setting your vector to zero
    SetToZero(v1);
    std::cout<<"SetToZero(v1); v1 = "<<v1<<std::endl;


    //Creating Rotations:
    //Default constructor
    KDL::Rotation r1;
    //Creating a rotation matrix out of three unit vectors Vx, Vy,
    //Vz. Be careful, these vectors should be normalised and
    //orthogonal. Otherwise this can result in an inconsistent
    //rotation matrix
    KDL::Rotation r2(KDL::Vector(0,0,1),
                     KDL::Vector(0,-1,0),
                     KDL::Vector(-1,0,0));
    //Creating a rotation matrix out of 9 values, Be careful, these
    //values can result in an inconsistent rotation matrix if the
    //resulting rows/columns are not orthogonal/normalized
    KDL::Rotation r3(0,0,-1,1,0,0,0,-1,0);
    //Creating an Identity rotation matrix
    KDL::Rotation r4=KDL::Rotation::Identity();
    //Creating a Rotation matrix from a rotation around X
    KDL::Rotation r5=KDL::Rotation::RotX(KDL::PI/3);
    //Creating a Rotation matrix from a rotation around Y
    KDL::Rotation r6=KDL::Rotation::RotY(KDL::PI/3);
    //Creating a Rotation matrix from a rotation around Z
    KDL::Rotation r7=KDL::Rotation::RotZ(KDL::PI/3);
    //Creating a Rotation matrix from a rotation around a arbitrary
    //vector, the vector should not be normalised
    KDL::Rotation r8=KDL::Rotation::Rot(KDL::Vector(1.,2.,3.),KDL::PI_4);
    //Creating a Rotation matrix from a rotation around a arbitrary
    //vector, the vector should be normalised
    KDL::Rotation r9=KDL::Rotation::Rot2(KDL::Vector(0.4472,0.5477,0.7071),
                                         KDL::PI_4);
    //Creating a Rotation matrix from Euler ZYZ rotation angles
    KDL::Rotation r10=KDL::Rotation::EulerZYZ(1.,2.,3.);
    //Creating a Rotation matrix from Euler ZYX rotation angles
    KDL::Rotation r11=KDL::Rotation::EulerZYX(1.,2.,3.);
    //Creating a Rotation matrix from Roll-Pitch-Yaw rotation angles
    KDL::Rotation r12=KDL::Rotation::RPY(1.,2.,3.);

    //Printing the rotations:
    std::cout<<"r1: "<<r1<<std::endl;
    std::cout<<"r2: "<<r2<<std::endl;
    std::cout<<"r3: "<<r3<<std::endl;
    std::cout<<"r4: "<<r4<<std::endl;
    std::cout<<"r5: "<<r5<<std::endl;
    std::cout<<"r6: "<<r6<<std::endl;
    std::cout<<"r7: "<<r7<<std::endl;
    std::cout<<"r8: "<<r8<<std::endl;
    std::cout<<"r9: "<<r9<<std::endl;
    std::cout<<"r10: "<<r10<<std::endl;
    std::cout<<"r11: "<<r11<<std::endl;
    std::cout<<"r12: "<<r12<<std::endl;

    //Getting information out of the rotation matrix:
    //The individual elements
    std::cout<<"r8(1,2): "<<r8(1,2)<<std::endl;
    //The equivalent rotation vector;
    std::cout<<"equiv rot vector of r11: "<<r11.GetRot()<<std::endl;
    //The equivalent rotation vector and angle:
    double angle=r10.GetRotAngle(v1);
    std::cout<<"equiv rot vector of r10:"<<v1<<"and angle: "<<angle<<std::endl;
    //The Euler ZYZ angles
    double alfa,beta,gamma;
    r9.GetEulerZYZ(alfa,beta,gamma);
    std::cout<<"EulerZYZ: "<<alfa<<", "<<beta<<", "<<gamma<<std::endl;
    //The Euler ZYZ angles
    r9.GetEulerZYX(alfa,beta,gamma);
    std::cout<<"EulerZYX: "<<alfa<<", "<<beta<<", "<<gamma<<std::endl;
    //The Roll-Pitch-Yaw angles
    r9.GetRPY(alfa,beta,gamma);
    std::cout<<"Roll-Pitch-Yaw: "<<alfa<<", "<<beta<<", "<<gamma<<std::endl;
    //The underlying unitvector X
    r8.UnitX(v1);//or
    std::cout<<"UnitX of r8:"<<r8.UnitX()<<std::endl;
    //The underlying unitvector Y
    r8.UnitY(v1);//or
    std::cout<<"Unity of r8:"<<r8.UnitY()<<std::endl;
    //The underlying unitvector Z
    r8.UnitZ(v1);//or
    std::cout<<"UnitZ of r8:"<<r8.UnitZ()<<std::endl;
}
