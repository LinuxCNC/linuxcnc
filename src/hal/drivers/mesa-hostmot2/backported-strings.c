//    Copyright 2005-2008, various authors
//
//    This program is free software; you can redistribute it and/or modify
//    it under the terms of the GNU General Public License as published by
//    the Free Software Foundation; either version 2 of the License, or
//    (at your option) any later version.
//
//    This program is distributed in the hope that it will be useful,
//    but WITHOUT ANY WARRANTY; without even the implied warranty of
//    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//    GNU General Public License for more details.
//
//    You should have received a copy of the GNU General Public License
//    along with this program; if not, write to the Free Software
//    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA

// 
// This file contains some handy string parsing functions lifted from Linux
// 2.6.27.44, from the files mm/util.c and lib/argv_split.c.
//
// It gets compiled into the hostmot2 driver if the kernel does not provide
// its own versions of these functions: argv_split(), argv_free(), and
// kstrndup().
//


#include <linux/kernel.h>
#include <linux/version.h>
//CS//#include <linux/ctype.h>
//CS//#include <linux/slab.h>
//CS//#include <linux/module.h>




#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,23)
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,14)
typedef unsigned long gfp_t;
char *kstrdup(const char *s, gfp_t gfp)
{
    size_t len = strlen(s)+1;
    char *r = kmalloc(len, gfp);
    if(r) memcpy(r, s, len);
    return r;
}
void *kzalloc(size_t sz, gfp_t gfp) {
    void *r = kmalloc(sz, gfp);
    if(r) memset(r, 0, sz);
    return r;
}
#endif

/**
 * kstrndup - allocate space for and copy an existing string
 * @s: the string to duplicate
 * @max: read at most @max chars from @s
 * @gfp: the GFP mask used in the kmalloc() call when allocating memory
 */
char *kstrndup(const char *s, size_t max, gfp_t gfp)
{
	size_t len;
	char *buf;

	if (!s)
		return NULL;

	len = strnlen(s, max);
	buf = kmalloc(len+1, gfp);
	if (buf) {
		memcpy(buf, s, len);
		buf[len] = '\0';
	}
	return buf;
}


/*
 * Helper function for splitting a string into an argv-like array.
 */

static const char *skip_sep(const char *cp)
{
	while (*cp && isspace(*cp))
		cp++;

	return cp;
}

static const char *skip_arg(const char *cp)
{
	while (*cp && !isspace(*cp))
		cp++;

	return cp;
}

static int count_argc(const char *str)
{
	int count = 0;

	while (*str) {
		str = skip_sep(str);
		if (*str) {
			count++;
			str = skip_arg(str);
		}
	}

	return count;
}

/**
 * argv_free - free an argv
 * @argv - the argument vector to be freed
 *
 * Frees an argv and the strings it points to.
 */
void argv_free(char **argv)
{
	char **p;
	for (p = argv; *p; p++)
		kfree(*p);

	kfree(argv);
}

/**
 * argv_split - split a string at whitespace, returning an argv
 * @gfp: the GFP mask used to allocate memory
 * @str: the string to be split
 * @argcp: returned argument count
 *
 * Returns an array of pointers to strings which are split out from
 * @str.  This is performed by strictly splitting on white-space; no
 * quote processing is performed.  Multiple whitespace characters are
 * considered to be a single argument separator.  The returned array
 * is always NULL-terminated.  Returns NULL on memory allocation
 * failure.
 */
char **argv_split(gfp_t gfp, const char *str, int *argcp)
{
	int argc = count_argc(str);
	char **argv = kzalloc(sizeof(*argv) * (argc+1), gfp);
	char **argvp;

	if (argv == NULL)
		goto out;

	if (argcp)
		*argcp = argc;

	argvp = argv;

	while (*str) {
		str = skip_sep(str);

		if (*str) {
			const char *p = str;
			char *t;

			str = skip_arg(str);

			t = kstrndup(p, str-p, gfp);
			if (t == NULL)
				goto fail;
			*argvp++ = t;
		}
	}
	*argvp = NULL;

  out:
	return argv;

  fail:
	argv_free(argv);
	return NULL;
}

#endif

