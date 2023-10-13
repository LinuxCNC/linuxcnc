#include "sc_engine.h"

inline I sc_engine::t1(T vo, T acs, T ace, sc_period &p){

    acs=std::abs(acs);
    ace=std::abs(ace);
    acs=std::min(as,acs);
    ace=std::min(as,ace);

    T ts=acs/jm;
    T vf=vo-(jm*(ts*ts)/2);
    T so=vf*ts+jm*(ts*ts*ts)/6;

    T te=ace/jm;
    T ve=vf+jm*(te*te)/2;
    T se=vf*te+jm*(te*te*te)/6;

    T ncs=se-so;
    T nct=te-ts;

    p={sc_period_id::id_t1,vo,ve,acs,ace,ncs,nct};

    return 1;
}

I sc_engine::t1_pid(T vo, T acs, T interval, sc_period &p){

    acs=std::abs(acs);
    acs=std::min(as,acs);

    T ts=acs/jm;
    T vf=vo-(jm*(ts*ts)/2);
    T so=vf*ts+jm*(ts*ts*ts)/6;

    T te=ts+interval;
    te=std::min(te,as/jm); //! Limits te to time at as.
    T ve=vf+jm*(te*te)/2;
    T se=vf*te+jm*(te*te*te)/6;

    T ace=jm*te;

    T ncs=se-so;
    T nct=te-ts;

    p={sc_period_id::id_t1,vo,ve,acs,ace,ncs,nct};

    return 1;
}

inline I sc_engine::t1_ve(T vo, T ve, T acs, sc_period &p){

    acs=std::abs(acs);
    acs=std::min(acs,as);

    T ts=acs/jm;
    T vf=vo-(jm*(ts*ts)/2);

    T so=vf*ts+jm*(ts*ts*ts)/6;

    T te=sqrt(-2*vf+2*ve)/sqrt(jm);
    te=std::min(0.5*ct,te);
    T ve_=vf+jm*(te*te)/2;
    T ace=jm*te;

    T se=vf*te+jm*(te*te*te)/6;

    T ncs=se-so;
    T nct=te-ts;

    p={sc_period_id::id_t1,vo,ve_,acs,ace,ncs,nct};

    return 1;
}

inline I sc_engine::t1_i(sc_period p, T ti, T &vi, T &si, T &ai){

    T ts=p.acs/jm;
    T vf=p.vo-(jm*(ts*ts)/2);

    ti+=ts;
    vi=vf+jm*(ti*ti)/2;

    T so=vf*ts+jm*(ts*ts*ts)/6;
    T se=vf*ti+jm*(ti*ti*ti)/6;
    si=se-so;

    ai=jm*ti;
    return 1;
}

inline I sc_engine::t2(T vo, T ve, T a, sc_period &p){

    T ncs=((ve*ve) - (vo*vo))/(2*a) ;
    T nct=(ve-vo)/a;

    p={sc_period_id::id_t2,vo,ve,a,a,ncs,nct};

    return 1;
}

I sc_engine::t2_pid(T vo, T a, T interval, sc_period &p){

    T t=interval;
    T ve=vo + a*t;
    T s=vo*t + 0.5*a*(t*t);

    T ncs=s;
    T nct=interval;

    p={sc_period_id::id_t2,vo,ve,a,a,ncs,nct};

    return 1;
}

inline I sc_engine::t2_i(sc_period p, T ti, T &vi, T &si, T &ai){

    vi=p.vo + p.acs*ti;
    si=p.vo*ti + 0.5*p.acs*(ti*ti);
    ai=p.acs;

    return 1;
}

inline I sc_engine::t3(T vo, T acs, T ace, sc_period &p){

    acs=std::abs(acs);
    ace=std::abs(ace);
    acs=std::min(as,acs);
    ace=std::min(as,ace);

    T ts=(as-acs)/jm;
    T vf = -as*ts+((jm*(ts*ts))/2)+vo;
    T so=vf*ts + as*(ts*ts)/2 - jm*(ts*ts*ts)/6;

    T te=(as-ace)/jm;
    T ve=vf + as*te - jm*(te*te)/2;
    T se=vf*te + as*(te*te)/2 - jm*(te*te*te)/6;

    T ncs=se-so;
    T nct=te-ts;

    p={sc_period_id::id_t3,vo,ve,acs,ace,ncs,nct};

    return 2;
}

I sc_engine::t3_pid(T vo, T acs, T interval, sc_period &p){

    acs=std::abs(acs);
    acs=std::min(as,acs);

    T ts=(as-acs)/jm;
    T vf = -as*ts+((jm*(ts*ts))/2)+vo;
    T so=vf*ts + as*(ts*ts)/2 - jm*(ts*ts*ts)/6;

    T te=ts+interval;
    te=std::min(0.5*ct,te);
    T ace=as-(te*jm);

    T ve=vf + as*te - jm*(te*te)/2;
    T se=vf*te + as*(te*te)/2 - jm*(te*te*te)/6;

    T ncs=se-so;
    T nct=te-ts;

    p={sc_period_id::id_t3,vo,ve,acs,ace,ncs,nct};

    return 2;
}

