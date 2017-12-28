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




#if !defined(TARGET_PLATFORM_H3)
#error "This driver is for the H3 SoC platform only"
#endif




MODULE_AUTHOR("Mikhail Vydrenko");
MODULE_DESCRIPTION("Driver for the Orange Pi (H3 SoC) GPIO pins");
MODULE_LICENSE("GPL");




#define PHY_MEM_BLOCK_SIZE      4096
#define GPIO_PHY_MEM_OFFSET1    0x01C20800 // GPIO_A .. GPIO_G
#define GPIO_PHY_MEM_OFFSET2    0x01F02C00 // GPIO_L
#define GPIO_PIN_COUNT          43
#define USE_GPIO_PORT_L         0 // 0 = don't use port L

enum
{
    GPIO_A, GPIO_B, GPIO_C, GPIO_D,
    GPIO_E, GPIO_F, GPIO_G, GPIO_L
};

#if USE_GPIO_PORT_L
    #define GPIO_PORT_COUNT 8
    uint32_t * vrt_block_addr[2];
#else
    #define GPIO_PORT_COUNT 7
    uint32_t * vrt_block_addr[1];
#endif

struct _GPIO_PORT_REG_t
{
    uint32_t config[4];
    uint32_t data;
    uint32_t drive[2];
    uint32_t pull[2];
};

struct _GPIO_LIST_t
{
    int8_t port;
    int8_t pin;
};




static const uint8_t * comp_name = "hal_gpio_h3";

static struct _GPIO_PORT_REG_t * _GPIO_port_reg[GPIO_PORT_COUNT] = {0};

static const struct _GPIO_LIST_t _GPIO_LIST[GPIO_PIN_COUNT] =
{
    // dummy
    {-4, 0},

    // general pins 1-40                1       2
    {-2, 0},        {-3, 0},        //  +3.3V   +5V
    {GPIO_A, 12},   {-3, 0},        //  PA12    +5V
    {GPIO_A, 11},   {-1, 0},        //  PA11    GND
    {GPIO_A,  6},   {GPIO_A, 13},   //  PA6     PA13
    {-1, 0},        {GPIO_A, 14},   //  GND     PA14
    {GPIO_A,  1},   {GPIO_D, 14},   //  PA1     PD14
    {GPIO_A,  0},   {-1, 0},        //  PA0     GND
    {GPIO_A,  3},   {GPIO_C,  4},   //  PA3     PC4
    {-2, 0},        {GPIO_C,  7},   //  +3.3V   PC7
    {GPIO_C,  0},   {-1, 0},        //  PC0     GND
    {GPIO_C,  1},   {GPIO_A,  2},   //  PC1     PA2
    {GPIO_C,  2},   {GPIO_C,  3},   //  PC2     PC3
    {-1, 0},        {GPIO_A, 21},   //  GND     PA21
    {GPIO_A, 19},   {GPIO_A, 18},   //  PA19    PA18
    {GPIO_A,  7},   {-1, 0},        //  PA7     GND
    {GPIO_A,  8},   {GPIO_G,  8},   //  PA8     PG8
    {GPIO_A,  9},   {-1, 0},        //  PA9     GND
    {GPIO_A, 10},   {GPIO_G,  9},   //  PA10    PG9
    {GPIO_A, 20},   {GPIO_G,  6},   //  PA20    PG6
    {-1, 0},        {GPIO_G,  7},   //  GND     PG7

    // pins 41,42 are serial console TX,RX pins
    {GPIO_A,  4},   {GPIO_A,  5}    //  PA4     PA5
};

static const uint8_t _available_pins[GPIO_PIN_COUNT] =
{
    // dummy
    0,

    // general pins 1-40
    0, 0,
    1, 0,
    1, 0,
    1, 1,
    0, 1,
    1, 1,
    1, 0,
    1, 1,
    0, 1,
    1, 0,
    1, 1,
    1, 1,
    0, 1,
    1, 1,
    1, 0,
    1, 1,
    1, 0,
    1, 1,
    1, 1,
    0, 1,

    // pins 41,42 are serial console TX,RX pins
    1, 1
};

static int32_t comp_id; // component ID

