/**
 * @file gmi_ethercat.c
 * @brief GMI REST API callbacks for the EtherCAT cmod.
 *
 * Implements the ethercat_callbacks_t function pointers generated from
 * ethercat.gmi.  Each callback resolves the appropriate ec_master_t from
 * the instance's master linked list (via master_index) and delegates to
 * the ecrt_tool_*() in-process API.
 *
 * The ctx pointer passed to each callback is the lcec_rt_context_t*.
 */

#include "priv.h"
#include "lcec.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#include "ecrt_tool.h"
#include "ethercat_api.h"

#include <arpa/inet.h>

/* ─── Helpers ─────────────────────────────────────────────────────────────── */

/**
 * Resolve the Nth ec_master_t from the instance's master linked list.
 * Returns NULL if index is out of range.
 */
static ec_master_t *resolve_master(lcec_rt_context_t *ctx, uint32_t master_index)
{
    lcec_master_t *m = ctx->first_master;
    uint32_t i = 0;
    while (m) {
        if (i == master_index)
            return m->master;
        m = m->next;
        i++;
    }
    return NULL;
}

/* Format a MAC address as "XX:XX:XX:XX:XX:XX". Returns thread-local rotating buffer. */
static const char *format_mac(const uint8_t *addr)
{
    static __thread char buf[EC_TOOL_MAX_NUM_DEVICES][18];
    static __thread int idx = 0;
    char *out = buf[idx % EC_TOOL_MAX_NUM_DEVICES];
    idx++;
    snprintf(out, 18, "%02x:%02x:%02x:%02x:%02x:%02x",
             addr[0], addr[1], addr[2], addr[3], addr[4], addr[5]);
    return out;
}

/*
 * String lifetime: The Go cgo dispatch layer copies all returned const char *
 * fields immediately via C.GoString() before the next callback can be invoked.
 * We use thread-local storage for tool structs whose char[] fields we expose
 * as const char * pointers. This is safe because all REST dispatch happens on
 * a single goroutine per request (serialized by cgo).
 */

/* ─── Callback implementations ────────────────────────────────────────────── */

static ethercat_module_info_t gmi_ethercat_get_module(void *ctx)
{
    (void)ctx;
    ec_tool_module_t mod;
    ethercat_module_info_t out = {0};

    if (ecrt_tool_get_module(&mod) == 0) {
        out.ioctl_version_magic = mod.ioctl_version_magic;
        out.master_count = mod.master_count;
    }
    return out;
}

static ethercat_master_info_t gmi_ethercat_get_master(void *ctx,
        uint32_t master_index)
{
    lcec_rt_context_t *rt = (lcec_rt_context_t *)ctx;
    ethercat_master_info_t out = {0};
    ec_tool_master_t mst;
    ec_master_t *master;

    master = resolve_master(rt, master_index);
    if (!master)
        return out;

    if (ecrt_tool_get_master(master, &mst) != 0)
        return out;

    out.slave_count = mst.slave_count;
    out.scan_index = mst.scan_index;
    out.config_count = mst.config_count;
    out.domain_count = mst.domain_count;
    out.eoe_handler_count = mst.eoe_handler_count;
    out.phase = mst.phase;
    out.active = mst.active;
    out.scan_busy = mst.scan_busy;
    out.num_devices = mst.num_devices;
    out.tx_count = mst.tx_count;
    out.rx_count = mst.rx_count;
    out.tx_bytes = mst.tx_bytes;
    out.rx_bytes = mst.rx_bytes;
    memcpy(out.tx_frame_rates, mst.tx_frame_rates, sizeof(out.tx_frame_rates));
    memcpy(out.rx_frame_rates, mst.rx_frame_rates, sizeof(out.rx_frame_rates));
    memcpy(out.tx_byte_rates, mst.tx_byte_rates, sizeof(out.tx_byte_rates));
    memcpy(out.rx_byte_rates, mst.rx_byte_rates, sizeof(out.rx_byte_rates));
    memcpy(out.loss_rates, mst.loss_rates, sizeof(out.loss_rates));
    out.app_time = mst.app_time;
    out.dc_ref_time = mst.dc_ref_time;
    out.ref_clock = mst.ref_clock;

    /* Device stats — dynamic array. */
    out.devices = calloc(mst.num_devices, sizeof(ethercat_device_stats_t));
    out.devices_len = mst.num_devices;
    if (out.devices) {
        for (uint32_t i = 0; i < mst.num_devices && i < EC_TOOL_MAX_NUM_DEVICES; i++) {
            out.devices[i].address = format_mac(mst.devices[i].address);
            out.devices[i].attached = mst.devices[i].attached;
            out.devices[i].link_state = mst.devices[i].link_state;
            out.devices[i].tx_count = mst.devices[i].tx_count;
            out.devices[i].rx_count = mst.devices[i].rx_count;
            out.devices[i].tx_bytes = mst.devices[i].tx_bytes;
            out.devices[i].rx_bytes = mst.devices[i].rx_bytes;
            out.devices[i].tx_errors = mst.devices[i].tx_errors;
            memcpy(out.devices[i].tx_frame_rates, mst.devices[i].tx_frame_rates,
                   sizeof(out.devices[i].tx_frame_rates));
            memcpy(out.devices[i].rx_frame_rates, mst.devices[i].rx_frame_rates,
                   sizeof(out.devices[i].rx_frame_rates));
            memcpy(out.devices[i].tx_byte_rates, mst.devices[i].tx_byte_rates,
                   sizeof(out.devices[i].tx_byte_rates));
            memcpy(out.devices[i].rx_byte_rates, mst.devices[i].rx_byte_rates,
                   sizeof(out.devices[i].rx_byte_rates));
        }
    }

    return out;
}

