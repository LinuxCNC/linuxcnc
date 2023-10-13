#include "rtapi.h"
#include "sc_engine.h"

//! A few variables.
//! as  acceleration at inflection point. as=2*a.
//! jm  jerk max.
//! a   acceleration.
//! dv  delta_velocity, the curve power, defines the portion of period t2.
//!     if velocity end-start>dv. The leftover is the t2 for acc periods,
//!     or t6 for dcc periods..
//! ct  curte time.
T as,jm,a,dv,ct;

I set_a_dv(T theA, T theDv){
    a=theA;
    dv=theDv;
    ct=dv/a;
    as=2*a;
    jm=2*as/ct;

    return check_a_as_jm_dv();
}

I check_a_as_jm_dv(){
    if(a==0 || as==0 || jm==0 || dv==0){
        rtapi_print_msg(RTAPI_MSG_ERR,"scurve, check values for a,as,dv.\n");
        return 1;
    }
    rtapi_print_msg(RTAPI_MSG_ERR,"check: values for a,as,dv ok.\n");
    return 1;
}

I t1(T vo, T acs, T ace, struct sc_period *p){

    acs=fabs(acs);
    ace=fabs(ace);
    acs=fmin(as,acs);
    ace=fmin(as,ace);

    T ts=acs/jm;
    T vf=vo-(jm*(ts*ts)/2);
    T so=vf*ts+jm*(ts*ts*ts)/6;

    T te=ace/jm;
    T ve=vf+jm*(te*te)/2;
    T se=vf*te+jm*(te*te*te)/6;

    T ncs=se-so;
    T nct=te-ts;

    p->id=id_t1;
    p->vo=vo;
    p->ve=ve;
    p->acs=acs;
    p->ace=ace;
    p->ncs=ncs;
    p->nct=nct;

    return 0;
}

I t1_i(struct sc_period p, T ti, struct sc_vsa *vsa){

    T ts=p.acs/jm;
    T vf=p.vo-(jm*(ts*ts)/2);

    ti+=ts;
    T vi=vf+jm*(ti*ti)/2;
    vsa->v=vi;

    T so=vf*ts+jm*(ts*ts*ts)/6;
    T se=vf*ti+jm*(ti*ti*ti)/6;
    T si=se-so;
    vsa->s=si;

    T ai=jm*ti;
    vsa->a=ai;

    return 0;
}

I t1_ve(T vo, T ve, T acs, struct sc_period *p){

    acs=fabs(acs);
    acs=fmin(acs,as);

    T ts=acs/jm;
    T vf=vo-(jm*(ts*ts)/2);

    T so=vf*ts+jm*(ts*ts*ts)/6;

    T te=sqrt(-2*vf+2*ve)/sqrt(jm);
    te=fmin(0.5*ct,te);
    T ve_=vf+jm*(te*te)/2;
    T ace=jm*te;

    T se=vf*te+jm*(te*te*te)/6;

    T ncs=se-so;
    T nct=te-ts;

    p->id=id_t1;
    p->vo=vo;
    p->ve=ve_;
    p->acs=acs;
    p->ace=ace;
    p->ncs=ncs;
    p->nct=nct;

    return 0;
}

I t1_pid(T vo, T acs, T interval, struct sc_period *p){

    acs=fabs(acs);
    acs=fmin(as,acs);

    T ts=acs/jm;
    T vf=vo-(jm*(ts*ts)/2);
    T so=vf*ts+jm*(ts*ts*ts)/6;

    T te=ts+interval;
    te=fmin(te,as/jm); //! Limits te to time at as.
    T ve=vf+jm*(te*te)/2;
    T se=vf*te+jm*(te*te*te)/6;

    T ace=jm*te;

    T ncs=se-so;
    T nct=te-ts;

    p->id=id_t1;
    p->vo=vo;
    p->ve=ve;
    p->acs=acs;
    p->ace=ace;
    p->ncs=ncs;
    p->nct=nct;

    return 0;
}

I t2(T vo, T ve, T a, struct sc_period *p){

    T ncs=((ve*ve) - (vo*vo))/(2*a) ;
    T nct=(ve-vo)/a;

    p->id=id_t2;
    p->vo=vo;
    p->ve=ve;
    p->acs=a;
    p->ace=a;
    p->ncs=ncs;
    p->nct=nct;

    return 0;
}

I t2_i(struct sc_period p, T ti, struct sc_vsa *vsa){

    T vi=p.vo + p.acs*ti;
    T si=p.vo*ti + 0.5*p.acs*(ti*ti);
    T ai=p.acs;

    vsa->v=vi;
    vsa->s=si;
    vsa->a=ai;

    return 0;
}

I t2_pid(T vo, T a, T interval, struct sc_period *p){

    T t=interval;
    T ve=vo + a*t;
    T s=vo*t + 0.5*a*(t*t);

    T ncs=s;
    T nct=interval;

    p->id=id_t2;
    p->vo=vo;
    p->ve=ve;
    p->acs=a;
    p->ace=a;
    p->ncs=ncs;
    p->nct=nct;

    return 0;
}

I t3(T vo, T acs, T ace, struct sc_period *p){

    acs=fabs(acs);
    ace=fabs(ace);
    acs=fmin(as,acs);
    ace=fmin(as,ace);

    T ts=(as-acs)/jm;
    T vf = -as*ts+((jm*(ts*ts))/2)+vo;
    T so=vf*ts + as*(ts*ts)/2 - jm*(ts*ts*ts)/6;

    T te=(as-ace)/jm;
    T ve=vf + as*te - jm*(te*te)/2;
    T se=vf*te + as*(te*te)/2 - jm*(te*te*te)/6;

    T ncs=se-so;
    T nct=te-ts;

    p->id=id_t3;
    p->vo=vo;
    p->ve=ve;
    p->acs=acs;
    p->ace=ace;
    p->ncs=ncs;
    p->nct=nct;

    return 0;
}

I t3_i(struct sc_period p, T ti, struct sc_vsa *vsa){

    T ts=(as-p.acs)/jm;
    T t=ts;
    T vf = -as*t+((jm*(t*t))/2)+p.vo;
    T so=vf*t + as*(t*t)/2 - jm*(t*t*t)/6;

    t=0;
    T sf=vf*t + as*(t*t)/2 - jm*(t*t*t)/6;

    t=ti+ts;
    T vi=vf + as*t - jm*(t*t)/2;
    T si=vf*t + as*(t*t)/2 - jm*(t*t*t)/6;
    si-=so-sf;
    T ai=as-(jm*t);

    vsa->v=vi;
    vsa->s=si;
    vsa->a=ai;

    return 0;
}

I t3_pid(T vo, T acs, T interval, struct sc_period *p){

    acs=fabs(acs);
    acs=fmin(as,acs);

    T ts=(as-acs)/jm;
    T vf = -as*ts+((jm*(ts*ts))/2)+vo;
    T so=vf*ts + as*(ts*ts)/2 - jm*(ts*ts*ts)/6;

    T te=ts+interval;
    te=fmin(0.5*ct,te);
    T ace=as-(te*jm);

    T ve=vf + as*te - jm*(te*te)/2;
    T se=vf*te + as*(te*te)/2 - jm*(te*te*te)/6;

    T ncs=se-so;
    T nct=te-ts;

    p->id=id_t3;
    p->vo=vo;
    p->ve=ve;
    p->acs=acs;
    p->ace=ace;
    p->ncs=ncs;
    p->nct=nct;

    return 0;
}

I t4(T vo, T s, struct sc_period *p){

    T nct=s/vo;
    T ncs=s;

    p->id=id_t4;
    p->vo=vo;
    p->ve=vo;
    p->acs=0;
    p->ace=0;
    p->ncs=ncs;
    p->nct=nct;

    return 0;
}

I t4_i(struct sc_period p, T ti, struct sc_vsa *vsa){

    T vi=p.vo;
    T si=p.vo*ti;
    T ai=0;

    vsa->v=vi;
    vsa->s=si;
    vsa->a=ai;

    return 0;
}

