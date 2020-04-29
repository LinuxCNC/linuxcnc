#include <sys/time.h>
#include "mb2hal.h"

retCode fnct_02_read_discrete_inputs(mb_tx_t *this_mb_tx, mb_link_t *this_mb_link)
{
    char *fnct_name = "fnct_02_read_discrete_inputs";
    int counter, ret;
    uint8_t bits[MB2HAL_MAX_FNCT02_ELEMENTS];

    if (this_mb_tx == NULL || this_mb_link == NULL) {
        return retERR;
    }
    if (this_mb_tx->mb_tx_nelem > MB2HAL_MAX_FNCT02_ELEMENTS) {
        return retERR;
    }

    DBG(this_mb_tx->cfg_debug, "mb_tx[%d] mb_links[%d] slave[%d] fd[%d] 1st_addr[%d] nelem[%d]",
        this_mb_tx->mb_tx_num, this_mb_tx->mb_link_num, this_mb_tx->mb_tx_slave_id, modbus_get_socket(this_mb_link->modbus),
        this_mb_tx->mb_tx_1st_addr, this_mb_tx->mb_tx_nelem);

    ret = modbus_read_input_bits(this_mb_link->modbus, this_mb_tx->mb_tx_1st_addr, this_mb_tx->mb_tx_nelem, bits);
    if (ret < 0) {
        if (modbus_get_socket(this_mb_link->modbus) < 0) {
            modbus_close(this_mb_link->modbus);
        }
        ERR(this_mb_tx->cfg_debug, "mb_tx[%d] mb_links[%d] slave[%d] = ret[%d] fd[%d]",
            this_mb_tx->mb_tx_num, this_mb_tx->mb_link_num, this_mb_tx->mb_tx_slave_id, ret,
            modbus_get_socket(this_mb_link->modbus));
        return retERR;
    }

    for (counter = 0; counter < this_mb_tx->mb_tx_nelem; counter++) {
        *(this_mb_tx->bit[counter]) = bits[counter];
    }

    return retOK;
}

retCode fnct_03_read_holding_registers(mb_tx_t *this_mb_tx, mb_link_t *this_mb_link)
{
    char *fnct_name = "fnct_03_read_holding_registers";
    int counter, ret;
    uint16_t data[MB2HAL_MAX_FNCT03_ELEMENTS];

    if (this_mb_tx == NULL || this_mb_link == NULL) {
        return retERR;
    }
    if (this_mb_tx->mb_tx_nelem > MB2HAL_MAX_FNCT03_ELEMENTS) {
        return retERR;
    }

    DBG(this_mb_tx->cfg_debug, "mb_tx[%d] mb_links[%d] slave[%d] fd[%d] 1st_addr[%d] nelem[%d]",
        this_mb_tx->mb_tx_num, this_mb_tx->mb_link_num, this_mb_tx->mb_tx_slave_id,
        modbus_get_socket(this_mb_link->modbus), this_mb_tx->mb_tx_1st_addr, this_mb_tx->mb_tx_nelem);

    ret = modbus_read_registers(this_mb_link->modbus, this_mb_tx->mb_tx_1st_addr, this_mb_tx->mb_tx_nelem, data);
    if (ret < 0) {
        if (modbus_get_socket(this_mb_link->modbus) < 0) {
            modbus_close(this_mb_link->modbus);
        }
        ERR(this_mb_tx->cfg_debug, "mb_tx[%d] mb_links[%d] slave[%d] = ret[%d] fd[%d]",
            this_mb_tx->mb_tx_num, this_mb_tx->mb_link_num, this_mb_tx->mb_tx_slave_id, ret,
            modbus_get_socket(this_mb_link->modbus));
        return retERR;
    }

    for (counter = 0; counter < this_mb_tx->mb_tx_nelem; counter++) {
        float val = data[counter];
        //val *= this_mb_tx->scale[counter];
        //val += this_mb_tx->offset[counter];
        *(this_mb_tx->float_value[counter]) = val;
        *(this_mb_tx->int_value[counter]) = (hal_s32_t) val;
    }

    return retOK;
}

