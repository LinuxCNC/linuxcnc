#include "rtapi.h"

#include "vtexample.h"

// the 'methods exported through the vtable'
static int vte_hello(const char *s)
{
    rtapi_print_msg(RTAPI_MSG_INFO, "vte_hello: %s\n", s);
    return 0;
}

static int vte_goodbye(const char *s)
{
    rtapi_print_msg(RTAPI_MSG_INFO, "vte_goodbye: %s\n", s);
    return 0;
}

// intramodule symbol, referenced in vtexport.c

vtexample_t demo_vtable = {
    vte_hello,
    vte_goodbye
};
