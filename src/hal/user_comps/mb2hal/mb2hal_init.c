#include "mb2hal.h"

retCode parse_main_args(char **ini_file_path, int argc, char **argv)
{
    char *fnct_name = "parse_main_args";
    char *arg_filename;
    char *tmp;

    if (argc != 2) {
        ERR(init_debug, "wrong number of args");
        return retERR;
    }

    if (strncmp(argv[1], "config=", 7) != 0) { // has config file
        ERR(init_debug, "config parameter not found");
        return retERR;
    }

    arg_filename = argv[1];
    arg_filename += 7;

    if (*arg_filename == '\"') { // file name is quoted
        arg_filename++;
        tmp = arg_filename;
        while (*tmp != '\"' && *tmp != 0) {
            tmp++;
        }
        *tmp = 0; // remove trailing quote
    }
    *ini_file_path = strdup(arg_filename);

    return retOK;
}

retCode parse_ini_file(FILE *ini_file_ptr)
{
    char *fnct_name = "parse_ini_file";
    int counter;

    if (ini_file_ptr == NULL) {
        ERR(init_debug, "NULL pointer");
        return retERR;
    }

    if (parse_common_section(ini_file_ptr) != retOK) {
        ERR(init_debug, "parse_common_section failed");
        return retERR;
    }

    gbl_mb_tx = malloc(sizeof(mb_tx_t) * gbl_n_mb_tx);
    memset(gbl_mb_tx, 0, sizeof(mb_tx_t) * gbl_n_mb_tx);

    for (counter = 0; counter < gbl_n_mb_tx; counter++) {
        if (parse_transaction_section(ini_file_ptr, counter) != retOK) {
            ERR(init_debug, "parse_transaction_section %d failed", counter);
            return retERR;
        }
        OK(init_debug, "parse_transaction_section %d OK", counter);
    }

    return retOK;
}

retCode parse_common_section(FILE *ini_file_ptr)
{
    char *fnct_name = "parse_common_section";
    char *section, *tag;

    if (ini_file_ptr == NULL) {
        ERR(init_debug, "NULL pointer");
        return retERR;
    }

    section = "MB2HAL_INIT";
    tag     = "INIT_DEBUG"; //optional
    //init_debug = debugMAX; //previously initialized in main()
    iniFindInt(ini_file_ptr, tag, section, &init_debug);
    DBG(init_debug, "[%s] [%s] [%d]", section, tag, init_debug);

    section = "MB2HAL_INIT";
    tag     = "SLOWDOWN"; //optional
    //init_debug_slowdown = 0; //previously initialized in main()
    iniFindDouble(ini_file_ptr, tag, section, &slowdown);
    DBG(init_debug, "[%s] [%s] [%0.3f]", section, tag, slowdown);

    section = "MB2HAL_INIT";
    tag     = "TOTAL_TRANSACTIONS"; //required
    if (iniFindInt(ini_file_ptr, tag, section, &gbl_n_mb_tx) != 0) {
        ERR(init_debug, "required [%s] [%s] not found", section, tag);
        return retERR;
    }
    if (gbl_n_mb_tx <= 0) {
        ERR(init_debug, "[%s] [%s] [%d], must be > 0", section, tag, gbl_n_mb_tx);
        return retERR;
    }
    DBG(init_debug, "[%s] [%s] [%d]", section, tag, gbl_n_mb_tx);

    return retOK;
}


