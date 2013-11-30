
#ifndef __INCLUDE_HM2_ETH_H
#define __INCLUDE_HM2_ETH_H

#include RTAPI_INC_SLAB_H

#define MAX_ETH_BOARDS 4

#define HM2_ETH_VERSION "0.2"
#define HM2_LLIO_NAME "hm2_eth"

#define MAX_ETH_READS 64

typedef struct {
    hm2_lowlevel_io_t llio;
} hm2_eth_t;

typedef struct {
    void *buffer;
    int size;
    int from;
} read_queue_entry_t;

#endif
