#ifndef _SELECT_INTERFACE_H
#define _SELECT_INTERFACE_H

#ifdef __cplusplus
extern "C" {
#endif

//     given an interface preference list, return interface, IPv4, and 0
//     or < 0 if no match found
//     if nonzero, the interface index will be stored in ifindex
//
//     If an interface has several IPv4 addresses, the first one is picked.
//     pref is a list of interface names or prefixes:
//
//   pref = ['eth0','usb3']
//     or
//  pref = ['wlan','eth', 'usb']
//
    int select_interface(int npref, const char **pref, char *ifname,
			 char *ip, unsigned int *ifindex);


// parse a preference string like
//
// INTERFACES= eth1 eth0 wlan usb2
//
// and find a matching interface with an ipv4 address
// return 0 on success, < 0 on failure
//
int parse_interface_prefs(const char *line,  char *ifname,
			  char *ip, unsigned int *ifindex);

#ifdef __cplusplus
}
#endif
#endif
