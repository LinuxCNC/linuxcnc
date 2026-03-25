#include "mb2hal.h"

retCode parse_args(mb2hal_module *m, int argc, const char **argv)
{
    char *fnct_name = "parse_args";
    int i;

    for (i = 0; i < argc; i++) {
        if (strncmp(argv[i], "config=", 7) == 0) {
            const char *path = argv[i] + 7;

            if (*path == '\"') { //file name is quoted
                path++;
                char *tmp = strdup(path);
                char *end = strchr(tmp, '\"');
                if (end) *end = '\0';
                m->ini_file_path = tmp;
            } else {
                m->ini_file_path = strdup(path);
            }
            return retOK;
        }
    }

    ERR(m, m->init_dbg, "config parameter not found");
    return retERR;
}

retCode parse_ini_file(mb2hal_module *m)
{
    char *fnct_name = "parse_ini_file";
    int counter;

    if (m->ini_file_ptr == NULL) {
        ERR(m, m->init_dbg, "m->ini_file_ptr NULL pointer");
        return retERR;
    }

    if (parse_common_section(m) != retOK) {
        ERR(m, m->init_dbg, "parse_common_section failed");
        return retERR;
    }

    //default = one link per transaction
    //realloc will be used if there are common links
    m->mb_links = malloc(sizeof(mb_link_t) * m->tot_mb_tx);
    if (m->mb_links == NULL) {
        ERR(m, m->init_dbg, "malloc m->mb_links failed [%s]", strerror(errno));
        return retERR;
    }
    memset(m->mb_links, 0, sizeof(mb_link_t) * m->tot_mb_tx);

    m->mb_tx = malloc(sizeof(mb_tx_t) * m->tot_mb_tx);
    if (m->mb_tx == NULL) {
        ERR(m, m->init_dbg, "malloc m->mb_tx failed [%s]", strerror(errno));
        return retERR;
    }
    memset(m->mb_tx, 0, sizeof(mb_tx_t) * m->tot_mb_tx);

    for (counter = 0; counter < m->tot_mb_tx; counter++) {
        if (parse_transaction_section(m, counter) != retOK) {
            ERR(m, m->init_dbg, "parse_transaction_section %d failed", counter);
            return retERR;
        }
        OK(m, m->init_dbg, "parse_transaction_section %d OK", counter);
    }

    return retOK;
}

retCode parse_common_section(mb2hal_module *m)
{
    char *fnct_name = "parse_common_section";
    char *section = "MB2HAL_INIT", *tag;
    const char *tmpstr;

    if (m->ini_file_ptr == NULL) {
        ERR(m, m->init_dbg, "m->ini_file_ptr NULL pointer");
        return retERR;
    }

    tag     = "INIT_DEBUG"; //optional
    iniFindInt(m->ini_file_ptr, tag, section, &m->init_dbg);
    DBG(m, m->init_dbg, "[%s] [%s] [%d]", section, tag, m->init_dbg);

    tag     = "VERSION"; //optional
    tmpstr = iniFind(m->ini_file_ptr, tag, section);
    if (tmpstr != NULL) {
        int major, minor;
        sscanf(tmpstr, "%d.%d", &major, &minor);
        m->version = major*1000 + minor;
    }
    DBG(m, m->init_dbg, "[%s] [%s] [%d]", section, tag, m->version);

    // HAL_MODULE_NAME from INI is ignored — the component name from the
    // constructor (m->name) is used instead.
    DBG(m, m->init_dbg, "using component name [%s] from constructor", m->name);

    tag     = "SLOWDOWN"; //optional
    iniFindDouble(m->ini_file_ptr, tag, section, &m->slowdown);
    DBG(m, m->init_dbg, "[%s] [%s] [%0.3f]", section, tag, m->slowdown);

    tag     = "TOTAL_TRANSACTIONS"; //required
    if (iniFindInt(m->ini_file_ptr, tag, section, &m->tot_mb_tx) != 0) {
        ERR(m, m->init_dbg, "required [%s] [%s] not found", section, tag);
        return retERR;
    }
    if (m->tot_mb_tx <= 0) {
        ERR(m, m->init_dbg, "[%s] [%s] [%d], must be > 0", section, tag, m->tot_mb_tx);
        return retERR;
    }
    DBG(m, m->init_dbg, "[%s] [%s] [%d]", section, tag, m->tot_mb_tx);

    return retOK;
}

