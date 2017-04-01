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
#include <unistd.h>

#include "extern.h"

enum	op {
	OP_NOOP,
	OP_HEADER,
	OP_SOURCE,
	OP_SQL
};

int
main(int argc, char *argv[])
{
	FILE		*conf;
	const char	*confile = NULL;
	struct strctq	*sq;
	int		 c;
	enum op		 op = OP_HEADER;

#if HAVE_PLEDGE
	if (-1 == pledge("stdio rpath", NULL))
		err(EXIT_FAILURE, "pledge");
#endif

	while (-1 != (c = getopt(argc, argv, "chns")))
		switch (c) {
		case ('c'):
			op = OP_SOURCE;
			break;
		case ('h'):
			op = OP_HEADER;
			break;
		case ('n'):
			op = OP_NOOP;
			break;
		case ('s'):
			op = OP_SQL;
			break;
		default:
			goto usage;
		}

	argc -= optind;
	argv += optind;

	if (0 == argc) {
		confile = "<stdin>";
		conf = stdin;
	} else if (argc > 1)
		goto usage;

	confile = argv[0];
	if (NULL == (conf = fopen(confile, "r")))
		err(EXIT_FAILURE, "%s", confile);

#if HAVE_PLEDGE
	if (-1 == pledge("stdio", NULL))
		err(EXIT_FAILURE, "pledge");
#endif

	/*
	 * First, parse the file.
	 * This pulls all of the data from the configuration file.
	 * If there are any errors, it will return NULL.
	 */

	sq = parse_config(conf, confile);
	fclose(conf);

	/*
	 * After parsing, we need to link.
	 * Linking connects foreign key references.
	 */

	if (NULL == sq || ! parse_link(sq)) {
		parse_free(sq);
		return(EXIT_FAILURE);
	}

	/* Finally, (optionally) generate output. */

	if (OP_SOURCE == op)
		gen_source(sq);
	else if (OP_HEADER == op)
		gen_header(sq);
	else if (OP_SQL == op)
		gen_sql(sq);

	parse_free(sq);
	return(EXIT_SUCCESS);

usage:
	fprintf(stderr, "usage: %s [-chns] [config]\n",
		getprogname());
	return(EXIT_FAILURE);
}
