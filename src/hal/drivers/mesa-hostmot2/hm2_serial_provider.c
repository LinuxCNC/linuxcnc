// hm2_serial_provider.c — GMI hm2_serial API provider shims.
//
// These thin wrappers adapt the hostmot2 internal hm2_bspi_*, hm2_uart_*,
// hm2_pktuart_* functions (which take no ctx) to the GMI callback interface
// (which passes ctx as the first argument).
//
// The callbacks struct is populated by hm2_serial_provider_init() and
// registered with the GMI API registry by hostmot2.c in New().

#include "rtapi_stdint.h"
#include "hm2_serial_api.h"
#include "hostmot2-serial.h"
#include <string.h>

// ---------------------------------------------------------------------------
// PktUART shims
// ---------------------------------------------------------------------------

static int32_t shim_pktuart_config(void *ctx, const char *name,
    const hm2_serial_pkt_uart_config_t *rxcfg,
    const hm2_serial_pkt_uart_config_t *txcfg, int32_t queue)
{
    (void)ctx;
    // Convert GMI types to internal types (layout-compatible for first 5 fields)
    hm2_pktuart_config_t rx = {0};
    hm2_pktuart_config_t tx = {0};
    memcpy(&rx, rxcfg, sizeof(*rxcfg));
    memcpy(&tx, txcfg, sizeof(*txcfg));
    return hm2_pktuart_config(name, &rx, &tx, queue);
}

static int32_t shim_pktuart_send(void *ctx, const char *name,
    void *data, uint8_t num_frames, void *frame_sizes)
{
    (void)ctx;
    return hm2_pktuart_send(name, (unsigned char *)data,
                            (uint8_t *)&num_frames, (uint16_t *)frame_sizes);
}

static int32_t shim_pktuart_read(void *ctx, const char *name,
    void *data, uint8_t *num_frames, void *max_frame_length, void *frame_sizes)
{
    (void)ctx;
    return hm2_pktuart_read(name, (unsigned char *)data,
                            (uint8_t *)num_frames,
                            (uint16_t *)max_frame_length,
                            (uint16_t *)frame_sizes);
}

static int32_t shim_pktuart_queue_get_frame_sizes(void *ctx, const char *name,
    void *fsizes)
{
    (void)ctx;
    return hm2_pktuart_queue_get_frame_sizes(name, (uint32_t *)fsizes);
}

static int32_t shim_pktuart_queue_read_data(void *ctx, const char *name,
    void *data, int32_t bytes)
{
    (void)ctx;
    return hm2_pktuart_queue_read_data(name, (uint32_t *)data, bytes);
}

static void shim_pktuart_reset(void *ctx, const char *name)
{
    (void)ctx;
    hm2_pktuart_reset(name);
}

static void shim_pktuart_queue_reset(void *ctx, const char *name)
{
    (void)ctx;
    hm2_pktuart_queue_reset(name);
}

static int32_t shim_pktuart_get_clock(void *ctx, const char *name)
{
    (void)ctx;
    return hm2_pktuart_get_clock(name);
}

static int32_t shim_pktuart_get_version(void *ctx, const char *name)
{
    (void)ctx;
    return hm2_pktuart_get_version(name);
}

static uint32_t shim_pktuart_get_rx_status(void *ctx, const char *name)
{
    (void)ctx;
    return hm2_pktuart_get_rx_status(name);
}

static uint32_t shim_pktuart_get_tx_status(void *ctx, const char *name)
{
    (void)ctx;
    return hm2_pktuart_get_tx_status(name);
}

// ---------------------------------------------------------------------------
// UART shims
// ---------------------------------------------------------------------------

static int32_t shim_uart_setup(void *ctx, const char *name,
    int32_t bitrate, int32_t tx_mode, int32_t rx_mode)
{
    (void)ctx;
    return hm2_uart_setup((char *)name, bitrate, tx_mode, rx_mode);
}

