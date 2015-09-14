#ifndef _TEST_TRIPLE_BUFFER_SUITE
#define _TEST_TRIPLE_BUFFER_SUITE


// see if this works for us:
// http://remis-thoughts.blogspot.co.at/2012/01/triple-buffering-as-concurrency_30.html
#include <triple-buffer.h>

TB_FLAG_FAST(tb);
int tb_buf[3];

START_TEST(test_triple_buffer)
{
    int i;
    rtapi_tb_init(&tb);
    hal_u8_t startup_state = tb;

    // show startup conditions
    ck_assert_int_eq(rtapi_tb_write_idx(&tb), 0);     // current write index=0
    ck_assert_int_eq(rtapi_tb_snap_idx(&tb), 2);      // current snap index=2
    ck_assert_int_eq(rtapi_tb_snapshot(&tb), false);  // nothing to snapshot

    // snapshot does not change the write index:
    ck_assert_int_eq(rtapi_tb_write_idx(&tb), 0);     // current write index still 0

    // an unsuccessful snapshot does not change the snap index:
    ck_assert_int_eq(rtapi_tb_snap_idx(&tb), 2);      // current snap index still 2

    // show cycling through states:
    i = 10;
    while (i--) {
	rtapi_tb_flip(&tb);
	ck_assert_int_eq(rtapi_tb_write_idx(&tb), 1);     // flip: write index 0 -> 1
	ck_assert_int_eq(rtapi_tb_snapshot(&tb), true);   // after a flip snapshot returns true
	ck_assert_int_eq(rtapi_tb_snap_idx(&tb), 0);      // snap index:  2 -> 0

	rtapi_tb_flip(&tb);
	ck_assert_int_eq(rtapi_tb_write_idx(&tb), 2);     // flip: write index 1 -> 2
	ck_assert_int_eq(rtapi_tb_snapshot(&tb), true);   //
	ck_assert_int_eq(rtapi_tb_snap_idx(&tb), 1);      // snap index:  0 -> 1

	rtapi_tb_flip(&tb);
	ck_assert_int_eq(rtapi_tb_write_idx(&tb), 0);     // flip: write index 2 -> 0
	ck_assert_int_eq(rtapi_tb_snapshot(&tb), true);   //
	ck_assert_int_eq(rtapi_tb_snap_idx(&tb), 2);      // snap index:  1 -> 2

	// we've come full circle ;)
	ck_assert_int_eq(tb,startup_state);
    }

    // regressions
    rtapi_tb_init(&tb); // re-init

    ck_assert_int_eq(tb_buf[rtapi_tb_snap_idx(&tb)], 0);
    ck_assert_int_eq(rtapi_tb_snapshot(&tb), false);  // no new data

    /* Test 1 */
    tb_buf[rtapi_tb_write_idx(&tb)] = 3;
    rtapi_tb_flip(&tb);   // commit 3

    ck_assert_int_eq(rtapi_tb_snapshot(&tb), true);      // flipped - new data available
    ck_assert_int_eq(tb_buf[rtapi_tb_snap_idx(&tb)], 3);
    ck_assert_int_eq(rtapi_tb_snapshot(&tb), false);     // no new data

    /* Test 2 */
    tb_buf[rtapi_tb_write_idx(&tb)] = 4;
    rtapi_tb_flip(&tb);                           // commit 4
    tb_buf[rtapi_tb_write_idx(&tb)] = 5;                     // 5 not committed

    ck_assert_int_eq(rtapi_tb_snapshot(&tb), true);      // new data
    ck_assert_int_eq(tb_buf[rtapi_tb_snap_idx(&tb)], 4);     // equals last committed, 4
    rtapi_tb_flip(&tb);                           // commit 5
    ck_assert_int_eq(tb_buf[rtapi_tb_snap_idx(&tb)], 4);     // still 4 since no new snap

    ck_assert_int_eq(rtapi_tb_snapshot(&tb), true);      // new data
    ck_assert_int_eq(tb_buf[rtapi_tb_snap_idx(&tb)], 5);     // must be 5 now since new snap

    rtapi_tb_flip(&tb);
    tb_buf[rtapi_tb_write_idx(&tb)] = 6;                     // 6 but not committed
    rtapi_tb_flip(&tb);
    ck_assert_int_eq(rtapi_tb_snapshot(&tb), true);      // must be new data
    ck_assert_int_eq(tb_buf[rtapi_tb_snap_idx(&tb)], 6);     // must be 6 now since new snap

    tb_buf[rtapi_tb_write_idx(&tb)] = 7;
    rtapi_tb_flip(&tb);
    tb_buf[rtapi_tb_write_idx(&tb)] = 8;
    rtapi_tb_flip(&tb);
    ck_assert_int_eq(tb_buf[rtapi_tb_snap_idx(&tb)], 6); // must be still 6 since no new snap

    tb_buf[rtapi_tb_write_idx(&tb)] = 7;
    rtapi_tb_flip(&tb);
    tb_buf[rtapi_tb_write_idx(&tb)] = 8;
    rtapi_tb_flip(&tb);

    ck_assert_int_eq(rtapi_tb_snapshot(&tb), true);      // must be new data, 8
    ck_assert_int_eq(tb_buf[rtapi_tb_snap_idx(&tb)], 8);

    ck_assert_int_eq(rtapi_tb_snapshot(&tb), false);      // no new data, 8
    ck_assert_int_eq(tb_buf[rtapi_tb_snap_idx(&tb)], 8);
}
END_TEST