inline I sc_engine::t3_i(sc_period p, T ti, T &vi, T &si, T &ai){

    T ts=(as-p.acs)/jm;
    T t=ts;
    T vf = -as*t+((jm*(t*t))/2)+p.vo;
    T so=vf*t + as*(t*t)/2 - jm*(t*t*t)/6;

    t=0;
    T sf=vf*t + as*(t*t)/2 - jm*(t*t*t)/6;

    t=ti+ts;
    vi=vf + as*t - jm*(t*t)/2;
    si=vf*t + as*(t*t)/2 - jm*(t*t*t)/6;
    si-=so-sf;
    ai=as-(jm*t);

    return 1;
}

inline I sc_engine::t4(T vo, T s, sc_period &p){

    T nct=s/vo;
    T ncs=s;

    p={sc_period_id::id_t4,vo,vo,0,0,ncs,nct};

    return 1;
}

I sc_engine::t4_pid(T vo, T a, T interval, sc_period &p){

    a=0.0; //! Used as dummy input. To keep pid function input the same.

    T nct=interval;
    T ncs=vo*interval;

    p={sc_period_id::id_t4,vo,vo,a,a,ncs,nct};

    return 1;
}

inline I sc_engine::t4_i(sc_period p, T ti, T &vi, T &si, T &ai){

    vi=p.vo;
    si=p.vo*ti;
    ai=0;

    return 1;
}

inline I sc_engine::t5(T vo, T acs, T ace, sc_period &p){

    acs=std::abs(acs);
    ace=std::abs(ace);
    acs=std::min(as,acs);
    ace=std::min(as,ace);
    acs=std::min(acs,ace);

    T ts=acs/jm;
    T vf=((jm*(ts*ts))/2)+vo;
    T so=vf*ts-jm*(ts*ts*ts)/6;

    T te=ace/jm;
    T ve=vf-jm*(te*te)/2;
    T se=vf*te-jm*(te*te*te)/6;

    T ncs=se-so;
    T nct=te-ts;

    acs=-std::abs(acs);
    ace=-std::abs(ace);

    p={sc_period_id::id_t5,vo,ve,acs,ace,ncs,nct};

    return 1;
}

I sc_engine::t5_pid(T vo, T acs, T interval, sc_period &p){

    acs=std::abs(acs);
    acs=std::min(acs,as);

    T ts=acs/jm;
    T vf=((jm*(ts*ts))/2)+vo;
    T so=vf*ts-jm*(ts*ts*ts)/6;

    T te=ts+interval;
    te=std::min(0.5*ct,te);
    T ace=jm*te;
    T ve=vf-jm*(te*te)/2;
    T se=vf*te-jm*(te*te*te)/6;

    T ncs=se-so;
    T nct=te-ts;

    acs=-std::abs(acs);
    ace=-std::abs(ace);

    p={sc_period_id::id_t5,vo,ve,acs,ace,ncs,nct};

    return 1;
}

inline I sc_engine::t5_ve(T vo, T ve, T acs, sc_period &p){

    acs=std::abs(acs);
    acs=std::min(acs,as);

    T ts=acs/jm;
    T vf=((jm*(ts*ts))/2)+vo;
    T so=vf*ts-jm*(ts*ts*ts)/6;

    T te=(sqrt(2)*sqrt(vf-ve))/sqrt(jm);
    te=std::min(0.5*ct,te);
    T ace_=-std::abs(jm*te);
    T ve_=vf-jm*(te*te)/2;
    T se=vf*te-jm*(te*te*te)/6;

    T ncs=se-so;
    T nct=te-ts;

    p={sc_period_id::id_t5,vo,ve_,acs,ace_,ncs,nct};

    return 1;
}

inline I sc_engine::t5_i(sc_period p, T ti, T &vi, T &si, T &ai){

    p.acs=std::abs(p.acs);

    T ts=p.acs/jm;
    T t=ts;
    T vf=((jm*(t*t))/2)+p.vo;
    T so=vf*t-jm*(t*t*t)/6;

    t=0;
    T sf=vf*t-jm*(t*t*t)/6;

    t=ti+ts;
    vi=vf-jm*(t*t)/2;
    ai=-std::abs(jm*t);
    si=vf*t-jm*(t*t*t)/6;
    si-=so-sf;

    return 1;
}

inline I sc_engine::t6(T vo, T ve, T a, sc_period &p){

    a=std::abs(a);
    T ncs=((vo*vo) - (ve*ve))/(2*a);
    T nct=(vo-ve)/a;

    p={sc_period_id::id_t6,vo,ve,a,a,ncs,nct};

    return 1;
}

I sc_engine::t6_pid(T vo, T a, T interval, sc_period &p){

    T t=interval;
    a=std::abs(a);
    T ve=vo - a*t;
    T s=vo*t - 0.5*a*(t*t);

    T ncs=s;
    T nct=interval;

    a=-std::abs(a);

    p={sc_period_id::id_t6,vo,ve,a,a,ncs,nct};

    return 1;
}

inline I sc_engine::t6_i(sc_period p, T ti, T &vi, T &si, T &ai){

    T t=0;
    p.acs=std::abs(p.acs);

    t=ti;
    vi=p.vo - p.acs*t;
    si=p.vo*t - 0.5*p.acs*(t*t);
    ai=-std::abs(p.acs);

    return 1;
}

inline I sc_engine::t7(T vo, T acs, T ace, sc_period &p){

    acs=std::abs(acs);
    ace=std::abs(ace);
    acs=std::min(as,acs);
    ace=std::min(as,ace);

    T ts=(as-acs)/jm;
    T vf=as*ts - ((jm*(ts*ts))/2) + vo;
    T so=vf*ts - as*(ts*ts)/2 + jm*(ts*ts*ts)/6;

    T te=(as-ace)/jm;
    T se=vf*te - as*(te*te)/2 + jm*(te*te*te)/6;
    T ve=vf - as*te + jm*(te*te)/2;

    T ncs=se-so;
    T nct=te-ts;

    acs=-std::abs(acs);
    ace=-std::abs(ace);

    p={sc_period_id::id_t7,vo,ve,acs,ace,ncs,nct};

    return 1;
}

