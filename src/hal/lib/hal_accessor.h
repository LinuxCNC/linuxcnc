#ifndef  HAL_ACCESSOR_H
#define  HAL_ACCESSOR_H

#include "config.h"  // HAVE_CK
#include "rtapi.h"
#include "rtapi_atomics.h"

RTAPI_BEGIN_DECLS

// see https://gcc.gnu.org/onlinedocs/gcc/_005f_005fatomic-Builtins.html

// NB these setters/getters work for V2 pins only which use hal_pin_t.data_ptr,
// instead of the legacy hal_pin_t.data_ptr_addr and hal_malloc()'d
// <haltype>*
// this means atomics+barrier support is possible only with V2 pins (!).

static inline void *hal_ptr(const shmoff_t offset) {
    return ((char *)hal_shmem_base + offset);
}
static inline shmoff_t hal_off(const void *p) {
    return ((char *)p - (char *)hal_shmem_base);
}
static inline shmoff_t hal_off_safe(const void *p) {
    if (p == NULL) return 0;
    return ((char *)p - (char *)hal_shmem_base);
}

void hal_typefailure(const char *file,
		     const int line,
		     const int object_type,
		     const int value_type);

#ifndef FAIL_STOP
#define FAIL_STOP(file, line, otype, expected)				\
    do {								\
	hal_typefailure(file, line, otype, expected);			\
    } while (0)
#endif

#if defined(CHECK_ACCESSOR_TYPE)
// optionally compiled-in runtime check on type compatibility
// when using raw descriptors
#define _CHECK(otype, vtype)			\
    if (otype != vtype)		\
	FAIL_STOP(__FILE__, __LINE__, otype, vtype);
#else
#define _CHECK(otype, vtype)
#endif

// conditional barriers
#define COND_WB(OBJ) if (unlikely(hh_get_wmb(&OBJ->hdr))) rtapi_smp_wmb()
#define COND_RB(OBJ) if (unlikely(hh_get_rmb(&OBJ->hdr))) rtapi_smp_rmb()

#ifdef HAVE_CK

// use concurrencykit.org primitives
#define BITCAST    (uint8_t *)
#define S32CAST    (uint32_t *)
#define U32CAST    (uint32_t *)
#define U64CAST    (uint64_t *)
#define S64CAST    (uint64_t *)
#define FLOATCAST  (double *)

#define _SETVALUE8( OBJ, TAG, VALUE, CAST)			\
    ck_pr_store_8(CAST &u->TAG, VALUE);					\
    COND_WB(OBJ)

#define _GETVALUE8( OBJ, TAG,  CAST)					\
    COND_RB(OBJ);							\
    hal_data_u rv;							\
    rv.TAG = ck_pr_load_8(CAST &u->TAG);				\
    return rv.TAG

#define _SETVALUE32( OBJ, TAG, VALUE,  CAST)				\
    ck_pr_store_32(CAST &u->TAG, VALUE);				\
    COND_WB(OBJ)

#define _GETVALUE32( OBJ, TAG, CAST)					\
    COND_RB(OBJ);							\
    hal_data_u rv;							\
    rv.TAG = ck_pr_load_32(CAST &u->TAG);				\
    return rv.TAG							\

#if defined(CK_F_PR_STORE_64)
#define _SETVALUE64( OBJ, TAG, VALUE,  CAST)				\
    ck_pr_store_64(CAST &u->TAG, VALUE);				\
    COND_WB(OBJ)

#define _SETVALUEDOUBLE( OBJ, TAG, VALUE,  CAST)	\
    ck_pr_store_double(CAST &u->TAG, VALUE);		\
    COND_WB(OBJ)

#else
#define _SETVALUE64( OBJ, TAG, VALUE,  CAST)				\
    __atomic_store(&u->TAG, &VALUE, RTAPI_MEMORY_MODEL);		\
    COND_WB(OBJ)

#define _SETVALUEDOUBLE _SETVALUE64
#endif

#if defined(CK_F_PR_LOAD_64)
#define _GETVALUE64( OBJ, TAG,  CAST)					\
    COND_RB(OBJ);							\
    hal_data_u rv;							\
    ck_pr_load_64(CAST &u->TAG);					\
    return rv.TAG

#define _GETVALUEDOUBLE( OBJ, TAG,  CAST)				\
    COND_RB(OBJ);							\
    return ck_pr_load_double(CAST &u->TAG);

