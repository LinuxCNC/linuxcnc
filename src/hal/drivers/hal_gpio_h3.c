/********************************************************************
 * Description:  hal_gpio_h3.c
 *               Driver for the Orange Pi (H3 SoC) GPIO pins
 *
 * Author: Mikhail Vydrenko (mikhail@vydrenko.ru)
 *
 ********************************************************************/

#include "rtapi.h"          /* RTAPI realtime OS API */
#include "rtapi_app.h"      /* RTAPI realtime module decls */
                            /* this also includes config.h */
#include "hal.h"            /* HAL public API decls */
#include <fcntl.h>
#include <sys/mman.h>
#include <unistd.h>



#if !defined(TARGET_PLATFORM_H3)
#error "This driver is for the H3 SoC platform only"
#endif




MODULE_AUTHOR("Mikhail Vydrenko");
MODULE_DESCRIPTION("Driver for the Orange Pi (H3 SoC) GPIO pins");
MODULE_LICENSE("GPL");




#define PHY_MEM_BLOCK_SIZE      4096
#define GPIO_PHY_MEM_OFFSET1    0x01C20800 // GPIO_A .. GPIO_G
#define GPIO_PHY_MEM_OFFSET2    0x01F02C00 // GPIO_L
#define GPIO_PORT_COUNT         8

enum
{
    GPIO_A, GPIO_B, GPIO_C, GPIO_D, // GPIO_B actually unused
    GPIO_E, GPIO_F, GPIO_G, GPIO_L
};

struct _GPIO_PORT_REG_t
{
    uint32_t config[4];
    uint32_t data;
    uint32_t drive[2];
    uint32_t pull[2];
};

struct _GPIO_PIN_t
{
    int8_t port;
    int8_t pin;
    int8_t OPI_pin;
};

struct _OPI_PIN_t
{
    int8_t H3_pin;
    int8_t valid;
};

struct _GPIO_PORT_t
{
    int8_t name;
    int8_t pins_count;
    int8_t id_offset;
};




static const char * comp_name = "hal_gpio_h3";

static struct _GPIO_PORT_REG_t * _GPIO[GPIO_PORT_COUNT] = {0};

