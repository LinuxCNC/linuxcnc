#ifndef PIN_HH
#define PIN_HH

#include "hal.h"
#include "hal_priv.h"
#include "hal_accessor.h"
#include <stdarg.h>

using namespace std;

class Pin
{

public:
    void *operator new(size_t size)    { throw; };
    void operator delete(void *p)      { throw; };
    void *operator new[](size_t size)  { throw; }
    void operator delete[](void *p)    { throw; };

    Pin(hal_pin_t *p) :  pin(p) {};
    Pin(const hal_pin_dir_t dir, const int owner_id, hal_s32_t defval, const char *fmt, ...) {
	hal_data_u u;
	u._s = defval;
	va_list ap;
	va_start(ap, fmt);
	pin = halg_pin_newfv(1, HAL_S32, dir, NULL, owner_id, u, fmt, ap);
	va_end(ap);
    }

    // conversions
    inline operator hal_s32_t() const { return  _get_s32_pin(pin); }

    // compound assignment +=
    inline Pin operator+=(const Pin &rhs) {
    	_incr_s32_pin(this->pin, _get_s32_pin(rhs.pin));
    	return *this;
    }
    inline Pin operator+=(const hal_s32_t& value) {
	_incr_s32_pin(this->pin, value);
	return *this;
    };
    // inline Pin operator+=(const int& value) {
    // 	return this->pin += (hal_s32_t) value;
    // };

    // compound logic
    inline Pin operator|=(const uint32_t& value) {
	if (unlikely(hh_get_rmb(&this->pin->hdr)))
	    rtapi_smp_rmb();
	hal_data_u *u = (hal_data_u *)hal_ptr(this->pin->data_ptr);
#ifdef HAVE_CK
  	ck_pr_or_32((uint32_t *)&u->_u, value);
#else
        __atomic_or_fetch ((uint32_t *)&u->_u, value, RTAPI_MEMORY_MODEL);
#endif
	if (unlikely(hh_get_wmb(&this->pin->hdr)))
	    rtapi_smp_wmb();
    	return *this;
    };

    //unary minus
    inline hal_s32_t operator-() {
    	return -_get_s32_pin(this->pin);
    }


    inline hal_s32_t operator+(const Pin &rhs) {
    	return _get_s32_pin(this->pin) + _get_s32_pin(rhs.pin);
    }
    inline hal_s32_t operator+(const hal_s32_t &rhs) {
	return _get_s32_pin(this->pin) + rhs;
    }
    // Pin operator-(const Pin &);
    // Pin operator*(const Pin &);
    // Pin operator/(const Pin &);

    // not needed, hal_s32_t conversion takes care of that
    // //overloaded relational operators
    // bool operator>(Pin &rhs) const  {
    // 	return _get_s32_pin(this->pin) - _get_s32_pin(rhs.pin) > 0;
    // }
    // bool operator>=(Pin &rhs) const  {
    // 	return _get_s32_pin(this->pin) - _get_s32_pin(rhs.pin) >= 0;
    // }
    // bool operator<(Pin &rhs) const  {
    // 	return _get_s32_pin(this->pin) < _get_s32_pin(rhs.pin);
    // }
    // bool operator<=(Pin &rhs) const  {
    // 	return _get_s32_pin(this->pin) <= _get_s32_pin(rhs.pin);
    // }
    // bool operator==(Pin &rhs) const  {
    // 	return _get_s32_pin(this->pin) == _get_s32_pin(rhs.pin);
    // }
    // bool operator!=(Pin &rhs) const  {
    // 	return _get_s32_pin(this->pin) != _get_s32_pin(rhs.pin);
    // }

private:
    hal_pin_t *pin;
};

#endif