#define NAME_ALLOC_SIZE 5
static retCode parse_pin_names(mb2hal_module *m, const char * const names_string, mb_tx_t * const this_mb_tx)
{
    char *fnct_name = "parse_pin_names";
    int name_count = 0;
    int name_buf_size = NAME_ALLOC_SIZE;
    char **name_ptrs = malloc(sizeof(char *) * name_buf_size);
    double *scales = malloc(sizeof(double) * name_buf_size);
    double *offsets = malloc(sizeof(double) * name_buf_size);
    /* FIXME This memory block is leaked */
    char *names = strndup(names_string,999942);
    if(name_ptrs == NULL || names == NULL || scales == NULL || offsets == NULL)
    {
        ERR(m, m->init_dbg, "Failed allocating memory");
        return retERR;
    }
    char * name = strtok(names, ",");
    while(name)
    {
        if(strlen(name) > HAL_NAME_LEN - 15) //this is only a rough estimate
        {
            ERR(m, m->init_dbg, "pin name '%s' is too long", name);
            return retERR;
        }
        if(name_count >= name_buf_size)
        {
            name_buf_size += NAME_ALLOC_SIZE;
            name_ptrs = realloc(name_ptrs, sizeof(char *) * name_buf_size);
            scales = realloc(scales, sizeof(double) * name_buf_size);
            offsets = realloc(offsets, sizeof(double) * name_buf_size);
            if(name_ptrs == NULL || scales == NULL || offsets == NULL)
            {
                ERR(m, m->init_dbg, "Failed allocating memory");
                return retERR;
            }
        }
        // Parse optional [*SCALE+OFFSET] or [*SCALE] or [+OFFSET] or [-OFFSET]
        double scale = 1.0;
        double offset = 0.0;
        char *bracket = strchr(name, '[');
        if (bracket) {
            *bracket = '\0'; // terminate the pin name
            char *p = bracket + 1;
            if (*p == '*') {
                p++;
                char *end;
                scale = strtod(p, &end);
                p = end;
            }
            if (*p == '+' || *p == '-') {
                char *end;
                offset = strtod(p, &end);
                p = end;
            }
            if (*p != ']') {
                ERR(m, m->init_dbg, "pin name '%s': malformed [*scale+offset] (expected ']' but got '%c')", name, *p);
                return retERR;
            }
        }
        scales[name_count] = scale;
        offsets[name_count] = offset;
        name_ptrs[name_count++]=name;
        name = strtok(NULL, ",");
    }
    if(name_count == 0)
    {
        ERR(m, m->init_dbg, "no pin names specified");
        return retERR;
    }
    this_mb_tx->mb_tx_nelem = name_count;
    this_mb_tx->mb_tx_names = name_ptrs;
    this_mb_tx->pin_scale = scales;
    this_mb_tx->pin_offset = offsets;
    return retOK;
}

