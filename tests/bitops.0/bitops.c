#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <rtapi_bitops.h>

// the symbol RTAPI_USE_ATOMIC is #defined only in the
// new version of rtapi_bitops.h and either 0 or 1, but defined

#ifdef RTAPI_USE_ATOMIC
#define ATOMIC_TYPE rtapi_atomic_type

// make any accidential imports vanish
#undef test_and_set_bit
#undef test_and_clear_bit
#undef set_bit
#undef clear_bit
#undef test_bit

#define test_and_set_bit rtapi_test_and_set_bit
#define test_and_clear_bit rtapi_test_and_clear_bit
#define set_bit rtapi_set_bit
#define clear_bit rtapi_clear_bit
#define test_bit rtapi_test_bit
#define DECLARE_BITMAP RTAPI_DECLARE_BITMAP
#define ZERO_BITMAP RTAPI_ZERO_BITMAP
#else
#define ATOMIC_TYPE unsigned long
#endif

int num_bits_set(ATOMIC_TYPE *map, int size)
{
   int i, cnt = 0;

    for (i = 0; i < size; i++) 
	if (test_bit(i, map))
	    cnt++;
    return cnt;
}

int main()
{

    ATOMIC_TYPE x = 1;

    assert(num_bits_set(&x,32)  == 1);

    ATOMIC_TYPE y = test_and_set_bit(31, &x);
    assert(!y);
    assert(num_bits_set(&x,32)  == 2);

    y = test_and_set_bit(31, &x);
    assert(y);
    assert(num_bits_set(&x,32)  == 2);

    x = 1;
    y = test_and_clear_bit(0, &x);
    assert(y);
    assert(num_bits_set(&x,32)  == 0);

    y = test_and_clear_bit(0, &x);
    assert(!y);
    assert(num_bits_set(&x,32)  == 0);
    y = test_and_clear_bit(1, &x);
    assert(!y);
    assert(num_bits_set(&x,32)  == 0);

    x = 0x80000001;
    assert(test_bit(0, &x));
    assert(num_bits_set(&x,32)  == 2);
    assert(test_bit(31, &x));
    assert(num_bits_set(&x,32)  == 2);
    assert(!test_bit(30, &x));
    assert(num_bits_set(&x,32)  == 2);
    assert(!test_bit(1, &x));
    assert(num_bits_set(&x,32)  == 2);
    assert(x = 0x80000001);

    RTAPI_DECLARE_BITMAP(bitmap, 64);
    RTAPI_ZERO_BITMAP(bitmap, 64);
    set_bit(0, bitmap);
    assert(num_bits_set(bitmap,64)  == 1);
    set_bit(31, bitmap);
    assert(num_bits_set(bitmap,64)  == 2);
    set_bit(32, bitmap);
    assert(num_bits_set(bitmap,64)  == 3);
    set_bit(63, bitmap);
    assert(num_bits_set(bitmap,64)  == 4);

    printf("all tests passed\n");
    return 0;
}