I sc_engine::t7_pid(T vo, T acs, T interval, sc_period &p){

    acs=std::abs(acs);
    acs=std::min(as,acs);

    T ts=(as-acs)/jm;
    T vf=as*ts - ((jm*(ts*ts))/2) + vo;
    T so=vf*ts - as*(ts*ts)/2 + jm*(ts*ts*ts)/6;

    T te=ts+interval;
    te=std::min(0.5*ct,te);
    T ace=as-(te*jm);

    T se=vf*te - as*(te*te)/2 + jm*(te*te*te)/6;
    T ve=vf - as*te + jm*(te*te)/2;

    T ncs=se-so;
    T nct=te-ts;

    acs=-std::abs(acs);
    ace=-std::abs(ace);

    p={sc_period_id::id_t7,vo,ve,acs,ace,ncs,nct};

    return 1;
}

inline I sc_engine::t7_i(sc_period p, T ti, T &vi, T &si, T &ai){

    p.acs=std::abs(p.acs);

    T ts=(as-p.acs)/jm;
    T t=ts;
    T vf=as*t - ((jm*(t*t))/2) + p.vo;
    T so=vf*t - as*(t*t)/2 + jm*(t*t*t)/6;

    t=0;
    T sf=vf*t - as*(t*t)/2 + jm*(t*t*t)/6;

    t=ti+ts;
    vi=vf - as*t + jm*(t*t)/2;
    si=vf*t - as*(t*t)/2 + jm*(t*t*t)/6;
    si-=so-sf;
    ai=-std::abs(as-jm*t);

    return 1;
}

V sc_engine::t1_t2_t3(T vo, T ve, T &s){

    sc_period p;
    T delta_v=ve-vo;
    s=0;

    if(delta_v==dv){ //! Ok, exact.
        t1(vo,0,as,p);
        s+=p.ncs;
        t3(p.ve,p.ace,0,p);
        s+=p.ncs;
    }
    if(delta_v>dv){ //! Need t2.
        T v2=delta_v-dv;
        t1(vo,0,as,p);
        s+=p.ncs;
        t2(p.ve,p.ve+v2,p.ace,p);
        s+=p.ncs;
        t3(p.ve,p.ace,0,p);
        s+=p.ncs;
    }
    if(delta_v<dv){ //! Need t1 ace.
        t1_ve(vo,to_vh_acc(vo,ve),0,p);
        s+=p.ncs;
        t3(p.ve,p.ace,0,p);
        s+=p.ncs;
    }
}

V sc_engine::t5_t6_t7(T vo, T ve, T &s){

    sc_period p;
    T delta_v=std::abs(ve-vo);
    s=0;

    if(delta_v==dv){ //! Ok, exact.
        t5(vo,0,as,p);
        s+=p.ncs;
        t7(p.ve,p.ace,0,p);
        s+=p.ncs;
    }
    if(delta_v>dv){ //! Need t6.
        T v6=delta_v-dv;
        t5(vo,0,as,p);
        s+=p.ncs;
        t6(p.ve,p.ve-v6,p.ace,p);
        s+=p.ncs;
        t7(p.ve,p.ace,0,p);
        s+=p.ncs;
    }
    if(delta_v<dv){ //! Need t5 ace.
        t5_ve(vo,to_vh_dcc(vo,ve),0,p);
        s+=p.ncs;
        t7(p.ve,p.ace,0,p);
        s+=p.ncs;
    }
}

inline I sc_engine::t1_t2_t3(sc_period p, std::vector<sc_period> &pvec){

    sc_period p1,p2,p3;
    T delta_v=0;

    t1(p.vo,p.acs,as,p1);
    t3(p1.ve,p1.ace,p.ace,p3);

    if(p3.ve<=p.ve){
        t1(p.vo,p.acs,as,p1);
        delta_v=p.ve-p3.ve;
        t2(p1.ve,p1.ve+delta_v,p1.ace,p2);
        t3(p2.ve,p2.ace,p.ace,p3);
        pvec={p1,p2,p3};
        return 1;
    }

    if(p3.ve>p.ve){ //! Sample as down.
        for(T i=as; i>0; i-=0.1*as){ //! Sampling 10%.
            t1(p.vo,p.acs,i,p1);
            t3(p1.ve,p1.ace,p.ace,p3);

            if(p3.ve<p.ve){

                delta_v=p.ve-p3.ve;

                t1(p.vo,p.acs,i,p1);
                t2(p1.ve,p1.ve+delta_v,p1.ace,p2);
                t3(p2.ve,p2.ace,p.ace,p3);

                pvec={p1,p2,p3};
                return 1;
            }
        }
    }
    return 0;
}

