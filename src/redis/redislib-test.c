#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include "redislib.h"
#include "redis-datamodel.h"
#define PI      3.14159265

const char *testtext = "lorem ipsum dolor etcetc";

int main(int argc, char**argv)
{
    int  ival;
    double dval;
    char buffer[1000];

    const char *inifile = NULL;
    int rc;
    if (argc > 1)
	inifile = argv[1];


    assert(redis_init(inifile) == 0);
    assert(redis_cmd("SET text %s", testtext) == 0);
    assert(redis_cmd("SET pi %6.10g",  PI) == 0);
    redis_cmd("DEL foo");
    assert(redis_cmd("INCRBY foo 1000") == 1000);

    rc = redis_get_int(&ival, "GET foo");
    // printf("get foo: rc=%d ival=%d\n", rc, ival);
    assert(rc == 0);
    assert(ival == 1000);

    assert(redis_get_double(&dval, "GET pi") == 0);
    // printf("get pi: rc=%d dval=%f\n", rc, dval);

    assert(dval == PI);

    assert(redis_get_string(buffer, sizeof buffer, "GET text") == 0);
    assert(strcmp(testtext,buffer) == 0);

    REDIS_SET_TOOL(47, 0.0, 1.0, 2.0, 3.0, 4.0, 5.0, 6.0,7.0, 8.0,
		   2.54, 9.0, 10.0, 5);

    // retrieve with redis-cli: HGETALL tool:47

    assert(redis_close() == 0);

    return 0;
}
