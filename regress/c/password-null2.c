/*	$Id$ */
/*
 * Copyright (c) 2020 Kristaps Dzonsons <kristaps@bsd.lv>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */
#include <sys/types.h>

#include <assert.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include <kcgi.h>
#include <kcgijson.h>

#include "password-null2.ort.h"

int
main(int argc, char *argv[])
{
	struct ort	*ort;
	int64_t		 id;
	const char	*fname;

	assert(argc == 2);
	fname = argv[1];

	if ((ort = db_open(fname)) == NULL)
		return 1;

	if ((id = db_foo_insert(ort, NULL)) == -1)
		return 1;

	if (db_foo_get_hash(ort, "password") != NULL)
		return 1;
	if (db_foo_get_nhash(ort, "shmassword") != NULL)
		return 1;
	if (db_foo_get_hash(ort, NULL) != NULL)
		return 1;
	if (db_foo_get_nhash(ort, NULL) != NULL)
		return 1;

	db_close(ort);
	return 0;
}