retCode parse_transaction_section(FILE *ini_file_ptr, const int mb_tx_num)
{
    char *fnct_name = "parse_transaction_section";
    char section[20];
    char *tag;
    const char *tmpstr;
    mb_tx_t *tmp_mb_tx;

    if (ini_file_ptr == NULL) {
        ERR(init_debug, "NULL pointer");
        return retERR;
    }
    if (mb_tx_num < 0 || mb_tx_num > gbl_n_mb_tx) {
        ERR(init_debug, "out of range");
        return retERR;
    }

    tmp_mb_tx = &gbl_mb_tx[mb_tx_num];

    if (ini_file_ptr == NULL || mb_tx_num < 0 || mb_tx_num > gbl_n_mb_tx) {
        ERR(init_debug, "parameter error");
        return retERR;
    }

    snprintf(section, sizeof(section)-1, "TRANSACTION_%02d", mb_tx_num);

    tag = "LINK_TYPE"; //required 1st time, then optional
    tmpstr = iniFind(ini_file_ptr, tag, section);
    if (tmpstr != NULL) {
        if (strcasecmp(tmpstr, "tcp") == retOK) {
            tmp_mb_tx->cfg_lk_param.type_com = TCP;
            strcpy(tmp_mb_tx->lp_type_com_str, tmpstr);
        }
        else if (strcasecmp(tmpstr, "serial") == retOK) {
            tmp_mb_tx->cfg_lk_param.type_com = RTU;
            strcpy(tmp_mb_tx->lp_type_com_str, tmpstr);
        }
        else {
            tmp_mb_tx->cfg_lk_param.type_com = -1;
            strcpy(tmp_mb_tx->lp_type_com_str, "");
            ERR(init_debug, "[%s] [%s] [%s] is not valid", section, tag, tmpstr);
            return retERR;
        }
    }
    else {
        if (mb_tx_num > 0) { //previous value
            tmp_mb_tx->cfg_lk_param.type_com = gbl_mb_tx[mb_tx_num-1].cfg_lk_param.type_com;
            strcpy(tmp_mb_tx->lp_type_com_str, gbl_mb_tx[mb_tx_num-1].lp_type_com_str);
        }
        else { //required 1rst time
            tmp_mb_tx->cfg_lk_param.type_com = -1;
            strcpy(tmp_mb_tx->lp_type_com_str, "");
            ERR(init_debug, "required 1st time [%s] [%s] not found", section, tag);
            return retERR;
        }
    }
    DBG(init_debug, "[%s] [%s] [%s] [%d]", section, tag, tmp_mb_tx->lp_type_com_str, tmp_mb_tx->cfg_lk_param.type_com);

    if (tmp_mb_tx->cfg_lk_param.type_com == TCP) { //tcp
        if (parse_tcp_subsection(ini_file_ptr, section, mb_tx_num) != retOK) {
            ERR(init_debug, "parsing error");
            return retERR;
        }
    }
    else { //serial
        if (parse_serial_subsection(ini_file_ptr, section, mb_tx_num) != retOK) {
            ERR(init_debug, "parsing error");
            return retERR;
        }
    }

    tag = "MB_SLAVE_ID"; //1st time required
    if (iniFindInt(ini_file_ptr, tag, section, &tmp_mb_tx->mb_slave_id) != 0) { //not found
        if (mb_tx_num > 0) { //previous value?
            if (strcasecmp(tmp_mb_tx->lp_type_com_str, gbl_mb_tx[mb_tx_num-1].lp_type_com_str) == 0) {
                tmp_mb_tx->mb_slave_id = gbl_mb_tx[mb_tx_num-1].mb_slave_id;
            }
            else {
                tmp_mb_tx->mb_slave_id = -1;
                ERR(init_debug, "required [%s] [%s] not found, and previous LINK_TYPE is useless", section, tag);
                return retERR;
            }
        }
        else { //required 1rst time
            tmp_mb_tx->mb_slave_id = -1;
            ERR(init_debug, "required 1st time [%s] [%s] not found", section, tag);
            return retERR;
        }
    }
    DBG(init_debug, "[%s] [%s] [%d]", section, tag, tmp_mb_tx->mb_slave_id);

    tag = "FIRST_ELEMENT"; //required
    if (iniFindInt(ini_file_ptr, tag, section, &tmp_mb_tx->mb_first_addr) != 0) {
        ERR(init_debug, "required [%s] [%s] not found", section, tag);
        return retERR;
    }
    if (tmp_mb_tx->mb_first_addr < 0) {
        ERR(init_debug, "[%s] [%s] [%d] out of range", section, tag, tmp_mb_tx->mb_first_addr);
        return retERR;
    }
    DBG(init_debug, "[%s] [%s] [%d]", section, tag, tmp_mb_tx->mb_first_addr);

    tag = "NELEMENTS";  //required
    if (iniFindInt(ini_file_ptr, tag, section, &tmp_mb_tx->mb_nelements) != 0) {
        ERR(init_debug, "required [%s] [%s] not found", section, tag);
        return retERR;
    }
    if (tmp_mb_tx->mb_nelements < 1) {
        ERR(init_debug, "[%s] [%s] [%d] out of range", section, tag, tmp_mb_tx->mb_nelements);
        return retERR;
    }
    DBG(init_debug, "[%s] [%s] [%d]", section, tag, tmp_mb_tx->mb_nelements);

    tag = "MAX_UPDATE_RATE"; //optional
    if (iniFindDouble(ini_file_ptr, tag, section, &tmp_mb_tx->cfg_update_rate) != 0) { //not found
        if (mb_tx_num > 0) { //previous value?
            if (strcasecmp(tmp_mb_tx->lp_type_com_str, gbl_mb_tx[mb_tx_num-1].lp_type_com_str) == 0) {
                tmp_mb_tx->cfg_update_rate = gbl_mb_tx[mb_tx_num-1].cfg_update_rate;
            }
            else { //default
                tmp_mb_tx->cfg_update_rate = 0; //0=infinit
            }
        }
    }
    DBG(init_debug, "[%s] [%s] [%0.3f]", section, tag, tmp_mb_tx->cfg_update_rate);

    tag = "DEBUG"; //optional
    if (iniFindInt(ini_file_ptr, tag, section, &tmp_mb_tx->cfg_debug) != 0) { //not found
        if (mb_tx_num > 0) { //previous value?
            if (strcasecmp(tmp_mb_tx->lp_type_com_str, gbl_mb_tx[mb_tx_num-1].lp_type_com_str) == 0) {
                tmp_mb_tx->cfg_debug = gbl_mb_tx[mb_tx_num-1].cfg_debug;
            }
            else { //default
                tmp_mb_tx->cfg_debug = debugERR;
            }
        }
    }
    DBG(init_debug, "[%s] [%s] [%d]", section, tag, tmp_mb_tx->cfg_debug);

    tag = "MB_TX_CODE"; //required
    tmpstr = iniFind(ini_file_ptr, tag, section);
    if (tmpstr != NULL) {
        int i;
        for (i=0 ; i<mbtxMAX; i++) {
            if (strcasecmp(tmpstr, gbl_mb_tx_codes[i]) == 0) {
                tmp_mb_tx->mb_tx_code = i;
                strncpy(tmp_mb_tx->cfg_mb_tx_code_name, tmpstr, sizeof(tmp_mb_tx->cfg_mb_tx_code_name)-1);
                break;
            }
        }
        if (tmp_mb_tx->mb_tx_code <= mbtxERR || tmp_mb_tx->mb_tx_code >= mbtxMAX) {
            ERR(init_debug, "[%s] [%s] [%s] out of range", section, tag, tmpstr);
            return retERR;
        }
    }
    else {
        ERR(init_debug, "required [%s] [%s] not found", section, tag);
        return retERR;
    }
    DBG(init_debug, "[%s] [%s] [%s] [%d]", section, tag, tmp_mb_tx->cfg_mb_tx_code_name, tmp_mb_tx->mb_tx_code);

    tag = "HAL_TX_NAME"; //optional
    tmpstr = iniFind(ini_file_ptr, tag, section);
    if (tmpstr != NULL) {
        strncpy(tmp_mb_tx->hal_tx_name, tmpstr, HAL_NAME_LEN);
    }
    else {
        sprintf(tmp_mb_tx->hal_tx_name, "%02d", mb_tx_num);
    }
    DBG(init_debug, "[%s] [%s] [%s]", section, tag, tmp_mb_tx->hal_tx_name);

    /*
        str = iniFind(ini_file_ptr, "PINNAME", mb_tx_name);
        if (str != NULL) {
            pin_name = malloc(strlen(str) + 1);
            strcpy(pin_name, str);	// convert a const string into one
            // we can modify
        }
        else {
            pin_name = malloc(1);	// empty string
            *pin_name = 0;
        }
        if (mb_tx->name[0] != 0) {
            strncpy(mb_tx_name, mb_tx->name, HAL_NAME_LEN);
        }
        else {
            sprintf(mb_tx_name, "%02d", mb_tx_num);
        }
        memcpy(&gbl_mb_tx[mb_tx_num], mb_tx, sizeof(mb_tx_t));
        rc = create_pins(mb_tx_name, &gbl_mb_tx[mb_tx_num], pin_name);
        free(pin_name);
        if (rc != retOK) {
            ERR(init_debug, "Failed to create pins");
            return retERR;
        }
    */

    return retOK;
}

