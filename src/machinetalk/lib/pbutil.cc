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
send_pbcontainer(const std::string &dest, machinetalk::Container &c, void *socket)
{
    int retval = 0;
    zmsg_t *msg = zmsg_new();
    zframe_t *d =  zframe_new (dest.c_str(), dest.size());
    zmsg_append(msg, &d);
    retval = send_pbcontainer(msg, c, socket);
    zmsg_destroy(&msg);
    return retval;
}

// send_pbcontainer: destination can contain multiple routing points
int
send_pbcontainer(zmsg_t *dest, machinetalk::Container &c, void *socket)
{
    int retval = 0;
    zframe_t *f;
    size_t nsize = zmsg_size(dest);

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

    for (size_t i = 0; i < nsize; ++i)
    {
        zframe_t *f = zmsg_pop (dest); 
        if (zframe_size(f)) {
            retval = zframe_send (&f, socket, ZMQ_MORE);
            if (retval) {
                std::string str( (const char *) zframe_data(f), zframe_size(f));
                syslog_async(LOG_ERR,"%s: FATAL - failed to send destination frame: '%.*s'",
                             __func__, str.size(), str.c_str());
                goto DONE;
            }
        }
        zframe_destroy(&f);
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
note_printf(machinetalk::Container &c, const char *fmt, ...)
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
