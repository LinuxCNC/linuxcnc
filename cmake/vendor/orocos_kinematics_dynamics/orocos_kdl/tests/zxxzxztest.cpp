#include <kdl/kinfam/zxxzxz.hpp>
#include <kdl/kinfam/zxxzxzjnt2cartpos.hpp>
#include <kdl/kinfam/zxxzxzcartpos2jnt.hpp>
#include <kdl/frames.hpp>
#include <kdl/frames_io.hpp>
#include <kdl/kinfam/lineartransmission.hpp>
#include <kdl/kinfam/unittransmission.hpp>

using namespace KDL;
using namespace std;
/**
 * testing of forward and inverse position kinematics of the  routines.
 * Also tests everything for all configurations.
 */
void test_zxxzxz(double l1,double l2,double l3,double l6) {
    cout << "========================================================================================"  << endl;
    cout << " test_zxxzxz : forward and inverse position kinematics for all configurations" << endl;
    cout << "========================================================================================"  << endl;
    ZXXZXZ kf("tst");
    kf.setLinkLengths(l1,l2,l3,l6);
    ZXXZXZJnt2CartPos* jnt2cartpos =
        (ZXXZXZJnt2CartPos*) kf.createJnt2CartPos();
    ZXXZXZCartPos2Jnt* cartpos2jnt =
        (ZXXZXZCartPos2Jnt*) kf.createCartPos2Jnt();
    SerialChain* kf2 = kf.createSerialChain();
    Jnt2CartPos* jnt2cartpos2 = kf2->createJnt2CartPos();

    std::vector<double> q(6);
    std::vector<double> q2(6);
    double epsq = 1E-8;
    int config,config2,config3;
    Frame F,F2,F3;
    bool exitflag=false;

    for (int config=0;config<8;config++) {
        cout << endl<<"=== Testing configuration : " << config << " === "<< endl;
        kf.setConfigurationToJoints(config,q);
        config2=kf.getConfigurationFromJoints(q);
        if (config != config2) {
            cout << "FAIL :test_zxxzxz(): configurations do not match using configuration representation transformation" << endl;
            cerr << "FAIL :test_zxxzxz(): configurations do not match using configuration representation transformation" << endl;
            cout << "original configuration " << kf.getConfigurationDescription(config)  << endl;
            cout << "reached  configuration " << kf.getConfigurationDescription(config2) << endl;
            exitflag=true;
        }

        // fwd kin
        jnt2cartpos->evaluate(q);
        jnt2cartpos->getFrame(F);
        jnt2cartpos->getConfiguration(config2);
        jnt2cartpos2->evaluate(q);
        jnt2cartpos2->getFrame(F3);
        if (!Equal(F,F3,1E-6)) {
             cout << "FAIL :test_zxxzxz(): fwd(q)!=fwd_serialchain(q)" << endl;
             cerr << "FAIL :test_zxxzxz(): fwd(q)!=fwd_serialchain(q)" << endl;
             cout << "Frame F = fwd(q) " << F << endl;
             cout << "Frame F = fwd_serialchain(q) " << F3 << endl;
             exitflag=true;
        }

        if (config!=config2) {
            cout << "FAIL :test_zxxzxz(): configurations do not match after jnt2cartpos transform" << endl;
            cout << "original configuration " << kf.getConfigurationDescription(config)  << endl;
            cout << "reached  configuration " << kf.getConfigurationDescription(config2) << endl;
            cerr << "FAIL :test_zxxzxz(): configurations do not match after jnt2cartpos transform" << endl;
            exitflag=true;
        }
        // inv kin
        cartpos2jnt->setConfiguration(config);
        cartpos2jnt->setFrame(F);
        cartpos2jnt->evaluate(q2);
        jnt2cartpos->evaluate(q2);
        jnt2cartpos->getConfiguration(config3);
        jnt2cartpos->getFrame(F2);
        if (!Equal(F,F2,1E-6)) {
             cout << "FAIL :test_zxxzxz(): fwd(inv(F))!=F" << endl;
             cerr << "FAIL :test_zxxzxz(): fwd(inv(F))!=F" << endl;
             exitflag=true;
        }
        for (int i=0;i<q.size();++i) {
            if (fabs(q[i]-q2[i])>epsq) {
                cout << "FAIL :test_zxxzxz(): inv(fwd(q))!=q" << endl;
                cerr << "FAIL :test_zxxzxz(): inv(fwd(q))!=q" << endl;
                exitflag=true;
            }
        }
        if (exitflag) {
                cout << "=========== FAILURE REPORT ================" << endl;
                cout << "Frame F = fwd(q) = " << F << endl;
                cout << "original configuration " << kf.getConfigurationDescription(config)  << endl;
                cout << "Frame F2 = fwd(inv(F)) = " << F2 << endl;
                cout << "configuration of fwd(inv(F)) = " << kf.getConfigurationDescription(config3)  << endl;
                cout << "Comparing q with q2=inv(fwd(q)) "<< endl;
                for (int j=0;j<6;++j) {
                    std::cout << "q["<<j<<"]="<<q[j]<<" and q2["<<j<<"]="<<q2[j]<<std::endl;
                }
        }
    }
    if (exitflag) return exit(-1);
    delete jnt2cartpos2;
    delete kf2;
    delete cartpos2jnt;
    delete jnt2cartpos;
}





int main(int argc,char* argv[]) {
    test_zxxzxz(0.33,0.305,0.33,0.176);
    test_zxxzxz(0.1,0.2,0.3,0.4);
    test_zxxzxz(0.5,0.3,0.2,0.5);
	return 0;
}

