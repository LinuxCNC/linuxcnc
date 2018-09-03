#include "config.h"
#include "hal.h"
#include "hal_priv.h"
#include "hal_accessor.h"
#include "hal_internal.h"

// allocators for accessor-style pins
// passing NULL for data_ptr_addr in halg_pin_newfv makes them v2 pins

bit_pin_ptr halx_pin_bit_newf(const hal_pin_dir_t dir,
			      const int owner_id,
			      const char *fmt, ...)
{
    va_list ap;
    bit_pin_ptr p;
    hal_data_u defval;
    memset((void *)&defval, 0, sizeof(defval));

    va_start(ap, fmt);
    p._bp = hal_off_safe(halg_pin_newfv(1, HAL_BIT, dir, NULL,
					owner_id, defval, fmt, ap));
    va_end(ap);
    return p;
}

float_pin_ptr halx_pin_float_newf(const hal_pin_dir_t dir,
				  const int owner_id,
				  const char *fmt, ...)
{
    va_list ap;
    float_pin_ptr p;
    hal_data_u defval;
    memset((void *)&defval, 0, sizeof(defval));
    va_start(ap, fmt);
    p._fp = hal_off_safe(halg_pin_newfv(1, HAL_FLOAT, dir, NULL,
					owner_id, defval, fmt, ap));
    va_end(ap);
    return p;
}

u32_pin_ptr halx_pin_u32_newf(const hal_pin_dir_t dir,
		      const int owner_id,
		      const char *fmt, ...)
{
    va_list ap;
    u32_pin_ptr p;
    hal_data_u defval;
    memset((void *)&defval, 0, sizeof(defval));

    va_start(ap, fmt);
    p._up = hal_off_safe(halg_pin_newfv(1, HAL_U32, dir, NULL,
					owner_id, defval, fmt, ap));
    va_end(ap);
    return p;
}

s32_pin_ptr halx_pin_s32_newf(const hal_pin_dir_t dir,
		      const int owner_id,
		      const char *fmt, ...)
{
    va_list ap;
    s32_pin_ptr p;
    hal_data_u defval;
    memset((void *)&defval, 0, sizeof(defval));

    va_start(ap, fmt);
    p._sp = hal_off_safe(halg_pin_newfv(1,HAL_S32, dir, NULL,
					owner_id, defval, fmt, ap));
    va_end(ap);
    return p;
}

u64_pin_ptr halx_pin_u64_newf(const hal_pin_dir_t dir,
		      const int owner_id,
		      const char *fmt, ...)
{
    va_list ap;
    u64_pin_ptr p;
    hal_data_u defval;
    memset((void *)&defval, 0, sizeof(defval));

    va_start(ap, fmt);
    p._lup = hal_off_safe(halg_pin_newfv(1, HAL_U64, dir, NULL,
					owner_id, defval, fmt, ap));
    va_end(ap);
    return p;
}

s64_pin_ptr halx_pin_s64_newf(const hal_pin_dir_t dir,
		      const int owner_id,
		      const char *fmt, ...)
{
    va_list ap;
    s64_pin_ptr p;
    hal_data_u defval;
    memset((void *)&defval, 0, sizeof(defval));

    va_start(ap, fmt);
    p._lsp = hal_off_safe(halg_pin_newfv(1,HAL_S64, dir, NULL,
					owner_id, defval, fmt, ap));
    va_end(ap);
    return p;
}

// default value versions

bit_pin_ptr halxd_pin_bit_newf(const hal_pin_dir_t dir,
			       const int owner_id,
			       const hal_bit_t def,
			       const char *fmt, ...)
{
    va_list ap;
    bit_pin_ptr p;
    hal_data_u defval = {._b = def};
    va_start(ap, fmt);
    p._bp = hal_off_safe(halg_pin_newfv(1, HAL_BIT, dir, NULL,
					owner_id, defval, fmt, ap));
    va_end(ap);
    return p;
}

float_pin_ptr halxd_pin_float_newf(const hal_pin_dir_t dir,
				   const int owner_id,
				   const hal_float_t def,
				   const char *fmt, ...)
{
    va_list ap;
    float_pin_ptr p;
    hal_data_u defval = {._f = def};
    va_start(ap, fmt);
    p._fp = hal_off_safe(halg_pin_newfv(1, HAL_FLOAT, dir, NULL,
					owner_id, defval, fmt, ap));
    va_end(ap);
    return p;
}

