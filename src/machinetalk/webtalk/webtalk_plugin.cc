
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <libwebsockets.h>

#include "webtalk.hh"
#include <dlfcn.h>

int wt_add_plugin(wtself_t *self, const char *sopath)
{
    dlerror(); // clear any existing error

    // try to open the plugin shared object
    void *libhandle = dlopen(sopath, RTLD_NOW);
    if (!libhandle) {
	const char *errmsg = dlerror();
	if (!errmsg)
	    errmsg = strerror(errno);
	syslog_async(LOG_ERR, "%s: error loading plugin DSO %s: %s\n",
		     __func__, sopath, errmsg);
	return -EINVAL;
    }

    // resolve address of descriptor structure
    zwspolicy_t *p = (zwspolicy_t *)  dlsym(libhandle, "proxy_policy");
    if (p == NULL) {
	syslog_async(LOG_ERR,
		     "%s: error resolving symbol 'proxy_policy' in %s: %s\n",
		     __func__,sopath, dlerror());
	return -ENOENT;
    }
    syslog_async(LOG_INFO, "%s: adding policy '%s' from plugin '%s'\n",
		 __func__, p->name, sopath);
    int retval = wt_proxy_add_policy(self, p->name, p->callback);
    assert(retval == 0);
    return 0;
}
