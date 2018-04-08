#include <sys/types.h>
#include <stdio.h>
#include <stdlib.h>

#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>

#define HW_REGS_SPAN ( 65536 )

//#define MAX_ADDR 65533 (higher creates no output error)
#define MAX_ADDR 1020

static void show_usage(void)
{
    printf("mksocmemio: Utility to read or write hm2socfpga memory locatons\n");
    printf("Note: the mksocfpga uio0 driver needs to be active\n ");
    printf("To input Hexadecimal Address and data values, preceed the number with 0x:\n");
    printf("Usage options:\n");
    printf(" -h For this help message.\n");
    printf(" -r For reading an address: [-r <address>]\n");
    printf(" -w For writing an address: [-w <address> <value>]\n");
}


int main ( int argc, char *argv[] )
{
    void *virtual_base;
    void *h2p_lw_axi_mem_addr=NULL;
    int fd;
    u_int32_t index, inval;
    
    // Open /dev/uio0
    if ( ( fd = open ( "/dev/uio0", ( O_RDWR | O_SYNC ) ) ) == -1 ) {
        printf ( "    ERROR: could not open \"/dev/uio0\"...\n" );
        return ( 1 );
    }
    
    // get virtual addr that maps to physical
    virtual_base = mmap( NULL, HW_REGS_SPAN, ( PROT_READ | PROT_WRITE ), MAP_SHARED, fd, 0);
    
    if ( virtual_base == MAP_FAILED ) {
        printf ( "    ERROR: mmap() failed...\n" );
        
        close ( fd );
        return ( 1 );
    }
    // Get the base address that maps to the device
    //    assign pointer
    h2p_lw_axi_mem_addr=virtual_base;
    if (argc > 2) {
        if(argv[2][0] == '0' && argv[2][1] == 'x') {
            printf("Hex address value input found\n");
            index = (u_int32_t) strtoul(argv[2], NULL, 16);
        }
        else {
            printf("Assuming decimal address value input\n");
            index = (u_int32_t) strtoul(argv[2], NULL, 10);
        }
        u_int32_t value = *((u_int32_t *)(h2p_lw_axi_mem_addr + index));
    
        switch (argv[1][1]) {
            case 'r':
                printf("Read: ");
                printf("Address   0x%08X\t%u\n \tvalue = 0x%08X\t%u \n", index, index, value, value);
                break;
            case 'w':
                if (argc == 4) {
                    printf("Write: ");
//                    printf("Address   0x%08X\t %u will be set to\nvalue   0x%08X \t %u \n", index, index, value, value);
                    if(argv[3][0] == '0' && argv[3][1] == 'x') {
                        inval = (u_int32_t) strtoul(argv[3], NULL, 16);
                    }
                    else {
                        inval = (u_int32_t) strtoul(argv[3], NULL, 10);
                    }
                    *((u_int32_t *)(h2p_lw_axi_mem_addr + index)) = inval;
                    printf("Wrote:  0x%08X\t %u\tto Address --> 0x%08X\t%u \n", inval, inval, index, index);                    
                } else {
                    printf("Value missing use: mksocmemio -h to show valid options and argunemts\n");
                }
                break;
            default:
                printf("Wrong option: %s\n use: mksocmemio -h to show valid options and argunemts\n", argv[1]);
                break;
        }

    }
    else if (argc == 2) {
        switch (argv[1][1]){
            case 'h':
                show_usage();
                break;
            default:
                printf("Wrong option %s\n use: mksocmemio -h to show valid options and argunemts\n", argv[1]);
                break;
        }
    }
    close ( fd );
    return (0);
}