retCode fnct_04_read_input_registers(mb_tx_t *this_mb_tx, mb_link_t *this_mb_link)
{
    char *fnct_name = "fnct_04_read_input_registers";
    int counter, ret;
    uint16_t data[MB2HAL_MAX_FNCT04_ELEMENTS];

    if (this_mb_tx == NULL || this_mb_link == NULL) {
        return retERR;
    }
    if (this_mb_tx->mb_tx_nelem > MB2HAL_MAX_FNCT04_ELEMENTS) {
        return retERR;
    }

    DBG(this_mb_tx->cfg_debug, "mb_tx[%d] mb_links[%d] slave[%d] fd[%d] 1st_addr[%d] nelem[%d]",
        this_mb_tx->mb_tx_num, this_mb_tx->mb_link_num, this_mb_tx->mb_tx_slave_id,
        modbus_get_socket(this_mb_link->modbus), this_mb_tx->mb_tx_1st_addr, this_mb_tx->mb_tx_nelem);

    ret = modbus_read_input_registers(this_mb_link->modbus, this_mb_tx->mb_tx_1st_addr, this_mb_tx->mb_tx_nelem, data);
    if (ret < 0) {
        if (modbus_get_socket(this_mb_link->modbus) < 0) {
            modbus_close(this_mb_link->modbus);
        }
        ERR(this_mb_tx->cfg_debug, "mb_tx[%d] mb_links[%d] slave[%d] = ret[%d] fd[%d]",
            this_mb_tx->mb_tx_num, this_mb_tx->mb_link_num, this_mb_tx->mb_tx_slave_id, ret,
            modbus_get_socket(this_mb_link->modbus));
        return retERR;
    }

    for (counter = 0; counter < this_mb_tx->mb_tx_nelem; counter++) {
        float val = data[counter];
        //val += this_mb_tx->offset[counter];
        //val *= this_mb_tx->scale[counter];
        *(this_mb_tx->float_value[counter]) = val;
        *(this_mb_tx->int_value[counter]) = (hal_s32_t) val;
    }

    return retOK;
}

retCode fnct_06_write_single_register(mb_tx_t *this_mb_tx, mb_link_t *this_mb_link)
{
    char *fnct_name = "fnct_06_write_single_register";
    int ret, data;

    if (this_mb_tx == NULL || this_mb_link == NULL) {
        return retERR;
    }
    if (this_mb_tx->mb_tx_nelem > MB2HAL_MAX_FNCT06_ELEMENTS) {
        return retERR;
    }

    float val = *(this_mb_tx->float_value[0]);
    data = (int) val;

    DBG(this_mb_tx->cfg_debug, "mb_tx[%d] mb_links[%d] slave[%d] fd[%d] 1st_addr[%d] nelem[%d]",
        this_mb_tx->mb_tx_num, this_mb_tx->mb_link_num, this_mb_tx->mb_tx_slave_id,
        modbus_get_socket(this_mb_link->modbus), this_mb_tx->mb_tx_1st_addr, this_mb_tx->mb_tx_nelem);

    ret = modbus_write_register(this_mb_link->modbus, this_mb_tx->mb_tx_1st_addr, data);
    if (ret < 0) {
        if (modbus_get_socket(this_mb_link->modbus) < 0) {
            modbus_close(this_mb_link->modbus);
        }
        ERR(this_mb_tx->cfg_debug, "mb_tx[%d] mb_links[%d] slave[%d] = ret[%d] fd[%d]",
            this_mb_tx->mb_tx_num, this_mb_tx->mb_link_num, this_mb_tx->mb_tx_slave_id, ret,
            modbus_get_socket(this_mb_link->modbus));
        return retERR;
    }

    return retOK;
}

