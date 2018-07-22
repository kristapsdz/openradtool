/*	$Id$ */
/*
 * Copyright (c) 2017 Kristaps Dzonsons <kristaps@bsd.lv>
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
#include "config.h"

#include <sys/queue.h>

#if HAVE_ERR
# include <err.h>
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "extern.h"

int
main(int argc, char *argv[])
{
	FILE		*conf = NULL;
	struct config	*cfg;
	size_t		 i;

#if HAVE_PLEDGE
	if (-1 == pledge("stdio rpath", NULL))
		err(EXIT_FAILURE, "pledge");
#endif

	if (-1 != getopt(argc, argv, ""))
		goto usage;

	argc -= optind;
	argv += optind;

	if (0 == argc) {
		cfg = parse_config(stdin, "<stdin>");
		if (NULL == cfg)
			return EXIT_FAILURE;
		if ( ! parse_link(cfg)) {
			parse_free(cfg);
			return EXIT_FAILURE;
		}
		parse_free(cfg);
	} 

	for (i = 0; i < (size_t)argc; i++)  {
	    	conf = fopen(argv[i], "r");
		if (NULL == conf)
			err(EXIT_FAILURE, "%s", argv[i]);
		cfg = parse_config(conf, argv[i]);
		fclose(conf);
		if (NULL == cfg)
			return EXIT_FAILURE;
		if ( ! parse_link(cfg)) {
			parse_free(cfg);
			return EXIT_FAILURE;
		}
		parse_free(cfg);
	}

	return EXIT_SUCCESS;

usage:
	fprintf(stderr, 
		"usage: %s [config]\n",
		getprogname());
	return EXIT_FAILURE;
}
