/*
V0.1 Initial release
*/

#include "rtapi_ctype.h"	/* isspace() */
#include "rtapi.h"			/* RTAPI realtime OS API */
#include "rtapi_app.h"		/* RTAPI realtime module decls */
#include "hal.h"			/* HAL public API decls */
#include <linux/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include "colorcnc.h"
#include "etherbone.h"
#include "rtapi_math64.h"
#include "rtapi_math.h"

//#define Max_pause 16777215

/* module information */
MODULE_AUTHOR("romanetz");
MODULE_DESCRIPTION("colorcnc board driver");
MODULE_LICENSE("GPL");


/***********************************************************************
*                STRUCTURES AND GLOBAL VARIABLES                       *
************************************************************************/
static char *board_ip="192.168.0.50";
RTAPI_MP_ARRAY_STRING(board_ip, 1, "ip address of ethernet board");

static int speedrange=0;
RTAPI_MP_INT(speedrange, "Speed range 0..3");

#define COLORCNC_SPEED_NS (20)
#define COLORCNC_SPEED    (COLORCNC_SPEED_NS * 1e-9)
#define COLORCNC_FREQ     (1e9 / COLORCNC_SPEED_NS)
#define TMAX           ((1<<10)-1)

#define W 32
#define F 30
#define MODULO ((1<<(W+F))-1)
#define MASK ((1<<(W+F))-1)
#define MAXDELTA (MASK/2)

#define ONE (1<<F)
#define MAX_STEP_RATE (1<<(F-1))

static data_t *device_data;

/* other globals */
static int comp_id;		/* component ID */
static int num_ports;		/* number of ports configured */

/***********************************************************************
*                  LOCAL FUNCTION DECLARATIONS                         *
************************************************************************/
/* These is the functions that actually do the I/O
   everything else is just init code
*/
static void update_port(void *arg, long period);

#if defined(__FreeBSD__)
#include <sys/endian.h>
#else
#include <endian.h>
#endif
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <errno.h>
#include <linux/errno.h>
#include <stdlib.h>
#include <netdb.h>
#include <sys/socket.h>
#include <netinet/in.h>

#include "etherbone.h"
#include "rtapi_ctype.h"	/* isspace() */
#include "rtapi.h"			/* RTAPI realtime OS API */
#include "rtapi_app.h"		/* RTAPI realtime module decls */




int eb_unfill_read32(uint8_t wb_buffer[20]) {
    int buffer;
    uint32_t intermediate;
    memcpy(&intermediate, &wb_buffer[16], sizeof(intermediate));
    intermediate = be32toh(intermediate);
    memcpy(&buffer, &intermediate, sizeof(intermediate));
    return buffer;
}

int eb_fill_readwrite32(uint8_t wb_buffer[20], uint32_t data, uint32_t address, int is_read) {
    memset(wb_buffer, 0, 20);
    wb_buffer[0] = 0x4e;	// Magic byte 0
    wb_buffer[1] = 0x6f;	// Magic byte 1
    wb_buffer[2] = 0x10;	// Version 1, all other flags 0
    wb_buffer[3] = 0x44;	// Address is 32-bits, port is 32-bits
    wb_buffer[4] = 0;		// Padding
    wb_buffer[5] = 0;		// Padding
    wb_buffer[6] = 0;		// Padding
    wb_buffer[7] = 0;		// Padding

    // Record
    wb_buffer[8] = 0;		// No Wishbone flags are set (cyc, wca, wff, etc.)
    wb_buffer[9] = 0x0f;	// Byte enable

    if (is_read) {
        wb_buffer[10] = 0;  // Write count
        wb_buffer[11] = 1;	// Read count
        data = htobe32(address);
        memcpy(&wb_buffer[16], &data, sizeof(data));
    }
    else {
        wb_buffer[10] = 1;	// Write count
        wb_buffer[11] = 0;  // Read count
        address = htobe32(address);
        memcpy(&wb_buffer[12], &address, sizeof(address));

        data = htobe32(data);
        memcpy(&wb_buffer[16], &data, sizeof(data));
    }
    return 20;
}

int eb_fill_write32(uint8_t wb_buffer[20], uint32_t data, uint32_t address) {
    return eb_fill_readwrite32(wb_buffer, data, address, 0);
}

int eb_fill_read32(uint8_t wb_buffer[20], uint32_t address) {
    return eb_fill_readwrite32(wb_buffer, 0, address, 1);
}

int eb_send(struct eb_connection *conn, const void *bytes, size_t len) {
    if (conn->is_direct)
        return sendto(conn->fd, bytes, len, 0, conn->addr->ai_addr, conn->addr->ai_addrlen);
    return write(conn->fd, bytes, len);
}

int eb_recv(struct eb_connection *conn, void *bytes, size_t max_len) {
    if (conn->is_direct)
        return recvfrom(conn->read_fd, bytes, max_len, 0, NULL, NULL);
    return read(conn->fd, bytes, max_len);
}

void eb_write32(struct eb_connection *conn, uint32_t val, uint32_t addr) {
    uint8_t raw_pkt[20];
    eb_fill_write32(raw_pkt, val, addr);
    eb_send(conn, raw_pkt, sizeof(raw_pkt));
}

