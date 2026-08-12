/*
  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation; either version 2 of the License, or
  (at your option) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
*/

/* switchkins_core.c makes switchkins.c and kins_util.c a module of their
*  own, for kinematics modules built out of tree.  It provides no
*  kinematics types itself: the module loaded after it registers its types
*  with switchkinsRegister() and calls switchkinsInit(), both exported with
*  the rest of the switchkins and kins_util interface, and motion reaches the
*  kinematics through the entry points exported here.
*
*  loadrt switchkins_core
*  loadrt [KINS]KINEMATICS
*/
#include <rtapi.h>
#include <rtapi_app.h>
#include <hal.h>

MODULE_LICENSE("GPL");

static int comp_id = -1;

int rtapi_app_main(void)
{
    comp_id = hal_init("switchkins_core");
    if (comp_id < 0) return comp_id;
    hal_ready(comp_id);
    return 0;
}

void rtapi_app_exit(void) { hal_exit(comp_id); }