I t4_pid(T vo, T a, T interval, struct sc_period *p){

    a=0.0; //! Used as dummy input. To keep pid function input the same.

    T nct=interval;
    T ncs=vo*interval;

    p->id=id_t4;
    p->vo=vo;
    p->ve=vo;
    p->acs=a;
    p->ace=a;
    p->ncs=ncs;
    p->nct=nct;

    return 0;
}

I t4_acs(struct sc_period p, struct sc_period **pvec, size_t *size){

    struct sc_period p1,p3,p4,p5,p7;
    if(p.acs>0){
        t3(p.vo,p.acs,0,&p3);

        t5_ve(p3.ve,to_vh_dcc(p3.ve,p.ve),0,&p5);
        t7(p5.ve,p5.ace,0,&p7);

        T s=p.ncs-(p3.ncs+p5.ncs+p7.ncs);
        if(s>0){ //! Need steady period.
            t4(p7.ve,s,&p4);

            *size=4;
            *pvec = malloc(*size * sizeof(struct sc_period));
            if (*pvec == NULL) {
                return 1; // Error allocating memory
            }

            (*pvec)[0] = p3;
            (*pvec)[1] = p5;
            (*pvec)[2] = p7;
            (*pvec)[3] = p4;

            return 0;
        } else {

            *size=3;
            *pvec = malloc(*size * sizeof(struct sc_period));
            if (*pvec == NULL) {
                return 1; // Error allocating memory
            }

            (*pvec)[0] = p3;
            (*pvec)[1] = p5;
            (*pvec)[2] = p7;

            return 0;
        }
    }
    if(p.acs<0){
        t7(p.vo,p.acs,0,&p7);

        t1_ve(p7.ve,to_vh_acc(p7.ve,p.ve),0,&p1);
        t3(p1.ve,p1.ace,0,&p3);

        T s=p.ncs-(p7.ncs+p1.ncs+p3.ncs);
        if(s>0){ //! Need steady period.
            t4(p3.ve,s,&p4);

            *size=4;
            *pvec = malloc(*size * sizeof(struct sc_period));
            if (*pvec == NULL) {
                return 1; // Error allocating memory
            }

            (*pvec)[0] = p7;
            (*pvec)[1] = p1;
            (*pvec)[2] = p3;
            (*pvec)[3] = p4;

            return 0;
        } else {

            *size=3;
            *pvec = malloc(*size * sizeof(struct sc_period));
            if (*pvec == NULL) {
                return 1; // Error allocating memory
            }

            (*pvec)[0] = p7;
            (*pvec)[1] = p1;
            (*pvec)[2] = p3;

            return 0;
        }
    }
    if(p.acs==0){
        t4(p.vo,p.ncs,&p4);

        *size=1;
        *pvec = malloc(*size * sizeof(struct sc_period));
        if (*pvec == NULL) {
            return 1; // Error allocating memory
        }

        (*pvec)[0] = p4;

        return 0;
    }
    return 1;
}

I t4_ace(struct sc_period p, struct sc_period **pvec, size_t *size){

    struct sc_period p1,p3,p4,p5,p7;
    if(p.ace>0){
        T a_number=100;
        t5(a_number,0,p.ace,&p5);
        T dv=a_number-p5.ve;

        t1_ve(p.vo,to_vh_acc(p.vo,p.vo+dv),0,&p1);
        t3(p1.ve,p1.ace,0,&p3);
        t5(p3.ve,0,p.ace,&p5);

        T s=p.ncs-(p1.ncs+p3.ncs+p5.ncs);
        if(s>0){ //! Need steady period.
            t4(p.vo,s,&p4);

            *size=4;
            *pvec = malloc(*size * sizeof(struct sc_period));
            if (*pvec == NULL) {
                return 1; // Error allocating memory
            }

            (*pvec)[0] = p4;
            (*pvec)[1] = p1;
            (*pvec)[2] = p3;
            (*pvec)[3] = p5;

            return 0;
        } else { //! Minimal curve.

            *size=3;
            *pvec = malloc(*size * sizeof(struct sc_period));
            if (*pvec == NULL) {
                return 1; // Error allocating memory
            }

            (*pvec)[0] = p1;
            (*pvec)[1] = p3;
            (*pvec)[2] = p5;

            return 0;
        }
    }
    if(p.ace<0){
        T a_number=100;
        t1(a_number,0,p.ace,&p1);
        T dv=p1.ve-a_number;

        t5_ve(p.vo,to_vh_dcc(p.vo,p.vo-dv),0,&p5);
        t7(p5.ve,p5.ace,0,&p7);
        t1(p7.ve,0,p.ace,&p1);

        T s=p.ncs-(p5.ncs+p7.ncs+p1.ncs);
        if(s>0){ //! Need steady period.
            t4(p.vo,s,&p4);

            *size=4;
            *pvec = malloc(*size * sizeof(struct sc_period));
            if (*pvec == NULL) {
                return 1; // Error allocating memory
            }

            (*pvec)[0] = p4;
            (*pvec)[1] = p5;
            (*pvec)[2] = p7;
            (*pvec)[3] = p1;

            return 0;
        } else { //! Minimal curve.

            *size=3;
            *pvec = malloc(*size * sizeof(struct sc_period));
            if (*pvec == NULL) {
                return 1; // Error allocating memory
            }

            (*pvec)[0] = p5;
            (*pvec)[1] = p7;
            (*pvec)[2] = p1;

            return 0;
        }
    }
    if(p.ace==0){
        t4(p.vo,p.ncs,&p4);

        *size=1;
        *pvec = malloc(*size * sizeof(struct sc_period));
        if (*pvec == NULL) {
            return 1; // Error allocating memory
        }

        (*pvec)[0] = p4;

        return 0;
    }
    return 1;
}

I t5(T vo, T acs, T ace, struct sc_period *p){

    acs=fabs(acs);
    ace=fabs(ace);
    acs=fmin(as,acs);
    ace=fmin(as,ace);
    acs=fmin(acs,ace);

    T ts=acs/jm;
    T vf=((jm*(ts*ts))/2)+vo;
    T so=vf*ts-jm*(ts*ts*ts)/6;

    T te=ace/jm;
    T ve=vf-jm*(te*te)/2;
    T se=vf*te-jm*(te*te*te)/6;

    T ncs=se-so;
    T nct=te-ts;

    acs=-fabs(acs);
    ace=-fabs(ace);

    p->id=id_t5;
    p->vo=vo;
    p->ve=ve;
    p->acs=acs;
    p->ace=ace;
    p->ncs=ncs;
    p->nct=nct;

    return 0;
}

I t5_i(struct sc_period p, T ti, struct sc_vsa *vsa){

    p.acs=fabs(p.acs);

    T ts=p.acs/jm;
    T t=ts;
    T vf=((jm*(t*t))/2)+p.vo;
    T so=vf*t-jm*(t*t*t)/6;

    t=0;
    T sf=vf*t-jm*(t*t*t)/6;

    t=ti+ts;
    T vi=vf-jm*(t*t)/2;
    T ai=-fabs(jm*t);
    T si=vf*t-jm*(t*t*t)/6;
    si-=so-sf;

    vsa->v=vi;
    vsa->s=si;
    vsa->a=ai;

    return 0;
}

I t5_ve(T vo, T ve, T acs, struct sc_period *p){

    acs=fabs(acs);
    acs=fmin(acs,as);

    T ts=acs/jm;
    T vf=((jm*(ts*ts))/2)+vo;
    T so=vf*ts-jm*(ts*ts*ts)/6;

    T te=(sqrt(2)*sqrt(vf-ve))/sqrt(jm);
    te=fmin(0.5*ct,te);
    T ace_=-fabs(jm*te);
    T ve_=vf-jm*(te*te)/2;
    T se=vf*te-jm*(te*te*te)/6;

    T ncs=se-so;
    T nct=te-ts;

    p->id=id_t5;
    p->vo=vo;
    p->ve=ve_;
    p->acs=acs;
    p->ace=ace_;
    p->ncs=ncs;
    p->nct=nct;

    return 0;
}

