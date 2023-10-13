#include "sc_struct.h"
#include "sc_lines.h"
#include "sc_arcs.h"

T blocklenght(struct sc_block b){

    if(b.primitive_id==sc_line){
        return sc_lines().sc_line_lenght(b.pnt_s,b.pnt_e);
    }
    if(b.primitive_id==sc_arc){
        return sc_arcs().sc_arc_lenght(b.pnt_s,b.pnt_w,b.pnt_e);
    }
    return 0;
}
