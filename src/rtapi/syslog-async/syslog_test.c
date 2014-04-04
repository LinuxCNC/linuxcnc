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

#include <stdio.h>
#include <stdlib.h>
#include <sys/file.h>
#include <string.h>
#include <unistd.h>

#include "syslog_async.h"


/* 
   Read lines from stdin and log them as facility user, priority critical.

   Note that this is a test harness for syslog_async, and not a replacement
   for logger(1).

   Options:

   -s : log to stderr
   -p : log process-id
   -c : fall-back to console
*/

int main(int argc, char *argv[])
{
  char *buf, *end;
  fd_set rs, ws;
  int flags = 0, opt, fd, size = 200;
  
  while ((opt = getopt(argc, argv, "cps")) != -1)
    if (opt == 's')
      flags |= LOG_PERROR;
    else if (opt == 'c')
      flags |= LOG_CONS;
    else if (opt == 'p')
      flags |= LOG_PID;
  
   if (!(buf = end = malloc(size)))
    exit(1);

   openlog_async("wibble", flags, LOG_USER);

   while (1)
    {
      FD_ZERO(&rs);
      FD_ZERO(&ws);
     
      FD_SET(STDIN_FILENO, &rs);

      if ((fd = log_fd_async()) != -1)
	FD_SET(fd, &ws);
      
      if (select(10, &rs, &ws, NULL, NULL) < 0 )
	{
	  FD_ZERO(&rs);
	  FD_ZERO(&ws);
	}
      
      if (fd != -1 && FD_ISSET(fd, &ws))
	log_write_async();
      
      if (FD_ISSET(STDIN_FILENO, &rs))
	{
	  ssize_t r;
	  char *nl;
	  
	  if ((r = read(STDIN_FILENO, end, size - (end - buf))) == 0)
	    break;
	  
	  if (r == -1)
	    {
	      perror("read");
	      exit(1);
	    }

	  end += r;
	  
	  for (nl = buf; nl < end; nl++)
	    if (*nl == '\n')
	      {
		*nl = 0;
		syslog_async(LOG_CRIT, "%s", buf);
		
		memmove(buf, nl+1, end - (nl + 1));
		end -= nl - buf + 1;
		nl = buf;
	      }

	  if (end == buf + size)
	    { 
	      size_t tmp = end - buf;
	      size *= 2;
	      if ((buf = realloc(buf, size)) == NULL)
		exit(1);
	      end = buf + tmp;
	    }
	}
    }

  closelog_async();
}