I t5_pid(T vo, T acs, T interval, struct sc_period *p){

    acs=fabs(acs);
    acs=fmin(acs,as);

    T ts=acs/jm;
    T vf=((jm*(ts*ts))/2)+vo;
    T so=vf*ts-jm*(ts*ts*ts)/6;

    T te=ts+interval;
    te=fmin(0.5*ct,te);
    T ace=jm*te;
    T ve=vf-jm*(te*te)/2;
    T se=vf*te-jm*(te*te*te)/6;

    T ncs=se-so;
    T nct=te-ts;

    acs=-fabs(acs);
    ace=-fabs(ace);

    p->id=id_t5;
    p->vo=vo;
    p->ve=ve;
    p->acs=acs;
    p->ace=ace;
    p->ncs=ncs;
    p->nct=nct;

    return 0;
}

I t6(T vo, T ve, T a, struct sc_period *p){

    a=fabs(a);
    T ncs=((vo*vo) - (ve*ve))/(2*a);
    T nct=(vo-ve)/a;

    p->id=id_t6;
    p->vo=vo;
    p->ve=ve;
    p->acs=a;
    p->ace=a;
    p->ncs=ncs;
    p->nct=nct;

    return 0;
}

I t6_i(struct sc_period p, T ti, struct sc_vsa *vsa){

    T t=0;
    p.acs=fabs(p.acs);

    t=ti;
    T vi=p.vo - p.acs*t;
    T si=p.vo*t - 0.5*p.acs*(t*t);
    T ai=-fabs(p.acs);

    vsa->v=vi;
    vsa->s=si;
    vsa->a=ai;

    return 0;
}

I t6_pid(T vo, T a, T interval, struct sc_period *p){

    T t=interval;
    a=fabs(a);
    T ve=vo - a*t;
    T s=vo*t - 0.5*a*(t*t);

    T ncs=s;
    T nct=interval;

    a=-fabs(a);

    p->id=id_t6;
    p->vo=vo;
    p->ve=ve;
    p->acs=a;
    p->ace=a;
    p->ncs=ncs;
    p->nct=nct;

    return 0;
}

I t7(T vo, T acs, T ace, struct sc_period *p){

    acs=fabs(acs);
    ace=fabs(ace);
    acs=fmin(as,acs);
    ace=fmin(as,ace);

    T ts=(as-acs)/jm;
    T vf=as*ts - ((jm*(ts*ts))/2) + vo;
    T so=vf*ts - as*(ts*ts)/2 + jm*(ts*ts*ts)/6;

    T te=(as-ace)/jm;
    T se=vf*te - as*(te*te)/2 + jm*(te*te*te)/6;
    T ve=vf - as*te + jm*(te*te)/2;

    T ncs=se-so;
    T nct=te-ts;

    acs=-fabs(acs);
    ace=-fabs(ace);

    p->id=id_t7;
    p->vo=vo;
    p->ve=ve;
    p->acs=acs;
    p->ace=ace;
    p->ncs=ncs;
    p->nct=nct;

    return 0;
}

I t7_i(struct sc_period p, T ti, struct sc_vsa *vsa){

    p.acs=fabs(p.acs);

    T ts=(as-p.acs)/jm;
    T t=ts;
    T vf=as*t - ((jm*(t*t))/2) + p.vo;
    T so=vf*t - as*(t*t)/2 + jm*(t*t*t)/6;

    t=0;
    T sf=vf*t - as*(t*t)/2 + jm*(t*t*t)/6;

    t=ti+ts;
    T vi=vf - as*t + jm*(t*t)/2;
    T si=vf*t - as*(t*t)/2 + jm*(t*t*t)/6;
    si-=so-sf;
    T ai=-fabs(as-jm*t);

    vsa->v=vi;
    vsa->s=si;
    vsa->a=ai;

    return 0;
}

I t7_pid(T vo, T acs, T interval, struct sc_period *p){

    acs=fabs(acs);
    acs=fmin(as,acs);

    T ts=(as-acs)/jm;
    T vf=as*ts - ((jm*(ts*ts))/2) + vo;
    T so=vf*ts - as*(ts*ts)/2 + jm*(ts*ts*ts)/6;

    T te=ts+interval;
    te=fmin(0.5*ct,te);
    T ace=as-(te*jm);

    T se=vf*te - as*(te*te)/2 + jm*(te*te*te)/6;
    T ve=vf - as*te + jm*(te*te)/2;

    T ncs=se-so;
    T nct=te-ts;

    acs=-fabs(acs);
    ace=-fabs(ace);

    p->id=id_t7;
    p->vo=vo;
    p->ve=ve;
    p->acs=acs;
    p->ace=ace;
    p->ncs=ncs;
    p->nct=nct;

    return 0;
}

T to_ttot_pvec(struct sc_period pvec[], size_t size){
    T t=0;
    for(uint i=0; i<size; i++){
        t+=pvec[i].nct;
    }
    return t;
}

T to_stot_pvec(struct sc_period pvec[], size_t size){
    T s=0;
    for(uint i=0; i<size; i++){
        s+=pvec[i].ncs;
    }
    return s;
}

T to_vh_acc(T vo, T ve){
    return ((ve-vo)/2)+vo;
}

T to_vh_dcc(T vo, T ve){
    return vo-((vo-ve)/2);
}

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

I is_inbetween_2_values(T a, T b, T value){
    if(value>=a && value<=b){
        return 0;
    }
    return 1;
}

I interpolate_period(T at_time,
                     struct sc_period p,
                     struct sc_vsa *vsa){

    I r=0;
    if(p.id==id_t1){
        r+=t1_i(p,at_time,vsa);
    }
    if(p.id==id_t2){
        r+=t2_i(p,at_time,vsa);
    }
    if(p.id==id_t3){
        r+=t3_i(p,at_time,vsa);
    }
    if(p.id==id_t4){
        r+=t4_i(p,at_time,vsa);
    }
    if(p.id==id_t5){
        r+=t5_i(p,at_time,vsa);
    }
    if(p.id==id_t6){
        r+=t6_i(p,at_time,vsa);
    }
    if(p.id==id_t7){
        r+=t7_i(p,at_time,vsa);
    }
    return r;
}

I interpolate_periods(T at_time,
                      struct sc_period pvec[],
                      size_t size,
                      struct sc_vsa *vsa,
                      I *finished){

    T t=0;
    T s=0;
    *finished=0;

    T ttot=to_ttot_pvec(pvec,size);

    if(at_time>ttot){
        *finished=1;
        at_time=ttot;
    }

    for(uint i=0; i<size; i++){

        if(at_time>=t && at_time<t+pvec[i].nct){
            T time=at_time-t;
            interpolate_period(time,pvec[i],vsa);
            vsa->s+=s;
            return 0;
        }

        t+=pvec[i].nct;
        s+=pvec[i].ncs;
    }
    return 1;
}

I curve_progress(struct sc_period pvec[],
                 size_t size,
                 T position,
                 T *curve_progress,
                 T *curve_dtg,
                 UI *curve_nr){

    T l=0;
    for(uint i=0; i<size; i++){

        if(position>=l && position<=l+pvec[i].ncs){

            *curve_nr=i;
            position-=l;
            *curve_progress=position/pvec[i].ncs; //! 0-1
            *curve_dtg=pvec[i].ncs-position;
            return 0;
        }
        l+=pvec[i].ncs;
    }
    return 1;
}

V t1_t2_t3_s(T vo, T ve, T *s){

    struct sc_period p;
    T delta_v=ve-vo;
    T st=0;

    if(delta_v==dv){ //! Ok, exact.
        t1(vo,0,as,&p);
        st+=p.ncs;
        t3(p.ve,p.ace,0,&p);
        st+=p.ncs;
    }
    if(delta_v>dv){ //! Need t2.
        T v2=delta_v-dv;
        t1(vo,0,as,&p);
        st+=p.ncs;
        t2(p.ve,p.ve+v2,p.ace,&p);
        st+=p.ncs;
        t3(p.ve,p.ace,0,&p);
        st+=p.ncs;
    }
    if(delta_v<dv){ //! Need t1 ace.
        t1_ve(vo,to_vh_acc(vo,ve),0,&p);
        st+=p.ncs;
        t3(p.ve,p.ace,0,&p);
        st+=p.ncs;
    }
    *s=st;
}

