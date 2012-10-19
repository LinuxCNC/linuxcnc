#include <sys/time.h>
#include "mb2hal.h"

const char *gbl_mb_tx_codes[mbtxMAX] = {
    "",	                               //mbtxERR
    "fnct_02_read_discrete_inputs",    //mbtx_02_READ_DISCRETE_INPUTS,
    "fnct_03_read_holding_registers",  //mbtx_03_READ_HOLDING_REGISTERS,
    "fnct_04_read_input_registers",    //mbtx_04_READ_INPUT_REGISTERS,
    "fnct_15_write_multiple_coils",    //mbtx_15_WRITE_MULTIPLE_COILS,
    "fnct_16_write_multiple_registers" //mbtx_16_WRITE_MULTIPLE_REGISTERS,
};

retCode fnct_02_read_discrete_inputs(mb_tx_t *mb_tx)
{
    char *fnct_name = "fnct_02_read_discrete_inputs";
    int counter;
    int bits[MAX_WRITE_COILS];
    int ret;

    if (mb_tx == NULL) {
        return retERR;
    }

    DBG(mb_tx->cfg_debug, "mb_tx[%d] mb_links[%d] slave[%d] fd[%d] 1st_addr[%d] nelem[%d]",
        mb_tx->mb_tx_num, mb_tx->mb_links_num, mb_tx->mb_slave_id, gbl_mb_links[mb_tx->mb_links_num].fd,
        mb_tx->mb_first_addr, mb_tx->mb_nelements);

    ret = read_input_status(&gbl_mb_links[mb_tx->mb_links_num], mb_tx->mb_slave_id,
                            mb_tx->mb_first_addr, mb_tx->mb_nelements, bits);
    if (ret < 0) {
        if (ret == PORT_SOCKET_FAILURE) {
            modbus_close(&gbl_mb_links[mb_tx->mb_links_num]);
            //transmit failed, force to reconnect the link
            gbl_mb_links[mb_tx->mb_links_num].fd = -1;
        }
        ERR(mb_tx->cfg_debug, "mb_tx[%d] mb_links[%d] slave[%d] = ret[%d] fd[%d]",
            mb_tx->mb_tx_num, mb_tx->mb_links_num, mb_tx->mb_slave_id, ret, gbl_mb_links[mb_tx->mb_links_num].fd);
        return retERR;
    }

    for (counter = 0; counter < mb_tx->mb_nelements; counter++) {
        *(mb_tx->bit[counter]) = bits[counter];
    }

    return retOK;
}

retCode fnct_03_read_holding_registers(mb_tx_t *mb_tx)
{
    char *fnct_name = "fnct_03_read_holding_registers";
    int data[MAX_READ_HOLD_REGS];
    int counter;
    int ret;

    if (mb_tx == NULL) {
        return retERR;
    }

    DBG(mb_tx->cfg_debug, "mb_tx[%d] mb_links[%d] slave[%d] fd[%d] 1st_addr[%d] nelem[%d]",
        mb_tx->mb_tx_num, mb_tx->mb_links_num, mb_tx->mb_slave_id, gbl_mb_links[mb_tx->mb_links_num].fd,
        mb_tx->mb_first_addr, mb_tx->mb_nelements);

    ret = read_holding_registers(&gbl_mb_links[mb_tx->mb_links_num], mb_tx->mb_slave_id,
                                 mb_tx->mb_first_addr, mb_tx->mb_nelements, data);
    if (ret < 0) {
        if (ret == PORT_SOCKET_FAILURE) {
            modbus_close(&gbl_mb_links[mb_tx->mb_links_num]);
            //transmit failed, force to reconnect the link
            gbl_mb_links[mb_tx->mb_links_num].fd = -1;
        }
        ERR(mb_tx->cfg_debug, "mb_tx[%d] mb_links[%d] slave[%d] = ret[%d] fd[%d]",
            mb_tx->mb_tx_num, mb_tx->mb_links_num, mb_tx->mb_slave_id, ret, gbl_mb_links[mb_tx->mb_links_num].fd);
        return retERR;
    }

    for (counter = 0; counter < mb_tx->mb_nelements; counter++) {
        float val = data[counter];
        //val *= mb_tx->scale[counter];
        //val += mb_tx->offset[counter];
        *(mb_tx->float_value[counter]) = val;
        *(mb_tx->int_value[counter]) = (hal_s32_t) val;
    }

    return retOK;
}

