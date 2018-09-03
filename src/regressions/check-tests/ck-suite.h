#ifndef _TEST_CK_SUITE
#define _TEST_CK_SUITE


#include <ck_ring.h>


// not really a test, just trying to get the hang of how to use ck_ring
// with a fixed-size struct

#define CK_NSLOTS 16  // CK_NSLOTS -1 usable

struct foo {
    int32_t serial;
    int32_t value;
};


// defines ck_ring_enqueue_spsc_foo() etc inlines
CK_RING_PROTOTYPE(foo, foo);

static	ck_ring_t ring;
static size_t ringsize = CK_NSLOTS;


START_TEST(test_ck)
{
    // ck_ring invariants
    ck_assert_int_eq((ringsize & (ringsize - 1)), 0);
    ck_assert_int_gt(ringsize, 4);
    printf("sizeof ck_ring_t=%zu\n", sizeof(ck_ring_t));

    void *b = calloc(ringsize, sizeof(struct foo));
    ck_assert_ptr_ne(b, NULL);

    struct foo *buf = (struct foo *) b;
    ck_ring_init(&ring, ringsize);

    //
    ck_assert_int_eq(ck_ring_capacity(&ring), ringsize);

    int i;
    struct foo f;
    for (i = 0; i < ringsize; i++) {
	f.serial = i;
	f.value = i;
	// last enqueue must fail
	ck_assert(ck_ring_enqueue_spsc_foo(&ring, buf, &f) == (i < (ringsize-1)));
    }

    // there must be ringsize-1 slots in use:
    ck_assert_int_eq(ck_ring_size(&ring), ringsize-1);

    for (i = 0; i < ringsize; i++) {
	struct foo f2;
	bool result = ck_ring_dequeue_spsc_foo(&ring, buf, &f2);

	// last dequeue must fail
	ck_assert(result == (i < (ringsize-1)));
	if (!result)
	    break;

	ck_assert_int_eq(f2.serial, i);
	ck_assert_int_eq(f2.value, i);
    }
    ck_assert_int_eq(ck_ring_size(&ring), 0);
}
END_TEST


Suite * ck_suite(void)
{
    Suite *s;
    TCase *tc_core;
    s = suite_create("ck exploration");

    tc_core = tcase_create("ck");
    tcase_add_test(tc_core, test_ck);
    suite_add_tcase(s, tc_core);

    return s;
}


#endif /* _TEST_CK_SUITE */
#if 0
__attribute__((unused)) static _Bool ck_ring_enqueue_spsc_size_foo(struct ck_ring *a, struct foo *b, struct foo *c, unsigned int *d)
{
    return _ck_ring_enqueue_spsc_size(a, b, c, sizeof(struct foo), d);
}
__attribute__((unused)) static _Bool ck_ring_enqueue_spsc_foo(struct ck_ring *a, struct foo *b, struct foo *c)
{
    return _ck_ring_enqueue_spsc(a, b, c, sizeof(struct foo));
}
__attribute__((unused)) static _Bool ck_ring_dequeue_spsc_foo(struct ck_ring *a, struct foo *b, struct foo *c)
{
    return _ck_ring_dequeue_spsc(a, b, c, sizeof(struct foo));
}
__attribute__((unused)) static _Bool ck_ring_enqueue_spmc_size_foo(struct ck_ring *a, struct foo *b, struct foo *c, unsigned int *d)
{
    return _ck_ring_enqueue_spsc_size(a, b, c, sizeof(struct foo), d);
}
__attribute__((unused)) static _Bool ck_ring_enqueue_spmc_foo(struct ck_ring *a, struct foo *b, struct foo *c)
{
    return _ck_ring_enqueue_spsc(a, b, c, sizeof(struct foo));
}
__attribute__((unused)) static _Bool ck_ring_trydequeue_spmc_foo(struct ck_ring *a, struct foo *b, struct foo *c)
{
    return _ck_ring_trydequeue_spmc(a, b, c, sizeof(struct foo));
}
__attribute__((unused)) static _Bool ck_ring_dequeue_spmc_foo(struct ck_ring *a, struct foo *b, struct foo *c)
{
    return _ck_ring_dequeue_spmc(a, b, c, sizeof(struct foo));
}


#endif
