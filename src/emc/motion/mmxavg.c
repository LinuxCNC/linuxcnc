/*
  mmxavg.c

  Min-max-average code

  Modification history:

  5-Jan-2004 MGS used this file to build a motion module for emc2.
  14-Jun-2001 WPS put in checks for NULL pointers and paranoid index checks.
  13-Mar-2000 WPS added unused attribute to pid_h to avoid 'defined but not used' compiler warning.
  10-Oct-1997  FMP created
  */

#include "mmxavg.h"             /* these declarations */

/* ident tag */
#ifndef __GNUC__
#ifndef __attribute__
#define __attribute__(x)
#endif
#endif

static char __attribute__((unused)) ident[] = "$Id$";

#ifndef MAIN
/* MAIN not defined-- compile functions only, no main() */

int mmxavgInit(MMXAVG_STRUCT *mmxavg, double *space, int size)
{
  if(0 == mmxavg)
    {
      return -1;
    }
  mmxavg->nums = space;
  mmxavg->size = size;

  return mmxavgReset(mmxavg);
}

int mmxavgReset(MMXAVG_STRUCT *mmxavg)
{
  if(0 == mmxavg)
    {
      return -1;
    }
  mmxavg->sum = 0;
  mmxavg->index = 0;
  mmxavg->in = 0;
  mmxavg->inited = 0;
  mmxavg->min = 0.0;
  mmxavg->max = 0.0;

  return 0;
}

int mmxavgAdd(MMXAVG_STRUCT *mmxavg, double num)
{
  if(0 == mmxavg)
    {
      return -1;
    }
  if(mmxavg->index >= mmxavg->size ||
     mmxavg->index < 0 )
    {
      return -1;
    }
  if (mmxavg->in == mmxavg->size)
    {
      /* already full-- knock off oldest */
      mmxavg->sum -= mmxavg->nums[mmxavg->index];
    }
  else
    {
      mmxavg->in++;
    }

  mmxavg->nums[mmxavg->index] = num;
  mmxavg->sum += num;
  mmxavg->index++;
  if (mmxavg->index == mmxavg->size)
    {
      mmxavg->index = 0;
    }

  if (! mmxavg->inited)
    {
      mmxavg->min = num;
      mmxavg->max = num;
      mmxavg->inited = 1;
    }
  else if (num < mmxavg->min)
    {
      mmxavg->min = num;
    }
  else if (num > mmxavg->max)
    {
      mmxavg->max = num;
    }

  return 0;
}

double mmxavgMin(MMXAVG_STRUCT *mmxavg)
{
  if(0 == mmxavg)
    {
      return -1;
    }
  return mmxavg->min;
}

double mmxavgMax(MMXAVG_STRUCT *mmxavg)
{
  if(0 == mmxavg)
    {
      return -1;
    }
  return mmxavg->max;
}

double mmxavgAvg(MMXAVG_STRUCT *mmxavg)
{
  if(0 == mmxavg)
    {
      return -1;
    }
  if (mmxavg->in > 0)
    {
      return mmxavg->sum / mmxavg->in;
    }
  else
    {
      return 0.0;
    }
}

#else
/* MAIN defined */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

/*
  syntax: mmx <size> {"min" | "max"}
  size is number of entries over which to average
  optional "min", "max" literals tell which to print, default is avg
  */
int main(int argc, char *argv[])
{
  MMXAVG_STRUCT mmxavg;
  int size;
  double *space;
  double num;
  int count = 0;
  int which = 3;                /* 1=min, 2=max, 3=avg printed */

  /* check args */
  if (argc < 2)
    {
      fprintf(stderr, "syntax: %s <size> {\"min\" | \"max\"}\n",
              argv[0]);
      exit(1);
    }

  /* read first mandatory arg */
  if (1 != sscanf(argv[1], "%d", &size) ||
      size < 1)
    {
      fprintf(stderr, "size arg must be > 0\n");
      exit(1);
    }

  /* read optional args */
  if (argc > 2)
    {
      if (!strcmp(argv[2], "min"))
        {
          which = 1;
        }
      else if (!strcmp(argv[2], "max"))
        {
          which = 2;
        }
      else
        {
          fprintf(stderr, "printing arg must be \"min\" or \"max\"\n");
          exit(1);
        }
    }

  /* get space */
  space = (double *) malloc(size * sizeof(double));
  mmxavgInit(&mmxavg, space, size);

  while (! feof(stdin))
    {
      if (1 != scanf("%lf", &num))
        {
          break;
        }

      mmxavgAdd(&mmxavg, num);

      if (which == 1)
        printf("%d\t%f\n", count++, mmxavgMin(&mmxavg));
      else if (which == 2)
        printf("%d\t%f\n", count++, mmxavgMax(&mmxavg));
      else
        printf("%d\t%f\n", count++, mmxavgAvg(&mmxavg));
    }

  /* release space */
  free(space);

  exit(0);
}

#endif /* MAIN defined */
