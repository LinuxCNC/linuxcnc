/**
 \file   chainiksolverpos_lma_demo.cpp
 \brief  Test program for inverse position kinematics

results with 1 million inv pos kin.
<code>
#times successful 999992
#times -1 result 0
#times -2 result 5
#times -3 result 3
average number of iter 16.6437
min. nr of iter 13
max. nr of iter 500
min. difference after solving 3.86952e-12
max. difference after solving 4.79339e-05
min. trans. difference after solving 3.86952e-12
max. trans. difference after solving 4.79339e-05
min. rot. difference after solving 0
max. rot. difference after solving 0.000261335
elapsed time 199.14
estimate of average time per invposkin (ms)0.19914
estimate of longest time per invposkin (ms) 5.98245
estimate of shortest time per invposkin (ms) 0.155544
</code>
*/

/**************************************************************************
    begin                : May 2011
    copyright            : (C) 2011 Erwin Aertbelien
    email                : firstname.lastname@mech.kuleuven.ac.be

 History (only major changes)( AUTHOR-Description ) :

 ***************************************************************************
 *   This library is free software; you can redistribute it and/or         *
 *   modify it under the terms of the GNU Lesser General Public            *
 *   License as published by the Free Software Foundation; either          *
 *   version 2.1 of the License, or (at your option) any later version.    *
 *                                                                         *
 *   This library is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU     *
 *   Lesser General Public License for more details.                       *
 *                                                                         *
 *   You should have received a copy of the GNU Lesser General Public      *
 *   License along with this library; if not, write to the Free Software   *
 *   Foundation, Inc., 59 Temple Place,                                    *
 *   Suite 330, Boston, MA  02111-1307  USA                                *
 *                                                                         *
 ***************************************************************************/

#include <iostream>
#include <frames_io.hpp>
#include <models.hpp>
#include <chainiksolverpos_lma.hpp>
#include <chainfksolverpos_recursive.hpp>
#include <utilities/utility.h>

#include <boost/timer.hpp>

/**
 * tests the inverse kinematics on the given kinematic chain for a
 * large number of times and provides statistics  on the result.
 * \TODO provide other examples.
 */
