#ifndef EMCPOS_H
#define EMCPOS_H


#include "posemath.h"		/* PmCartesian */

#ifdef __cplusplus
class EmcPose 
{
    public:

    EmcPose() {tran.x = tran.y = tran.z = a = b = c = 0;}

    PmCartesian tran;
    double a,b,c;
    double & operator[](int i);
    EmcPose operator - (EmcPose v1);
    EmcPose operator + (EmcPose v1);
    bool operator == (EmcPose v1);
    bool operator != (EmcPose v1);
};
#else

typedef struct _EmcPose {
    PmCartesian tran;
    double a,b,c;
} EmcPose;
#endif
#endif
