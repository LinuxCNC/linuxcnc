// http://www.codemaestro.com/reviews/11
//  gcc -lrt -o shm shm.c
#include <fcntl.h>
#include <sys/stat.h>
#include <errno.h>
#include <sys/mman.h>
#include <sys/types.h> //shm_open
#include <stdio.h>  //printf
#include <stdlib.h> //exit
#include <unistd.h> //close
#include <string.h> //strerror

/* This will be created under /dev/shm/ */
#define STATE_FILE "/program.shared"
#define  NAMESIZE 1024
#define   MAXNAMES 100

/* Define a struct we wish to share. Notice that we will allocate
 * only sizeof SHARED_VAR, so all sizes are constant
 */
typedef struct
{
    char name[MAXNAMES][NAMESIZE];
    int flags;
}  SHARED_VAR;

int main (void)
{
    int first = 0;
    int shm_fd;
    static SHARED_VAR *conf;

    /* Try to open the shm instance with  O_EXCL,
     * this tests if the shm is already opened by someone else
     */
    if((shm_fd = shm_open(STATE_FILE, (O_CREAT | O_EXCL | O_RDWR),
			  (S_IREAD | S_IWRITE))) > 0 ) {
	first = 1; /* We are the first instance */
    }
    else if((shm_fd = shm_open(STATE_FILE, (O_CREAT | O_RDWR),
			       (S_IREAD | S_IWRITE))) < 0) {
	/* Try to open the shm instance normally and share it with
	 * existing clients
	 */
	printf("Could not create shm object. %s\n", strerror(errno));
	return errno;
    } 

    /* Set the size of the SHM to be the size of the struct. */
    ftruncate(shm_fd, sizeof(SHARED_VAR));

    /* Connect the conf pointer to set to the shared memory area,
     * with desired permissions
     */
    if((conf =  mmap(0, sizeof(SHARED_VAR), (PROT_READ | PROT_WRITE),
		     MAP_SHARED, shm_fd, 0)) == MAP_FAILED) {

	return errno;

    }
    if(first) {
	/* Run a set up for the first time, fill some args */
	printf("First creation of the shm. Setting up default values\n");
	conf->flags = 4;
    }
    else
	{
	    printf("Value of flags = %d\n", conf->flags);
	}

    /* Do some work... */

    close(shm_fd);
    exit(0);
}