static const struct _GPIO_PIN_t _H3_pins[] =
{
    // 0 - 21
    {GPIO_A, 0,13},{GPIO_A, 1,11},{GPIO_A, 2,22},{GPIO_A, 3,15},{GPIO_A, 4, 0},
    {GPIO_A, 5, 0},{GPIO_A, 6, 7},{GPIO_A, 7,29},{GPIO_A, 8,31},{GPIO_A, 9,33},
    {GPIO_A,10,35},{GPIO_A,11, 5},{GPIO_A,12, 3},{GPIO_A,13, 8},{GPIO_A,14,10},
    {GPIO_A,15, 0},{GPIO_A,16, 0},{GPIO_A,17, 0},{GPIO_A,18,28},{GPIO_A,19,27},
    {GPIO_A,20,37},{GPIO_A,21,26},
    // 22 - 40
    {GPIO_C, 0,19},{GPIO_C, 1,21},{GPIO_C, 2,23},{GPIO_C, 3,24},{GPIO_C, 4,16},
    {GPIO_C, 5, 0},{GPIO_C, 6, 0},{GPIO_C, 7,18},{GPIO_C, 8, 0},{GPIO_C, 9, 0},
    {GPIO_C,10, 0},{GPIO_C,11, 0},{GPIO_C,12, 0},{GPIO_C,13, 0},{GPIO_C,14, 0},
    {GPIO_C,15, 0},{GPIO_C,16, 0},{GPIO_C,17, 0},{GPIO_C,18, 0},
    // 41 - 58
    {GPIO_D, 0, 0},{GPIO_D, 1, 0},{GPIO_D, 2, 0},{GPIO_D, 3, 0},{GPIO_D, 4, 0},
    {GPIO_D, 5, 0},{GPIO_D, 6, 0},{GPIO_D, 7, 0},{GPIO_D, 8, 0},{GPIO_D, 9, 0},
    {GPIO_D,10, 0},{GPIO_D,11, 0},{GPIO_D,12, 0},{GPIO_D,13, 0},{GPIO_D,14,12},
    {GPIO_D,15, 0},{GPIO_D,16, 0},{GPIO_D,17, 0},
    // 59 - 74
    {GPIO_E, 0, 0},{GPIO_E, 1, 0},{GPIO_E, 2, 0},{GPIO_E, 3, 0},{GPIO_E, 4, 0},
    {GPIO_E, 5, 0},{GPIO_E, 6, 0},{GPIO_E, 7, 0},{GPIO_E, 8, 0},{GPIO_E, 9, 0},
    {GPIO_E,10, 0},{GPIO_E,11, 0},{GPIO_E,12, 0},{GPIO_E,13, 0},{GPIO_E,14, 0},
    {GPIO_E,15, 0},
    // 75 - 81
    {GPIO_F, 0, 0},{GPIO_F, 1, 0},{GPIO_F, 2, 0},{GPIO_F, 3, 0},{GPIO_F, 4, 0},
    {GPIO_F, 5, 0},{GPIO_F, 6, 0},
    // 82 - 95
    {GPIO_G, 0, 0},{GPIO_G, 1, 0},{GPIO_G, 2, 0},{GPIO_G, 3, 0},{GPIO_G, 4, 0},
    {GPIO_G, 5, 0},{GPIO_G, 6,38},{GPIO_G, 7,40},{GPIO_G, 8,32},{GPIO_G, 9,36},
    {GPIO_G,10, 0},{GPIO_G,11, 0},{GPIO_G,12, 0},{GPIO_G,13, 0},
    // 96 - 107
    {GPIO_L, 0, 0},{GPIO_L, 1, 0},{GPIO_L, 2, 0},{GPIO_L, 3, 0},{GPIO_L, 4, 0},
    {GPIO_L, 5, 0},{GPIO_L, 6, 0},{GPIO_L, 7, 0},{GPIO_L, 8, 0},{GPIO_L, 9, 0},
    {GPIO_L,10, 0},{GPIO_L,11, 0}
};

#define H3_PINS_COUNT (sizeof _H3_pins / sizeof _H3_pins[0])

static const struct _GPIO_PORT_t _GPIO_port_info[] =
{
    {'A',22, 0},
    {'B', 0,22}, // actually unused
    {'C',19,22},
    {'D',18,41},
    {'E',16,59},
    {'F', 7,75},
    {'G',14,82},
    {'L',12,96}
};

static const struct _OPI_PIN_t _OPI_pins[] =
{
    // dummy
    {-4,0},

    // general pins 1-40
    {-2,0},     {-3,0},     //  +3.3V   +5V
    {12,1},     {-3,0},     //  PA12    +5V
    {11,1},     {-1,0},     //  PA11    GND
    {6,1},      {13,1},     //  PA6     PA13
    {-1,0},     {14,1},     //  GND     PA14
    {1,1},      {55,1},     //  PA1     PD14
    {0,1},      {-1,0},     //  PA0     GND
    {3,1},      {26,1},     //  PA3     PC4
    {-2,0},     {29,1},     //  +3.3V   PC7
    {22,1},     {-1,0},     //  PC0     GND
    {23,1},     {2,1},      //  PC1     PA2
    {24,1},     {25,1},     //  PC2     PC3
    {-1,0},     {21,1},     //  GND     PA21
    {19,1},     {18,1},     //  PA19    PA18
    {7,1},      {-1,0},     //  PA7     GND
    {8,1},      {90,1},     //  PA8     PG8
    {9,1},      {-1,0},     //  PA9     GND
    {10,1},     {91,1},     //  PA10    PG9
    {20,1},     {88,1},     //  PA20    PG6
    {-1,0},     {89,1},     //  GND     PG7

    // pins 41/42 are serial console TX,RX pins
    {4,1},      {5,1}       //  PA4     PA5
};

#define OPI_PINS_COUNT (sizeof _OPI_pins / sizeof _OPI_pins[0])

