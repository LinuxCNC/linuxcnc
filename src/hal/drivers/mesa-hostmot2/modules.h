//
//    Copyright (C) 2012 Michael Geszkiewicz
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

#ifndef __MODULES_H
#define __MODULES_H

#include <linux/list.h>

typedef struct {
    void (*read)(hostmot2_t *hm2, void *void_module);
    void (*write)(hostmot2_t *hm2, void *void_module);
    void (*force_write)(hostmot2_t *hm2, void *void_module);
    void (*cleanup)(hostmot2_t *hm2, void *void_module);
    void (*print_module)(hostmot2_t *hm2, void *void_module);
    void *data;
    int type;

    struct list_head list;
} hm2_module_t;


hm2_module_t *hm2_find_module(hostmot2_t *hm2, int tag);

#endif
