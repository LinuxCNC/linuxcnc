#include "sc_interpolate.h"

sc_interpolate::sc_interpolate()
{

}

V sc_interpolate::interpolate_blockvec(std::vector<sc_block> blockvec,
                                              T traject_progress,
                                              sc_pnt &pnt,
                                              sc_dir &dir,
                                              sc_ext &ext,
                                       T &curve_progress){

    T ltot=0;
    for(uint i=0; i<blockvec.size(); i++){
        ltot+=blocklenght(blockvec.at(i));
    }

    T l=0;
    for(uint i=0; i<blockvec.size(); i++){

        if(traject_progress>=l/ltot && traject_progress<(l+blocklenght(blockvec.at(i)))/ltot){

            T low_pct=l/ltot;                                   //10%
            T high_pct=(l+blocklenght(blockvec.at(i)))/ltot;   //25%
            T range=high_pct-low_pct;                           //25-10=15%
            T offset_low=traject_progress-low_pct;              //12-10=2%
            curve_progress=offset_low/range;

            if(blockvec.at(i).primitive_id==sc_line){
                sc_lines().sc_interpolate_lin(blockvec.at(i).pnt_s,blockvec.at(i).pnt_e,curve_progress,pnt);
            }
            if(blockvec.at(i).primitive_id==sc_arc){
                sc_arcs().sc_interpolate_arc(blockvec.at(i).pnt_s,blockvec.at(i).pnt_w,blockvec.at(i).pnt_e,curve_progress,pnt);
            }

            sc_lines().sc_interpolate_dir(blockvec.at(i).dir_s,blockvec.at(i).dir_e,curve_progress,dir);
            sc_lines().sc_interpolate_ext(blockvec.at(i).ext_s,blockvec.at(i).ext_e,curve_progress,ext);
        }

        l+=blocklenght(blockvec.at(i));
    }
}





















