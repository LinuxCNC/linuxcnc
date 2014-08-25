#define _GNU_SOURCE     /* To get defns of NI_MAXSERV and NI_MAXHOST */
#include <arpa/inet.h>
#include <net/if.h>
#include <sys/socket.h>
#include <netdb.h>
#include <ifaddrs.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <linux/if_link.h>

unsigned int if_nametoindex(const char *ifname);

#include "select_interface.h"


int select_interface(int npref, const char **pref, char *ifname,
		     char *ip, unsigned int *ifindex)
{
    struct ifaddrs *ifaddr, *ifa;
    int family, s, n,i;
    char host[NI_MAXHOST];
    const char *p;

    if (getifaddrs(&ifaddr) == -1) {
	perror("getifaddrs");
	return -ENOENT;
    }
    for (i = 0; i < npref; i++) {
	p = pref[i];

	// printf("pref '%s'\n", p);

	for (ifa = ifaddr, n = 0; ifa != NULL; ifa = ifa->ifa_next, n++) {
	    if (ifa->ifa_addr == NULL) // only ifs with address
		continue;
	    family = ifa->ifa_addr->sa_family;
	    if (family != AF_INET)	    // IPv4 only for now
		continue;
	    /* if (ifa->ifa_flags & IFF_LOOPBACK) // skip loopback interface */
	    /* 	continue; */

	    s = getnameinfo(ifa->ifa_addr,
			    (family == AF_INET) ? sizeof(struct sockaddr_in) :
			    sizeof(struct sockaddr_in6),
			    host, NI_MAXHOST,
			    NULL, 0, NI_NUMERICHOST);
	    if (s != 0) {
		fprintf(stderr, "getnameinfo() failed: %s\n", gai_strerror(s));
		goto fail;
	    }
	    if (!strncmp(ifa->ifa_name, p, strlen(p))) {

		// printf("MATCH %s address: %s\n", ifa->ifa_name, host);
		unsigned int ii = if_nametoindex(ifa->ifa_name);
		if (ii) {
		    if (ifindex)
			*ifindex = ii;
		} else {
		    fprintf(stderr, "if_nametoindex() failed: %s\n", strerror(errno));
		}
		strcpy(ifname, ifa->ifa_name);
		strcpy(ip, host);
		freeifaddrs(ifaddr);
		return 0;
	    }

	}
    }
 fail:
    freeifaddrs(ifaddr);
    return -ENOENT;
}

#define MAX_IFS 100
int parse_interface_prefs(const char *line,  char *ifname, char *ip, unsigned int *ifindex)
{
    const char *argv[MAX_IFS];
    int argc = 0;

    char *s, *iniline = strdup(line);

    // strip trailing comments
    if ((s = strchr(iniline, '#')) != NULL) {
	*s = '\0';
    }
    // split into tokens
    s = strtok((char *) iniline, " \t");

    while( s != NULL && argc < MAX_IFS - 1) {
	argv[argc++] = s;
	s = strtok( NULL, " \t" );
    }
    if (argc == MAX_IFS) {
	fprintf(stderr, "parse_interface_prefs: too many arguments (max %d)", MAX_IFS);
	free(iniline);
	return -1;
    }
    argv[argc] = NULL;
    return select_interface(argc, argv, ifname, ip, ifindex);
}


#ifdef TEST
int main(int argc, const char *argv[])
{
    char ifname[100],ipv4[100];
    memset(ifname, 0, 100);
    memset(ipv4, 0, 100);

    int retval =  select_interface(argc-1, &argv[1],ifname, ipv4, int *ifindex);

    printf("rc=%d if %s ip %s\n",retval, ifname, ipv4);
    exit(EXIT_SUCCESS);
}
#endif
