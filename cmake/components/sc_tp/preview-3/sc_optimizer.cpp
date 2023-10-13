#include "sc_optimizer.h"

sc_optimizer::sc_optimizer( )
{

}

V sc_optimizer::sc_set_a_dv_gforce_velmax(T acceleration, T delta_v, T gforce_max, T velocity_max){
    a=acceleration;
    dv=delta_v;
    gforcemax=gforce_max;
    vm=velocity_max;
    engine->sc_set_a_dv(a,dv);
}

std::vector<sc_block> sc_optimizer::sc_optimize_all(std::vector<sc_block> blockvec){

    //! std::cout<<"optimizer acc:"<<a<<std::endl;
    //! std::cout<<"optimizer dv:"<<dv<<std::endl;
    //! std::cout<<"optimizer gforce:"<<gforcemax<<std::endl;
    //! std::cout<<"optimizer vm:"<<vm<<std::endl;

    blockvec=sc_optimize_block_angles_ve(blockvec);
    blockvec=sc_optimize_gforce_arcs(blockvec);
    blockvec=sc_optimize_G0_ve(blockvec);
    blockvec=sc_optimize_G123_ve_backward(blockvec);
    blockvec=sc_optimize_G123_ve_forward(blockvec);
    return blockvec;
}

sc_block sc_optimizer::convert_cpp_to_c(sc_block in){

    sc_block out;
    out.primitive_id=in.primitive_id;
    out.type=in.type;

    out.gcode_line_nr=in.gcode_line_nr;

    out.pnt_s=in.pnt_s, out.pnt_e=in.pnt_e, out.pnt_w=in.pnt_w;
    out.dir_s=in.dir_s, out.dir_e=in.dir_e;
    out.ext_s=in.ext_s, out.ext_e=in.ext_e;

    //! The look ahead angle to next primitive,
    //! to calculate acceptable end velocity.
    out.angle_end_deg=in.angle_end_deg;

    //! If arc's velmax is reduced by gforce impact value, this is maxvel.
    //! Otherwise the velmax is set to program velmax.
    out.velmax=in.velmax;
    out.vo=in.vo;
    out.ve=in.ve;

    return out;
}

sc_block sc_optimizer::convert_c_to_cpp(sc_block in){

    sc_block out;
    out.primitive_id=in.primitive_id;
    out.type=in.type;

    out.gcode_line_nr=in.gcode_line_nr;

    out.pnt_s=in.pnt_s, out.pnt_e=in.pnt_e, out.pnt_w=in.pnt_w;
    out.dir_s=in.dir_s, out.dir_e=in.dir_e;
    out.ext_s=in.ext_s, out.ext_e=in.ext_e;

    //! The look ahead angle to next primitive,
    //! to calculate acceptable end velocity.
    out.angle_end_deg=in.angle_end_deg;

    //! If arc's velmax is reduced by gforce impact value, this is maxvel.
    //! Otherwise the velmax is set to program velmax.
    out.velmax=in.velmax;
    out.vo=in.vo;
    out.ve=in.ve;

    return out;
}

sc_block* sc_optimizer::sc_optimize_all(sc_block *blockvec,
                                        int size,
                                        T acceleration,
                                        T delta_v,
                                        T gforce_max,
                                        T velocity_max){


    std::vector<sc_block> bvec;

    for(I i=0; i<size; i++){
        //! std::cout<<"a storing block i:"<<i<<" gcode line nr:"<<blockvec[i].gcode_line_nr<<std::endl;
        //! std::cout<<"a storing block i:"<<i<<" vo:"<<blockvec[i].vo<<std::endl;
        //! std::cout<<"a storing block i:"<<i<<" vm:"<<blockvec[i].velmax<<std::endl;
        //! std::cout<<"a storing block i:"<<i<<" ve:"<<blockvec[i].ve<<std::endl;

        bvec.push_back(blockvec[i]);
    }


    sc_set_a_dv_gforce_velmax(acceleration,
                              delta_v,
                              gforce_max,
                              velocity_max);
    bvec=sc_optimize_all(bvec);

    for(int i=0; i<size; i++){

        blockvec[i]=bvec.at(i);

        //! std::cout<<"b storing block i:"<<i<<" gcode line nr:"<<blockvec[i].gcode_line_nr<<std::endl;
        //! std::cout<<"b storing block i:"<<i<<" vo:"<<blockvec[i].vo<<std::endl;
        //! std::cout<<"b storing block i:"<<i<<" vm:"<<blockvec[i].velmax<<std::endl;
        //! std::cout<<"b storing block i:"<<i<<" ve:"<<blockvec[i].ve<<std::endl;
    }

    sc_print_blockvec(bvec);

    return blockvec;
}

//! Todo return pointer ....
extern "C" struct sc_block* sc_optimize_all_c(struct sc_block *blockvec_c,
                                              int size,
                                              T acceleration,
                                              T delta_v,
                                              T gforce_max,
                                              T velocity_max){


    return sc_optimizer().sc_optimize_all(blockvec_c,
                                          size,
                                          acceleration,
                                          delta_v,
                                          gforce_max,
                                          velocity_max);
}

std::vector<sc_block> sc_optimizer::sc_optimize_block_angles_ve(std::vector<sc_block> blockvec){

    //! Calculate motion block corners.
    blockvec=sc_get_blockangles(blockvec);

    //! Set end velocity, based on block corners, if no angle, ve is set to vm at this stage.
    blockvec=sc_get_corner_ve_blockangles(blockvec,vm);

    return blockvec;
}

