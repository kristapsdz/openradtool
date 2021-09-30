/*	$Id$ */
/*
 * Copyright (c) 2018--2019 Kristaps Dzonsons <kristaps@bsd.lv>
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

#if HAVE_SYS_QUEUE
# include <sys/queue.h>
#endif

#if HAVE_ERR
# include <err.h>
#endif
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "ort.h"
#include "ort-lang-xliff.h"

int
main(int argc, char *argv[])
{
	struct ort_lang_xliff	  args;
	FILE			**confs = NULL;
	FILE			 *defin = stdin;
	const char		 *deffname = "<stdin>";
	struct config		 *cfg = NULL;
#define	OP_EXTRACT		  0
#define	OP_JOIN			  1
#define	OP_UPDATE		 (-1)
	int			  c, op = OP_EXTRACT, rc = 0;
	size_t			  confsz = 0, i, j, xmlstart = 0, sz;
	struct msgq		  mq = TAILQ_HEAD_INITIALIZER(mq);

#if HAVE_PLEDGE
	if (pledge("stdio rpath", NULL) == -1)
		err(1, "pledge");
#endif

	memset(&args, 0, sizeof(struct ort_lang_xliff));

	while ((c = getopt(argc, argv, "cju")) != -1)
		switch (c) {
		case 'c':
			args.flags |= ORT_LANG_XLIFF_COPY;
			break;
		case 'j':
			op = OP_JOIN;
			break;
		case 'u':
			op = OP_UPDATE;
			break;
		default:
			goto usage;
		}

	argc -= optind;
	argv += optind;

	/*
	 * Open all of our files prior to the pledge(2), failing
	 * immediately if any open fails.
	 * This makes cleanup easier.
	 */

	if (op == OP_JOIN || op == OP_UPDATE) {
		sz = (size_t)argc;
		for (confsz = 0; confsz < sz; confsz++)
			if (strcmp(argv[confsz], "-x") == 0)
				break;

		/* If we have >2 w/o -x, error (which conf/xml?). */

		if (confsz == sz && sz > 2)
			goto usage;

		if ((i = confsz) < sz)
			i++;

		xmlstart = i;
		args.insz = sz - i;

		if (confsz == 0 && args.insz == 0)
			goto usage;

		/* If we have 2 w/o -x, it's old-new. */

		if (args.insz == 0 && sz == 2)
			args.insz = confsz = xmlstart = 1;

		if (OP_UPDATE == op && args.insz > 1)
			goto usage;

		args.fnames = (const char *const *)&argv[xmlstart];

		if (args.insz > 0 &&
		    (args.in = calloc(args.insz, sizeof(FILE *))) == NULL)
			err(1, "calloc");
		if (confsz > 0 &&
		    (confs = calloc(confsz, sizeof(FILE *))) == NULL)
			err(1, "calloc");

		for (i = 0; i < confsz; i++)
			if ((confs[i] = fopen(argv[i], "r")) == NULL)
				err(1, "%s", argv[i]);
		if (i < sz && 0 == strcmp(argv[i], "-x"))
			i++;
		for (j = 0; i < sz; j++, i++)
			if ((args.in[j] = fopen(argv[i], "r")) == NULL)
				err(1, "%s", argv[i]);

		/* Handle stdin case. */

		if (args.insz == 0) {
			args.insz = 1;
			args.fnames = &deffname;
			args.in = &defin;
		}
	} else {
		confsz = (size_t)argc;
		if (confsz > 0 &&
		    (confs = calloc(confsz, sizeof(FILE *))) == NULL)
			err(1, NULL);
		for (i = 0; i < confsz; i++)
			if ((confs[i] = fopen(argv[i], "r")) == NULL)
				err(1, "%s", argv[i]);
	}

#if HAVE_PLEDGE
	if (pledge("stdio", NULL) == -1)
		err(1, "pledge");
#endif
	if ((cfg = ort_config_alloc()) == NULL)
		err(1, NULL);

	for (i = 0; i < confsz; i++)
		if (!ort_parse_file(cfg, confs[i], argv[i]))
			goto out;

	if (confsz == 0 && !ort_parse_file(cfg, stdin, "<stdin>"))
		goto out;

	if (!ort_parse_close(cfg))
		goto out;

	/* Operate on the XLIFF/configuration file(s). */

	switch (op) {
	case OP_EXTRACT:
		rc = ort_lang_xliff_extract(&args, cfg, stdout, &mq);
		break;
	case OP_JOIN:
		rc = ort_lang_xliff_join(&args, cfg, stdout, &mq);
		break;
	case OP_UPDATE:
		rc = ort_lang_xliff_update(&args, cfg, stdout, &mq);
		break;
	}

	if (!rc)
		warn(NULL);
out:
	ort_write_msg_file(stderr, &cfg->mq);
	ort_write_msg_file(stderr, &mq);

	ort_msgq_free(&mq);
	ort_config_free(cfg);

	if (args.in == &defin) {
		args.in = NULL;
		args.insz = 0;
	}

	for (i = 0; i < args.insz; i++)
		fclose(args.in[i]);
	for (i = 0; i < confsz; i++)
		fclose(confs[i]);

	free(confs);
	free(args.in);
	return !rc;
usage:
	fprintf(stderr, 
		"usage: %s [-c] -j [config...] -x [xliff...]\n"
		"       %s [-c] -j config [xliff]\n"
		"       %s [-c] -u [config...] -x [xliff]\n"
		"       %s [-c] -u config [xliff]\n"
		"       %s [-c] [config...]\n",
		getprogname(), getprogname(), getprogname(),
		getprogname(), getprogname());
	return 1;
}