inline I sc_engine::t7_t1_t2_t3_t5(sc_period p, std::vector<sc_period> &pvec){

    sc_period p7,p1,p2,p3,p5;
    T delta_v=0;

    if(p.acs<0){
        t7(p.vo,p.acs,0,p7);
        p.vo=p7.ve;
        p.acs=0;
    }

    t1(p.vo,p.acs,as,p1);

    if(p.ace<0){
        t3(p1.ve,p1.ace,0,p3);
        t5(p3.ve,0,p.ace,p5);
        p3.ve=p5.ve;
    } else {
        t3(p1.ve,p1.ace,p.ace,p3);
    }

    if(p3.ve<=p.ve){ //! Need t2.
        t1(p.vo,p.acs,as,p1);
        delta_v=p.ve-p3.ve;
        t2(p1.ve,p1.ve+delta_v,p1.ace,p2);

        if(p.ace<0){
            t3(p2.ve,p2.ace,0,p3);
            t5(p3.ve,0,p.ace,p5);
            pvec={p7,p1,p2,p3,p5};
        } else {
            t3(p2.ve,p2.ace,p.ace,p3);
            pvec={p7,p1,p2,p3};
        }
        return 1;
    }

    if(p3.ve>p.ve){ //! Sample as down.
        for(T i=as; i>0; i-=0.1*as){ //! Sampling 10%.
            t1(p.vo,p.acs,i,p1);
            t3(p1.ve,p1.ace,p.ace,p3);

            if(p.ace<0){
                t3(p1.ve,p1.ace,0,p3);
                t5(p3.ve,0,p.ace,p5);
                p3.ve=p5.ve;
            } else {
                t3(p1.ve,p1.ace,p.ace,p3);
            }

            if(p3.ve<p.ve){

                delta_v=p.ve-p3.ve;

                t1(p.vo,p.acs,i,p1);
                t2(p1.ve,p1.ve+delta_v,p1.ace,p2);

                if(p.ace<0){
                    t3(p2.ve,p2.ace,0,p3);
                    t5(p3.ve,0,p.ace,p5);
                    pvec={p7,p1,p2,p3,p5};
                } else {
                    t3(p2.ve,p2.ace,p.ace,p3);
                    pvec={p7,p1,p2,p3};
                }
                return 1;
            }
        }
    }
    return 0;
}

inline I sc_engine::t5_t6_t7(sc_period p, std::vector<sc_period> &pvec){

    sc_period p5,p6,p7;
    T delta_v=0;

    t5(p.vo,p.acs,as,p5);
    t7(p5.ve,p5.ace,p.ace,p7);

    if(p7.ve>=p.ve){

        delta_v=p7.ve-p.ve;

        t6(p5.ve,p5.ve-delta_v,p5.ace,p6);
        t7(p6.ve,p6.ace,p.ace,p7);
        //! std::cout<<"normal, ve:"<<p7.ve<<std::endl;
    }

    if(p7.ve<p.ve){ //! Sample as down.
        for(T i=as; i>0; i-=(0.1*as)){ //! Sampling 10%.
            t5(p.vo,p.acs,i,p5);
            t7(p5.ve,p5.ace,p.ace,p7);

            if(p7.ve>p.ve){

                delta_v=p7.ve-p.ve;

                t5(p.vo,p.acs,i,p5);
                t6(p5.ve,p5.ve-delta_v,p5.ace,p6);
                t7(p6.ve,p6.ace,p.ace,p7);
                break;
            }
        }
    }
    pvec={p5,p6,p7};
    return 1;
}

inline I sc_engine::t4_acs(sc_period p, std::vector<sc_period> &pvec){

    sc_period p1,p3,p4,p5,p7;
    if(p.acs>0){
        t3(p.vo,p.acs,0,p3);

        t5_ve(p3.ve,to_vh_dcc(p3.ve,p.ve),0,p5);
        t7(p5.ve,p5.ace,0,p7);

        T s=p.ncs-(p3.ncs+p5.ncs+p7.ncs);
        if(s>0){ //! Need steady period.
            t4(p7.ve,s,p4);
            pvec={p3,p5,p7,p4};
            return 1;
        } else {
            pvec={p3,p5,p7};
            return 1;
        }
    }
    if(p.acs<0){
        t7(p.vo,p.acs,0,p7);

        t1_ve(p7.ve,to_vh_acc(p7.ve,p.ve),0,p1);
        t3(p1.ve,p1.ace,0,p3);

        T s=p.ncs-(p7.ncs+p1.ncs+p3.ncs);
        if(s>0){ //! Need steady period.
            t4(p3.ve,s,p4);
            pvec={p7,p1,p3,p4};
            return 1;
        } else {
            pvec={p7,p1,p3};
            return 1;
        }
    }
    if(p.acs==0){
        t4(p.vo,p.ncs,p4);
        pvec={p4};
        return 1;
    }
    return 0;
}

inline I sc_engine::t4_ace(sc_period p, std::vector<sc_period> &pvec){

    sc_period p1,p3,p4,p5,p7;
    if(p.ace>0){
        T a_number=100;
        t5(a_number,0,p.ace,p5);
        T dv=a_number-p5.ve;

        t1_ve(p.vo,to_vh_acc(p.vo,p.vo+dv),0,p1);
        t3(p1.ve,p1.ace,0,p3);
        t5(p3.ve,0,p.ace,p5);

        T s=p.ncs-(p1.ncs+p3.ncs+p5.ncs);
        if(s>0){ //! Need steady period.
            t4(p.vo,s,p4);
            pvec={p4,p1,p3,p5};
            return 1;
        } else { //! Minimal curve.
            pvec={p1,p3,p5};
            return 1;
        }
    }
    if(p.ace<0){
        T a_number=100;
        t1(a_number,0,p.ace,p1);
        T dv=p1.ve-a_number;

        t5_ve(p.vo,to_vh_dcc(p.vo,p.vo-dv),0,p5);
        t7(p5.ve,p5.ace,0,p7);
        t1(p7.ve,0,p.ace,p1);

        T s=p.ncs-(p5.ncs+p7.ncs+p1.ncs);
        if(s>0){ //! Need steady period.
            t4(p.vo,s,p4);
            pvec={p4,p5,p7,p1};
            return 1;
        } else { //! Minimal curve.
            pvec={p5,p7,p1};
            return 1;
        }
    }
    if(p.ace==0){
        t4(p.vo,p.ncs,p4);
        pvec={p4};
        return 1;
    }
    return 0;
}

