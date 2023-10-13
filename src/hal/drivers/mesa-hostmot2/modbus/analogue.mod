/* Modbus definition file for a single-channel analogue input module
   found on eBay                                                      */

/*
The format of the channel descriptors is:

{TYPE, FUNC, ADDR, COUNT, pin_name}

TYPE is one of HAL_BIT, HAL_FLOAT, HAL_S32, HAL_U32, ENCODER
FUNC = 1, 2, 3, 4, 5, 6, 15, 16 - Modbus commands
COUNT = number of coils/registers to read
*/

#define MAX_MSG_LEN 16   // may be increased if necessary to max 251

static const hm2_modbus_chan_descriptor_t channels[] = {
/*  {TYPE,    FUNC, ADDR,   COUNT, pin_name} */
    {HAL_FLOAT, 3,  0x0000, 1,     "volts"},
};

/* Optionally #define DEBUG to aid with troubleshooting.
   Use 3 to see a lot of information about the modbus
   internal workings. 1 is default (errors only)                    */

#define DEBUG 1
