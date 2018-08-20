#include "mb2hal.h"

retCode create_HAL_pins()
{
    char *fnct_name = "create_HAL_pins";
    int tx_counter;

    for (tx_counter = 0; tx_counter < gbl.tot_mb_tx; tx_counter++) {
        if (create_each_mb_tx_hal_pins(&gbl.mb_tx[tx_counter]) != retOK) {
            ERR(gbl.init_dbg, "failed to initialize hal pins in tx_num[%d] [%d] [%s]",
                tx_counter, gbl.mb_tx[tx_counter].mb_tx_fnct, gbl.mb_tx[tx_counter].mb_tx_fnct_name);
            return retERR;
        }
    }

    return retOK;
}

retCode create_each_mb_tx_hal_pins(mb_tx_t *mb_tx)
{
    char *fnct_name = "create_each_mb_tx_hal_pins";
    char hal_pin_name[HAL_NAME_LEN + 1];
    int pin_counter;

    if (mb_tx == NULL) {
        ERR(gbl.init_dbg, "NULL pointer");
        return retERR;
    }

    //num_errors hal pin
    mb_tx->num_errors = hal_malloc(sizeof(hal_u32_t *));
    if (mb_tx->num_errors == NULL) {
        ERR(gbl.init_dbg, "[%d] [%s] NULL hal_malloc num_errors",
            mb_tx->mb_tx_fnct, mb_tx->mb_tx_fnct_name);
        return retERR;
    }
    memset(mb_tx->num_errors, 0, sizeof(hal_u32_t *));
    snprintf(hal_pin_name, HAL_NAME_LEN, "%s.%s.num_errors", gbl.hal_mod_name, mb_tx->hal_tx_name);
    if (0 != hal_pin_u32_newf(HAL_OUT, mb_tx->num_errors, gbl.hal_mod_id, "%s", hal_pin_name)) {
        ERR(gbl.init_dbg, "[%d] [%s] [%s] hal_pin_u32_newf failed", mb_tx->mb_tx_fnct, mb_tx->mb_tx_fnct_name, hal_pin_name);
        return retERR;
    }
    **(mb_tx->num_errors) = 0;
    DBG(gbl.init_dbg, "mb_tx_num [%d] pin_name [%s]", mb_tx->mb_tx_num, hal_pin_name);

    switch (mb_tx->mb_tx_fnct) {

    case mbtx_02_READ_DISCRETE_INPUTS:
    case mbtx_15_WRITE_MULTIPLE_COILS:
        mb_tx->bit = hal_malloc(sizeof(hal_bit_t *) * mb_tx->mb_tx_nelem);
        if (mb_tx->bit == NULL) {
            ERR(gbl.init_dbg, "[%d] [%s] NULL hal_malloc [%d] elements",
                mb_tx->mb_tx_fnct, mb_tx->mb_tx_fnct_name, mb_tx->mb_tx_nelem);
            return retERR;
        }
        memset(mb_tx->bit, 0, sizeof(hal_bit_t *) * mb_tx->mb_tx_nelem);
        break;

    case mbtx_03_READ_HOLDING_REGISTERS:
    case mbtx_04_READ_INPUT_REGISTERS:
    case mbtx_06_WRITE_SINGLE_REGISTER:
    case mbtx_16_WRITE_MULTIPLE_REGISTERS:
        mb_tx->float_value= hal_malloc(sizeof(hal_float_t *) * mb_tx->mb_tx_nelem);
        mb_tx->int_value  = hal_malloc(sizeof(hal_s32_t *) * mb_tx->mb_tx_nelem);
        //mb_tx->scale      = hal_malloc(sizeof(hal_float_t) * mb_tx->mb_tx_nelem);
        //mb_tx->offset     = hal_malloc(sizeof(hal_float_t) * mb_tx->mb_tx_nelem);
        //if (mb_tx->float_value == NULL || mb_tx->int_value == NULL
        //        || mb_tx->scale == NULL || mb_tx->offset == NULL) {
        if (mb_tx->float_value == NULL || mb_tx->int_value == NULL) {
            ERR(gbl.init_dbg, "[%d] [%s] NULL hal_malloc [%d] elements",
                mb_tx->mb_tx_fnct, mb_tx->mb_tx_fnct_name, mb_tx->mb_tx_nelem);
            return retERR;
        }
        memset(mb_tx->float_value,     0, sizeof(hal_float_t *) * mb_tx->mb_tx_nelem);
        memset(mb_tx->int_value,       0, sizeof(hal_s32_t *)   * mb_tx->mb_tx_nelem);
        //memset((void *) mb_tx->scale,  0, sizeof(hal_float_t)   * mb_tx->mb_tx_nelem);
        //memset((void *) mb_tx->offset, 0, sizeof(hal_float_t)   * mb_tx->mb_tx_nelem);
        break;

    default:
        ERR(gbl.init_dbg, "[%d] wrong mb_tx_fnct", mb_tx->mb_tx_fnct);
        return retERR;
        break;
    }

    for (pin_counter = 0; pin_counter < mb_tx->mb_tx_nelem; pin_counter++) {
        if(mb_tx->mb_tx_names){
            snprintf(hal_pin_name, HAL_NAME_LEN, "%s.%s.%s", gbl.hal_mod_name, mb_tx->hal_tx_name, mb_tx->mb_tx_names[pin_counter]);
        }else{
            snprintf(hal_pin_name, HAL_NAME_LEN, "%s.%s.%02d", gbl.hal_mod_name, mb_tx->hal_tx_name, pin_counter);
        }
        DBG(gbl.init_dbg, "mb_tx_num [%d] pin_name [%s]", mb_tx->mb_tx_num, hal_pin_name);

        switch (mb_tx->mb_tx_fnct) {
        case mbtx_15_WRITE_MULTIPLE_COILS:
            if (0 != hal_pin_bit_newf(HAL_IN, mb_tx->bit + pin_counter, gbl.hal_mod_id,
                                      "%s", hal_pin_name)) {
                ERR(gbl.init_dbg, "[%d] [%s] [%s] hal_pin_bit_newf failed",
                    mb_tx->mb_tx_fnct, mb_tx->mb_tx_fnct_name, hal_pin_name);
                return retERR;
            }
            *mb_tx->bit[pin_counter] = 0;
            break;
        case mbtx_02_READ_DISCRETE_INPUTS:
            if (0 != hal_pin_bit_newf(HAL_OUT, mb_tx->bit + pin_counter, gbl.hal_mod_id,
                                      "%s", hal_pin_name)) {
                ERR(gbl.init_dbg, "[%d] [%s] [%s] hal_pin_bit_newf failed",
                    mb_tx->mb_tx_fnct, mb_tx->mb_tx_fnct_name, hal_pin_name);
                return retERR;
            }
            *mb_tx->bit[pin_counter] = 0;
            break;
        case mbtx_04_READ_INPUT_REGISTERS:
        case mbtx_03_READ_HOLDING_REGISTERS:
            if (0 != hal_pin_float_newf(HAL_OUT, mb_tx->float_value + pin_counter, gbl.hal_mod_id,
                                        "%s.float", hal_pin_name)) {
                ERR(gbl.init_dbg, "[%d] [%s] [%s] hal_pin_float_newf failed",
                    mb_tx->mb_tx_fnct, mb_tx->mb_tx_fnct_name, hal_pin_name);
                return retERR;
            }
            if (0 != hal_pin_s32_newf(HAL_OUT, mb_tx->int_value + pin_counter, gbl.hal_mod_id,
                                      "%s.int", hal_pin_name)) {
                ERR(gbl.init_dbg, "[%d] [%s] [%s] hal_pin_s32_newf failed",
                    mb_tx->mb_tx_fnct, mb_tx->mb_tx_fnct_name, hal_pin_name);
                return retERR;
            }
            //if (0 != hal_param_float_newf(HAL_RW, mb_tx->scale + pin_counter, gbl.hal_mod_id,
            //                              "%s.scale", hal_pin_name)) {
            //    ERR(gbl.init_dbg, "[%d] [%s] [%s]", mb_tx->mb_tx_fnct, mb_tx->mb_tx_fnct_name, hal_pin_name);
            //    return retERR;
            //}
            //if (0 != hal_param_float_newf(HAL_RW, mb_tx->offset + pin_counter, gbl.hal_mod_id,
            //                              "%s.offset", hal_pin_name)) {
            //    ERR(gbl.init_dbg, "[%d] [%s] [%s]", mb_tx->mb_tx_fnct, mb_tx->mb_tx_fnct_name, hal_pin_name);
            //    return retERR;
            //}
            *mb_tx->float_value[pin_counter] = 0;
            *mb_tx->int_value[pin_counter] = 0;
            //mb_tx->scale[pin_counter] = 1;
            //mb_tx->offset[pin_counter] = 0;
            break;
        case mbtx_06_WRITE_SINGLE_REGISTER:
        case mbtx_16_WRITE_MULTIPLE_REGISTERS:
            if (0 != hal_pin_float_newf(HAL_IN, mb_tx->float_value + pin_counter, gbl.hal_mod_id,
                                        "%s", hal_pin_name)) {
                ERR(gbl.init_dbg, "[%d] [%s] [%s] hal_pin_float_newf failed",
                    mb_tx->mb_tx_fnct, mb_tx->mb_tx_fnct_name, hal_pin_name);
                return retERR;
            }
            //if (0 != hal_param_float_newf(HAL_RW, mb_tx->scale + pin_counter, gbl.hal_mod_id,
            //                              "%s.scale", hal_pin_name)) {
            //    ERR(gbl.init_dbg, "[%d] [%s] [%s]", mb_tx->mb_tx_fnct, mb_tx->mb_tx_fnct_name, hal_pin_name);
            //    return retERR;
            //}
            //if (0 != hal_param_float_newf(HAL_RW, mb_tx->offset + pin_counter, gbl.hal_mod_id,
            //                              "%s.offset", hal_pin_name)) {
            //    ERR(gbl.init_dbg, "[%d] [%s]", mb_tx->mb_tx_fnct, mb_tx->mb_tx_fnct_name);
            //    return retERR;
            //}
            *mb_tx->float_value[pin_counter] = 0;
            //*mb_tx->int_value[pin_counter] = 0;
            //mb_tx->scale[pin_counter] = 1;
            //mb_tx->offset[pin_counter] = 0;
            break;
        default:
            ERR(gbl.init_dbg, "[%d]", mb_tx->mb_tx_fnct);
            return retERR;
            break;
        }
    }

    return retOK;
}
