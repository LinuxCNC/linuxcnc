/*
  getinput.c
  Non-blocking input on stdin
*/

#include <unistd.h>             /* STDIN_FILENO */
#include <fcntl.h>              /* F_GETFL, O_NONBLOCK */

/*
  getinput() returns the number of chars read, -1 if no chars were available,
  or 0 if EOF. It doesn't block, so you can call this repeatedly and when
  it returns non-zero you have that many chars, not including the added NULL.
  */
int getinput(char *buffer, int maxchars)
{
  int flags;
  int nchars;
  int index = 0;

  /* save the flags */
  flags = fcntl(STDIN_FILENO, F_GETFL);

  /* make terminal non-blocking */
  fcntl(STDIN_FILENO, F_SETFL, O_NONBLOCK);

  /* read the outstanding chars, one by one, until newline or no more */
  while (1 == (nchars = read(STDIN_FILENO, &buffer[index], 1)))
    {
      if (buffer[index++] == '\n')
        {
          buffer[index] = 0;    /* null terminate */
          break;
        }
    }

  /* restore the terminal */
  fcntl(STDIN_FILENO, F_SETFL, flags);

  if (nchars == -1)
    {
      return -1;                /* nothing new */
    }

  if (nchars == 0)
    {
      return 0;                 /* end of file */
    }

  return index;
}