static ethercat_slave_info_t gmi_ethercat_get_slave(void *ctx,
        uint32_t master_index, uint16_t position)
{
    lcec_rt_context_t *rt = (lcec_rt_context_t *)ctx;
    ethercat_slave_info_t out = {0};
    static __thread ec_tool_slave_t slv;
    ec_master_t *master;

    master = resolve_master(rt, master_index);
    if (!master)
        return out;

    slv.position = position;
    if (ecrt_tool_get_slave(master, &slv) != 0)
        return out;

    out.position = slv.position;
    out.device_index = slv.device_index;
    out.vendor_id = slv.vendor_id;
    out.product_code = slv.product_code;
    out.revision_number = slv.revision_number;
    out.serial_number = slv.serial_number;
    out.alias = slv.alias;
    out.boot_rx_mailbox_offset = slv.boot_rx_mailbox_offset;
    out.boot_rx_mailbox_size = slv.boot_rx_mailbox_size;
    out.boot_tx_mailbox_offset = slv.boot_tx_mailbox_offset;
    out.boot_tx_mailbox_size = slv.boot_tx_mailbox_size;
    out.std_rx_mailbox_offset = slv.std_rx_mailbox_offset;
    out.std_rx_mailbox_size = slv.std_rx_mailbox_size;
    out.std_tx_mailbox_offset = slv.std_tx_mailbox_offset;
    out.std_tx_mailbox_size = slv.std_tx_mailbox_size;
    out.mailbox_protocols = slv.mailbox_protocols;
    out.has_general_category = slv.has_general_category;
    out.current_on_ebus = slv.current_on_ebus;
    for (int i = 0; i < ETHERCAT_MAX_PORTS; i++) {
        out.ports[i].desc = slv.ports[i].desc;
        out.ports[i].link = (slv.ports[i].link.link_up ? 0x01 : 0)
                          | (slv.ports[i].link.loop_closed ? 0x02 : 0)
                          | (slv.ports[i].link.signal_detected ? 0x04 : 0);
        out.ports[i].receive_time = slv.ports[i].receive_time;
        out.ports[i].next_slave = slv.ports[i].next_slave;
        out.ports[i].delay_to_next_dc = slv.ports[i].delay_to_next_dc;
    }
    out.dc_supported = slv.dc_supported;
    out.dc_range = (uint8_t)slv.dc_range;
    out.has_dc_system_time = slv.has_dc_system_time;
    out.transmission_delay = slv.transmission_delay;
    out.al_state = slv.al_state;
    out.error_flag = slv.error_flag;
    out.sync_count = slv.sync_count;
    out.sdo_count = slv.sdo_count;
    out.sii_nwords = slv.sii_nwords;
    out.group = slv.group;
    out.image = slv.image;
    out.order = slv.order;
    out.name = slv.name;

    return out;
}

static ethercat_sync_info_t gmi_ethercat_get_slave_sync(void *ctx,
        uint32_t master_index, uint16_t position, uint32_t sync_index)
{
    lcec_rt_context_t *rt = (lcec_rt_context_t *)ctx;
    ethercat_sync_info_t out = {0};
    ec_tool_slave_sync_t sync;
    ec_master_t *master;

    master = resolve_master(rt, master_index);
    if (!master)
        return out;

    sync.slave_position = position;
    sync.sync_index = sync_index;
    if (ecrt_tool_get_slave_sync(master, &sync) != 0)
        return out;

    out.slave_position = position;
    out.sync_index = sync_index;
    out.physical_start_address = sync.physical_start_address;
    out.default_size = sync.default_size;
    out.control_register = sync.control_register;
    out.enable = sync.enable;
    out.pdo_count = sync.pdo_count;
    return out;
}

