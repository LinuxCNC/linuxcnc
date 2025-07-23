#ifndef POSEMATH_HH
#define POSEMATH_HH

#include "posemath.h"

#define USE_CONST
#define USE_CCONST
#define USE_REF

#ifdef USE_CCONST
#define PM_CCONST const
#else
#define PM_CCONST
#endif

#ifdef USE_CONST
#define PM_CONST const
#else
#define PM_CONST
#endif

#ifdef USE_REF
#define PM_REF  &
#else
#define PM_REF
#endif

#define INCLUDE_POSEMATH_COPY_CONSTRUCTORS

/* forward declarations-- conversion ctors will need these */

/* translation types */
struct PM_CARTESIAN;		/* Cart */
struct PM_SPHERICAL;		/* Sph */
struct PM_CYLINDRICAL;		/* Cyl */

/* rotation types */
struct PM_ROTATION_VECTOR;	/* Rot */
struct PM_ROTATION_MATRIX;	/* Mat */
struct PM_QUATERNION;		/* Quat */
struct PM_EULER_ZYZ;		/* Zyz */
struct PM_EULER_ZYX;		/* Zyx */
struct PM_RPY;			/* Rpy */

/* pose types */
struct PM_POSE;			/* Pose */
struct PM_HOMOGENEOUS;		/* Hom */

/* PM_CARTESIAN */

struct PM_CARTESIAN : public PmCartesian {
    /* ctors/dtors */
    PM_CARTESIAN() = default;
    PM_CARTESIAN(double _x, double _y, double _z);
    PM_CARTESIAN(const PmCartesian & cart) : PmCartesian(cart) {};

    PM_CARTESIAN(PM_CONST PM_CYLINDRICAL PM_REF c);	/* conversion */
    PM_CARTESIAN(PM_CONST PM_SPHERICAL PM_REF s);	/* conversion */

    /* operators */
    double &operator[] (int n);	/* this[n] */
    PM_CARTESIAN & operator += (const PM_CARTESIAN &o);
    PM_CARTESIAN & operator -= (const PM_CARTESIAN &o);

    // Scalar operations
    PM_CARTESIAN & operator *= (double o);
    PM_CARTESIAN & operator /= (double o);

    void shift(int n);
};

PM_CARTESIAN shift(PM_CARTESIAN const &p, int n);

/* PM_SPHERICAL */

struct PM_SPHERICAL {
    /* ctors/dtors */
    PM_SPHERICAL() {
    }
#ifdef INCLUDE_POSEMATH_COPY_CONSTRUCTORS
    PM_SPHERICAL(PM_CCONST PM_SPHERICAL & s);
#endif
    PM_SPHERICAL(double _theta, double _phi, double _r);
    PM_SPHERICAL(PM_CONST PM_CYLINDRICAL PM_REF v);	/* conversion */
    PM_SPHERICAL(PM_CONST PM_CARTESIAN PM_REF v);	/* conversion */

    /* operators */
    double &operator[] (int n);	/* this[n] */

    /* data */
    double theta, phi, r;
};

/* PM_CYLINDRICAL */

struct PM_CYLINDRICAL {
    /* ctors/dtors */
    PM_CYLINDRICAL() {
    }
#ifdef INCLUDE_POSEMATH_COPY_CONSTRUCTORS
    PM_CYLINDRICAL(PM_CCONST PM_CYLINDRICAL & c);
#endif
    PM_CYLINDRICAL(double _theta, double _r, double _z);
    PM_CYLINDRICAL(PM_CONST PM_CARTESIAN PM_REF v);	/* conversion */
    PM_CYLINDRICAL(PM_CONST PM_SPHERICAL PM_REF v);	/* conversion */

    /* operators */
    double &operator[] (int n);	/* this[n] */

    /* data */
    double theta, r, z;
};

/* PM_ROTATION_VECTOR */

struct PM_ROTATION_VECTOR {
    /* ctors/dtors */
    PM_ROTATION_VECTOR() {
    }
#ifdef INCLUDE_POSEMATH_COPY_CONSTRUCTORS
    PM_ROTATION_VECTOR(PM_CCONST PM_ROTATION_VECTOR & r);
#endif
    PM_ROTATION_VECTOR(double _r, double _x, double _y, double _z);
    PM_ROTATION_VECTOR(PM_CONST PM_QUATERNION PM_REF q);	/* conversion
                                 */

    /* operators */
    double &operator[] (int n);	/* this[n] */

    /* data */
    double s, x, y, z;
};

/* PM_ROTATION_MATRIX */

