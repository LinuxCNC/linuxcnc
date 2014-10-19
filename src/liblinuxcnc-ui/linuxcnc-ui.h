//
// liblinuxcnc-ui: a library to control linuxcnc
//
// Copyright (C) 2014 Sebastian Kuzminsky
//
// This library is free software; you can redistribute it and/or
// modify it under the terms of the GNU Library General Public
// License as published by the Free Software Foundation; either
// version 2 of the License, or (at your option) any later version.
//
// This library is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
// Library General Public License for more details.
//
// You should have received a copy of the GNU Library General Public
// License along with this library; if not, write to the
// Free Software Foundation, Inc., 51 Franklin St, Fifth Floor,
// Boston, MA  02110-1301, USA.
//

#ifndef LIBLINUXCNC_UI
#define LIBLINUXCNC_UI

typedef struct lui lui_t;

#ifdef __cplusplus
extern "C" {
#endif

lui_t *lui_new(void);
void lui_free(lui_t *lui);

int lui_connect(lui_t *lui);

#ifdef __cplusplus
}
#endif

#endif  // LIBLINUXCNC_UI
