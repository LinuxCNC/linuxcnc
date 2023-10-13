
#include"kdl/kdltk/toolkit.hpp"
#include<kdl/kinfam/joint_io.hpp>
#include<kdl/kinfam/joint.hpp>
#include<rtt/os/main.h>
#include<rtt/Logger.hpp>
#include<rtt/TaskContext.hpp>
#include<kdl/kinfam/kuka361.hpp>
#include<kdl/kinfam/unittransmission.hpp>
#include<kdl/kinfam/lineartransmission.hpp>

using namespace RTT;
using namespace KDL;
using namespace std;

int ORO_main(int argc, char** argv)
{
    RTT::Toolkit::Import(KDLToolkit);

    TaskContext writer("writer");
    int retval = 0;

    JointTransX tx(Frame(Rotation::RPY(1,2,3),Vector(1,2,3)));
    JointTransY ty(Frame(Rotation::RPY(1,2,3),Vector(1,2,3)));
    JointTransZ tz(Frame(Rotation::RPY(1,2,3),Vector(1,2,3)));
    JointRotX rx(Frame(Rotation::RPY(1,2,3),Vector(1,2,3)));
    JointRotY ry(Frame(Rotation::RPY(1,2,3),Vector(1,2,3)));
    JointRotZ rz(Frame(Rotation::RPY(1,2,3),Vector(1,2,3)));

    Property<JointTransX> tx_prop("joint1","first joint",tx);
    Property<JointTransY> ty_prop("joint2","second joint",ty);
    Property<JointTransZ> tz_prop("joint3","third joint",tz);
    Property<JointRotX> rx_prop("joint4","fourth joint",rx);
    Property<JointRotY> ry_prop("joint5","fifth joint",ry);
    Property<JointRotZ> rz_prop("joint6","sixth joint",rz);
    Kuka361 kuka = Kuka361();
    SerialChain* robot = kuka.createSerialChain();
    LinearTransmission ltrans = LinearTransmission(6,vector<double>(6,1),vector<double>(6,0.5));
    Property<LinearTransmission> ltrans_prop("transmission","some transmission",ltrans);
    UnitTransmission utrans = UnitTransmission(6);
    Property<UnitTransmission> utrans_prop("transmission2","some other transmission",utrans);

    Property<SerialChain> kuka_prop("testchain","some chain",*(robot));
    Property<ZXXZXZ> kuka_prop2("testchain2","some other chain",kuka);

    writer.properties()->addProperty(&tx_prop);
    writer.properties()->addProperty(&ty_prop);
    writer.properties()->addProperty(&tz_prop);
    writer.properties()->addProperty(&rx_prop);
    writer.properties()->addProperty(&ry_prop);
    writer.properties()->addProperty(&rz_prop);
    writer.properties()->addProperty(&ltrans_prop);
    writer.properties()->addProperty(&utrans_prop);
    writer.properties()->addProperty(&kuka_prop);
    writer.properties()->addProperty(&kuka_prop2);

    writer.marshalling()->writeProperties("test.cpf");

    writer.marshalling()->readProperties("test.cpf");
    Logger::In in("KDLToolkitTest");
    if(tx_prop.value().getType()!=tx.getType()||!Equal(tx_prop.value().frame_before_joint(),tx.frame_before_joint(),1e-5)){
        log(Error)<<"Property is not the same after writing and rereading"<<endlog();
        log(Debug)<<"Property "<<tx_prop.value()<<" ,actual type: "<<tx<<endlog();
        retval=-1;
    }

    if(ty_prop.value().getType()!=ty.getType()||!Equal(ty_prop.value().frame_before_joint(),ty.frame_before_joint(),1e-5)){
        log(Error)<<"Property is not the same after writing and rereading"<<endlog();
        log(Debug)<<"Property "<<ty_prop.value()<<" ,actual type: "<<ty<<endlog();
        retval=-1;
    }

    if(tz_prop.value().getType()!=tz.getType()||!Equal(tz_prop.value().frame_before_joint(),tz.frame_before_joint(),1e-5)){
        log(Error)<<"Property is not the same after writing and rereading"<<endlog();
        log(Debug)<<"Property "<<tz_prop.value()<<" ,actual type: "<<tz<<endlog();
        retval=-1;
    }

    if(rx_prop.value().getType()!=rx.getType()||!Equal(rx_prop.value().frame_before_joint(),rx.frame_before_joint(),1e-5)){
        log(Error)<<"Property is not the same after writing and rereading"<<endlog();
        log(Debug)<<"Property "<<rx_prop.value()<<" ,actual type: "<<rx<<endlog();
        retval=-1;
    }

    if(ry_prop.value().getType()!=ry.getType()||!Equal(ry_prop.value().frame_before_joint(),ry.frame_before_joint(),1e-5)){
        log(Error)<<"Property is not the same after writing and rereading"<<endlog();
        log(Debug)<<"Property "<<ry_prop.value()<<" ,actual type: "<<ry<<endlog();
        retval=-1;
    }

    if(rz_prop.value().getType()!=rz.getType()||!Equal(rz_prop.value().frame_before_joint(),rz.frame_before_joint(),1e-5)){
        log(Error)<<"Property is not the same after writing and rereading"<<endlog();
        log(Debug)<<"Property "<<rz_prop.value()<<" ,actual type: "<<rz<<endlog();
        retval=-1;
    }


    delete robot;
    return retval;

}