retCode parse_transaction_section(mb2hal_module *m, const int mb_tx_num)
{
    char *fnct_name = "parse_transaction_section";
    char section[40];
    char *tag;
    const char *tmpstr;
    mb_tx_t *this_mb_tx;

    if (m->ini_file_ptr == NULL) {
        ERR(m, m->init_dbg, "m->ini_file_ptr NULL pointer");
        return retERR;
    }
    if (mb_tx_num < 0 || mb_tx_num > m->tot_mb_tx) {
        ERR(m, m->init_dbg, "out of range");
        return retERR;
    }

    this_mb_tx = &m->mb_tx[mb_tx_num];

    if (m->ini_file_ptr == NULL || mb_tx_num < 0 || mb_tx_num > m->tot_mb_tx) {
        ERR(m, m->init_dbg, "parameter error");
        return retERR;
    }

    snprintf(section, sizeof(section)-1, "TRANSACTION_%02d", mb_tx_num);

    tag = "LINK_TYPE"; //required 1st time, then optional
    tmpstr = iniFind(m->ini_file_ptr, tag, section);
    if (tmpstr != NULL) {
        if (strcasecmp(tmpstr, "tcp") == retOK) {
            this_mb_tx->cfg_link_type = linkTCP;
            rtapi_strxcpy(this_mb_tx->cfg_link_type_str, tmpstr);
        }
        else if (strcasecmp(tmpstr, "serial") == retOK) {
            this_mb_tx->cfg_link_type = linkRTU;
            rtapi_strxcpy(this_mb_tx->cfg_link_type_str, tmpstr);
        }
        else {
            this_mb_tx->cfg_link_type = -1;
            rtapi_strxcpy(this_mb_tx->cfg_link_type_str, "");
            ERR(m, m->init_dbg, "[%s] [%s] [%s] is not valid", section, tag, tmpstr);
            return retERR;
        }
    }
    else {
        if (mb_tx_num > 0) { //previous value
            this_mb_tx->cfg_link_type = m->mb_tx[mb_tx_num-1].cfg_link_type;
            rtapi_strxcpy(this_mb_tx->cfg_link_type_str, m->mb_tx[mb_tx_num-1].cfg_link_type_str);
        }
        else { //required 1rst time
            this_mb_tx->cfg_link_type = -1;
            rtapi_strxcpy(this_mb_tx->cfg_link_type_str, "");
            ERR(m, m->init_dbg, "required 1st time [%s] [%s] not found", section, tag);
            return retERR;
        }
    }
    DBG(m, m->init_dbg, "[%s] [%s] [%s] [%d]", section, tag, this_mb_tx->cfg_link_type_str, this_mb_tx->cfg_link_type);

    if (this_mb_tx->cfg_link_type == linkTCP) { //tcp
        if (parse_tcp_subsection(m, section, mb_tx_num) != retOK) {
            ERR(m, m->init_dbg, "parsing error");
            return retERR;
        }
    }
    else { //serial
        if (parse_serial_subsection(m, section, mb_tx_num) != retOK) {
            ERR(m, m->init_dbg, "parsing error");
            return retERR;
        }
    }

    tag = "MB_SLAVE_ID"; //1st time required
    if (iniFindInt(m->ini_file_ptr, tag, section, &this_mb_tx->mb_tx_slave_id) != 0) { //not found
        if (mb_tx_num > 0) { //previous value?
            if (strcasecmp(this_mb_tx->cfg_link_type_str, m->mb_tx[mb_tx_num-1].cfg_link_type_str) == 0) {
                this_mb_tx->mb_tx_slave_id = m->mb_tx[mb_tx_num-1].mb_tx_slave_id;
            }
            else {
                this_mb_tx->mb_tx_slave_id = -1;
                ERR(m, m->init_dbg, "required [%s] [%s] not found, and previous LINK_TYPE is useless", section, tag);
                return retERR;
            }
        }
        else { //required 1rst time
            this_mb_tx->mb_tx_slave_id = -1;
            ERR(m, m->init_dbg, "required 1st time [%s] [%s] not found", section, tag);
            return retERR;
        }
    }
    DBG(m, m->init_dbg, "[%s] [%s] [%d]", section, tag, this_mb_tx->mb_tx_slave_id);

    tag = "FIRST_ELEMENT"; //required
    if (iniFindInt(m->ini_file_ptr, tag, section, &this_mb_tx->mb_tx_1st_addr) != 0) {
        ERR(m, m->init_dbg, "required [%s] [%s] not found", section, tag);
        return retERR;
    }
    if (this_mb_tx->mb_tx_1st_addr < 0) {
        ERR(m, m->init_dbg, "[%s] [%s] [%d] out of range", section, tag, this_mb_tx->mb_tx_1st_addr);
        return retERR;
    }
    DBG(m, m->init_dbg, "[%s] [%s] [%d]", section, tag, this_mb_tx->mb_tx_1st_addr);


    tag = "PIN_NAMES";
    tmpstr = iniFind(m->ini_file_ptr, tag, section);
    if (tmpstr != NULL) {
        if(parse_pin_names(m, tmpstr, this_mb_tx) != retOK)
        {
            ERR(m, m->init_dbg, "[%s] [%s] [%s] list format error", section, tag, tmpstr);
            return retERR;
        }
        DBG(m, m->init_dbg, "[%s] [%s] [%s]", section, tag, tmpstr);
    }
    else {
        this_mb_tx->mb_tx_names = NULL;
    }

    tag = "NELEMENTS";  //required
    if (iniFindInt(m->ini_file_ptr, tag, section, &this_mb_tx->mb_tx_nelem) != 0 &&
        this_mb_tx->mb_tx_names == NULL) {
        ERR(m, m->init_dbg, "required [%s] [%s] or [%s] [PIN_NAMES] were not found", section, tag, section);
        return retERR;
    }
    if (this_mb_tx->mb_tx_nelem < 1) {
        ERR(m, m->init_dbg, "[%s] [%s] [%d] out of range", section, tag, this_mb_tx->mb_tx_nelem);
        return retERR;
    }
    DBG(m, m->init_dbg, "[%s] [%s] [%d]", section, tag, this_mb_tx->mb_tx_nelem);

    tag = "MAX_UPDATE_RATE"; //optional
    this_mb_tx->cfg_update_rate = 0; //default: 0=infinite
    if (iniFindDouble(m->ini_file_ptr, tag, section, &this_mb_tx->cfg_update_rate) != 0) { //not found
        if (mb_tx_num > 0) { //previous value?
            if (strcasecmp(this_mb_tx->cfg_link_type_str, m->mb_tx[mb_tx_num-1].cfg_link_type_str) == 0) {
                this_mb_tx->cfg_update_rate = m->mb_tx[mb_tx_num-1].cfg_update_rate;
            }
        }
    }
    DBG(m, m->init_dbg, "[%s] [%s] [%0.3f]", section, tag, this_mb_tx->cfg_update_rate);

    tag = "MB_RESPONSE_TIMEOUT_MS"; //optional
    this_mb_tx->mb_response_timeout_ms = MB2HAL_DEFAULT_MB_RESPONSE_TIMEOUT_MS; //default
    if (iniFindInt(m->ini_file_ptr, tag, section, &this_mb_tx->mb_response_timeout_ms) != 0) { //not found
        if (mb_tx_num > 0) { //previous value?
            if (strcasecmp(this_mb_tx->cfg_link_type_str, m->mb_tx[mb_tx_num-1].cfg_link_type_str) == 0) {
                this_mb_tx->mb_response_timeout_ms = m->mb_tx[mb_tx_num-1].mb_response_timeout_ms;
            }
        }
    }
    DBG(m, m->init_dbg, "[%s] [%s] [%d]", section, tag, this_mb_tx->mb_response_timeout_ms);

    tag = "MB_BYTE_TIMEOUT_MS"; //optional
    this_mb_tx->mb_byte_timeout_ms = MB2HAL_DEFAULT_MB_BYTE_TIMEOUT_MS; //default
    if (iniFindInt(m->ini_file_ptr, tag, section, &this_mb_tx->mb_byte_timeout_ms) != 0) { //not found
        if (mb_tx_num > 0) { //previous value?
            if (strcasecmp(this_mb_tx->cfg_link_type_str, m->mb_tx[mb_tx_num-1].cfg_link_type_str) == 0) {
                this_mb_tx->mb_byte_timeout_ms = m->mb_tx[mb_tx_num-1].mb_byte_timeout_ms;
            }
        }
    }
    DBG(m, m->init_dbg, "[%s] [%s] [%d]", section, tag, this_mb_tx->mb_byte_timeout_ms);

    tag = "DEBUG"; //optional
    this_mb_tx->cfg_debug = debugERR; //default
    if (iniFindInt(m->ini_file_ptr, tag, section, &this_mb_tx->cfg_debug) != 0) { //not found
        if (mb_tx_num > 0) { //previous value?
            if (strcasecmp(this_mb_tx->cfg_link_type_str, m->mb_tx[mb_tx_num-1].cfg_link_type_str) == 0) {
                this_mb_tx->cfg_debug = m->mb_tx[mb_tx_num-1].cfg_debug;
            }
        }
    }
    DBG(m, m->init_dbg, "[%s] [%s] [%d]", section, tag, this_mb_tx->cfg_debug);

    tag = "MB_TX_CODE"; //required
    tmpstr = iniFind(m->ini_file_ptr, tag, section);
    if (tmpstr != NULL) {
        int i;
        for (i=0 ; i<mbtxMAX; i++) {
            if (strcasecmp(tmpstr, m->mb_tx_fncts[i]) == 0) {
                this_mb_tx->mb_tx_fnct = i;
                strncpy(this_mb_tx->mb_tx_fnct_name, tmpstr, sizeof(this_mb_tx->mb_tx_fnct_name)-1);
                break;
            }
        }
        int max = m->version<1001?mbtx_01_READ_COILS:mbtxMAX;
        if (this_mb_tx->mb_tx_fnct <= mbtxERR || this_mb_tx->mb_tx_fnct >= max) {
            ERR(m, m->init_dbg, "[%s] [%s] [%s] out of range", section, tag, tmpstr);
            return retERR;
        }
    }
    else {
        ERR(m, m->init_dbg, "required [%s] [%s] not found", section, tag);
        return retERR;
    }
    DBG(m, m->init_dbg, "[%s] [%s] [%s] [%d]", section, tag, this_mb_tx->mb_tx_fnct_name, this_mb_tx->mb_tx_fnct);

    tag = "HAL_TX_NAME"; //optional
    tmpstr = iniFind(m->ini_file_ptr, tag, section);
    if (tmpstr != NULL) {
        strncpy(this_mb_tx->hal_tx_name, tmpstr, HAL_NAME_LEN);
    }
    else {
        snprintf(this_mb_tx->hal_tx_name, sizeof(this_mb_tx->hal_tx_name), "%02d", mb_tx_num);
    }
    DBG(m, m->init_dbg, "[%s] [%s] [%s]", section, tag, this_mb_tx->hal_tx_name);

    return retOK;
}