struct PM_ROTATION_MATRIX {
    /* ctors/dtors */
    PM_ROTATION_MATRIX() {
    }
#ifdef INCLUDE_POSEMATH_COPY_CONSTRUCTORS
    PM_ROTATION_MATRIX(PM_CCONST PM_ROTATION_MATRIX & mat);	/* added
                                   7-May-1997
                                   by WPS */
#endif
    PM_ROTATION_MATRIX(double xx, double xy, double xz,
    double yx, double yy, double yz, double zx, double zy, double zz);
    PM_ROTATION_MATRIX(PM_CARTESIAN _x, PM_CARTESIAN _y, PM_CARTESIAN _z);
    PM_ROTATION_MATRIX(PM_CONST PM_ROTATION_VECTOR PM_REF v);	/* conversion
                                 */
    PM_ROTATION_MATRIX(PM_CONST PM_QUATERNION PM_REF q);	/* conversion
                                 */
    PM_ROTATION_MATRIX(PM_CONST PM_EULER_ZYZ PM_REF zyz);	/* conversion
                                 */
    PM_ROTATION_MATRIX(PM_CONST PM_EULER_ZYX PM_REF zyx);	/* conversion
                                 */
    PM_ROTATION_MATRIX(PM_CONST PM_RPY PM_REF rpy);	/* conversion */

    /* operators */
    PM_CARTESIAN & operator[](int n);	/* this[n] */

    /* data */
    PM_CARTESIAN x, y, z;
};

/* PM_QUATERNION */

using PM_AXIS = PmAxis;

struct PM_QUATERNION : public PmQuaternion {
    /* ctors/dtors */
    PM_QUATERNION() {
    }
#ifdef INCLUDE_POSEMATH_COPY_CONSTRUCTORS
    PM_QUATERNION(PM_CCONST PM_QUATERNION & quat);	/* added 7-May-1997
                               by WPS */
#endif
    PM_QUATERNION(double _s, double _x, double _y, double _z);
    PM_QUATERNION(PM_CONST PM_ROTATION_VECTOR PM_REF v);	/* conversion
                                 */
    PM_QUATERNION(PM_CONST PM_ROTATION_MATRIX PM_REF m);	/* conversion
                                 */
    PM_QUATERNION(PM_CONST PM_EULER_ZYZ PM_REF zyz);	/* conversion */
    PM_QUATERNION(PM_CONST PM_EULER_ZYX PM_REF zyx);	/* conversion */
    PM_QUATERNION(PM_CONST PM_RPY PM_REF rpy);	/* conversion */
    PM_QUATERNION(PM_AXIS axis, double angle);	/* conversion */

    /* operators */
    double &operator[] (int n);	/* this[n] */

    /* functions */
    void axisAngleMult(PM_AXIS axis, double angle);
};

/* PM_EULER_ZYZ */

struct PM_EULER_ZYZ {
    /* ctors/dtors */
    PM_EULER_ZYZ() {
    }
#ifdef INCLUDE_POSEMATH_COPY_CONSTRUCTORS
    PM_EULER_ZYZ(PM_CCONST PM_EULER_ZYZ & zyz);
#endif
    PM_EULER_ZYZ(double _z, double _y, double _zp);
    PM_EULER_ZYZ(PM_CONST PM_QUATERNION PM_REF q);	/* conversion */
    PM_EULER_ZYZ(PM_CONST PM_ROTATION_MATRIX PM_REF m);	/* conversion */

    /* operators */
    double &operator[] (int n);

    /* data */
    double z, y, zp;
};

/* PM_EULER_ZYX */

struct PM_EULER_ZYX : public PmEulerZyx {
    /* ctors/dtors */
    PM_EULER_ZYX() {
    }
#ifdef INCLUDE_POSEMATH_COPY_CONSTRUCTORS
    PM_EULER_ZYX(PM_CCONST PM_EULER_ZYX & zyx);
#endif
    PM_EULER_ZYX(double _z, double _y, double _x);
    PM_EULER_ZYX(PM_CONST PM_QUATERNION PM_REF q);	/* conversion */
    PM_EULER_ZYX(PM_CONST PM_ROTATION_MATRIX PM_REF m);	/* conversion */

    /* operators */
    double &operator[] (int n);
};

/* PM_RPY */

struct PM_RPY {
    /* ctors/dtors */
    PM_RPY() {
    }
#ifdef INCLUDE_POSEMATH_COPY_CONSTRUCTORS
    PM_RPY(PM_CCONST PM_RPY PM_REF rpy);	/* added 7-May-1997 by WPS */
#endif
    PM_RPY(double _r, double _p, double _y);
    PM_RPY(PM_CONST PM_QUATERNION PM_REF q);	/* conversion */
    PM_RPY(PM_CONST PM_ROTATION_MATRIX PM_REF m);	/* conversion */

    /* operators */
    double &operator[] (int n);

    /* data */
    double r, p, y;
};

/* PM_POSE */

