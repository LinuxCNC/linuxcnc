/* Minimal GMI client — a compile/link check against the gomc control-API
 * client library (libgmi).  This is the gomc replacement for the classic
 * nml-position-logger build test, which linked the now-removed NML client
 * libs (-lnml -llinuxcnc).  Like the classic test, this is built to /dev/null
 * and never executed; it only verifies that an external program can compile
 * and link against the shipped client library and its public headers. */
#include "gmi_http.h"
#include <stddef.h>

int main(void)
{
    gmi_http_t *http = gmi_http_new("http://127.0.0.1:5080/api/v1");
    if (http == NULL)
        return 1;
    gmi_request_t *req = gmi_request_new(http, GMI_HTTP_GET, "/pins");
    gmi_request_free(req);
    gmi_http_free(http);
    return 0;
}