hal_bit_t **port_data; // port data pins states
hal_bit_t **port_data_inv; // port data inverted pins states
hal_bit_t *port_param_inv; // port params for the pins invert states
hal_bit_t *port_param_reset; // port params for the pins reset states
hal_u32_t *port_reset_time;

long long port_write_time = 0;

static uint8_t input_pins_list[GPIO_PIN_COUNT] = {0};
static uint8_t input_pins_count = 0;
static int8_t *input_pins;
RTAPI_MP_STRING(input_pins, "input pins, comma separated");

static uint8_t output_pins_list[GPIO_PIN_COUNT] = {0};
static uint8_t output_pins_count = 0;
static int8_t *output_pins;
RTAPI_MP_STRING(output_pins, "output pins, comma separated");

static unsigned long ns2tsc_factor;
#define ns2tsc(x) (((x) * (unsigned long long)ns2tsc_factor) >> 12)




static void write_port(void *arg, long period);
static void reset_port(void *arg, long period);
static void read_port(void *arg, long period);




static void config_pin_as_input(uint8_t n)
{
    _GPIO_port_reg[_GPIO_LIST[n].port]->config[_GPIO_LIST[n].pin / 8] &=
        ~(0b1111 << (_GPIO_LIST[n].pin % 8 * 4));
}

static void config_pin_as_output(uint8_t n)
{
    _GPIO_port_reg[_GPIO_LIST[n].port]->config[_GPIO_LIST[n].pin / 8] &=
        ~(0b1111 << (_GPIO_LIST[n].pin % 8 * 4));
    _GPIO_port_reg[_GPIO_LIST[n].port]->config[_GPIO_LIST[n].pin / 8] |=
         (0b0001 << (_GPIO_LIST[n].pin % 8 * 4));
}




int32_t rtapi_app_main(void)
{
    int32_t     mem_fd;
    uint32_t    vrt_offset = 0;
    off_t       phy_block_addr = 0;
    int32_t     n, retval;
    int8_t      *data, *token;
    uint8_t     pin;
    int8_t      name[HAL_NAME_LEN + 1];


#if LINUX_VERSION_CODE > KERNEL_VERSION(2,6,0) &&0//FIXME
    // this calculation fits in a 32-bit unsigned
    // as long as CPUs are under about 6GHz
    ns2tsc_factor = (cpu_khz << 6) / 15625ul;
#else
    ns2tsc_factor = 1ll<<12;
#endif


    comp_id = hal_init("hal_gpio_h3");
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
        _GPIO_port_reg[n] =
            (struct _GPIO_PORT_REG_t *)
            (vrt_block_addr[0] + n*(0x24/4));
    }

#if USE_GPIO_PORT_L
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
    _GPIO_port_reg[GPIO_L] = (struct _GPIO_PORT_REG_t *) vrt_block_addr[1];
