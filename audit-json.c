/*	$Id$ */
/*
 * Copyright (c) 2017--2018 Kristaps Dzonsons <kristaps@bsd.lv>
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

static	const char *const modtypes[MODTYPE__MAX] = {
	"cat", /* MODTYPE_CONCAT */
	"dec", /* MODTYPE_DEC */
	"inc", /* MODTYPE_INC */
	"set", /* MODTYPE_SET */
	"strset", /* MODTYPE_STRSET */
};

static	const char *const stypes[STYPE__MAX] = {
	"count", /* STYPE_COUNT */
	"get", /* STYPE_SEARCH */
	"list", /* STYPE_LIST */
	"iterate", /* STYPE_ITERATE */
};

static	const char *const optypes[OPTYPE__MAX] = {
	"eq", /* OPTYPE_EQUAL */
	"ge", /* OPTYPE_GE */
	"gt", /* OPTYPE_GT */
	"le", /* OPTYPE_LE */
	"lt", /* OPTYPE_LT */
	"neq", /* OPTYPE_NEQUAL */
	"like", /* OPTYPE_LIKE */
	"and", /* OPTYPE_AND */
	"or", /* OPTYPE_OR */
	"streq", /* OPTYPE_STREQ */
	"strneq", /* OPTYPE_STRNEQ */
	/* Unary types... */
	"isnull", /* OPTYPE_ISNULL */
	"notnull", /* OPTYPE_NOTNULL */
};

static	const char *const utypes[UP__MAX] = {
	"update", /* UP_MODIFY */
	"delete", /* UP_DELETE */
};

static size_t
print_name_db_insert(const struct strct *p)
{
	int	 rc;

	return (rc = printf("db_%s_insert", p->name)) > 0 ?
		(size_t)rc : 0;
}

static size_t
print_name_db_search(const struct search *s)
{
	const struct sent *sent;
	size_t		   sz = 0;
	int	 	   rc;

	rc = printf("db_%s_%s", s->parent->name, stypes[s->type]);
	sz += rc > 0 ? (size_t)rc : 0;

	if (s->name == NULL && !TAILQ_EMPTY(&s->sntq)) {
		sz += (rc = printf("_by")) > 0 ? (size_t)rc : 0;
		TAILQ_FOREACH(sent, &s->sntq, entries) {
			rc = printf("_%s_%s", 
				sent->uname, optypes[sent->op]);
			sz += rc > 0 ? (size_t)rc : 0;
		}
	} else if (s->name != NULL)
		sz += (rc = printf("_%s", s->name)) > 0 ?
			(size_t)rc : 0;

	return sz;
}

static size_t
print_name_db_update(const struct update *u)
{
	const struct uref *ur;
	size_t	 	   col = 0;
	int		   rc;

	rc = printf("db_%s_%s", u->parent->name, utypes[u->type]);
	col = rc > 0 ? (size_t)rc : 0;

	if (u->name == NULL && u->type == UP_MODIFY) {
		if (!(u->flags & UPDATE_ALL))
			TAILQ_FOREACH(ur, &u->mrq, entries) {
				rc = printf("_%s_%s", 
					ur->field->name, 
					modtypes[ur->mod]);
				col += rc > 0 ? (size_t)rc : 0;
			}
		if (!TAILQ_EMPTY(&u->crq)) {
			col += (rc = printf("_by")) > 0 ?
				(size_t)rc : 0;
			TAILQ_FOREACH(ur, &u->crq, entries) {
				rc = printf("_%s_%s", 
					ur->field->name, 
					optypes[ur->op]);
				col += rc > 0 ? (size_t)rc : 0;
			}
		}
	} else if (u->name == NULL) {
		if (!TAILQ_EMPTY(&u->crq)) {
			col += (rc = printf("_by")) > 0 ?
				(size_t)rc : 0;
			TAILQ_FOREACH(ur, &u->crq, entries) {
				rc = printf("_%s_%s", 
					ur->field->name, 
					optypes[ur->op]);
				col += rc > 0 ? (size_t)rc : 0;
			}
		}
	} else 
		col += (rc = printf("_%s", u->name)) > 0 ?
			(size_t)rc : 0;

	return col;
}

