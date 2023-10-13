#include "sc_optimizer.h"

sc_optimizer::sc_optimizer( )
{

}

std::vector<sc_block> sc_optimizer::sc_optimize_block_angles_ve(std::vector<sc_block> blockvec, I exec_nr, I ahead_nr, T vm){

    //! Calculate motion block corners.
    blockvec=sc_get_blockangles(blockvec,exec_nr,ahead_nr);

    //! Set end velocity, based on block corners, if no angle, ve is set to vm at this stage.
    blockvec=sc_get_corner_ve_blockangles(blockvec,exec_nr,ahead_nr,vm);

    return blockvec;
}

std::vector<sc_block> sc_optimizer::sc_optimize_gforce_arcs(std::vector<sc_block> blockvec, T vm, T gforcemax){
    //! Set the velmax for arc's using gforce value. Set the velmax for lines to program velmax.
    blockvec=sc_get_velmax_gforce(blockvec,vm,gforcemax);

    return blockvec;
}

std::vector<sc_block> sc_optimizer::sc_optimize_G0_ve(std::vector<sc_block> blockvec){

    for(UI i=0; i<blockvec.size(); i++){

        if(blockvec.at(i).type==sc_rapid){ //! End velocity=0 if motion is G0, rapid.
            blockvec.at(i).vo=0;
            blockvec.at(i).ve=0;

            //! Set next block vo to 0.
            if(i<blockvec.size()-1){
                blockvec.at(i+1).vo=0;
            }
        }
    }
    return blockvec;
}

std::vector<sc_block> sc_optimizer::sc_get_velmax_gforce(std::vector<sc_block> blockvec,
                                                         T velmax, T gforcemax){
    for(UI i=0; i<blockvec.size(); i++){

        if(blockvec.at(i).primitive_id==sc_primitive_id::sc_arc){

            T radius=0;
            sc_arcs().sc_arc_radius(blockvec.at(i).pnt_s,
                                    blockvec.at(i).pnt_w,
                                    blockvec.at(i).pnt_e,radius);

            //! Checks gforce using the program's velmax value.
            T gforce=0;
            sc_get_gforce(velmax,radius,gforce);

            if(gforce>gforcemax){
                // std::cerr<<"gforce arc reduced from:"<<gforce<<" to:"<<gforcemax<<std::endl;

                T maxvel_arc=0;
                sc_set_gforce(radius,gforcemax,maxvel_arc);
                // std::cerr<<"arc vm reduced from:"<<velmax<<" to:"<<maxvel_arc<<std::endl;

                blockvec.at(i).vm=maxvel_arc;
            } else {
                blockvec.at(i).vm=velmax;
            }
        } else { //! For a line set velmax as usual.
            blockvec.at(i).vm=velmax;
        }
    }
    return blockvec;
}

std::vector<sc_block> sc_optimizer::sc_get_corner_ve_blockangles(std::vector<sc_block> blockvec, I exec_nr, I ahead_nr, T velmax){

    for(UI i=exec_nr; i<fmin(blockvec.size(),exec_nr+ahead_nr); i++){

        if(blockvec.at(i).angle_end_deg<=90){ //! Stop required.
            blockvec.at(i).ve=0;
            //! Set next block vo to zero.
            //if(i<blockvec.size()-1){
               // blockvec.at(i+1).vo=0;
           // }
            // std::cout<<"angle<=90, ve:"<<0<<std::endl;
        }

        if(blockvec.at(i).angle_end_deg>90){ //! Percentage ve up to 180 degrees. //! 180 degrees = colinear.
            T angle_deg=blockvec.at(i).angle_end_deg;
            T factor=(angle_deg-90)/90; //! 0-1, 1=straight on. 0=90 degrees.
            T ve=velmax*factor;

            blockvec.at(i).ve=ve;
            // std::cout<<"angle>90, ve:"<<ve<<std::endl;
            //! Set next block vo to this ve.
           // if(i<blockvec.size()-1){
                //blockvec.at(i+1).vo=ve;
           // }
        }
    }
    return blockvec;
}



V sc_optimizer::sc_set_gforce(T radius, T gforce, T &vel_mm_sec){

    T circumfence=(radius*2)*M_PI;
    T a=gforce/0.0001;
    T rps= sqrt(a/(4*(M_PI*M_PI)*radius));
    vel_mm_sec=rps*circumfence;
}

V sc_optimizer::sc_get_gforce(T vel_mm_sec, T radius, T &gforce){

    T circumfence=(radius*2)*M_PI;
    T rps=vel_mm_sec/circumfence;
    T a=4*(M_PI*M_PI)*radius*(rps*rps); //! [mm/s^2]
    gforce=a*0.0001; //! [g]
}