V t5_t6_t7_s(T vo, T ve, T *s){

    struct sc_period p;
    T delta_v=fabs(ve-vo);
    T  st=0;

    if(delta_v==dv){ //! Ok, exact.
        t5(vo,0,as,&p);
        st+=p.ncs;
        t7(p.ve,p.ace,0,&p);
        st+=p.ncs;
    }
    if(delta_v>dv){ //! Need t6.
        T v6=delta_v-dv;
        t5(vo,0,as,&p);
        st+=p.ncs;
        t6(p.ve,p.ve-v6,p.ace,&p);
        st+=p.ncs;
        t7(p.ve,p.ace,0,&p);
        st+=p.ncs;
    }
    if(delta_v<dv){ //! Need t5 ace.
        t5_ve(vo,to_vh_dcc(vo,ve),0,&p);
        st+=p.ncs;
        t7(p.ve,p.ace,0,&p);
        st+=p.ncs;
    }
    *s=st;
}

I t1_t2_t3_pvec(struct sc_period p, struct sc_period **pvec, size_t *size){

    struct sc_period p1,p2,p3;

    *size=3;
    *pvec = malloc(*size * sizeof(struct sc_period));
    if (*pvec == NULL) {
        return 1; // Error allocating memory
    }

    T delta_v=0;

    t1(p.vo,p.acs,as,&p1);
    t3(p1.ve,p1.ace,p.ace,&p3);

    if(p3.ve<=p.ve){
        t1(p.vo,p.acs,as,&p1);
        delta_v=p.ve-p3.ve;
        t2(p1.ve,p1.ve+delta_v,p1.ace,&p2);
        t3(p2.ve,p2.ace,p.ace,&p3);

        (*pvec)[0] = p1;
        (*pvec)[1] = p2;
        (*pvec)[2] = p3;

        return 0;
    }

    if(p3.ve>p.ve){ //! Sample as down.
        for(T i=as; i>0; i-=0.1*as){ //! Sampling 10%.
            t1(p.vo,p.acs,i,&p1);
            t3(p1.ve,p1.ace,p.ace,&p3);

            if(p3.ve<p.ve){

                delta_v=p.ve-p3.ve;

                t1(p.vo,p.acs,i,&p1);
                t2(p1.ve,p1.ve+delta_v,p1.ace,&p2);
                t3(p2.ve,p2.ace,p.ace,&p3);

                (*pvec)[0] = p1;
                (*pvec)[1] = p2;
                (*pvec)[2] = p3;

                return 0;
            }
        }
    }
    return 1;
}

I t7_t1_t2_t3_t5_pvec(struct sc_period p, struct sc_period **pvec, size_t *size){

    struct sc_period p7,p1,p2,p3,p5;
    T delta_v=0;

    if(p.acs<0){
        t7(p.vo,p.acs,0,&p7);
        p.vo=p7.ve;
        p.acs=0;
    }

    t1(p.vo,p.acs,as,&p1);

    if(p.ace<0){
        t3(p1.ve,p1.ace,0,&p3);
        t5(p3.ve,0,p.ace,&p5);
        p3.ve=p5.ve;
    } else {
        t3(p1.ve,p1.ace,p.ace,&p3);
    }

    if(p3.ve<=p.ve){ //! Need t2.
        t1(p.vo,p.acs,as,&p1);
        delta_v=p.ve-p3.ve;
        t2(p1.ve,p1.ve+delta_v,p1.ace,&p2);

        if(p.ace<0){
            t3(p2.ve,p2.ace,0,&p3);
            t5(p3.ve,0,p.ace,&p5);

            *size=5;
            *pvec = malloc(*size * sizeof(struct sc_period));
            if (*pvec == NULL) {
                return 1; // Error allocating memory
            }

            (*pvec)[0] = p7;
            (*pvec)[1] = p1;
            (*pvec)[2] = p2;
            (*pvec)[3] = p3;
            (*pvec)[4] = p5;

        } else {
            t3(p2.ve,p2.ace,p.ace,&p3);

            *size=4;
            *pvec = malloc(*size * sizeof(struct sc_period));
            if (*pvec == NULL) {
                return 1; // Error allocating memory
            }

            (*pvec)[0] = p7;
            (*pvec)[1] = p1;
            (*pvec)[2] = p2;
            (*pvec)[3] = p3;
        }
        return 0;
    }

    if(p3.ve>p.ve){ //! Sample as down.
        for(T i=as; i>0; i-=0.1*as){ //! Sampling 10%.
            t1(p.vo,p.acs,i,&p1);
            t3(p1.ve,p1.ace,p.ace,&p3);

            if(p.ace<0){
                t3(p1.ve,p1.ace,0,&p3);
                t5(p3.ve,0,p.ace,&p5);
                p3.ve=p5.ve;
            } else {
                t3(p1.ve,p1.ace,p.ace,&p3);
            }

            if(p3.ve<p.ve){

                delta_v=p.ve-p3.ve;

                t1(p.vo,p.acs,i,&p1);
                t2(p1.ve,p1.ve+delta_v,p1.ace,&p2);

                if(p.ace<0){
                    t3(p2.ve,p2.ace,0,&p3);
                    t5(p3.ve,0,p.ace,&p5);

                    *size=5;
                    *pvec = malloc(*size * sizeof(struct sc_period));
                    if (*pvec == NULL) {
                        return 1; // Error allocating memory
                    }

                    (*pvec)[0] = p7;
                    (*pvec)[1] = p1;
                    (*pvec)[2] = p2;
                    (*pvec)[3] = p3;
                    (*pvec)[4] = p5;

                } else {
                    t3(p2.ve,p2.ace,p.ace,&p3);

                    *size=4;
                    *pvec = malloc(*size * sizeof(struct sc_period));
                    if (*pvec == NULL) {
                        return 1; // Error allocating memory
                    }

                    (*pvec)[0] = p7;
                    (*pvec)[1] = p1;
                    (*pvec)[2] = p2;
                    (*pvec)[3] = p3;
                }
                return 0;
            }
        }
    }
    return 1;
}

I t5_t6_t7_pvec(struct sc_period p, struct sc_period **pvec, size_t *size){

    struct sc_period p5,p6,p7;
    T delta_v=0;

    t5(p.vo,p.acs,as,&p5);
    t7(p5.ve,p5.ace,p.ace,&p7);

    if(p7.ve>=p.ve){

        delta_v=p7.ve-p.ve;

        t6(p5.ve,p5.ve-delta_v,p5.ace,&p6);
        t7(p6.ve,p6.ace,p.ace,&p7);
        //! std::cout<<"normal, ve:"<<p7.ve<<std::endl;
    }

    if(p7.ve<p.ve){ //! Sample as down.
        for(T i=as; i>0; i-=(0.1*as)){ //! Sampling 10%.
            t5(p.vo,p.acs,i,&p5);
            t7(p5.ve,p5.ace,p.ace,&p7);

            if(p7.ve>p.ve){

                delta_v=p7.ve-p.ve;

                t5(p.vo,p.acs,i,&p5);
                t6(p5.ve,p5.ve-delta_v,p5.ace,&p6);
                t7(p6.ve,p6.ace,p.ace,&p7);
                break;
            }
        }
    }

    *size=3;
    *pvec = malloc(*size * sizeof(struct sc_period));
    if (*pvec == NULL) {
        return 1; // Error allocating memory
    }

    (*pvec)[0] = p5;
    (*pvec)[1] = p6;
    (*pvec)[2] = p7;

    return 0;
}

