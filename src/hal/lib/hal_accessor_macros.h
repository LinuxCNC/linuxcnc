#ifndef  HAL_ACCESSOR_MACROS_H
#define  HAL_ACCESSOR_MACROS_H

#include <hal_accessor.h>

/***************************************************
A set of shorthand macros for use in components
which utilise the multicore SMP safe  pin_ptr's
arceye-15012017
***************************************************/

#undef gb
#define gb(pn1)  get_bit_pin(pn1)
#undef gs
#define gs(pn1)  get_s32_pin(pn1)
#undef gu
#define gu(pn1)  get_u32_pin(pn1)
#undef gf
#define gf(pn1)  get_float_pin(pn1)
#undef sb
#define sb(pn1, pn2)  set_bit_pin(pn1, pn2)
#undef ss
#define ss(pn1, pn2)  set_s32_pin(pn1, pn2)
#undef su
#define su(pn1, pn2)  set_u32_pin(pn1, pn2)
#undef sf
#define sf(pn1, pn2)  set_float_pin(pn1, pn2)
#undef incs
#define incs(pn1)  ss(pn1, (gs(pn1) + 1))
#undef decs
#define decs(pn1)  ss(pn1, (gs(pn1) - 1))
#undef incu
#define incu(pn1)  su(pn1, (gu(pn1) + 1))
#undef decu
#define decu(pn1)  su(pn1, (gu(pn1) - 1))

#endif
