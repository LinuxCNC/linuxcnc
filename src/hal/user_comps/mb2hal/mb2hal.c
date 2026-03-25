/*
 * mb2hal.c
 * Userspace HAL component to communicate with one or more Modbus devices.
 * Migrated to cmod plugin API for in-process launcher operation.
 *
 * Victor Rocco, adapted from Les Newell's modbuscomms.c which is
 * Copyright (C) 2009-2012 Les Newell <les@sheetcam.com>
 * source code in http://wiki.linuxcnc.org/cgi-bin/wiki.pl?ContributedComponents
 *
 * Copyright (C) 2012 Victor Rocco <victor_rocco AT hotmail DOT com>
 *
 * This library is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published
 * by the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301-1307
 * USA.
 */

#include "mb2hal.h"

/*
 * One thread loop for each link
 * The LOGIC is here
 * arg is a pointer to this thread's mb_link_t (which includes a back-pointer to the module)
 */

void *link_loop_and_logic(void *arg)
{
    char *fnct_name = "link_loop_and_logic";
    int ret, ret_available, ret_connected;
    int tx_counter;
    mb_tx_t   *this_mb_tx = NULL;
    int        this_mb_tx_num;
    mb_link_t *this_mb_link = NULL;
    int        this_mb_link_num;

    if (arg == NULL) {
        return NULL;
    }
    this_mb_link = (mb_link_t *)arg;
    mb2hal_module *m = this_mb_link->m;
    this_mb_link_num = this_mb_link->mb_link_num;

    if (this_mb_link_num < 0 || this_mb_link_num >= m->tot_mb_links) {
        ERR(m, m->init_dbg, "parameter out of range this_mb_link_num[%d]", this_mb_link_num);
        return NULL;
    }

    while (1) {

        for (tx_counter = 0; tx_counter < m->tot_mb_tx; tx_counter++) {

            if (m->done != 0) {
                return NULL;
            }

            this_mb_tx_num = tx_counter;
            this_mb_tx = &m->mb_tx[this_mb_tx_num];

            DBGMAX(m, this_mb_tx->cfg_debug, "mb_tx_num[%d] mb_links[%d] thread[%d] fd[%d] going to TEST availability",
                this_mb_tx_num, this_mb_tx->mb_link_num, this_mb_link_num, modbus_get_socket(this_mb_link->modbus));

            //corresponding link and time (update_rate)
            if (is_this_tx_ready(m, this_mb_link_num, this_mb_tx_num, &ret_available) != retOK) {
                ERR(m, this_mb_tx->cfg_debug, "mb_tx_num[%d] mb_links[%d] thread[%d] fd[%d] is_this_tx_ready ERR",
                    this_mb_tx_num, this_mb_tx->mb_link_num, this_mb_link_num, modbus_get_socket(this_mb_link->modbus));
                return NULL;
            }
            if (ret_available == 0) {
                DBGMAX(m, this_mb_tx->cfg_debug, "mb_tx_num[%d] mb_links[%d] thread[%d] fd[%d] NOT available",
                    this_mb_tx_num, this_mb_tx->mb_link_num, this_mb_link_num, modbus_get_socket(this_mb_link->modbus));
                usleep(1000);
                continue;
            }

            DBGMAX(m, this_mb_tx->cfg_debug, "mb_tx_num[%d] mb_links[%d] thread[%d] fd[%d] going to TEST connection",
                this_mb_tx_num, this_mb_tx->mb_link_num, this_mb_link_num, modbus_get_socket(this_mb_link->modbus));

            //first time connection or reconnection, run time parameters setting
            if (get_tx_connection(m, this_mb_tx_num, &ret_connected) != retOK) {
                ERR(m, this_mb_tx->cfg_debug, "mb_tx_num[%d] mb_links[%d] thread[%d] fd[%d] get_tx_connection ERR",
                    this_mb_tx_num, this_mb_tx->mb_link_num, this_mb_link_num, modbus_get_socket(this_mb_link->modbus));
                return NULL;
            }
            if (ret_connected == 0) {
                DBGMAX(m, this_mb_tx->cfg_debug, "mb_tx_num[%d] mb_links[%d] thread[%d] fd[%d] NOT connected",
                    this_mb_tx_num, this_mb_tx->mb_link_num, this_mb_link_num, modbus_get_socket(this_mb_link->modbus));
                usleep(1000);
                continue;
            }

            DBGMAX(m, this_mb_tx->cfg_debug, "mb_tx_num[%d] mb_links[%d] thread[%d] fd[%d] lk_dbg[%d] going to EXECUTE transaction",
                this_mb_tx_num, this_mb_tx->mb_link_num, this_mb_link_num, modbus_get_socket(this_mb_link->modbus),
                this_mb_tx->protocol_debug);

            switch (this_mb_tx->mb_tx_fnct) {

            case mbtx_01_READ_COILS:
                ret = fnct_01_read_coils(m, this_mb_tx, this_mb_link);
                break;
            case mbtx_02_READ_DISCRETE_INPUTS:
                ret = fnct_02_read_discrete_inputs(m, this_mb_tx, this_mb_link);
                break;
            case mbtx_03_READ_HOLDING_REGISTERS:
                ret = fnct_03_read_holding_registers(m, this_mb_tx, this_mb_link);
                break;
            case mbtx_04_READ_INPUT_REGISTERS:
                ret = fnct_04_read_input_registers(m, this_mb_tx, this_mb_link);
                break;
            case mbtx_05_WRITE_SINGLE_COIL:
                ret = fnct_05_write_single_coil(m, this_mb_tx, this_mb_link);
                break;
            case mbtx_06_WRITE_SINGLE_REGISTER:
                ret = fnct_06_write_single_register(m, this_mb_tx, this_mb_link);
                break;
            case mbtx_15_WRITE_MULTIPLE_COILS:
                ret = fnct_15_write_multiple_coils(m, this_mb_tx, this_mb_link);
                break;
            case mbtx_16_WRITE_MULTIPLE_REGISTERS:
                ret = fnct_16_write_multiple_registers(m, this_mb_tx, this_mb_link);
                break;
            default:
                ret = -1;
                ERR(m, this_mb_tx->cfg_debug, "case error with mb_tx_fnct %d [%s] in mb_tx_num[%d]",
                    this_mb_tx->mb_tx_fnct, this_mb_tx->mb_tx_fnct_name, this_mb_tx_num);
                break;
            }

            if (m->done != 0) {
                return NULL;
            }

            if (ret != retOK && modbus_get_socket(this_mb_link->modbus) < 0) { //link failure
                (**this_mb_tx->num_errors)++;
                ERR(m, this_mb_tx->cfg_debug, "mb_tx_num[%d] mb_links[%d] thread[%d] fd[%d] link failure, going to close link",
                    this_mb_tx_num, this_mb_tx->mb_link_num, this_mb_link_num, modbus_get_socket(this_mb_link->modbus));
                modbus_close(this_mb_link->modbus);
            }
            else if (ret != retOK) {  //transaction failure but link OK
                (**this_mb_tx->num_errors)++;
                ERR(m, this_mb_tx->cfg_debug, "mb_tx_num[%d] mb_links[%d] thread[%d] fd[%d] transaction failure, num_errors[%d]",
                    this_mb_tx_num, this_mb_tx->mb_link_num, this_mb_link_num, modbus_get_socket(this_mb_link->modbus), **this_mb_tx->num_errors);
                // Clear any unread data. Otherwise the link might get out of sync
                modbus_flush(this_mb_link->modbus);
            }
            else { //transaction and link OK
                OK(m, this_mb_tx->cfg_debug, "mb_tx_num[%d] mb_links[%d] thread[%d] fd[%d] transaction OK, update_HZ[%0.03f]",
                   this_mb_tx_num, this_mb_tx->mb_link_num, this_mb_link_num, modbus_get_socket(this_mb_link->modbus),
                   1.0/(get_time()-this_mb_tx->last_time_ok));
                this_mb_tx->last_time_ok = get_time();
                (**this_mb_tx->num_errors) = 0;
            }

            //set the next (waiting) time for update rate
            this_mb_tx->next_time = get_time() + this_mb_tx->time_increment;

            //wait time for serial lines
            if (this_mb_tx->cfg_link_type == linkRTU) {
                DBG(m, this_mb_tx->cfg_debug, "mb_tx_num[%d] mb_links[%d] thread[%d] fd[%d] SERIAL_DELAY_MS activated [%d]",
                    this_mb_tx_num, this_mb_tx->mb_link_num, this_mb_link_num, modbus_get_socket(this_mb_link->modbus),
                    this_mb_tx->cfg_serial_delay_ms);
                usleep(this_mb_tx->cfg_serial_delay_ms * 1000);
            }

            //wait time to slowdown activity (debugging)
            if (m->slowdown > 0) {
                DBG(m, this_mb_tx->cfg_debug, "mb_tx_num[%d] mb_links[%d] thread[%d] fd[%d] slowdown activated [%0.3f]",
                    this_mb_tx_num, this_mb_tx->mb_link_num, this_mb_link_num, modbus_get_socket(this_mb_link->modbus), m->slowdown);
                usleep(m->slowdown * 1000 * 1000);
            }
        }  //end for

    } //end while

    return NULL;
}