static ethercat_pdo_info_t gmi_ethercat_get_slave_sync_pdo(void *ctx,
        uint32_t master_index, uint16_t position,
        uint32_t sync_index, uint32_t pdo_pos)
{
    lcec_rt_context_t *rt = (lcec_rt_context_t *)ctx;
    ethercat_pdo_info_t out = {0};
    static __thread ec_tool_slave_sync_pdo_t pdo;
    ec_master_t *master;

    master = resolve_master(rt, master_index);
    if (!master)
        return out;

    pdo.slave_position = position;
    pdo.sync_index = sync_index;
    pdo.pdo_pos = pdo_pos;
    if (ecrt_tool_get_slave_sync_pdo(master, &pdo) != 0)
        return out;

    out.slave_position = position;
    out.sync_index = sync_index;
    out.pdo_pos = pdo_pos;
    out.index = pdo.index;
    out.entry_count = pdo.entry_count;
    out.name = (const char *)pdo.name;
    return out;
}

static ethercat_pdo_entry_info_t gmi_ethercat_get_slave_sync_pdo_entry(void *ctx,
        uint32_t master_index, uint16_t position,
        uint32_t sync_index, uint32_t pdo_pos, uint32_t entry_pos)
{
    lcec_rt_context_t *rt = (lcec_rt_context_t *)ctx;
    ethercat_pdo_entry_info_t out = {0};
    static __thread ec_tool_slave_sync_pdo_entry_t entry;
    ec_master_t *master;

    master = resolve_master(rt, master_index);
    if (!master)
        return out;

    entry.slave_position = position;
    entry.sync_index = sync_index;
    entry.pdo_pos = pdo_pos;
    entry.entry_pos = entry_pos;
    if (ecrt_tool_get_slave_sync_pdo_entry(master, &entry) != 0)
        return out;

    out.slave_position = position;
    out.sync_index = sync_index;
    out.pdo_pos = pdo_pos;
    out.entry_pos = entry_pos;
    out.index = entry.index;
    out.subindex = entry.subindex;
    out.bit_length = entry.bit_length;
    out.name = (const char *)entry.name;
    return out;
}

static ethercat_domain_info_t gmi_ethercat_get_domain(void *ctx,
        uint32_t master_index, uint32_t index)
{
    lcec_rt_context_t *rt = (lcec_rt_context_t *)ctx;
    ethercat_domain_info_t out = {0};
    ec_tool_domain_t dom;
    ec_master_t *master;

    master = resolve_master(rt, master_index);
    if (!master)
        return out;

    dom.index = index;
    if (ecrt_tool_get_domain(master, &dom) != 0)
        return out;

    out.index = index;
    out.data_size = dom.data_size;
    out.logical_base_address = dom.logical_base_address;
    memcpy(out.working_counter, dom.working_counter, sizeof(out.working_counter));
    out.expected_working_counter = dom.expected_working_counter;
    out.fmmu_count = dom.fmmu_count;
    return out;
}

static ethercat_domain_fmmu_info_t gmi_ethercat_get_domain_fmmu(void *ctx,
        uint32_t master_index, uint32_t domain_index, uint32_t fmmu_index)
{
    lcec_rt_context_t *rt = (lcec_rt_context_t *)ctx;
    ethercat_domain_fmmu_info_t out = {0};
    ec_tool_domain_fmmu_t fmmu;
    ec_master_t *master;

    master = resolve_master(rt, master_index);
    if (!master)
        return out;

    fmmu.domain_index = domain_index;
    fmmu.fmmu_index = fmmu_index;
    if (ecrt_tool_get_domain_fmmu(master, &fmmu) != 0)
        return out;

    out.domain_index = domain_index;
    out.fmmu_index = fmmu_index;
    out.slave_config_alias = fmmu.slave_config_alias;
    out.slave_config_position = fmmu.slave_config_position;
    out.sync_index = fmmu.sync_index;
    out.dir = (uint8_t)fmmu.dir;
    out.logical_address = fmmu.logical_address;
    out.data_size = fmmu.data_size;
    return out;
}

static ethercat_get_domain_data_result_t gmi_ethercat_get_domain_data(void *ctx,
        uint32_t master_index, uint32_t domain_index)
{
    ethercat_get_domain_data_result_t out = {0};
    lcec_rt_context_t *rt = (lcec_rt_context_t *)ctx;
    ec_master_t *master;

    master = resolve_master(rt, master_index);
    if (!master)
        return out;

    /* TODO: implement domain data readback via ecrt_tool API */
    (void)domain_index;
    return out;
}

