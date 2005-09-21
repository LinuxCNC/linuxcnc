#include <stdio.h>

/*
  Compile this with "gcc M102.c -o M102" to build an M102 executable
  program in the emc/programs/ directory. M102 in your G code program
  will execute this code, passing the P and Q variables as command
  line arguments.
 */

int main(int argc, char *argv[])
{
  double p = 0.0, q = 0.0;

  /* process the P and Q command line args we will be given */
  if (argc > 1) {
    sscanf(argv[1], "%lf", &p);
  }
  if (argc > 2) {
    sscanf(argv[2], "%lf", &q);
  }

  /* put your code here */

  printf("M102 P%f Q%f: put your code here\n", p, q);

  return 0;
}