/*
 * Check if the transaction is available for this link
 */

retCode is_this_tx_ready(mb2hal_module *m, const int this_mb_link_num, const int this_mb_tx_num, int *ret_available)
{
    char *fnct_name = "is_this_tx_available";
    mb_tx_t *this_mb_tx;
    int this_mb_tx_link_num;

    if (this_mb_tx_num < 0 || this_mb_tx_num > m->tot_mb_tx) {
        ERR(m, m->init_dbg, "parameter out of range this_mb_tx_num[%d]", this_mb_tx_num);
        return retERR;
    }
    this_mb_tx = &m->mb_tx[this_mb_tx_num];

    if (ret_available == NULL) {
        ERR(m, this_mb_tx->cfg_debug, "NULL pointer");
        return retERR;
    }

    this_mb_tx_link_num = this_mb_tx->mb_link_num;
    if (this_mb_tx_link_num < 0 || this_mb_tx_link_num >= m->tot_mb_links) {
        ERR(m, this_mb_tx->cfg_debug, "parameter out of range this_mb_tx_link_num[%d]", this_mb_tx_link_num);
        return retERR;
    }

    *ret_available = 0; //defaults to not available

    //the tx is not of this link
    if (this_mb_link_num != this_mb_tx_link_num) {
        return retOK;
    }

    //not now
    if (get_time() < this_mb_tx->next_time) {
        return retOK;
    }

    *ret_available = 1; //is available
    return retOK;
}

