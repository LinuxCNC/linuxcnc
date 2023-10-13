#include "sc_conversion.h"

struct sc_pnt emc_pose_to_sc_pnt(struct EmcPose pose){
    struct sc_pnt pnt;
    pnt.x=pose.tran.x;
    pnt.y=pose.tran.y;
    pnt.z=pose.tran.z;
    return pnt;
}

struct sc_dir emc_pose_to_sc_dir(struct EmcPose pose){
    struct sc_dir dir;
    dir.a=pose.a;
    dir.b=pose.b;
    dir.c=pose.c;
    return dir;
}

struct sc_ext emc_pose_to_sc_ext(struct EmcPose pose){
    struct sc_ext ext;
    ext.u=pose.u;
    ext.v=pose.v;
    ext.w=pose.w;
    return ext;
}

struct sc_pnt emc_cart_to_sc_pnt( PmCartesian pnt){
    struct sc_pnt p;
    p.x=pnt.x;
    p.y=pnt.y;
    p.z=pnt.z;
    return p;
}

PmCartesian sc_pnt_to_emc_cart(struct sc_pnt pnt){
    PmCartesian p;
    p.x=pnt.x;
    p.y=pnt.y;
    p.z=pnt.z;
    return p;
}

B emc_pose_xyz_equal(struct EmcPose pose0, struct EmcPose pose1){

    if(pose0.tran.x==pose1.tran.x && pose0.tran.y==pose1.tran.y && pose0.tran.z==pose1.tran.z){
        return 1;
    }
    return 0;
}


