std::vector<sc_block> sc_optimizer::sc_optimize_gforce_arcs(std::vector<sc_block> blockvec){
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

std::vector<sc_block> sc_optimizer::sc_optimize_G123_ve_forward(std::vector<sc_block> blockvec){

    T  vo=0, ve=0, vm=0, acs=0, ace=0, s=0, pvec_s=0, pvec_ve=0;

    for(UI i=0; i<blockvec.size(); i++){

        if(blockvec.at(i).type==sc_rapid){

        }

        if(blockvec.at(i).type==sc_linear || blockvec.at(i).type==sc_circle || blockvec.at(i).type==sc_G3){ //! End velocity=0 if motion is G0, rapid.

            acs=0, ace=0; //! To keep it simple, can be used later on to improve this library.

            vo=blockvec.at(i).vo;
            ve=blockvec.at(i).ve;
            vm=blockvec.at(i).velmax;
            s=blocklenght(blockvec.at(i));


            //! Check if vo to ve fits s.
            std::vector<sc_period> pvec;
            engine->process_curve(id_run, vo, ve, acs, ace, s, vm, pvec);

            pvec_s=engine->to_stot_pvec(pvec);
            pvec_ve=pvec.back().ve;

            if(pvec_s==s){
                //! Current given ve is ok, do nothing.
            } else {

                for(T j=ve; j>=0; j-=0.1*ve){ //! Sample down ve until pvec_s=s
                    std::vector<sc_period> pvec;
                    engine->process_curve(id_run,
                                          vo,
                                          j /*sampled ve*/,
                                          acs,
                                          ace,
                                          s,
                                          vm,
                                          pvec);

                    pvec_s=engine->to_stot_pvec(pvec);
                    pvec_ve=pvec.back().ve;

                    if(pvec_s==s || j==0){
                        blockvec.at(i).ve=j; //! Set lower ve.

                        if(i<blockvec.size()-1){    //! Set next vo to this ve.
                            blockvec.at(i+1).vo=j;
                        }
                        break;
                    }
                }
            }
        }
    }
    return blockvec;
}

std::vector<sc_block> sc_optimizer::sc_optimize_G123_ve_backward(std::vector<sc_block> blockvec){

    T  vo=0, ve=0, vm=0, acs=0, ace=0, s=0, pvec_s=0, pvec_ve=0;

    for(UI ii=blockvec.size(); ii>0; ii--){ //! Stupid counters when using unsigned int. Maybe use integer for this.
        UI i=ii-1;

        if(blockvec.at(i).type==sc_rapid){

        }

        if(blockvec.at(i).type==sc_linear || blockvec.at(i).type==sc_circle || blockvec.at(i).type==sc_G3){ //! End velocity=0 if motion is G0, rapid.

            acs=0, ace=0; //! To keep it simple, can be used later on to improve this library.

            //! Swap vo,ve.
            ve=blockvec.at(i).vo;
            vo=blockvec.at(i).ve;
            vm=blockvec.at(i).velmax;
            s=blocklenght(blockvec.at(i));

            //! Check if vo to ve fits s.
            std::vector<sc_period> pvec;
            engine->process_curve(id_run, vo, ve, acs, ace, s, vm, pvec);

            pvec_s=engine->to_stot_pvec(pvec);
            pvec_ve=pvec.back().ve;

            if(pvec_s==s){
                //! Current given ve is ok, do nothing.
            } else {

                for(T j=ve; j>=0; j-=0.1*ve){ //! Sample down ve until pvec_s=s

                    std::vector<sc_period> pvec;
                    engine->process_curve(id_run,
                                          vo,
                                          j /*sampled ve*/,
                                          acs,
                                          ace,
                                          s,
                                          vm,
                                          pvec);

                    pvec_s=engine->to_stot_pvec(pvec);
                    pvec_ve=pvec.back().ve;

                    if(pvec_s==s || j==0){
                        blockvec.at(i).vo=j;

                        if(i>1){
                            blockvec.at(i-1).ve=j;
                        }
                        break;
                    }
                }
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

                blockvec.at(i).velmax=maxvel_arc;
            } else {
                blockvec.at(i).velmax=velmax;
            }
        } else { //! For a line set velmax as usual.
            blockvec.at(i).velmax=velmax;
        }
    }
    return blockvec;
}

std::vector<sc_block> sc_optimizer::sc_get_corner_ve_blockangles(std::vector<sc_block> blockvec, T velmax){

    for(UI i=0; i<blockvec.size(); i++){

        if(blockvec.at(i).angle_end_deg<=90){ //! Stop required.
            blockvec.at(i).ve=0;
            //! Set next block vo to zero.
            if(i<blockvec.size()-1){
                blockvec.at(i+1).vo=0;
            }
        }

        if(blockvec.at(i).angle_end_deg>90){ //! Percentage ve up to 180 degrees. //! 180 degrees = colinear.
            T angle_deg=blockvec.at(i).angle_end_deg;
            T factor=(angle_deg-90)/90; //! 0-1, 1=straight on. 0=90 degrees.
            T ve=velmax*factor;

            blockvec.at(i).ve=ve;
            //! Set next block vo to this ve.
            if(i<blockvec.size()-1){
                blockvec.at(i+1).vo=ve;
            }
        }
    }
    return blockvec;
}

std::vector<sc_block> sc_optimizer::sc_get_blockangles(std::vector<sc_block> blockvec){

    if(blockvec.size()>0){ //! Vector safe.
        for(UI i=0; i<blockvec.size()-1; i++){
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






