static bool gmi_ethercat_set_debug(void *ctx,
        uint32_t master_index, uint32_t level)
{
    lcec_rt_context_t *rt = (lcec_rt_context_t *)ctx;
    ec_master_t *master = resolve_master(rt, master_index);
    if (!master)
        return false;
    return ecrt_tool_set_debug(master, level) == 0;
}

static bool gmi_ethercat_rescan(void *ctx, uint32_t master_index)
{
    lcec_rt_context_t *rt = (lcec_rt_context_t *)ctx;
    ec_master_t *master = resolve_master(rt, master_index);
    if (!master)
        return false;
    return ecrt_tool_rescan(master) == 0;
}

static bool gmi_ethercat_set_slave_state(void *ctx,
        uint32_t master_index, uint16_t position,
        const ethercat_slave_state_request_t *state)
{
    lcec_rt_context_t *rt = (lcec_rt_context_t *)ctx;
    ec_tool_slave_state_t req;
    ec_master_t *master = resolve_master(rt, master_index);
    if (!master)
        return false;

    req.slave_position = position;
    req.al_state = state->al_state;
    return ecrt_tool_set_slave_state(master, &req) == 0;
}

static ethercat_slave_sdo_info_t gmi_ethercat_get_slave_sdo(void *ctx,
        uint32_t master_index, uint16_t position, uint16_t sdo_position)
{
    lcec_rt_context_t *rt = (lcec_rt_context_t *)ctx;
    ethercat_slave_sdo_info_t out = {0};
    static __thread ec_tool_slave_sdo_t sdo;
    ec_master_t *master = resolve_master(rt, master_index);
    if (!master)
        return out;

    sdo.slave_position = position;
    sdo.sdo_position = sdo_position;
    if (ecrt_tool_get_slave_sdo(master, &sdo) != 0)
        return out;

    out.slave_position = position;
    out.sdo_position = sdo_position;
    out.sdo_index = sdo.sdo_index;
    out.max_subindex = sdo.max_subindex;
    out.name = (const char *)sdo.name;
    return out;
}

static ethercat_sdo_entry_info_t gmi_ethercat_get_slave_sdo_entry(void *ctx,
        uint32_t master_index, uint16_t position,
        int32_t sdo_spec, uint8_t subindex)
{
    lcec_rt_context_t *rt = (lcec_rt_context_t *)ctx;
    ethercat_sdo_entry_info_t out = {0};
    static __thread ec_tool_slave_sdo_entry_t entry;
    ec_master_t *master = resolve_master(rt, master_index);
    if (!master)
        return out;

    entry.slave_position = position;
    entry.sdo_spec = sdo_spec;
    entry.sdo_entry_subindex = subindex;
    if (ecrt_tool_get_slave_sdo_entry(master, &entry) != 0)
        return out;

    out.slave_position = position;
    out.sdo_spec = sdo_spec;
    out.sdo_entry_subindex = subindex;
    out.data_type = entry.data_type;
    out.bit_length = entry.bit_length;
    for (int i = 0; i < ETHERCAT_SDO_ENTRY_ACCESS_COUNT; i++) {
        out.read_access[i] = entry.read_access[i];
        out.write_access[i] = entry.write_access[i];
    }
    out.description = (const char *)entry.description;
    return out;
}

static ethercat_sdo_upload_result_t gmi_ethercat_sdo_upload(void *ctx,
        uint32_t master_index, uint16_t position,
        uint16_t sdo_index, uint8_t subindex, uint32_t size)
{
    lcec_rt_context_t *rt = (lcec_rt_context_t *)ctx;
    ethercat_sdo_upload_result_t out = {0};
    ec_tool_slave_sdo_upload_t req;
    ec_master_t *master = resolve_master(rt, master_index);
    uint32_t target_size;
    uint8_t *target;

    if (!master)
        return out;

    target_size = size ? size : ETHERCAT_MAX_SDO_DATA_SIZE;
    target = malloc(target_size);
    if (!target)
        return out;

    req.slave_position = position;
    req.sdo_index = sdo_index;
    req.sdo_entry_subindex = subindex;
    req.target_size = target_size;
    req.target = target;

    if (ecrt_tool_sdo_upload(master, &req) == 0) {
        out.data = target;
        out.data_len = req.data_size;
        out.abort_code = req.abort_code;
    } else {
        out.abort_code = req.abort_code;
        free(target);
    }
    return out;
}