static void
gen_audit_exportable(const struct strct *p, const struct audit *a,
	const struct role *r)
{
	const struct field 	*f;
	size_t			 i;

	printf("\t\t\t\"exportable\": %s,\n"
	       "\t\t\t\"data\": [\n", 
	       a->ar.exported ? "true" : "false");

	TAILQ_FOREACH(f, &p->fq, entries)
		printf("\t\t\t\t\"%s\"%s\n", f->name, 
		       TAILQ_NEXT(f, entries) != NULL ?  "," : "");

	puts("\t\t\t],\n"
	     "\t\t\t\"accessfrom\": [");

	for (i = 0; i < a->ar.srsz; i++) {
		printf("\t\t\t\t{ \"function\": \"");
		print_name_db_search(a->ar.srs[i].sr);
		printf("\",\n"
		       "\t\t\t\t  \"exporting\": %s,\n"
		       "\t\t\t\t  \"path\": \"%s\" }%s\n",
		       a->ar.srs[i].exported ? "true" : "false",
		       a->ar.srs[i].path == NULL ? 
		       	"" : a->ar.srs[i].path,
		       i < a->ar.srsz - 1 ? "," : "");
	}

	puts("\t\t\t],");
}

static void
print_doc(const char *cp)
{
	char	 c;

	if (NULL == cp) {
		printf("null");
		return;
	}

	putchar('"');

	while ('\0' != (c = *cp++))
		switch (c) {
		case ('\b'):
			fputs("\\b", stdout);
			break;
		case ('\f'):
			fputs("\\f", stdout);
			break;
		case ('\n'):
			fputs("\\n", stdout);
			break;
		case ('\r'):
			fputs("\\r", stdout);
			break;
		case ('\t'):
			fputs("\\t", stdout);
			break;
		default:
			if ((unsigned char)c < 32)
				printf("\\u%.4u", c);
			else
				fputc(c, stdout);
			break;
		}

	putchar('"');
}

static void
gen_audit_inserts(const struct strct *p, const struct auditq *aq)
{
	const struct audit	*a;

	printf("\t\t\t\"insert\": ");

	TAILQ_FOREACH(a, aq, entries)
		if (a->type == AUDIT_INSERT && a->st == p) {
			putchar('"');
			print_name_db_insert(p);
			puts("\",");
			return;
		}

	puts("null,");
}

static void
gen_audit_deletes(const struct strct *p, const struct auditq *aq)
{
	const struct audit	*a;
	int	 		 first = 1;

	printf("\t\t\t\"delete\": [");

	TAILQ_FOREACH(a, aq, entries)
		if (a->type == AUDIT_UPDATE &&
		    a->up->parent == p &&
		    a->up->type == UP_DELETE) {
			printf("%s\n\t\t\t\t\"", first ? "" : ",");
			print_name_db_update(a->up);
			putchar('"');
			first = 0;
		}

	puts("],");
}

static void
gen_audit_updates(const struct strct *p, const struct auditq *aq)
{
	const struct audit	*a;
	int			 first = 1;

	printf("\t\t\t\"update\": [");

	TAILQ_FOREACH(a, aq, entries)
		if (a->type == AUDIT_UPDATE &&
		    a->up->parent == p &&
		    a->up->type == UP_MODIFY) {
			printf("%s\n\t\t\t\t\"", first ? "" : ",");
			print_name_db_update(a->up);
			putchar('"');
			first = 0;
		}

	puts("],");
}

static void
gen_protos_insert(const struct strct *s, int *first)
{

	printf("%s\n\t\t\"", *first ? "" : ",");
	print_name_db_insert(s);
	fputs("\": {\n\t\t\t\"doc\": null,\n"
		"\t\t\t\"type\": \"insert\" }", stdout);
	*first = 0;
}

static void
gen_protos_updates(const struct update *u, int *first)
{

	printf("%s\n\t\t\"", *first ? "" : ",");
	print_name_db_update(u);
	fputs("\": {\n\t\t\t\"doc\": ", stdout);
	print_doc(u->doc);
	printf(",\n\t\t\t\"type\": \"%s\" }", utypes[u->type]);
	*first = 0;
}

static void
gen_protos_queries(const struct search *s, int *first)
{

	printf("%s\n\t\t\"", *first ? "" : ",");
	print_name_db_search(s);
	fputs("\": {\n\t\t\t\"doc\": ", stdout);
	print_doc(s->doc);
	printf(",\n\t\t\t\"type\": \"%s\" }", stypes[s->type]);
	*first = 0;
}

static void
gen_protos_fields(const struct audit *a, int *first)
{
	size_t	 i;

	for (i = 0; i < a->ar.fdsz; i++) {
		printf("%s\n\t\t\"%s.%s\": {\n"
		       "\t\t\t\"export\": %s,\n"
		       "\t\t\t\"doc\": ",
			*first ? "" : ",",
			a->ar.fds[i].fd->parent->name, 
			a->ar.fds[i].fd->name, 
			a->ar.fds[i].exported ? "true" : "false");
		print_doc(a->ar.fds[i].fd->doc);
		printf(" }");
		*first = 0;
	}
}

