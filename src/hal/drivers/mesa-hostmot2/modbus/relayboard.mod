/* Modbus definition file for a 4-channel relay board found on eBay */

/*
The format of the channel descriptors is:

{TYPE, FUNC, ADDR, COUNT, pin_name}

TYPE is one of HAL_BIT, HAL_FLOAT, HAL_S32, HAL_U32
FUNC = 1, 2, 3, 4, 5, 6, 15, 16 - Modbus commands
COUNT = number of coils/registers to read
*/

#define MAX_MSG_LEN 16   // may be increased if necessary to max 251

static const hm2_modbus_chan_descriptor_t channels[] = {
/*  {TYPE,    FUNC, ADDR,   COUNT, pin_name} */
    {HAL_BIT, 1,  0x0000, 8,     "state"},
    {HAL_BIT, 2,  0x0000, 8,     "input"},
    {HAL_BIT, 5,  0x0000, 1,     "relay-0"},
    {HAL_BIT, 5,  0x0001, 1,     "relay-1"},
    {HAL_BIT, 5,  0x0002, 1,     "relay-2"},
    {HAL_BIT, 5,  0x0003, 1,     "relay-3"},
};


/* Optionally #define DEBUG to aid with troubleshooting.
   Use 3 to see a lot of information about the modbus
   internal workings. 1 is default (errors only)                    */

