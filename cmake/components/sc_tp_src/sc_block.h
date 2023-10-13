#ifndef SC_BLOCK_H
#define SC_BLOCK_H

//! Author  : SKynet Cyberdyne
//! Licence : MIT
//! Date    : 2023

#include "sc_struct.h"

//! C style vector functions thanks to chatgdp, not verified.
void block_add(struct sc_block **blocks, int *num_blocks, struct sc_block new_block);
void block_insert_front(struct sc_block **pvec, int *size, struct sc_block *block);
void insert_at_index(struct sc_block **pvec, int *size, int index, struct sc_block *block);
void block_delete(struct sc_block **pvec, int *size, int index);
void block_pop_back(struct sc_block **pvec, int *size);
void block_pop_front(struct sc_block **pvec, int *size);
void block_print(struct sc_block *pvec, int index);

#endif