static uint32_t *vrt_block_addr[2];

static int32_t comp_id; // component ID

hal_bit_t **port_data; // port data pins states
hal_bit_t **port_data_inv; // port data inverted pins states
hal_bit_t *port_param_inv; // port params for the pins invert states
hal_bit_t *port_param_reset; // port params for the pins reset states
hal_u32_t *port_reset_time;

long long port_write_time = 0;

static uint8_t input_pins_list[H3_PINS_COUNT] = {0};
static uint8_t input_pins_count = 0;
static char *input_pins;
RTAPI_MP_STRING(input_pins, "input pins, comma separated");

static uint8_t output_pins_list[H3_PINS_COUNT] = {0};
static uint8_t output_pins_count = 0;
static char *output_pins;
RTAPI_MP_STRING(output_pins, "output pins, comma separated");

static unsigned long ns2tsc_factor;
#define ns2tsc(x) (((x) * (unsigned long long)ns2tsc_factor) >> 12)




static void write_port(void *arg, long period);
static void reset_port(void *arg, long period);
static void read_port(void *arg, long period);




static void config_pin_as_input(uint8_t n)
{
    _GPIO[_H3_pins[n].port]->config[_H3_pins[n].pin / 8] &=
        ~(0b1111 << (_H3_pins[n].pin % 8 * 4));
}

static void config_pin_as_output(uint8_t n)
{
    _GPIO[_H3_pins[n].port]->config[_H3_pins[n].pin / 8] &=
        ~(0b1111 << (_H3_pins[n].pin % 8 * 4));
    _GPIO[_H3_pins[n].port]->config[_H3_pins[n].pin / 8] |=
         (0b0001 << (_H3_pins[n].pin % 8 * 4));
}