struct PM_POSE : public PmPose {
    /* ctors/dtors */
    PM_POSE() {
    }
#ifdef INCLUDE_POSEMATH_COPY_CONSTRUCTORS
    PM_POSE(PM_CCONST PM_POSE & p);
#endif
    PM_POSE(PM_CARTESIAN v, PM_QUATERNION q);
    PM_POSE(double x, double y, double z,
    double s, double sx, double sy, double sz);
    PM_POSE(PM_CONST PM_HOMOGENEOUS PM_REF h);	/* conversion */

    /* operators */
    double &operator[] (int n);	/* this[n] */
};

/* PM_HOMOGENEOUS */

struct PM_HOMOGENEOUS {
    /* ctors/dtors */
    PM_HOMOGENEOUS() {
    }
#ifdef INCLUDE_POSEMATH_COPY_CONSTRUCTORS
    PM_HOMOGENEOUS(PM_CCONST PM_HOMOGENEOUS & h);
#endif
    PM_HOMOGENEOUS(PM_CARTESIAN v, PM_ROTATION_MATRIX m);
    PM_HOMOGENEOUS(PM_CONST PM_POSE PM_REF p);	/* conversion */

    /* operators */
    PM_CARTESIAN & operator[](int n);	/* column vector */

    /* data ( [ 0 0 0 1 ] element is manually returned by [] if needed ) */
    PM_CARTESIAN tran;
    PM_ROTATION_MATRIX rot;
};

/* PM_LINE */

struct PM_LINE {
    /* ctors/dtors */
    PM_LINE() {
    }
#ifdef INCLUDE_POSEMATH_COPY_CONSTRUCTORS
    PM_LINE(PM_CCONST PM_LINE &);
#endif

    /* functions */
    int init(PM_POSE start, PM_POSE end);
    int point(double len, PM_POSE * point);

    /* data */
    PM_POSE start;		/* where motion was started */
    PM_POSE end;		/* where motion is going */
    PM_CARTESIAN uVec;		/* unit vector from start to end */
};

/* PM_CIRCLE */

struct PM_CIRCLE : public PmCircle {
    /* ctors/dtors */
    PM_CIRCLE() {
    }
#ifdef INCLUDE_POSEMATH_COPY_CONSTRUCTORS
    PM_CIRCLE(PM_CCONST PM_CIRCLE &);
#endif

    /* functions */
    int init(PM_POSE start, PM_POSE end,
    PM_CARTESIAN center, PM_CARTESIAN normal, int turn, double expected_angle_rad);
    int point(double angle_, PM_POSE * point);
};

/* overloaded external functions */

/* dot */
extern double dot(const PM_CARTESIAN &v1, const PM_CARTESIAN &v2);

/* cross */
extern PM_CARTESIAN cross(const PM_CARTESIAN &v1, const PM_CARTESIAN &v2);

#if 0
/* norm */
extern PM_CARTESIAN norm(PM_CARTESIAN v);
extern PM_QUATERNION norm(PM_QUATERNION q);
extern PM_ROTATION_VECTOR norm(PM_ROTATION_VECTOR r);
extern PM_ROTATION_MATRIX norm(PM_ROTATION_MATRIX m);
#endif

/* unit */
extern PM_CARTESIAN unit(const PM_CARTESIAN &v);
extern PM_QUATERNION unit(const PM_QUATERNION &q);
extern PM_ROTATION_VECTOR unit(const PM_ROTATION_VECTOR &r);
extern PM_ROTATION_MATRIX unit(const PM_ROTATION_MATRIX &m);

/* isNorm */
extern int isNorm(const PM_CARTESIAN &v);
extern int isNorm(const PM_QUATERNION &q);
extern int isNorm(const PM_ROTATION_VECTOR &r);
extern int isNorm(const PM_ROTATION_MATRIX &m);

/* mag */
extern double mag(const PM_CARTESIAN &v);

/* disp */
extern double disp(const PM_CARTESIAN &v1, const PM_CARTESIAN &v2);

/* inv */
extern PM_CARTESIAN inv(const PM_CARTESIAN &v);
extern PM_ROTATION_MATRIX inv(const PM_ROTATION_MATRIX &m);
extern PM_QUATERNION inv(const PM_QUATERNION &q);
extern PM_POSE inv(const PM_POSE &p);
extern PM_HOMOGENEOUS inv(const PM_HOMOGENEOUS &h);

/* project */
extern PM_CARTESIAN proj(const PM_CARTESIAN &v1, const PM_CARTESIAN &v2);

/* overloaded arithmetic functions */

/* unary +, - for translation, rotation, pose */
extern PM_CARTESIAN operator + (const PM_CARTESIAN &v);
extern PM_CARTESIAN operator - (const PM_CARTESIAN &v);
extern PM_QUATERNION operator + (const PM_QUATERNION &q);
extern PM_QUATERNION operator - (const PM_QUATERNION &q);
extern PM_POSE operator + (const PM_POSE &p);
extern PM_POSE operator - (const PM_POSE &p);

