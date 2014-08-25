/*
 * Copyright (C) 2013-2014 Michael Haberler <license@mah.priv.at>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include "webtalk.hh"


void
echo_thread(void *args, zctx_t *ctx, void *pipe)
{
    wtconf_t *conf = (wtconf_t *) args;

    void *rs = zsocket_new (ctx, ZMQ_ROUTER);
    assert(rs);
    zsocket_bind(rs, "inproc://echo");

    while (1) {
	zmsg_t *rx = zmsg_recv(rs);
	if (!rx) {
	    perror("echo rx");
	    break;
	}
	if (conf->debug) {
	    fprintf (stderr, "-------- echo ----------\n");
	    zframe_t *frame = zmsg_first (rx);
	    while (frame) {
		zframe_fprint (frame, NULL, stderr);
		frame = zmsg_next (rx);
	    }
	}
	zmsg_send(&rx, rs);
    }
}
