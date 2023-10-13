#include "sc_block.h"

void block_add(struct sc_block **blocks, int *num_blocks, struct sc_block new_block) {
    (*num_blocks)++;
    *blocks = realloc(*blocks, (*num_blocks) * sizeof(struct sc_block));
    (*blocks)[(*num_blocks) - 1] = new_block;
}

// function to delete a block from an array
void block_delete(struct sc_block **pvec, int *size, int index) {
    if (index < 0 || index >= *size) {
        printf("Error: invalid index\n");
        return;
    }

    // free the memory for the block being deleted
    free(pvec[index]);

    // shift all the blocks after the deleted block to the left by one position
    for (int i = index; i < *size-1; i++) {
        pvec[i] = pvec[i+1];
    }

    // decrement the size of the array
    (*size)--;
}

// function to add a block to the beginning of an array
void block_insert_front(struct sc_block **pvec, int *size, struct sc_block *block) {
    // allocate memory for a new block array
    struct sc_block *temp = malloc((*size + 1) * sizeof(struct sc_block));

    // assign the new block to the first position in the array
    temp[0] = *block;

    // copy the existing blocks to the remaining positions in the array
    for (int i = 0; i < *size; i++) {
        temp[i+1] = (*pvec)[i];
    }

    // free the memory allocated for the original block array
    free(*pvec);

    // assign the new block array to pvec and update the size
    *pvec = temp;
    (*size)++;
}

void insert_at_index(struct sc_block **pvec, int *size, int index, struct sc_block *block) {
    // allocate memory for a new block array
    struct sc_block *temp = malloc((*size + 1) * sizeof(struct sc_block));

    // copy the existing blocks up to the insertion point
    for (int i = 0; i < index; i++) {
        temp[i] = (*pvec)[i];
    }

    // insert the new block
    temp[index] = *block;

    // copy the remaining blocks
    for (int i = index+1; i < *size+1; i++) {
        temp[i] = (*pvec)[i-1];
    }

    // free the memory allocated for the original block array
    free(*pvec);

    // assign the new block array to pvec and update the size
    *pvec = temp;
    (*size)++;
}

// function to delete the last element from an array
void block_pop_back(struct sc_block **pvec, int *size) {
    if (*size > 0) {
        // decrement the size of the array
        (*size)--;
        // deallocate the memory for the last element
        free(pvec[*size]);
    }
}

// function to delete the first element from an array
void block_pop_front(struct sc_block **pvec, int *size) {
    if (*size > 0) {
        // decrement the size of the array
        (*size)--;
        // shift all the elements to the left by one position
        for (int i = 0; i < *size; i++) {
            pvec[i] = pvec[i+1];
        }
        // deallocate the memory for the first element
        free(pvec[0]);
    }
}

void block_print(struct sc_block *pvec, int index) {
    printf("Printing %d blocks:\n", index);
    for (int i = 0; i < index+1; i++) {
        printf("Block %d:\n", i+1);
        printf("  G-code primitive ID: %d\n", pvec[i].primitive_id);
        printf("  G-code line number: %d\n", pvec[i].gcode_line_nr);
        printf("  Canonical motion type: %d\n", pvec[i].type);
        printf("  Start position: (%f, %f, %f, %f, %f, %f, %f, %f, %f)\n",
               pvec[i].pnt_s.x, pvec[i].pnt_s.y, pvec[i].pnt_s.z,
               pvec[i].dir_s.a, pvec[i].dir_s.b, pvec[i].dir_s.c,
               pvec[i].ext_s.u, pvec[i].ext_s.v, pvec[i].ext_s.w);
        printf("  Way position: (%f, %f, %f)\n",
               pvec[i].pnt_w.x, pvec[i].pnt_w.y, pvec[i].pnt_w.z);
        printf("  End position: (%f, %f, %f, %f, %f, %f, %f, %f, %f)\n",
               pvec[i].pnt_e.x, pvec[i].pnt_e.y, pvec[i].pnt_e.z,
               pvec[i].dir_e.a, pvec[i].dir_e.b, pvec[i].dir_e.c,
               pvec[i].ext_e.u, pvec[i].ext_e.v, pvec[i].ext_e.w);
        printf("  Center point: (%f, %f, %f)\n", pvec[i].pnt_c.x, pvec[i].pnt_c.y, pvec[i].pnt_c.z);
        //! Velcocity, acc.
        printf("  vo: %f\n", pvec[i].vo);
        printf("  ve: %f\n", pvec[i].ve);

        printf("\n");
    }
}
