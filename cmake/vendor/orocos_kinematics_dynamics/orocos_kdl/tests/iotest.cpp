#include <iostream>
#include <kdl/error_stack.h>
#include <kdl/error.h>
#include <kdl/frames.hpp>
#include <kdl/frames_io.hpp>
#include <kdl/kinfam/joint.hpp>
#include <kdl/kinfam/serialchain.hpp>
#include <kdl/kinfam/kinematicfamily_io.hpp>
#include <kdl/kinfam/crs450.hpp>
#include <kdl/kinfam/jnt2cartpos.hpp>
#include <memory>

using namespace std;
using namespace KDL;

void test_io(KinematicFamily* kf) {
	// write a kf to the file tst.dat
	ofstream os("tst.dat");
	os << kf << endl;
	cout << kf  << endl;
	os.close();

	// read a serial chain from the file tst.dat
	ifstream is ("tst.dat");
	KinematicFamily* kf2;
	try {
	  kf2 = readKinematicFamily(is);
	  cout << kf2 << endl;
	} catch (Error& err) {
	  IOTraceOutput(cerr);
	  cout << "ERROR : " << err.Description() << endl;
	  exit(-1);
	}

	std::vector<double> q(6);
	for (int i=0;i<q.size();++i) q[i] = i*0.2;
	Frame F1,F2;
	Jnt2CartPos* jnt2cartpos  = kf->createJnt2CartPos();
	Jnt2CartPos* jnt2cartpos2 = kf2->createJnt2CartPos();

	jnt2cartpos->evaluate(q);jnt2cartpos->getFrame(F1);
	jnt2cartpos2->evaluate(q);jnt2cartpos2->getFrame(F2);
	cout << F1 << endl;
	cout << F2 << endl;
	if (!Equal(F1,F2)) {
		cerr << "Results are not equal" << endl;
		exit(-1);
	}
	delete jnt2cartpos;
	delete jnt2cartpos2;
	delete kf2;
	delete kf;
}

int main(int argc,char* argv[]) {
    test_io(new CRS450());
    test_io(new CRS450Feath());
}