static ethercat_sdo_download_result_t gmi_ethercat_sdo_download(void *ctx,
        uint32_t master_index, uint16_t position,
        uint16_t sdo_index, uint8_t subindex,
        const ethercat_sdo_download_request_t *req_in)
{
    lcec_rt_context_t *rt = (lcec_rt_context_t *)ctx;
    ethercat_sdo_download_result_t out = {0};
    ec_tool_slave_sdo_download_t req;
    ec_master_t *master = resolve_master(rt, master_index);
    if (!master)
        return out;

    req.slave_position = position;
    req.sdo_index = sdo_index;
    req.sdo_entry_subindex = subindex;
    req.complete_access = req_in->complete_access;
    req.data_size = (uint32_t)req_in->data_len;
    req.data = (uint8_t *)req_in->data;

    ecrt_tool_sdo_download(master, &req);
    out.abort_code = req.abort_code;
    return out;
}

static ethercat_sii_data_t gmi_ethercat_sii_read(void *ctx,
        uint32_t master_index, uint16_t position,
        uint16_t offset, uint32_t nwords)
{
    lcec_rt_context_t *rt = (lcec_rt_context_t *)ctx;
    ethercat_sii_data_t out = {0};
    ec_tool_slave_sii_t req;
    ec_master_t *master = resolve_master(rt, master_index);
    uint16_t *words;

    if (!master || !nwords)
        return out;

    words = malloc(nwords * 2);
    if (!words)
        return out;

    req.slave_position = position;
    req.offset = offset;
    req.nwords = nwords;
    req.words = words;

    if (ecrt_tool_sii_read(master, &req) == 0) {
        out.offset = offset;
        out.nwords = nwords;
        out.words = (uint8_t *)words;
        out.words_len = nwords * 2;
    } else {
        free(words);
    }
    return out;
}

static bool gmi_ethercat_sii_write(void *ctx,
        uint32_t master_index, uint16_t position,
        const ethercat_sii_data_t *sii)
{
    lcec_rt_context_t *rt = (lcec_rt_context_t *)ctx;
    ec_tool_slave_sii_t req;
    ec_master_t *master = resolve_master(rt, master_index);
    if (!master)
        return false;

    req.slave_position = position;
    req.offset = sii->offset;
    req.nwords = sii->nwords;
    req.words = (uint16_t *)sii->words;
    return ecrt_tool_sii_write(master, &req) == 0;
}

static ethercat_reg_read_result_t gmi_ethercat_reg_read(void *ctx,
        uint32_t master_index, uint16_t position,
        uint16_t address, uint32_t size)
{
    lcec_rt_context_t *rt = (lcec_rt_context_t *)ctx;
    ethercat_reg_read_result_t out = {0};
    ec_tool_slave_reg_t req;
    ec_master_t *master = resolve_master(rt, master_index);
    uint8_t *data;

    if (!master || !size)
        return out;

    data = malloc(size);
    if (!data)
        return out;

    req.slave_position = position;
    req.emergency = 0;
    req.address = address;
    req.size = size;
    req.data = data;

    if (ecrt_tool_reg_read(master, &req) == 0) {
        out.data = data;
        out.data_len = size;
    } else {
        free(data);
    }
    return out;
}

static bool gmi_ethercat_reg_write(void *ctx,
        uint32_t master_index, uint16_t position,
        uint16_t address, const ethercat_reg_write_request_t *req_in)
{
    lcec_rt_context_t *rt = (lcec_rt_context_t *)ctx;
    ec_tool_slave_reg_t req;
    ec_master_t *master = resolve_master(rt, master_index);
    if (!master)
        return false;

    req.slave_position = position;
    req.emergency = req_in->emergency;
    req.address = address;
    req.size = (uint32_t)req_in->data_len;
    req.data = (uint8_t *)req_in->data;
    return ecrt_tool_reg_write(master, &req) == 0;
}

static ethercat_foe_read_result_t gmi_ethercat_foe_read(void *ctx,
        uint32_t master_index, uint16_t position, const char *file_name)
{
    lcec_rt_context_t *rt = (lcec_rt_context_t *)ctx;
    ethercat_foe_read_result_t out = {0};
    ec_tool_slave_foe_t req;
    ec_master_t *master = resolve_master(rt, master_index);
    uint8_t *buffer;
    uint32_t buf_size = 65536;

    if (!master)
        return out;

    buffer = malloc(buf_size);
    if (!buffer)
        return out;

    memset(&req, 0, sizeof(req));
    req.slave_position = position;
    if (file_name)
        strncpy(req.file_name, file_name, sizeof(req.file_name) - 1);
    req.buffer_size = buf_size;
    req.buffer = buffer;

    int ret = ecrt_tool_foe_read(master, &req);
    out.result = req.result;
    out.error_code = req.error_code;

    if (ret == 0) {
        out.data = buffer;
        out.data_len = req.data_size;
    } else {
        free(buffer);
    }
    return out;
}