retCode fnct_04_read_input_registers(mb_tx_t *mb_tx)
{
    char *fnct_name = "fnct_04_read_input_registers";
    int counter;
    int data[MAX_READ_INPUT_REGS];
    int ret;

    if (mb_tx == NULL) {
        return retERR;
    }

    DBG(mb_tx->cfg_debug, "mb_tx[%d] mb_links[%d] slave[%d] fd[%d] 1st_addr[%d] nelem[%d]",
        mb_tx->mb_tx_num, mb_tx->mb_links_num, mb_tx->mb_slave_id, gbl_mb_links[mb_tx->mb_links_num].fd,
        mb_tx->mb_first_addr, mb_tx->mb_nelements);

    ret = read_input_registers(&gbl_mb_links[mb_tx->mb_links_num], mb_tx->mb_slave_id,
                               mb_tx->mb_first_addr, mb_tx->mb_nelements, data);
    if (ret < 0) {
        if (ret == PORT_SOCKET_FAILURE) {
            modbus_close(&gbl_mb_links[mb_tx->mb_links_num]);
            //transmit failed, force to reconnect the link
            gbl_mb_links[mb_tx->mb_links_num].fd = -1;
        }
        ERR(mb_tx->cfg_debug, "mb_tx[%d] mb_links[%d] slave[%d] = ret[%d] fd[%d]",
            mb_tx->mb_tx_num, mb_tx->mb_links_num, mb_tx->mb_slave_id, ret, gbl_mb_links[mb_tx->mb_links_num].fd);
        return retERR;
    }

    for (counter = 0; counter < mb_tx->mb_nelements; counter++) {
        float val = data[counter];
        //val += mb_tx->offset[counter];
        //val *= mb_tx->scale[counter];
        *(mb_tx->float_value[counter]) = val;
        *(mb_tx->int_value[counter]) = (hal_s32_t) val;
    }

    return retOK;
}

retCode fnct_15_write_multiple_coils(mb_tx_t *mb_tx)
{
    char *fnct_name = "fnct_15_write_multiple_coils";
    int start = mb_tx->mb_nelements - 1;
    int end = 0;
    int counter;
    int ret;
    int bits[MAX_WRITE_COILS];

    if (mb_tx == NULL) {
        return retERR;
    }

    start = 0;
    end = mb_tx->mb_nelements;

    for (counter = start; counter < end; counter++) {
        bits[counter - start] = *(mb_tx->bit[counter]);
    }

    DBG(mb_tx->cfg_debug, "mb_tx[%d] mb_links[%d] slave[%d] fd[%d] 1st_addr[%d] nelem[%d]",
        mb_tx->mb_tx_num, mb_tx->mb_links_num, mb_tx->mb_slave_id, gbl_mb_links[mb_tx->mb_links_num].fd,
        mb_tx->mb_first_addr, mb_tx->mb_nelements);

    // we could use force_single_coil if only one coil has changed but it
    // is probably not worth the effort
    ret = force_multiple_coils(&gbl_mb_links[mb_tx->mb_links_num], mb_tx->mb_slave_id,
                               mb_tx->mb_first_addr + start, end - start, bits);
    if (ret < 0) {
        if (ret == PORT_SOCKET_FAILURE) {
            modbus_close(&gbl_mb_links[mb_tx->mb_links_num]);
            //transmit failed, force to reconnect the link
            gbl_mb_links[mb_tx->mb_links_num].fd = -1;
        }
        ERR(mb_tx->cfg_debug, "mb_tx[%d] mb_links[%d] slave[%d] = ret[%d] fd[%d]",
            mb_tx->mb_tx_num, mb_tx->mb_links_num, mb_tx->mb_slave_id, ret, gbl_mb_links[mb_tx->mb_links_num].fd);
        return retERR;
    }

    return retOK;
}

retCode fnct_16_write_multiple_registers(mb_tx_t *mb_tx)
{
    char *fnct_name = "scan_write_holding";
    int start = mb_tx->mb_nelements - 1;
    int end = 0;
    int counter;
    int data[MAX_WRITE_REGS];
    int ret;

    if (mb_tx == NULL) {
        return retERR;
    }

    start = 0;
    end = mb_tx->mb_nelements;

    for (counter = start; counter < end; counter++) {
        //float val = *(mb_tx->float_value[counter]) / mb_tx->scale[counter];
        //val -= mb_tx->offset[counter];
        float val = *(mb_tx->float_value[counter]);
        data[counter] = (int) val;
    }

    DBG(mb_tx->cfg_debug, "mb_tx[%d] mb_links[%d] slave[%d] fd[%d] 1st_addr[%d] nelem[%d]",
        mb_tx->mb_tx_num, mb_tx->mb_links_num, mb_tx->mb_slave_id, gbl_mb_links[mb_tx->mb_links_num].fd,
        mb_tx->mb_first_addr, mb_tx->mb_nelements);

    ret = preset_multiple_registers(&gbl_mb_links[mb_tx->mb_links_num], mb_tx->mb_slave_id,
                                    mb_tx->mb_first_addr + start, end - start, data + start);
    if (ret < 0) {
        if (ret == PORT_SOCKET_FAILURE) {
            modbus_close(&gbl_mb_links[mb_tx->mb_links_num]);
            //transmit failed, force to reconnect the link
            gbl_mb_links[mb_tx->mb_links_num].fd = -1;
        }
        ERR(mb_tx->cfg_debug, "mb_tx[%d] mb_links[%d] slave[%d] = ret[%d] fd[%d]",
            mb_tx->mb_tx_num, mb_tx->mb_links_num, mb_tx->mb_slave_id, ret, gbl_mb_links[mb_tx->mb_links_num].fd);
        return retERR;
    }

    return retOK;
}
