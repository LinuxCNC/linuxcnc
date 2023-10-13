#ifndef PVTESTS_H
#define PVTESTS_H



#include <kdl/jacobianexpr.hpp>
#include <kdl/test_macros.h>
#include <iostream>

#include <string>
#include <iomanip>

namespace KDL {

template <typename T>
void random(Jacobian<T>& rv) {
	random(rv.value());
	for (int i=0;i<rv.nrOfDeriv();++i) {
		random(rv.deriv(i));
	}
}


template <typename T>
void posrandom(Jacobian<T>& rv) {
	posrandom(rv.value());
	for (int i=0;i<rv.nrOfDeriv();++i) {
		posrandom(rv.deriv(i));
	}
}


template <typename T>
inline void checkEqual(const T& a,const T& b,double eps) {
    KDL_CTX;
    KDL_DIFF(a,b);
	assert(Equal(a,b,eps));
}


template <typename OpID,typename A>
class checkUnary {
	typedef UnaryOp<OpID,A> myOp;
public:
	inline static void check(void (*rnd)(Jacobian<A>&) = &random,double dt=1E-8,double eps=1E-4,int size=1) {
        KDL_CTX;
		Jacobian<A> a(size);
		rnd(a);
        KDL_ARG1(a);
		int i;
		for (i=0;i<a.nrOfDeriv();++i) {
			checkEqual(
				myOp::deriv(a.value(),a.deriv(i)),
				diff(
					myOp::value(a.value()),
					myOp::value(
						addDelta(a.value(),a.deriv(i),dt)
					),
					dt),
				eps
				);
		}		
	}
};


template <typename OpID,typename A>
class checkUnaryVel {
	typedef UnaryOp<OpID,A> myOp;

public:
	inline static void check(void (*rnd)(Jacobian<A>&) = &random,double dt=1E-8,double eps=1E-4,int size=1) {
        KDL_CTX;
		Jacobian<A> a(size);
		rnd(a);
        KDL_ARG1(a);
		int i;
		for (i=0;i<a.nrOfDeriv();++i) {
            KDL_MSG("testing value() components of deriv ");
			checkEqual(
				myOp::deriv(a.value(),a.deriv(i)).value(),
				diff(
					myOp::value(a.value()),
					myOp::value(
						addDelta(a.value(),a.deriv(i).value(),dt)
					),
					dt),
				eps
			);
            typename Traits<A>::derivType d1(
                addDelta(a.deriv(i).value(), a.deriv(i).deriv(),dt));
            typename Traits<A>::valueType a1(
                addDelta(a.value(),a.deriv(i).value(),dt)
            );
            KDL_MSG("testing deriv() components of deriv ");
   	        checkEqual(
				myOp::deriv(a.value(),a.deriv(i)).deriv(),
				diff(
					myOp::deriv(a.value(),a.deriv(i)).value(),
					myOp::deriv(a1, d1).value(),
					dt),
				eps
			);
		}		
	}
};

template <typename OpID,typename A,typename B>
class checkBinary {
	typedef BinaryOp<OpID,A,B> myOp;
public:
	inline static void check(double dt=1E-8,double eps=1E-4,int size=1) {
        KDL_CTX;
		Jacobian<A> a(size);
		random(a);
		Jacobian<B> b(size);
		random(b);
        KDL_ARG2(a,b);
		int i;
		for (i=0;i<a.nrOfDeriv();++i) {
			checkEqual(
				myOp::derivVV(a.value(),a.deriv(i),b.value(),b.deriv(i)),
				diff(
					myOp::value(a.value(),b.value()),
					myOp::value(
						addDelta(a.value(),a.deriv(i),dt),
						addDelta(b.value(),b.deriv(i),dt)
					),
					dt),
				eps
				);
			checkEqual(
				myOp::derivVC(a.value(),a.deriv(i),b.value()),
				diff(
					myOp::value(a.value(),b.value()),
					myOp::value(
						addDelta(a.value(),a.deriv(i),dt),
						b.value()
					),
					dt),
				eps
				);
			checkEqual(
				myOp::derivCV(a.value(),b.value(),b.deriv(i)),
				diff(
					myOp::value(a.value(),b.value()),
					myOp::value(
						a.value(),
						addDelta(b.value(),b.deriv(i),dt)
					),
					dt),
				eps
				);

		}		
	}
};


template <typename OpID,typename A,typename B>
class checkBinary_displ {
	typedef BinaryOp<OpID,A,B> myOp;
public:
	inline static void check(double dt=1E-8,double eps=1E-4,int size=1) {
        KDL_CTX;
		Jacobian<A> a(size);
		random(a);
		Jacobian<B> b(size);
		random(b);
        KDL_ARG2(a,b);
		int i;
		for (i=0;i<a.nrOfDeriv();++i) {
			checkEqual(
				myOp::derivVV(a.value(),a.deriv(i),b.value(),b.deriv(i)),
				diff_displ(
					myOp::value(a.value(),b.value()),
					myOp::value(
						addDelta(a.value(),a.deriv(i),dt),
						addDelta(b.value(),b.deriv(i),dt)
					),
					dt),
				eps
				);
			checkEqual(
				myOp::derivVC(a.value(),a.deriv(i),b.value()),
				diff_displ(
					myOp::value(a.value(),b.value()),
					myOp::value(
						addDelta(a.value(),a.deriv(i),dt),
						b.value()
					),
					dt),
				eps
				);
			checkEqual(
				myOp::derivCV(a.value(),b.value(),b.deriv(i)),
				diff_displ(
					myOp::value(a.value(),b.value()),
					myOp::value(
						a.value(),
						addDelta(b.value(),b.deriv(i),dt)
					),
					dt),
				eps
				);

		}		
	}
};


template <typename OpID,typename A,typename B>
class checkBinaryVel {
	typedef BinaryOp<OpID,A,B> myOp;
public:
	inline static void check(double dt=1E-8,double eps=1E-4,int size=1) {
        KDL_CTX;
		Jacobian<A> a(size);
		random(a);
		Jacobian<B> b(size);
		random(b);
        KDL_ARG2(a,b);
		int i;
		for (i=0;i<a.nrOfDeriv();++i) {
            //A Avalue = A(a.value(),a.deriv(i).value());
            //B Bvalue = B(b.value(),b.deriv(i).value());
            KDL_MSG("testing value() component of derivVV ");
			checkEqual(
				myOp::derivVV(a.value(),a.deriv(i),b.value(),b.deriv(i)).value(),
				diff(
					myOp::value(a.value(),b.value()),
					myOp::value(
						addDelta(a.value(),a.deriv(i).value(),dt),
						addDelta(b.value(),b.deriv(i).value(),dt)
					),
					dt),
				eps
				);
            typename Traits<A>::derivType da1(
                addDelta(a.deriv(i).value(), a.deriv(i).deriv(),dt));
            typename Traits<A>::valueType a1(
                addDelta(a.value(),a.deriv(i).value(),dt)
            );
            typename Traits<B>::derivType db1(
                addDelta(b.deriv(i).value(), b.deriv(i).deriv(),dt));
            typename Traits<B>::valueType b1(
                addDelta(b.value(),b.deriv(i).value(),dt)
            );
 
            KDL_MSG("testing deriv() components of derivVV ");
   	        checkEqual(
				myOp::derivVV(a.value(),a.deriv(i),b.value(),b.deriv(i)).deriv(),
				diff(
					myOp::derivVV(a.value(),a.deriv(i),b.value(),b.deriv(i)).value(),
					myOp::derivVV(a1, da1,b1,db1).value(),
					dt),
				eps
			);
            KDL_MSG("testing deriv() components of derivVC ");
  	        checkEqual(
				myOp::derivVC(a.value(),a.deriv(i),b.value()).deriv(),
				diff(
					myOp::derivVC(a.value(),a.deriv(i),b.value()).value(),
					myOp::derivVC(a1, da1,b.value()).value(),
					dt),
				eps
			);
            KDL_MSG("testing deriv() components of derivCV ");
 	        checkEqual(
				myOp::derivCV(a.value(),b.value(),b.deriv(i)).deriv(),
				diff(
					myOp::derivCV(a.value(),b.value(),b.deriv(i)).value(),
					myOp::derivCV(a.value(),b1,db1).value(),
					dt),
				eps
			);
 
            KDL_MSG("testing value() components of derivVC ");
			checkEqual(
				myOp::derivVC(a.value(),a.deriv(i),b.value()).value(),
				diff(
					myOp::value(a.value(),b.value()),
					myOp::value(
						addDelta(a.value(),a.deriv(i).value(),dt),
						b.value()
					),
					dt),
				eps
				);
            KDL_MSG("testing value() components of derivCV ");
			checkEqual(
				myOp::derivCV(a.value(),b.value(),b.deriv(i)).value(),
				diff(
					myOp::value(a.value(),b.value()),
					myOp::value(
						a.value(),
						addDelta(b.value(),b.deriv(i).value(),dt)
					),
					dt),
				eps
				);
		}		
	}
};


} // namespace
#endif