static int32_t shim_uart_send(void *ctx, const char *name,
    void *data, int32_t count)
{
    (void)ctx;
    return hm2_uart_send((char *)name, (unsigned char *)data, count);
}

static int32_t shim_uart_read(void *ctx, const char *name, void *data)
{
    (void)ctx;
    return hm2_uart_read((char *)name, (unsigned char *)data);
}

// ---------------------------------------------------------------------------
// BSPI shims
// ---------------------------------------------------------------------------

static int32_t shim_bspi_setup_chan(void *ctx, const char *name,
    int32_t chan, int32_t cs, int32_t bits, double mhz,
    int32_t delay, int32_t cpol, int32_t cpha,
    int32_t noclear, int32_t noecho, int32_t samplelate)
{
    (void)ctx;
    return hm2_bspi_setup_chan((char *)name, chan, cs, bits, mhz,
                               delay, cpol, cpha, noclear, noecho, samplelate);
}

static int32_t shim_bspi_set_read_function(void *ctx, const char *name,
    void *func_ptr, void *subdata)
{
    (void)ctx;
    return hm2_bspi_set_read_function((char *)name,
                                      (int (*)(void *))func_ptr, subdata);
}

static int32_t shim_bspi_set_write_function(void *ctx, const char *name,
    void *func_ptr, void *subdata)
{
    (void)ctx;
    return hm2_bspi_set_write_function((char *)name,
                                       (int (*)(void *))func_ptr, subdata);
}

static int32_t shim_bspi_write_chan(void *ctx, const char *name,
    int32_t chan, uint32_t val)
{
    (void)ctx;
    return hm2_bspi_write_chan((char *)name, chan, val);
}

static int32_t shim_bspi_allocate_tram(void *ctx, const char *name)
{
    (void)ctx;
    return hm2_allocate_bspi_tram((char *)name);
}

static int32_t shim_bspi_tram_add_frame(void *ctx, const char *name,
    int32_t chan, void *wbuff, void *rbuff)
{
    (void)ctx;
    return hm2_tram_add_bspi_frame((char *)name, chan,
                                   (uint32_t **)wbuff, (uint32_t **)rbuff);
}

static int32_t shim_bspi_clear_fifo(void *ctx, const char *name)
{
    (void)ctx;
    return hm2_bspi_clear_fifo((char *)name);
}

// ---------------------------------------------------------------------------
// Provider initialization
// ---------------------------------------------------------------------------

void hm2_serial_provider_init(hm2_serial_callbacks_t *cb)
{
    cb->ctx = NULL;  // shims don't use ctx
    cb->pktuart_config = shim_pktuart_config;
    cb->pktuart_send = shim_pktuart_send;
    cb->pktuart_read = shim_pktuart_read;
    cb->pktuart_queue_get_frame_sizes = shim_pktuart_queue_get_frame_sizes;
    cb->pktuart_queue_read_data = shim_pktuart_queue_read_data;
    cb->pktuart_reset = shim_pktuart_reset;
    cb->pktuart_queue_reset = shim_pktuart_queue_reset;
    cb->pktuart_get_clock = shim_pktuart_get_clock;
    cb->pktuart_get_version = shim_pktuart_get_version;
    cb->pktuart_get_rx_status = shim_pktuart_get_rx_status;
    cb->pktuart_get_tx_status = shim_pktuart_get_tx_status;
    cb->uart_setup = shim_uart_setup;
    cb->uart_send = shim_uart_send;
    cb->uart_read = shim_uart_read;
    cb->bspi_setup_chan = shim_bspi_setup_chan;
    cb->bspi_set_read_function = shim_bspi_set_read_function;
    cb->bspi_set_write_function = shim_bspi_set_write_function;
    cb->bspi_write_chan = shim_bspi_write_chan;
    cb->bspi_allocate_tram = shim_bspi_allocate_tram;
    cb->bspi_tram_add_frame = shim_bspi_tram_add_frame;
    cb->bspi_clear_fifo = shim_bspi_clear_fifo;
}
