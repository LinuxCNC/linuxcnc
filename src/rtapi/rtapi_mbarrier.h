/********************************************************************
 * Copyright (C) 2012, 2013 Michael Haberler <license AT mah DOT priv DOT at>
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
 ********************************************************************/

#ifndef _RTAPI_MBARRIER_H
#define _RTAPI_MBARRIER_H


// memory barrier primitives
// required for SMP-safe lock-free datastructures
// see https://www.kernel.org/doc/Documentation/memory-barriers.txt

// use gcc intrinsics

#define	rtapi_smp_mb()  __sync_synchronize()
#define	rtapi_smp_wmb() __sync_synchronize()
#define	rtapi_smp_rmb() __sync_synchronize()


#endif // _RTAPI_MBARRIER_H
