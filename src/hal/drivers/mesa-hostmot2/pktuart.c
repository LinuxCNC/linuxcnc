//
//    Copyright (C) 2011 Andy Pugh, 2016 Boris Skegin
//
//    This program is free software; you can redistribute it and/or modify
//    it under the terms of the GNU General Public License as published by
//    the Free Software Foundation; either version 2 of the License, or
//    (at your option) any later version.
//
//    This program is distributed in the hope that it will be useful,
//    but WITHOUT ANY WARRANTY; without even the implied warranty of
//    MERinstTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//    GNU General Public License for more details.
//
//    You should have received a copy of the GNU General Public License
//    along with this program; if not, write to the Free Software
//    Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301 USA
//

#include <rtapi_slab.h>
#include "rtapi.h"
#include "rtapi_string.h"
#include "rtapi_math.h"
#include "hal.h"
#include "hostmot2.h"
#include "hostmot2-serial.h"


#define MAX_TX_FRAMES     (16) // Send counts are written to 16 deep FIFO, burst mode


// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
//
// hostmot2 interface functions
//
// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

//
// Parse a PktUART instance
// All this function actually does is allocate memory and give the uart modules
// names.
//
int hm2_pktuart_parse_md(hostmot2_t *hm2, int md_index)
{
	int i, r = -EINVAL;
	hm2_module_descriptor_t *md = &hm2->md[md_index];
	static int last_gtag = -1;

	//
	// some standard sanity checks
	//
	//The PktUART declares a TX and RX module separately
	//And Rx can currently be v1 or v2
	if(md->gtag == HM2_GTAG_PKTUART_TX
		&& !hm2_md_is_consistent(hm2,md_index, 0, 4, 4, 0x000F)
		&& !hm2_md_is_consistent(hm2,md_index, 1, 4, 4, 0x000F)
		&& !hm2_md_is_consistent(hm2,md_index, 2, 4, 4, 0x000F)
		&& !hm2_md_is_consistent(hm2,md_index, 3, 4, 4, 0x000F))  {
		HM2_ERR("Unsupported or inconsistent PktUART TX module (version %i) "
				"not loading driver\n", md->version);
		return -EINVAL;
	}
	if(md->gtag == HM2_GTAG_PKTUART_RX
		&& !hm2_md_is_consistent(hm2, md_index, 0, 4, 4, 0x000F)
		&& !hm2_md_is_consistent(hm2, md_index, 1, 4, 4, 0x000F)
		&& !hm2_md_is_consistent(hm2, md_index, 2, 4, 4, 0x000F)
		&& !hm2_md_is_consistent(hm2, md_index, 3, 4, 4, 0x000F)) {
		HM2_ERR("Unsupported or inconsistent PktUART RX module (version %i) "
				"not loading driver\n", md->version);
		return -EINVAL;
	}

	if(hm2->pktuart.num_instances > 1 && last_gtag == md->gtag) {
		HM2_ERR("found duplicate Module Descriptor for %s (inconsistent "
				"firmware), not loading driver %i %i\n",
				hm2_get_general_function_name(md->gtag), md->gtag, last_gtag);
		return -EINVAL;
	}
	last_gtag = md->gtag;

	if(hm2->config.num_pktuarts > md->instances) {
		HM2_ERR("config defines %d pktuarts, but only %d are available, "
				"not loading driver\n",
				hm2->config.num_pktuarts,
				md->instances);
		return -EINVAL;
	}

	if(hm2->config.num_pktuarts == 0) {
		return 0;
	}

	//
	// looks good, start, or continue, initializing
	//
	if(hm2->pktuart.num_instances == 0) {
		if(hm2->config.num_pktuarts == -1) {
			hm2->pktuart.num_instances = md->instances;
		} else {
			hm2->pktuart.num_instances = hm2->config.num_pktuarts;
		}

		hm2->pktuart.instance = (hm2_pktuart_instance_t *)hal_malloc(hm2->pktuart.num_instances
															* sizeof(hm2_pktuart_instance_t));
		if(hm2->pktuart.instance == NULL) {
			HM2_ERR("out of memory!\n");
			r = -ENOMEM;
			goto fail0;
		}
	}

	// Register automatic updating of the Rx and Tx mode (status) registers
	if(md->gtag == HM2_GTAG_PKTUART_RX) {
		hm2->pktuart.rx_version = md->version;
		r = hm2_register_tram_read_region(hm2, md->base_address + 3 * md->register_stride,
						(hm2->pktuart.num_instances * sizeof(rtapi_u32)), &hm2->pktuart.rx_status_reg);
		if(r < 0) {
			HM2_ERR("error registering tram read region for PktUART Rx status(%d)\n", r);
			goto fail0;
		}
	} else if(md->gtag == HM2_GTAG_PKTUART_TX) {
		hm2->pktuart.tx_version = md->version;
		r = hm2_register_tram_read_region(hm2, md->base_address + 3 * md->register_stride,
						(hm2->pktuart.num_instances * sizeof(rtapi_u32)), &hm2->pktuart.tx_status_reg);
		if(r < 0) {
			HM2_ERR("error registering tram read region for PktUART Tx status(%d)\n", r);
			goto fail0;
		}
	}

	for(i = 0 ; i < hm2->pktuart.num_instances ; i++) {
		hm2_pktuart_instance_t *inst = &hm2->pktuart.instance[i];
		// For the time being we assume that all PktUARTS come on pairs
		if(inst->clock_freq == 0) {
			inst->clock_freq = md->clock_freq;
			r = rtapi_snprintf(inst->name, sizeof(inst->name), "%s.pktuart.%01d", hm2->llio->name, i);
			HM2_PRINT("created PktUART Interface function %s.\n", inst->name);
		}
		if(md->gtag == HM2_GTAG_PKTUART_TX) {
			inst->tx_addr = md->base_address + i * md->instance_stride;
			inst->tx_fifo_count_addr = md->base_address + md->register_stride + i * md->instance_stride;
			inst->tx_bitrate_addr = md->base_address + 2 * md->register_stride + i * md->instance_stride;
			inst->tx_mode_addr = md->base_address + 3 * md->register_stride +i * md->instance_stride;
		}
		else if(md->gtag == HM2_GTAG_PKTUART_RX) {
			inst->rx_addr = md->base_address + i * md->instance_stride;
			inst->rx_fifo_count_addr = md->base_address + md->register_stride + i * md->instance_stride;
			inst->rx_bitrate_addr = md->base_address + 2 * md->register_stride + i * md->instance_stride;
			inst->rx_mode_addr = md->base_address + 3 * md->register_stride +i * md->instance_stride;
		}
		else{
			HM2_ERR("hm2_pktuart_parse_md(): md->gtag changed? Memory corruption or wrong function call.\n");
			r = -ENODEV;
			goto fail0;
		}
	}

	return hm2->pktuart.num_instances;
fail0:
	return r;
}