#else
#define _GETVALUE64( OBJ, TAG, CAST)					\
    COND_RB(OBJ);							\
    hal_data_u rv;							\
    __atomic_load(&u->TAG, &rv.TAG, RTAPI_MEMORY_MODEL);		\
    return rv.TAG

#define _GETVALUEDOUBLE _GETVALUE64
#endif

#else // gcc intrinsics

#define BITCAST
#define S32CAST
#define U32CAST
#define S64CAST
#define U64CAST
#define FLOATCAST

#define _SETVALUE64( OBJ, TAG, VALUE,  CAST)				\
    __atomic_store(&u->TAG, &VALUE, RTAPI_MEMORY_MODEL);		\
    COND_WB(OBJ)

#define _GETVALUE64( OBJ, TAG,  CAST)					\
    COND_RB(OBJ);							\
    hal_data_u rv;							\
    __atomic_load(&u->TAG, &rv.TAG, RTAPI_MEMORY_MODEL);		\
    return rv.TAG

#define _SETVALUE8       _SETVALUE64
#define _SETVALUE32      _SETVALUE64
#define _SETVALUEDOUBLE  _SETVALUE64

#define _GETVALUE8       _GETVALUE64
#define _GETVALUE32      _GETVALUE64
#define _GETVALUEDOUBLE  _GETVALUE64

#endif

// export context-independent setters which are strongly typed,
// and context-dependent accessors with a descriptor argument,
// and an optional runtime type check
#define PINSETTER(SETTER,  TYPE, OTYPE, LETTER, ACCESS,  CAST)	\
									\
    static inline const hal_##TYPE##_t					\
    _set_##TYPE##_pin(hal_pin_t *pin,					\
		      const hal_##TYPE##_t value) {			\
    hal_data_u *u =							\
	(hal_data_u *)hal_ptr(pin->data_ptr);				\
    _CHECK(pin_type(pin), OTYPE);					\
    SETTER( pin, ACCESS, value,  CAST);				\
    return value;							\
    }									\
									\
    static inline const hal_##TYPE##_t					\
    set_##TYPE##_pin(TYPE##_pin_ptr p,					\
		     const hal_##TYPE##_t value) {			\
	return  _set_##TYPE##_pin((hal_pin_t *)hal_ptr(p._##LETTER##p),	\
			      value);					\
    }


// emit typed pin setters
PINSETTER(_SETVALUE8, bit,   HAL_BIT,   b,   _b,     BITCAST);
PINSETTER(_SETVALUE32, s32,   HAL_S32,   s,   _s,     S32CAST);
PINSETTER(_SETVALUE32, u32,   HAL_U32,   u,   _u,     U32CAST);
PINSETTER(_SETVALUE64, u64,   HAL_U64,   lu,  _lu,    U64CAST);
PINSETTER(_SETVALUE64, s64,   HAL_S64,   ls,  _ls,    S64CAST);
PINSETTER(_SETVALUEDOUBLE, float, HAL_FLOAT, f,   _f,   FLOATCAST);


#define PINGETTER(GETTER,  TYPE, OTYPE, LETTER, ACCESS,  CAST)	\
    static inline const hal_##TYPE##_t					\
    _get_##TYPE##_pin(const hal_pin_t *pin) {				\
	const hal_data_u *u =						\
	    (const hal_data_u *)hal_ptr(pin->data_ptr);			\
	_CHECK(pin_type(pin), OTYPE)					\
	    GETTER( pin, _##LETTER, CAST);		\
    }									\
									\
    static inline const hal_##TYPE##_t					\
    get_##TYPE##_pin(const TYPE##_pin_ptr p) {				\
	return _get_##TYPE##_pin((const hal_pin_t *)hal_ptr(p._##LETTER##p)); \
    }

// emit typed pin getters
PINGETTER(_GETVALUE8,      bit,   HAL_BIT,   b,   _b,  BITCAST);
PINGETTER(_GETVALUE32,     s32,   HAL_S32,   s,   _s,  S32CAST);
PINGETTER(_GETVALUE32,     u32,   HAL_U32,   u,   _u,  U32CAST);
PINGETTER(_GETVALUE64,     u64,   HAL_U64,   lu,  _lu, U64CAST);
PINGETTER(_GETVALUE64,     s64,   HAL_S64,   ls,  _ls, S64CAST);
PINGETTER(_GETVALUEDOUBLE, float, HAL_FLOAT, f,   _f,  FLOATCAST);

