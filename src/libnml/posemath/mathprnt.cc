
/*
  Modification History:

  21-Jan-2004  P.C. Moved across from the original EMC source tree.
*/

#include <iostream.h>
#include "posemath.h"
#include "mathprnt.h"

ostream & operator <<(ostream & stream, PM_CARTESIAN v)
{
    stream << v.x << "\t" << v.y << "\t" << v.z;

    return stream;
}

ostream & operator <<(ostream & stream, PM_SPHERICAL s)
{
    stream << s.theta << "\t" << s.phi << "\t" << s.r;

    return stream;
}

ostream & operator <<(ostream & stream, PM_CYLINDRICAL c)
{
    stream << c.theta << "\t" << c.r << "\t" << c.z;

    return stream;
}

ostream & operator <<(ostream & stream, PM_QUATERNION q)
{
    stream << q.s << "\t" << q.x << "\t" << q.y << "\t" << q.z;

    return stream;
}

ostream & operator <<(ostream & stream, PM_ROTATION_VECTOR r)
{
    stream << r.s << "\t" << r.x << "\t" << r.y << "\t" << r.z;

    return stream;
}

ostream & operator <<(ostream & stream, PM_ROTATION_MATRIX m)
{
    int row, col;

    for (col = 0; col < 3; col++) {
	for (row = 0; row < 3; row++) {
	    stream << m[row][col] << "\t";
	}
	stream << endl;
    }

    return stream;
}

ostream & operator <<(ostream & stream, PM_EULER_ZYZ zyz)
{
    stream << zyz.z << "\t" << zyz.y << "\t" << zyz.zp;

    return stream;
}

ostream & operator <<(ostream & stream, PM_EULER_ZYX zyx)
{
    stream << zyx.z << "\t" << zyx.y << "\t" << zyx.x;

    return stream;
}

ostream & operator <<(ostream & stream, PM_RPY rpy)
{
    stream << rpy.r << "\t" << rpy.p << "\t" << rpy.y;

    return stream;
}

ostream & operator <<(ostream & stream, PM_POSE pose)
{
    stream << pose.tran << "\t" << pose.rot;

    return stream;
}

ostream & operator <<(ostream & stream, PM_HOMOGENEOUS hom)
{
    int row, col;

    for (col = 0; col < 4; col++) {
	for (row = 0; row < 4; row++) {
	    stream << hom[row][col] << "\t";
	}
	stream << endl;
    }

    return stream;
}