uint32_t eb_read32(struct eb_connection *conn, uint32_t addr) {
    uint8_t raw_pkt[20];
    eb_fill_read32(raw_pkt, addr);

    eb_send(conn, raw_pkt, sizeof(raw_pkt));

    int count = eb_recv(conn, raw_pkt, sizeof(raw_pkt));
    if (count != sizeof(raw_pkt)) {
        fprintf(stderr, "unexpected read length: %d\n", count);
        return -1;
    }
    return eb_unfill_read32(raw_pkt);
}

struct eb_connection *eb_connect(const char *addr, const char *port, int is_direct) {

    struct addrinfo hints;
    struct addrinfo* res = 0;
    int err;
    int sock;

    struct eb_connection *conn = malloc(sizeof(struct eb_connection));
    if (!conn) {
        rtapi_print_msg(RTAPI_MSG_ERR,"colorcnc: couldn't allocate memory for eb_connection");
        return NULL;
    }

    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = is_direct ? SOCK_DGRAM : SOCK_STREAM;
    hints.ai_protocol = is_direct ? IPPROTO_UDP : IPPROTO_TCP;
    hints.ai_flags = AI_ADDRCONFIG;
    err = getaddrinfo(addr, port, &hints, &res);
    if (err != 0) {
        rtapi_print_msg(RTAPI_MSG_ERR,"colorcnc: failed to resolve remote socket address (err=%d / %s)\n", err, gai_strerror(err));
        free(conn);
        return NULL;
    }

    conn->is_direct = is_direct;

    if (is_direct) {
        // Rx half
        struct sockaddr_in si_me;

        memset((char *) &si_me, 0, sizeof(si_me));
        si_me.sin_family = res->ai_family;
        si_me.sin_port = ((struct sockaddr_in *)res->ai_addr)->sin_port;
        si_me.sin_addr.s_addr = htobe32(INADDR_ANY);

        int rx_socket;
        if ((rx_socket = socket(res->ai_family, res->ai_socktype, res->ai_protocol)) == -1) {
            rtapi_print_msg(RTAPI_MSG_ERR, "colorcnc: Unable to create Rx socket: %s\n", strerror(errno));
            freeaddrinfo(res);
            free(conn);
            return NULL;
        }
        if (bind(rx_socket, (struct sockaddr*)&si_me, sizeof(si_me)) == -1) {
            rtapi_print_msg(RTAPI_MSG_ERR, "colorcnc: Unable to bind Rx socket to port: %s\n", strerror(errno));
            close(rx_socket);
            freeaddrinfo(res);
            free(conn);
            return NULL;
        }
		struct timeval timeout;
		timeout.tv_sec = 0;
		timeout.tv_usec = 10000; //FIRST_PACKETS
		err = setsockopt(rx_socket, SOL_SOCKET, SO_RCVTIMEO, (char *)&timeout, sizeof(timeout));
		if (err < 0) {
			rtapi_print_msg(RTAPI_MSG_ERR,"colorcnc: could NOT to set setsockopt for tx\n");
            free(conn);
            return NULL;
		}
        // Tx half
        int tx_socket = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
        if (tx_socket == -1) {
            rtapi_print_msg(RTAPI_MSG_ERR,"colorcnc: Unable to create socket: %s\n", strerror(errno));
            close(rx_socket);
            close(tx_socket);
            freeaddrinfo(res);
            rtapi_print_msg(RTAPI_MSG_ERR,"colorcnc: unable to create socket: %s\n", strerror(errno));
            free(conn);
            return NULL;
        }

		timeout.tv_usec = SEND_TIMEOUT_US;
		err = setsockopt(tx_socket, SOL_SOCKET, SO_SNDTIMEO, (char *)&timeout, sizeof(timeout));
		if (err < 0) {
			rtapi_print_msg(RTAPI_MSG_ERR,"colorcnc: could NOT to set setsockopt for tx\n");
            free(conn);
            return NULL;
		}

        conn->read_fd = rx_socket;
        conn->fd = tx_socket;
        conn->addr = res;
    }
    else {
        sock = socket(AF_INET, SOCK_STREAM, 0);
        if (sock == -1) {
            rtapi_print_msg(RTAPI_MSG_ERR, "colorcnc: failed to create socket: %s\n", strerror(errno));
            freeaddrinfo(res);
            free(conn);
            return NULL;
        }

        int connection = connect(sock, res->ai_addr, res->ai_addrlen);
        if (connection == -1) {
            close(sock);
            freeaddrinfo(res);
            rtapi_print_msg(RTAPI_MSG_ERR,"colorcnc: unable to create socket: %s\n", strerror(errno));
            free(conn);
            return NULL;
        }

        conn->fd = sock;
        conn->addr = res;
    }

    return conn;
}

void eb_disconnect(struct eb_connection **conn) {
    if (!conn || !*conn)
        return;

    freeaddrinfo((*conn)->addr);
    close((*conn)->fd);
    if ((*conn)->read_fd)
        close((*conn)->read_fd);
    free(*conn);
    *conn = NULL;
    return;
}


/***********************************************************************
*                       INIT AND EXIT CODE                             *
************************************************************************/