inline I sc_engine::t3_t5_t6_t7_t1(sc_period p, std::vector<sc_period> &pvec){

    sc_period p3,p5,p6,p7,p1;
    T delta_v=0;

    if(p.acs>0){
        t3(p.vo,p.acs,0,p3);
        p.vo=p3.ve;
        p.acs=0;
    }

    t5(p.vo,p.acs,as,p5);

    if(p.ace>0){
        t7(p5.ve,p5.ace,0,p7);
        t1(p7.ve,0,p.ace,p1);
        p7.ve=p1.ve;
    } else {
        t7(p5.ve,p5.ace,p.ace,p7);
    }

    if(p7.ve>=p.ve){ //! Need t2.

        delta_v=p7.ve-p.ve;

        t6(p5.ve,p5.ve-delta_v,p5.ace,p6);

        if(p.ace>0){
            t7(p6.ve,p6.ace,0,p7);
            t1(p7.ve,0,p.ace,p1);
            pvec={p3,p5,p6,p7,p1};
        } else {
            t7(p6.ve,p6.ace,p.ace,p7);
            pvec={p3,p5,p6,p7};
        }
        return 1;
    }

    if(p7.ve<p.ve){ //! Sample as down.
        for(T i=as; i>0; i-=(0.1*as)){ //! Sampling 10%.
            t5(p.vo,p.acs,i,p5);

            if(p.ace>0){
                t7(p5.ve,p5.ace,0,p7);
                t1(p7.ve,0,p.ace,p1);
                p7.ve=p1.ve;
            } else {
                t7(p5.ve,p5.ace,p.ace,p7);
            }

            if(p7.ve>p.ve){

                delta_v=p7.ve-p.ve;

                t5(p.vo,p.acs,i,p5);
                t6(p5.ve,p5.ve-delta_v,p5.ace,p6);

                if(p.ace>0){
                    t7(p6.ve,p6.ace,0,p7);
                    t1(p7.ve,0,p.ace,p1);
                    pvec={p3,p5,p6,p7,p1};
                } else {
                    t7(p6.ve,p6.ace,p.ace,p7);
                    pvec={p3,p5,p6,p7};
                }
                return 1;
            }
        }
    }
    return 0;
}

I sc_engine::t3_t5_t6_t7_pid(sc_period p, std::vector<sc_period> &pvec){

    sc_period p3,p5,p6,p7;
    T delta_v=0;

    if(p.acs>0){
        t3(p.vo,p.acs,0,p3);
        p.vo=p3.ve;
        p.acs=0;
    }

    t5(p.vo,p.acs,as,p5);
    t7(p5.ve,p5.ace,p.ace,p7);

    if(p7.ve>=p.ve){

        delta_v=p7.ve-p.ve;

        t6(p5.ve,p5.ve-delta_v,p5.ace,p6);
        t7(p6.ve,p6.ace,p.ace,p7);
    }

    if(p7.ve<p.ve){ //! Sample as down.

        t5_ve(p.vo,to_vh_dcc(p.vo,p.ve),p.acs,p5);
        t7(p5.ve,p5.ace,p.ace,p7);
    }

    pvec={p3,p5,p6,p7};

    return 1;
}

inline T sc_engine::to_vh_acc(T vo, T ve){
    return ((ve-vo)/2)+vo;
}

inline T sc_engine::to_vh_dcc(T vo, T ve){
    return vo-((vo-ve)/2);
}

V sc_engine::sc_set_a_dv(T theA, T theDv){

    a=theA;
    dv=theDv;
    ct=dv/a;
    as=2*a;
    jm=2*as/ct;
}

V sc_engine::interpolate_period(T at_time,
                                sc_period p,
                                T &pos,
                                T &vel,
                                T &acc){

    if(p.id==sc_period_id::id_t1){
        t1_i(p,at_time,vel,pos,acc);
    }
    if(p.id==sc_period_id::id_t2){
        t2_i(p,at_time,vel,pos,acc);
    }
    if(p.id==sc_period_id::id_t3){
        t3_i(p,at_time,vel,pos,acc);
    }
    if(p.id==sc_period_id::id_t4){
        t4_i(p,at_time,vel,pos,acc);
    }
    if(p.id==sc_period_id::id_t5){
        t5_i(p,at_time,vel,pos,acc);
    }
    if(p.id==sc_period_id::id_t6){
        t6_i(p,at_time,vel,pos,acc);
    }
    if(p.id==sc_period_id::id_t7){
        t7_i(p,at_time,vel,pos,acc);
    }
}



