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

enum	op {
	OP_AUDIT,
	OP_NOOP,
	OP_DIFF,
	OP_C_HEADER,
	OP_C_SOURCE,
	OP_JAVASCRIPT,
	OP_SQL
};

int
main(int argc, char *argv[])
{
	FILE		*conf = NULL, *dconf = NULL;
	const char	*confile = NULL, *dconfile = NULL,
	      		*header = NULL, *incls = NULL, *role = NULL;
	struct config	*cfg, *dcfg = NULL;
	int		 c, rc = 1, json = 0, valids = 0,
			 splitproc = 0, dbin = 1, dstruct = 1;
	enum op		 op = OP_NOOP;

#if HAVE_PLEDGE
	if (-1 == pledge("stdio rpath", NULL))
		err(EXIT_FAILURE, "pledge");
#endif

	if (0 == strcmp(getprogname(), "kwebapp-c-header")) {
		op = OP_C_HEADER;
		while (-1 != (c = getopt(argc, argv, "jN:sv")))
			switch (c) {
			case ('j'):
				json = 1;
				break;
			case ('N'):
				dbin = NULL == strchr(optarg, 'b');
				dstruct = NULL == strchr(optarg, 'd');
				break;
			case ('s'):
				splitproc = 1;
				break;
			case ('v'):
				valids = 1;
				break;
			default:
				goto usage;
			}
		argc -= optind;
		argv += optind;
	} else if (0 == strcmp(getprogname(), "kwebapp-javascript")) {
		op = OP_JAVASCRIPT;
		if (-1 != getopt(argc, argv, ""))
			goto usage;
		argc -= optind;
		argv += optind;
	} else if (0 == strcmp(getprogname(), "kwebapp-sql")) {
		op = OP_SQL;
		if (-1 != getopt(argc, argv, ""))
			goto usage;
		argc -= optind;
		argv += optind;
	} else if (0 == strcmp(getprogname(), "kwebapp-audit")) {
		op = OP_AUDIT;
		while (-1 != (c = getopt(argc, argv, "j")))
			switch (c) {
			case ('j'):
				json = 1;
				break;
			default:
				goto usage;
			}
		argc -= optind;
		argv += optind;
		if (0 == argc)
			goto usage;
		role = argv[0];
		argv++;
		argc--;
	} else if (0 == strcmp(getprogname(), "kwebapp-sqldiff")) {
		op = OP_DIFF;
		if (-1 != getopt(argc, argv, ""))
			goto usage;
		argc -= optind;
		argv += optind;
		if (0 == argc)
			goto usage;
		dconfile = argv[0];
		argv++;
		argc--;
	} else if (0 == strcmp(getprogname(), "kwebapp-c-source")) {
		op = OP_C_SOURCE;
		while (-1 != (c = getopt(argc, argv, "h:I:jN:sv")))
			switch (c) {
			case ('h'):
				header = optarg;
				break;
			case ('I'):
				incls = optarg;
				break;
			case ('j'):
				json = 1;
				break;
			case ('N'):
				dbin = NULL == strchr(optarg, 'b');
				break;
			case ('s'):
				splitproc = 1;
				break;
			case ('v'):
				valids = 1;
				break;
			default:
				goto usage;
			}
		argc -= optind;
		argv += optind;
	} else {
		if (-1 != getopt(argc, argv, ""))
			goto usage;
		argc -= optind;
		argv += optind;
	}

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

	cfg = parse_config(conf, confile);
	fclose(conf);

	if (NULL != dconf) {
		dcfg = parse_config(dconf, dconfile);
		fclose(dconf);
	}

	/*
	 * After parsing, we need to link.
	 * Linking connects foreign key references.
	 * Do this for both the main and (if applicable) diff
	 * configuration files.
	 */

	if ((NULL == cfg || ! parse_link(cfg)) ||
	    (NULL != dconfile && (NULL == dcfg || ! parse_link(dcfg)))) {
		parse_free(cfg);
		parse_free(dcfg);
		return(EXIT_FAILURE);
	}

	/* Finally, (optionally) generate output. */

	if (OP_C_SOURCE == op)
		gen_c_source(cfg, json, valids, 
			splitproc, dbin, header, incls);
	else if (OP_C_HEADER == op)
		gen_c_header(cfg, json, valids, 
			splitproc, dbin, dstruct);
	else if (OP_SQL == op)
		gen_sql(&cfg->sq);
	else if (OP_DIFF == op)
		rc = gen_diff(cfg, dcfg);
	else if (OP_JAVASCRIPT == op)
		gen_javascript(cfg);
	else if (OP_AUDIT == op)
		rc = gen_audit(cfg, json, role);

	parse_free(cfg);
	return(rc ? EXIT_SUCCESS : EXIT_FAILURE);

usage:
	if (OP_C_SOURCE == op)
		fprintf(stderr, 
			"usage: %s "
			"[-jsv] "
			"[-h header[,header...] "
			"[-I bjv] "
			"[-N b] "
			"[config]\n",
			getprogname());
	else if (OP_C_HEADER == op)
		fprintf(stderr, 
			"usage: %s "
			"[-jsv] "
			"[-N bd] "
			"[config]\n",
			getprogname());
	else if (OP_SQL == op)
		fprintf(stderr, 
			"usage: %s [config]\n",
			getprogname());
	else if (OP_DIFF == op)
		fprintf(stderr, 
			"usage: %s oldconfig [config]\n",
			getprogname());
	else if (OP_AUDIT == op)
		fprintf(stderr, 
			"usage: %s "
		 	"[-j] "
			"role [config]\n",
			getprogname());
	else if (OP_JAVASCRIPT == op)
		fprintf(stderr, 
			"usage: %s [config]\n",
			getprogname());
	else
		fprintf(stderr, 
			"usage: %s [config]\n",
			getprogname());
	return(EXIT_FAILURE);
}