u32_pin_ptr halxd_pin_u32_newf(const hal_pin_dir_t dir,
			       const int owner_id,
			       const hal_u32_t def,
			       const char *fmt, ...)
{
    va_list ap;
    u32_pin_ptr p;
    hal_data_u defval = {._u = def};
    va_start(ap, fmt);
    p._up = hal_off_safe(halg_pin_newfv(1, HAL_U32, dir, NULL,
					owner_id, defval, fmt, ap));
    va_end(ap);
    return p;
}

s32_pin_ptr halxd_pin_s32_newf(const hal_pin_dir_t dir,
			       const int owner_id,
			       const hal_s32_t def,
			       const char *fmt, ...)
{
    va_list ap;
    s32_pin_ptr p;
    hal_data_u defval = {._s = def};
    va_start(ap, fmt);
    p._sp = hal_off_safe(halg_pin_newfv(1,HAL_S32, dir, NULL,
					owner_id, defval, fmt, ap));
    va_end(ap);
    return p;
}


u64_pin_ptr halxd_pin_u64_newf(const hal_pin_dir_t dir,
			       const int owner_id,
			       const hal_u64_t def,
			       const char *fmt, ...)
{
    va_list ap;
    u64_pin_ptr p;
    hal_data_u defval = {._lu = def};
    va_start(ap, fmt);
    p._lup = hal_off_safe(halg_pin_newfv(1, HAL_U64, dir, NULL,
					owner_id, defval, fmt, ap));
    va_end(ap);
    return p;
}

s64_pin_ptr halxd_pin_s64_newf(const hal_pin_dir_t dir,
			       const int owner_id,
			       const hal_s64_t def,
			       const char *fmt, ...)
{
    va_list ap;
    s64_pin_ptr p;
    hal_data_u defval = {._ls = def};
    va_start(ap, fmt);
    p._lsp = hal_off_safe(halg_pin_newfv(1,HAL_S64, dir, NULL,
					owner_id, defval, fmt, ap));
    va_end(ap);
    return p;
}


const char *hals_pindir(const hal_pin_dir_t dir)
{
    switch (dir) {
    case HAL_IN:
	return "IN";
    case HAL_OUT:
	return "OUT";
    case HAL_IO:
	return "I/O";
    default:
	return "*invalid*";
    }
}

const char *hals_type(const hal_type_t type)
{
    switch (type) {
    case HAL_BIT:
	return "bit";
    case HAL_FLOAT:
	return "float";
    case HAL_S32:
	return "s32";
    case HAL_U32:
	return "u32";
    case HAL_S64:
	return "s64";
    case HAL_U64:
	return "u64";
    default:
	return "*invalid*";
    }
}

const int hal_valid_type(const hal_type_t type)
{
    switch (type) {
    case HAL_BIT:
    case HAL_FLOAT:
    case HAL_S32:
    case HAL_U32:
    case HAL_S64:
    case HAL_U64:
	return 1;
    default:
	return 0;
    }
}

const int hal_valid_dir(const hal_pin_dir_t dir)
{
    switch (dir) {
    case HAL_IN:
    case HAL_OUT:
    case HAL_IO:
	return 1;
    default:
	return 0;
    }
}

int hals_value(char *buffer,
	       const size_t s,
	       const hal_type_t type,
	       const hal_data_u *u)
{
    switch (type) {
    case HAL_BIT:
	return rtapi_snprintf(buffer, s, "%s", u->_b ? "true" : "false");
    case HAL_FLOAT:
	return rtapi_snprintf(buffer, s, "%f", u->_f);
    case HAL_S32:
	return rtapi_snprintf(buffer, s, "%d", u->_s);
    case HAL_U32:
	return rtapi_snprintf(buffer, s, "%u", u->_u);
    case HAL_S64:
	return rtapi_snprintf(buffer, s, "%lld", (long long) u->_ls);
    case HAL_U64:
	return rtapi_snprintf(buffer, s, "%llu", (unsigned long long) u->_lu);
    default:
	HALFAIL_RC(EINVAL, "invalid type %d", type);
    }
}


void hal_typefailure(const char *file,
		     const int line,
		     const int object_type,
		     const int value_type)
{
    rtapi_print_msg(RTAPI_MSG_ERR,
		    "%s:%d TYPE VIOLATION: object type=%s value type=%s",
		    file,
		    line,
		    hals_type(object_type),
		    hals_type(value_type));
}
