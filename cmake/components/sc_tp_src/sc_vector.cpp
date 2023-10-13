#include "sc_vector.h"

sc_vector::sc_vector()
{

}

V sc_vector::popfront(){
    pvec.erase(pvec.begin());
}

T sc_vector::traject_lenght(){
    T l=0;
    for(UI i=0; i<pvec.size(); i++){
        l+=pvec.at(i).path_lenght;
    }
    return l;
}

V sc_vector::optimize_gcode(){

    optimizedVec.clear();

    //! Fuse colinair curves info one motion.
    T l=0, ltot=0;
    for(UI i=0; i<pvec.size(); i++){

        // optimizedVec.push_back(pvec.at(i).path_lenght);
        l=pvec.at(i).path_lenght;
        if(pvec.at(i).type!=sc_type::sc_rapid ){
            ltot+=l;
            //std::cout<<"type:"<<pvec.at(i).type<<std::endl;
            if(i==pvec.size()-1 || (i<pvec.size()-1 && pvec.at(i+1).type==sc_type::sc_rapid) ){
                optimizedVec.push_back(ltot);
                   //std::cout<<"ltot:"<<ltot<<std::endl;
                ltot=0;
            }
        }
        if(pvec.at(i).type==sc_type::sc_rapid ){
            optimizedVec.push_back(l);
           // std::cout<<"type:"<<pvec.at(i).type<<std::endl;
           // std::cout<<"l:"<<l<<std::endl;
        }
    }

    T l0=0;
      for(UI i=0; i< optimizedVec.size(); i++){
          l0+=optimizedVec.at(i);
      }
      //std::cout<<"total optimized pathlenght:"<<l0<<std::endl;
        //std::cout<<"optimized vec size:"<<optimizedVec.size()<<std::endl;

   // std::cout<<"total pvec pathlenght:"<<traject_lenght()<<std::endl;
}

//! Interpolates traject progress 0-1.
V sc_vector::interpolate_traject(T traject_progress, T traject_lenght, T &curve_progress, I &curve_nr){

    T ltot=traject_lenght;
    T l=0;
    for(UI i=0; i<pvec.size(); i++){
        T blocklenght=pvec[i].path_lenght;
        if(traject_progress>=l/ltot && traject_progress<(l+blocklenght)/ltot){

            T low_pct=l/ltot;                                   //10%
            T high_pct=(l+blocklenght)/ltot;                    //25%
            T range=high_pct-low_pct;                           //25-10=15%
            T offset_low=traject_progress-low_pct;              //12-10=2%
            curve_progress=offset_low/range;
            curve_nr=i;
            return;
        }
        l+=blocklenght;
    }
}

extern "C" V vector_interpolate_traject(sc_vector *ptr, T traject_progress, T traject_lenght, T *curve_progress, I *curve_nr){

    T curve_progress_=0;
    I curve_nr_=0;
    ptr->interpolate_traject(traject_progress, traject_lenght, curve_progress_, curve_nr_);

    *curve_progress=curve_progress_;
    *curve_nr=curve_nr_;
}



extern "C" sc_vector* vector_init_ptr(){
    return new sc_vector();
}

extern "C" V vector_pushback(sc_vector *ptr, struct sc_block b){
    ptr->pvec.push_back(b);
}

extern "C" V vector_insert(sc_vector *ptr, struct sc_block b, int index){
    ptr->pvec.insert(ptr->pvec.begin()+index,b);
}

extern "C" V vector_popfront(sc_vector *ptr){
    ptr->popfront();
}

extern "C" V vector_popback(sc_vector *ptr){
    ptr->pvec.pop_back();
}

extern "C" V vector_clear(sc_vector *ptr){
    ptr->pvec.clear();
}

extern "C" int vector_size(sc_vector *ptr){
    return ptr->pvec.size();
}

extern "C" struct sc_block vector_at(sc_vector *ptr, int index){
    return ptr->pvec.at(index);
}

extern "C" struct sc_block vector_front(sc_vector *ptr){
    return ptr->pvec.front();
}

extern "C" struct sc_block vector_back(sc_vector *ptr){
    return ptr->pvec.back();
}

extern "C" T vector_traject_lenght(sc_vector *ptr){
    return ptr->traject_lenght();
}


extern "C" int vector_optimized_size(sc_vector *ptr){
    return ptr->optimizedVec.size();
}

extern "C" T vector_optimized_at(sc_vector *ptr, int index){
    return ptr->optimizedVec.at(index);
}

extern "C" V vector_optimize_gcode(sc_vector *ptr){
    ptr->optimize_gcode();
}




