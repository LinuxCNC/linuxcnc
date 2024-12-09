#define driver_NAME "colorcnc"
#define encoders_count 6
#define di_count 24
#define do_count 12
#define steppers_count 6
#define pwm_count 3


#ifndef __ETHERBONE_H__
#define __ETHERBONE_H__

#define UDP_PORT 1234
#define SEND_TIMEOUT_US 1000
#define RECV_TIMEOUT_US 1000
#define READ_PCK_DELAY_NS 100000

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#include <stdint.h>
#include <stdlib.h>

#define BOARD_CLK 50000000ULL
#define SERVO_PERIOD 1000ULL

/*

The EtherBone record has a struct that looks like this:

struct etherbone_record {
	// 1...
	uint8_t bca : 1;
	uint8_t rca : 1;
	uint8_t rff : 1;
	uint8_t ign1 : 1;
	uint8_t cyc : 1;
	uint8_t wca : 1;
	uint8_t wff : 1;
	uint8_t ign2 : 1;

	uint8_t byte_enable;

	uint8_t wcount;

	uint8_t rcount;

	uint32_t write_addr;
    union {
    	uint32_t value;
        read_addr;
    };
} __attribute__((packed));

This is wrapped inside of an EtherBone network packet header:

struct etherbone_packet {
	uint8_t magic[2]; // 0x4e 0x6f
	uint8_t version : 4;
	uint8_t ign : 1;
	uint8_t no_reads : 1;
	uint8_t probe_reply : 1;
	uint8_t probe_flag : 1;
	uint8_t port_size : 4;
	uint8_t addr_size : 4;
	uint8_t padding[4];

	struct etherbone_record records[0];
} __attribute__((packed));

LiteX only supports a single record per packet, so either wcount or rcount
is set to 1.  For a read, the read_addr is specified.  For a write, the
write_addr is specified along with a value.

The same type of record is returned, so your data is at offset 16.
*/

struct eb_connection {
    int fd;
    int read_fd;
    int is_direct;
    struct addrinfo* addr;
};

//struct eb_connection;

int eb_unfill_read32(uint8_t wb_buffer[20]);
int eb_fill_write32(uint8_t wb_buffer[20], uint32_t data, uint32_t address);
int eb_fill_read32(uint8_t wb_buffer[20], uint32_t address);

struct eb_connection *eb_connect(const char *addr, const char *port, int is_direct);
void eb_disconnect(struct eb_connection **conn);
uint32_t eb_read32(struct eb_connection *conn, uint32_t addr);
void eb_write32(struct eb_connection *conn, uint32_t val, uint32_t addr);

#ifdef __cplusplus
};
#endif /* __cplusplus */

#endif /* __ETHERBONE_H__ */


char msg[] = "Hello there!\n";

// structura driver data

typedef struct {
	// hal work signals
  	hal_bit_t *digital_in[di_count];    /* ptrs for digital input pins 16 */
	hal_bit_t *digital_in_n[di_count];
  	hal_bit_t *digital_out[do_count];    /* ptrs for digital output pins 8 */
	hal_bit_t *enable_dr;

//	hal_bit_t *index_en[NUM_chanal];
//	hal_float_t *enccounts[NUM_chanal];
//	hal_float_t *encscale[NUM_chanal];
//	hal_float_t *encvel[NUM_chanal];

	hal_float_t *stepgen_position_cmd[steppers_count];
	hal_float_t *stepgen_velocity_cmd[steppers_count];
	hal_float_t *stepgen_position_fb[steppers_count];
	hal_float_t *stepgen_velocity_fb[steppers_count];
	hal_s32_t stepgen_rate[steppers_count];
	hal_float_t stepgen_scale[steppers_count];
	hal_float_t stepgen_maxvel[steppers_count];
	//hal_float_t *velocity[step\pers_count];
	hal_bit_t *stepgen_reset[steppers_count];
    hal_bit_t *stepgen_enable[steppers_count];
	hal_bit_t stepgen_mode[steppers_count];
	hal_u32_t stepgen_steptime;
	hal_u32_t stepgen_steplen;
	hal_u32_t stepgen_stepspace;
	hal_u32_t stepgen_dirtime;
    hal_bit_t stepgen_step_polarity;
	hal_s32_t *stepgen_counts[steppers_count];
    hal_s32_t debug_0;
    hal_s32_t debug_1;
    hal_float_t debug_2;
    hal_float_t debug_3;
    hal_float_t fmax_limit1;
    hal_float_t fmax_limit2;
    hal_float_t fmax_limit3;
//	hal_float_t *sdscale[Max_sdchanel];
//	hal_bit_t *invert_dir[Max_sdchanel];
//	hal_float_t *Limp[Max_sdchanel];
//	__u32 last_pause[Max_sdchanel];
//tests signals
	hal_u32_t *test;
	bool first_run;
	struct eb_connection* eb;
	__u64 board_wallclock;
	__u64 board_wallclock_old;	
	hal_u32_t *board_wallclock_lsb;
	hal_u32_t *board_wallclock_msb;
	hal_u32_t *pwm_width[pwm_count];
	hal_u32_t *pwm_period[pwm_count];
	__u64 rcvd_pos[steppers_count];
    int64_t last_count[steppers_count];
    int64_t reset_count[steppers_count];
    double old_position_cmd[steppers_count];
    double old_velocity_cmd[steppers_count];
    double t_period_real;
} data_t;

// structura packets data for MAIN board

typedef struct __attribute__ ((packed)) {
__s64 stepper_position[steppers_count];
__u64 board_wallclock;
__u32 gpios_in;
} pack_mainR_t;

typedef struct __attribute__ ((packed)) {
__s32 velocity;
} stepper_parameters;

typedef struct __attribute__ ((packed)) {
__u32 pwm_width;
__u32 pwm_period;
} pwm_parameters;


typedef struct __attribute__ ((packed)) {
stepper_parameters steppers[steppers_count];
__u32 dirtime;
__u32 steptime;
__u32 steppers_ctrlword;
__u64 apply_time;
__u32 gpios_out;
pwm_parameters pwm[pwm_count];
} pack_mainW_t;

static inline int64_t extend(int64_t newlow, int nbits) {
    __u64 mask = (1<<(nbits-1)); //sign bit
    __u64 mask2 = ((1<<nbits)-1); //all bits of a number
    const __u64 max64=0xFFFFFFFFFFFFFFFF;
	//if (newlow&(1<<61)mask)  //number<0
	if (newlow & 0x2000000000000000)
		return newlow|0xD000000000000000; //(newlow|(max64^mask2));
	else
		return newlow;
}



