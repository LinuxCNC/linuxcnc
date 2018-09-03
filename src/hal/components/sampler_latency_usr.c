/********************************************************************
* Description:  sampler_latency_usr.c
*
* A hack of sampler_usr.c, to provide a print of latency into
* a file, with real latency periods, not highest measured
*
********************************************************************/


#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include "rtapi.h"      /* RTAPI realtime OS API */
#include "hal.h"                /* HAL public API decls */
#include "streamer.h"

/***********************************************************************
*                  LOCAL FUNCTION DECLARATIONS                         *
************************************************************************/

/***********************************************************************
*                         GLOBAL VARIABLES                             *
************************************************************************/

int comp_id = -1;   /* -1 means hal_init() not called yet */
int shmem_id = -1;
int exitval = 1;    /* program return code - 1 means error */
int ignore_sig = 0; /* used to flag critical regions */
char comp_name[HAL_NAME_LEN+1]; /* name for this instance of sampler */

char titles[8][HAL_NAME_LEN+1] = {"SMax\0", "SLast\0", "SMax Jitter\0", "SAvg Jitter\0", "BMax\0", "BLast\0", "BMax Jitter\0", "BAvg Jitter\0"};

/***********************************************************************
*                            MAIN PROGRAM                              *
************************************************************************/

/* signal handler */
static void quit(int sig)
{
    if ( ignore_sig ) {
    return;
    }
    if ( shmem_id >= 0 ) {
    rtapi_shmem_delete(shmem_id, comp_id);
    }
    if ( comp_id >= 0 ) {
    hal_exit(comp_id);
    }
    exit(exitval);
}

#define BUF_SIZE 4000

