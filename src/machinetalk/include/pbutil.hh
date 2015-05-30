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

#ifndef _PBUTIL_HH_INCLUDED
#define _PBUTIL_HH_INCLUDED

#include <machinetalk/generated/message.pb.h>
#include <string>

// for repeated string field creation (Container.note, Container.argv)
typedef ::google::protobuf::RepeatedPtrField< ::std::string> pbstringarray_t;

// send a protobuf - encoded Container message
// optionally prepend destination field
// log any failure to RTAPI
int send_pbcontainer(const std::string &dest, pb::Container &c, void *socket);

// add an printf-formatted string to the 'note' repeated string in a
// Container
// also log to syslog
// the a string longer than MAX_NOTESIZE-4 will be truncated to
// MAX_NOTESIZE-4 and the string "..." appended, indicating truncation
#define MAX_NOTESIZE 4096
int
note_printf(pb::Container &c, const char *fmt, ...);

// fold a RepeatedPtrField into a std::string, separated by delim
std::string pbconcat(const pbstringarray_t &args, const std::string &delim = " ", const std::string &quote = "");

#endif // _PBUTIL_HH_INCLUDED