retCode parse_tcp_subsection(FILE *ini_file_ptr, const char *section, const int mb_tx_num)
{
    char *fnct_name="parse_tcp_subsection";
    char *tag;
    const char *tmpstr;
    mb_tx_t *tmp_mb_tx;

    if (ini_file_ptr == NULL || section == NULL) {
        ERR(init_debug, "NULL pointer");
        return retERR;
    }
    if (mb_tx_num < 0 || mb_tx_num > gbl_n_mb_tx) {
        ERR(init_debug, "out of range");
        return retERR;
    }

    tmp_mb_tx = &gbl_mb_tx[mb_tx_num];

    tag = "TCP_IP"; //required 1st time, then optional
    tmpstr = iniFind(ini_file_ptr, tag, section);
    if (tmpstr != NULL) {
        strncpy(tmp_mb_tx->cfg_lk_param.ip, tmpstr, sizeof(tmp_mb_tx->cfg_lk_param.ip)-1);
    }
    else {
        if (mb_tx_num > 0) { //previous value?
            if (strcasecmp(tmp_mb_tx->lp_type_com_str, gbl_mb_tx[mb_tx_num-1].lp_type_com_str) == 0) {
                strcpy(tmp_mb_tx->cfg_lk_param.ip, gbl_mb_tx[mb_tx_num-1].cfg_lk_param.ip);
            }
            else {
                strcpy(tmp_mb_tx->cfg_lk_param.ip, "");
                ERR(init_debug, "required [%s] [%s] not found, and previous LINK_TYPE is useless", section, tag);
                return retERR;
            }
        }
        else { //required 1rst time
            strcpy(tmp_mb_tx->cfg_lk_param.ip, "");
            ERR(init_debug, "required 1st time [%s] [%s] not found", section, tag);
            return retERR;
        }
    }
    DBG(init_debug, "[%s] [%s] [%s]", section, tag, tmp_mb_tx->cfg_lk_param.ip);

    //Ignored due to NO supported in hal/user_comps/modbus.c, fixed at 502
    tag = "TCP_PORT"; //optional
    if (iniFindInt(ini_file_ptr, tag, section, &tmp_mb_tx->lp_tcp_port) != 0) { //not found
        if (mb_tx_num > 0) { //previous value?
            if (strcasecmp(tmp_mb_tx->lp_type_com_str, gbl_mb_tx[mb_tx_num-1].lp_type_com_str) == 0) {
                tmp_mb_tx->lp_tcp_port = gbl_mb_tx[mb_tx_num-1].lp_tcp_port;
            }
            else { //default
                tmp_mb_tx->lp_tcp_port = MODBUS_TCP_PORT;
            }
        }
    }
    DBG(init_debug, "[%s] [%s] [%d]", section, tag, tmp_mb_tx->lp_tcp_port);

    return retOK;
}

