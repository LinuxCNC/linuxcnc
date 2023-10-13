#ifndef SC_BLOCK_H
#define SC_BLOCK_H

//! Author  : SKynet Cyberdyne
//! Licence : MIT
//! Date    : 2023

#include "sc_struct.h"
#include "sc_lines.h"
#include "sc_arcs.h"

//struct sc_block {

//    sc_primitive_id primitive_id=sc_line;
//    sc_type type=sc_rapid;

//    int gcode_line_nr;

//    sc_pnt pnt_s; //! Start.
//    sc_pnt pnt_e; //! End.
//    sc_pnt pnt_w; //! Way.
//    sc_dir dir_s, dir_e;
//    sc_ext ext_s, ext_e;

//    //! The look ahead angle to next primitive,
//    //! to calculate acceptable end velocity.
//    T angle_end_deg=0;

//    //! If arc's velmax is reduced by gforce impact value, this is maxvel.
//    //! Otherwise the velmax is set to program velmax.
//    T velmax=0;
//    T vo=0;
//    T ve=0;

//    V set_pnt(sc_pnt start, sc_pnt end){
//        pnt_s=start;
//        pnt_e=end;
//    }

//    V set_pnt(sc_pnt start, sc_pnt way, sc_pnt end){
//        pnt_s=start;
//        pnt_w=way;
//        pnt_e=end;
//    }

//    V set_dir(sc_dir start, sc_dir end){
//        dir_s=start;
//        dir_e=end;
//    }

//    V set_ext(sc_ext start, sc_ext end){
//        ext_s=start;
//        ext_e=end;
//    }

//    T blocklenght(){

//        if(primitive_id==sc_line){
//            return sc_lines().sc_line_lenght(pnt_s,pnt_e);
//        }
//        if(primitive_id==sc_arc){
//            return sc_arcs().sc_arc_lenght(pnt_s,pnt_w,pnt_e);
//        }
//        return 0;
//    }
//};


#endif