V sc_engine::interpolate_periods(T at_time,
                                 std::vector<sc_period> pvec,
                                 T &pos,
                                 T &vel,
                                 T &acc,
                                 bool &finished){
    T t=0;
    T s=0;

    if(at_time>to_ttot_pvec(pvec)){
        finished=true;
        at_time=to_ttot_pvec(pvec);
    }

    for(uint i=0; i<pvec.size(); i++){

        if(at_time>=t && at_time<t+pvec.at(i).nct){
            T time=at_time-t;
            interpolate_period(time,pvec.at(i),pos,vel,acc);
            pos+=s;
            return;
        }

        t+=pvec.at(i).nct;
        s+=pvec.at(i).ncs;
    }
}

extern "C" V interpolate_periods_c(T at_time,
                                   sc_period *pvec,
                                   T &pos,
                                   T &vel,
                                   T &acc,
                                   bool &finished){

}

B sc_engine::process_curve(sc_period_id id,
                           T vo,
                           T ve,
                           T acs,
                           T ace,
                           T ncs,
                           T vm,
                           std::vector<sc_period> &pvec){

    return process_curve({ id, vo, ve, acs, ace, ncs }, vm, pvec);
}

B sc_engine::process_curve(sc_period p, T vm, std::vector<sc_period> &pvec){

    p.ncs=std::abs(p.ncs); //! Ensure positive input.

    //! Pause requests:
    if(p.id==id_pause){
        if(p.vo>p.ve){
            t3_t5_t6_t7_t1({sc_period_id::id_none,p.vo,0,p.acs,0},pvec);
            return 1;
        }
        if(p.vo<p.ve){
            t7_t1_t2_t3_t5({sc_period_id::id_none,p.vo,0,p.acs,0},pvec);
            return 1;
        }
        if(p.vo==p.ve){
            sc_period p4;
            t4(p.vo,0,p4);
            pvec={p4};
            return 1;
        }
    }

    //! Pause resume requests:
    if(p.id==id_pause_resume){
        if(p.vo>p.ve){
            t3_t5_t6_t7_t1({sc_period_id::id_none,0,p.ve,0,p.ace},pvec);
            return 1;
        }
        if(p.vo<p.ve){
            t7_t1_t2_t3_t5({sc_period_id::id_none,0,p.ve,0,p.ace},pvec);
            return 1;
        }
        if(p.vo==p.ve){
            sc_period p4;
            t4(p.vo,0,p4);
            pvec={p4};
            return 1;
        }
    }

    if(p.id==id_run){

        //! The most common motion curve.
        if(p.vo<vm && p.ve<vm){

            std::vector<sc_period> vec_1, vec_3;
            sc_period p4;
            T stot=0;

            t7_t1_t2_t3_t5({sc_period_id::id_none,p.vo,vm,p.acs,0},vec_1);
            t3_t5_t6_t7_t1({sc_period_id::id_none,vm,p.ve,0,p.ace},vec_3);

            stot=to_stot_pvec(vec_1)+to_stot_pvec(vec_3);

            if(p.ncs==stot){
                pvec.insert(pvec.end(),vec_1.begin(),vec_1.end());
                pvec.insert(pvec.end(),vec_3.begin(),vec_3.end());
                return 1;
            }
            if(p.ncs>stot){ //! Need t4.
                pvec.insert(pvec.end(),vec_1.begin(),vec_1.end());
                t4(vm,p.ncs-stot,p4);
                pvec.push_back(p4);
                pvec.insert(pvec.end(),vec_3.begin(),vec_3.end());

                return 1;
            }

            if(p.ncs<stot){ //! Sample vm down, add t4 to fit s.
                for(T i=vm; i>std::min(p.vo,p.ve); i-=0.01*vm){ //! Sampling 10%.

                    t7_t1_t2_t3_t5({sc_period_id::id_none,p.vo,i,p.acs,0},vec_1);
                    t3_t5_t6_t7_t1({sc_period_id::id_none,i,p.ve,0,p.ace},vec_3);


                    stot=to_stot_pvec(vec_1)+to_stot_pvec(vec_3);

                    if(stot<p.ncs){

                        t4(i,p.ncs-stot,p4);

                        pvec.insert(pvec.end(),vec_1.begin(),vec_1.end());
                        pvec.push_back(p4);
                        pvec.insert(pvec.end(),vec_3.begin(),vec_3.end());

                        return 1;
                    }
                }
            }

            //! At this stage curve don't fit s.
            if(p.vo<p.ve){
                vm=p.ve;
            }
            if(p.vo>p.ve){
                vm=p.vo;
            }
            if(p.vo==p.ve){
                vm=p.ve;
            }
            //! Go on, process another curve.
        }

        if(p.vo>vm && p.ve>vm){

            std::vector<sc_period> vec_1, vec_3;
            sc_period p4;
            T stot=0;

            t3_t5_t6_t7_t1({sc_period_id::id_none,p.vo,vm,p.acs,0},vec_1);
            t7_t1_t2_t3_t5({sc_period_id::id_none,vm,p.ve,0,p.ace},vec_3);

            stot=to_stot_pvec(vec_1)+to_stot_pvec(vec_3);

            if(p.ncs==stot){
                pvec.insert(pvec.end(),vec_1.begin(),vec_1.end());
                pvec.insert(pvec.end(),vec_3.begin(),vec_3.end());
                return 1;
            }
            if(p.ncs>stot){ //! Need t4.
                pvec.insert(pvec.end(),vec_1.begin(),vec_1.end());
                t4(vm,p.ncs-stot,p4);
                pvec.push_back(p4);
                pvec.insert(pvec.end(),vec_3.begin(),vec_3.end());
                return 1;
            }

            if(p.ncs<stot){ //! Sample vm up, add t4 to fit s.
                for(T i=vm; i<std::min(p.vo,p.ve); i+=0.01*vm){ //! Sampling 10%.

                    t3_t5_t6_t7_t1({sc_period_id::id_none,p.vo,i,p.acs,0},vec_1);
                    t7_t1_t2_t3_t5({sc_period_id::id_none,i,p.ve,0,p.ace},vec_3);

                    stot=to_stot_pvec(vec_1)+to_stot_pvec(vec_3);

                    if(stot<=p.ncs){

                        t4(i,p.ncs-stot,p4);

                        pvec.insert(pvec.end(),vec_1.begin(),vec_1.end());
                        pvec.push_back(p4);
                        pvec.insert(pvec.end(),vec_3.begin(),vec_3.end());

                        return 1;
                    }
                }
            }

            //! At this stage curve don't fit s.
            if(p.vo<p.ve){
                vm=p.ve;
            }
            if(p.vo>p.ve){
                vm=p.vo;
            }
            if(p.vo==p.ve){
                vm=p.ve;
            }
            //! Go on, process another curve.
        }

        if(p.vo<vm && p.ve==vm){ //! Limits: dcc possible at end of vm.

            std::vector<sc_period> vec_1, vec_2;
            T s1=0;

            t7_t1_t2_t3_t5({sc_period_id::id_none,p.vo,vm,p.acs,0},vec_1);
            t4_ace({sc_period_id::id_none,vm,p.ve,0,p.ace},vec_2);

            s1=p.ncs-to_stot_pvec(vec_1);

            if(s1>0){ //! Steady period to fit s.
                t4_ace({sc_period_id::id_none,vm,p.ve,0,p.ace,s1},vec_2);
                pvec.insert(pvec.end(),vec_1.begin(),vec_1.end());
                pvec.insert(pvec.end(),vec_2.begin(),vec_2.end());
                return 1;
            }

            //! minimal curve.
            pvec.insert(pvec.end(),vec_1.begin(),vec_1.end());
            pvec.insert(pvec.end(),vec_2.begin(),vec_2.end());
            return 1;
        }

        if(p.vo>vm && p.ve==vm){

            std::vector<sc_period> vec_1, vec_2;
            T s1=0;

            t3_t5_t6_t7_t1({sc_period_id::id_none,p.vo,vm,p.acs,0},vec_1);
            t4_ace({sc_period_id::id_none,vm,p.ve,0,p.ace},vec_2);

            s1=p.ncs-to_stot_pvec(vec_1);

            if(s1>0){ //! Steady period to fit s.
                t4_ace({sc_period_id::id_none,vm,p.ve,0,p.ace,s1},vec_2);
                pvec.insert(pvec.end(),vec_1.begin(),vec_1.end());
                pvec.insert(pvec.end(),vec_2.begin(),vec_2.end());
                return 1;
            }

            //! minimal curve.
            pvec.insert(pvec.end(),vec_1.begin(),vec_1.end());
            pvec.insert(pvec.end(),vec_2.begin(),vec_2.end());
            return 1;
        }

        if(p.vo==vm && p.ve<vm){

            std::vector<sc_period> vec_1, vec_2;
            T s2=0;

            t4_acs({sc_period_id::id_none,p.vo,vm,p.acs,0},vec_1);
            t3_t5_t6_t7_t1({sc_period_id::id_none,p.vo,p.ve,0,p.ace},vec_2);

            s2=p.ncs-to_stot_pvec(vec_2);

            if(s2>0){ //! Steady period to fit s.
                t4_acs({sc_period_id::id_none,p.vo,vm,p.acs,0,s2},vec_1);
                pvec.insert(pvec.end(),vec_1.begin(),vec_1.end());
                pvec.insert(pvec.end(),vec_2.begin(),vec_2.end());
                return 1;
            }

            //! minimal curve.
            pvec.insert(pvec.end(),vec_1.begin(),vec_1.end());
            pvec.insert(pvec.end(),vec_2.begin(),vec_2.end());
            return 1;
        }

        if(p.vo==vm && p.ve>vm){

            std::vector<sc_period> vec_1, vec_2;
            T s2=0;

            t4_acs({sc_period_id::id_none,p.vo,vm,p.acs,0},vec_1);
            t7_t1_t2_t3_t5({sc_period_id::id_none,p.vo,p.ve,0,p.ace},vec_2);

            s2=p.ncs-to_stot_pvec(vec_2);

            if(s2>0){ //! Steady period to fit s.
                t4_acs({sc_period_id::id_none,p.vo,vm,p.acs,0,s2},vec_1);
                pvec.insert(pvec.end(),vec_1.begin(),vec_1.end());
                pvec.insert(pvec.end(),vec_2.begin(),vec_2.end());
                return 1;
            }

            //! minimal curve.
            pvec.insert(pvec.end(),vec_1.begin(),vec_1.end());
            pvec.insert(pvec.end(),vec_2.begin(),vec_2.end());
            return 1;
        }

        if(p.vo<vm && p.ve>vm){ // Stay below vm.

            std::vector<sc_period> vec_1, vec_3;
            sc_period p4;
            T stot=0;

            t7_t1_t2_t3_t5({sc_period_id::id_none,p.vo,vm,p.acs,0},vec_1);
            t7_t1_t2_t3_t5({sc_period_id::id_none,vm,p.ve,0,p.ace},vec_3);

            stot=to_stot_pvec(vec_1)+to_stot_pvec(vec_3);

            if(p.ncs==stot){
                pvec.insert(pvec.end(),vec_1.begin(),vec_1.end());
                pvec.insert(pvec.end(),vec_3.begin(),vec_3.end());
                return 1;
            }
            if(p.ncs>stot){ //! Need t4.
                pvec.insert(pvec.end(),vec_1.begin(),vec_1.end());
                t4(vm,p.ncs-stot,p4);
                pvec.push_back(p4);
                pvec.insert(pvec.end(),vec_3.begin(),vec_3.end());
                return 1;
            }
            if(p.ncs<stot){ //! Sample vm down, add t4 to fit s.
                for(T i=vm; i>p.vo; i-=0.1*vm){ //! Sampling 10%.

                    t7_t1_t2_t3_t5({sc_period_id::id_none,p.vo,vm,p.acs,0},vec_1);
                    t7_t1_t2_t3_t5({sc_period_id::id_none,vm,p.ve,0,p.ace},vec_3);

                    stot=to_stot_pvec(vec_1)+to_stot_pvec(vec_3);

                    if(stot<=p.ncs){

                        t4(i,p.ncs-stot,p4);

                        pvec.insert(pvec.end(),vec_1.begin(),vec_1.end());
                        pvec.push_back(p4);
                        pvec.insert(pvec.end(),vec_3.begin(),vec_3.end());

                        return 1;
                    }
                }
            }

            //! Minimal curve.
            pvec.insert(pvec.end(),vec_1.begin(),vec_1.end());
            pvec.insert(pvec.end(),vec_3.begin(),vec_3.end());
        }

        if(p.vo>vm && p.ve<vm){

            std::vector<sc_period> vec_1, vec_3;
            sc_period p4;
            T stot=0;

            t3_t5_t6_t7_t1({sc_period_id::id_none,p.vo,vm,p.acs,0},vec_1);
            t3_t5_t6_t7_t1({sc_period_id::id_none,vm,p.ve,0,p.ace},vec_3);

            stot=to_stot_pvec(vec_1)+to_stot_pvec(vec_3);

            if(p.ncs==stot){
                pvec.insert(pvec.end(),vec_1.begin(),vec_1.end());
                pvec.insert(pvec.end(),vec_3.begin(),vec_3.end());
                return 1;
            }
            if(p.ncs>stot){ //! Need t4.
                pvec.insert(pvec.end(),vec_1.begin(),vec_1.end());
                t4(vm,p.ncs-stot,p4);
                pvec.push_back(p4);
                pvec.insert(pvec.end(),vec_3.begin(),vec_3.end());
                return 1;
            }
            if(p.ncs<stot){ //! Sample vm down, add t4 to fit s.
                for(T i=vm; i>std::max(p.vo,p.ve); i-=0.1*vm){ //! Sampling 10%.

                    t3_t5_t6_t7_t1({sc_period_id::id_none,p.vo,i,p.acs,0},vec_1);
                    t3_t5_t6_t7_t1({sc_period_id::id_none,i,p.ve,0,p.ace},vec_3);

                    stot=to_stot_pvec(vec_1)+to_stot_pvec(vec_3);

                    if(stot<=p.ncs){

                        t4(i,p.ncs-stot,p4);

                        pvec.insert(pvec.end(),vec_1.begin(),vec_1.end());
                        pvec.push_back(p4);
                        pvec.insert(pvec.end(),vec_3.begin(),vec_3.end());

                        return 1;
                    }
                }
            }

            //! Minimal curve.
            pvec.insert(pvec.end(),vec_1.begin(),vec_1.end());
            pvec.insert(pvec.end(),vec_3.begin(),vec_3.end());
        }

        if(p.vo==vm && p.ve==vm){
            sc_period pr;
            t4(p.vo,p.ncs,pr);
            pvec={pr};
            return 1;
        }
    }
    return 0;
}

