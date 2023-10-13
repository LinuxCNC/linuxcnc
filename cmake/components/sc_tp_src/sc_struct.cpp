#include "sc_struct.h"
#include "sc_lines.h"
#include "sc_arcs.h"

T netto_difference_of_2_values(T a, T b){

    T diff=0;
    if(a<0 && b<0){
        a=fabs(a);
        b=fabs(b);
        diff=fabs(a-b);
    }
    if(a>=0 && b>=0){
        diff=fabs(a-b);
    }
    if(a<=0 && b>=0){;
        diff=fabs(a)+b;
    }
    if(a>=0 && b<=0){
        diff=a+fabs(b);
    }
    return diff;
}

T blocklenght(struct sc_block b){

    if(b.primitive_id==sc_line){
        return sc_lines().sc_line_lenght(b.pnt_s,b.pnt_e);
    }
    if(b.primitive_id==sc_arc){
        return sc_arcs().sc_arc_lenght(b.pnt_s,b.pnt_w,b.pnt_e);
    }
    return 0;
}

extern "C" T blocklenght_c(struct sc_block b){
    return blocklenght(b);
}