retCode parse_serial_subsection(FILE *ini_file_ptr, const char *section, const int mb_tx_num)
{
    char *fnct_name="parse_serial_subsection";
    char *tag;
    const char *tmpstr;
    mb_tx_t *tmp_mb_tx;

    if (ini_file_ptr == NULL || section == NULL) {
        ERR(init_debug, "NULL pointer");
        return retERR;
    }
    if (mb_tx_num < 0 || mb_tx_num > gbl_n_mb_tx) {
        ERR(init_debug, "out of range");
        return retERR;
    }

    tmp_mb_tx = &gbl_mb_tx[mb_tx_num];

    tag = "SERIAL_PORT"; //required 1st time
    tmpstr = iniFind(ini_file_ptr, tag, section);
    if (tmpstr != NULL) {
        strncpy(tmp_mb_tx->cfg_lk_param.device, tmpstr, sizeof(tmp_mb_tx->cfg_lk_param.device)-1);
    }
    else {
        if (mb_tx_num > 0) { //previous value?
            if (strcasecmp(tmp_mb_tx->lp_type_com_str, gbl_mb_tx[mb_tx_num-1].lp_type_com_str) == 0) {
                strcpy(tmp_mb_tx->cfg_lk_param.device, gbl_mb_tx[mb_tx_num-1].cfg_lk_param.device);
            }
            else {
                strcpy(tmp_mb_tx->cfg_lk_param.device, "");
                ERR(init_debug, "required [%s] [%s] not found, and previous LINK_TYPE is useless", section, tag);
                return retERR;
            }
        }
        else { //required 1rst time
            strcpy(tmp_mb_tx->cfg_lk_param.device, "");
            ERR(init_debug, "required 1st time [%s] [%s] not found", section, tag);
            return retERR;
        }
    }
    DBG(init_debug, "[%s] [%s] [%s]", section, tag, tmp_mb_tx->cfg_lk_param.device);

    tag = "SERIAL_BAUD"; //1st time required
    if (iniFindInt(ini_file_ptr, tag, section, &tmp_mb_tx->cfg_lk_param.baud_i) != 0) { //not found
        if (mb_tx_num > 0) { //previous value?
            if (strcasecmp(tmp_mb_tx->lp_type_com_str, gbl_mb_tx[mb_tx_num-1].lp_type_com_str) == 0) {
                tmp_mb_tx->cfg_lk_param.baud_i = gbl_mb_tx[mb_tx_num-1].cfg_lk_param.baud_i;
            }
            else {
                tmp_mb_tx->cfg_lk_param.baud_i = -1;
                ERR(init_debug, "required [%s] [%s] not found, and previous LINK_TYPE is useless", section, tag);
                return retERR;
            }
        }
        else { //required 1rst time
            tmp_mb_tx->cfg_lk_param.baud_i = -1;
            ERR(init_debug, "required 1st time [%s] [%s] not found", section, tag);
            return retERR;
        }
    }
    DBG(init_debug, "[%s] [%s] [%d]", section, tag, tmp_mb_tx->cfg_lk_param.baud_i);

    tag = "SERIAL_BITS"; //1st time required
    if (iniFindInt(ini_file_ptr, tag, section, &tmp_mb_tx->cfg_lk_param.data_bit) != 0) { //not found
        if (mb_tx_num > 0) { //previous value?
            if (strcasecmp(tmp_mb_tx->lp_type_com_str, gbl_mb_tx[mb_tx_num-1].lp_type_com_str) == 0) {
                tmp_mb_tx->cfg_lk_param.data_bit = gbl_mb_tx[mb_tx_num-1].cfg_lk_param.data_bit;
            }
            else {
                tmp_mb_tx->cfg_lk_param.data_bit = -1;
                ERR(init_debug, "required [%s] [%s] not found, and previous LINK_TYPE is useless", section, tag);
                return retERR;
            }
        }
        else { //required 1rst time
            tmp_mb_tx->cfg_lk_param.data_bit = -1;
            ERR(init_debug, "required 1st time [%s] [%s] not found", section, tag);
            return retERR;
        }
    }
    if (check_int_in(4, tmp_mb_tx->cfg_lk_param.data_bit, 5, 6, 7, 8) != retOK) {
        ERR(init_debug, "[%s] [%s] [%d] out of range", section, tag, tmp_mb_tx->cfg_lk_param.data_bit);
        return retERR;
    }
    DBG(init_debug, "[%s] [%s] [%d]", section, tag, tmp_mb_tx->cfg_lk_param.data_bit);

    tag = "SERIAL_PARITY"; //required 1st time
    tmpstr = iniFind(ini_file_ptr, tag, section);
    if (tmpstr != NULL) {
        strncpy(tmp_mb_tx->cfg_lk_param.parity, tmpstr, sizeof(tmp_mb_tx->cfg_lk_param.parity)-1);
    }
    else {
        if (mb_tx_num > 0) { //previous value?
            if (strcasecmp(tmp_mb_tx->lp_type_com_str, gbl_mb_tx[mb_tx_num-1].lp_type_com_str) == 0) {
                strcpy(tmp_mb_tx->cfg_lk_param.parity, gbl_mb_tx[mb_tx_num-1].cfg_lk_param.parity);
            }
            else {
                strcpy(tmp_mb_tx->cfg_lk_param.parity, "");
                ERR(init_debug, "required [%s] [%s] not found, and previous LINK_TYPE is useless", section, tag);
                return retERR;
            }
        }
        else { //required 1rst time
            strcpy(tmp_mb_tx->cfg_lk_param.parity, "");
            ERR(init_debug, "required 1st time [%s] [%s] not found", section, tag);
            return retERR;
        }
    }
    if (check_str_in(3, tmp_mb_tx->cfg_lk_param.parity, "even", "odd", "none") != retOK) {
        ERR(init_debug, "[%s] [%s] [%s] out of range", section, tag, tmp_mb_tx->cfg_lk_param.parity);
        return retERR;
    }
    DBG(init_debug, "[%s] [%s] [%s]", section, tag, tmp_mb_tx->cfg_lk_param.parity);

    tag = "SERIAL_STOP"; //1st time required
    if (iniFindInt(ini_file_ptr, tag, section, &tmp_mb_tx->cfg_lk_param.stop_bit) != 0) { //not found
        if (mb_tx_num > 0) { //previous value?
            if (strcasecmp(tmp_mb_tx->lp_type_com_str, gbl_mb_tx[mb_tx_num-1].lp_type_com_str) == 0) {
                tmp_mb_tx->cfg_lk_param.stop_bit = gbl_mb_tx[mb_tx_num-1].cfg_lk_param.stop_bit;
            }
            else {
                tmp_mb_tx->cfg_lk_param.stop_bit = -1;
                ERR(init_debug, "required [%s] [%s] not found, and previous LINK_TYPE is useless", section, tag);
                return retERR;
            }
        }
        else { //required 1rst time
            tmp_mb_tx->cfg_lk_param.stop_bit = -1;
            ERR(init_debug, "required 1st time [%s] [%s] not found", section, tag);
            return retERR;
        }
    }
    if (check_int_in(2, tmp_mb_tx->cfg_lk_param.stop_bit, 1, 2) != retOK) {
        ERR(init_debug, "[%s] [%s] [%d] out of range", section, tag, tmp_mb_tx->cfg_lk_param.stop_bit);
        return retERR;
    }
    DBG(init_debug, "[%s] [%s] [%d]", section, tag, tmp_mb_tx->cfg_lk_param.stop_bit);

    tag = "SERIAL_DELAY_MS"; //optional
    if (iniFindInt(ini_file_ptr, tag, section, &tmp_mb_tx->lp_serial_delay_ms) != 0) { //not found
        if (mb_tx_num > 0) { //previous value?
            if (strcasecmp(tmp_mb_tx->lp_type_com_str, gbl_mb_tx[mb_tx_num-1].lp_type_com_str) == 0) {
                tmp_mb_tx->lp_serial_delay_ms = gbl_mb_tx[mb_tx_num-1].lp_serial_delay_ms;
            }
            else { //default
                tmp_mb_tx->lp_serial_delay_ms = 0;
            }
        }
    }
    DBG(init_debug, "[%s] [%s] [%d]", section, tag, tmp_mb_tx->lp_serial_delay_ms);

    return retOK;
}