/*
 * First time connection or reconnection
 */

retCode get_tx_connection(mb2hal_module *m, const int this_mb_tx_num, int *ret_connected)
{
    char *fnct_name = "get_tx_connection";
    int ret;
    mb_tx_t   *this_mb_tx;
    mb_link_t *this_mb_link;
    int        this_mb_link_num;
    struct timeval timeout;

    if (this_mb_tx_num < 0 || this_mb_tx_num > m->tot_mb_tx) {
        ERR(m, m->init_dbg, "parameter out of range this_mb_tx_num[%d]", this_mb_tx_num);
        return retERR;
    }
    this_mb_tx = &m->mb_tx[this_mb_tx_num];

    if (ret_connected == NULL) {
        ERR(m, this_mb_tx->cfg_debug, "NULL pointer");
        return retERR;
    }

    this_mb_link_num = this_mb_tx->mb_link_num;
    if (this_mb_link_num < 0 || this_mb_link_num >= m->tot_mb_links) {
        ERR(m, this_mb_tx->cfg_debug, "parameter out of range this_mb_link_num[%d]", this_mb_link_num);
        return retERR;
    }
    this_mb_link = &m->mb_links[this_mb_link_num];

    *ret_connected = 0; //defaults to not connected

    if (modbus_get_socket(this_mb_link->modbus) < 0) {
        ret = modbus_connect(this_mb_link->modbus);
        if (ret != 0 || modbus_get_socket(this_mb_link->modbus) < 0) {
            modbus_set_socket(this_mb_link->modbus, -1); //some times ret was < 0 and fd > 0
            (**this_mb_tx->num_errors)++;
            ERR(m, this_mb_tx->cfg_debug, "mb_tx_num[%d] mb_links[%d] cannot connect to link, ret[%d] fd[%d]",
                this_mb_tx_num, this_mb_tx->mb_link_num, ret, modbus_get_socket(this_mb_link->modbus));
            return retOK; //not connected
        }
        DBGMAX(m, this_mb_tx->cfg_debug, "mb_tx_num[%d] mb_links[%d] new connection -> fd[%d]",
            this_mb_tx_num, this_mb_tx->mb_link_num, modbus_get_socket(this_mb_link->modbus));
    }
    else {
        DBGMAX(m, this_mb_tx->cfg_debug, "mb_tx_num[%d] mb_links[%d] already connected to fd[%d]",
            this_mb_tx_num, this_mb_tx->mb_link_num, modbus_get_socket(this_mb_link->modbus));
    }

    //set slave id according to each mb_tx
    ret = modbus_set_slave(this_mb_link->modbus, this_mb_tx->mb_tx_slave_id);
    if (ret != 0) {
        ERR(m, this_mb_tx->cfg_debug, "mb_tx_num[%d] mb_links[%d] cannot set slave [%d]",
            this_mb_tx_num, this_mb_tx->mb_link_num, this_mb_tx->mb_tx_slave_id);
        return retOK; //not connected
    }

    //set the low level mb_link debug according to each mb_tx
    modbus_set_debug(this_mb_link->modbus, this_mb_tx->protocol_debug);

    //set response and byte timeout according to each mb_tx
    timeout.tv_sec  = this_mb_tx->mb_response_timeout_ms / 1000;
    timeout.tv_usec = (this_mb_tx->mb_response_timeout_ms % 1000) * 1000;
#if LIBMODBUS_VERSION_CHECK(3, 1, 2)
    modbus_set_response_timeout(this_mb_link->modbus, timeout.tv_sec, timeout.tv_usec);
#else
    modbus_set_response_timeout(this_mb_link->modbus, &timeout);
#endif

    timeout.tv_sec  = this_mb_tx->mb_byte_timeout_ms / 1000;
    timeout.tv_usec = (this_mb_tx->mb_byte_timeout_ms % 1000) * 1000;
#if LIBMODBUS_VERSION_CHECK(3, 1, 2)
    modbus_set_byte_timeout(this_mb_link->modbus, timeout.tv_sec, timeout.tv_usec);
#else
    modbus_set_byte_timeout(this_mb_link->modbus, &timeout);
#endif

    *ret_connected = 1; //is connected (fd >= 0)
    return retOK;
}

