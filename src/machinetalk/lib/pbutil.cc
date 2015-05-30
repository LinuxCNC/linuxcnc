/*
 * Copyright (C) 2013-2014 Michael Haberler <license@mah.priv.at>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include "pbutil.hh"
#include "syslog_async.h"
#include <czmq.h>
#include <google/protobuf/text_format.h>

// send_pbcontainer: if set, dump container to stderr in TextFormat
int __attribute__((weak)) print_container;

int
send_pbcontainer(const std::string &dest, pb::Container &c, void *socket)
{
    int retval = 0;
    zframe_t *f;

    f = zframe_new(NULL, c.ByteSize());
    if (f == NULL) {
	syslog_async(LOG_ERR,"%s: FATAL - failed to zframe_new(%d)",
			__func__, c.ByteSize());
	return -ENOMEM;
    }
    if (print_container) {
	std::string s;
	google::protobuf::TextFormat::PrintToString(c, &s);
	fprintf(stderr,"%s: %s\n",__func__,s.c_str());
    }
    unsigned char *buf = zframe_data(f);
    unsigned char *end = c.SerializeWithCachedSizesToArray(buf);
    if ((end - buf) == 0) {
	// serialize failed
	syslog_async(LOG_ERR,"%s: FATAL - SerializeWithCachedSizesToArray() failed",
			__func__);
	goto DONE;
    }
    if (dest.size()) {
	zframe_t *d =  zframe_new (dest.c_str(), dest.size());
	retval = zframe_send (&d, socket, ZMQ_MORE);
	if (retval) {
	    syslog_async(LOG_ERR,"%s: FATAL - failed to send destination frame: '%.*s'",
			 __func__, dest.size(), dest.c_str());
	    goto DONE;
	}
    }
    retval = zframe_send(&f, socket, 0);
    if (retval) {
	syslog_async(LOG_ERR,"%s: FATAL - failed to zframe_sendm(%d)",
			    __func__, end-buf);
    }
 DONE:
    c.Clear();
    return retval;
}


int
note_printf(pb::Container &c, const char *fmt, ...)
{
    va_list ap;
    int n;
    char buf[MAX_NOTESIZE];

    va_start(ap, fmt);
    strcpy(&buf[MAX_NOTESIZE] - 4, "...");

    n = vsnprintf(buf, MAX_NOTESIZE-4, fmt, ap);
    if (n > MAX_NOTESIZE-4)
	n = MAX_NOTESIZE;
    va_end(ap);
    c.add_note(buf, n);

    // split into lines to keep syslog_async happy
    char *save, *token, *s = buf;
    while (1) {
	token = strtok_r(s, "\n", &save);
	if (token == NULL)
	    break;
	syslog_async(LOG_ERR, token);
	s = NULL;
    }
    return n;
}

std::string pbconcat(const pbstringarray_t &args, const std::string &delim, const std::string &quote)
{
    std::string s;
    for (int i = 0; i < args.size(); i++) {
	s += quote + args.Get(i) + quote;
	if (i < args.size()-1)
	    s += delim;
    }
    return s;
}