static ethercat_foe_write_result_t gmi_ethercat_foe_write(void *ctx,
        uint32_t master_index, uint16_t position,
        const char *file_name, const ethercat_foe_write_request_t *req_in)
{
    lcec_rt_context_t *rt = (lcec_rt_context_t *)ctx;
    ethercat_foe_write_result_t out = {0};
    ec_tool_slave_foe_t req;
    ec_master_t *master = resolve_master(rt, master_index);
    if (!master)
        return out;

    memset(&req, 0, sizeof(req));
    req.slave_position = position;
    if (file_name)
        strncpy(req.file_name, file_name, sizeof(req.file_name) - 1);
    req.buffer_size = (uint32_t)req_in->data_len;
    req.buffer = (uint8_t *)req_in->data;

    ecrt_tool_foe_write(master, &req);
    out.result = req.result;
    out.error_code = req.error_code;
    return out;
}

static ethercat_soe_read_result_t gmi_ethercat_soe_read(void *ctx,
        uint32_t master_index, uint16_t position,
        uint8_t drive_no, uint16_t idn, uint32_t mem_size)
{
    lcec_rt_context_t *rt = (lcec_rt_context_t *)ctx;
    ethercat_soe_read_result_t out = {0};
    ec_tool_slave_soe_read_t req;
    ec_master_t *master = resolve_master(rt, master_index);
    uint32_t buf_size;
    uint8_t *data;

    if (!master)
        return out;

    buf_size = mem_size ? mem_size : ETHERCAT_MAX_IDN_DATA_SIZE;
    data = malloc(buf_size);
    if (!data)
        return out;

    req.slave_position = position;
    req.drive_no = drive_no;
    req.idn = idn;
    req.mem_size = buf_size;
    req.data = data;

    if (ecrt_tool_soe_read(master, &req) == 0) {
        out.data = data;
        out.data_len = req.data_size;
        out.error_code = req.error_code;
    } else {
        out.error_code = req.error_code;
        free(data);
    }
    return out;
}

static ethercat_soe_write_result_t gmi_ethercat_soe_write(void *ctx,
        uint32_t master_index, uint16_t position,
        uint8_t drive_no, uint16_t idn,
        const ethercat_soe_write_request_t *req_in)
{
    lcec_rt_context_t *rt = (lcec_rt_context_t *)ctx;
    ethercat_soe_write_result_t out = {0};
    ec_tool_slave_soe_write_t req;
    ec_master_t *master = resolve_master(rt, master_index);
    if (!master)
        return out;

    req.slave_position = position;
    req.drive_no = drive_no;
    req.idn = idn;
    req.data_size = (uint32_t)req_in->data_len;
    req.data = (uint8_t *)req_in->data;

    ecrt_tool_soe_write(master, &req);
    out.error_code = req.error_code;
    return out;
}

static ethercat_config_info_t gmi_ethercat_get_config(void *ctx,
        uint32_t master_index, uint32_t config_index)
{
    lcec_rt_context_t *rt = (lcec_rt_context_t *)ctx;
    ethercat_config_info_t out = {0};
    ec_tool_config_t cfg;
    ec_master_t *master = resolve_master(rt, master_index);
    if (!master)
        return out;

    cfg.config_index = config_index;
    if (ecrt_tool_get_config(master, &cfg) != 0)
        return out;

    out.config_index = config_index;
    out.alias = cfg.alias;
    out.position = cfg.position;
    out.vendor_id = cfg.vendor_id;
    out.product_code = cfg.product_code;
    for (int i = 0; i < ETHERCAT_MAX_SYNC_MANAGERS; i++) {
        out.syncs[i].dir = (uint8_t)cfg.syncs[i].dir;
        out.syncs[i].watchdog_mode = (uint8_t)cfg.syncs[i].watchdog_mode;
        out.syncs[i].pdo_count = cfg.syncs[i].pdo_count;
        out.syncs[i].config_this = cfg.syncs[i].config_this;
    }
    out.watchdog_divider = cfg.watchdog_divider;
    out.watchdog_intervals = cfg.watchdog_intervals;
    out.sdo_count = cfg.sdo_count;
    out.idn_count = cfg.idn_count;
    out.flag_count = cfg.flag_count;
    out.slave_position = cfg.slave_position;
    out.dc_assign_activate = cfg.dc_assign_activate;
    for (int i = 0; i < ETHERCAT_SYNC_SIGNAL_COUNT; i++) {
        out.dc_sync[i].cycle_time = cfg.dc_sync[i].cycle_time;
        out.dc_sync[i].shift_time = cfg.dc_sync[i].shift_time;
    }
    return out;
}