retCode parse_tcp_subsection(mb2hal_module *m, const char *section, const int mb_tx_num)
{
    char *fnct_name="parse_tcp_subsection";
    char *tag;
    const char *tmpstr;
    mb_tx_t *this_mb_tx;

    if (m->ini_file_ptr == NULL || section == NULL) {
        ERR(m, m->init_dbg, "m->ini_file_ptr NULL pointer");
        return retERR;
    }
    if (mb_tx_num < 0 || mb_tx_num > m->tot_mb_tx) {
        ERR(m, m->init_dbg, "out of range");
        return retERR;
    }

    this_mb_tx = &m->mb_tx[mb_tx_num];

    tag = "TCP_IP"; //required 1st time, then optional
    tmpstr = iniFind(m->ini_file_ptr, tag, section);
    if (tmpstr != NULL) {
        strncpy(this_mb_tx->cfg_tcp_ip, tmpstr, sizeof(this_mb_tx->cfg_tcp_ip)-1);
    }
    else {
        if (mb_tx_num > 0) { //previous value?
            if (strcasecmp(this_mb_tx->cfg_link_type_str, m->mb_tx[mb_tx_num-1].cfg_link_type_str) == 0) {
                rtapi_strxcpy(this_mb_tx->cfg_tcp_ip, m->mb_tx[mb_tx_num-1].cfg_tcp_ip);
            }
            else {
                rtapi_strxcpy(this_mb_tx->cfg_tcp_ip, "");
                ERR(m, m->init_dbg, "required [%s] [%s] not found, and previous LINK_TYPE is useless", section, tag);
                return retERR;
            }
        }
        else { //required 1rst time
            rtapi_strxcpy(this_mb_tx->cfg_tcp_ip, "");
            ERR(m, m->init_dbg, "required 1st time [%s] [%s] not found", section, tag);
            return retERR;
        }
    }
    DBG(m, m->init_dbg, "[%s] [%s] [%s]", section, tag, this_mb_tx->cfg_tcp_ip);

    tag = "TCP_PORT"; //optional
    this_mb_tx->cfg_tcp_port = MB2HAL_DEFAULT_TCP_PORT; //default
    if (iniFindInt(m->ini_file_ptr, tag, section, &this_mb_tx->cfg_tcp_port) != 0) { //not found
        if (mb_tx_num > 0) { //previous value?
            if (strcasecmp(this_mb_tx->cfg_link_type_str, m->mb_tx[mb_tx_num-1].cfg_link_type_str) == 0) {
                this_mb_tx->cfg_tcp_port = m->mb_tx[mb_tx_num-1].cfg_tcp_port;
            }
        }
    }
    DBG(m, m->init_dbg, "[%s] [%s] [%d]", section, tag, this_mb_tx->cfg_tcp_port);

    return retOK;
}