int main(int argc, char **argv)
{
    int n, channel, retval, size, tag;
    long int samples;
    unsigned long this_sample;
    char  *cp2;
    char *name = NULL;
    void *shmem_ptr;
    fifo_t *fifo;
    shmem_data_t *data, *dptr, buf[MAX_PINS];
    int tmpout, newout;
    struct timespec delay;

    /* set return code to "fail", clear it later if all goes well */
    exitval = 1;
    channel = 0;
    tag = 0;
    samples = -1;  /* -1 means run forever */
    int  opt;

    while ((opt = getopt(argc, argv, "tn:c:N:")) != -1)
        {
        switch (opt)
            {
            case 'c':
                channel = strtol(optarg, &cp2, 10);
                if (( *cp2 ) || ( channel < 0 ) || ( channel >= MAX_SAMPLERS ))
                    {
                    fprintf(stderr,"ERROR: invalid channel number '%s'\n", optarg );
                    exit(1);
                    }
                break;
            case 'n':
                samples = strtol(optarg, &cp2, 10);
                if (( *cp2 ) || ( samples < 0 ))
                    {
                    fprintf(stderr, "ERROR: invalid sample count '%s'\n", optarg );
                    exit(1);
                    }
                break;
            case 'N':
                name = optarg;
                break;
            case 't':
                tag = 1;
                break;
            default: /* '?' */
                fprintf(stderr,"ERROR: unknown option '%c'\n", opt);
                fprintf(stderr,"valid options are:\n" );
                fprintf(stderr,"\t-t\t\ttag values with sample number\n" );
                fprintf(stderr,"\t-c <int>\t channel number\n" );
                fprintf(stderr,"\t-n <int>\t sample count\n" );
                fprintf(stderr,"\t-N <name>\t set HAL component name\n" );
                exit(1);
                exit(EXIT_FAILURE);
            }
        }
    if (optind < argc)
        {
        int fd;
        if(argc > optind+1)
            {
            fprintf(stderr, "ERROR: At most one filename may be specified\n");
            exit(1);
            }
        // make stdout be the named file
        fd = open(argv[optind], O_WRONLY | O_CREAT, 0666);
        close(1);
        dup2(fd, 1);
        }

    /* register signal handlers - if the process is killed
       we need to call hal_exit() to free the shared memory */
    signal(SIGINT, quit);
    signal(SIGTERM, quit);
    signal(SIGPIPE, quit);

    /* connect to HAL */
    if (name == NULL)
        {
        /* create a unique module name, to allow for multiple samplers */
        snprintf(comp_name, sizeof(comp_name), "halsampler%d", getpid());
        name = comp_name;
        }

    /* connect to the HAL */
    ignore_sig = 1;
    comp_id = hal_init(name);
    ignore_sig = 0;

    /* check result */
    if (comp_id < 0)
        {
        fprintf(stderr, "ERROR: hal_init() failed: %d\n", comp_id );
        goto out;
        }
    hal_ready(comp_id);

    /* open shmem for user/RT comms (fifo) */
    /* initial size is unknown, assume only the fifo structure */
    shmem_id = rtapi_shmem_new(SAMPLER_SHMEM_KEY+channel, comp_id, sizeof(fifo_t));
    if ( shmem_id < 0 )
        {
        fprintf(stderr, "ERROR: couldn't allocate user/RT shared memory\n");
        goto out;
        }

    retval = rtapi_shmem_getptr(shmem_id, &shmem_ptr, 0);
    if ( retval < 0 )
        {
        fprintf(stderr, "ERROR: couldn't map user/RT shared memory\n");
        goto out;
        }

    fifo = shmem_ptr;
    if ( fifo->magic != FIFO_MAGIC_NUM )
        {
        //fprintf(stderr, "ERROR: channel %d realtime part is not loaded\n", channel );
        goto out;
        }

    /* now use data in fifo structure to calculate proper shmem size */
    size = sizeof(fifo_t) + (1+fifo->num_pins) * fifo->depth * sizeof(shmem_data_t);
    /* close shmem, re-open with proper size */
    rtapi_shmem_delete(shmem_id, comp_id);
    shmem_id = rtapi_shmem_new(SAMPLER_SHMEM_KEY+channel, comp_id, size);
    if ( shmem_id < 0 )
        {
        fprintf(stderr, "ERROR: couldn't re-allocate user/RT shared memory\n");
        goto out;
        }

    retval = rtapi_shmem_getptr(shmem_id, &shmem_ptr, 0);
    if ( retval < 0 )
        {
        fprintf(stderr, "ERROR: couldn't re-map user/RT shared memory\n");
        goto out;
        }

    fifo = shmem_ptr;
    data = fifo->data;

    // This is the main printing loop
    while ( samples != 0 )
        {
        while ( fifo->in == fifo->out )
            {
            /* fifo empty, sleep for 10mS */
            delay.tv_sec = 0;
            delay.tv_nsec = 10000000;
            nanosleep(&delay,NULL);
            }
        /* make pointer to fifo entry */
        tmpout = fifo->out;
        newout = tmpout + 1;
        if ( newout >= fifo->depth )
            newout = 0;

        dptr = &data[tmpout * (fifo->num_pins+1)];
        /* read data from shmem into buffer */
        for ( n = 0 ; n < fifo->num_pins ; n++ )
            buf[n] = *(dptr++);

        /* and read sample number */
        this_sample = dptr->u;
        if ( fifo->out != tmpout )
        /* the sample was overwritten while we were reading it */
        /* so ignore it */
            continue;
        else
            /* update 'out' for next sample */
            fifo->out = newout;

        if ( this_sample != ++(fifo->last_sample) )
            {
            printf ( "overrun\n" );
            fifo->last_sample = this_sample;
            }

        if ( tag )
            printf ( "# %ld", this_sample );

        for ( n = 0 ; n < 8 ; n++ )
            {
            switch ( fifo->type[n] )
                {
                case HAL_S32:
                    if((long)buf[n].s > 0L )
                        printf ( "\t%s\t %8ld", titles[n], (long)buf[n].s);
                    break;
                default:
                    /* better not happen */
                    goto out;
                }
            }
        printf ( "\n" );
        if ( samples > 0 )
            samples--;

        }
    /* run was succesfull */
    exitval = 0;

out:
    ignore_sig = 1;
    if ( shmem_id >= 0 )
        rtapi_shmem_delete(shmem_id, comp_id);

    if ( comp_id >= 0 )
        hal_exit(comp_id);

    return exitval;
}
