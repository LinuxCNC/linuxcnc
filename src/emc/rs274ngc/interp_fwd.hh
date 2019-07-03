/**
 * @file interp_fwd.hh
 * 
 * Forward declarations for interp_internal.hh.
 *
 * @author Robert W. Ellenberg <rwe24g@gmail.com>
 *
 * Copyright (c) 2019, Robert W. Ellenberg
 *
 * This source code is released for free distribution under the terms of the
 * GNU General Public License (V2) as published by the Free Software Foundation.
 */

#ifndef INTERP_FWD_HH
#define INTERP_FWD_HH

class Interp;

struct block_struct;
typedef struct block_struct *block_pointer;
typedef struct block_struct block;

struct setup;
typedef struct setup *setup_pointer;

struct remap_struct;
typedef struct remap_struct *remap_pointer;
typedef struct remap_struct remap;

struct context_struct;
typedef struct context_struct *context_pointer;
typedef struct context_struct context;

struct parameter_value_struct;
typedef parameter_value_struct *parameter_pointer;
typedef parameter_value_struct parameter_value;

struct offset_struct;
typedef offset_struct *offset_pointer;
typedef offset_struct offset;

#endif // INTERP_FWD_HH
