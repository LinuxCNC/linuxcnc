/*
 * This is a component for hostmot2 board drivers
 * Copyright (c) 2013,2014,2020,2024 Michael Geszkiewicz <micges@wp.pl>,
 *    Jeff Epler <jepler@unpythonic.net>
 *    B.Stultiens <lcnc@vagrearg.org>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the Free
 * Software Foundation; either version 2 of the License, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program; if not, see <https://www.gnu.org/licenses/>.
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdarg.h>
#include <errno.h>
#include <sys/wait.h>

#include <rtapi.h>

int shell(char *command)
{
	char *const argv[] = {"sh", "-c", command, NULL};
	pid_t pid;
	int res = rtapi_spawn_as_root(&pid, "/bin/sh", NULL, NULL, argv, environ);
	if(res < 0)
		perror("rtapi_spawn_as_root");
	int status;
	waitpid(pid, &status, 0);
	if(WIFEXITED(status))
		return WEXITSTATUS(status);
	else if(WIFSTOPPED(status))
		return WTERMSIG(status)+128;
	return status;
}

int eshellf(const char *errpfx, const char *fmt, ...)
{
	char commandbuf[1024];
	va_list ap;
	va_start(ap, fmt);
	vsnprintf(commandbuf, sizeof(commandbuf), fmt, ap);
	va_end(ap);

	int res = shell(commandbuf);
	if(res == EXIT_SUCCESS)
		return 0;

	rtapi_print_msg(RTAPI_MSG_ERR, "%s: ERROR: Failed to execute '%s'\n", errpfx ? errpfx : "eshellf()", commandbuf);
	return -EINVAL;
}

/* vim: ts=4
 */
