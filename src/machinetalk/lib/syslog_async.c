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

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/file.h>
#include <sys/syslog.h>

#include <sys/uio.h>
#include <sys/wait.h>
#include <netdb.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <errno.h>
#include <stdarg.h>
#include <stdlib.h>
#include <paths.h>
#include <stdio.h>
#include <ctype.h>

#include "syslog_async.h"

/* From RFC 3164 */
#define MAX_MESSAGE 1024

#define DEF_BACKLOG 5
#define DEF_DELAY 1000 /* doesn't come into effect until backlog > 10 */

static int log_fac = LOG_USER;
static int log_opts = LOG_ODELAY;
static const char *log_tag = "syslog";
static int log_mask = 0xff;
static int log_backlog = DEF_BACKLOG;
static int log_delay = DEF_DELAY;

static int log_fd = -1;
static int entries_alloced = 0;
static int entries_lost = 0;
static int connection_good = 1;

struct log_entry {
  int offset, length;
  struct log_entry *next;
  char payload[MAX_MESSAGE];
};

static struct log_entry *entries = NULL;
static struct log_entry *free_entries = NULL;

static int mksock(int type)
{
  int flags;
  int fd = socket(AF_UNIX, type, 0);

  if (fd != -1)
    {
      if ((flags = fcntl(fd, F_GETFL)) == -1 ||
	  fcntl(fd, F_SETFL, flags | O_NONBLOCK) == -1 ||
	  (flags = fcntl(fd, F_GETFD)) == -1 ||
	  fcntl(fd, F_SETFD, flags | FD_CLOEXEC) == -1)
	{
	  close(fd);
	  fd = -1;
	}
    }
  return fd;
}

void openlog_async(const char *ident, int option, int facility)
{
  if (ident)
    log_tag = ident;

  log_opts = option;

  if (facility != 0 && (facility &~ LOG_FACMASK) == 0)
    log_fac = facility;

  if (log_opts & LOG_NDELAY)
    log_fd = mksock(SOCK_DGRAM);
}

int setlogmask_async(int mask)
{
  int old = log_mask;

  if (mask != 0)
    log_mask = mask;

  return old;
}

void tunelog_async(int backlog, int delay)
{
  /* we need at least one buffer, and the
     delay calculations overflow for more than 99 */
  if (backlog < 1)
    backlog = 1;
  else if (backlog > 99)
    backlog = 99;

  /* don't lose existing buffers */
  if (backlog < entries_alloced)
    log_backlog = entries_alloced;
  else
    log_backlog = backlog;

  if (delay < 0)
    log_delay = 0;
  else if (delay > 1000)
    log_delay = 1000;
  else
    log_delay = delay;
}

void closelog_async(void)
{
  /* maybe last chance to flush */
  log_write_async();

  if (log_fd != -1)
    {
      close(log_fd);
      log_fd = -1;
    }

  /* restore defaults */
  log_fac = LOG_USER;
  log_opts = LOG_ODELAY;
  log_tag = "syslog";
  log_mask = 0xff;

  log_delay = DEF_DELAY;

  if (entries_alloced < DEF_BACKLOG)
    log_backlog = entries_alloced;
  else
    log_backlog = DEF_BACKLOG;
}

int log_fd_async(void)
{
  if (!entries || !connection_good)
    return -1;

  return log_fd;
}

void log_write_async(void)
{
  ssize_t rc;
  int fd, tried_stream = 0;
  struct log_entry *tmp;

  while (entries)
    {
      if (log_fd == -1 &&
	  (log_fd = mksock(SOCK_DGRAM)) == -1)
	goto fail;

      connection_good = 1;

      if ((rc = send(log_fd,
		     entries->payload + entries->offset,
		     entries->length,
		     MSG_NOSIGNAL)) != -1)
	{
	  entries->length -= rc;
	  entries->offset += rc;
	  connection_good = 1;

	  if (entries->length == 0)
	    goto free;

	  continue;
	}

      if (errno == EINTR)
	continue;

      if (errno == EAGAIN)
	return;

      /* *BSD, returns this instead of blocking? */
      if (errno == ENOBUFS)
	{
	  connection_good = 0;
	  return;
	}

      /* A stream socket closed at the other end goes into EPIPE
	 forever, close and re-open. */
      if (errno == EPIPE)
	goto reopen_stream;

      if (errno == ECONNREFUSED ||
	  errno == ENOTCONN ||
	  errno == EDESTADDRREQ ||
	  errno == ECONNRESET)
	{
	  /* socket went (syslogd down?), try and reconnect. If we fail,
	     stop trying until the next call to my_syslog()
	     ECONNREFUSED -> connection went down
	     ENOTCONN -> nobody listening
	     (ECONNRESET, EDESTADDRREQ are *BSD equivalents) */

	  struct sockaddr_un logaddr;

	  logaddr.sun_family = AF_LOCAL;
	  strncpy(logaddr.sun_path, _PATH_LOG, sizeof(logaddr.sun_path));

	  /* Got connection back? try again. */
	  if (connect(log_fd, (struct sockaddr *)&logaddr, sizeof(logaddr)) != -1)
	    continue;

	  /* errors from connect which mean we should keep trying */
	  if (errno == ENOENT ||
	      errno == EALREADY ||
	      errno == ECONNREFUSED ||
	      errno == EISCONN ||
	      errno == EINTR ||
	      errno == EAGAIN)
	    {
	      /* try again on next syslog() call */
	      connection_good = 0;
	      return;
	    }

	  /* we start with a SOCK_DGRAM socket, but syslog may want SOCK_STREAM */
	  if (!tried_stream && errno == EPROTOTYPE)
	    {
	    reopen_stream:
	      tried_stream = 1;
	      close(log_fd);
	      if ((log_fd = mksock(SOCK_STREAM)) != -1)
		continue;
	    }
	}

    fail:
      tried_stream = 0;

      /* give up - try to write to console if we've been asked
	 take care not to block in open() or write() */
      if ((log_opts & LOG_CONS) &&
	  (fd = open(_PATH_CONSOLE, O_WRONLY | O_NONBLOCK, 0)) != -1)
	{
	  char *start = strchr(entries->payload, '>') + 1;
	  int flags = fcntl(fd, F_GETFL);

	  if (flags != -1)
	    fcntl(fd, F_SETFL, flags | O_NONBLOCK);

	  entries->length -= start - entries->payload;
	  /* move down to remove the tag, and make room for the \r\n */
	  memmove(entries->payload, start, entries->length);
	  entries->payload[entries->length - 1] = '\r';
	  entries->payload[entries->length] = '\n';
	  write(fd, entries->payload, entries->length + 1);
	  close(fd);
	}

    free:
      tmp = entries;
      entries = tmp->next;
      tmp->next = free_entries;
      free_entries = tmp;

      if (entries_lost != 0)
	{
	  int e = entries_lost;
	  entries_lost = 0; /* avoid wild recursion */
	  syslog_async(LOG_WARNING, "async_syslog overflow: %d log entries lost", e);
	}
      continue;
    }
}