// atomically increment a value (integral types only)
// unclear how to do the equivalent of an __atomic_add_fetch
// with ck, so use gcc intrinsics for now:
#define _INCREMENT(U, DESC, TYPE, TAG, VALUE)				\
    TYPE rvalue = __atomic_add_fetch(&U->TAG, VALUE,			\
				     RTAPI_MEMORY_MODEL);		\
    if (unlikely(hh_get_wmb(&DESC->hdr)))				\
	rtapi_smp_wmb();						\
    return rvalue;

#define PIN_INCREMENTER(type, tag)					\
    static inline const hal_##type##_t					\
	 incr_##type##_pin(type##_pin_ptr p,				\
			   const hal_##type##_t value) {		\
        hal_pin_t *pin = (hal_pin_t *)hal_ptr(p._##tag##p);		\
	hal_data_u *u = (hal_data_u*)hal_ptr(pin->data_ptr);		\
	_INCREMENT(u, pin, hal_##type##_t,  _##tag, value);		\
    }									\
									\
    static inline const hal_##type##_t					\
    _incr_##type##_pin(hal_pin_t *pin,					\
		       const hal_##type##_t value) {			\
	hal_data_u *u = (hal_data_u*)hal_ptr(pin->data_ptr);		\
	_INCREMENT(u, pin, hal_##type##_t,  _##tag, value)		\
    }

// typed pin incrementers
PIN_INCREMENTER(s32, s)
PIN_INCREMENTER(u32, u)


// signal getters
#define SIGGETTER(GETTER,  TYPE, OTYPE, LETTER, ACCESS,  CAST)	\
    static inline const hal_##TYPE##_t					\
    _get_##TYPE##_sig(const hal_sig_t *sig) {				\
	_CHECK(sig_type(sig), OTYPE);					\
	hal_data_u *u = (hal_data_u*)&sig->value;			\
	GETTER( sig, _##LETTER, CAST);			\
    }									\
									\
    static inline const hal_##TYPE##_t					\
    get_##TYPE##_sig(const TYPE##_sig_ptr s) {				\
    return  _get_##TYPE##_sig((const hal_sig_t *)hal_ptr(s._##LETTER##s)); \
    }



// emit typed signal getters
SIGGETTER(_GETVALUE8,      bit,   HAL_BIT,   b,   _b,   BITCAST);
SIGGETTER(_GETVALUE32,     s32,   HAL_S32,   s,   _s,   S32CAST);
SIGGETTER(_GETVALUE32,     u32,   HAL_U32,   u,   _u,   U32CAST);
SIGGETTER(_GETVALUE64,     u64,   HAL_U64,   lu,  _lu,  U64CAST);
SIGGETTER(_GETVALUE64,     s64,   HAL_S64,   ls,  _ls,  S64CAST);
SIGGETTER(_GETVALUEDOUBLE, float, HAL_FLOAT, f,   _f,   FLOATCAST);



#define SIGSETTER(SETTER,  TYPE, OTYPE, LETTER, ACCESS,  CAST)	\
    static inline const hal_##TYPE##_t					\
    _set_##TYPE##_sig(hal_sig_t *sig,					\
		      const hal_##TYPE##_t value) {			\
	hal_data_u *u = &sig->value;					\
	_CHECK(sig_type(sig), OTYPE);					\
	SETTER( sig, ACCESS, value,  CAST);			\
	return value;							\
    }									\
									\
    static inline const hal_##TYPE##_t					\
    set_##TYPE##_sig(TYPE##_sig_ptr s,					\
		     const hal_##TYPE##_t value) {			\
	return  _set_##TYPE##_sig((hal_sig_t *)hal_ptr(s._##LETTER##s),	\
				  value);				\
    }


// emit typed signal setters
SIGSETTER(_SETVALUE8,      bit,   HAL_BIT,   b,   _b,   BITCAST);
SIGSETTER(_SETVALUE32,     s32,   HAL_S32,   s,   _s,   S32CAST);
SIGSETTER(_SETVALUE32,     u32,   HAL_U32,   u,   _u,   U32CAST);
SIGSETTER(_SETVALUE64,     u64,   HAL_U64,   lu,  _lu,  U64CAST);
SIGSETTER(_SETVALUE64,     s64,   HAL_S64,   ls,  _ls,  S64CAST);
SIGSETTER(_SETVALUEDOUBLE, float, HAL_FLOAT, f,   _f,   FLOATCAST);