I t3_t5_t6_t7_t1_pvec(struct sc_period p, struct sc_period **pvec, size_t *size){

    struct sc_period p3,p5,p6,p7,p1;
    T delta_v=0;

    if(p.acs>0){
        t3(p.vo,p.acs,0,&p3);
        p.vo=p3.ve;
        p.acs=0;
    }

    t5(p.vo,p.acs,as,&p5);

    if(p.ace>0){
        t7(p5.ve,p5.ace,0,&p7);
        t1(p7.ve,0,p.ace,&p1);
        p7.ve=p1.ve;
    } else {
        t7(p5.ve,p5.ace,p.ace,&p7);
    }

    if(p7.ve>=p.ve){ //! Need t2.

        delta_v=p7.ve-p.ve;

        t6(p5.ve,p5.ve-delta_v,p5.ace,&p6);

        if(p.ace>0){
            t7(p6.ve,p6.ace,0,&p7);
            t1(p7.ve,0,p.ace,&p1);

            *size=5;
            *pvec = malloc(*size * sizeof(struct sc_period));
            if (*pvec == NULL) {
                return 1; // Error allocating memory
            }

            (*pvec)[0] = p3;
            (*pvec)[1] = p5;
            (*pvec)[2] = p6;
            (*pvec)[3] = p7;
            (*pvec)[4] = p1;

        } else {
            t7(p6.ve,p6.ace,p.ace,&p7);

            *size=4;
            *pvec = malloc(*size * sizeof(struct sc_period));
            if (*pvec == NULL) {
                return 1; // Error allocating memory
            }

            (*pvec)[0] = p3;
            (*pvec)[1] = p5;
            (*pvec)[2] = p6;
            (*pvec)[3] = p7;
        }
        return 0;
    }

    if(p7.ve<p.ve){ //! Sample as down.
        for(T i=as; i>0; i-=(0.1*as)){ //! Sampling 10%.
            t5(p.vo,p.acs,i,&p5);

            if(p.ace>0){
                t7(p5.ve,p5.ace,0,&p7);
                t1(p7.ve,0,p.ace,&p1);
                p7.ve=p1.ve;
            } else {
                t7(p5.ve,p5.ace,p.ace,&p7);
            }

            if(p7.ve>p.ve){

                delta_v=p7.ve-p.ve;

                t5(p.vo,p.acs,i,&p5);
                t6(p5.ve,p5.ve-delta_v,p5.ace,&p6);

                if(p.ace>0){
                    t7(p6.ve,p6.ace,0,&p7);
                    t1(p7.ve,0,p.ace,&p1);

                    *size=5;
                    *pvec = malloc(*size * sizeof(struct sc_period));
                    if (*pvec == NULL) {
                        return 1; // Error allocating memory
                    }

                    (*pvec)[0] = p3;
                    (*pvec)[1] = p5;
                    (*pvec)[2] = p6;
                    (*pvec)[3] = p7;
                    (*pvec)[4] = p1;

                } else {
                    t7(p6.ve,p6.ace,p.ace,&p7);

                    *size=4;
                    *pvec = malloc(*size * sizeof(struct sc_period));
                    if (*pvec == NULL) {
                        return 1; // Error allocating memory
                    }

                    (*pvec)[0] = p3;
                    (*pvec)[1] = p5;
                    (*pvec)[2] = p6;
                    (*pvec)[3] = p7;
                }
                return 0;
            }
        }
    }
    return 1;
}

I t3_t5_t6_t7_pid(struct sc_period p, struct sc_period **pvec, size_t *size){

    struct sc_period p3,p5,p6,p7,p1;
    T delta_v=0;

    if(p.acs>0){
        t3(p.vo,p.acs,0,&p3);
        p.vo=p3.ve;
        p.acs=0;
    }

    t5(p.vo,p.acs,as,&p5);

    if(p.ace>0){
        t7(p5.ve,p5.ace,0,&p7);
        t1(p7.ve,0,p.ace,&p1);
        p7.ve=p1.ve;
    } else {
        t7(p5.ve,p5.ace,p.ace,&p7);
    }

    if(p7.ve>=p.ve){ //! Need t2.

        delta_v=p7.ve-p.ve;

        t6(p5.ve,p5.ve-delta_v,p5.ace,&p6);

        if(p.ace>0){
            t7(p6.ve,p6.ace,0,&p7);
            t1(p7.ve,0,p.ace,&p1);

            *size=5;
            *pvec = malloc(*size * sizeof(struct sc_period));
            if (*pvec == NULL) {
                return 1; // Error allocating memory
            }

            (*pvec)[0] = p3;
            (*pvec)[1] = p5;
            (*pvec)[2] = p6;
            (*pvec)[3] = p7;
            (*pvec)[4] = p1;

        } else {
            t7(p6.ve,p6.ace,p.ace,&p7);

            *size=4;
            *pvec = malloc(*size * sizeof(struct sc_period));
            if (*pvec == NULL) {
                return 1; // Error allocating memory
            }

            (*pvec)[0] = p3;
            (*pvec)[1] = p5;
            (*pvec)[2] = p6;
            (*pvec)[3] = p7;
        }
        return 0;
    }

    if(p7.ve<p.ve){ //! Sample as down.
        for(T i=as; i>0; i-=(0.1*as)){ //! Sampling 10%.
            t5(p.vo,p.acs,i,&p5);

            if(p.ace>0){
                t7(p5.ve,p5.ace,0,&p7);
                t1(p7.ve,0,p.ace,&p1);
                p7.ve=p1.ve;
            } else {
                t7(p5.ve,p5.ace,p.ace,&p7);
            }

            if(p7.ve>p.ve){

                delta_v=p7.ve-p.ve;

                t5(p.vo,p.acs,i,&p5);
                t6(p5.ve,p5.ve-delta_v,p5.ace,&p6);

                if(p.ace>0){
                    t7(p6.ve,p6.ace,0,&p7);
                    t1(p7.ve,0,p.ace,&p1);

                    *size=5;
                    *pvec = malloc(*size * sizeof(struct sc_period));
                    if (*pvec == NULL) {
                        return 1; // Error allocating memory
                    }

                    (*pvec)[0] = p3;
                    (*pvec)[1] = p5;
                    (*pvec)[2] = p6;
                    (*pvec)[3] = p7;
                    (*pvec)[4] = p1;

                } else {
                    t7(p6.ve,p6.ace,p.ace,&p7);

                    *size=4;
                    *pvec = malloc(*size * sizeof(struct sc_period));
                    if (*pvec == NULL) {
                        return 1; // Error allocating memory
                    }

                    (*pvec)[0] = p3;
                    (*pvec)[1] = p5;
                    (*pvec)[2] = p6;
                    (*pvec)[3] = p7;
                }
                return 0;
            }
        }
    }
    return 1;
}

I process_curve_simple(enum sc_period_id id,
                       T vo,
                       T ve,
                       T acs,
                       T ace,
                       T ncs,
                       T vm,
                       struct sc_period **pvec, size_t size){

    struct sc_period p;
    p.id=id;
    p.vo=vo;
    p.ve=ve;
    p.acs=acs;
    p.ace=ace;
    p.ncs=ncs;

    return process_curve(p, vm, pvec, &size);
}

