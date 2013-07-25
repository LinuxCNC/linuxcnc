

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include "rtapi.h"

int main()
{
    int comp_id = rtapi_init("testmod");

    if (comp_id < 0) {
	printf("rtapi_init() failed: %d\n", comp_id);
	exit(1);
    }
    printf("rtapi_init() succeeded\n");
    rtapi_exit(comp_id);
    exit(0);
}