void test_inverseposkin(KDL::Chain& chain) {
	using namespace KDL;
	using namespace std;
	boost::timer timer;
	int num_of_trials = 1000000;
	int total_number_of_iter = 0;
	int n = chain.getNrOfJoints();
	int nrofresult_ok = 0;
	int nrofresult_minus1=0;
	int nrofresult_minus2=0;
	int nrofresult_minus3=0;
	int min_num_of_iter = 10000000;
	int max_num_of_iter = 0;
	double min_diff = 1E10;
	double max_diff = 0;
	double min_trans_diff = 1E10;
	double max_trans_diff = 0;
	double min_rot_diff = 1E10;
	double max_rot_diff = 0;
	Eigen::Matrix<double,6,1> L;
	L(0)=1;L(1)=1;L(2)=1;
	L(3)=0.01;L(4)=0.01;L(5)=0.01;
	ChainFkSolverPos_recursive fwdkin(chain);
	ChainIkSolverPos_LMA solver(chain,L);
	JntArray q(n);
	JntArray q_init(n);
	JntArray q_sol(n);
	for (int trial=0;trial<num_of_trials;++trial) {
		q.data.setRandom();
		q.data *= PI;
		q_init.data.setRandom();
		q_init.data *= PI;
		Frame pos_goal,pos_reached;
		fwdkin.JntToCart(q,pos_goal);
		//solver.compute_fwdpos(q.data);
		//pos_goal = solver.T_base_head;
		int retval;
		retval = solver.CartToJnt(q_init,pos_goal,q_sol);
		switch (retval) {
		case 0:
			nrofresult_ok++;
			break;
		case -1:
			nrofresult_minus1++;
			break;
		case -2:
			nrofresult_minus2++;
			break;
		case -3:
			nrofresult_minus3++;
			break;
		}
		if (retval !=0) {
			cout << "-------------- failed ----------------------------" << endl;
			cout << "pos " << pos_goal << endl;
			cout << "reached pos " << solver.T_base_head << endl;
			cout << "TF from pos to head \n" << pos_goal.Inverse()*solver.T_base_head << endl;
			cout << "gradient " << solver.grad.transpose() << endl;
			cout << "q   " << q.data.transpose()/M_PI*180.0 << endl;
			cout << "q_sol " << q_sol.data.transpose()/M_PI*180.0 << endl;
			cout << "q_init " << q_init.data.transpose()/M_PI*180.0 << endl;
			cout << "return value " << retval << endl;
			cout << "last #iter " << solver.lastNrOfIter << endl;
			cout << "last diff  " << solver.lastDifference << endl;
			cout << "jacobian of goal values ";
			solver.display_jac(q);
			std::cout << "jacobian of solved values ";
			solver.display_jac(q_sol);

		}
		total_number_of_iter += solver.lastNrOfIter;
		if (solver.lastNrOfIter > max_num_of_iter) max_num_of_iter = solver.lastNrOfIter;
		if (solver.lastNrOfIter < min_num_of_iter) min_num_of_iter = solver.lastNrOfIter;
		if (retval!=-3) {
		if (solver.lastDifference > max_diff) max_diff = solver.lastDifference;
		if (solver.lastDifference < min_diff) min_diff = solver.lastDifference;

		if (solver.lastTransDiff > max_trans_diff) max_trans_diff = solver.lastTransDiff;
		if (solver.lastTransDiff < min_trans_diff) min_trans_diff = solver.lastTransDiff;

		if (solver.lastRotDiff > max_trans_diff) max_rot_diff = solver.lastRotDiff;
		if (solver.lastRotDiff < min_trans_diff) min_rot_diff = solver.lastRotDiff;
		}
		fwdkin.JntToCart(q_sol,pos_reached);
	}
	cout << "------------------ statistics ------------------------------"<<endl;
	cout << "#times successful " << nrofresult_ok << endl;
	cout << "#times -1 result " << nrofresult_minus1 << endl;
	cout << "#times -2 result " << nrofresult_minus2 << endl;
	cout << "#times -3 result " << nrofresult_minus3 << endl;
	cout << "average number of iter " << (double)total_number_of_iter/(double)num_of_trials << endl;
	cout << "min. nr of iter " << min_num_of_iter << endl;
	cout << "max. nr of iter " << max_num_of_iter << endl;
	cout << "min. difference after solving " << min_diff << endl;
	cout << "max. difference after solving " << max_diff << endl;
	cout << "min. trans. difference after solving " << min_trans_diff << endl;
	cout << "max. trans. difference after solving " << max_trans_diff << endl;
	cout << "min. rot. difference after solving " << min_rot_diff << endl;
	cout << "max. rot. difference after solving " << max_rot_diff << endl;
	double el = timer.elapsed();
	cout << "elapsed time " << el << endl;
	cout << "estimate of average time per invposkin (ms)" << el/num_of_trials*1000 << endl;
	cout << "estimate of longest time per invposkin (ms) " << el/total_number_of_iter*max_num_of_iter *1000 << endl;
	cout << "estimate of shortest time per invposkin (ms) " << el/total_number_of_iter*min_num_of_iter *1000 << endl;
}

int main(int argc,char* argv[]) {
	std::cout <<
			" This example generates random joint positions, applies a forward kinematic transformation,\n"
		<<  " and calls ChainIkSolverPos_LMA on the resulting pose.  In this way we are sure that\n"
		<<  " the resulting pose is reachable.  However, some percentage of the trials will be at\n"
		<<  " near singular position, where it is more difficult to achieve convergence and accuracy\n"
		<<  " The initial position given to the algorithm is also a random joint position\n"
		<<  " This routine asks for an accuracy of 10 micrometer, one order of magnitude better than a\n"
		<<  " a typical industrial robot.\n"
		<<  " This routine can take more then 6 minutes to execute. It then gives statistics on execution times\n"
		<<  " and failures.\n"
		<<  " Typically when executed 1 000 000 times, you will still see some small amount of failures\n"
		<<  " Typically these failures are in the neighbourhoud of singularities.  Most failures of type -2 still\n"
		<<  " reach an accuracy better than 1E-4.\n"
		<<  " This is much better than ChainIkSolverPos_NR, which fails a few times per 100 trials.\n";
	using namespace KDL;
	Chain chain;
	chain = KDL::Puma560();
	//chain = KDL::KukaLWR_DHnew();

	ChainIkSolverPos_LMA solver(chain);

	test_inverseposkin(chain);

    return 0;
}

/** results with 1 million inv pos kin.

#times successful 999992
#times -1 result 0
#times -2 result 5
#times -3 result 3
average number of iter 16.6437
min. nr of iter 13
max. nr of iter 500
min. difference after solving 3.86952e-12
max. difference after solving 4.79339e-05
min. trans. difference after solving 3.86952e-12
max. trans. difference after solving 4.79339e-05
min. rot. difference after solving 0
max. rot. difference after solving 0.000261335
elapsed time 199.14
estimate of average time per invposkin (ms)0.19914
estimate of longest time per invposkin (ms) 5.98245
estimate of shortest time per invposkin (ms) 0.155544

*/

