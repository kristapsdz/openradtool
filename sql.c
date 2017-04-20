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
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "extern.h"

static	const char *const ftypes[FTYPE__MAX] = {
	"INTEGER", /* FTYPE_INT */
	"REAL", /* FTYPE_REAL */
	"BLOB", /* FTYPE_BLOB */
	"TEXT", /* FTYPE_TEXT */
	"TEXT", /* FTYPE_PASSWORD */
	NULL, /* FTYPE_STRUCT */
};

static void gen_warnx(const struct pos *, const char *, ...)
	__attribute__((format(printf, 2, 3)));
static void diff_warnx(const struct pos *,
		const struct pos *, const char *, ...)
	__attribute__((format(printf, 3, 4)));

static void
gen_warnx(const struct pos *pos, const char *fmt, ...)
{
	va_list	 ap;
	char	 buf[1024];

	va_start(ap, fmt);
	vsnprintf(buf, sizeof(buf), fmt, ap);
	va_end(ap);

	fprintf(stderr, "%s:%zu:%zu: %s\n", 
		pos->fname, pos->line, pos->column, buf);
}

static void
diff_warnx(const struct pos *posold, 
	const struct pos *posnew, const char *fmt, ...)
{
	va_list	 ap;
	char	 buf[1024];

	va_start(ap, fmt);
	vsnprintf(buf, sizeof(buf), fmt, ap);
	va_end(ap);

	fprintf(stderr, "%s:%zu:%zu -> %s:%zu:%zu: %s\n", 
		posold->fname, posold->line, posold->column, 
		posnew->fname, posnew->line, posnew->column, 
		buf);
}

/*
 * Generate the "UNIQUE" statements on this table.
 */
static void
gen_unique(const struct unique *n, int *first)
{
	struct nref	*ref;
	int		 ffirst = 1;

	printf("%s\n\tUNIQUE(", *first ? "" : ",");

	TAILQ_FOREACH(ref, &n->nq, entries) {
		printf("%s%s", ffirst ? "" : ", ", ref->name);
		ffirst = 0;
	}
	putchar(')');
	*first = 0;
}

/*
 * Generate the "FOREIGN KEY" statements on this table.
 */
static void
gen_fkeys(const struct field *f, int *first)
{

	if (FTYPE_STRUCT == f->type || NULL == f->ref)
		return;

	printf("%s\n\tFOREIGN KEY(%s) REFERENCES %s(%s)",
		*first ? "" : ",",
		f->ref->source->name,
		f->ref->target->parent->name,
		f->ref->target->name);
	*first = 0;
}

/*
 * Generate the columns for this table.
 */
static void
gen_field(const struct field *f, int *first, int comments)
{

	if (FTYPE_STRUCT == f->type)
		return;

	printf("%s\n", *first ? "" : ",");
	if (comments)
		print_commentt(1, COMMENT_SQL, f->doc);
	printf("\t%s %s", f->name, ftypes[f->type]);
	if (FIELD_ROWID & f->flags)
		printf(" PRIMARY KEY");
	if (FIELD_UNIQUE & f->flags)
		printf(" UNIQUE");
	if ( ! (FIELD_ROWID & f->flags) &&
	     ! (FIELD_NULL & f->flags))
		printf(" NOT NULL");
	*first = 0;
}

/*
 * Generate a table and all of its components.
 */
static void
gen_struct(const struct strct *p, int comments)
{
	const struct field *f;
	const struct unique *n;
	int	 first = 1;

	if (comments)
		print_commentt(0, COMMENT_SQL, p->doc);

	printf("CREATE TABLE %s (", p->name);
	TAILQ_FOREACH(f, &p->fq, entries)
		gen_field(f, &first, comments);
	TAILQ_FOREACH(f, &p->fq, entries)
		gen_fkeys(f, &first);
	TAILQ_FOREACH(n, &p->nq, entries)
		gen_unique(n, &first);
	puts("\n);\n"
	     "");
}

void
gen_sql(const struct strctq *q)
{
	const struct strct *p;

	puts("PRAGMA foreign_keys=ON;\n"
	     "");

	TAILQ_FOREACH(p, q, entries)
		gen_struct(p, 1);
}

/*
 * Perform a variety of checks: the fields must have the
 * same type, flags (rowid, etc.), and references.
 * Returns zero on difference, non-zero on equality.
*/
static int
gen_diff_field(const struct field *f, const struct field *df)
{
	int	 rc = 1;

	if (f->type != df->type) {
		diff_warnx(&f->parent->pos, &df->parent->pos, 
			"type change from %s to %s",
			ftypes[df->type], ftypes[f->type]);
		rc = 0;
	} 

	if (f->flags != df->flags) {
		diff_warnx(&f->parent->pos, &df->parent->pos,
			"attribute change");
		rc = 0;
	}

	if ((NULL != f->ref && NULL == df->ref) ||
	    (NULL == f->ref && NULL != df->ref)) {
		diff_warnx(&f->parent->pos, &df->parent->pos,
			"foreign reference change");
		rc = 0;
	}

	if (NULL != f->ref && NULL != df->ref &&
	    (strcasecmp(f->ref->source->parent->name,
			df->ref->source->parent->name))) {
		diff_warnx(&f->parent->pos, &df->parent->pos,
			"foreign reference source change");
		rc = 0;
	}

	return(rc);
}

