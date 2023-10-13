/**
 * \file path_example.cpp
 * An example to demonstrate the use of trajectory generation
 * functions.
 *
 * There are is a matlab/octave file in the examples directory to visualise the results
 * of this example program. (visualize_trajectory.m)
 *
 */

#include <frames.hpp>
#include <frames_io.hpp>
#include <trajectory.hpp>
#include <trajectory_segment.hpp>
#include <trajectory_stationary.hpp>
#include <trajectory_composite.hpp>
#include <trajectory_composite.hpp>
#include <velocityprofile_trap.hpp>
#include <path_roundedcomposite.hpp>
#include <rotational_interpolation_sa.hpp>
#include <utilities/error.h>
#include <utilities/utility.h>
#include <trajectory_composite.hpp>

int main(int argc,char* argv[]) {
	using namespace KDL;
	// Create the trajectory:
    // use try/catch to catch any exceptions thrown.
    // NOTE:  exceptions will become obsolete in a future version.
	try {
        // Path_RoundedComposite defines the geometric path along
        // which the robot will move.
		//
		Path_RoundedComposite* path = new Path_RoundedComposite(0.2,0.01,new RotationalInterpolation_SingleAxis());
		// The routines are now robust against segments that are parallel.
		// When the routines are parallel, no rounding is needed, and no attempt is made
		// add constructing a rounding arc.
		// (It is still not possible when the segments are on top of each other)
		// Note that you can only rotate in a deterministic way over an angle less then PI!
		// With an angle == PI, you cannot predict over which side will be rotated.
		// With an angle > PI, the routine will rotate over 2*PI-angle.
		// If you need to rotate over a larger angle, you need to introduce intermediate points.
		// So, there is a common use case for using parallel segments.
		path->Add(Frame(Rotation::RPY(PI,0,0), Vector(-1,0,0)));
		path->Add(Frame(Rotation::RPY(PI_2,0,0), Vector(-0.5,0,0)));
		path->Add(Frame(Rotation::RPY(0,0,0), Vector(0,0,0)));
		path->Add(Frame(Rotation::RPY(0.7,0.7,0.7), Vector(1,1,1)));
		path->Add(Frame(Rotation::RPY(0,0.7,0), Vector(1.5,0.3,0)));
		path->Add(Frame(Rotation::RPY(0.7,0.7,0), Vector(1,1,0)));

		// always call Finish() at the end, otherwise the last segment will not be added.
		path->Finish();

        // Trajectory defines a motion of the robot along a path.
        // This defines a trapezoidal velocity profile.
		VelocityProfile* velpref = new VelocityProfile_Trap(0.5,0.1);
		velpref->SetProfile(0,path->PathLength());  
		Trajectory* traject = new Trajectory_Segment(path, velpref);


		Trajectory_Composite* ctraject = new Trajectory_Composite();
		ctraject->Add(traject);
		ctraject->Add(new Trajectory_Stationary(1.0,Frame(Rotation::RPY(0.7,0.7,0), Vector(1,1,0))));

		// use the trajectory
		double dt=0.1;
		std::ofstream of("./trajectory.dat");
		for (double t=0.0; t <= traject->Duration(); t+= dt) {
			Frame current_pose;
			current_pose = traject->Pos(t);
			for (int i=0;i<4;++i)
				for (int j=0;j<4;++j)
					of << current_pose(i,j) << "\t";
			of << "\n";
			// also velocities and accelerations are available !
			//traject->Vel(t);
			//traject->Acc(t);
		}
		of.close();

		// you can get some meta-info on the path:
		for (int segmentnr=0;  segmentnr < path->GetNrOfSegments(); segmentnr++) {
			double starts,ends;
			Path::IdentifierType pathtype;
			if (segmentnr==0) {
				starts = 0.0;
			} else {
				starts = path->GetLengthToEndOfSegment(segmentnr-1);
			}
			ends = path->GetLengthToEndOfSegment(segmentnr);
			pathtype = path->GetSegment(segmentnr)->getIdentifier();
			std::cout << "segment " << segmentnr << " runs from s="<<starts << " to s=" <<ends;
			switch(pathtype) {
				case Path::ID_CIRCLE:
					std::cout << " circle";
					break;
				case Path::ID_LINE:
					std::cout << " line ";
					break;
				default:
					std::cout << " unknown ";
					break;
			}
			std::cout << std::endl;
		}
        std::cout << " trajectory written to the ./trajectory.dat file " << std::endl;

        delete ctraject;
	} catch(Error& error) {
		std::cout <<"I encountered this error : " << error.Description() << std::endl;
		std::cout << "with the following type " << error.GetType() << std::endl;
	}

}