/* compare operators */
extern int operator == (const PM_CARTESIAN &v1, const PM_CARTESIAN &v2);
extern int operator == (const PM_QUATERNION &q1, const PM_QUATERNION &q2);
extern int operator == (const PM_POSE &p1, const PM_POSE &p2);
extern int operator != (const PM_CARTESIAN &v1, const PM_CARTESIAN &v2);
extern int operator != (const PM_QUATERNION &q1, const PM_QUATERNION &q2);
extern int operator != (const PM_POSE &p1, const PM_POSE &p2);

/* translation +, -, scalar *, - */

/* v + v */
extern PM_CARTESIAN operator + (PM_CARTESIAN v1, const PM_CARTESIAN &v2);
/* v - v */
extern PM_CARTESIAN operator - (PM_CARTESIAN v1, const PM_CARTESIAN &v2);
/* v * s */
extern PM_CARTESIAN operator *(PM_CARTESIAN v, double s);
/* s * v */
extern PM_CARTESIAN operator *(double s, PM_CARTESIAN v);
/* v / s */
extern PM_CARTESIAN operator / (const PM_CARTESIAN &v, double s);

/* rotation * by scalar, translation, and rotation */

/* s * q */
extern PM_QUATERNION operator *(double s, const PM_QUATERNION &q);
/* q * s */
extern PM_QUATERNION operator *(const PM_QUATERNION &q, double s);
/* q / s */
extern PM_QUATERNION operator / (const PM_QUATERNION &q, double s);
/* q * v */
extern PM_CARTESIAN operator *(const PM_QUATERNION &q, const PM_CARTESIAN &v);
/* q * q */
extern PM_QUATERNION operator *(const PM_QUATERNION &q1, const PM_QUATERNION &q2);
/* m * m */
extern PM_ROTATION_MATRIX operator *(const PM_ROTATION_MATRIX &m1,
    const PM_ROTATION_MATRIX &m2);

/* pose operators */

/* q * p */
extern PM_POSE operator *(const PM_QUATERNION &q, const PM_POSE &p);
/* p * p */
extern PM_POSE operator *(const PM_POSE &p1, const PM_POSE &p2);
/* p * v */
extern PM_CARTESIAN operator *(const PM_POSE &p, const PM_CARTESIAN &v);

/* slicky macros for item-by-item copying between C and C++ structs */

#define toCart(src,dst) do {*dst = PM_CARTESIAN(src);} while (0)

#define toCyl(src,dst) do {(dst)->theta = (src).theta; (dst)->r = (src).r; (dst)->z = (src).z;} while (0)

#define toSph(src,dst) do {(dst)->theta = (src).theta; (dst)->phi = (src).phi; (dst)->r = (src).r;} while (0)

#define toQuat(src,dst) do {(dst)->s = (src).s; (dst)->x = (src).x; (dst)->y = (src).y; (dst)->z = (src).z;} while (0)

#define toRot(src,dst) do {(dst)->s = (src).s; (dst)->x = (src).x; (dst)->y = (src).y; (dst)->z = (src).z;} while (0)

#define toMat(src,dst) do {toCart((src).x, &((dst)->x)); toCart((src).y, &((dst)->y)); toCart((src).z, &((dst)->z));} while (0)

#define toEulerZyz(src,dst) do {(dst)->z = (src).z; (dst)->y = (src).y; (dst)->zp = (src).zp;} while (0)

#define toEulerZyx(src,dst) do {(dst)->z = (src).z; (dst)->y = (src).y; (dst)->x = (src).x;} while (0)

#define toRpy(src,dst) do {(dst)->r = (src).r; (dst)->p = (src).p; (dst)->y = (src).y;} while (0)

#define toPose(src,dst) do {toCart((src).tran, &((dst)->tran)); toQuat((src).rot, &((dst)->rot));} while (0)

#define toHom(src,dst) do {toCart((src).tran, &((dst)->tran)); toMat((src).rot, &((dst)->rot));} while (0)

#define toLine(src,dst) do {toPose((src).start, &((dst)->start)); toPose((src).end, &((dst)->end)); toCart((src).uVec, &((dst)->uVec));} while (0)

#define toCircle(src,dst) do {toCart((src).center, &((dst)->center)); toCart((src).normal, &((dst)->normal)); toCart((src).rTan, &((dst)->rTan)); toCart((src).rPerp, &((dst)->rPerp)); toCart((src).rHelix, &((dst)->rHelix)); (dst)->radius = (src).radius; (dst)->angle = (src).angle; (dst)->spiral = (src).spiral;} while (0)


#endif // POSEMATH_HH