//
// Human readable info
//
void hm2_pktuart_print_module(hostmot2_t *hm2)
{
	int i;
	HM2_PRINT("PktUART: %d\n", hm2->pktuart.num_instances);
	if(hm2->pktuart.num_instances <= 0)
		return;
	HM2_PRINT("	version: %d\n", hm2->pktuart.version);
	HM2_PRINT("	channel configurations\n");
	for(i = 0; i < hm2->pktuart.num_instances; i ++) {
		HM2_PRINT("	clock_frequency: %d Hz (%s MHz)\n",
					hm2->pktuart.instance[i].clock_freq,
					hm2_hz_to_mhz(hm2->pktuart.instance[i].clock_freq));
		HM2_PRINT("	instance %d:\n", i);
		HM2_PRINT("	HAL name = %s\n", hm2->pktuart.instance[i].name);
	}
}

// The following standard Hostmot2 functions are not currently used by pktuart.

void hm2_pktuart_cleanup(hostmot2_t *hm2)
{
	(void)hm2;
}

void hm2_pktuart_write(hostmot2_t *hm2)
{
	(void)hm2;
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
//
// Driver interface functions
//
// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

// Warn when we know we're gonna crash...
static void llio_noqueue_warn(const hostmot2_t *hm2)
{
	static int warned = 0;
	if(warned)
		return;
	if(!hm2->llio->queue_write)
		HM2_ERR("Hostmot2 low-level driver is missing the queue_write() function. PktUART will crash.\n");
	if(!hm2->llio->queue_read)
		HM2_ERR("Hostmot2 low-level driver is missing the queue_read() function. PktUART will crash.\n");
	warned = 1;
}

//
// The hm2_pktuart_setup_tx() function is DEPRECATED
// Instead use hm2_pktuart_config().
//
int hm2_pktuart_setup_tx(const char *name, unsigned int bitrate, unsigned int parity, int frame_delay, bool drive_enable, bool drive_auto, int enable_delay)
{
	hm2_pktuart_config_t cfg = {
		.baudrate   = bitrate,
		.filterrate = 0,
		.drivedelay = enable_delay,
		.ifdelay    = frame_delay,
		.flags      = 0
	};
	if(parity != 0)  cfg.flags |= HM2_PKTUART_CONFIG_PARITYEN;
	if(parity == 1)  cfg.flags |= HM2_PKTUART_CONFIG_PARITYODD;
	if(drive_enable) cfg.flags |= HM2_PKTUART_CONFIG_DRIVEEN;
	if(drive_auto)   cfg.flags |= HM2_PKTUART_CONFIG_DRIVEAUTO;
	return hm2_pktuart_config(name, NULL, &cfg, 0);	// Send immediately
}
EXPORT_SYMBOL_GPL(hm2_pktuart_setup_tx);

//
// The hm2_pktuart_setup_rx() function is DEPRECATED
// Instead use hm2_pktuart_config().
//
int hm2_pktuart_setup_rx(const char *name, unsigned int bitrate, unsigned int filter_hz, unsigned int parity, int frame_delay, bool rx_enable, bool rx_mask)
{
	hm2_pktuart_config_t cfg = {
		.baudrate   = bitrate,
		.filterrate = filter_hz,
		.drivedelay = 0,
		.ifdelay    = frame_delay,
		.flags      = 0
	};
	if(parity != 0)  cfg.flags |= HM2_PKTUART_CONFIG_PARITYEN;
	if(parity == 1)  cfg.flags |= HM2_PKTUART_CONFIG_PARITYODD;
	if(rx_enable) cfg.flags |= HM2_PKTUART_CONFIG_RXEN;
	if(rx_mask)   cfg.flags |= HM2_PKTUART_CONFIG_RXMASKEN;
	return hm2_pktuart_config(name, &cfg, NULL, 0);	// Send immediately
}
EXPORT_SYMBOL_GPL(hm2_pktuart_setup_rx);

//
// PktUART configuration implementation for TX
//
static int config_tx(const char *name, const hostmot2_t* hm2, hm2_pktuart_instance_t *inst, const hm2_pktuart_config_t *cfg, int queue)
{
	rtapi_u32 bitrate;
	rtapi_u32 mode = 0;
	int r;

	if(hm2->pktuart.tx_version >= 2) {
		bitrate = (rtapi_u64)cfg->baudrate * 16777216ul / inst->clock_freq; // 24 bits in v2+
		if(!bitrate)
			bitrate = 1;
		if(bitrate > 0x00ffffff)
			bitrate = 0x00ffffff;
	} else {
		bitrate = (rtapi_u64)cfg->baudrate * 1048576ul / inst->clock_freq;  // 20 bits in v0 & v1
	}

	if(cfg->ifdelay > 0xff) {
		if(hm2->pktuart.tx_version >= 3) {
			mode |= HM2_PKTUART_TXMODE_IFSCALE;
			if(cfg->ifdelay > 0x3fc)
				mode |= HM2_PKTUART_TXMODE_INTERFRAMEDLY(0xff);
			else
				mode |= HM2_PKTUART_TXMODE_INTERFRAMEDLY((cfg->ifdelay + 3) >> 2);
		} else {
			mode |= HM2_PKTUART_TXMODE_INTERFRAMEDLY(0xff);
		}
	} else {
		mode |= HM2_PKTUART_TXMODE_INTERFRAMEDLY(cfg->ifdelay);
	}
	mode |= HM2_PKTUART_TXMODE_DRIVEENDLY(cfg->drivedelay);
	if(cfg->flags & HM2_PKTUART_CONFIG_DRIVEAUTO) mode |= HM2_PKTUART_TXMODE_DRIVEAUTO;
	if(cfg->flags & HM2_PKTUART_CONFIG_DRIVEEN)   mode |= HM2_PKTUART_TXMODE_DRIVEEN;
	if(cfg->flags & HM2_PKTUART_CONFIG_PARITYEN)  mode |= HM2_PKTUART_TXMODE_PARITYEN;
	if(cfg->flags & HM2_PKTUART_CONFIG_PARITYODD) mode |= HM2_PKTUART_TXMODE_PARITYODD;
	if(cfg->flags & HM2_PKTUART_CONFIG_STOPBITS2 && hm2->pktuart.tx_version >= 3)
	mode |= HM2_PKTUART_TXMODE_STOPBITS2;

	int (*writefn)(hm2_lowlevel_io_t *, rtapi_u32, const void *, int);
	const char *writenm;
	if(queue && hm2->llio->queue_write) {
		writefn = hm2->llio->queue_write;
		writenm = "queue_write";
	} else {
		writefn = hm2->llio->write;
		writenm = "write";
	}

	if((cfg->flags & HM2_PKTUART_CONFIG_FORCECONFIG) || bitrate != inst->tx_bitrate) {
			inst->tx_bitrate = bitrate;
		if((r = writefn(hm2->llio, inst->tx_bitrate_addr, &bitrate, sizeof(bitrate))) < 0) {
			HM2_ERR("Configure TX baudrate: hm2->llio->%s failure %s (error %d)\n", writenm, name, r);
			return r;
		}
	}
	if((cfg->flags & HM2_PKTUART_CONFIG_FORCECONFIG) || mode != inst->tx_mode) {
		inst->tx_mode = mode;
		if((r = writefn(hm2->llio, inst->tx_mode_addr, &mode, sizeof(mode))) < 0) {
			HM2_ERR("Configure TX mode: hm2->llio->%s failure %s (error %d)\n", writenm, name, r);
			return r;
		}
	}
	if(cfg->flags & HM2_PKTUART_CONFIG_FLUSH) {
		rtapi_u32 buff = HM2_PKTUART_CLEAR; // clear data FIFO and count register
		if((r = writefn(hm2->llio, inst->tx_mode_addr, &buff, sizeof(buff))) < 0) {
			HM2_ERR("Configure TX flush: hm2->llio->%s failure %s (error %d)\n", writenm, name, r);
			return r;
		}
	}
	return 0;
}

//
// PktUART configuration implementation for RX
//
static int config_rx(const char *name, const hostmot2_t *hm2, hm2_pktuart_instance_t *inst, const hm2_pktuart_config_t *cfg, int queue)
{
	rtapi_u32 bitrate;
	rtapi_u32 mode = 0;
	rtapi_u32 filter = cfg->filterrate ? cfg->filterrate : 2*cfg->baudrate;
	int r = 0;

	filter = inst->clock_freq / filter;
	if(hm2->pktuart.rx_version >= 2) {
		if(filter > 0xFFFF) filter = 0xFFFF;
		bitrate = (rtapi_u64)cfg->baudrate * 16777216ul / inst->clock_freq; // 24 bits in v2+
		if(!bitrate)
			bitrate = 1;
		if(bitrate > 0x00ffffff)
			bitrate = 0x00ffffff;
		bitrate |= (rtapi_u32)((filter & 0xFF00) << 16); // High 8 bits
		// Low 8 filter bits set below
	} else {
		if(filter > 0xFF) filter = 0xFF;
		bitrate = (rtapi_u64)cfg->baudrate * 1048576ul / inst->clock_freq;  // 20 bits in v0 & v1
		// Low 8 filter bits set below
	}
	if(cfg->ifdelay > 0xff) {
		if(hm2->pktuart.rx_version >= 3) {
			mode |= HM2_PKTUART_RXMODE_IFSCALE;
			if(cfg->ifdelay > 0x3fc)
				mode |= HM2_PKTUART_RXMODE_INTERFRAMEDLY(0xff);
			else
				mode |= HM2_PKTUART_RXMODE_INTERFRAMEDLY(cfg->ifdelay >> 2);
		} else {
			mode |= HM2_PKTUART_RXMODE_INTERFRAMEDLY(0xff);
		}
	} else {
		mode |= HM2_PKTUART_RXMODE_INTERFRAMEDLY(cfg->ifdelay);
	}
	mode |= HM2_PKTUART_RXMODE_RXFILTER(filter); // Low 8 bits
	if(cfg->flags & HM2_PKTUART_CONFIG_RXMASKEN)  mode |= HM2_PKTUART_RXMODE_RXMASKEN;
	if(cfg->flags & HM2_PKTUART_CONFIG_RXEN)      mode |= HM2_PKTUART_RXMODE_RXEN;
	if(cfg->flags & HM2_PKTUART_CONFIG_PARITYEN)  mode |= HM2_PKTUART_RXMODE_PARITYEN;
	if(cfg->flags & HM2_PKTUART_CONFIG_PARITYODD) mode |= HM2_PKTUART_RXMODE_PARITYODD;
	if(cfg->flags & HM2_PKTUART_CONFIG_STOPBITS2 && hm2->pktuart.rx_version >= 3)
	mode |= HM2_PKTUART_RXMODE_STOPBITS2;

	int (*writefn)(hm2_lowlevel_io_t *, rtapi_u32, const void *, int);
	const char *writenm;
	if(queue && hm2->llio->queue_write) {
		writefn = hm2->llio->queue_write;
		writenm = "queue_write";
	} else {
		writefn = hm2->llio->write;
		writenm = "write";
	}

	if((cfg->flags & HM2_PKTUART_CONFIG_FORCECONFIG) || bitrate != inst->rx_bitrate) {
		inst->rx_bitrate = bitrate;
		if((r = writefn(hm2->llio, inst->rx_bitrate_addr, &bitrate, sizeof(bitrate))) < 0) {
			HM2_ERR("Configure RX baudrate: hm2->llio->%s failure %s (error %d)\n", writenm, name, r);
			return r;
		}
	}
	if((cfg->flags & HM2_PKTUART_CONFIG_FORCECONFIG) || mode != inst->rx_mode) {
		inst->rx_mode = mode;
		if((r = writefn(hm2->llio, inst->rx_mode_addr, &mode, sizeof(mode))) < 0) {
			HM2_ERR("Configure RX mode: hm2->llio->%s failure %s (error %d)\n", writenm, name, r);
			return r;
		}
	}
	if(cfg->flags & HM2_PKTUART_CONFIG_FLUSH) {
		rtapi_u32 buff = HM2_PKTUART_CLEAR; // clear data FIFO and count register
		if((r = writefn(hm2->llio, inst->rx_mode_addr, &buff, sizeof(buff))) < 0) {
			HM2_ERR("Configure RX flush: hm2->llio->%s failure %s (error %d)\n", writenm, name, r);
			return r;
		}
	}
	return 0;
}

//
// Configure a PktUART interface.
// Separate configurations are possible for RX and TX. The configuration will
// be queued if the queue parameter is set to non-zero.
//
int hm2_pktuart_config(const char *name, const hm2_pktuart_config_t *rxcfg, const hm2_pktuart_config_t *txcfg, int queue)
{
	hostmot2_t *hm2;
	int r;

	int i = hm2_get_pktuart(&hm2, name);
	if(i < 0) {
		HM2_ERR_NO_LL("Can not find PktUART instance %s (error %d).\n", name, i);
		return -ENODEV;
	}

	llio_noqueue_warn(hm2);
	hm2_pktuart_instance_t *inst = &hm2->pktuart.instance[i];

	if(rxcfg) {
		if((r = config_rx(name, hm2, inst, rxcfg, queue)) < 0)
		return r;
	}
	if(txcfg) {
		if((r = config_tx(name, hm2, inst, txcfg, queue)) < 0)
		return r;
	}
	return 0;
}
EXPORT_SYMBOL_GPL(hm2_pktuart_config);

static void perform_reset(const char *name, int queue)
{
	hostmot2_t *hm2;
	hm2_pktuart_instance_t *inst = 0;
	rtapi_u32 buff = HM2_PKTUART_CLEAR;
	int i = hm2_get_pktuart(&hm2, name);
	if(i < 0) {
		HM2_ERR_NO_LL("Can not find PktUART instance %s (error %d).\n", name, i);
		return;
	}
	inst = &hm2->pktuart.instance[i];
	int (*wr)(hm2_lowlevel_io_t*, rtapi_u32, const void *, int);
	const char *msg = "write";
	// Also check case the low level driver does not support queueing
	if(queue && hm2->llio->queue_write) {
		wr = hm2->llio->queue_write;
		msg = "queue_write";
	} else {
		wr = hm2->llio->write;
	}

	// clear sends, data FIFO and count register
	if((i = wr(hm2->llio, inst->tx_mode_addr, &buff, sizeof(rtapi_u32))) < 0)
		HM2_ERR("Failed to %s reset of TX data and FIFO (error %d)\n", msg, i);
	// clear receives, data FIFO and count register
	if((i = wr(hm2->llio, inst->rx_mode_addr, &buff, sizeof(rtapi_u32))) < 0)
		HM2_ERR("Failed to %s reset of RX data and FIFO (error %d)\n", msg, i);
}

//
// Perform an immediate TX and RX interface reset clearing the FIFOs.
// XXX: This function bypasses queued transfers!
//
void hm2_pktuart_reset(const char *name)
{
	perform_reset(name, 0);
}
EXPORT_SYMBOL_GPL(hm2_pktuart_reset);

//
// Perform a TX and RX interface reset clearing the FIFOs next time the write
// queue is sent.
//
void hm2_pktuart_queue_reset(const char *name)
{
	perform_reset(name, 1);
}
EXPORT_SYMBOL_GPL(hm2_pktuart_queue_reset);

//
// The hm2_pktuart_setup() function is DEPRECATED
// Instead use hm2_pktuart_config().
//
// use -1 for bitrate, tx_mode and rx_mode to leave the mode unchanged
// use 1 for txclear or rxclear to issue a clear command for Tx or Rx registers
//
int hm2_pktuart_setup(const char *name, unsigned bitrate, rtapi_s32 tx_mode, rtapi_s32 rx_mode, int txclear, int rxclear)
{
	hostmot2_t *hm2;
	hm2_pktuart_instance_t *inst = 0;
	rtapi_u32 buff;
	int i;
	int r = 0;

	i = hm2_get_pktuart(&hm2, name);
	if(i < 0) {
		HM2_ERR_NO_LL("Can not find PktUART instance %s (error %d).\n", name, i);
		return -ENODEV;
	}

	llio_noqueue_warn(hm2);

	inst = &hm2->pktuart.instance[i];

	if(bitrate > 0) {
		if(hm2->pktuart.tx_version >= 2) {
			buff = (rtapi_u32)((bitrate * 16777216.0)/inst->clock_freq); // 24 bits in v2+
		} else {
			buff = (rtapi_u32)((bitrate * 1048576.0)/inst->clock_freq); // 20 bits in v0 & v1
		}
		if(buff != inst->tx_bitrate) {
			inst->tx_bitrate = buff;
			if((r = hm2->llio->write(hm2->llio, inst->tx_bitrate_addr, &buff, sizeof(rtapi_u32))) < 0) {
				HM2_ERR("PktUART setup: hm2->llio->write failure %s on tx bitrate (error %d)\n", name, r);
				return r;
			}
		}
		if(hm2->pktuart.rx_version >= 2) {
			buff = (rtapi_u32)((bitrate * 16777216.0)/inst->clock_freq); // 24 bits in v2+
		} else {
			buff = (rtapi_u32)((bitrate * 1048576.0)/inst->clock_freq); // 20 bits in v0 & v1
		}
		if(buff != inst->rx_bitrate) {
			inst->rx_bitrate = buff;
			if((r = hm2->llio->write(hm2->llio, inst->rx_bitrate_addr, &buff, sizeof(rtapi_u32))) < 0) {
				HM2_ERR("PktUART setup: hm2->llio->write failure %s on rx bitrate (error %d)\n", name, r);
				return r;
			}
		}
	}

	if(tx_mode >= 0) {
		buff = ((rtapi_u32)tx_mode) & HM2_PKTUART_TXMODE_MASK;
		if((r = hm2->llio->write(hm2->llio, inst->tx_mode_addr, &buff, sizeof(rtapi_u32))) < 0) {
			HM2_ERR("PktUART setup: hm2->llio->write failure %s on tx_mode (error %d)\n", name, r);
			return r;
		}
	}

	if(rx_mode >= 0) {
		buff = ((rtapi_u32)rx_mode) & HM2_PKTUART_RXMODE_MASK;
		if((r = hm2->llio->write(hm2->llio, inst->rx_mode_addr, &buff, sizeof(rtapi_u32))) < 0) {
			HM2_ERR("PktUART setup: hm2->llio->write failure %s on rx_mode (error %d)\n", name, r);
			return r;
		}
	}

	buff = HM2_PKTUART_CLEAR;
	if(txclear == 1) {
		// clear sends, data FIFO and count register
		if((r = hm2->llio->write(hm2->llio, inst->tx_mode_addr, &buff, sizeof(rtapi_u32))) < 0) {
			HM2_ERR("PktUART setup: hm2->llio->write failure %s on tx clear (error %d)\n", name, r);
			return r;
		}
	}
	if(rxclear == 1) {
		// clear receives, data FIFO and count register
		if((r = hm2->llio->write(hm2->llio, inst->rx_mode_addr, &buff, sizeof(rtapi_u32))) < 0 ) {
			HM2_ERR("PktUART setup: hm2->llio->write failure %s on rx clear (error %d)\n", name, r);
			return r;
		}
	}

	return 0;
}
EXPORT_SYMBOL_GPL(hm2_pktuart_setup);

//
// Queue *num_framed of data with each frame's size as an entry in the
// frame_sizes array.
// Returns the total number of bytes sent.
//
int hm2_pktuart_send(const char *name, const unsigned char data[], rtapi_u8 *num_frames, const rtapi_u16 frame_sizes[])
{
	hostmot2_t *hm2;
	int c = 0;
	int r = 0;
	int count = 0;
	int inst;
	// we work with nframes as a local copy of num_frames, so that we can
	// return the num_frames sent out in case of SCFIFO error.
	unsigned nframes;

	inst = hm2_get_pktuart(&hm2, name);
	if(inst < 0) {
		HM2_ERR_NO_LL("Can not find PktUART instance %s (error %d).\n", name, inst);
		return -ENODEV;
	}
	if(hm2->pktuart.instance[inst].tx_bitrate == 0) {
		HM2_ERR("%s has not been configured.\n", name);
		return -EINVAL;
	}

	// http://freeby.mesanet.com/regmap
	// Send counts are written to 16 deep FIFO allowing up to 16 packets to be
	// sent in a burst (subject to data FIFO depth limits).

	// Test if num_frames <= MAX_TX_FRAMES
	if((*num_frames) > MAX_TX_FRAMES) {
		nframes = MAX_TX_FRAMES;
	} else{
		nframes = *num_frames;
	}

	*num_frames = 0;

	for(unsigned i = 0; i < nframes; i++) {
		count += frame_sizes[i];
		while(c < count - 3) {
			rtapi_u32 buff = data[c] + (data[c+1] << 8) + (data[c+2] << 16) + (data[c+3] << 24);
			r = hm2->llio->queue_write(hm2->llio, hm2->pktuart.instance[inst].tx_addr, &buff, sizeof(buff));
			if(r < 0) {
				HM2_ERR("%s send: hm2->llio->queue_write failure\n", name);
				return r;
			}
			c += 4;
		}

		// Now write the last bytes with bytes number < 4
		if(count - c) {
			rtapi_u32 buff;
			switch(count - c) {
			case 1: buff = data[c]; break;
			case 2: buff = (data[c] + (data[c+1] << 8)); break;
			case 3: buff = (data[c] + (data[c+1] << 8) + (data[c+2] << 16)); break;
			default:
				HM2_ERR("%s send error in buffer parsing: count = %i, i = %i\n", name, count, c);
				return -1;
			} // end switch
			r = hm2->llio->queue_write(hm2->llio, hm2->pktuart.instance[inst].tx_addr, &buff, sizeof(buff));
			if(r < 0) {
			   HM2_ERR("%s send: hm2->llio->queue_write failure\n", name);
			   return r;
			}
		}
		(*num_frames)++;
		c = count;
	} // for loop

	// Write the number of bytes to be sent to PktUARTx sendcount register
	for(unsigned i = 0; i < nframes; i++) {
		rtapi_u32 buff = frame_sizes[i];
		r = hm2->llio->queue_write(hm2->llio, hm2->pktuart.instance[inst].tx_fifo_count_addr, &buff, sizeof(buff));
		// Check for Send Count FIFO error
		// XXX ==> Deleted a queue_read that originally was a non-queued read.
		// Not possible to check for errors on the fly when using queued
		// transfers. The data does not arrive immediately. Therefore, no
		// checks can be done here.
		if(r < 0) {
			HM2_ERR("%s send: hm2->llio->queue_write failure\n", name);
			return r;
		}
	}
	return count;
}
EXPORT_SYMBOL_GPL(hm2_pktuart_send);

//
// The function hm2_pktuart_read() performs reads/writes outside of the normal
// thread cycles. This is especially a problem with the Ethernet and p-port
// connected cards where the reads and writes should be packeted. This function
// has been left in place for backwards compatibility, but it is recommended to
// use the hm2_pktuart_queue_*() functions instead
//
// This function is DEPRECATED
//
int hm2_pktuart_read(const char *name, unsigned char data[], rtapi_u8 *num_frames, rtapi_u16 *max_frame_length, rtapi_u16 frame_sizes[])
{
	hostmot2_t *hm2;
	int r, c;
	int bytes_total = 0; // total amount of bytes read
	rtapi_u16 countp; // packets count
	rtapi_u16 countb; // bytes count for the oldest packet received
	int inst;
	rtapi_u32 buff;
	rtapi_u16 data_size=(*num_frames)*(*max_frame_length);

	inst = hm2_get_pktuart(&hm2, name);

	if(inst < 0) {
		HM2_ERR_NO_LL("Can not find PktUART instance %s (error %d).\n", name, inst);
		*num_frames=0;
		return -ENODEV;
	}
	if(hm2->pktuart.instance[inst].rx_bitrate == 0 ) {
		HM2_ERR("%s has not been configured.\n", name);
		*num_frames=0;
		return -EINVAL;
	}


	// First poll the mode register for a non zero frames received count
	// (mode register bits 20..16)
	r = hm2->llio->read(hm2->llio, hm2->pktuart.instance[inst].rx_mode_addr, &buff, sizeof(rtapi_u32));
	if(r < 0) {
		HM2_ERR("%s read: hm2->llio->read failure\n", name);
		return r;
	}
	if(buff & (0x1 << 21)) {
		countp = (buff >> 16)  & 0x1f;
	} else {
		countp = 0;
	}
	//HM2_INFO("hm2_pktuart: buffer = %08x\n", buff);
	//HM2_INFO("hm2_pktuart: %i frames received\n", countp);

	// We expect to read at least 1 frame.
	// If there is no complete frame yet in the buffer,
	// we'll deal with this by checking error bits.
	*num_frames = 0;

	// Bit 7 set does not really indicate any error condition,
	// but very probably means that the cycle time of the thread,
	// which you attach this function to, is not appropriate.
	if(buff & HM2_PKTUART_RXMODE_RXBUSY) {
		HM2_INFO("%s: Buffer error (RX idle but data in RX data FIFO)\n", name);
	}

	// Now check the error bits
	if(buff & HM2_PKTUART_RXMODE_ERROROVERRUN) {
		HM2_ERR_NO_LL("%s: Overrun error, no stop bit\n", name);
		//return -HM2_PKTUART_RxOverrunError;
		return -EIO;
	}
	if(buff & HM2_PKTUART_RXMODE_ERRORSTARTBIT) {
		HM2_ERR_NO_LL("%s: False Start bit error\n", name);
		//return -HM2_PKTUART_RxStartbitError;
		return -EIO;
	}

	// RCFIFO Error will get sticky if it is a consequence of either Overrun or False Start bit error?
	if(buff & HM2_PKTUART_RXMODE_ERRORRCFIFO) {
		HM2_ERR_NO_LL("%s: RCFIFO Error\n", name);
		//return -HM2_PKTUART_RxRCFIFOError;
		return -EIO;
	}

	if(countp==0) {
		HM2_INFO_NO_LL("%s: no new frames \n", name);
		return 0;  // return zero bytes and zero frames
	}

	rtapi_u16 i=0;
	while(i < countp) {
		buff=0;
	/* The receive count register is a FIFO that contains the byte counts
	of received packets. Since it is a FIFO it must only be read once after it
	has be determined that there are packets available to read. */
		r = hm2->llio->read(hm2->llio, hm2->pktuart.instance[inst].rx_fifo_count_addr, &buff, sizeof(buff));

		countb = buff & 0x3ff; // PktUARTr  receive count register Bits 9..0 : bytes in receive packet

		if((buff >> 14) & 0x1) {
			HM2_ERR_NO_LL("%s has False Start bit error in this packet.\n", name);
			//return -HM2_PKTUART_RxPacketStartbitError;
			return -EIO;
		}

		if((buff >> 15) & 0x1) {
			HM2_ERR_NO_LL("%s has Overrun error in this packet\n", name);
			//return -HM2_PKTUART_RxPacketOverrrunError;
			return -EIO;
		}

		// a packet is completely received, but its byte count is zero
		// is very improbable, however we intercept this error too
		if(countb==0) {
			HM2_ERR_NO_LL("%s: packet %d has %d bytes.\n", name, countp+1, countb);
			//return -HM2_PKTUART_RxPacketSizeZero;
			return -EIO;
		}

		if(( bytes_total+countb)> data_size) {
			HM2_ERR_NO_LL("%s: bytes available %d are more than data array size %d\n", name, bytes_total+countb, data_size);
			//return -HM2_PKTUART_RxArraySizeError;
			//countb = data_size - bytes_total;
			return -EIO;
		}

		(*num_frames)++; // increment num_frames to be returned at the end
		c = 0;
		buff = 0;
		frame_sizes[i]=countb;

		while(c < countb - 3) {
			r = hm2->llio->read(hm2->llio, hm2->pktuart.instance[inst].rx_addr, &buff, sizeof(buff));

			if(r < 0) {
				HM2_ERR("%s read: hm2->llio->read failure\n", name);
				return r;
			}

			data[bytes_total+c+0] = (buff & 0x000000FF); // i*frame_sizes[i]
			data[bytes_total+c+1] = (buff & 0x0000FF00) >> 8;
			data[bytes_total+c+2] = (buff & 0x00FF0000) >> 16;
			data[bytes_total+c+3] = (buff & 0xFF000000) >> 24;
			c = c + 4;
		}

		if(countb - c) {
			r = hm2->llio->read(hm2->llio, hm2->pktuart.instance[inst].rx_addr, &buff, sizeof(buff));
			if(r < 0) {
				HM2_ERR("%s read: hm2->llio->queue_write failure\n", name);
				return -1;
			}
			switch(countb - c) {
			case 1:
				data[bytes_total+c+0] = (buff & 0x000000FF);
				break;
			case 2:
				data[bytes_total+c+0] = (buff & 0x000000FF);
				data[bytes_total+c+1] = (buff & 0x0000FF00) >> 8;
				break;
			case 3:
				data[bytes_total+c+0] = (buff & 0x000000FF);
				data[bytes_total+c+1] = (buff & 0x0000FF00) >> 8;
				data[bytes_total+c+2] = (buff & 0x00FF0000) >> 16;
				break;
			default:
				HM2_ERR_NO_LL("PktUART READ: Error in buffer parsing.\n");
				return -EINVAL;
			}
		}

		bytes_total = bytes_total + countb;

		i++; // one frame/datagram read
	} // frame loop

	return bytes_total;
}
EXPORT_SYMBOL_GPL(hm2_pktuart_read);

//
// The hm2_pktuart_queue_get_frame_sizes() function queues sufficient reads to
// get the available frame sizes data is loaded into the *fsizes array on the
// next read cycle. Parameter fsizes should be u32 x 16.
// FIXME: decide how to work out that the data has all been transferred
//
int hm2_pktuart_queue_get_frame_sizes(const char *name, rtapi_u32 fsizes[])
{
	// queue as many reads of the FIFO as there are frames
	hostmot2_t *hm2;
	int inst;
	int j;
	int r;

	inst = hm2_get_pktuart(&hm2, name);

	if(inst < 0) {
		HM2_ERR_NO_LL("Can not find PktUART instance %s (error %d).\n", name, inst);
		return -ENODEV;
	}

	if(hm2->pktuart.instance[inst].rx_bitrate == 0 ) {
		HM2_ERR("%s has not been configured.\n", name);
		return -EINVAL;
	}

	int nfs = (int)((hm2->pktuart.rx_status_reg[inst] >> 16) & 0x1F);
	for(j = 0; j < nfs; j++ ) {
		r = hm2->llio->queue_read(hm2->llio, hm2->pktuart.instance[inst].rx_fifo_count_addr,
				&fsizes[j], sizeof(rtapi_u32));
		if(r < 0) {
			HM2_ERR("Unable to queue Rx FIFO count read %d of %d (error %d))\n", j, nfs, r);
		}
	}
	return j - 1;
}
EXPORT_SYMBOL_GPL(hm2_pktuart_queue_get_frame_sizes);


//
// The hm2_pktuart_queue_read_data() function queues sufficient reads to
// extract the available data.  It does no error checking and does not
// explicitly check if there is data to read (it will just queue zero reads in
// that case).
// The using function should use the hm2_pktuart_get_rx_status functions to
// determine whether data exists and error status.
// The function queues enough reads to read the given number of 32-bit frames,
// which must have been previously read by hm2_pktuart_queue_get_frame_sizes().
// Returns the number of frame reads queued.
//
int hm2_pktuart_queue_read_data(const char *name, rtapi_u32 data[], int bytes)
{
	hostmot2_t *hm2;
	int r;
	int i;
	int inst;

	inst = hm2_get_pktuart(&hm2, name);

	if(inst < 0) {
		HM2_ERR_NO_LL("Can not find PktUART instance %s (error %d).\n", name, inst);
		return -ENODEV;
	}
	if(hm2->pktuart.instance[inst].rx_bitrate == 0 ) {
		HM2_ERR("%s has not been configured.\n", name);
		return -EINVAL;
	}

	// queue enough reads to get the whole frame. Data will be transferred
	// to data[] next thread cycle, direct from FPGA, no serial latency
	int nrx = (bytes + 3) / 4;	// Always full 32-bit words
	for(i = 0; i < nrx; i++ ) {
		r = hm2->llio->queue_read(hm2->llio, hm2->pktuart.instance[inst].rx_addr,
					 &data[i], sizeof(rtapi_u32));
		if(r < 0) {
			HM2_ERR("Unable to queue Rx FIFO read %d of %d (error %d)\n", i, nrx, r);
		}
	}
	return i - 1;
}
EXPORT_SYMBOL_GPL(hm2_pktuart_queue_read_data);

//
// Return the current RX status from last tram read
//
rtapi_u32 hm2_pktuart_get_rx_status(const char *name)
{
	hostmot2_t *hm2;
	int i = hm2_get_pktuart(&hm2, name);
	if(i < 0) {
		HM2_ERR_NO_LL("Can not find PktUART instance %s (error %d).\n", name, i);
		return 0;
	}
	return hm2->pktuart.rx_status_reg[i];
}
EXPORT_SYMBOL_GPL(hm2_pktuart_get_rx_status);

//
// Return the current TX status from last tram read
//
rtapi_u32 hm2_pktuart_get_tx_status(const char *name)
{
	hostmot2_t *hm2;
	int i = hm2_get_pktuart(&hm2, name);
	if(i < 0) {
		HM2_ERR_NO_LL("Can not find PktUART instance %s (error %d).\n", name, i);
		return 0;
	}
	return hm2->pktuart.tx_status_reg[i];
}
EXPORT_SYMBOL_GPL(hm2_pktuart_get_tx_status);

//
// Return the lower clock used by the PktUART
//
int hm2_pktuart_get_clock(const char* name)
{
	hostmot2_t *hm2;
	int i = hm2_get_pktuart(&hm2, name);
	if(i < 0) {
		HM2_ERR_NO_LL("Can not find PktUART instance %s (error %d).\n", name, i);
		return -ENODEV;
	}
	hm2_pktuart_instance_t inst = hm2->pktuart.instance[i];
	return inst.clock_freq;
}
EXPORT_SYMBOL_GPL(hm2_pktuart_get_clock);

//
// Return the RX/TX PktUART version as implemented by the fpga.
// The value is: (RX_version << 4) + TX_version
int hm2_pktuart_get_version(const char* name)
{
	hostmot2_t *hm2;
	int i = hm2_get_pktuart(&hm2, name);
	if(i < 0) {
		HM2_ERR_NO_LL("Can not find PktUART instance %s (error %d).\n", name, i);
		return -ENODEV;
	}
	return hm2->pktuart.tx_version + 16 * hm2->pktuart.rx_version;
}
EXPORT_SYMBOL_GPL(hm2_pktuart_get_version);

// vim: ts=4
