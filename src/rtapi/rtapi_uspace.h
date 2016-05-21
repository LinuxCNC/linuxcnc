#ifndef RTAPI_USPACE_H
#define RTAPI_USPACE_H

/*    This is a component of LinuxCNC
 *    Copyright 2014 Jeff Epler <jepler@unpythonic.net>
 *
 *    This program is free software; you can redistribute it and/or modify
 *    it under the terms of the GNU General Public License as published by
 *    the Free Software Foundation; either version 2 of the License, or
 *    (at your option) any later version.
 *
 *    This program is distributed in the hope that it will be useful,
 *    but WITHOUT ANY WARRANTY; without even the implied warranty of
 *    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *    GNU General Public License for more details.
 *
 *    You should have received a copy of the GNU General Public License
 *    along with this program; if not, write to the Free Software
 *    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

/*
 * This header file contains declarations of rtapi APIs only available with
 * uspace realtime.
 */

#include "rtapi.h"
RTAPI_BEGIN_DECLS
void rtapi_request_temporary_root();
void rtapi_release_temporary_root();
int rtapi_open_as_root(const char *filename, int mode);
RTAPI_END_DECLS

#endif