#endif

    // no need to keep phy memory file open after mmap
    close(mem_fd);


    // allocate some space for the port data arrays (normal & inverted)
    port_data           = hal_malloc(GPIO_PIN_COUNT * sizeof(hal_bit_t *));
    port_data_inv       = hal_malloc(GPIO_PIN_COUNT * sizeof(hal_bit_t *));
    port_param_inv      = hal_malloc(GPIO_PIN_COUNT * sizeof(hal_bit_t));
    port_param_reset    = hal_malloc(GPIO_PIN_COUNT * sizeof(hal_bit_t));
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

    // configure input pins
    if (input_pins != NULL)
    {
        data = input_pins;

        while ((token = strtok(data, ",")) != NULL)
        {
            pin = (uint8_t) strtol(token, NULL, 10);

            if ( pin < 0 || pin >= GPIO_PIN_COUNT || !_available_pins[pin] )
            {
                rtapi_print_msg(RTAPI_MSG_ERR,
                    "%s: ERROR: invalid pin number %d\n", comp_name, pin);
                hal_exit(comp_id);
                return -1;
            }

            input_pins_list[input_pins_count] = pin;
            ++input_pins_count;

            // configure OrangePi pin as input
            config_pin_as_input(pin);

            // normal pin function
            retval = hal_pin_bit_newf(HAL_OUT, &port_data[pin],
                comp_id, "%s.pin-%02d-in", comp_name, pin);
            if (retval < 0)
            {
                rtapi_print_msg(RTAPI_MSG_ERR,
                    "%s: ERROR: input pin %d export failed\n", comp_name, pin);
                hal_exit(comp_id);
                return -1;
            }

            // inverted pin function
            retval = hal_pin_bit_newf(HAL_OUT, &port_data_inv[pin],
                comp_id, "%s.pin-%02d-in-not", comp_name, pin);
            if (retval < 0)
            {
                rtapi_print_msg(RTAPI_MSG_ERR,
                    "%s: ERROR: inverted input pin %d export failed\n",
                    comp_name, pin);
                hal_exit(comp_id);
                return -1;
            }

            data = NULL; // after the first call, subsequent calls to
                         // strtok need to be on NULL
        }
    }

    // configure output pins
    if (output_pins != NULL)
    {
        data = output_pins;

        while ((token = strtok(data, ",")) != NULL)
        {
            pin = (uint8_t) strtol(token, NULL, 10);

            if ( pin < 0 || pin >= GPIO_PIN_COUNT || !_available_pins[pin] )
            {
                rtapi_print_msg(RTAPI_MSG_ERR,
                    "%s: ERROR: invalid pin number %d\n", comp_name, pin);
                hal_exit(comp_id);
                return -1;
            }

            // check - if pin already exported as input
            for ( n = input_pins_count; n--; )
            {
                if ( input_pins_list[n] == pin )
                {
                    rtapi_print_msg(RTAPI_MSG_ERR,
                        "%s: ERROR: output pin %d exported before as input\n",
                        comp_name, pin);
                    break;
                }
            }

            // this pin is free and can be exported as output
            if ( n < 0 )
            {
                output_pins_list[output_pins_count] = pin;
                ++output_pins_count;

                // configure OrangePi pin as output
                config_pin_as_output(pin);

                // normal pin function
                retval = hal_pin_bit_newf(HAL_IN, &port_data[pin],
                    comp_id, "%s.pin-%02d-out", comp_name, pin);
                if (retval < 0)
                {
                    rtapi_print_msg(RTAPI_MSG_ERR,
                        "%s: ERROR: output pin %d export failed\n",
                        comp_name, pin);
                    hal_exit(comp_id);
                    return -1;
                }

                // inverted pin parameter
                retval = hal_param_bit_newf(HAL_RW, &port_param_inv[pin],
                    comp_id, "%s.pin-%02d-out-invert", comp_name, pin);
                if (retval < 0)
                {
                    rtapi_print_msg(RTAPI_MSG_ERR,
                        "%s: ERROR: output pin %d invert param export failed\n",
                        comp_name, pin);
                    hal_exit(comp_id);
                    return -1;
                }

                // reset pin parameter
                retval = hal_param_bit_newf(HAL_RW, &port_param_reset[pin],
                    comp_id, "%s.pin-%02d-out-reset", comp_name, pin);
                if (retval < 0)
                {
                    rtapi_print_msg(RTAPI_MSG_ERR,
                        "%s: ERROR: output pin %d reset param export failed\n",
                        comp_name, pin);
                    hal_exit(comp_id);
                    return -1;
                }
            }

            data = NULL; // after the first call, subsequent calls to
                         // strtok need to be on NULL
        }
    }

    // export port reset time parameter
    retval = hal_param_u32_newf(HAL_RW, port_reset_time,
        comp_id, "%s.reset-time", comp_name);
    if (retval < 0)
    {
        rtapi_print_msg(RTAPI_MSG_ERR,
            "%s: ERROR: reset-time param export failed\n", comp_name, pin);
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
#if USE_GPIO_PORT_L
    munmap(vrt_block_addr[1], PHY_MEM_BLOCK_SIZE);
#endif

    hal_exit(comp_id);
}




#define pd_pin      output_pins_list[n]             // port_data pin ID
#define g_pin       _GPIO_LIST[pd_pin].pin          // GPIO pin ID
#define g_prt       _GPIO_LIST[pd_pin].port         // GPIO port ID
#define g_prt_data  _GPIO_port_reg[g_prt]->data     // GPIO port data value

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
#define g_pin       _GPIO_LIST[pd_pin].pin
#define g_prt       _GPIO_LIST[pd_pin].port
#define g_prt_data  _GPIO_port_reg[g_prt]->data

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