V sc_optimizer::line_line_angle(sc_pnt p0, sc_pnt p1, sc_pnt p2, T &angle_deg){

    Eigen::Vector3d p00(p0.x,p0.y,p0.z);
    Eigen::Vector3d p11(p1.x,p1.y,p1.z); //! Common point.
    Eigen::Vector3d p22(p2.x,p2.y,p2.z);

    Eigen::Vector3d v1 = p00-p11;
    Eigen::Vector3d v2 = p22-p11;

    v1.normalize();
    v2.normalize();
    T dot = v1.dot(v2);
    T angle_rad = acos(dot);
    angle_deg = angle_rad*to_degrees;
}

V sc_optimizer::line_arc_angle(sc_pnt p0,
                               sc_pnt p1,
                               sc_pnt p2,
                               sc_pnt p3,
                               T &angle_deg){

    sc_pnt pi;
    sc_arcs().sc_interpolate_arc(p1,p2,p3,0.1,pi);
    line_line_angle(p0,p1,pi,angle_deg);
}

V sc_optimizer::arc_line_angle(sc_pnt p0,
                               sc_pnt p1,
                               sc_pnt p2,
                               sc_pnt p3,
                               T &angle_deg){

    sc_pnt pi;
    sc_arcs().sc_interpolate_arc(p0,p1,p2,0.9,pi);
    line_line_angle(pi,p2,p3,angle_deg);
}

V sc_optimizer::arc_arc_angle(sc_pnt p0,
                              sc_pnt p1,
                              sc_pnt p2,
                              sc_pnt p3,
                              sc_pnt p4,
                              T &angle_deg){

    sc_pnt pi0, pi1;
    sc_arcs().sc_interpolate_arc(p0,p1,p2,0.9,pi0);
    sc_arcs().sc_interpolate_arc(p2,p3,p4,0.1,pi1);
    line_line_angle(pi0,p2,pi1,angle_deg);
}

V sc_optimizer::sc_print_blockvec(std::vector<sc_block> blockvec){

    std::cout<<"Optimzer results:"<<std::endl;

    for(UI i=0; i<blockvec.size(); i++){
        std::cout<<"    nr:"<<i<<std::endl;

        if(blockvec.at(i).primitive_id==sc_primitive_id::sc_arc){
            std::cout<<"    id: arc"<<std::endl;
        }
        if(blockvec.at(i).primitive_id==sc_primitive_id::sc_line){
            std::cout<<"    id: line"<<std::endl;
        }
        if(blockvec.at(i).type==sc_type::sc_rapid){
            std::cout<<"    type: G0"<<std::endl;
        }
        if(blockvec.at(i).type==sc_type::sc_linear){
            std::cout<<"    type: G1"<<std::endl;
        }
        if(blockvec.at(i).type==sc_type::sc_circle){
            std::cout<<"    type: G2"<<std::endl;
        }
        if(blockvec.at(i).type==sc_type::sc_G3){
            std::cout<<"    type: G3"<<std::endl;
        }

        std::cout<<"    vo:"<<blockvec.at(i).vo<<std::endl;
        std::cout<<"    ve:"<<blockvec.at(i).ve<<std::endl;
        std::cout<<"    s:"<<blocklenght(blockvec.at(i))<<std::endl;
        std::cout<<"    gcode_line_nr:"<<blockvec.at(i).gcode_line_nr<<std::endl;
        std::cout<<""<<std::endl;
    }
    std::cout<<""<<std::endl;

}