static int
gen_diff_fields_old(const struct strct *s, const struct strct *ds)
{
	const struct field *f, *df;
	size_t	 errors = 0;

	TAILQ_FOREACH(df, &ds->fq, entries) {
		TAILQ_FOREACH(f, &s->fq, entries)
			if (0 == strcasecmp(f->name, df->name))
				break;

		if (NULL == f && FTYPE_STRUCT == df->type) {
			gen_warnx(&df->pos, "old inner joined field");
		} else if (NULL == f) {
			gen_warnx(&df->pos, "column was dropped");
			errors++;
		} else if ( ! gen_diff_field(df, f))
			errors++;
	}

	return(errors ? 0 : 1);
}

static int
gen_diff_fields_new(const struct strct *s, const struct strct *ds)
{
	const struct field *f, *df;
	size_t	 count = 0, errors = 0;

	/*
	 * Structure in both new and old queues.
	 * Go through all fields in the new queue.
	 * If they're not found in the old queue, modify and add.
	 * Otherwise, make sure they're the same type and have the same
	 * references.
	 */

	TAILQ_FOREACH(f, &s->fq, entries) {
		TAILQ_FOREACH(df, &ds->fq, entries)
			if (0 == strcasecmp(f->name, df->name))
				break;

		/* 
		 * New "struct" fields are a no-op.
		 * Otherwise, have an ALTER TABLE clause.
		 */

		if (NULL == df && FTYPE_STRUCT == f->type) {
			gen_warnx(&f->pos, "new inner joined field");
		} else if (NULL == df) {
			printf("ALTER TABLE %s ADD COLUMN %s %s",
				f->parent->name, f->name, 
				ftypes[f->type]);
			if (FIELD_ROWID & f->flags)
				printf(" PRIMARY KEY");
			if (FIELD_UNIQUE & f->flags)
				printf(" UNIQUE");
			if ( ! (FIELD_ROWID & f->flags) &&
			     ! (FIELD_NULL & f->flags))
				printf(" NOT NULL");
			if (NULL != f->ref)
				printf(" REFERENCES %s(%s)",
					f->ref->target->parent->name,
					f->ref->target->name);
			puts(";");
			count++;
		} else if ( ! gen_diff_field(f, df))
			errors++;
	}

	return(errors ? -1 : count ? 1 : 0);
}

static int
gen_diff_uniques_new(const struct strct *s, const struct strct *ds)
{
	struct unique	*us, *uds;
	size_t		 errs = 0;

	TAILQ_FOREACH(us, &s->nq, entries) {
		TAILQ_FOREACH(uds, &ds->nq, entries)
			if (0 == strcasecmp(uds->cname, us->cname))
				break;
		if (NULL != uds) 
			continue;
		gen_warnx(&us->pos, "new unique fields");
		errs++;
	}
	return(0 == errs);
}

static int
gen_diff_uniques_old(const struct strct *s, const struct strct *ds)
{
	struct unique	*us, *uds;
	size_t		 errs = 0;

	TAILQ_FOREACH(uds, &ds->nq, entries) {
		TAILQ_FOREACH(us, &s->nq, entries)
			if (0 == strcasecmp(uds->cname, us->cname))
				break;
		if (NULL != us) 
			continue;
		gen_warnx(&uds->pos, "unique field disappeared");
		errs++;
	}

	return(0 == errs);
}

int
gen_diff(const struct strctq *sq, const struct strctq *dsq)
{
	const struct strct *s, *ds;
	size_t	 errors = 0;
	int	 rc;

	puts("PRAGMA foreign_keys=ON;\n"
	     "");

	/*
	 * Start by looking through all structures in the new queue and
	 * see if they exist in the old queue.
	 * If they don't exist in the old queue, we put out a CREATE
	 * TABLE for them.
	 * We do this first to handle ADD COLUMN dependencies.
	 */

	TAILQ_FOREACH(s, sq, entries) {
		TAILQ_FOREACH(ds, dsq, entries)
			if (0 == strcasecmp(s->name, ds->name))
				break;
		if (NULL == ds)
			gen_struct(s, 0);
	}

	/* 
	 * Now generate table differences.
	 * Do this afterward because we might reference the new tables
	 * that we've created.
	 * If the old configuration has different fields, exit with
	 * error.
	 */

	TAILQ_FOREACH(s, sq, entries) {
		TAILQ_FOREACH(ds, dsq, entries)
			if (0 == strcasecmp(s->name, ds->name))
				break;
		if (NULL == ds)
			continue;
		if ((rc = gen_diff_fields_new(s, ds)) < 0)
			errors++;
		else if (rc)
			puts("");
	}

	/*
	 * Now reverse and see if we should drop tables.
	 * Don't do this---just tell the user and return an error.
	 * Also see if there are old fields that are different.
	 */

	TAILQ_FOREACH(ds, dsq, entries) {
		TAILQ_FOREACH(s, sq, entries)
			if (0 == strcasecmp(s->name, ds->name))
				break;
		if (NULL == s) {
			gen_warnx(&ds->pos, "table was dropped");
			errors++;
		} else if ( ! gen_diff_fields_old(s, ds))
			errors++;
	}

	/*
	 * Test for old and new unique fields.
	 * We'll test both for newly added unique fields (all done by
	 * canonical name) and also lost uinque fields.
	 * Obviously this is only for matching table entries.
	 */

	TAILQ_FOREACH(s, sq, entries) 
		TAILQ_FOREACH(ds, dsq, entries) {
			if (strcasecmp(s->name, ds->name))
				continue;
			errors += ! gen_diff_uniques_new(s, ds);
		}
	TAILQ_FOREACH(ds, dsq, entries) 
		TAILQ_FOREACH(s, sq, entries) {
			if (strcasecmp(s->name, ds->name))
				continue;
			errors += ! gen_diff_uniques_old(s, ds);
		}

	return(errors ? 0 : 1);
}
