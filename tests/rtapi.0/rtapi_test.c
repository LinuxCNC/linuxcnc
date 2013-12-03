/* Copyright (C) 2012, 2013 Michael Haberler <license AT mah DOT priv DOT at>
 * 
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 * 
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */


#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include "rtapi.h"

int main()
{
    int comp_id = rtapi_init("testmod");

    if (comp_id < 0) {
	printf("rtapi_init() failed: %d\n", comp_id);
	exit(1);
    }
    printf("rtapi_init() succeeded\n");
    rtapi_exit(comp_id);
    exit(0);
}