#define N_TB_OPS 10000

static bool tb_start;
enum {
    TB_WRITE_FLIP,
    TB_SNAP_READ,

};
struct tb_test {
    hal_u8_t *tbflag;
    int *tb_buf;
    int op;
    int count;
    int value;
    int snapped;
    char *name;
};

static void *test_tbop(void * arg)
{
    struct tb_test *t = arg;
    if (delta) aff_iterate(&a);
    while (!tb_start);
        {
	WITH_THREAD_CPUTIME_N(t->name, t->count, RES_NS);
	while (t->value < t->count) {
	    switch (t->op) {

	    case TB_WRITE_FLIP:
		t->tb_buf[rtapi_tb_write_idx(t->tbflag)] = t->value;
		rtapi_tb_flip(t->tbflag);
		t->value++;
		break;

	    case TB_SNAP_READ:
		if (rtapi_tb_snapshot(t->tbflag)) {
		    t->snapped++;
		    int v = t->tb_buf[rtapi_tb_snap_idx(t->tbflag)];
		    // expect nondecreasing sequence of values
		    ck_assert(v >= t->value);
		    t->value = v;
		}
		break;
	    }
	    if (hop && (t->value % hop == 0))
		aff_iterate(&a);
	}
    }
    return NULL;
}

START_TEST(test_triple_buffer_threaded)
{
    int i;
   struct tb_test tbt[] = {
       {
	   .tbflag = &tb,
	   .tb_buf = tb_buf,
	   .op = TB_WRITE_FLIP,
	   .count =  N_TB_OPS,
	   .value = 0,
	   .name = "write_flip"
       },
       {
	   .tbflag = &tb,
	   .tb_buf = tb_buf,
	   .op = TB_SNAP_READ,
	   .count =  N_TB_OPS-1, // termination condition
	   .value = 0,
	   .snapped = 0,
	   .name = "snap_read"
       },
    };
    rtapi_tb_init(&tb);
    pthread_t tids[2];
    tb_start = false;
    for(i = 0; i < 2; i++){
	pthread_create(&(tids[i]), NULL, test_tbop, &tbt[i]);
    }
    {
	WITH_PROCESS_CPUTIME_N("triple buffer roundtrips", N_TB_OPS , RES_NS);
	tb_start = true;
	for(i = 0; i < 2; i++){
	    pthread_join(tids[i], NULL);
	}
    }
    // last value read must be last value written, flipped and snapped
    ck_assert_int_eq(tbt[0].value - 1, tbt[1].value);
    ck_assert(tbt[1].snapped > 0);
    ck_assert(tbt[1].snapped <=  N_TB_OPS);
    printf("snapped = %d out of %d\n", tbt[1].snapped,  N_TB_OPS);
}
END_TEST

Suite * triple_buffer_suite(void)
{
    Suite *s;
    TCase *tc_core;
    s = suite_create("triple buffer tests");

    tc_core = tcase_create("triple buffer");
    tcase_add_test(tc_core, test_triple_buffer);
    tcase_add_test(tc_core, test_triple_buffer_threaded);
    suite_add_tcase(s, tc_core);

    return s;
}

#endif
