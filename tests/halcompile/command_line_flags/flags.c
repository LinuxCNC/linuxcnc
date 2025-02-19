/*
  flags.c
  To test command line compiler flags to halcompile

  Copyright (C) 2023 Andy Pugh
 */


#ifndef ULAPI
#error This is intended as a userspace component only.
#endif

#include <stdio.h>
#include <readline.h>

int main(int argc, char **argv)
{
    const char* res;
    rl_initialize();
    return 0;
}
