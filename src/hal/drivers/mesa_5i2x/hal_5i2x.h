/*************************************************************************

Header for Mesa Electronics 5i2x board driver

Copyright (C) 2007 John Kasunich <jmkasunich AT sourceforge DOT net>

*************************************************************************/

/* code assumes that PINS_PER_PORT is <= 32 */
#define PINS_PER_PORT		24
#define PORTS_PER_BOARD		3

#define CFG_RAM_SIZE		1024

#define PROTOCOL_VERSION_ADDR	4
#define MIN_PROTOCOL_VERSION	1
#define MAX_PROTOCOL_VERSION	1

#define BOARD_CODE_ADDR		5
#define BOARD_CODE_5I20		1

#define DATA_START_ADDR		6
#define DATA_END_ADDR 		(CFG_RAM_SIZE-3)

#define CHECKSUM1_ADDR		(CFG_RAM_SIZE-2)
#define CHECKSUM2_ADDR		(CFG_RAM_SIZE-1)

/*************************************************************************
                         Data Structures
*************************************************************************/

/* digital I/O pin */
typedef struct dig_io_t {
    hal_bit_t *in;
    hal_bit_t *in_not;
    hal_bit_t *out;
    hal_bit_t invert;
} dig_io_t;

/* one connector worth of digital I/O */
typedef struct dig_port_t {
    __u32 ins;		/* bitmap marking all inputs */
    __u32 outs;		/* bitmap marking all outputs */
    __u32 ocs;		/* bitmap marking open collector outputs */
    void __iomem *data_addr;
    void __iomem *dir_addr;
    dig_io_t pins[24];
} dig_port_t;

/* master board structure */
typedef struct board_data_t {
    struct pci_dev *pci_dev;		/* PCI bus info */
    int slot;				/* PCI slot number */
    int num;				/* HAL board number */
    void __iomem *base;			/* base address */
    int len;				/* FIXME what's this? */
    dig_port_t gpio[PORTS_PER_BOARD];
    struct stepgen_t *stepgen;		/* linked list of stepgens */
} board_data_t; 

/*************************************************************************
                                Globals
*************************************************************************/

extern int comp_id;	/* HAL component ID */


/*************************************************************************
                                Functions
*************************************************************************/


#define STEPGEN_VEL_MODE 10
#define STEPGEN_POS_MODE 11

/* stepgen block format
  0: block code (either VEL_MODE or POS_MODE)
  1-2: base address
  3: pin number for step/up/phaseA
  4: pin number for dir/down/phaseB
*/
int export_stepgen(__u8 **ppcfg, board_data_t *board);
