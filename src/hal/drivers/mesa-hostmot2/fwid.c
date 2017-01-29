
//
//    Copyright (C) 2016 Michael Haberler

//    This program is free software; you can redistribute it and/or modify
//    it under the terms of the GNU General Public License as published by
//    the Free Software Foundation; either version 2 of the License, or
//    (at your option) any later version.
//
//    This program is distributed in the hope that it will be useful,
//    but WITHOUT ANY WARRANTY; without even the implied warranty of
//    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//    GNU General Public License for more details.
//
//    You should have received a copy of the GNU General Public License
//    along with this program; if not, write to the Free Software
//    Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301 USA
//

// Firmware ID decoder for the Mesa FPGA firwmare (socfpga)
#include "config_module.h"
#include RTAPI_INC_SLAB_H

#include "rtapi.h"
#include "rtapi_string.h"
#include "rtapi_hexdump.h"

#include "hal/drivers/mesa-hostmot2/hostmot2.h"
#include <machinetalk/nanopb/pb_decode.h>
#include <machinetalk/build/machinetalk/protobuf/firmware.npb.c>

int hm2_fwid_parse_md(hostmot2_t *hm2, int md_index) {
    int r = 0;
    u16 addr;
    u32 rawsize, alignedsize, cookie;
    void *buf;

    hm2->fwid.dmsg = NULL;

    if (md_index > -1) {
	hm2_module_descriptor_t *md = &hm2->md[md_index];

	if (!hm2_md_is_consistent_or_complain(hm2, md_index, 0, 1, 4, 0x0000)) {
	    HM2_ERR("inconsistent Module Descriptor!\n");
	    return -EINVAL;
	}
	addr = md->base_address;
    } else
	addr = HM2_FWID_BASE;


    if (hm2->llio->fwid_len == 0) {
	// no override msg, read from FPGA
	// read the fwid cookie (u32)
	hm2->llio->read(hm2->llio, addr, &cookie,
			sizeof(cookie));
	if (cookie != HM2_FWID_COOKIE) {
	    HM2_DBG("no FirmwareID cookie present: 0x%ux (expected %x)\n", cookie, HM2_FWID_COOKIE);
	    goto fail0;
	}

	// read the encoded message size field (u32), follows after cookie
	hm2->llio->read(hm2->llio, addr + 4, &rawsize,
			sizeof(rawsize));
	if (rawsize > HM2_FWID_MAXLEN) {
	    HM2_ERR("unreasonable FirmwareID message size: %u\n", rawsize );
	    r = -EFBIG;
	    goto fail0;
	}
	if (rawsize == 0) {
	    HM2_INFO("no FirmwareID message present\n");
	    goto fail0;  // not really a failure
	}
	// align read size on next 32bit boundary to keep llio->read() from complaining
	alignedsize = (rawsize+3) & ~3;

	HM2_DBG("protobuf raw size=%u, aligned to %u\n", rawsize, alignedsize);
	buf = kmalloc(alignedsize, GFP_KERNEL);
	if (buf == NULL) {
	    HM2_ERR("out of memory allocating pbmsg buffer size=%u\n", alignedsize);
	    r = -ENOMEM;
	    goto fail1;
	}
	// read the encoded message into the temp buffer
	// starts right after the uint32-sized size field
	hm2->llio->read(hm2->llio, addr + 8, buf, alignedsize);
	rtapi_print_hex_dump(RTAPI_MSG_DBG, RTAPI_DUMP_PREFIX_OFFSET,
			     16, 1, (const void *)buf, rawsize, 1, NULL,
			     "fwid msg at %p:", buf);
    } else {
	alignedsize = rawsize = hm2->llio->fwid_len; // override msg was passed
	HM2_DBG("custom fwid message from llio, size=%u\n", alignedsize);
	buf =  hm2->llio->fwid_msg;
    }

    hm2->fwid.dmsg = (machinetalk_Firmware *)kmalloc(sizeof(machinetalk_Firmware), GFP_KERNEL);
    if (hm2->fwid.dmsg == NULL) {
	HM2_ERR("out of memory allocating machinetalk_Firmware\n");
	r = -ENOMEM;
	goto fail0;
    }

    // this is the actual decoding step:
    pb_istream_t stream = pb_istream_from_buffer(buf,  rawsize);
    if (!pb_decode(&stream, machinetalk_Firmware_fields, hm2->fwid.dmsg)) {
	HM2_ERR("pb_decode(Firmware) failed: '%s'\n", PB_GET_ERROR(&stream));
	r = -EINVAL;
	goto fail2;
    }
    if (hm2->llio->fwid_len == 0)
	kfree(buf);

    machinetalk_Firmware *fw = hm2->fwid.dmsg;
    if (fw->has_board_name)
	HM2_DBG("board_name = '%s'", fw->board_name);
    if (fw->has_build_sha)
	HM2_DBG("build_sha = '%s'", fw->build_sha);
    if (fw->has_fpga_part_number)
	HM2_DBG("fpga_part_number = '%s'", fw->fpga_part_number);
    if (fw->has_num_leds) {
	HM2_DBG("num_leds = %d", fw->num_leds);
	hm2->llio->num_leds = fw->num_leds;
    }
    if (fw->has_comment)
	HM2_DBG("comment = %s", fw->comment);

    size_t n;
    for (n = 0; n < fw->connector_count; n++) {
	machinetalk_Connector *conn = &fw->connector[n];
	if (conn->has_name) {
	    HM2_DBG("connector %zu name = '%s'", n, conn->name);
	    hm2->llio->ioport_connector_name[n] = conn->name;
	}
	if (conn->has_pins)
	    HM2_DBG("connector %zu pins = %d", n, conn->pins);
    }
    hm2->llio->num_ioport_connectors =  fw->connector_count;

    // for now, use connector[0].pins as there is no way to describe
    // per-connector pincount in the idrom and llio daza
    hm2->llio->pins_per_connector = fw->connector[0].pins;
    hm2->llio->fpga_part_number = fw->fpga_part_number;

    return 0;

 fail2:
    kfree(buf);
 fail1:
    kfree(hm2->fwid.dmsg);
    hm2->fwid.dmsg = NULL;
 fail0:
    return r;
}

void hm2_fwid_cleanup(hostmot2_t *hm2) {
    if (hm2->fwid.dmsg != NULL) {
	kfree(hm2->fwid.dmsg);
	hm2->fwid.dmsg = NULL;
    }
}