double get_time()
{
    struct timeval time;
    gettimeofday(&time, NULL);
    return (time.tv_sec + ((double) time.tv_usec / 1000000.0f));
}

/*
 * Clean up all module resources
 */

static void mb2hal_cleanup(mb2hal_module *m)
{
    char *fnct_name = "mb2hal_cleanup";
    int counter;

    DBG(m, m->init_dbg, "started");

    for (counter = 0; counter < m->tot_mb_links; counter++) {
        if (m->mb_links[counter].modbus != NULL) {
            modbus_close(m->mb_links[counter].modbus);
            modbus_free(m->mb_links[counter].modbus);
            m->mb_links[counter].modbus = NULL;
        }
    }
    m->tot_mb_links = 0;

    if (m->mb_tx != NULL) {
        free(m->mb_tx);
        m->mb_tx = NULL;
    }
    m->tot_mb_tx = 0;

    if (m->mb_links != NULL) {
        free(m->mb_links);
        m->mb_links = NULL;
    }

    if (m->ini_file_ptr != NULL) {
        fclose(m->ini_file_ptr);
        m->ini_file_ptr = NULL;
    }

    if (m->ini_file_path != NULL) {
        free(m->ini_file_path);
        m->ini_file_path = NULL;
    }

    DBG(m, m->init_dbg, "done OK");
}

/********************************************************************
* cmod lifecycle functions
********************************************************************/

static int mb2hal_start(cmod_t *self)
{
    mb2hal_module *m = (mb2hal_module *)self->priv;
    char *fnct_name = "mb2hal_start";
    int counter, ret;

    m->done = 0;

    /* Each link has it's own thread */
    for (counter = 0; counter < m->tot_mb_links; counter++) {
        ret = pthread_create(&m->mb_links[counter].thrd, NULL, link_loop_and_logic, (void *) &m->mb_links[counter]);
        if (ret != 0) {
            ERR(m, m->init_dbg, "Unable to start thread for link number %d", counter);
            /* Stop already-started threads */
            m->done = 1;
            for (int i = 0; i < counter; i++) {
                pthread_join(m->mb_links[i].thrd, NULL);
            }
            return -1;
        }
        OK(m, m->init_dbg, "Link thread loop and logic %d created OK", counter);
    }

    OK(m, m->init_dbg, "%s is running", m->name);
    return 0;
}

static void mb2hal_stop(cmod_t *self)
{
    mb2hal_module *m = (mb2hal_module *)self->priv;
    char *fnct_name = "mb2hal_stop";
    int counter;

    m->done = 1;

    for (counter = 0; counter < m->tot_mb_links; counter++) {
        int ret = pthread_join(m->mb_links[counter].thrd, NULL);
        if (ret != 0) {
            ERR(m, m->init_dbg, "Unable to join thread for link number %d: %s", counter, strerror(ret));
        }
    }

    DBG(m, m->init_dbg, "all threads joined");
}

static void mb2hal_destroy(cmod_t *self)
{
    mb2hal_module *m = (mb2hal_module *)self->priv;
    char *fnct_name = "mb2hal_destroy";
    int ret;

    mb2hal_cleanup(m);

    if (m->hal_mod_id >= 0) {
        ret = hal_exit(m->hal_mod_id);
        DBG(m, m->init_dbg, "unloading HAL module [%d] ret[%d]", m->hal_mod_id, ret);
    }

    OK(m, m->init_dbg, "going to exit!");
    free(m);
}

