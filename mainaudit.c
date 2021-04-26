/*	$Id$ */
/*
 * Copyright (c) 2021 Kristaps Dzonsons <kristaps@bsd.lv>
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

#include <assert.h>
#if HAVE_ERR
# include <err.h>
#endif
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "ort.h"

static size_t
audit_buf(const struct audit *a, char *b, size_t bsz, size_t i)
{
	int	 c;

	switch (a->type) {
	case AUDIT_INSERT:
		c = snprintf(b, bsz, "%s", a->st->name);
		break;
	case AUDIT_UPDATE:
		c = snprintf(b, bsz, "%s:%s", 
			a->up->parent->name, 
			a->up->name == NULL ? "-" : a->up->name);
		break;
	case AUDIT_QUERY:
		c = snprintf(b, bsz, "%s:%s",
			a->sr->parent->name,
			a->sr->name == NULL ? "-" : a->sr->name);
		break;
	case AUDIT_REACHABLE:
		if (i == a->ar.srsz) {
			c = snprintf(b, bsz, "%s", a->ar.st->name);
			break;
		}
		assert(i < a->ar.srsz);
		c = snprintf(b, bsz, "%s:%s:%s", 
			a->ar.st->name,
			a->ar.srs[i].sr->name == NULL ? 
			"-" : a->ar.srs[i].sr->name,
			a->ar.srs[i].path == NULL ? 
			"-" : a->ar.srs[i].path);
		break;
	}
	if (c < 0)
		err(1, NULL);
	return (size_t)c;
}

static void
audit_insert(const struct audit *a, char *b, size_t bsz)
{

	assert(a->st->ins != NULL);
	audit_buf(a, b, bsz, 0);
	printf("%-11s %-*s %s:%zu:%zu\n", "insert", (int)bsz, b,
		a->st->ins->pos.fname, a->st->ins->pos.line, 
		a->st->ins->pos.column);
}

static void
audit_update(const struct audit *a, char *b, size_t bsz)
{

	audit_buf(a, b, bsz, 0);
	printf("%-11s %-*s %s:%zu:%zu\n", a->up->type == UP_DELETE ?
		"delete" : "update", (int)bsz, b,
		a->up->pos.fname, a->up->pos.line, a->up->pos.column);
}

static void
audit_query(const struct audit *a, char *b, size_t bsz)
{

	audit_buf(a, b, bsz, 0);
	printf("%-11s %-*s %s:%zu:%zu\n", 
		a->sr->type == STYPE_COUNT ? "count" : 
			a->sr->type == STYPE_ITERATE ? "iterate" : 
			a->sr->type == STYPE_SEARCH ? "search" : 
			"list", (int)bsz, b, a->sr->pos.fname, 
		a->sr->pos.line, a->sr->pos.column);
}

static void
audit_reach(const struct audit *a, char *b, size_t bsz, int verb)
{
	size_t	 i;

	if (verb == 0) {
		audit_buf(a, b, bsz, a->ar.srsz);
		printf("%-11s %-*s %s:%zu:%zu\n", a->ar.exported ?
			"readwrite" : "read", (int)bsz, b, 
			a->ar.st->pos.fname, 
			a->ar.st->pos.line, 
			a->ar.st->pos.column);
		return;
	}

	for (i = 0; i < a->ar.srsz; i++) {
		audit_buf(a, b, bsz, i);
		printf("%-11s %-*s %s:%zu:%zu\n", a->ar.srs[i].exported ?
			"readwrite" : "read", (int)bsz, b, 
			a->ar.srs[i].sr->pos.fname, 
			a->ar.srs[i].sr->pos.line, 
			a->ar.srs[i].sr->pos.column);
	}
}

static void
audit_write(const struct auditq *aq, int verb)
{
	const struct audit	*a;
	size_t			 i, msz = 0, sz, asz;
	char			*b;

	TAILQ_FOREACH(a, aq, entries) {
		switch (a->type) {
		case AUDIT_REACHABLE:
			if (verb == 0) {
				sz = audit_buf(a, NULL, 0, a->ar.srsz);
				break;
			}
			for (i = 0; i < a->ar.srsz; i++) {
				asz = audit_buf(a, NULL, 0, i);
				if (asz > sz)
					sz = asz;
			}
			break;
		default:
			sz = audit_buf(a, NULL, 0, 0);
			break;
		}
		if (sz > msz)
			msz = sz;
	}

	if ((b = malloc(msz + 1)) == NULL)
		err(1, NULL);

	TAILQ_FOREACH(a, aq, entries)
		switch (a->type) {
		case AUDIT_INSERT:
			audit_insert(a, b, msz + 1);
			break;
		case AUDIT_UPDATE:
			audit_update(a, b, msz + 1);
			break;
		case AUDIT_QUERY:
			audit_query(a, b, msz + 1);
			break;
		case AUDIT_REACHABLE:
			audit_reach(a, b, msz + 1, verb);
			break;
		}

	free(b);
}

int
main(int argc, char *argv[])
{
	const char		 *role = "default";
	struct config		 *cfg = NULL;
	const struct role	 *r;
	int			  c, rc = 0, verb = 0;
	size_t			  i;
	FILE			**confs = NULL;
	struct auditq		 *aq = NULL;

#if HAVE_PLEDGE
	if (pledge("stdio rpath", NULL) == -1)
		err(1, "pledge");
#endif

	while ((c = getopt(argc, argv, "vr:")) != -1)
		switch (c) {
		case 'r':
			role = optarg;
			break;
		case 'v':
			verb = 1;
			break;
		default:
			goto usage;
		}

	argc -= optind;
	argv += optind;

	/* Read in all of our files now so we can repledge. */

	if (argc > 0 &&
	    (confs = calloc(argc, sizeof(FILE *))) == NULL)
		err(1, NULL);

	for (i = 0; i < (size_t)argc; i++)
		if ((confs[i] = fopen(argv[i], "r")) == NULL)
			err(1, "%s", argv[i]);

#if HAVE_PLEDGE
	if (pledge("stdio", NULL) == -1)
		err(1, "pledge");
#endif

	if ((cfg = ort_config_alloc()) == NULL)
		err(1, NULL);

	for (i = 0; i < (size_t)argc; i++)
		if (!ort_parse_file(cfg, confs[i], argv[i]))
			goto out;

	if (argc == 0 && !ort_parse_file(cfg, stdin, "<stdin>"))
		goto out;

	if (!ort_parse_close(cfg))
		goto out;

	if (TAILQ_EMPTY(&cfg->arq)) {
		warnx("roles not enabled");
		goto out;
	}

	TAILQ_FOREACH(r, &cfg->arq, allentries)
		if (strcasecmp(r->name, role) == 0)
			break;

	if (r == NULL) {
		warnx("role not found: %s", role);
		goto out;
	} else if ((aq = ort_audit(r, cfg)) == NULL) {
		warn(NULL);
		goto out;
	}

	audit_write(aq, verb);
	rc = 1;
out:
	ort_write_msg_file(stderr, &cfg->mq);
	ort_auditq_free(aq);
	ort_config_free(cfg);

	for (i = 0; i < (size_t)argc; i++)
		fclose(confs[i]);

	free(confs);
	return !rc;
usage:
	fprintf(stderr, "usage: %s [-v] [-r role] [config...]\n",
		getprogname());
	return 1;
}
