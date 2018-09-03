
//
//    Copyright (C) 2007-2008 Sebastian Kuzminsky
//
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

#include "config_module.h"
#include RTAPI_INC_SLAB_H

#include "rtapi.h"
#include "rtapi_string.h"
#include "rtapi_math.h"

#include "hal.h"

#include "hal/drivers/mesa-hostmot2/hostmot2.h"




//
// This function is called by the MD parsers.  It records that in the
// specified hm2 instance, the specified register address range should be
// copied to memory on the Linux computer, and *buffer should point to that
// memory.
//
// The hm2_allocate_tram_regions() function below actually allocates
// the memory and writes the buffer variable.
//
// in the future this function will inform the Translation RAM
//

int hm2_register_tram_read_region(hostmot2_t *hm2, u16 addr, u16 size, u32 **buffer) {
    hm2_tram_entry_t *tram_entry;

    tram_entry = kmalloc(sizeof(hm2_tram_entry_t), GFP_KERNEL);
    if (tram_entry == NULL) {
        HM2_ERR("out of memory!\n");
        return -ENOMEM;
    }

    tram_entry->addr = addr;
    tram_entry->size = size;
    tram_entry->buffer = buffer;

    list_add_tail(&tram_entry->list, &hm2->tram_read_entries);

    return 0;
}


int hm2_register_tram_write_region(hostmot2_t *hm2, u16 addr, u16 size, u32 **buffer) {
    hm2_tram_entry_t *tram_entry;

    tram_entry = kmalloc(sizeof(hm2_tram_entry_t), GFP_KERNEL);
    if (tram_entry == NULL) {
        HM2_ERR("out of memory!\n");
        return -ENOMEM;
    }

    tram_entry->addr = addr;
    tram_entry->size = size;
    tram_entry->buffer = buffer;

    list_add_tail(&tram_entry->list, &hm2->tram_write_entries);

    return 0;
}


int hm2_allocate_tram_regions(hostmot2_t *hm2) {
    struct list_head *ptr;
    u16 offset;
    
    int old_tram_read_size = hm2->tram_read_size;
    int old_tram_write_size = hm2->tram_write_size;

    hm2->tram_read_size = 0;
    list_for_each(ptr, &hm2->tram_read_entries) {
        hm2_tram_entry_t *tram_entry = list_entry(ptr, hm2_tram_entry_t, list);
        hm2->tram_read_size += tram_entry->size;
    }

    hm2->tram_write_size = 0;
    list_for_each(ptr, &hm2->tram_write_entries) {
        hm2_tram_entry_t *tram_entry = list_entry(ptr, hm2_tram_entry_t, list);
        hm2->tram_write_size += tram_entry->size;
    }

    HM2_DBG(
        "allocating Translation RAM buffers (reading %d bytes, writing %d bytes)\n",
        hm2->tram_read_size,
        hm2->tram_write_size
    );

    hm2->tram_read_buffer = (u32 *)krealloc(hm2->tram_read_buffer, hm2->tram_read_size, GFP_KERNEL);
    if (hm2->tram_read_buffer == NULL) {
        HM2_ERR("Error while (re)allocating Translation RAM read buffer (%d bytes)\n", hm2->tram_read_size);
        return -ENOMEM;
    }
    if(hm2->tram_read_size>old_tram_read_size)
        memset(hm2->tram_read_buffer+old_tram_read_size, 0, hm2->tram_read_size-old_tram_read_size);

    hm2->tram_write_buffer = (u32 *)krealloc(hm2->tram_write_buffer, hm2->tram_write_size, GFP_KERNEL);
    if (hm2->tram_write_buffer == NULL) {
        HM2_ERR("Error while (re)allocating Translation RAM write buffer (%d bytes)\n", hm2->tram_write_size);
        return -ENOMEM;
    }
    if(hm2->tram_write_size>old_tram_write_size)
        memset(hm2->tram_write_buffer+old_tram_write_size, 0, hm2->tram_write_size-old_tram_write_size);

    HM2_DBG("buffer address %p\n", &hm2->tram_write_buffer);
    HM2_DBG("Translation RAM read buffer:\n");
    offset = 0;
    list_for_each(ptr, &hm2->tram_read_entries) {
        hm2_tram_entry_t *tram_entry = list_entry(ptr, hm2_tram_entry_t, list);
        *tram_entry->buffer = (u32*)((u8*)hm2->tram_read_buffer + offset);
        offset += tram_entry->size;
        HM2_DBG("    addr=0x%04x, size=%d, buffer=%p\n", tram_entry->addr, tram_entry->size, *tram_entry->buffer);
    }

    HM2_DBG("Translation RAM write buffer:\n");
    offset = 0;
    list_for_each(ptr, &hm2->tram_write_entries) {
        hm2_tram_entry_t *tram_entry = list_entry(ptr, hm2_tram_entry_t, list);
        *tram_entry->buffer = (u32*)((u8*)hm2->tram_write_buffer + offset);
        offset += tram_entry->size;
        HM2_DBG("    addr=0x%04x, size=%d, buffer=%p\n", tram_entry->addr, tram_entry->size, *tram_entry->buffer);
    }
    
    return 0;
}