int32_t rtapi_app_main(void)
{
    int32_t     mem_fd;
    uint32_t    vrt_offset = 0;
    off_t       phy_block_addr = 0;
    int32_t     n, retval, p;
    char        *data, *token;
    uint8_t     pin;
    char        name[HAL_NAME_LEN + 1];


#if LINUX_VERSION_CODE > KERNEL_VERSION(2,6,0) &&0//FIXME
    // this calculation fits in a 32-bit unsigned
    // as long as CPUs are under about 6GHz
    ns2tsc_factor = (cpu_khz << 6) / 15625ul;
#else
    ns2tsc_factor = 1ll<<12;
#endif


    comp_id = hal_init(comp_name);
    if (comp_id < 0)
    {
        rtapi_print_msg(RTAPI_MSG_ERR,
                        "%s: ERROR: hal_init() failed\n", comp_name);
        return -1;
    }


    // open physical memory file
    if ( (mem_fd = open("/dev/mem", O_RDWR|O_SYNC) ) < 0 )
    {
       rtapi_print_msg(RTAPI_MSG_ERR,
                       "%s: ERROR: can't open /dev/mem file\n", comp_name);
       return -1;
    }

    // calculate phy memory block start
    vrt_offset = GPIO_PHY_MEM_OFFSET1 % PHY_MEM_BLOCK_SIZE;
    phy_block_addr = GPIO_PHY_MEM_OFFSET1 - vrt_offset;

    // make a block of phy memory visible in our user space
    vrt_block_addr[0] = mmap(
       NULL,                    // Any adddress in our space
       PHY_MEM_BLOCK_SIZE,      // Map length
       PROT_READ | PROT_WRITE,  // Enable reading & writting to mapped memory
       MAP_SHARED,              // Shared with other processes
       mem_fd,                  // File to map
       phy_block_addr           // Offset to GPIO peripheral
    );

    // exit program if mmap is failed
    if (vrt_block_addr[0] == MAP_FAILED)
    {
       rtapi_print_msg(RTAPI_MSG_ERR, "%s: ERROR: mmap() failed\n", comp_name);
       return -1;
    }

    // adjust offset to correct value
    vrt_block_addr[0] += (vrt_offset/4);

    // add correct address values to global GPIO array
    for ( n = GPIO_A; n <= GPIO_G; ++n )
    {
        _GPIO[n] =
            (struct _GPIO_PORT_REG_t *)
            (vrt_block_addr[0] + n*(0x24/4));
    }

    // calculate phy memory block start
    vrt_offset = GPIO_PHY_MEM_OFFSET2 % PHY_MEM_BLOCK_SIZE;
    phy_block_addr = GPIO_PHY_MEM_OFFSET2 - vrt_offset;

    // make a block of phy memory visible in our user space
    vrt_block_addr[1] = mmap(
       NULL,                    // Any adddress in our space
       PHY_MEM_BLOCK_SIZE,      // Map length
       PROT_READ | PROT_WRITE,  // Enable reading & writting to mapped memory
       MAP_SHARED,              // Shared with other processes
       mem_fd,                  // File to map
       phy_block_addr           // Offset to GPIO peripheral
    );

    // exit program if mmap is failed
    if (vrt_block_addr[1] == MAP_FAILED)
    {
        rtapi_print_msg(RTAPI_MSG_ERR, "%s: ERROR: mmap() failed\n", comp_name);
        return -1;
    }

    // adjust offset to correct value
    vrt_block_addr[1] += (vrt_offset/4);

    // add correct address values to global GPIO array
    _GPIO[GPIO_L] = (struct _GPIO_PORT_REG_t *) vrt_block_addr[1];

    // no need to keep phy memory file open after mmap
    close(mem_fd);


    // allocate some space for the port data arrays (normal & inverted)
    port_data           = hal_malloc(H3_PINS_COUNT * sizeof(hal_bit_t *));
    port_data_inv       = hal_malloc(H3_PINS_COUNT * sizeof(hal_bit_t *));
    port_param_inv      = hal_malloc(H3_PINS_COUNT * sizeof(hal_bit_t));
    port_param_reset    = hal_malloc(H3_PINS_COUNT * sizeof(hal_bit_t));
    port_reset_time     = hal_malloc(sizeof(hal_u32_t));
    if ( port_data == 0         || port_data_inv == 0 ||
         port_param_inv == 0    || port_param_reset == 0 ||
         port_reset_time == 0 )
    {
        rtapi_print_msg(RTAPI_MSG_ERR,
                        "%s: ERROR: hal_malloc() failed\n", comp_name);
        hal_exit(comp_id);
        return -1;
    }


#define INVALID_PIN_MSG_AND_RETURN \
    rtapi_print_msg(RTAPI_MSG_ERR, "%s: ERROR: invalid pin %s\n",\
        comp_name, token);\
    hal_exit(comp_id);\
    return -1;

#define PIN_EXPORT_FAILED_MSG_AND_RETURN \
    rtapi_print_msg(RTAPI_MSG_ERR, "%s: ERROR: pin %s export failed\n",\
        comp_name, token);\
    hal_exit(comp_id);\
    return -1;


    // configure input pins
    if (input_pins != NULL)
    {
        data = input_pins;

        // break input_pins string by comma
        while ((token = strtok(data, ",")) != NULL)
        {
            // get token's size
            size_t len = strlen(token);

            // ignore empty tokens
            if ( len < 1 ) continue;

            // if we have the GPIO pin name
            if ( token[0] == 'P' && len >= 3 && len <= 4 )
            {
                uint8_t pin_found = 0;

                // trying to find a valid port name
                for ( p = GPIO_PORT_COUNT; p--; )
                {
                    // if valid port name found
                    if ( token[1] == _GPIO_port_info[p].name )
                    {
                        // trying to find a correct pin number
                        pin = (uint8_t) simple_strtol(&token[2], NULL, 10);

                        // if a correct pin number wasn't found
                        if ( pin >= _GPIO_port_info[p].pins_count )
                        {
                            INVALID_PIN_MSG_AND_RETURN;
                        }

                        // correct pin number found
                        pin += _GPIO_port_info[p].id_offset;
                        pin_found = 1;

                        // export H3 pin input function
                        retval = hal_pin_bit_newf(HAL_OUT,
                            &port_data[pin], comp_id,
                            "%s.pin-%s-in", comp_name, token);
                        if (retval < 0)
                        {
                            PIN_EXPORT_FAILED_MSG_AND_RETURN;
                        }

                        // export H3 pin inverted input function
                        retval = hal_pin_bit_newf(HAL_OUT,
                            &port_data_inv[pin], comp_id,
                            "%s.pin-%s-in-not", comp_name, token);
                        if (retval < 0)
                        {
                            PIN_EXPORT_FAILED_MSG_AND_RETURN;
                        }

                        break;
                    }
                }

                // if valid port name wasn't found
                if ( !pin_found )
                {
                    INVALID_PIN_MSG_AND_RETURN;
                }
            }
            // if we have the OPI pin number
            else if ( token[0] >= '0' && token[0] <= '9' && len <= 2 )
            {
                // trying to find a correct pin number
                pin = (uint8_t) simple_strtol(token, NULL, 10);

                // if a correct pin number wasn't found
                if ( pin < 1 || pin >= OPI_PINS_COUNT || !_OPI_pins[pin].valid )
                {
                    INVALID_PIN_MSG_AND_RETURN;
                }

                // correct pin number found
                pin = _OPI_pins[pin].H3_pin;

                // export OPI pin input function
                retval = hal_pin_bit_newf(HAL_OUT, &port_data[pin], comp_id,
                    "%s.pin-%02d-in", comp_name, _H3_pins[pin].OPI_pin);
                if (retval < 0)
                {
                    PIN_EXPORT_FAILED_MSG_AND_RETURN;
                }

                // export OPI pin inverted input function
                retval = hal_pin_bit_newf(HAL_OUT, &port_data_inv[pin],
                    comp_id, "%s.pin-%02d-in-not", comp_name,
                    _H3_pins[pin].OPI_pin);
                if (retval < 0)
                {
                    PIN_EXPORT_FAILED_MSG_AND_RETURN;
                }
            }
            // we have unknown pin
            else
            {
                INVALID_PIN_MSG_AND_RETURN;
            }

            // add OPI pin id to the input pins list
            input_pins_list[input_pins_count] = pin;
            ++input_pins_count;

            // configure OrangePi pin as input
            config_pin_as_input(pin);

            data = NULL; // after the first call, subsequent calls to
                         // strtok need to be on NULL
        }
    }


    // configure output pins
    if (output_pins != NULL)
    {
        data = output_pins;

        // break input_pins string by comma
        while ((token = strtok(data, ",")) != NULL)
        {
            // get token's size
            size_t len = strlen(token);

            // ignore empty tokens
            if ( len < 1 ) continue;

            // if we have the GPIO pin name
            if ( token[0] == 'P' && len >= 3 && len <= 4 )
            {
                uint8_t pin_found = 0;

                // trying to find a valid port name
                for ( p = GPIO_PORT_COUNT; p--; )
                {
                    // if valid port name found
                    if ( token[1] == _GPIO_port_info[p].name )
                    {
                        // trying to find a correct pin number
                        pin = (uint8_t) simple_strtol(&token[2], NULL, 10);

                        // if a correct pin number wasn't found
                        if ( pin >= _GPIO_port_info[p].pins_count )
                        {
                            INVALID_PIN_MSG_AND_RETURN;
                        }

                        // correct pin number found
                        pin += _GPIO_port_info[p].id_offset;
                        pin_found = 1;

                        // export H3 pin output function
                        retval = hal_pin_bit_newf(HAL_IN, &port_data[pin],
                            comp_id, "%s.pin-%s-out", comp_name, token);
                        if (retval < 0)
                        {
                            PIN_EXPORT_FAILED_MSG_AND_RETURN;
                        }

                        // inverted H3 pin parameter
                        retval = hal_param_bit_newf(HAL_RW,
                            &port_param_inv[pin], comp_id,
                            "%s.pin-%s-out-invert", comp_name, token);
                        if (retval < 0)
                        {
                            rtapi_print_msg(RTAPI_MSG_ERR,
                                "%s: ERROR: output pin %s "
                                "invert param export failed\n",
                                comp_name, token);
                            hal_exit(comp_id);
                            return -1;
                        }

                        // reset H3 pin parameter
                        retval = hal_param_bit_newf(HAL_RW,
                            &port_param_reset[pin], comp_id,
                            "%s.pin-%s-out-reset", comp_name, token);
                        if (retval < 0)
                        {
                            rtapi_print_msg(RTAPI_MSG_ERR,
                                "%s: ERROR: output pin %s "
                                "reset param export failed\n",
                                comp_name, token);
                            hal_exit(comp_id);
                            return -1;
                        }

                        break;
                    }
                }

                // if valid port name wasn't found
                if ( !pin_found )
                {
                    INVALID_PIN_MSG_AND_RETURN;
                }
            }
            // if we have the OPI pin number
            else if ( token[0] >= '0' && token[0] <= '9' && len <= 2 )
            {
                // trying to find a correct pin number
                pin = (uint8_t) simple_strtol(token, NULL, 10);

                // if a correct pin number wasn't found
                if ( pin < 1 || pin >= OPI_PINS_COUNT ||
                     !_OPI_pins[pin].valid )
                {
                    INVALID_PIN_MSG_AND_RETURN;
                }

                // correct pin number found
                pin = _OPI_pins[pin].H3_pin;

                // export OPI pin output function
                retval = hal_pin_bit_newf(HAL_IN, &port_data[pin],
                    comp_id, "%s.pin-%02d-out", comp_name,
                    _H3_pins[pin].OPI_pin);
                if (retval < 0)
                {
                    PIN_EXPORT_FAILED_MSG_AND_RETURN;
                }

                // inverted OPI pin parameter
                retval = hal_param_bit_newf(HAL_RW, &port_param_inv[pin],
                    comp_id, "%s.pin-%02d-out-invert", comp_name,
                    _H3_pins[pin].OPI_pin);
                if (retval < 0)
                {
                    rtapi_print_msg(RTAPI_MSG_ERR,
                        "%s: ERROR: output pin %s "
                        "invert param export failed\n",
                        comp_name, token);
                    hal_exit(comp_id);
                    return -1;
                }

                // reset OPI pin parameter
                retval = hal_param_bit_newf(HAL_RW,
                    &port_param_reset[pin], comp_id,
                    "%s.pin-%02d-out-reset", comp_name, _H3_pins[pin].OPI_pin);
                if (retval < 0)
                {
                    rtapi_print_msg(RTAPI_MSG_ERR,
                        "%s: ERROR: output pin %s "
                        "reset param export failed\n",
                        comp_name, token);
                    hal_exit(comp_id);
                    return -1;
                }
            }
            // we have unknown pin
            else
            {
                INVALID_PIN_MSG_AND_RETURN;
            }

            // add pin id to the output pins list
            output_pins_list[output_pins_count] = pin;
            ++output_pins_count;

            // configure pin as output
            config_pin_as_output(pin);

            data = NULL; // after the first call, subsequent calls to
                         // strtok need to be on NULL
        }
    }


#undef INVALID_PIN_MSG_AND_RETURN
#undef PIN_EXPORT_FAILED_MSG_AND_RETURN


    // export port reset time parameter
    retval = hal_param_u32_newf(HAL_RW, port_reset_time,
        comp_id, "%s.reset-time", comp_name);
    if (retval < 0)
    {
        rtapi_print_msg(RTAPI_MSG_ERR,
            "%s: ERROR: reset-time param export failed\n", comp_name);
        hal_exit(comp_id);
        return -1;
    }

    // export port WRITE function
    rtapi_snprintf(name, sizeof(name), "%s.write", comp_name);
    retval = hal_export_funct(name, write_port, 0, 0, 0, comp_id);
    if (retval < 0)
    {
        rtapi_print_msg(RTAPI_MSG_ERR,
                        "%s: ERROR: write funct export failed\n", comp_name);
        hal_exit(comp_id);
        return -1;
    }

    // export port READ function
    rtapi_snprintf(name, sizeof(name), "%s.read", comp_name);
    retval = hal_export_funct(name, read_port, 0, 0, 0, comp_id);
    if (retval < 0)
    {
        rtapi_print_msg(RTAPI_MSG_ERR,
                        "%s: ERROR: read funct export failed\n", comp_name);
        hal_exit(comp_id);
        return -1;
    }

    // export port RESET function
    rtapi_snprintf(name, sizeof(name), "%s.reset", comp_name);
    retval = hal_export_funct(name, reset_port, 0, 0, 0, comp_id);
    if (retval < 0)
    {
        rtapi_print_msg(RTAPI_MSG_ERR,
                        "%s: ERROR: reset funct export failed\n", comp_name);
        hal_exit(comp_id);
        return -1;
    }

    // driver is ready to use
    rtapi_print_msg(RTAPI_MSG_INFO, "%s: installed driver\n", comp_name);
    hal_ready(comp_id);

    // no errors
    return 0;
}

