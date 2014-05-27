
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <libwebsockets.h>

#include <google/protobuf/text_format.h>
#include <machinetalk/generated/message.pb.h>
using namespace google::protobuf;

#include "webtalk.hh"
#include <dlfcn.h>

int wt_add_plugin(wtself_t *self, const char *sopath)
{
    dlerror(); // clear any existing error

    // Attempt to open the plugin DSO
    void *libhandle = dlopen(sopath, RTLD_NOW);
    if (!libhandle) {
	const char *errmsg = dlerror();
	if (!errmsg)
	    errmsg = strerror(errno);
	syslog_async(LOG_ERR, "%s: error loading plugin DSU %s: %s\n",
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
    syslog_async(LOG_INFO, "%s: found policy descriptor in %s: '%s' callback=%p\n",
		 __func__,  sopath, p->name, p->callback);
    int retval = wt_proxy_add_policy(self, p->name, p->callback);
    assert(retval == 0);
    return 0;
}