int hm2_tram_read(hostmot2_t *hm2) {
    static u32 tram_read_iteration = 0;
    struct list_head *ptr;

    list_for_each(ptr, &hm2->tram_read_entries) {
        hm2_tram_entry_t *tram_entry = list_entry(ptr, hm2_tram_entry_t, list);

        if (!hm2->llio->queue_read(hm2->llio, tram_entry->addr, *tram_entry->buffer, tram_entry->size)) {
            HM2_ERR("TRAM read error! (addr=0x%04x, size=%d, iter=%u)\n", tram_entry->addr, tram_entry->size, tram_read_iteration);
            return -EIO;
        }
    }

    if (!hm2->llio->queue_read(hm2->llio, 0, NULL, -1)) {
        HM2_ERR("TRAM read error finishing read! iter=%u)\n",
            tram_read_iteration);
    }
    tram_read_iteration ++;

    return 0;
}


int hm2_tram_write(hostmot2_t *hm2) {
    static u32 tram_write_iteration = 0;
    struct list_head *ptr;

    list_for_each(ptr, &hm2->tram_write_entries) {
        hm2_tram_entry_t *tram_entry = list_entry(ptr, hm2_tram_entry_t, list);

        if (!hm2->llio->queue_write(hm2->llio, tram_entry->addr, *tram_entry->buffer, tram_entry->size)) {
            HM2_ERR("TRAM write error! (addr=0x%04x, size=%d, iter=%u)\n", tram_entry->addr, tram_entry->size, tram_write_iteration);
            return -EIO;
        }
    }

    if (!hm2->llio->queue_write(hm2->llio, 0, NULL, -1)) {
        HM2_ERR("TRAM write error finishing write! iter=%u)\n",
            tram_write_iteration);
    }
    tram_write_iteration ++;

    return 0;
}


void hm2_tram_cleanup(hostmot2_t *hm2) {
    while (hm2->tram_read_entries.next != &hm2->tram_read_entries) {
        struct list_head *te_ptr = hm2->tram_read_entries.next;
        hm2_tram_entry_t *te = list_entry(te_ptr, hm2_tram_entry_t, list);
        list_del(te_ptr);
        kfree(te);
    }
    while (hm2->tram_write_entries.next != &hm2->tram_write_entries) {
        struct list_head *te_ptr = hm2->tram_write_entries.next;
        hm2_tram_entry_t *te = list_entry(te_ptr, hm2_tram_entry_t, list);
        list_del(te_ptr);
        kfree(te);
    }

    // free the tram buffers
    if (hm2->tram_read_buffer != NULL) kfree(hm2->tram_read_buffer);
    if (hm2->tram_write_buffer != NULL) kfree(hm2->tram_write_buffer);
}