retCode check_int_in(int n_args, const int int_value, ...)
{
    va_list ap;
    int counter;

    va_start(ap, int_value);

    for (counter = 2; counter < (n_args+2); counter++) {
        if (int_value == va_arg(ap, int)) {
            va_end(ap);
            return retOK; //found
        }
    }

    va_end(ap);
    return retERR; //not found
}

retCode check_str_in(int n_args, const char *str_value, ...)
{
    va_list ap;
    int counter;

    if (str_value == NULL) {
        return retERR; //not found
    }

    va_start(ap, str_value);

    for (counter = 2; counter < (n_args+2); counter++) {
        if (strcasecmp(str_value, va_arg(ap, char *)) == 0) {
            va_end(ap);
            return retOK; //found
        }
    }

    va_end(ap);
    return retERR; //not found
}

/*
 * init global (unrepeated) modbus links (gbl_mb_links)
 * (serial or tcp connections)
 */
retCode init_gbl_mb_links()
{
    char *fnct_name="init_gbl_mb_links";
    int tx_counter, lk_counter;
    int isNewLink;
    modbus_param_t *tmp_lk_param_ptr, *new_gbl_mb_links_ptr;
    mb_tx_t *tmp_mb_tx_ptr;

    //maximum one link per transaction
    gbl_mb_links = malloc(sizeof(modbus_param_t) * gbl_n_mb_tx);
    memset(gbl_mb_links, 0, sizeof(modbus_param_t) * gbl_n_mb_tx);

    //maximum one link per transaction
    tmp_mb_links_num = malloc(sizeof(int) * gbl_n_mb_tx);
    memset(tmp_mb_links_num, 0, sizeof(int) * gbl_n_mb_tx);

    //group common links transactions
    gbl_n_mb_links = 0; //next available unused link

    for (tx_counter = 0; tx_counter < gbl_n_mb_tx; tx_counter++) {
        tmp_mb_tx_ptr    = &gbl_mb_tx[tx_counter];
        tmp_lk_param_ptr = &gbl_mb_tx[tx_counter].cfg_lk_param;

        isNewLink = 1; //Default to true
        if (tmp_lk_param_ptr->type_com == RTU) { //serial
            for (lk_counter = 0; lk_counter < gbl_n_mb_links; lk_counter++) {
                if (strcasecmp(tmp_lk_param_ptr->device, gbl_mb_links[lk_counter].device) == 0) {
                    isNewLink = 0; //repeated link
                    tmp_mb_tx_ptr->mb_links_num = lk_counter; //each tx know its own link
                    break;
                }
            }
        }
        else { //tcp
            for (lk_counter = 0; lk_counter < gbl_n_mb_links; lk_counter++) {
                if (strcasecmp(tmp_lk_param_ptr->ip, gbl_mb_links[lk_counter].ip) == 0) {
                    isNewLink = 0; //repeated link
                    tmp_mb_tx_ptr->mb_links_num = lk_counter; //each tx know its own link
                    break;
                }
            }
        }
        if (isNewLink != 0) { //initialize new link
            tmp_mb_tx_ptr->mb_links_num = gbl_n_mb_links; //each tx know its own link
            tmp_mb_links_num[gbl_n_mb_links] = gbl_n_mb_links; //for thread parameter use only

            new_gbl_mb_links_ptr = &gbl_mb_links[gbl_n_mb_links];

            new_gbl_mb_links_ptr->type_com = tmp_lk_param_ptr->type_com;

            new_gbl_mb_links_ptr->fd = -1; //link closed
            //new_gbl_mb_links_ptr->mb_links_num; //already initialized
            //copied run time for each mb_tx from mb_tx.cfk_lk_param to mb_link
            //new_gbl_mb_links_ptr->print_errors = (tmp_mb_tx_ptr->cfg_debug >= debugERR)? 1 : 0;
            //new_gbl_mb_links_ptr->debug = (tmp_mb_tx_ptr->cfg_debug >= debugDEBUG)? 1 : 0;

            if (new_gbl_mb_links_ptr->type_com == RTU) { //serial
                modbus_init_rtu(new_gbl_mb_links_ptr, tmp_lk_param_ptr->device,
                                tmp_lk_param_ptr->baud_i, tmp_lk_param_ptr->parity,
                                tmp_lk_param_ptr->data_bit, tmp_lk_param_ptr->stop_bit, 0);
            }
            else { //tcp
                modbus_init_tcp(new_gbl_mb_links_ptr, tmp_lk_param_ptr->ip, 0);
            }

            new_gbl_mb_links_ptr->fd = -1; //link closed
            gbl_n_mb_links++; //next available unused link
        }
    }

    //DEBUG messagess if needed
    for (lk_counter = 0; lk_counter < gbl_n_mb_links; lk_counter++) {
        tmp_lk_param_ptr = &gbl_mb_links[lk_counter];

        if (tmp_lk_param_ptr->type_com == RTU) { //serial
            DBG(init_debug, "LINK %d (RTU) type_com[%d] device[%s] baud[%d] data[%d] parity[%s] stop[%d] fd[%d]",
                lk_counter, tmp_lk_param_ptr->type_com, tmp_lk_param_ptr->device,
                tmp_lk_param_ptr->baud_i, tmp_lk_param_ptr->data_bit, tmp_lk_param_ptr->parity,
                tmp_lk_param_ptr->stop_bit, tmp_lk_param_ptr->fd);
        }
        else { //tcp
            DBG(init_debug, "LINK %d (TCP) type_com[%d] IP[%s] fd[%d]",
                lk_counter, tmp_lk_param_ptr->type_com, tmp_lk_param_ptr->ip, tmp_lk_param_ptr->fd);
        }
    }

    return retOK;
}

