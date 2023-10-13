/** \file  
 *		 To test Rall1d.h , Rall2d.h and FVector... and demonstrate 
 *		 some combinations of the datastructures.
 *		 TURN OPTIMIZE OFF (it's a bit to complicated for the optimizer).
 *  \author Erwin Aertbelien, Div. PMA, Dep. of Mech. Eng., K.U.Leuven
 *  \version 
 *		KDL V2
 *   
 *  \par History
 *
 */



#include <kdl/rall1d.h>
#include <kdl/rall1d_io.h>
#include <kdl/rall2d.h>
#include <kdl/rall2d_io.h>
#include <kdl/fvector.h>
#include <kdl/fvector_io.h>
#include <kdl/fvector2.h>
#include <kdl/fvector2_io.h>
#include <kdl/test_macros.h>

//#include <fstream>


using namespace KDL;
using namespace std;

// Again something a little bit more complicated : autodiff to 2nd derivative with N variables
template <class T,int N>
class Rall2dN : 
	public Rall1d<Rall1d<T, FVector<T,N> >, FVector2<Rall1d<T,FVector<T,N> >,N,T>,T>
{
	public:
		Rall2dN() {}
		// dd is an array of an array of doubles
		// dd[i][j] is the derivative towards ith and jth variable
		Rall2dN(T val,T d[N],T dd[N][N]) {
			this->t.t = val;
			this->t.grad= FVector<T,N>(d);
			for (int i=0;i<N;i++) {
				this->grad[i].t = d[i];
				this->grad[i].grad = FVector<T,N>(dd[i]);
			}
		}
};

// Again something a little bit more complicated : the Nth derivative can be defined in a recursive way
template <int N>
class RallNd :
	public Rall1d< RallNd<N-1>, RallNd<N-1>, double >
{
public:
	RallNd() {}
	RallNd(const Rall1d< RallNd<N-1>, RallNd<N-1>,double>& arg) : 
		Rall1d< RallNd<N-1>, RallNd<N-1>,double>(arg) {}
	RallNd(double value,double d[]) {
		this->t    = RallNd<N-1>(value,d);
		this->grad = RallNd<N-1>(d[0],&d[1]);
	}
};

template <>
class RallNd<1> : public Rall1d<double>  {
public:
	RallNd() {}
	RallNd(const Rall1d<double>& arg) :
		Rall1d<double,double,double>(arg) {}
	RallNd(double value,double d[]) {
		t    = value;
		grad = d[0];
	}
};

	


template <class T>
void TstArithm(T& a) {
    KDL_CTX;
	KDL_DISP(a);
	T b(a);
	T c;
	c = a;
	KDL_DIFF(b,a);
	KDL_DIFF(c,a);
	KDL_DIFF( (a*a)*a, a*(a*a)          );
	KDL_DIFF( (-a)+a,T::Zero()                  );
	KDL_DIFF( 2.0*a, a+a );
	KDL_DIFF( a*2.0, a+a );
	KDL_DIFF( a+2.0*a, 3.0*a );
	KDL_DIFF( (a+2.0)*a, a*a +2.0*a );
	KDL_DIFF( ((a+a)+a),(a+(a+a))      );
	KDL_DIFF( asin(sin(a)),  a         );
	KDL_DIFF( atan(tan(a)),  a         );
	KDL_DIFF( acos(cos(a)),  a         );
	KDL_DIFF( tan(a),   sin(a)/cos(a)  );
	KDL_DIFF( exp(log(a)),  a          );
	KDL_DIFF( exp(log(a)*2.0),sqr(a)     );
	KDL_DIFF( exp(log(a)*3.5),pow(a,3.5) );
	KDL_DIFF( sqr(sqrt(a)), a         );
	KDL_DIFF( 2.0*sin(a)*cos(a), sin(2.0*a) );
	KDL_DIFF( (a*a)/a, a              );
	KDL_DIFF( (a*a*a)/(a*a), a              );
	KDL_DIFF( sqr(sin(a))+sqr(cos(a)),T::Identity() );
	KDL_DIFF( sqr(cosh(a))-sqr(sinh(a)),T::Identity() );
	KDL_DIFF( pow(pow(a,3.0),1.0/3) , a );
	KDL_DIFF( hypot(3.0*a,4.0*a),      5.0*a);
	KDL_DIFF( atan2(5.0*sin(a),5.0*cos(a)) , a );
	KDL_DIFF( tanh(a) , sinh(a)/cosh(a) );
}


int main() {
    KDL_CTX;
    //DisplContext::display=true;
	Rall1d<double> a(0.12345,1);
	TstArithm(a);
	const double pb[4] = { 1.0, 2.0, 3.0, 4.0 };
	Rall1d<double,FVector<double,4> > b(0.12345,FVector<double,4>(pb));
	TstArithm(b);

	
	Rall2d<double> d(0.12345,-1,0.9);
	TstArithm(d);
	

	Rall1d<Rall1d<double>,Rall1d<double>,double> f(Rall1d<double>(1,2),Rall1d<double>(2,3));
	TstArithm(f);

	Rall2d<Rall1d<double>,Rall1d<double>,double> g(Rall1d<double>(1,2),Rall1d<double>(2,3),Rall1d<double>(3,4));
	TstArithm(g);
	
	// something more complicated without helper classes :
	Rall1d<Rall1d<double, FVector<double,2> >, FVector2<Rall1d<double,FVector<double,2> >,2,double>,double> h(
		Rall1d<double, FVector<double,2> >(
			1.3,
			FVector<double,2>(2,3)
		),
		FVector2<Rall1d<double,FVector<double,2> >,2,double> (
			Rall1d<double,FVector<double,2> >(
				2,
				FVector<double,2>(5,6)
			),
			Rall1d<double,FVector<double,2> >(
				3,
				FVector<double,2>(6,9)
			)
		)
	);
	TstArithm(h);
	
	// with a helper-class and 3 variables
	double pj[] = {2.0,3.0,4.0};
	double ppj[][3] = {{5.0,6.0,7.0},{6.0,8.0,9.0},{7.0,9.0,10.0}};
	Rall2dN<double,3> j(1.3,pj,ppj);
	TstArithm(j);

	// to calculate the Nth derivative :
	double pk[] = {2.0,3.0,4.0,5.0};
	RallNd<4> k(1.3,pk);
	TstArithm(k);

	return 0;
}