I process_curve(struct sc_period p,
                T vm,
                struct sc_period **pvec,
                size_t *size){

    if(p.id==id_pause || p.id==id_pause_resume || p.id==id_run){
        // ok
    } else {
        rtapi_print_msg(RTAPI_MSG_ERR,"set process curve id.\n");
    }

    if(a>0 && dv>0){
        // ok
    } else {
        rtapi_print_msg(RTAPI_MSG_ERR,"set a, dv.\n");
    }

    if(vm>0){
        // ok
    } else {
        rtapi_print_msg(RTAPI_MSG_ERR,"vm is zero.\n");
    }

    p.ncs=fabs(p.ncs); //! Ensure positive input.

    //! Pause requests:
    if(p.id==id_pause){
        if(p.vo>p.ve){

            struct sc_period p_in=p;
            p_in.id=id_none;
            p_in.ve=0;
            p_in.ace=0;

            t3_t5_t6_t7_t1_pvec(p_in,pvec,size);

            return 0;
        }
        if(p.vo<p.ve){

            struct sc_period p_in=p;
            p_in.id=id_none;
            p_in.ve=0;
            p_in.ace=0;

            t7_t1_t2_t3_t5_pvec(p_in,pvec,size);

            return 0;
        }
        if(p.vo==p.ve){
            struct sc_period p4;
            t4(p.vo,0,&p4);

            *size=1;
            *pvec = malloc(*size * sizeof(struct sc_period));
            if (*pvec == NULL) {
                return 1; // Error allocating memory
            }
            (*pvec)[0]=p4;

            return 0;
        }
    }

    //! Pause resume requests:
    if(p.id==id_pause_resume){
        if(p.vo>p.ve){

            struct sc_period p_in=p;
            p_in.id=id_none;
            p_in.vo=0;
            p_in.acs=0;

            t3_t5_t6_t7_t1_pvec(p_in,pvec,size);

            return 0;
        }
        if(p.vo<p.ve){

            struct sc_period p_in=p;
            p_in.id=id_none;
            p_in.vo=0;
            p_in.acs=0;

            t7_t1_t2_t3_t5_pvec(p_in,pvec,size);

            return 0;
        }
        if(p.vo==p.ve){
            struct sc_period p4;
            t4(p.vo,0,&p4);

            *size=1;
            *pvec = malloc(*size * sizeof(struct sc_period));
            if (*pvec == NULL) {
                return 1; // Error allocating memory
            }
            (*pvec)[0]=p4;

            return 0;
        }
    }

    if(p.id==id_run){

        //! The most common motion curve.
        if(p.vo<vm && p.ve<vm){

            struct sc_period *vec_1, *vec_2, *vec_3;
            size_t size_vec_1, size_vec_2, size_vec_3;
            struct sc_period p4;
            T stot=0;

            struct sc_period p_in=p;
            p_in.id=id_none;
            p_in.ve=vm;
            p_in.ace=0;

            t7_t1_t2_t3_t5_pvec(p_in,&vec_1,&size_vec_1);

            p_in=p;
            p_in.id=id_none;
            p_in.vo=vm;
            p_in.acs=0;

            t3_t5_t6_t7_t1_pvec(p_in,&vec_3,&size_vec_3);

            stot=to_stot_pvec(vec_1,size_vec_1)+to_stot_pvec(vec_3,size_vec_3);

            if(p.ncs==stot){
                append_to_pvec(pvec,size,vec_1,size_vec_1);
                append_to_pvec(pvec,size,vec_3,size_vec_3);

                //free(vec_1);
                //free(vec_3);
                //cleanup_periods(&vec_1,&size_vec_1);
                //cleanup_periods(&vec_3,&size_vec_3);
                return 0;
            }
            if(p.ncs>stot){ //! Need t4.
                append_to_pvec(pvec,size,vec_1,size_vec_1);

                t4(vm,p.ncs-stot,&p4);

                size_vec_2=1;
                vec_2 = malloc(size_vec_2 * sizeof(struct sc_period));
                if (vec_2 == NULL) {
                    return 1; // Error allocating memory
                }
                (vec_2)[0]=p4;
                append_to_pvec(pvec,size,vec_2,size_vec_2);

                append_to_pvec(pvec,size,vec_3,size_vec_3);

                //free(vec_1);
               // free(vec_2);
                //free(vec_3);
               // cleanup_periods(&vec_1,&size_vec_1);
                //cleanup_periods(&vec_2,&size_vec_2);
                //cleanup_periods(&vec_3,&size_vec_3);

                return 0;
            }

            if(p.ncs<stot){ //! Sample vm down, add t4 to fit s.
                for(T i=vm; i>fmin(p.vo,p.ve); i-=0.01*vm){ //! Sampling 10%.

                    p_in=p;
                    p_in.id=id_none;
                    p_in.ve=i;
                    p_in.ace=0;

                    t7_t1_t2_t3_t5_pvec(p_in,&vec_1,&size_vec_1);

                    p_in=p;
                    p_in.id=id_none;
                    p_in.vo=i;
                    p_in.acs=0;

                    t3_t5_t6_t7_t1_pvec(p_in,&vec_3,&size_vec_3);

                    stot=to_stot_pvec(vec_1,size_vec_1)+to_stot_pvec(vec_3,size_vec_3);

                    if(stot<p.ncs){

                        t4(i,p.ncs-stot,&p4);

                        append_to_pvec(pvec,size,vec_1,size_vec_1);

                        size_vec_2=1;
                        vec_2 = malloc(size_vec_2 * sizeof(struct sc_period));
                        if (vec_2 == NULL) {
                            return 1; // Error allocating memory
                        }
                        (vec_2)[0]=p4;
                        append_to_pvec(pvec,size,vec_2,size_vec_2);

                        append_to_pvec(pvec,size,vec_3,size_vec_3);

                        //free(vec_1);
                        //free(vec_2);
                        //free(vec_3);
                       // cleanup_periods(&vec_1,&size_vec_1);
                       // cleanup_periods(&vec_2,&size_vec_2);
                        //cleanup_periods(&vec_3,&size_vec_3);

                        return 0;
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

            struct sc_period *vec_1, *vec_2, *vec_3;
            size_t size_vec_1, size_vec_2, size_vec_3;
            struct sc_period p4;
            T stot=0;

            struct sc_period p_in=p;
            p_in.id=id_none;
            p_in.ve=vm;
            p_in.ace=0;

            t3_t5_t6_t7_t1_pvec(p_in,&vec_1,&size_vec_1);

            p_in=p;
            p_in.id=id_none;
            p_in.vo=vm;
            p_in.acs=0;

            t7_t1_t2_t3_t5_pvec(p_in,&vec_3,&size_vec_3);

            stot=to_stot_pvec(vec_1,size_vec_1)+to_stot_pvec(vec_3,size_vec_3);

            if(p.ncs==stot){

                append_to_pvec(pvec,size,vec_1,size_vec_1);
                append_to_pvec(pvec,size,vec_3,size_vec_3);

               // free(vec_1);
               // free(vec_3);
               // cleanup_periods(&vec_1,&size_vec_1);
               // cleanup_periods(&vec_3,&size_vec_3);

                return 0;
            }
            if(p.ncs>stot){ //! Need t4.

                append_to_pvec(pvec,size,vec_1,size_vec_1);

                t4(vm,p.ncs-stot,&p4);
                size_vec_2=1;
                vec_2 = malloc(size_vec_2 * sizeof(struct sc_period));
                if (vec_2 == NULL) {
                    return 1; // Error allocating memory
                }
                (vec_2)[0]=p4;
                append_to_pvec(pvec,size,vec_2,size_vec_2);

                append_to_pvec(pvec,size,vec_3,size_vec_3);

                //free(vec_1);
                //free(vec_2);
                //free(vec_3);
                //cleanup_periods(&vec_1,&size_vec_1);
                //cleanup_periods(&vec_2,&size_vec_2);
               // cleanup_periods(&vec_3,&size_vec_3);

                return 0;
            }

            if(p.ncs<stot){ //! Sample vm up, add t4 to fit s.
                for(T i=vm; i<fmin(p.vo,p.ve); i+=0.01*vm){ //! Sampling 10%.

                    struct sc_period p_in=p;
                    p_in.id=id_none;
                    p_in.ve=i;
                    p_in.ace=0;

                    t3_t5_t6_t7_t1_pvec(p_in,&vec_1,&size_vec_1);

                    p_in=p;
                    p_in.id=id_none;
                    p_in.vo=i;
                    p_in.acs=0;

                    t7_t1_t2_t3_t5_pvec(p_in,&vec_3,&size_vec_3);

                    stot=to_stot_pvec(vec_1,size_vec_1)+to_stot_pvec(vec_3,size_vec_3);

                    if(stot<=p.ncs){

                        t4(i,p.ncs-stot,&p4);

                        append_to_pvec(pvec,size,vec_1,size_vec_1);

                        size_vec_2=1;
                        vec_2 = malloc(size_vec_2 * sizeof(struct sc_period));
                        if (vec_2 == NULL) {
                            return 1; // Error allocating memory
                        }
                        (vec_2)[0]=p4;
                        append_to_pvec(pvec,size,vec_2,size_vec_2);

                        append_to_pvec(pvec,size,vec_3,size_vec_3);

                        //free(vec_1);
                        //free(vec_2);
                        //ree(vec_3);
                       // cleanup_periods(&vec_1,&size_vec_1);
                       // cleanup_periods(&vec_2,&size_vec_2);
                       // cleanup_periods(&vec_3,&size_vec_3);

                        return 0;
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

            struct sc_period *vec_1, *vec_2;
            size_t size_vec_1, size_vec_2;
            T s1=0;

            struct sc_period p_in=p;
            p_in.id=id_none;
            p_in.ve=vm;
            p_in.ace=0;

            t7_t1_t2_t3_t5_pvec(p_in,&vec_1,&size_vec_1);

            p_in=p;
            p_in.id=id_none;
            p_in.vo=vm;
            p_in.acs=0;

            t4_ace(p_in,&vec_2,&size_vec_2);

            s1=p.ncs-to_stot_pvec(vec_1,size_vec_1);

            if(s1>0){ //! Steady period to fit s.

                p_in=p;
                p_in.id=id_none;
                p_in.vo=vm;
                p_in.acs=0;
                p_in.ncs=s1;

                t4_ace(p_in,&vec_2,&size_vec_2);

                append_to_pvec(pvec,size,vec_1,size_vec_1);
                append_to_pvec(pvec,size,vec_2,size_vec_2);

                //free(vec_1);
                //free(vec_2);
               // cleanup_periods(&vec_1,&size_vec_1);
               // cleanup_periods(&vec_2,&size_vec_2);
                return 0;
            }

            //! minimal curve.
            append_to_pvec(pvec,size,vec_1,size_vec_1);
            append_to_pvec(pvec,size,vec_2,size_vec_2);

            //free(vec_1);
            //free(vec_2);
            //cleanup_periods(&vec_1,&size_vec_1);
            //cleanup_periods(&vec_2,&size_vec_2);

            return 0;
        }

        if(p.vo>vm && p.ve==vm){

            struct sc_period *vec_1, *vec_2;
            size_t size_vec_1, size_vec_2;
            T s1=0;

            struct sc_period p_in=p;
            p_in.id=id_none;
            p_in.ve=vm;
            p_in.ace=0;

            t3_t5_t6_t7_t1_pvec(p_in,&vec_1,&size_vec_1);

            p_in=p;
            p_in.id=id_none;
            p_in.vo=vm;
            p_in.acs=0;

            t4_ace(p_in,&vec_2,&size_vec_2);

            s1=p.ncs-to_stot_pvec(vec_1,size_vec_1);

            if(s1>0){ //! Steady period to fit s.

                p_in=p;
                p_in.id=id_none;
                p_in.vo=vm;
                p_in.acs=0;
                p_in.ncs=s1;

                t4_ace(p_in,&vec_2,&size_vec_2);

                append_to_pvec(pvec,size,vec_1,size_vec_1);
                append_to_pvec(pvec,size,vec_2,size_vec_2);

                //free(vec_1);
                //free(vec_2);
               // cleanup_periods(&vec_1,&size_vec_1);
               // cleanup_periods(&vec_2,&size_vec_2);
                return 0;
            }

            //! minimal curve.
            append_to_pvec(pvec,size,vec_1,size_vec_1);
            append_to_pvec(pvec,size,vec_2,size_vec_2);

           // free(vec_1);
           // free(vec_2);
           // cleanup_periods(&vec_1,&size_vec_1);
           // cleanup_periods(&vec_2,&size_vec_2);

            return 0;
        }

        if(p.vo==vm && p.ve<vm){

            struct sc_period *vec_1, *vec_2;
            size_t size_vec_1, size_vec_2;
            T s2=0;

            struct sc_period p_in=p;
            p_in.id=id_none;
            p_in.ve=vm;
            p_in.ace=0;

            t4_acs(p_in,&vec_1,&size_vec_1);

            p_in=p;
            p_in.id=id_none;
            p_in.acs=0;

            t3_t5_t6_t7_t1_pvec(p_in,&vec_2,&size_vec_2);

            s2=p.ncs-to_stot_pvec(vec_2,size_vec_2);

            if(s2>0){ //! Steady period to fit s.

                struct sc_period p_in=p;
                p_in.id=id_none;
                p_in.ve=vm;
                p_in.ace=0;
                p_in.ncs=s2;

                t4_acs(p_in,&vec_1,&size_vec_1);
                append_to_pvec(pvec,size,vec_1,size_vec_1);
                append_to_pvec(pvec,size,vec_2,size_vec_2);

                free(vec_1);
                free(vec_2);
                return 0;
            }

            //! minimal curve.
            append_to_pvec(pvec,size,vec_1,size_vec_1);
            append_to_pvec(pvec,size,vec_2,size_vec_2);

            //free(vec_1);
            //free(vec_2);
           // cleanup_periods(&vec_1,&size_vec_1);
            //cleanup_periods(&vec_2,&size_vec_2);

            return 0;
        }

        if(p.vo==vm && p.ve>vm){

            struct sc_period *vec_1, *vec_2;
            size_t size_vec_1, size_vec_2;
            T s2=0;

            struct sc_period p_in=p;
            p_in.id=id_none;
            p_in.ve=vm;
            p_in.ace=0;

            t4_acs(p_in,&vec_1,&size_vec_1);

            p_in=p;
            p_in.id=id_none;
            p_in.acs=0;

            t7_t1_t2_t3_t5_pvec(p_in,&vec_2,&size_vec_2);

            s2=p.ncs-to_stot_pvec(vec_2,size_vec_2);

            if(s2>0){ //! Steady period to fit s.
                struct sc_period p_in=p;
                p_in.id=id_none;
                p_in.ve=vm;
                p_in.ace=0;
                p_in.ncs=s2;

                t4_acs(p_in,&vec_1,&size_vec_1);
                append_to_pvec(pvec,size,vec_1,size_vec_1);
                append_to_pvec(pvec,size,vec_2,size_vec_2);

                //free(vec_1);
                //free(vec_2);
               // cleanup_periods(&vec_1,&size_vec_1);
               // cleanup_periods(&vec_2,&size_vec_2);
                return 0;
            }

            //! minimal curve.
            append_to_pvec(pvec,size,vec_1,size_vec_1);
            append_to_pvec(pvec,size,vec_2,size_vec_2);

            //free(vec_1);
            //free(vec_2);
            //cleanup_periods(&vec_1,&size_vec_1);
           // cleanup_periods(&vec_2,&size_vec_2);

            return 0;
        }

        if(p.vo<vm && p.ve>vm){ // Stay below vm.

            struct sc_period *vec_1, *vec_2, *vec_3;
            size_t size_vec_1, size_vec_2, size_vec_3;
            struct sc_period p4;
            T stot=0;

            struct sc_period p_in=p;
            p_in.id=id_none;
            p_in.ve=vm;
            p_in.ace=0;

            t7_t1_t2_t3_t5_pvec(p_in,&vec_1,&size_vec_1);

            p_in=p;
            p_in.id=id_none;
            p_in.vo=vm;
            p_in.acs=0;

            t7_t1_t2_t3_t5_pvec(p_in,&vec_3,&size_vec_3);

            stot=to_stot_pvec(vec_1,size_vec_1)+to_stot_pvec(vec_3,size_vec_3);

            if(p.ncs==stot){
                append_to_pvec(pvec,size,vec_1,size_vec_1);
                append_to_pvec(pvec,size,vec_3,size_vec_3);

                //free(vec_1);
                //free(vec_3);

               // cleanup_periods(&vec_1,&size_vec_1);
               // cleanup_periods(&vec_3,&size_vec_3);

                return 0;
            }
            if(p.ncs>stot){ //! Need t4.

                append_to_pvec(pvec,size,vec_1,size_vec_1);

                t4(vm,p.ncs-stot,&p4);
                size_vec_2=1;
                vec_2 = malloc(size_vec_2 * sizeof(struct sc_period));
                if (vec_2 == NULL) {
                    return 1; // Error allocating memory
                }
                (vec_2)[0]=p4;
                append_to_pvec(pvec,size,vec_2,size_vec_2);

                append_to_pvec(pvec,size,vec_3,size_vec_3);

                // free(vec_1);
                // free(vec_2);
                // free(vec_3);

               // cleanup_periods(&vec_1,&size_vec_1);
               // cleanup_periods(&vec_2,&size_vec_2);
               // cleanup_periods(&vec_3,&size_vec_3);

                return 0;
            }
            if(p.ncs<stot){ //! Sample vm down, add t4 to fit s.
                for(T i=vm; i>p.vo; i-=0.1*vm){ //! Sampling 10%.

                    struct sc_period p_in=p;
                    p_in.id=id_none;
                    p_in.ve=vm;
                    p_in.ace=0;

                    t7_t1_t2_t3_t5_pvec(p_in,&vec_1,&size_vec_1);

                    p_in=p;
                    p_in.id=id_none;
                    p_in.vo=vm;
                    p_in.acs=0;

                    t7_t1_t2_t3_t5_pvec(p_in,&vec_3,&size_vec_3);

                    stot=to_stot_pvec(vec_1,size_vec_1)+to_stot_pvec(vec_3,size_vec_3);

                    if(stot<=p.ncs){

                        t4(i,p.ncs-stot,&p4);

                        append_to_pvec(pvec,size,vec_1,size_vec_1);

                        size_vec_2=1;
                        vec_2 = malloc(size_vec_2 * sizeof(struct sc_period));
                        if (vec_2 == NULL) {
                            return 1; // Error allocating memory
                        }
                        (vec_2)[0]=p4;
                        append_to_pvec(pvec,size,vec_2,size_vec_2);

                        append_to_pvec(pvec,size,vec_3,size_vec_3);

                        //free(vec_1);
                        //free(vec_2);
                        //free(vec_3);
                        //cleanup_periods(&vec_1,&size_vec_1);
                        //cleanup_periods(&vec_2,&size_vec_2);
                        //cleanup_periods(&vec_3,&size_vec_3);

                        return 0;
                    }
                }
            }

            //! Minimal curve.
            append_to_pvec(pvec,size,vec_1,size_vec_1);
            append_to_pvec(pvec,size,vec_3,size_vec_3);

            //free(vec_1);
            //free(vec_3);
           // cleanup_periods(&vec_1,&size_vec_1);
           // cleanup_periods(&vec_3,&size_vec_3);

            return 0;
        }

        if(p.vo>vm && p.ve<vm){

            struct sc_period *vec_1, *vec_2, *vec_3;
            size_t size_vec_1, size_vec_2, size_vec_3;
            struct sc_period p4;
            T stot=0;

            struct sc_period p_in=p;
            p_in.id=id_none;
            p_in.ve=vm;
            p_in.ace=0;

            t3_t5_t6_t7_t1_pvec(p_in,&vec_1,&size_vec_1);

            p_in=p;
            p_in.id=id_none;
            p_in.vo=vm;
            p_in.acs=0;

            t3_t5_t6_t7_t1_pvec(p_in,&vec_3,&size_vec_3);

            stot=to_stot_pvec(vec_1,size_vec_1)+to_stot_pvec(vec_3,size_vec_3);

            if(p.ncs==stot){
                append_to_pvec(pvec,size,vec_1,size_vec_1);
                append_to_pvec(pvec,size,vec_3,size_vec_3);

                //cleanup_periods(&vec_1,&size_vec_1);
                //cleanup_periods(&vec_3,&size_vec_3);
                //free(vec_1);
                //free(vec_3);

                return 0;
            }
            if(p.ncs>stot){ //! Need t4.

                append_to_pvec(pvec,size,vec_1,size_vec_1);

                t4(vm,p.ncs-stot,&p4);
                size_vec_2=1;
                vec_2 = malloc(size_vec_2 * sizeof(struct sc_period));
                if (vec_2 == NULL) {
                    return 1; // Error allocating memory
                }
                (vec_2)[0]=p4;
                append_to_pvec(pvec,size,vec_2,size_vec_2);

                append_to_pvec(pvec,size,vec_3,size_vec_3);

                //cleanup_periods(&vec_1,&size_vec_1);
                //cleanup_periods(&vec_2,&size_vec_2);
                //cleanup_periods(&vec_3,&size_vec_3);
                // free(vec_1);
                // free(vec_2);
                // free(vec_3);

                return 0;
            }
            if(p.ncs<stot){ //! Sample vm down, add t4 to fit s.
                for(T i=vm; i>fmax(p.vo,p.ve); i-=0.1*vm){ //! Sampling 10%.

                    struct sc_period p_in=p;
                    p_in.id=id_none;
                    p_in.ve=i;
                    p_in.ace=0;

                    t3_t5_t6_t7_t1_pvec(p_in,&vec_1,&size_vec_1);

                    p_in=p;
                    p_in.id=id_none;
                    p_in.vo=i;
                    p_in.acs=0;

                    t3_t5_t6_t7_t1_pvec(p_in,&vec_3,&size_vec_3);

                    stot=to_stot_pvec(vec_1,size_vec_1)+to_stot_pvec(vec_3,size_vec_3);

                    if(stot<=p.ncs){

                        t4(i,p.ncs-stot,&p4);

                        append_to_pvec(pvec,size,vec_1,size_vec_1);

                        size_vec_2=1;
                        vec_2 = malloc(size_vec_2 * sizeof(struct sc_period));
                        if (vec_2 == NULL) {
                            return 1; // Error allocating memory
                        }
                        (vec_2)[0]=p4;
                        append_to_pvec(pvec,size,vec_2,size_vec_2);

                        append_to_pvec(pvec,size,vec_3,size_vec_3);

                        //cleanup_periods(&vec_1,&size_vec_1);
                        //cleanup_periods(&vec_2,&size_vec_2);
                        //cleanup_periods(&vec_3,&size_vec_3);
                        // free(vec_1);
                        // free(vec_2);
                        // free(vec_3);

                        return 0;
                    }
                }
            }

            //! Minimal curve.
            append_to_pvec(pvec,size,vec_1,size_vec_1);
            append_to_pvec(pvec,size,vec_3,size_vec_3);

            // free(vec_1);
            // free(vec_3);
            ///cleanup_periods(&vec_1,&size_vec_1);
            //cleanup_periods(&vec_3,&size_vec_3);

            return 0;
        }

        if(p.vo==vm && p.ve==vm){
            struct sc_period *vec_1;
            size_t size_vec_1;
            struct sc_period pr;

            t4(p.vo,p.ncs,&pr);

            size_vec_1=1;
            vec_1 = malloc(size_vec_1 * sizeof(struct sc_period));
            if (vec_1 == NULL) {
                return 1; // Error allocating memory
            }
            (vec_1)[0]=pr;
            append_to_pvec(pvec,size,vec_1,size_vec_1);

            //free(vec_1);
            //cleanup_periods(&vec_1,&size_vec_1);

            return 0;
        }
    }
    return 1;
}

int append_to_pvec(struct sc_period **pvec, size_t *size, struct sc_period *vec_1, size_t size_1) {
    // allocate memory for the combined vectors
    struct sc_period *new_vec = (struct sc_period*) malloc((*size + size_1) * sizeof(struct sc_period));
    if (new_vec == NULL) {
        // error allocating memory
        return 1;
    }

    // copy elements from the original vector to the new vector
    for (size_t i = 0; i < *size; i++) {
        new_vec[i] = (*pvec)[i];
    }

    // copy elements from vec_1 to the new vector
    for (size_t i = 0; i < size_1; i++) {
        new_vec[*size + i] = vec_1[i];
    }

    // update the size and pointer of the original vector
    *size += size_1;
    *pvec = new_vec;

    return 0;
}

V cleanup_periods(struct sc_period **pvec, size_t size) {
    for (size_t i = 0; i < size; i++) {
        free(pvec[i]);
    }
    *pvec = NULL;
    size = 0;
}




