/*
 * init more parameters of global modbus transactions (gbl_mb_tx)
 */
retCode init_gbl_mb_tx()
{
    char *fnct_name="init_gbl_mb_tx";
    int tx_counter;
    modbus_param_t *tmp_lk_param_ptr;
    mb_tx_t *tmp_mb_tx_ptr;

    for (tx_counter = 0; tx_counter < gbl_n_mb_tx; tx_counter++) {
        tmp_mb_tx_ptr    = &gbl_mb_tx[tx_counter];
        tmp_lk_param_ptr = &gbl_mb_tx[tx_counter].cfg_lk_param;

        tmp_mb_tx_ptr->mb_tx_num = tx_counter;

        //initialized in init_gbl_mb_links
        //tmp_lk_param_ptr->fd = -1; //link closed
        //tmp_mb_tx_ptr->mb_links_num;
        //copied run time from this to mb_link for each mb_tx
        tmp_lk_param_ptr->print_errors = (tmp_mb_tx_ptr->cfg_debug >= debugERR)? 1 : 0;
        tmp_lk_param_ptr->debug = (tmp_mb_tx_ptr->cfg_debug >= debugDEBUG)? 1 : 0;

        if (tmp_mb_tx_ptr->cfg_update_rate > 0) {
            tmp_mb_tx_ptr->time_increment = 1.0 / tmp_mb_tx_ptr->cfg_update_rate; //wait time between tx
        }
        tmp_mb_tx_ptr->next_time = 0; //next time for this tx

        DBG(init_debug, "MB_TX %d lk_n[%d] tx_n[%d] cfg_dbg[%d] fd[%d] lk_print_err[%d] lk_dbg[%d] t_inc[%0.3f] nxt_t[%0.3f]",
            tx_counter, tmp_mb_tx_ptr->mb_links_num, tmp_mb_tx_ptr->mb_tx_num, tmp_mb_tx_ptr->cfg_debug,
            tmp_lk_param_ptr->fd, tmp_lk_param_ptr->print_errors,
            tmp_lk_param_ptr->debug, tmp_mb_tx_ptr->time_increment, tmp_mb_tx_ptr->next_time);
    }

    return retOK;
}