static void
gen_audit_queries(const struct strct *p, const struct auditq *aq,
	enum stype t, const char *tp)
{
	const struct audit	*a;
	int			 first = 1;

	printf("\t\t\t\"%s\": [", tp);

	TAILQ_FOREACH(a, aq, entries)
		if (a->type == AUDIT_QUERY &&
		    a->sr->parent == p &&
		    a->sr->type == t) {
			printf("%s\n\t\t\t\t\"", first ? "" : ",");
			print_name_db_search(a->sr);
			putchar('"');
			first = 0;
		}

	/* Last item: don't have a trailing comma. */

	printf("]%s\n", t != (STYPE__MAX - 1) ? "," : "");
}

static void
gen_audit_json(const struct config *cfg, const struct auditq *aq,
	const struct role *r, int standalone)
{
	const struct strct	*s;
	const struct audit	*a;
	int	 		 first;
	enum stype		 st;

	if (standalone)
		fputs("(function(root) {\n"
		     " 'use strict';\n"
		     " var audit = ", stdout);

	printf("{\n"
	       "\t\"role\": \"%s\",\n"
	       "\t\"doc\": ", r->name);
	print_doc(r->doc);
	puts(",\n\t\"access\": [");

	TAILQ_FOREACH(s, &cfg->sq, entries) {
		printf("\t\t{ \"name\": \"%s\",\n"
		       "\t\t  \"access\": {\n", s->name);

		TAILQ_FOREACH(a, aq, entries)
			if (a->type == AUDIT_REACHABLE &&
			    a->ar.st == s) {
				gen_audit_exportable(s, a, r);
				break;
			}

		gen_audit_inserts(s, aq);
		gen_audit_updates(s, aq);
		gen_audit_deletes(s, aq);
		for (st = 0; st < STYPE__MAX; st++)
			gen_audit_queries(s, aq, st, stypes[st]);
		printf("\t\t}}%s\n", 
			TAILQ_NEXT(s, entries) != NULL ? "," : "");
	}

	fputs("\t],\n"
	      "\t\"functions\": {", stdout);

	first = 1;
	TAILQ_FOREACH(a, aq, entries) 
		switch (a->type) {
		case AUDIT_UPDATE:
			gen_protos_updates(a->up, &first);
			break;
		case AUDIT_QUERY:
			gen_protos_queries(a->sr, &first);
			break;
		case AUDIT_INSERT:
			gen_protos_insert(a->st, &first);
			break;
		default:
			break;
		}

	puts("\n\t},\n"
	     "\t\"fields\": {");

	first = 1;
	TAILQ_FOREACH(a, aq, entries)
		if (a->type == AUDIT_REACHABLE)
			gen_protos_fields(a, &first);

	puts("\n\t}");

	if (standalone)
		puts(" };\n"
		     " root.audit = audit;\n"
		     "})(this);");
	else
		puts("}");

}

int
main(int argc, char *argv[])
{
	const char		 *role = "default";
	struct config		 *cfg = NULL;
	const struct role	 *r;
	int			  c, rc = 0, standalone = 0;
	size_t			  i;
	FILE			**confs = NULL;
	struct auditq		 *aq = NULL;

#if HAVE_PLEDGE
	if (pledge("stdio rpath", NULL) == -1)
		err(EXIT_FAILURE, "pledge");
#endif

	while ((c = getopt(argc, argv, "sr:")) != -1)
		switch (c) {
		case 's':
			standalone = 1;
			break;
		case 'r':
			role = optarg;
			break;
		default:
			goto usage;
		}

	argc -= optind;
	argv += optind;

	/* Read in all of our files now so we can repledge. */

	if (argc > 0 &&
	    (confs = calloc((size_t)argc, sizeof(FILE *))) == NULL)
		err(EXIT_FAILURE, NULL);

	for (i = 0; i < (size_t)argc; i++)
		if ((confs[i] = fopen(argv[i], "r")) == NULL)
			err(EXIT_FAILURE, "%s", argv[i]);

#if HAVE_PLEDGE
	if (pledge("stdio", NULL) == -1)
		err(EXIT_FAILURE, "pledge");
#endif

	if ((cfg = ort_config_alloc()) == NULL)
		err(EXIT_FAILURE, NULL);

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

	gen_audit_json(cfg, aq, r, standalone);
	rc = 1;
out:
	ort_write_msg_file(stderr, &cfg->mq);
	ort_auditq_free(aq);
	ort_config_free(cfg);

	for (i = 0; i < (size_t)argc; i++)
		fclose(confs[i]);

	free(confs);
	return rc ? EXIT_SUCCESS : EXIT_FAILURE;
usage:
	fprintf(stderr, "usage: %s [-r role] [config...]\n",
		getprogname());
	return EXIT_FAILURE;
}