T  sc_engine::to_stot_pvec(std::vector<sc_period> pvec){
    T s=0;
    for(uint i=0; i<pvec.size(); i++){
        s+=pvec.at(i).ncs;
    }
    return s;
}

T  sc_engine::to_ttot_pvec(std::vector<sc_period> pvec){
    T t=0;
    for(uint i=0; i<pvec.size(); i++){
        t+=pvec.at(i).nct;
    }
    return t;
}

T  sc_engine::netto_difference_of_2_values(T a, T b){

    T diff=0;
    if(a<0 && b<0){
        a=std::abs(a);
        b=std::abs(b);
        diff=std::abs(a-b);
    }
    if(a>=0 && b>=0){
        diff=std::abs(a-b);
    }
    if(a<=0 && b>=0){;
        diff=std::abs(a)+b;
    }
    if(a>=0 && b<=0){
        diff=a+std::abs(b);
    }
    return diff;
}

B sc_engine::is_inbetween_2_values(T a, T b, T value){

    if(value>=a && value<=b){
        return true;
    }
    return false;
}

V sc_engine::curve_progress(std::vector<sc_period> pvec,
                            T position,
                            T &curve_progress,
                            T &curve_dtg,
                            UI &curve_nr){

    T l=0;
    for(uint i=0; i<pvec.size(); i++){

        if(position>=l && position<=l+pvec.at(i).ncs){

            curve_nr=i;
            position-=l;
            curve_progress=position/pvec.at(i).ncs; //! 0-1
            curve_dtg=pvec.at(i).ncs-position;
            return;
        }

        l+=pvec.at(i).ncs;
    }
}