static ethercat_config_pdo_info_t gmi_ethercat_get_config_pdo(void *ctx,
        uint32_t master_index, uint32_t config_index,
        uint8_t sync_index, uint16_t pdo_pos)
{
    lcec_rt_context_t *rt = (lcec_rt_context_t *)ctx;
    ethercat_config_pdo_info_t out = {0};
    static __thread ec_tool_config_pdo_t pdo;
    ec_master_t *master = resolve_master(rt, master_index);
    if (!master)
        return out;

    pdo.config_index = config_index;
    pdo.sync_index = sync_index;
    pdo.pdo_pos = pdo_pos;
    if (ecrt_tool_get_config_pdo(master, &pdo) != 0)
        return out;

    out.config_index = config_index;
    out.sync_index = sync_index;
    out.pdo_pos = pdo_pos;
    out.index = pdo.index;
    out.entry_count = pdo.entry_count;
    out.name = (const char *)pdo.name;
    return out;
}

static ethercat_config_pdo_entry_info_t gmi_ethercat_get_config_pdo_entry(void *ctx,
        uint32_t master_index, uint32_t config_index,
        uint8_t sync_index, uint16_t pdo_pos, uint8_t entry_pos)
{
    lcec_rt_context_t *rt = (lcec_rt_context_t *)ctx;
    ethercat_config_pdo_entry_info_t out = {0};
    static __thread ec_tool_config_pdo_entry_t entry;
    ec_master_t *master = resolve_master(rt, master_index);
    if (!master)
        return out;

    entry.config_index = config_index;
    entry.sync_index = sync_index;
    entry.pdo_pos = pdo_pos;
    entry.entry_pos = entry_pos;
    if (ecrt_tool_get_config_pdo_entry(master, &entry) != 0)
        return out;

    out.config_index = config_index;
    out.sync_index = sync_index;
    out.pdo_pos = pdo_pos;
    out.entry_pos = entry_pos;
    out.index = entry.index;
    out.subindex = entry.subindex;
    out.bit_length = entry.bit_length;
    out.name = (const char *)entry.name;
    return out;
}

static ethercat_config_sdo_info_t gmi_ethercat_get_config_sdo(void *ctx,
        uint32_t master_index, uint32_t config_index, uint32_t sdo_pos)
{
    lcec_rt_context_t *rt = (lcec_rt_context_t *)ctx;
    ethercat_config_sdo_info_t out = {0};
    static __thread ec_tool_config_sdo_t sdo;
    ec_master_t *master = resolve_master(rt, master_index);
    if (!master)
        return out;

    sdo.config_index = config_index;
    sdo.sdo_pos = sdo_pos;
    if (ecrt_tool_get_config_sdo(master, &sdo) != 0)
        return out;

    out.config_index = config_index;
    out.sdo_pos = sdo_pos;
    out.index = sdo.index;
    out.subindex = sdo.subindex;
    out.size = sdo.size;
    if (sdo.size > 0) {
        out.data = malloc(sdo.size);
        if (out.data)
            memcpy(out.data, sdo.data, sdo.size);
        out.data_len = sdo.size;
    }
    out.complete_access = sdo.complete_access;
    return out;
}

static ethercat_config_idn_info_t gmi_ethercat_get_config_idn(void *ctx,
        uint32_t master_index, uint32_t config_index, uint32_t idn_pos)
{
    lcec_rt_context_t *rt = (lcec_rt_context_t *)ctx;
    ethercat_config_idn_info_t out = {0};
    static __thread ec_tool_config_idn_t idn;
    ec_master_t *master = resolve_master(rt, master_index);
    if (!master)
        return out;

    idn.config_index = config_index;
    idn.idn_pos = idn_pos;
    if (ecrt_tool_get_config_idn(master, &idn) != 0)
        return out;

    out.config_index = config_index;
    out.idn_pos = idn_pos;
    out.drive_no = idn.drive_no;
    out.idn = idn.idn;
    out.state = (uint8_t)idn.state;
    out.size = idn.size;
    if (idn.size > 0) {
        out.data = malloc(idn.size);
        if (out.data)
            memcpy(out.data, idn.data, idn.size);
        out.data_len = idn.size;
    }
    return out;
}