retCode fnct_15_write_multiple_coils(mb_tx_t *this_mb_tx, mb_link_t *this_mb_link)
{
    char *fnct_name = "fnct_15_write_multiple_coils";
    int counter, ret;
    uint8_t bits[MB2HAL_MAX_FNCT15_ELEMENTS];

    if (this_mb_tx == NULL || this_mb_link == NULL) {
        return retERR;
    }
    if (this_mb_tx->mb_tx_nelem > MB2HAL_MAX_FNCT15_ELEMENTS) {
        return retERR;
    }

    for (counter = 0; counter < this_mb_tx->mb_tx_nelem; counter++) {
        bits[counter] = *(this_mb_tx->bit[counter]);
    }

    DBG(this_mb_tx->cfg_debug, "mb_tx[%d] mb_links[%d] slave[%d] fd[%d] 1st_addr[%d] nelem[%d]",
        this_mb_tx->mb_tx_num, this_mb_tx->mb_link_num, this_mb_tx->mb_tx_slave_id,
        modbus_get_socket(this_mb_link->modbus), this_mb_tx->mb_tx_1st_addr, this_mb_tx->mb_tx_nelem);

    ret = modbus_write_bits(this_mb_link->modbus, this_mb_tx->mb_tx_1st_addr, this_mb_tx->mb_tx_nelem, bits);
    if (ret < 0) {
        if (modbus_get_socket(this_mb_link->modbus) < 0) {
            modbus_close(this_mb_link->modbus);
        }
        ERR(this_mb_tx->cfg_debug, "mb_tx[%d] mb_links[%d] slave[%d] = ret[%d] fd[%d]",
            this_mb_tx->mb_tx_num, this_mb_tx->mb_link_num, this_mb_tx->mb_tx_slave_id, ret,
            modbus_get_socket(this_mb_link->modbus));
        return retERR;
    }

    return retOK;
}

retCode fnct_16_write_multiple_registers(mb_tx_t *this_mb_tx, mb_link_t *this_mb_link)
{
    char *fnct_name = "fnct_16_write_multiple_registers";
    int counter, ret;
    uint16_t data[MB2HAL_MAX_FNCT16_ELEMENTS];

    if (this_mb_tx == NULL || this_mb_link == NULL) {
        return retERR;
    }
    if (this_mb_tx->mb_tx_nelem > MB2HAL_MAX_FNCT16_ELEMENTS) {
        return retERR;
    }

    for (counter = 0; counter < this_mb_tx->mb_tx_nelem; counter++) {
        //float val = *(this_mb_tx->float_value[counter]) / this_mb_tx->scale[counter];
        //val -= this_mb_tx->offset[counter];
        float val = *(this_mb_tx->float_value[counter]);
        data[counter] = (int) val;
    }

    DBG(this_mb_tx->cfg_debug, "mb_tx[%d] mb_links[%d] slave[%d] fd[%d] 1st_addr[%d] nelem[%d]",
        this_mb_tx->mb_tx_num, this_mb_tx->mb_link_num, this_mb_tx->mb_tx_slave_id,
        modbus_get_socket(this_mb_link->modbus), this_mb_tx->mb_tx_1st_addr, this_mb_tx->mb_tx_nelem);

    ret = modbus_write_registers(this_mb_link->modbus, this_mb_tx->mb_tx_1st_addr, this_mb_tx->mb_tx_nelem, data);
    if (ret < 0) {
        if (modbus_get_socket(this_mb_link->modbus) < 0) {
            modbus_close(this_mb_link->modbus);
        }
        ERR(this_mb_tx->cfg_debug, "mb_tx[%d] mb_links[%d] slave[%d] = ret[%d] fd[%d]",
            this_mb_tx->mb_tx_num, this_mb_tx->mb_link_num, this_mb_tx->mb_tx_slave_id, ret,
            modbus_get_socket(this_mb_link->modbus));
        return retERR;
    }

    return retOK;
}
