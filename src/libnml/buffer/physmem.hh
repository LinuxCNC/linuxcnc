/***********************************************************************
* File: physmem.hh
* Purpose: Define the PHYSMEM_HANDLE class which creates objects that represent
*  a portable interface to sections of physical memory.
* NOTES:
*  On some platforms accessing a block of physical memory is a simple as
* intitializing a pointer and then dereferencing it, but accessing specific
* sections of extended memory under DOS or Windows requires changing the
* descriptor tables and therefore circumventing protected mode security.
* Other platforms require the use of mmap on memory device files to choose
* a section of memory mapped IO.
***********************************************************************/

#ifndef PHYSMEM_HH
#define PHYSMEM_HH


#ifdef  __cplusplus
extern "C" {
#endif
#include <stddef.h>		/* size_t */

#ifdef __cplusplus
}
#endif
typedef char *LOCAL_ADDRESS_TYPE;

class PHYSMEM_HANDLE {
  public:
    PHYSMEM_HANDLE();		/* Constructor for blank handle. */

    /* Constructor to access memory starting at _physical_address, for _size
       bytes. Some platform implementations distinguish between various types 
       of addresses using the address code. */
    PHYSMEM_HANDLE(unsigned long _physical_address,
	long _address_code, long _size);

      virtual ~ PHYSMEM_HANDLE();	/* Destructor */

    long offset;		/* Operations read and write work use offset */
    long size;
    int read(void *_to, long _read_size);	/* Read _read_size bytes and
						   store */
    /* at _to */
    int write(void *_from, long _write_size);	/* Write _write_size bytes */
    /* using data at _from */

    void set_to_ptr(void *_ptr, long size);	/* Use the physical memory at 
						   _ptr. */

    long address_code;		/* Platform specific address type code. */
    /* (See vme.h for VXWORKS) */

    void memsetf(long offset, char _byte, long _memset_size);
    int clear_memory();
    int valid();
    int isvalid;
    char *temp_buf;
    unsigned long physical_address;
    LOCAL_ADDRESS_TYPE local_address;
  protected:

  public:

    int using_bit3;
    double total_bytes_moved;
    int enable_byte_counting;
};

#endif