void optimize_gcode(struct sc_block *pvec, int size, T vm, T **path_lenghts, int *path_lenght_size, T *stot){

    printf("optimzing gcode given vm: %f \n",vm);

    //! Set all vo & ve to vm except first and last of gcode file.
    for(int i=0; i<size; i++){
        pvec[i].vo=vm;
        pvec[i].ve=vm;

        if(i==0){
            pvec[i].vo=0;
        }
        if(i==size-1){
            pvec[i].ve=0;
        }
    }

    //! If G0, set vo, ve to zero, set next gcode line start vo to zero.
    for(int i=0; i<size; i++){
        if(pvec[i].type==sc_rapid){
            pvec[i].vo=0;
            pvec[i].ve=0;

            if(i<size-1){
                pvec[i+1].vo=0;
            }
        }
    }


    std::vector<int> itemvec;
    std::vector<std::vector<int>> program;
    std::vector<double> programlenght;
    T ttot=0;

    //! Group motions of type G1,2,3 that may be fused to a single scurve.
    for(int i=0; i<size; i++){
        T lenght=pvec[i].path_lenght;

       if(pvec[i].type==sc_rapid){

           itemvec.push_back(i);
           program.push_back(itemvec);
           programlenght.push_back(lenght);
           itemvec.clear();
       }

       if(pvec[i].type!=sc_rapid){

            itemvec.push_back(i);
            ttot+=lenght;

            if(i<size-1){
                if(pvec[i+1].type==sc_rapid){
                    program.push_back(itemvec);
                    programlenght.push_back(ttot);
                    ttot=0;
                    itemvec.clear();
                }
            }
        }
    }

    //! Print fusec results.
    *stot=0;
    for(uint i=0; i<program.size(); i++){

        std::cout<<"program at i:"<<i<<std::endl;
        std::cout<<"stot:"<<programlenght[i]<<std::endl;
        *stot+=programlenght[i];

        for(uint j=0; j<program.at(i).size(); j++){

            std::cout<<"fused id:"<<program[i][j]<<std::endl;
            std::cout<<"fused gcode_line_nr:"<<pvec[program[i][j]].gcode_line_nr<<std::endl;
        }
    }

    //! To c style.
    *path_lenght_size=programlenght.size();
    *path_lenghts = (double*) malloc(*path_lenght_size * sizeof(double));
    //! Copy.
    double old=0;
    for(int i=0; i<*path_lenght_size; i++){
        //! Save as absolute path values.
        (*path_lenghts)[i]=programlenght[i]+old;
        old+=programlenght[i];
    }
}

extern "C" void optimize_gcode_c(struct sc_block *pvec, int size, T vm, T **path_lenghts, int *path_lenght_size, T *stot){
    optimize_gcode(pvec,size,vm,path_lenghts,path_lenght_size,stot);
}


//! Test function.
//! printf("%lf", (*path_lenghts)[0]);
void test( double **path_lenghts, int size){
    *path_lenghts = (double*) malloc(size * sizeof(double));
    //! Copy.
    for(int i=0; i<size; i++){
        (*path_lenghts)[i]=10;
    }
}

std::vector<sc_block> sc_optimizer::sc_get_blockangles(std::vector<sc_block> blockvec, I exec_nr, I ahead_nr){

    if(blockvec.size()>0){ //! Vector safe.
        for(UI i=exec_nr; i<fmin(blockvec.size()-1,exec_nr+ahead_nr); i++){
            T angle_deg=0;
            if(blockvec.at(i).primitive_id==sc_primitive_id::sc_line &&
                    blockvec.at(i+1).primitive_id==sc_primitive_id::sc_line  ){
                line_line_angle(blockvec.at(i).pnt_s,
                                blockvec.at(i).pnt_e,
                                blockvec.at(i+1).pnt_e,angle_deg);
                blockvec.at(i).angle_end_deg=angle_deg;
            }
            if(blockvec.at(i).primitive_id==sc_primitive_id::sc_line &&
                    blockvec.at(i+1).primitive_id==sc_primitive_id::sc_arc  ){
                line_arc_angle(blockvec.at(i).pnt_s,
                               blockvec.at(i).pnt_e,
                               blockvec.at(i+1).pnt_w,
                               blockvec.at(i+1).pnt_e,angle_deg);
                blockvec.at(i).angle_end_deg=angle_deg;
            }
            if(blockvec.at(i).primitive_id==sc_primitive_id::sc_arc &&
                    blockvec.at(i+1).primitive_id==sc_primitive_id::sc_line ){
                arc_line_angle(blockvec.at(i).pnt_s,
                               blockvec.at(i).pnt_w,
                               blockvec.at(i).pnt_e,
                               blockvec.at(i+1).pnt_e,angle_deg);
                blockvec.at(i).angle_end_deg=angle_deg;
            }
            if(blockvec.at(i).primitive_id==sc_primitive_id::sc_arc &&
                    blockvec.at(i+1).primitive_id==sc_primitive_id::sc_arc ){
                arc_arc_angle(blockvec.at(i).pnt_s,
                              blockvec.at(i).pnt_w,
                              blockvec.at(i).pnt_e,
                              blockvec.at(i+1).pnt_w,
                              blockvec.at(i+1).pnt_e,angle_deg);
                blockvec.at(i).angle_end_deg=angle_deg;
            }
            std::cout<<"i:"<<i<<" angle:"<<angle_deg<<std::endl;
        }

    }
    return blockvec;
}












