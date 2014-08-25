/* syslog_async is Copyright (c) 2007 Simon Kelley

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 dated June, 1991, or
   (at your option) version 3 dated 29 June, 2007.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#ifndef _SYSLOG_ASYNC_H
#define _SYSLOG_ASYNC_H 1


#include <syslog.h>
#include <stdarg.h>
#ifdef __cplusplus

#define BEGIN_DECLS extern "C" {
#define END_DECLS }
#else
#define BEGIN_DECLS
#define END_DECLS
#endif

BEGIN_DECLS

/* Syslog_async is a non-blocking replacement for the
   POSIX-standard syslog() system call. Instead of blocking,
   log-lines are buffered in memory. The buffer size is limited
   and if the buffer overflows log lines are lost. When lines are
   lost this fact is logged with a message of the form:

   async_syslog overflow: 5 log entries lost

   In order to limit the probability of buffer overflow
   short delays are added to syslog_async() calls when the
   queue is getting full. The delay added is strictly
   bounded and tunable.

   The API is very close to the standard syslog(), with an
   additional call the tune buffer parameters and a couple
   of calls into the event loop.

   The code has been tested under Linux and BSD, and with both
   the syslog and syslog-ng log daemons.
*/


/*
   openlog_async(), closelog_async() and setlogmask_async() are
   identical to the POSIX equivalents.
*/

void openlog_async(const char *ident, int option, int facility);
void closelog_async(void);
int setlogmask_async(int mask);



/*
   syslog_async() and vsyslog_async() are identical to syslog() and vsyslog()
   except for their blocking behaviour. The formatting is done using printf(),
   so the additional format operator %m is available only if the system
   printf() provides it. (GNU printf() does.)
*/

void syslog_async(int priority, const char *format, ...);
void vsyslog_async(int priority, const char *format, va_list ap);



/*
   log_fd_async() and log_write_async() are the interface between the library
   and the daemon event loop.

   log_fd_async() returns a file descriptor which the library needs to write,
   or -1 if no write is queued. log_write_async() does the write.

   The result of log_fd_async() is only valid until [v]syslog_async() or
   log_write_async() is called, so it should be called each time around the
   event loop, just before the call to select() or poll().

   A typical event loop looks like this:

   while (1)
     {
        int log_fd;
        fd_set read_set, write_set;

	FD_ZERO(&read_set);
	FD_ZERO(&write_set);

        ...other stuff..

	if ((log_fd = log_fd_async()) != -1)
	   FD_SET(log_fd, &write_set);

        select(..., &read_set, &write_set, ...);

	if (log_fd != -1 && FD_ISSET(log_fd, &write))
	   log_write_async();


	...other stuff....
      }
*/

int log_fd_async(void);
void log_write_async(void);



/*
   tunelog_async() tunes the log-line buffer. Backlog is the limit
   on the number of queued log-lines. These are stored in malloc'ed memory
   and each line is stored in a fixed-size buffer which is just over 1K bytes.
   The library maintains a buffer pool to avoid heap fragmentation. Delay
   is the upper bound on the time taken to run syslog_async, in milliseconds.
   This delay is added when syslog is busy in order to reduce the probability
   of buffer overflow. Backlog is constrained between 1 and 99 and delay
   between 1 millisecond and 1000 millisconds. The default for backlog
   is 5 and for delay 1000. Note that delay is calculated from queue size as
   2^queue_size (in milliseconds) therefore the maximum delay for the default
   queue size is 64ms. Setting delay to zero is allowed, and inhibits the delay
   completely.
*/

void tunelog_async(int backlog, int delay);

END_DECLS

#endif