int rtapi_app_main(void)
{
    char name[HAL_NAME_LEN + 1];
    int retval,n,i,r ;

    num_ports = 1;
    n = 0;


    /* STEP 1: initialise the driver */
    comp_id = hal_init(driver_NAME);
    if (comp_id < 0) {
    	rtapi_print_msg(RTAPI_MSG_ERR,"colorcnc: ERROR: hal_init() failed\n");
    	goto fail0;
    }


    /* STEP 2: allocate shared memory for to_hal data */
    device_data = hal_malloc(num_ports * sizeof(data_t));
    if (device_data == 0) {
    	rtapi_print_msg(RTAPI_MSG_ERR,"colorcnc: ERROR: hal_malloc() failed\n");
		r = -1;
		goto fail0;
    }

//###################################################
///// INIT ETH BOARD ; OPEN SOCKET
//###################################################
    device_data->eb = eb_connect("192.168.0.50", "1234", 1);

	if (!device_data->eb)
    {
        rtapi_print_msg(RTAPI_MSG_ERR,"colorcnc: ERROR: failed to connect to board\n");
        goto fail1;
    }

  	device_data->first_run = 1;
	//device_data->block = 0; //local block
//######################################################
//######### EXPORT SIGNALS, PIN, FUNCTION
//######################################################

//////////////////////
//////////////////////
//////////////////////
/* Export IO pin's */

//########################## INPUTS ###############################
    /* export write only HAL pin's for the input bit */
    for ( i=0; i<di_count;i++) {
		retval = hal_pin_bit_newf(HAL_OUT, &(device_data->digital_in[i]),comp_id, "colorcnc.%d.pins.pin-%02d-in", 1, i);
      	if (retval < 0) {
			rtapi_print_msg(RTAPI_MSG_ERR, "colorcnc: ERROR: port %d var export failed with err=%i\n", i + 1,retval);
			r = -1;
			goto fail1;
		}

      	retval = hal_pin_bit_newf(HAL_OUT, &(device_data->digital_in_n[i]),comp_id, "colorcnc.%d.pins.pin-%02d-in-n", 1, i);
      	if (retval < 0) {
        	rtapi_print_msg(RTAPI_MSG_ERR, "colorcnc: ERROR: port %d var export failed with err=%i\n", i + 1,retval);
        	r = -1;
        	goto fail1;
      	}

    }


//########################## OUTPUTS ###############################
    /* export read only HAL pin's for the output bit */
    for ( i=0; i<do_count;i++) {
		retval = hal_pin_bit_newf(HAL_IN, &(device_data->digital_out[i]),comp_id, "colorcnc.%d.pins.pin-%02d-out", 1, i);

		if (retval < 0) {
			rtapi_print_msg(RTAPI_MSG_ERR,"colorcnc: ERROR: port %d var export failed with err=%i\n", i + 1,retval);
			r = -1;
			goto fail1;
		}
    }


///////////////////////////
///////////////////////////
///////////////////////////
/* export read only HAL pin's for the control bit */

	retval = hal_pin_bit_newf(HAL_IN, &(device_data->enable_dr),comp_id, "colorcnc.%d.enable_drive", 1);

	if (retval < 0) {
		rtapi_print_msg(RTAPI_MSG_ERR,"colorcnc: ERROR: enable_dr var export failed with err=%i\n",retval);
		r = -1;
		goto fail1;
	}

	retval = hal_pin_u32_newf(HAL_OUT, &(device_data->board_wallclock_lsb),comp_id, "colorcnc.%d.board_wallclock_lsb", 1);

	if (retval < 0) {
		rtapi_print_msg(RTAPI_MSG_ERR,"colorcnc: ERROR: board_wallclock var export failed with err=%i\n",retval);
		r = -1;
		goto fail1;
	}

	retval = hal_pin_u32_newf(HAL_OUT, &(device_data->board_wallclock_msb),comp_id, "colorcnc.%d.board_wallclock_msb", 1);

	if (retval < 0) {
		rtapi_print_msg(RTAPI_MSG_ERR,"colorcnc: ERROR: board_wallclock var export failed with err=%i\n",retval);
		r = -1;
		goto fail1;
	}


	/*retval = hal_pin_bit_newf(HAL_OUT, &(device_data->WD_lock),comp_id, "colorcnc.%d.WD_lock", 1);

	if (retval < 0) {
		rtapi_print_msg(RTAPI_MSG_ERR,"colorcnc: ERROR: enable_dr var export failed with err=%i\n",retval);
		r = -1;
		goto fail1;
	}
*/
/////////////////////////////
/////////////////////////////
/////////////////////////////
/* export encoder signal */
/*
	for ( i=0; i<=(NUM_chanal-1);i++) {
		// encoder_count
		retval = hal_pin_float_newf(HAL_OUT, &(device_data-> enccounts[i]), comp_id, "colorcnc.%d.feedback.encoder%d", 1, i);
		if (retval < 0) {
			rtapi_print_msg(RTAPI_MSG_ERR,"colorcnc: ERROR:  err encoder=%i\n", retval);
			r = -1;
			goto fail1;
		}
        // encoder_scale
		retval = hal_pin_float_newf(HAL_IN, &(device_data-> encscale[i]), comp_id, "colorcnc.%d.feedback.enc_scale%d", 1, i);
		if (retval < 0) {
			rtapi_print_msg(RTAPI_MSG_ERR,"colorcnc: ERROR:  err enc_scale=%i\n", retval);
			r = -1;
			goto fail1;
		}
		*device_data-> encscale[i] = 1;

        // index
		retval = hal_pin_bit_newf(HAL_IO, &(device_data->index_en[i]),comp_id, "colorcnc.%d.feedback.index_en%01d", 1, i);
		if (retval < 0) {
			rtapi_print_msg(RTAPI_MSG_ERR,"colorcnc: ERROR: port %d var export failed with err=%i\n", n + 1,retval);
			r = -1;
			goto fail1;
		}
        // encoder_velocity
		retval = hal_pin_float_newf(HAL_IN, &(device_data-> encvel[i]), comp_id, "colorcnc.%d.feedback.enc_vel%d", 1, i);
		if (retval < 0) {
			rtapi_print_msg(RTAPI_MSG_ERR,"colorcnc: ERROR:  err enc_velocity=%i\n", retval);
			r = -1;
			goto fail1;
		}
	}

*/

//////////////////////////////
/////////////////////////////
/////////////////////////////
	/* export control drive STEP_DIR*/

	for ( i=0; i<steppers_count;i++) {

       r = hal_pin_float_newf(HAL_IN, &(device_data->stepgen_position_cmd[i]), comp_id, "colorcnc.%d.stepgen.%01d.position-cmd", 1, i);
        if(r != 0) return r;
       r = hal_pin_float_newf(HAL_IN, &(device_data->stepgen_velocity_cmd[i]), comp_id, "colorcnc.%d.stepgen.%01d.velocity-cmd", 1, i);
        if(r != 0) return r;

        r = hal_pin_float_newf(HAL_OUT, &(device_data->stepgen_velocity_fb[i]), comp_id, "colorcnc.%d.stepgen.%01d.velocity-fb", 1, i);
        if(r != 0) return r;

        r = hal_pin_float_newf(HAL_OUT, &(device_data->stepgen_position_fb[i]), comp_id, "colorcnc.%d.stepgen.%01d.position-fb", 1, i);
        if(r != 0) return r;

        r = hal_pin_s32_newf(HAL_OUT, &(device_data->stepgen_counts[i]), comp_id, "colorcnc.%d.stepgen.%01d.counts", 1, i);
        if(r != 0) return r;

        r = hal_pin_bit_newf(HAL_IN, &(device_data->stepgen_enable[i]), comp_id, "colorcnc.%d.stepgen.%01d.enable", 1, i);
        if(r != 0) return r;

        r = hal_pin_bit_newf(HAL_IN, &(device_data->stepgen_reset[i]), comp_id, "colorcnc.%d.stepgen.%01d.reset", 1, i);
        if(r != 0) return r;

        r = hal_param_float_newf(HAL_RW, &(device_data->stepgen_scale[i]), comp_id, "colorcnc.%d.stepgen.%01d.scale", 1, i);
        if(r != 0) return r;
        device_data->stepgen_scale[i] = 1.0;

        r = hal_param_float_newf(HAL_RW, &(device_data->stepgen_maxvel[i]), comp_id, "colorcnc.%d.stepgen.%01d.maxvel", 1, i);
        if(r != 0) return r;
        device_data->stepgen_maxvel[i] = 0;

        r = hal_param_bit_newf(HAL_RW, &(device_data->stepgen_mode[i]), comp_id, "colorcnc.%d.stepgen.%01d.mode", 1, i);
        if(r != 0) return r;
        device_data->stepgen_mode[i] = 0;

        r = hal_param_s32_newf(HAL_RO, &(device_data->stepgen_rate[i]), comp_id, "colorcnc.%d.stepgen.%01d.rate", 1, i);
        if(r != 0) return r;

        r = hal_param_u32_newf(HAL_RO,((char*)&(device_data->rcvd_pos[i]))+4, comp_id, "colorcnc.%d.stepgen.%01d.rcvd_position_msb", 1, i);
        if(r != 0) return r;
        
        r = hal_param_u32_newf(HAL_RO,&(device_data->rcvd_pos[i]), comp_id, "colorcnc.%d.stepgen.%01d.rcvd_position_lsb", 1, i);
        if(r != 0) return r;
        

			/* feed back step/dir */
/*		retval = hal_pin_float_newf(HAL_OUT, &(device_data-> position[i]), comp_id, "colorcnc.%d.stepgen.position%d", 1, i);
		if (retval < 0) {
			rtapi_print_msg(RTAPI_MSG_ERR,"colorcnc: ERROR:  err stepgen=%i\n", retval);
			r = -1;
			goto fail1;
		}
*/

			/* DIRC step/dir */
	/*	retval = hal_pin_bit_newf(HAL_IN, &(device_data->invert_dir[i]), comp_id, "colorcnc.%d.step_dir.invert_dir%d", 1, i);
		if (retval < 0) {
			rtapi_print_msg(RTAPI_MSG_ERR,"colorcnc: ERROR:  err invert_dir=%i\n", retval);
			r = -1;
			goto fail1;
		}
		*device_data->invert_dir[i] = 0;
    */

	/*	retval = hal_pin_float_newf(HAL_OUT, &(device_data-> velocity[i]), comp_id, "colorcnc.%d.stepgen.velocity%d", 1, i);
		if (retval < 0) {
			rtapi_print_msg(RTAPI_MSG_ERR,"colorcnc: ERROR:  err stepgen velocity=%i\n", retval);
			r = -1;
			goto fail1;
		}
*/

    }

            r = hal_param_float_newf(HAL_RO, &(device_data->fmax_limit1), comp_id, "colorcnc.%d.stepgen.fmax_limit1", 1);
        if(r != 0) return r;
            r = hal_param_float_newf(HAL_RO, &(device_data->fmax_limit2), comp_id, "colorcnc.%d.stepgen.fmax_limit2", 1);
        if(r != 0) return r;
            r = hal_param_float_newf(HAL_RO, &(device_data->fmax_limit3), comp_id, "colorcnc.%d.stepgen.fmax_limit3", 1);
        if(r != 0) return r;
            r = hal_param_float_newf(HAL_RO, &(device_data->t_period_real), comp_id, "colorcnc.%d.t_period_real", 1);
        if(r != 0) return r;



	        retval = hal_param_bit_newf(HAL_RW, &(device_data->stepgen_step_polarity), comp_id, "colorcnc.%d.stepgen.step-polarity", 1);
		if (retval < 0) {
			rtapi_print_msg(RTAPI_MSG_ERR,"colorcnc: ERROR:  err step-polarity=%i\n", retval);
			r = -1;
			goto fail1;
		}

		retval = hal_param_u32_newf(HAL_RW, &(device_data-> stepgen_steplen), comp_id, "colorcnc.%d.stepgen.steplen", 1, i);
		if (retval < 0) {
			rtapi_print_msg(RTAPI_MSG_ERR,"colorcnc: ERROR:  err steptime=%i\n", retval);
			r = -1;
			goto fail1;
		}

		retval = hal_param_u32_newf(HAL_RW, &(device_data-> stepgen_dirtime), comp_id, "colorcnc.%d.stepgen.dirtime", 1, i);
		if (retval < 0) {
			rtapi_print_msg(RTAPI_MSG_ERR,"colorcnc: ERROR:  err dirtime=%i\n", retval);
			r = -1;
			goto fail1;
		}

for ( i=0; i<pwm_count;i++) {
		retval = hal_pin_u32_newf(HAL_IN, &(device_data-> pwm_width[i]), comp_id, "colorcnc.%d.pwm.width%d", 1, i);
		if (retval < 0) {
			rtapi_print_msg(RTAPI_MSG_ERR,"colorcnc: ERROR:  err steptime=%i\n", retval);
			r = -1;
			goto fail1;
		}
		retval = hal_pin_u32_newf(HAL_IN, &(device_data-> pwm_period[i]), comp_id, "colorcnc.%d.pwm.period%d", 1, i);
		if (retval < 0) {
			rtapi_print_msg(RTAPI_MSG_ERR,"colorcnc: ERROR:  err dirtime=%i\n", retval);
			r = -1;
			goto fail1;
		}
    }
/*	else if(device_data->PWM_board_num == 1)
{

	/////////////////////////////
	// export control drive PWM


	for ( i=0; i<=(NUM_chanal-1);i++) {
		retval = hal_pin_float_newf(HAL_IN, &(device_data-> dcontrol[i]), comp_id, "colorcnc.%d.PWM.dcontrol%d", 1, i);
		if (retval < 0) {
			rtapi_print_msg(RTAPI_MSG_ERR,"colorcnc: ERROR:  err dcontrol=%i\n", retval);
			r = -1;
			goto fail1;
		}

		retval = hal_pin_float_newf(HAL_IN, &(device_data-> outscale[i]), comp_id, "colorcnc.%d.PWM.out_scale%d", 1, i);
		if (retval < 0) {
			rtapi_print_msg(RTAPI_MSG_ERR,"colorcnc: ERROR:  err out_scale=%i\n", retval);
			r = -1;
			goto fail1;
		}
	}

}
*/
//////////////////////////////
/////////////////////////////
/////////////////////////////
// Export TESTS signls
/*

	retval = hal_pin_u32_newf(HAL_OUT, &(device_data->test),comp_id, "colorcnc.%d.TEST", 1);
      	if (retval < 0) {
			rtapi_print_msg(RTAPI_MSG_ERR, "colorcnc: ERR: port %d TEST var export failed with err=%i\n", n + 1,retval);
			r = -1;
			goto fail1;
		}
	retval = hal_pin_u32_newf(HAL_OUT, &(device_data->testi),comp_id, "colorcnc.%d.TEST_INT", 1);
      	if (retval < 0) {
			rtapi_print_msg(RTAPI_MSG_ERR, "colorcnc: ERR: port %d TEST var export failed with err=%i\n", n + 1,retval);
			r = -1;
			goto fail1;
		}

	*device_data->testi = 0;
*/
////////////////////////////////////////
//// UPDATE  export function
////////////////////////////////////////

    /* STEP 4: export function */
    rtapi_snprintf(name, sizeof(name), "colorcnc.%d.update", n + 1);
    retval = hal_export_funct(name, update_port, device_data, 1, 0,comp_id);
    if (retval < 0) {
		rtapi_print_msg(RTAPI_MSG_ERR,"colorcnc: ERROR: port %d write funct export failed\n", n + 1);
		r = -1;
		goto fail1;
    }


    rtapi_print_msg(RTAPI_MSG_ERR,"colorcnc: installed driver for %d card(s)\n", num_ports);
    hal_ready(comp_id);
    return 0;


//####### ERROR ############

fail1:

	eb_disconnect(&device_data->eb);

fail0:
    hal_exit(comp_id);
return r;

}
//####### EXIT ############
void rtapi_app_exit(void)
{
	eb_disconnect(&device_data->eb );
	hal_exit(comp_id);
}