retCode parse_serial_subsection(mb2hal_module *m, const char *section, const int mb_tx_num)
{
    char *fnct_name="parse_serial_subsection";
    char *tag;
    const char *tmpstr;
    mb_tx_t *this_mb_tx;

    if (m->ini_file_ptr == NULL || section == NULL) {
        ERR(m, m->init_dbg, "m->ini_file_ptr NULL pointer");
        return retERR;
    }
    if (mb_tx_num < 0 || mb_tx_num > m->tot_mb_tx) {
        ERR(m, m->init_dbg, "out of range");
        return retERR;
    }

    this_mb_tx = &m->mb_tx[mb_tx_num];

    tag = "SERIAL_PORT"; //required 1st time
    tmpstr = iniFind(m->ini_file_ptr, tag, section);
    if (tmpstr != NULL) {
        strncpy(this_mb_tx->cfg_serial_device, tmpstr, sizeof(this_mb_tx->cfg_serial_device)-1);
    }
    else {
        if (mb_tx_num > 0) { //previous value?
            if (strcasecmp(this_mb_tx->cfg_link_type_str, m->mb_tx[mb_tx_num-1].cfg_link_type_str) == 0) {
                rtapi_strxcpy(this_mb_tx->cfg_serial_device, m->mb_tx[mb_tx_num-1].cfg_serial_device);
            }
            else {
                rtapi_strxcpy(this_mb_tx->cfg_serial_device, "");
                ERR(m, m->init_dbg, "required [%s] [%s] not found, and previous LINK_TYPE is useless", section, tag);
                return retERR;
            }
        }
        else { //required 1rst time
            rtapi_strxcpy(this_mb_tx->cfg_serial_device, "");
            ERR(m, m->init_dbg, "required 1st time [%s] [%s] not found", section, tag);
            return retERR;
        }
    }
    DBG(m, m->init_dbg, "[%s] [%s] [%s]", section, tag, this_mb_tx->cfg_serial_device);

    tag = "SERIAL_BAUD"; //1st time required
    if (iniFindInt(m->ini_file_ptr, tag, section, &this_mb_tx->cfg_serial_baud) != 0) { //not found
        if (mb_tx_num > 0) { //previous value?
            if (strcasecmp(this_mb_tx->cfg_link_type_str, m->mb_tx[mb_tx_num-1].cfg_link_type_str) == 0) {
                this_mb_tx->cfg_serial_baud = m->mb_tx[mb_tx_num-1].cfg_serial_baud;
            }
            else {
                this_mb_tx->cfg_serial_baud = -1;
                ERR(m, m->init_dbg, "required [%s] [%s] not found, and previous LINK_TYPE is useless", section, tag);
                return retERR;
            }
        }
        else { //required 1rst time
            this_mb_tx->cfg_serial_baud = -1;
            ERR(m, m->init_dbg, "required 1st time [%s] [%s] not found", section, tag);
            return retERR;
        }
    }
    DBG(m, m->init_dbg, "[%s] [%s] [%d]", section, tag, this_mb_tx->cfg_serial_baud);

    tag = "SERIAL_BITS"; //1st time required
    if (iniFindInt(m->ini_file_ptr, tag, section, &this_mb_tx->cfg_serial_data_bit) != 0) { //not found
        if (mb_tx_num > 0) { //previous value?
            if (strcasecmp(this_mb_tx->cfg_link_type_str, m->mb_tx[mb_tx_num-1].cfg_link_type_str) == 0) {
                this_mb_tx->cfg_serial_data_bit = m->mb_tx[mb_tx_num-1].cfg_serial_data_bit;
            }
            else {
                this_mb_tx->cfg_serial_data_bit = -1;
                ERR(m, m->init_dbg, "required [%s] [%s] not found, and previous LINK_TYPE is useless", section, tag);
                return retERR;
            }
        }
        else { //required 1rst time
            this_mb_tx->cfg_serial_data_bit = -1;
            ERR(m, m->init_dbg, "required 1st time [%s] [%s] not found", section, tag);
            return retERR;
        }
    }
    if (check_int_in(4, this_mb_tx->cfg_serial_data_bit, 5, 6, 7, 8) != retOK) {
        ERR(m, m->init_dbg, "[%s] [%s] [%d] out of range", section, tag, this_mb_tx->cfg_serial_data_bit);
        return retERR;
    }
    DBG(m, m->init_dbg, "[%s] [%s] [%d]", section, tag, this_mb_tx->cfg_serial_data_bit);

    tag = "SERIAL_PARITY"; //required 1st time
    tmpstr = iniFind(m->ini_file_ptr, tag, section);
    if (tmpstr != NULL) {
        strncpy(this_mb_tx->cfg_serial_parity, tmpstr, sizeof(this_mb_tx->cfg_serial_parity)-1);
    }
    else {
        if (mb_tx_num > 0) { //previous value?
            if (strcasecmp(this_mb_tx->cfg_link_type_str, m->mb_tx[mb_tx_num-1].cfg_link_type_str) == 0) {
                rtapi_strxcpy(this_mb_tx->cfg_serial_parity, m->mb_tx[mb_tx_num-1].cfg_serial_parity);
            }
            else {
                rtapi_strxcpy(this_mb_tx->cfg_serial_parity, "");
                ERR(m, m->init_dbg, "required [%s] [%s] not found, and previous LINK_TYPE is useless", section, tag);
                return retERR;
            }
        }
        else { //required 1rst time
            rtapi_strxcpy(this_mb_tx->cfg_serial_parity, "");
            ERR(m, m->init_dbg, "required 1st time [%s] [%s] not found", section, tag);
            return retERR;
        }
    }
    if (check_str_in(3, this_mb_tx->cfg_serial_parity, "even", "odd", "none") != retOK) {
        ERR(m, m->init_dbg, "[%s] [%s] [%s] out of range", section, tag, this_mb_tx->cfg_serial_parity);
        return retERR;
    }
    DBG(m, m->init_dbg, "[%s] [%s] [%s]", section, tag, this_mb_tx->cfg_serial_parity);

    tag = "SERIAL_STOP"; //1st time required
    if (iniFindInt(m->ini_file_ptr, tag, section, &this_mb_tx->cfg_serial_stop_bit) != 0) { //not found
        if (mb_tx_num > 0) { //previous value?
            if (strcasecmp(this_mb_tx->cfg_link_type_str, m->mb_tx[mb_tx_num-1].cfg_link_type_str) == 0) {
                this_mb_tx->cfg_serial_stop_bit = m->mb_tx[mb_tx_num-1].cfg_serial_stop_bit;
            }
            else {
                this_mb_tx->cfg_serial_stop_bit = -1;
                ERR(m, m->init_dbg, "required [%s] [%s] not found, and previous LINK_TYPE is useless", section, tag);
                return retERR;
            }
        }
        else { //required 1rst time
            this_mb_tx->cfg_serial_stop_bit = -1;
            ERR(m, m->init_dbg, "required 1st time [%s] [%s] not found", section, tag);
            return retERR;
        }
    }
    if (check_int_in(2, this_mb_tx->cfg_serial_stop_bit, 1, 2) != retOK) {
        ERR(m, m->init_dbg, "[%s] [%s] [%d] out of range", section, tag, this_mb_tx->cfg_serial_stop_bit);
        return retERR;
    }
    DBG(m, m->init_dbg, "[%s] [%s] [%d]", section, tag, this_mb_tx->cfg_serial_stop_bit);

    tag = "SERIAL_DELAY_MS"; //optional
    this_mb_tx->cfg_serial_delay_ms = 0; //default
    if (iniFindInt(m->ini_file_ptr, tag, section, &this_mb_tx->cfg_serial_delay_ms) != 0) { //not found
        if (mb_tx_num > 0) { //previous value?
            if (strcasecmp(this_mb_tx->cfg_link_type_str, m->mb_tx[mb_tx_num-1].cfg_link_type_str) == 0) {
                this_mb_tx->cfg_serial_delay_ms = m->mb_tx[mb_tx_num-1].cfg_serial_delay_ms;
            }
        }
    }
    DBG(m, m->init_dbg, "[%s] [%s] [%d]", section, tag, this_mb_tx->cfg_serial_delay_ms);

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
 * init global (unrepeated) modbus links (m->mb_links[])
 * (serial or tcp connections)
 */
retCode init_mb_links(mb2hal_module *m)
{
    char *fnct_name="init_mb_links";
    int tx_counter, lk_counter;
    int isNewLink;
    mb_link_t *this_mb_link;
    mb_tx_t   *this_mb_tx;

    //group common links transactions
    m->tot_mb_links = 0; //total and next available unused link

    for (tx_counter = 0; tx_counter < m->tot_mb_tx; tx_counter++) {
        this_mb_tx   = &m->mb_tx[tx_counter];

        isNewLink = 1; //Default to true
        if (this_mb_tx->cfg_link_type == linkRTU) { //serial
            for (lk_counter = 0; lk_counter < m->tot_mb_links; lk_counter++) {
                if (strcasecmp(this_mb_tx->cfg_serial_device, m->mb_links[lk_counter].lp_serial_device) == 0) {
                    isNewLink = 0; //repeated link
                    this_mb_tx->mb_link_num = lk_counter; //each tx know its own link
                    break;
                }
            }
        }
        else { //tcp
            for (lk_counter = 0; lk_counter < m->tot_mb_links; lk_counter++) {
                if (strcasecmp(this_mb_tx->cfg_tcp_ip, m->mb_links[lk_counter].lp_tcp_ip) == 0) {
                    isNewLink = 0; //repeated link
                    this_mb_tx->mb_link_num = lk_counter; //each tx know its own link
                    break;
                }
            }
        }
        if (isNewLink != 0) { //initialize new link
            this_mb_tx->mb_link_num = m->mb_links[m->tot_mb_links].mb_link_num  = m->tot_mb_links; //next available unused link
            this_mb_link = &m->mb_links[m->tot_mb_links];

            this_mb_link->lp_link_type = this_mb_tx->cfg_link_type;
            this_mb_link->m = m; //set back-pointer to parent module

            if (this_mb_link->lp_link_type == linkRTU) { //serial
                if (strlen(this_mb_tx->cfg_serial_device) >= MB2HAL_MAX_DEVICE_LENGTH) {
                    ERR(m, m->init_dbg, "serial_device name to long [%s]", this_mb_tx->cfg_serial_device);
                    return retERR;
                }
                rtapi_strlcpy(this_mb_link->lp_serial_device, this_mb_tx->cfg_serial_device, MB2HAL_MAX_DEVICE_LENGTH);
                this_mb_link->lp_serial_baud=this_mb_tx->cfg_serial_baud;

                if (strcasecmp(this_mb_tx->cfg_serial_parity, "even") == 0) {
                    this_mb_link->lp_serial_parity = 'E';
                }
                else if (strcasecmp(this_mb_tx->cfg_serial_parity, "odd") == 0) {
                    this_mb_link->lp_serial_parity = 'O';
                }
                else { //default = parity none
                    this_mb_link->lp_serial_parity = 'N';
                }

                this_mb_link->lp_serial_data_bit=this_mb_tx->cfg_serial_data_bit;
                this_mb_link->lp_serial_stop_bit=this_mb_tx->cfg_serial_stop_bit;

                this_mb_link->modbus = modbus_new_rtu(this_mb_link->lp_serial_device,
                                                      this_mb_link->lp_serial_baud, this_mb_link->lp_serial_parity,
                                                      this_mb_link->lp_serial_data_bit, this_mb_link->lp_serial_stop_bit);
                if (this_mb_link->modbus == NULL) {
                    ERR(m, m->init_dbg, "modbus_new_rtu failed [%s] [%d] [%c] [%d] [%d]", this_mb_link->lp_serial_device,
                        this_mb_link->lp_serial_baud, this_mb_link->lp_serial_parity, this_mb_link->lp_serial_data_bit,
                        this_mb_link->lp_serial_stop_bit);
                    return retERR;
                }
            }
            else { //tcp
                if (strlen(this_mb_tx->cfg_tcp_ip) >= sizeof(this_mb_link->lp_tcp_ip)) {
                    ERR(m, m->init_dbg, "tcp_ip too long [%s]", this_mb_tx->cfg_tcp_ip);
                    return retERR;
                }
                rtapi_strlcpy(this_mb_link->lp_tcp_ip, this_mb_tx->cfg_tcp_ip, sizeof(this_mb_tx->cfg_tcp_ip));
                this_mb_link->lp_tcp_port=this_mb_tx->cfg_tcp_port;

                this_mb_link->modbus = modbus_new_tcp(this_mb_link->lp_tcp_ip, this_mb_link->lp_tcp_port);
                if (this_mb_link->modbus == NULL) {
                    ERR(m, m->init_dbg, "modbus_new_tcp failed [%s] [%d]", this_mb_link->lp_tcp_ip, this_mb_link->lp_tcp_port);
                    return retERR;
                }
            }

            if (this_mb_link->modbus)
            {
                m->tot_mb_links++;    //set new total and next available unused link
            }
        }
    }

    //DEBUG messagess if needed
    for (lk_counter = 0; lk_counter < m->tot_mb_links; lk_counter++) {
        this_mb_link = &m->mb_links[lk_counter];

        if (this_mb_link->lp_link_type == linkRTU) { //serial
            DBG(m, m->init_dbg, "LINK %d (RTU) link_type[%d] device[%s] baud[%d] data[%d] parity[%c] stop[%d] fd[%d]",
                lk_counter, this_mb_link->lp_link_type, this_mb_link->lp_serial_device,
                this_mb_link->lp_serial_baud, this_mb_link->lp_serial_data_bit, this_mb_link->lp_serial_parity,
                this_mb_link->lp_serial_stop_bit, modbus_get_socket(this_mb_link->modbus));
        }
        else { //tcp
            DBG(m, m->init_dbg, "LINK %d (TCP) link_type[%d] IP[%s] port[%d] fd[%d]",
                lk_counter, this_mb_link->lp_link_type, this_mb_link->lp_tcp_ip,
                this_mb_link->lp_tcp_port, modbus_get_socket(this_mb_link->modbus));
        }
    }

    m->mb_links = realloc(m->mb_links, sizeof(mb_link_t) * m->tot_mb_links);
    if (m->mb_links == NULL) {
        DBG(m, m->init_dbg, "realloc m->mb_links failed [%s]", strerror(errno));
        return retERR;
    }

    // Ensure back-pointers are valid after realloc
    for (lk_counter = 0; lk_counter < m->tot_mb_links; lk_counter++) {
        m->mb_links[lk_counter].m = m;
    }

    return retOK;
}

/*
 * init more parameters of modbus transactions (m->mb_tx)
 */
retCode init_mb_tx(mb2hal_module *m)
{
    char *fnct_name="init_mb_tx";
    int tx_counter;
    mb_tx_t   *this_mb_tx;

    for (tx_counter = 0; tx_counter < m->tot_mb_tx; tx_counter++) {
        this_mb_tx = &m->mb_tx[tx_counter];

        this_mb_tx->mb_tx_num = tx_counter;
        this_mb_tx->protocol_debug = (this_mb_tx->cfg_debug >= debugDEBUG)? 1 : 0;

        if (this_mb_tx->cfg_update_rate > 0) {
            this_mb_tx->time_increment = 1.0 / this_mb_tx->cfg_update_rate; //wait time between tx
        }
        this_mb_tx->next_time = 0; //next time for this tx

        DBG(m, m->init_dbg, "MB_TX %d lk_n[%d] tx_n[%d] cfg_dbg[%d] lk_dbg[%d] t_inc[%0.3f] nxt_t[%0.3f]",
            tx_counter, this_mb_tx->mb_link_num, this_mb_tx->mb_tx_num, this_mb_tx->cfg_debug,
            this_mb_tx->protocol_debug, this_mb_tx->time_increment, this_mb_tx->next_time);
    }

    return retOK;
}
