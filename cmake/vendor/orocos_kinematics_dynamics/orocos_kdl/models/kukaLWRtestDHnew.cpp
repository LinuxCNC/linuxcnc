#include <chain.hpp>
#include "models.hpp"
#include <frames_io.hpp>
#include <kinfam_io.hpp>

#include <chainfksolverpos_recursive.hpp>
#include <chainidsolver_recursive_newton_euler.hpp>

using namespace KDL;
using namespace std;

void outputLine( double, double, double, double, double, double, double);
int getInputs(JntArray&, JntArray&, JntArray&, int&);

int main(int argc , char** argv){
    
    Chain kLWR=KukaLWR_DHnew();
  
    JntArray q(kLWR.getNrOfJoints());
    JntArray qdot(kLWR.getNrOfJoints());
    JntArray qdotdot(kLWR.getNrOfJoints());
    JntArray tau(kLWR.getNrOfJoints());
    Wrenches f(kLWR.getNrOfSegments());
    int linenum; //number of experiment= number of line       
    getInputs(q, qdot,qdotdot,linenum);
    
    ChainFkSolverPos_recursive fksolver(kLWR);
    Frame T;
    ChainIdSolver_RNE idsolver(kLWR,Vector(0.0,0.0,-9.81));

    fksolver.JntToCart(q,T);
    idsolver.CartToJnt(q,qdot,qdotdot,f,tau);

    std::cout<<"pose: \n"<<T<<std::endl;
    std::cout<<"tau: "<<tau<<std::endl;
    
//write file: code based on example 14.4, c++ how to program, Deitel and Deitel, book p 708
  ofstream outPoseFile("poseResultaat.dat",ios::app);
  if(!outPoseFile)
  {
  cerr << "File poseResultaat could not be opened" <<endl;
  exit(1);
  }
  outPoseFile << "linenumber=experimentnr= "<< linenum << "\n";
  outPoseFile << T << "\n \n";
  outPoseFile.close();

  ofstream outTauFile("tauResultaat.dat",ios::app);
  if(!outTauFile)
  {
  cerr << "File tauResultaat could not be opened" <<endl;
  exit(1);
  }
  outTauFile << setiosflags( ios::left) << setw(10)  << linenum;
  outTauFile << tau << "\n";
  outTauFile.close();
}
    


int getInputs(JntArray &_q, JntArray &_qdot, JntArray &_qdotdot, int &linenr)
{
  //cout << " q" << _q<< "\n";
  
  //declaration
  //int linenr; //line =experiment number
  int counter;
  
  //initialisation
  counter=0;
  
  //ask which experiment number= line number in files
  cout << "Give experiment number= line number in files \n ?";
  cin >> linenr;
    
  //read files: code based on example 14.8, c++ how to program, Deitel and Deitel, book p 712
  
  /*
   *READING Q = joint positions
   */
  
  ifstream inQfile("interpreteerbaar/q", ios::in);

  if (!inQfile)
  {
    cerr << "File q could not be opened \n";
    exit(1);
  }
  
  //print headers
  cout << setiosflags( ios::left) << setw(15) << "_q(0)" << setw(15) << "_q(1)" << setw(15) << "_q(2)" << setw(15) << "_q(3)" << setw(15) << "_q(4)" << setw(15) << "_q(5)" << setw(15) << "_q(6)"   << " \n" ;
  
  while(!inQfile.eof())
  {
    //read out a line of the file
    inQfile >> _q(0) >> _q(1) >> _q(2) >> _q(3) >> _q(4) >> _q(5) >> _q(6); 
    counter++;
    if(counter==linenr)
    {
      outputLine( _q(0), _q(1), _q(2), _q(3), _q(4), _q(5), _q(6));
      break;
    }
    
  }
  inQfile.close();
  
  /*
   *READING Qdot = joint velocities
   */
  counter=0;//reset counter
  ifstream inQdotfile("interpreteerbaar/qdot", ios::in);

  if (!inQdotfile)
  {
    cerr << "File qdot could not be opened \n";
    exit(1);
  }
  
  //print headers
  cout << setiosflags( ios::left) << setw(15) << "_qdot(0)" << setw(15) << "_qdot(1)" << setw(15) << "_qdot(2)" << setw(15) << "_qdot(3)" << setw(15) << "_qdot(4)" << setw(15) << "_qdot(5)" << setw(15) << "_qdot(6)"   << " \n" ;
  
  while(!inQdotfile.eof())
  {
    //read out a line of the file
    inQdotfile >> _qdot(0) >> _qdot(1) >> _qdot(2) >> _qdot(3) >> _qdot(4) >> _qdot(5) >> _qdot(6) ; 
    counter++;
    if(counter==linenr)
    {
      outputLine( _qdot(0), _qdot(1), _qdot(2), _qdot(3), _qdot(4), _qdot(5), _qdot(6));
      break;
    }
    
  }
  inQdotfile.close(); 
  
  /*
   *READING Qdotdot = joint accelerations
   */
  counter=0;//reset counter
  ifstream inQdotdotfile("interpreteerbaar/qddot", ios::in);

  if (!inQdotdotfile)
  {
    cerr << "File qdotdot could not be opened \n";
    exit(1);
  }
  
  //print headers
  cout << setiosflags( ios::left) << setw(15) << "_qdotdot(0)" << setw(15) << "_qdotdot(1)" << setw(15) << "_qdotdot(2)" << setw(15) << "_qdotdot(3)" << setw(15) << "_qdotdot(4)" << setw(15) << "_qdotdot(5)" << setw(15) << "_qdotdot(6)"  << " \n" ;
  
  while(!inQdotdotfile.eof())
  {
    //read out a line of the file
    inQdotdotfile >> _qdotdot(0) >> _qdotdot(1) >> _qdotdot(2) >> _qdotdot(3) >> _qdotdot(4) >> _qdotdot(5) >> _qdotdot(6); 
    counter++;
    if(counter==linenr)
    {
      outputLine(_qdotdot(0), _qdotdot(1), _qdotdot(2), _qdotdot(3), _qdotdot(4), _qdotdot(5), _qdotdot(6) );
      break;
    }
    
  }
  inQdotdotfile.close();

  
  return 0;
}

void outputLine( double x1, double x2, double x3, double x4, double x5, double x6, double x7)
{
  cout << setiosflags(ios::left) << setiosflags(ios::fixed | ios::showpoint) <<setw(15) 
  << x1 << setw(15) << x2 <<setw(15) <<setw(15) << x3 <<setw(15) << x4 <<setw(15) << x5 <<setw(15) << x6 <<setw(15) << x7 <<"\n";
}
