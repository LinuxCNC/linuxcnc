#include "sc_lines.h"
#include "sc_engine.h"

sc_lines::sc_lines()
{

}

V sc_lines::sc_interpolate_lin(sc_pnt p0, sc_pnt p1, T progress, sc_pnt &pi){
    sc_interpolate_lenght(p0.x,p1.x,progress,pi.x);
    sc_interpolate_lenght(p0.y,p1.y,progress,pi.y);
    sc_interpolate_lenght(p0.z,p1.z,progress,pi.z);
}

V sc_lines::sc_interpolate_dir(sc_dir d0, sc_dir d1, T progress, sc_dir &di){
    sc_interpolate_lenght(d0.a,d1.a,progress,di.a);
    sc_interpolate_lenght(d0.b,d1.b,progress,di.b);
    sc_interpolate_lenght(d0.c,d1.c,progress,di.c);
}

V sc_lines::sc_interpolate_ext(sc_ext e0, sc_ext e1, T progress, sc_ext &ei){
    sc_interpolate_lenght(e0.u,e1.u,progress,ei.u);
    sc_interpolate_lenght(e0.v,e1.v,progress,ei.v);
    sc_interpolate_lenght(e0.w,e1.w,progress,ei.w);
}

V sc_lines::sc_interpolate_lenght(T start, T end, T progress, T &li){
    if(start<end){
        li=start+(progress*sc_engine().netto_difference_of_2_values(start,end));
        return;
    }
    if(start>end){
        li=start-(progress*sc_engine().netto_difference_of_2_values(start,end));
        return;
    }
    if(start==end){
        li=end;
    }
}

T sc_lines::sc_line_lenght(sc_pnt p0, sc_pnt p1){
    return sqrt(pow(p1.x-p0.x,2)+pow(p1.y-p0.y,2)+pow(p1.z-p0.z,2));
}

















