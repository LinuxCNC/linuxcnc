//
// This is lui-test, a test program for liblinuxcnc-ui
//
// Copyright (C) 2014 Sebastian Kuzminsky <seb@highlab.com>
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
//


#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "linuxcnc-ui.h"

#define fatal_if(cond, message, ...) do { \
    if(cond) { printf(message, ## __VA_ARGS__); exit(1); } \
} while(0)

int main(int argc, char *argv[]) {
    lui_t *lui;

    lui = lui_new();
    if (lui == NULL) {
        fprintf(stderr, "failed to allocate lui\n");
        return 1;
    }

    lui_free(lui);
    printf("kthxbye\n");

    return 0;
}
