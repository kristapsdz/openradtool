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
	OP_DIFF,
	OP_HEADER,
	OP_SOURCE,
	OP_SQL
};

int
main(int argc, char *argv[])
{
	FILE		*conf = NULL, *dconf = NULL;
	const char	*confile = NULL, *dconfile = NULL,
	      		*header = NULL;
	struct strctq	*sq, *dsq = NULL;
	int		 c, rc = 1;
	enum op		 op = OP_HEADER;

#if HAVE_PLEDGE
	if (-1 == pledge("stdio rpath", NULL))
		err(EXIT_FAILURE, "pledge");
#endif

	while (-1 != (c = getopt(argc, argv, "c:d:hns")))
		switch (c) {
		case ('c'):
			op = OP_SOURCE;
			header = optarg;
			break;
		case ('d'):
			op = OP_DIFF;
			dconfile = optarg;
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
	} else if (argc > 1) {
		goto usage;
	} else
		confile = argv[0];

	if (NULL == conf &&
	    NULL == (conf = fopen(confile, "r")))
		err(EXIT_FAILURE, "%s", confile);

	if (NULL != dconfile && 
	    NULL == (dconf = fopen(dconfile, "r")))
		err(EXIT_FAILURE, "%s", dconfile);

#if HAVE_PLEDGE
	if (-1 == pledge("stdio", NULL))
		err(EXIT_FAILURE, "pledge");
#endif

	/*
	 * First, parse the file.
	 * This pulls all of the data from the configuration file.
	 * If there are any errors, it will return NULL.
	 * Also parse the "diff" configuration, if it exists.
	 */

	sq = parse_config(conf, confile);
	fclose(conf);

	if (NULL != dconf) {
		dsq = parse_config(dconf, dconfile);
		fclose(dconf);
	}

	/*
	 * After parsing, we need to link.
	 * Linking connects foreign key references.
	 * Do this for both the main and (if applicable) diff
	 * configuration files.
	 */

	if ((NULL == sq || ! parse_link(sq)) ||
	    (NULL != dconfile && (NULL == dsq || ! parse_link(dsq)))) {
		parse_free(sq);
		parse_free(dsq);
		return(EXIT_FAILURE);
	}

	/* Finally, (optionally) generate output. */

	if (OP_SOURCE == op)
		gen_source(sq, header);
	else if (OP_HEADER == op)
		gen_header(sq);
	else if (OP_SQL == op)
		gen_sql(sq);
	else if (OP_DIFF == op)
		rc = gen_diff(sq, dsq);

	parse_free(sq);
	return(rc ? EXIT_SUCCESS : EXIT_FAILURE);

usage:
	fprintf(stderr, 
		"usage: %s "
		"[-hns] "
		"[-c header] "
		"[-d config] "
		"[config]\n",
		getprogname());
	return(EXIT_FAILURE);
}