//###########################################################################################################
//
//			FUNCTIONS
//
//###########################################################################################################

#undef fperiod
#define fperiod (period * 1e-9)
#undef stepgen_position_cmd
#define stepgen_position_cmd(i) (0+*(port->stepgen_position_cmd[i]))
#undef stepgen_velocity_cmd
#define stepgen_velocity_cmd(i) (0+*(port->stepgen_velocity_cmd[i]))
#undef stepgen_velocity_fb
#define stepgen_velocity_fb(i) (*(port->stepgen_velocity_fb[i]))
#undef stepgen_position_fb
#define stepgen_position_fb(i) (*(port->stepgen_position_fb[i]))
#undef stepgen_counts
#define stepgen_counts(i) (*(port->stepgen_counts[i]))
#undef stepgen_enable
#define stepgen_enable(i) (0+*(port->stepgen_enable[i]))
#undef stepgen_reset
#define stepgen_reset(i) (0+*(port->stepgen_reset[i]))
#undef stepgen_scale
#define stepgen_scale(i) (port->stepgen_scale[i])
#undef stepgen_maxvel
#define stepgen_maxvel(i) (port->stepgen_maxvel[i])
#undef stepgen_step_polarity
#define stepgen_step_polarity (port->stepgen_step_polarity)
#undef stepgen_steplen
#define stepgen_steplen (port->stepgen_steplen)
#undef stepgen_stepspace
#define stepgen_stepspace (port->stepgen_stepspace)
#undef stepgen_dirtime
#define stepgen_dirtime (port->stepgen_dirtime)
#undef debug_0
#define debug_0 (port->debug_0)
#undef debug_1
#define debug_1 (port->debug_1)
#undef debug_2
#define debug_2 (port->debug_2)
#undef debug_3
#define debug_3 (port->debug_3)

