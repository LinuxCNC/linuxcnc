#include "hal.h"
#include "hal_priv.h"
#include "hal_accessor.h"




// "mixed instance data"
struct instdata {
    char blabla[100];
    bit_pin_ptr bin;
    bit_pin_ptr bout;
    s32_pin_ptr sout;
    float_pin_ptr fout;

    float_sig_ptr fsig;
    s32_sig_ptr ssig;

    hal_float_t *floatpin;
};

// use externs so the refs cant be optimized away
extern struct instdata *ip;
extern hal_bit_t b;
extern hal_float_t f;
extern char *hal_shmem_base;

void foo(void)
{
    // very likely in a comp
    b = get_bit_pin(ip->bin); // loc 1 31


    set_s32_pin(ip->sout, 4711);  // loc 1 34

    // a typical JMK pinop:
    *(ip->floatpin) = 3.14;


}
