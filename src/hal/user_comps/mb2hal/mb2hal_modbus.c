#include <sys/time.h>
#include "mb2hal.h"

retCode fnct_01_read_coils(mb_tx_t *this_mb_tx, mb_link_t *this_mb_link)
{
    char *fnct_name = "fnct_01_read_coils";
    int counter, ret;
    uint8_t bits[MB2HAL_MAX_FNCT01_ELEMENTS];

    if (this_mb_tx == NULL || this_mb_link == NULL) {
        return retERR;
    }
    if (this_mb_tx->mb_tx_nelem > MB2HAL_MAX_FNCT01_ELEMENTS) {
        return retERR;
    }

    DBG(this_mb_tx->cfg_debug, "mb_tx[%d] mb_links[%d] slave[%d] fd[%d] 1st_addr[%d] nelem[%d]",
        this_mb_tx->mb_tx_num, this_mb_tx->mb_link_num, this_mb_tx->mb_tx_slave_id, modbus_get_socket(this_mb_link->modbus),
        this_mb_tx->mb_tx_1st_addr, this_mb_tx->mb_tx_nelem);

    ret = modbus_read_bits(this_mb_link->modbus, this_mb_tx->mb_tx_1st_addr, this_mb_tx->mb_tx_nelem, bits);
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
        *(this_mb_tx->bit_inv[counter]) = !bits[counter];
    }

    return retOK;
}

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
        if (gbl.version > 1000)
            *(this_mb_tx->bit_inv[counter]) = !bits[counter];
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
        *(this_mb_tx->int_value[counter]) = data[counter];
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
        *(this_mb_tx->int_value[counter]) = data[counter];
    }

    return retOK;
}

retCode fnct_05_write_single_coil(mb_tx_t *this_mb_tx, mb_link_t *this_mb_link)
{
    char *fnct_name = "fnct_05_write_single_coil";
    int ret, bit;

    if (this_mb_tx == NULL || this_mb_link == NULL) {
        return retERR;
    }
    if (this_mb_tx->mb_tx_nelem > MB2HAL_MAX_FNCT05_ELEMENTS) {
        return retERR;
    }

    bit = *(this_mb_tx->bit[0]);

    DBG(this_mb_tx->cfg_debug, "mb_tx[%d] mb_links[%d] slave[%d] fd[%d] 1st_addr[%d] nelem[%d]",
        this_mb_tx->mb_tx_num, this_mb_tx->mb_link_num, this_mb_tx->mb_tx_slave_id,
        modbus_get_socket(this_mb_link->modbus), this_mb_tx->mb_tx_1st_addr, this_mb_tx->mb_tx_nelem);

    ret = modbus_write_bit(this_mb_link->modbus, this_mb_tx->mb_tx_1st_addr, bit);
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

    data = *(this_mb_tx->float_value[0]);
    if (gbl.version > 1000)
        data += *(this_mb_tx->int_value[0]);
    if(data > UINT16_MAX) { // prevent wrap on overflow
        data = UINT16_MAX;
    }
    
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
        int data32 = (uint16_t) *(this_mb_tx->float_value[counter]);
        if (gbl.version > 1000)
            data32 += *(this_mb_tx->int_value[counter]);
        if(data32 > UINT16_MAX) { // prevent wrap on overflow
            data[counter] = UINT16_MAX;
        }
        else {
            data[counter] = (uint16_t) data32;
        }
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