/**************************************************************
###############################################################
* REALTIME PORT WRITE FUNCTION                                *
###############################################################
**************************************************************/



void update_port(void *arg, long period){
	data_t *port;
    __u8 num_records_to_read,num_records_to_write;
    __u32 tmp1=0, tmp2=0;
    __u32 tmp_data;
    int i,res;
    int r = 0;

	port = arg;

    int stepspace_ticks = stepgen_stepspace/COLORCNC_SPEED_NS;
    int steplen_ticks = stepgen_steplen/COLORCNC_SPEED_NS;
    int dirtime_ticks = stepgen_dirtime/COLORCNC_SPEED_NS;
    int rate, maxrate = MAX_STEP_RATE;
    double fmax;
	volatile __u8 wb_wr_buffer[20+100*4];
	volatile __u8 wb_rd_buffer[20+100*4];
	pack_mainR_t *recv_payload;
	pack_mainW_t send_payload;

	if(port->first_run)
	{
		//fill etherbone packet headers
		eb_fill_readwrite32(wb_wr_buffer, 0, 0x0, 1);
        wb_wr_buffer[11]=2;
        *(uint32_t*)&wb_wr_buffer[12]=htobe32(0); //own address
        *(uint32_t*)&wb_wr_buffer[16]=htobe32(0x88); //data address
        *(uint32_t*)&wb_wr_buffer[20]=htobe32(0x88+4); //data address
        // #################  TRANSMIT DATA  ###############################################################
        int res=eb_send(device_data->eb, wb_wr_buffer, 24);
        if (res>0)
        {
            int count = eb_recv(device_data->eb, wb_rd_buffer, sizeof(wb_rd_buffer));
            if (count==24)
            {
                port->board_wallclock=htobe64(*(__u64*)&wb_rd_buffer[16]);
                rtapi_print_msg(RTAPI_MSG_INFO,"colorcnc: INFO: colorcnc timestamp %llu\n", port->board_wallclock);
            }
        }
		port->first_run=0;
	}
	else
	{

        if(steplen_ticks > TMAX)
            {
            steplen_ticks = TMAX;
            rtapi_print_msg(RTAPI_MSG_ERR,"Requested step length %dns decreased to %dns due to hardware limitations\n", stepgen_steplen, TMAX * COLORCNC_SPEED_NS);
            stepgen_steplen = TMAX * COLORCNC_SPEED_NS;
        }

        if(dirtime_ticks > TMAX)
            {
            dirtime_ticks = TMAX;
            rtapi_print_msg(RTAPI_MSG_ERR,"Requested direction change time %dns decreased to %dns due to hardware limitations\n", stepgen_dirtime, TMAX * COLORCNC_SPEED_NS);
            stepgen_dirtime = TMAX * COLORCNC_SPEED_NS;
        }

        // Speed limits come from several sources
        // First limit: step waveform timings
        fmax = 1. / COLORCNC_SPEED / (2 + steplen_ticks + stepspace_ticks);
        port->fmax_limit1=fmax;
        // Second limit: highest speed command
      /*  if(fmax > COLORCNC_FREQ / (2<<speedrange))
            {fmax = COLORCNC_SPEED * (2<<speedrange);
            port->fmax_limit2=fmax;
            }
        // Third limit: max sign-extenable counts per period
        if(fmax > MAXDELTA / fperiod / (1<<speedrange))
            {fmax = MAXDELTA / fperiod / (1<<speedrange);
            port->fmax_limit3=fmax;
            }
        */
        for(i=0; i<steppers_count; i++) {
        //fetch new position command from motion control task
        double new_position_cmd = stepgen_position_cmd(i);
        //calculate required speed
        double v;
        if (port->stepgen_mode[i])
         {v = new_position_cmd - port->old_position_cmd[i];}
        else
         {v = stepgen_velocity_cmd(i);}
        //estimate position error
        double est_err = stepgen_position_fb(i) + port->old_velocity_cmd[i] * fperiod - new_position_cmd;
        double actual_max;
        //absolute value of scale
        double scale_abs = fabs(stepgen_scale(i));

        v = v - debug_2 * est_err / fperiod;
            if(v > 0) v = v + .5/scale_abs;
            else if(v < 0) v = v - .5/scale_abs;
        //store position command
        port->old_position_cmd[i] = new_position_cmd;
        //store (corrected?) velocity command
        port->old_velocity_cmd[i] = v;
            actual_max = fmax / scale_abs;
            //implement boundaries on velocity
            if(stepgen_maxvel(i) < 0) stepgen_maxvel(i) = -stepgen_maxvel(i);
            if(stepgen_maxvel(i) != 0 && stepgen_maxvel(i) > actual_max) {
                static int message_printed[6] = {0,0,0,0,0,0};
                //notify user about limit of velocity
                if(!message_printed[i]) {
                    rtapi_print_msg(RTAPI_MSG_ERR,
                        "Requested step rate %dHz decreased to %dHz due to hardware or timing limitations\n",
                        (int)(stepgen_maxvel(i) * scale_abs),
                        (int)(fmax));
                    message_printed[i] = 1;
                }
                stepgen_maxvel(i) = actual_max;
            }

            if(stepgen_maxvel(i) == 0) {
                if(v < -actual_max) v = -actual_max;
                if(v > actual_max) v = actual_max;
            } else {
                if(v < -stepgen_maxvel(i)) v = -stepgen_maxvel(i);
                if(v > stepgen_maxvel(i)) v = stepgen_maxvel(i);
            }
        //velocity expressed in parts of 1
        rate = v * stepgen_scale(i) * ONE * COLORCNC_SPEED / (1<<speedrange);

        if(rate > maxrate) rate = maxrate;
        if(rate < -maxrate) rate = -maxrate;

        if(!(*(port->enable_dr))) rate = 0;

        send_payload.steppers[i].velocity=rate;
        port->stepgen_rate[i]=rate;
        }

        send_payload.steptime=steplen_ticks;
        send_payload.dirtime=dirtime_ticks;

        for (i=0;i<pwm_count;i++)
        {
        send_payload.pwm[i].pwm_period=*(port->pwm_period[i]);
        send_payload.pwm[i].pwm_width=*(port->pwm_width[i]);
        }

        __u32 tmp=1;
        send_payload.gpios_out=0;
        for (i=0;i<do_count;i++)
        {
        if (port->digital_out[i])
            send_payload.gpios_out|= *(port->digital_out[i])?tmp:0;
        tmp<<=1;
        }
        send_payload.apply_time=port->board_wallclock+(BOARD_CLK*SERVO_PERIOD)/1000000ULL;

        send_payload.steppers_ctrlword=0;
        send_payload.steppers_ctrlword|=*(port->enable_dr)?1:0;

        memset((void*)wb_wr_buffer,0,sizeof(wb_wr_buffer));
        //fill etherbone packet headers
        eb_fill_readwrite32(wb_wr_buffer, 0, 0x0, 1);

        num_records_to_read=15;//(sizeof(pack_mainR_t))/sizeof(__u32);
        num_records_to_write=(sizeof(send_payload))/sizeof(__u32);

        wb_wr_buffer[10]=num_records_to_write;   //number of registers to write + (slave) start address
        wb_wr_buffer[11]=0;   //number of registers to write + (slave) start address

        tmp_data=htobe32(0); //fill addresses to read
        memcpy((void*)&wb_wr_buffer[12],(void*)&tmp_data,4);
        //store stepgen parameters, apply time and gpios
        __u64 tmp_data64;
        __u32* tmp_ptr=&send_payload;
        for (i=0;i<num_records_to_write;i++)
        {
            if (i==((int)(((void*)&send_payload.apply_time)-((void*)&send_payload)))/sizeof(__u32))
            {
                tmp_data64=htobe64(*(__u64*)(&tmp_ptr[i])); //fill data to write
                memcpy((void*)&wb_wr_buffer[16+i*4],(void*)&tmp_data64,8);
                i++;
            }
            else
            {
                tmp_data=htobe32(tmp_ptr[i]); //fill data to write
                memcpy((void*)&wb_wr_buffer[16+i*4],(void*)&tmp_data,4);
            }
        }

    // #################  TRANSMIT DATA  ###############################################################
        res=eb_send(device_data->eb, wb_wr_buffer, num_records_to_write*4+16);
    // #################################################################################################


        memset(wb_wr_buffer,0,sizeof(wb_wr_buffer));
        //fill etherbone packet headers
        eb_fill_readwrite32(wb_wr_buffer, 0, 0x0, 1);
        wb_wr_buffer[11]=num_records_to_read;	 //number of registers to read+(master) start address
        wb_wr_buffer[10]=0;	 //number of registers to write+(slave) start address

        tmp_data=htobe32(0); //fill addresses to read
        memcpy((void*)&wb_wr_buffer[12],(void*)&tmp_data,4);
        //store stepgen parameters, apply time and gpios
        
        //read positions, wallclock time and gpios
        for (int i=0;i<num_records_to_read;i++)
        {
            tmp_data=htobe32(0x48+i*4); //fill addresses to read
            memcpy((void*)&wb_wr_buffer[16+i*4],&tmp_data,4);
        }

    // #################  TRANSMIT DATA  ###############################################################
        res=eb_send(device_data->eb, wb_wr_buffer, num_records_to_read*4+16);

    // #################  RECEIVE DATA  ###############################################################

        int count = eb_recv(device_data->eb, wb_rd_buffer, sizeof(wb_rd_buffer));
        if (count <0) {
                //(24+num_records_to_read*4)) {
            fprintf(stderr, "unexpected read length: %d\n", count);
            return;
        }

        //end receive process
        else {
        //parse received data
            ///////////// read digital inputs /////////////////////////////////////////////
                //pack_mainR_t *t=(pack_mainR_t *)&wb_rd_buffer[16];
                //16+6*4+8
                //16+6*8+8
                tmp1=htobe32(*(__u32*)&wb_rd_buffer[16+6*8+8]);
                //tmp1=htobe32(t->gpios_in);
                tmp2 = 0x01;
                for (i=0 ; i < di_count ; i++) {
                    if (port->digital_in[i])
                        *(port->digital_in[i]) = (tmp1 & tmp2) ? 1:0 ;
                    if (port->digital_in_n[i])
                        *(port->digital_in_n[i]) = (tmp1 & tmp2) ? 0:1 ;
                    tmp2 <<= 1;
                }

	port->board_wallclock_old=port->board_wallclock;
	port->board_wallclock=htobe64(*(__u64*)&wb_rd_buffer[16+6*8]);//htobe64(t->board_wallclock);
	*(port->board_wallclock_msb)=htobe32(*(__u32*)&wb_rd_buffer[16+6*8]);
	*(port->board_wallclock_lsb)=htobe32(*(__u32*)&wb_rd_buffer[16+6*8+4]);
	if (port->board_wallclock>port->board_wallclock_old)
		port->t_period_real=(port->board_wallclock-port->board_wallclock_old)*(20.0e-9);//time between measurements
	else
		port->t_period_real=fperiod;
		
    //////////////////////////////////////////////////////////////////////////////
    ////////////////////STEP dir section///////////////////////////////////////////////
    //////////////////////////////////////////////////////////////////////////////

    ///////////// read s/d count /////////////////////////////////////////////

            for ( i=0; i<steppers_count;i++)
            {
                int64_t count;
                double fcount;
                int64_t newlow;
                int reset;
                //ppdata = read32();
                reset = stepgen_reset(i);
                newlow = (htobe64(*(__u64*)&wb_rd_buffer[i*8+16]));// & MASK;
				port->rcvd_pos[i]=newlow; //extend(newlow, W+F);
                count = extend(newlow, W+F);
                stepgen_velocity_fb(i) = ((double)(count - port->last_count[i])) / ((double) stepgen_scale(i)) / (port->t_period_real) / (1 << F);
                port->last_count[i] = count;
                if(reset) port->reset_count[i] = count;
                fcount = (count - port->reset_count[i]) * 1. / (1<<F);
                stepgen_counts(i) = fcount;
                stepgen_position_fb(i) = fcount / ((double) stepgen_scale(i));
            }

             /*   port->board_wallclock=htobe64(*(__u64*)&wb_rd_buffer[16+6*8]);//htobe64(t->board_wallclock);
                *(port->board_wallclock_msb)=htobe32(*(__u32*)&wb_rd_buffer[16+6*8]);
                *(port->board_wallclock_lsb)=htobe32(*(__u32*)&wb_rd_buffer[16+6*8+4]);
                */
        }
	}
}