static ethercat_config_flag_info_t gmi_ethercat_get_config_flag(void *ctx,
        uint32_t master_index, uint32_t config_index, uint32_t flag_pos)
{
    lcec_rt_context_t *rt = (lcec_rt_context_t *)ctx;
    ethercat_config_flag_info_t out = {0};
    static __thread ec_tool_config_flag_t flag;
    ec_master_t *master = resolve_master(rt, master_index);
    if (!master)
        return out;

    flag.config_index = config_index;
    flag.flag_pos = flag_pos;
    if (ecrt_tool_get_config_flag(master, &flag) != 0)
        return out;

    out.config_index = config_index;
    out.flag_pos = flag_pos;
    out.key = flag.key;
    out.value = flag.value;
    return out;
}

static ethercat_eoe_handler_info_t gmi_ethercat_get_eoe_handler(void *ctx,
        uint32_t master_index, uint16_t eoe_index)
{
    ethercat_eoe_handler_info_t out = {0};
#ifdef EC_EOE
    lcec_rt_context_t *rt = (lcec_rt_context_t *)ctx;
    static __thread ec_tool_eoe_handler_t eoe;
    ec_master_t *master = resolve_master(rt, master_index);
    if (!master)
        return out;

    eoe.eoe_index = eoe_index;
    if (ecrt_tool_get_eoe_handler(master, &eoe) != 0)
        return out;

    out.eoe_index = eoe_index;
    out.name = eoe.name;
    out.slave_position = eoe.slave_position;
    out.open = eoe.open;
    out.rx_bytes = eoe.rx_bytes;
    out.rx_rate = eoe.rx_rate;
    out.tx_bytes = eoe.tx_bytes;
    out.tx_rate = eoe.tx_rate;
    out.tx_queued_frames = eoe.tx_queued_frames;
    out.tx_queue_size = eoe.tx_queue_size;
#else
    (void)ctx; (void)master_index; (void)eoe_index;
#endif
    return out;
}

static ethercat_eoe_ip_result_t gmi_ethercat_set_eoe_ip(void *ctx,
        uint32_t master_index, uint16_t position, const ethercat_eoe_ip_request_t *req)
{
    ethercat_eoe_ip_result_t out = {0};
#ifdef EC_EOE
    lcec_rt_context_t *rt = (lcec_rt_context_t *)ctx;
    ec_tool_eoe_ip_t io = {0};
    ec_master_t *master = resolve_master(rt, master_index);
    if (!master)
        return out;

    io.slave_position = position;

    if (req->mac_address) {
        /* Parse MAC: "aa:bb:cc:dd:ee:ff" or "aa-bb-cc-dd-ee-ff" */
        unsigned int m[6];
        if (sscanf(req->mac_address, "%x:%x:%x:%x:%x:%x",
                &m[0], &m[1], &m[2], &m[3], &m[4], &m[5]) == 6 ||
            sscanf(req->mac_address, "%x-%x-%x-%x-%x-%x",
                &m[0], &m[1], &m[2], &m[3], &m[4], &m[5]) == 6) {
            for (int i = 0; i < 6; i++)
                io.mac_address[i] = (unsigned char)m[i];
            io.mac_address_included = 1;
        }
    }

    if (req->ip_address) {
        struct in_addr addr;
        if (inet_pton(AF_INET, req->ip_address, &addr) == 1) {
            memcpy(&io.ip_address, &addr, 4);
            io.ip_address_included = 1;
        }
    }

    if (req->subnet_mask) {
        struct in_addr addr;
        if (inet_pton(AF_INET, req->subnet_mask, &addr) == 1) {
            memcpy(&io.subnet_mask, &addr, 4);
            io.subnet_mask_included = 1;
        }
    }

    if (req->gateway) {
        struct in_addr addr;
        if (inet_pton(AF_INET, req->gateway, &addr) == 1) {
            memcpy(&io.gateway, &addr, 4);
            io.gateway_included = 1;
        }
    }

    if (req->dns) {
        struct in_addr addr;
        if (inet_pton(AF_INET, req->dns, &addr) == 1) {
            memcpy(&io.dns, &addr, 4);
            io.dns_included = 1;
        }
    }

    if (req->hostname) {
        size_t len = strlen(req->hostname);
        if (len >= EC_TOOL_MAX_HOSTNAME_SIZE)
            len = EC_TOOL_MAX_HOSTNAME_SIZE - 1;
        memcpy(io.name, req->hostname, len);
        io.name[len] = '\0';
        io.name_included = 1;
    }

    if (ecrt_tool_set_eoe_ip(master, &io) != 0)
        return out;

    out.result = io.result;
#else
    (void)ctx; (void)master_index; (void)position; (void)req;
#endif
    return out;
}

/* ─── Public interface ────────────────────────────────────────────────────── */

ethercat_callbacks_t gmi_ethercat_callbacks(void)
{
    ethercat_callbacks_t cb = GMI_ETHERCAT_CALLBACKS;
    return cb;
}