/********************************************************************
* New — cmod factory function.
* The launcher calls dlsym(handle, "New") to find this.
*
* Init sequence: parse args, open ini file, parse ini file
* (transaction structures), init links (links structures), init and
* create hal pins.
********************************************************************/
int New(const cmod_env_t *env, const char *name,
        int argc, const char **argv, cmod_t **out)
{
    char *fnct_name = "New";
    mb2hal_module *m;

    m = calloc(1, sizeof(mb2hal_module));
    if (m == NULL) {
        return -1;
    }

    m->env = env;
    strncpy(m->name, name, sizeof(m->name) - 1);

    // Set defaults
    m->hal_mod_id   = -1;
    m->init_dbg     = debugERR;
    m->version      = 1000;
    m->slowdown     = 0;
    m->done         = 0;
    m->ini_file_ptr = NULL;
    m->ini_file_path = NULL;
    m->mb_tx        = NULL;
    m->mb_links     = NULL;
    m->tot_mb_tx    = 0;
    m->tot_mb_links = 0;
    m->mb_tx_fncts[mbtxERR]                         = "";
    m->mb_tx_fncts[mbtx_01_READ_COILS]              = "fnct_01_read_coils";
    m->mb_tx_fncts[mbtx_02_READ_DISCRETE_INPUTS]    = "fnct_02_read_discrete_inputs";
    m->mb_tx_fncts[mbtx_03_READ_HOLDING_REGISTERS]  = "fnct_03_read_holding_registers";
    m->mb_tx_fncts[mbtx_04_READ_INPUT_REGISTERS]    = "fnct_04_read_input_registers";
    m->mb_tx_fncts[mbtx_05_WRITE_SINGLE_COIL]       = "fnct_05_write_single_coil";
    m->mb_tx_fncts[mbtx_06_WRITE_SINGLE_REGISTER]   = "fnct_06_write_single_register";
    m->mb_tx_fncts[mbtx_15_WRITE_MULTIPLE_COILS]    = "fnct_15_write_multiple_coils";
    m->mb_tx_fncts[mbtx_16_WRITE_MULTIPLE_REGISTERS]= "fnct_16_write_multiple_registers";

    if (parse_args(m, argc, argv) != retOK) {
        ERR(m, m->init_dbg, "Unable to parse arguments");
        free(m);
        return -1;
    }

    m->ini_file_ptr = fopen(m->ini_file_path, "r");
    if (m->ini_file_ptr == NULL) {
        ERR(m, m->init_dbg, "Unable to open INI file [%s]", m->ini_file_path);
        free(m->ini_file_path);
        free(m);
        return -1;
    }

    if (parse_ini_file(m) != retOK) {
        ERR(m, m->init_dbg, "Unable to parse INI file [%s]", m->ini_file_path);
        mb2hal_cleanup(m);
        free(m);
        return -1;
    }
    OK(m, m->init_dbg, "parse_ini_file done OK");

    if (init_mb_links(m) != retOK) {
        ERR(m, m->init_dbg, "init_mb_links failed");
        mb2hal_cleanup(m);
        free(m);
        return -1;
    }
    OK(m, m->init_dbg, "init_mb_links done OK");

    if (init_mb_tx(m) != retOK) {
        ERR(m, m->init_dbg, "init_mb_tx failed");
        mb2hal_cleanup(m);
        free(m);
        return -1;
    }
    OK(m, m->init_dbg, "init_mb_tx done OK");

    // Use the component name from the constructor
    m->hal_mod_id = hal_init(m->name);
    if (m->hal_mod_id < 0) {
        ERR(m, m->init_dbg, "Unable to initialize HAL component [%s]", m->name);
        mb2hal_cleanup(m);
        free(m);
        return -1;
    }
    if (create_HAL_pins(m) != retOK) {
        ERR(m, m->init_dbg, "Unable to create HAL pins");
        hal_exit(m->hal_mod_id);
        mb2hal_cleanup(m);
        free(m);
        return -1;
    }

    hal_ready(m->hal_mod_id);
    OK(m, m->init_dbg, "HAL component [%s] created OK", m->name);

    // Wire up the lifecycle vtable
    m->base.Start   = mb2hal_start;
    m->base.Stop    = mb2hal_stop;
    m->base.Destroy = mb2hal_destroy;
    m->base.priv    = m;

    *out = &m->base;
    return 0;
}
