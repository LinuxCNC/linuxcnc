#include "sc_interpolate.h"

//! Interpolates traject progress 0-1.
void interpolate_traject(struct sc_block *blockvec, int size, T traject_progress, T traject_lenght, T *curve_progress, int *curve_nr){

    T ltot=traject_lenght;
    T l=0;
    for(int i=0; i<size; i++){
        T blocklenght=blockvec[i].path_lenght;
        if(traject_progress>=l/ltot && traject_progress<(l+blocklenght)/ltot){

            T low_pct=l/ltot;                                   //10%
            T high_pct=(l+blocklenght)/ltot;                    //25%
            T range=high_pct-low_pct;                           //25-10=15%
            T offset_low=traject_progress-low_pct;              //12-10=2%
            *curve_progress=offset_low/range;
            *curve_nr=i;
            return;
        }
        l+=blocklenght;
    }
}



