void rtapi_app_exit(void)
{
    // unlink phy space from our user space
    munmap(vrt_block_addr[0], PHY_MEM_BLOCK_SIZE);
    munmap(vrt_block_addr[1], PHY_MEM_BLOCK_SIZE);

    hal_exit(comp_id);
}




#define pd_pin      output_pins_list[n]     // port_data pin ID
#define g_pin       _H3_pins[pd_pin].pin    // GPIO pin ID
#define g_prt       _H3_pins[pd_pin].port   // GPIO port ID
#define g_prt_data  _GPIO[g_prt]->data      // GPIO port data value

static void write_port(void *arg, long period)
{
    static int8_t n = 0;

    // set GPIO output pins state from the port_data array
    for ( n = output_pins_count; n--; )
    {
        if ( *(port_data[pd_pin]) ^ port_param_inv[pd_pin] )
        {
            // clear GPIO pin
            g_prt_data &= ~(1 << g_pin);
        }
        else
        {
            // set GPIO pin
            g_prt_data |= (1 << g_pin);
        }
    }

    // save write time for the reset function
    port_write_time = rtapi_get_clocks();
}

static void reset_port(void *arg, long period)
{
    static int8_t n = 0;
    static long long deadline, reset_time_tsc;

    // adjust reset time
    if(*port_reset_time > period/4) *port_reset_time = period/4;
    reset_time_tsc = ns2tsc(*port_reset_time);

    // set deadline time and make a busy waiting
    deadline = port_write_time + reset_time_tsc;
    while(rtapi_get_clocks() < deadline) {}

    // reset pin states
    for ( n = output_pins_count; n--; )
    {
        // do nothing if pin reset param = 0 or pin already reset
        if ( !port_param_reset[pd_pin] || !(*(port_data[pd_pin])) ) continue;

        // reset pin
        if ( port_param_inv[pd_pin] )
        {
            // clear pin state
            *port_data[pd_pin] = 0;
            // clear GPIO pin
            g_prt_data &= ~(1 << g_pin);
        }
        else
        {
            // set pin state
            *port_data[pd_pin] = 1;
            // set GPIO pin
            g_prt_data |= (1 << g_pin);
        }
    }
}

#undef pd_pin
#undef g_pin
#undef g_prt
#undef g_prt_data




#define pd_pin      input_pins_list[n]
#define g_pin       _H3_pins[pd_pin].pin
#define g_prt       _H3_pins[pd_pin].port
#define g_prt_data  _GPIO[g_prt]->data

static void read_port(void *arg, long period)
{
    static int8_t n = 0;

    // put GPIO input pins state into the port_data array
    for ( n = input_pins_count; n--; )
    {
        if ( ((1 << g_pin) & g_prt_data) )
        {
            *port_data[pd_pin]      = 1;
            *port_data_inv[pd_pin]  = 0;
        }
        else
        {
            *port_data[pd_pin]      = 0;
            *port_data_inv[pd_pin]  = 1;
        }
    }
}

#undef pd_pin
#undef g_pin
#undef g_prt
#undef g_prt_data