void syslog_async(int priority, const char *format, ...)
{
  va_list ap;

  va_start(ap, format);
  vsyslog_async(priority, format, ap);
  va_end(ap);
}

void vsyslog_async(int priority, const char *format, va_list ap)
{
  struct log_entry *entry;
  time_t time_now;
  char *p, *q, *r;
  size_t len;

  if (!(log_mask & LOG_MASK(LOG_PRI(priority))) || (priority &~ (LOG_PRIMASK|LOG_FACMASK)))
    return;

  if ((entry = free_entries))
    free_entries = entry->next;
  else if (entries_alloced < log_backlog && (entry = malloc(sizeof(struct log_entry))))
    entries_alloced++;

  if (!entry)
    entries_lost++;
  else
    {
      /* add to end of list, consumed from the start */
      entry->next = NULL;
      if (!entries)
	entries = entry;
      else
	{
	  struct log_entry *tmp;
	  for (tmp = entries; tmp->next; tmp = tmp->next);
	  tmp->next = entry;
	}

      time(&time_now);
      p = entry->payload;
      p += sprintf(p, "<%d>", priority | log_fac);

      q = p;

      if (log_opts & LOG_PID)
	p += sprintf(p, "%.15s %s[%d]: ", ctime(&time_now) + 4, log_tag, getpid());
      else
	p += sprintf(p, "%.15s %s: ", ctime(&time_now) + 4, log_tag);

      len = p - entry->payload;
      len += vsnprintf(p, MAX_MESSAGE - len, format, ap) + 1; /* include zero-terminator */
      entry->length = len > MAX_MESSAGE ? MAX_MESSAGE : len;

      /* remove trailing '\n's passed to us. */
      for (r = &entry->payload[entry->length - 2]; r >= entry->payload; r--)
	if (*r == '\n')
	  entry->length--;
	else
	  break;

      entry->offset = 0;

      if (log_opts & LOG_PERROR)
	{
	  ssize_t rc, s = entry->length - (q - entry->payload);
	  /* replace terminator with \n */
	  entry->payload[entry->length - 1] = '\n';

	  while (s != 0)
	    if ((rc = write(STDERR_FILENO, q, s)) != -1)
	      {
		s -= rc;
		q += rc;
		continue;
	      }
	    else if (errno == EINTR)
	      continue;
	    else
	      break;
	}
      entry->payload[entry->length - 1] = 0;
    }

  /* almost always, logging won't block, so try and write this now,
     to save collecting too many log messages during a select loop. */
  log_write_async();

  /* Since we're doing things asynchronously, we
     can now generate log lines very fast. With a small buffer (desirable),
     that means it can overflow the log-buffer very quickly.
     To avoid this, we delay here, the delay growing exponentially
     with queue length. Delay is  limited to 1 second, by default
     but can be tuned for less if needed. Note that for a responsive
     syslog, the log-line we just created will have been writen by the
     call the log_write_async() above, so that this doesn't delay at all. */
  if (entries && log_delay != 0)
    {
      struct timespec waiter;
      int d;

      for (d = 1,entry = entries; entry->next; entry = entry->next)
        {
          d *= 2;
          if (d >= log_delay) /* limit to 999ms */
            {
              d = log_delay - 1;
              break;
            }
        }

      waiter.tv_sec = 0;
      waiter.tv_nsec = d * 1000000; /* 1 ms */
      nanosleep(&waiter, NULL);

      /* try and write again */
      log_write_async();
    }
}