// typed NULL tests for pins and signals
#define PINNULL(TYPE, FIELD)						\
    static inline bool TYPE##_pin_null(const TYPE##_pin_ptr p) {	\
	return p.FIELD == 0;						\
}
PINNULL(bit,  _bp)
PINNULL(s32,  _sp)
PINNULL(u32,  _up)
PINNULL(u64,  _lup)
PINNULL(s64,  _lsp)
PINNULL(float,_fp)

#define SIGNULL(TYPE, FIELD)						\
    static inline bool TYPE##_sig_null(const TYPE##_sig_ptr s) {	\
	return s.FIELD == 0;						\
}
SIGNULL(bit,  _bs)
SIGNULL(s32,  _ss)
SIGNULL(u32,  _us)
SIGNULL(u64,  _lus)
SIGNULL(s64,  _lss)
SIGNULL(float,_fs)


// convert hal type to string
const char *hals_type(const hal_type_t type);

// convert hal_data_u to string
int hals_value(char *buffer,
	       const size_t s,
	       const hal_type_t type,
	       const hal_data_u *u);

// convert pin direction to string
const char *hals_pindir(const hal_pin_dir_t dir);


// test if dir is in [HAL_IN, HAL_OUT, HAL_IO]
const int hal_valid_dir(const hal_pin_dir_t dir);

// test if dir is in [HAL_BIT,...]
const int hal_valid_type(const hal_type_t type);


// pin allocators, in hal_accessor.c
bit_pin_ptr halx_pin_bit_newf(const hal_pin_dir_t dir,
			      const int owner_id,
			      const char *fmt, ...)
    __attribute__((format(printf,3,4)));

float_pin_ptr halx_pin_float_newf(const hal_pin_dir_t dir,
				  const int owner_id,
				  const char *fmt, ...)
    __attribute__((format(printf,3,4)));

u32_pin_ptr halx_pin_u32_newf(const hal_pin_dir_t dir,
			      const int owner_id,
			      const char *fmt, ...)
    __attribute__((format(printf,3,4)));

s32_pin_ptr halx_pin_s32_newf(const hal_pin_dir_t dir,
			      const int owner_id,
			      const char *fmt, ...)
    __attribute__((format(printf,3,4)));

u64_pin_ptr halx_pin_u64_newf(const hal_pin_dir_t dir,
			      const int owner_id,
			      const char *fmt, ...)
    __attribute__((format(printf,3,4)));

s64_pin_ptr halx_pin_s64_newf(const hal_pin_dir_t dir,
			      const int owner_id,
			      const char *fmt, ...)
    __attribute__((format(printf,3,4)));

// as above, but with explicit default value as 3rd argument
bit_pin_ptr halxd_pin_bit_newf(const hal_pin_dir_t dir,
			       const int owner_id,
			       const hal_bit_t defval,
			       const char *fmt, ...)
    __attribute__((format(printf,4,5)));

float_pin_ptr halxd_pin_float_newf(const hal_pin_dir_t dir,
				   const int owner_id,
				   const hal_float_t defval,
				   const char *fmt, ...)
    __attribute__((format(printf,4,5)));

u32_pin_ptr halxd_pin_u32_newf(const hal_pin_dir_t dir,
			       const int owner_id,
			       const hal_u32_t defval,
			       const char *fmt, ...)
    __attribute__((format(printf,4,5)));

s32_pin_ptr halxd_pin_s32_newf(const hal_pin_dir_t dir,
			       const int owner_id,
			       const hal_s32_t defval,
			       const char *fmt, ...)
    __attribute__((format(printf,4,5)));

u64_pin_ptr halxd_pin_u64_newf(const hal_pin_dir_t dir,
			       const int owner_id,
			       const hal_u64_t defval,
			       const char *fmt, ...)
    __attribute__((format(printf,4,5)));

s64_pin_ptr halxd_pin_s64_newf(const hal_pin_dir_t dir,
			       const int owner_id,
			       const hal_s64_t defval,
			       const char *fmt, ...)
    __attribute__((format(printf,4,5)));

RTAPI_END_DECLS
#endif // HAL_ACCESSOR_H
