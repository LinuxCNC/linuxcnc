// test program to time ppdev filedescriptor I/O versus
// direct register writes
// compile like so:
//
// gcc -O5 -g -Wall ppioctl.c  -oppioctl
//
// results: Intel(R) Atom(TM) CPU D525   @ 1.80GHz
// ioctl-based parport write: 2.8uSec/write
// direct register access: 1.8usec/write
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <sys/io.h>

#include <string.h>
#include <linux/ppdev.h> 
#include <linux/parport.h>

typedef unsigned long long u64;
typedef unsigned long      u32;

#define N_ITER 10000

struct ppres  {
    unsigned int state;
    unsigned int reg_start;
    unsigned int reg_end;
    unsigned int irq;
};

static inline u64 rdtsc(void)
{
    if (sizeof(long) == sizeof(u64)) {
	u32 lo, hi;
	asm volatile("rdtsc" : "=a" (lo), "=d" (hi));
	return ((u64)(hi) << 32) | lo;
    }
    else {
	u64 tsc;
	asm volatile("rdtsc" : "=A" (tsc));
	return tsc;
    }
}

int get_ppdev_res(int dev, struct ppres *ppres)
{
	FILE *f;
	char path[1024], value[100],line[1024], *s;
	unsigned from, to;

	// parse this:
	// $ cat /sys/class/ppdev/parport0/device/resources 
	// state = active
	// io 0x378-0x37f
	// irq 7
	ppres->state = 0;
	ppres->reg_start = 0;
	ppres->reg_end = 0;
	ppres->irq = 0;

	sprintf(path,"/sys/class/ppdev/parport%d/device/resources",dev);

	f = fopen(path, "r");
	if (!f) {
	    perror(path);
	    return -1;
	}
	while (fgets(line, sizeof(line), f)) {
	    if (!strncmp(line, "state", 5)) {
		s = strchr(line, '=');
		if (s && 1 == sscanf(s, "= %s", value)) {
		    ppres->state = !strcmp(value,"active");
		} else  {
		    fprintf(stderr, "get_ppdev_res: cant parse '%s'\n",
			    line);
		    fclose(f);
		    return -1;
		}
	    }
	    if (!strncmp(line, "irq", 3)) {
		if (1 != sscanf(line+4, " %d", &ppres->irq)) {
		    fprintf(stderr, "get_ppdev_res: cant parse '%s'\n",
			    line);
		    fclose(f);
		    return -1;
		}
	    }
	    if (!strncmp(line, "io", 2)) {
		if (2 != sscanf(line+2, " 0x%x-0x%x", &from, &to)) {
		    fprintf(stderr, "get_ppdev_res: cant parse '%s'\n",
			    line);
		    fclose(f);
		    return -1;
		    
		} else {
		    ppres->reg_start = from;
		    ppres->reg_end = to;
		}
	    }
	}
	fclose(f);
	return 0;
}

float cpu_MHz(void) 
{
    char *path = "/proc/cpuinfo",  *s, line[1024];
    float freq;
    char *cpu_line = "cpu MHz";

    // parse /proc/cpuinfo for the line:
    // cpu MHz		: 2378.041
    FILE *f = fopen(path,"r");
    if (!f) {
	perror(path);
	return -1.0;
    }
    while (fgets(line, sizeof(line), f)) {
	if (!strncmp(line, cpu_line, strlen(cpu_line))) {
	    s = strchr(line, ':');
	    if (s && 1 == sscanf(s, ":%g", &freq)) {
		fclose(f);
		return freq;
	    }
	}
    }
    fclose(f);
    return -1.0;
}


int main(int argc, char *argv[])
{
    int i,result;
    unsigned char ctrl,data, status;
    int mode;
    int parportfd = 0;
    u64 start;
    unsigned int lap;
    double time;
    struct ppres ppres;
    char path[1024];
    int dev = 0;
    int time_ioctl = 1;
    float mhz = cpu_MHz();
    if (mhz < 0.0) {
	fprintf(stderr, "cant get CPU frequency from /proc/cpuinfo\n");
	exit (1);
    }
    printf("mhz = %g\n",mhz);

    if (argc > 1) {
	dev = atoi(argv[1]);
    }
    if (argc >  2) {
	time_ioctl = 0;
    }
    sprintf(path, "/dev/parport%d", dev);

    parportfd = open(path, O_RDWR);  
    if (parportfd > 0) 
	result = ioctl(parportfd, PPCLAIM);
    else {
	perror(path);
	exit(1);
    }
    ioctl(parportfd, PPGETMODES, &mode);
    printf("modes=0x%x\n", mode);

    if (get_ppdev_res(dev, &ppres) < 0) {
	exit(1);
    } else 
	printf("%s: io=0x%x-0x%x irq=%d state=%d\n", 
	       path, ppres.reg_start, 
	       ppres.reg_end, ppres.irq,ppres.state);

    ioctl( parportfd, PPRCONTROL, &ctrl );
    ioctl( parportfd, PPRSTATUS , &data );
    ioctl( parportfd, PPRDATA   , &status );
    
    printf("control=0x%x data=0x%x status=0x%x\n", ctrl, data, status);
   
    if (time_ioctl) { 
	lap = 0;
	for (i = 0; i < N_ITER; i++) {
	    if (data == 0) 
		data = 1;
	    start = rdtsc();
	    ioctl(parportfd, PPWDATA, &data);
	    lap += rdtsc() - start;
	    data = data << 1;
	}
	time = (double) lap;
	time /= ((double) mhz * (double)N_ITER) ;
	printf("time=%g uS/ioctl() write\n",time);

    } else { // timing with raw register I/O
	if (ioperm(ppres.reg_start, 3, 1) < 0) {
	    perror("ioperm access");
	    exit(1);
	}
	lap = 0;
	for (i = 0; i < N_ITER; i++) {
	    if (data == 0) 
		data = 1;
	    start = rdtsc();
	    outb(data, ppres.reg_start);  
	    lap += rdtsc() - start;
	    data = data << 1;
	}
	time = (double) lap;
	time /= ((double) mhz * (double)N_ITER) ;
	printf("direct register write: time=%g uS/write()\n",time);

	if (ioperm(ppres.reg_start, 3, 0) < 0) {  //Release the IO ports
	    perror("ioperm release");  
	}
    }
    result = ioctl(parportfd,PPRELEASE);
    close(parportfd);
    return result;
}
