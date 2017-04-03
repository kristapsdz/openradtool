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

#include "extern.h"

static	const char *const ftypes[FTYPE__MAX] = {
	"INTEGER",
	"TEXT",
	NULL,
};

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

static void
gen_field(const struct field *f, int *first)
{

	if (FTYPE_STRUCT == f->type)
		return;

	printf("%s\n", *first ? "" : ",");

	gen_comment(f->doc, 1, NULL, "-- ", NULL);

	switch (f->type) {
	case (FTYPE_INT):
		printf("\t%s %s", f->name, ftypes[FTYPE_INT]);
		if (FIELD_ROWID & f->flags)
			printf(" PRIMARY KEY");
		break;
	case (FTYPE_TEXT):
		printf("\t%s %s", f->name, ftypes[FTYPE_TEXT]);
		break;
	default:
		break;
	}

	*first = 0;
}

static void
gen_struct(const struct strct *p)
{
	const struct field *f;
	int	 first = 1;

	gen_comment(p->doc, 0, NULL, "-- ", NULL);

	printf("CREATE TABLE %s (", p->name);
	TAILQ_FOREACH(f, &p->fq, entries)
		gen_field(f, &first);
	TAILQ_FOREACH(f, &p->fq, entries)
		gen_fkeys(f, &first);
	puts("\n);\n"
	     "");
}

void
gen_sql(const struct strctq *q)
{
	const struct strct *p;

	TAILQ_FOREACH(p, q, entries)
		gen_struct(p);
}

static size_t
gen_diff_fields(const struct strct *s, const struct strct *ds)
{
	const struct field *f, *df;
	size_t	 count = 0;

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
			warnx("%s.%s: new inner joined field",
				f->parent->name, f->name);
			continue;
		} else if (NULL == df) {
			printf("ALTER TABLE %s ADD COLUMN %s %s;\n",
				f->parent->name, f->name, 
				ftypes[f->type]);
			count++;
			/* FIXME: foreign key? */
			continue;
		} else if (df->type != f->type) {
			warnx("%s.%s: type change from %s to %s",
				f->parent->name, f->name,
				ftypes[df->type],
				ftypes[f->type]);
			continue;
		}
	}

	return(count);
}

void
gen_diff(const struct strctq *sq, const struct strctq *dsq)
{
	const struct strct *s, *ds;

	/*
	 * Start by looking through all structures in the new queue and
	 * see if they exist in the old queue.
	 * If they don't exist in the old queue, we put out a CREATE
	 * TABLE for them.
	 * If they do, then look through them field by field.
	 */

	TAILQ_FOREACH(s, sq, entries) {
		TAILQ_FOREACH(ds, dsq, entries)
			if (0 == strcasecmp(s->name, ds->name))
				break;
		if (NULL == ds) {
			/* Structure in "sq" does not exist in "dsq. */
			gen_struct(s);
			continue;
		}
		if (gen_diff_fields(s, ds))
			puts("");
	}

	TAILQ_FOREACH(ds, dsq, entries) {
		TAILQ_FOREACH(s, sq, entries)
			if (0 == strcasecmp(s->name, ds->name))
				break;
		if (NULL == s) {
		}
	}
}
