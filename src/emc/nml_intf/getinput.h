#ifndef GETINPUT_H
#define GETINPUT_H

#ifdef __cplusplus
extern "C" {
#endif

/*
  getinput() returns the number of chars read, or 0 if no
  chars were available. It doesn't block, so you can call this
  repeatedly and when it returns non-zero you have that many chars,
  not including the added NULL.
  */
    extern int getinput(char *buffer, int maxchars);

#ifdef __cplusplus
}
#endif
#endif
