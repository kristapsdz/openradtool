/*	$Id$ */
/*
 * Copyright (c) 2017--2019 Kristaps Dzonsons <kristaps@bsd.lv>
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
#include <inttypes.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "ort.h"
#include "lang.h"

static	const char *const realtypes[FTYPE__MAX] = {
	"int", /* FTYPE_BIT */
	"date", /* FTYPE_DATE */
	"epoch", /* FTYPE_EPOCH */
	"int", /* FTYPE_INT */
	"real", /* FTYPE_REAL */
	"blob", /* FTYPE_BLOB */
	"text", /* FTYPE_TEXT */
	"password", /* FTYPE_PASSWORD */
	"email", /* FTYPE_EMAIL */
	"struct", /* FTYPE_STRUCT */
	"enum", /* FTYPE_ENUM */
	"bitfield", /* FTYPE_BITFIELD */
};

static	const char *const upacts[UPACT__MAX] = {
	"NO ACTION", /* UPACT_NONE */
	"RESTRICT", /* UPACT_RESTRICT */
	"SET NULL", /* UPACT_NULLIFY */
	"CASCADE", /* UPACT_CASCADE */
	"SET DEFAULT", /* UPACT_DEFAULT */
};

static	const char *const ftypes[FTYPE__MAX] = {
	"INTEGER", /* FTYPE_BIT */
	"INTEGER", /* FTYPE_DATE */
	"INTEGER", /* FTYPE_EPOCH */
	"INTEGER", /* FTYPE_INT */
	"REAL", /* FTYPE_REAL */
	"BLOB", /* FTYPE_BLOB */
	"TEXT", /* FTYPE_TEXT */
	"TEXT", /* FTYPE_PASSWORD */
	"TEXT", /* FTYPE_EMAIL */
	NULL, /* FTYPE_STRUCT */
	"INTEGER", /* FTYPE_ENUM */
	"INTEGER", /* FTYPE_BITFIELD */
};

/* Forward declarations to get __attribute__ bits. */

static void gen_warnx(const struct pos *, const char *, ...)
	__attribute__((format(printf, 2, 3)));
static void diff_warnx(const struct pos *,
		const struct pos *, const char *, ...)
	__attribute__((format(printf, 3, 4)));
static void diff_errx(const struct pos *,
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
diff_errx(const struct pos *posold, 
	const struct pos *posnew, const char *fmt, ...)
{
	va_list	 ap;
	char	 buf[1024];

	va_start(ap, fmt);
	vsnprintf(buf, sizeof(buf), fmt, ap);
	va_end(ap);

	fprintf(stderr, "%s:%zu:%zu -> %s:%zu:%zu: error: %s\n", 
		posold->fname, posold->line, posold->column, 
		posnew->fname, posnew->line, posnew->column, 
		buf);
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

	fprintf(stderr, "%s:%zu:%zu -> %s:%zu:%zu: warning: %s\n", 
		posold->fname, posold->line, posold->column, 
		posnew->fname, posnew->line, posnew->column, 
		buf);
}

/*
 * Generate all PRAGMA prologue statements and sets "prol" if they've
 * been emitted or not.
 */
static void
gen_prologue(int *prol)
{

	if (*prol == 1)
		return;
	puts("PRAGMA foreign_keys=ON;\n");
	*prol = 1;
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
		printf("%s%s", ffirst ? "" : ", ",
			ref->field->name);
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

	if (f->type == FTYPE_STRUCT || f->ref == NULL)
		return;

	printf("%s\n\tFOREIGN KEY(%s) REFERENCES %s(%s)",
		*first ? "" : ",",
		f->ref->source->name,
		f->ref->target->parent->name,
		f->ref->target->name);
	
	if (UPACT_NONE != f->actdel)
		printf(" ON DELETE %s", upacts[f->actdel]);
	if (UPACT_NONE != f->actup)
		printf(" ON UPDATE %s", upacts[f->actup]);

	*first = 0;
}

/*
 * Generate the columns for this table.
 */
static void
gen_field(const struct field *f, int *first, int comments)
{

	if (f->type == FTYPE_STRUCT)
		return;

	printf("%s\n", *first ? "" : ",");
	if (comments)
		print_commentt(1, COMMENT_SQL, f->doc);
	if (f->type == FTYPE_EPOCH || f->type == FTYPE_DATE)
		print_commentt(1, COMMENT_SQL, 
			"(Stored as a UNIX epoch value.)");
	printf("\t%s %s", f->name, ftypes[f->type]);
	if (FIELD_ROWID & f->flags)
		printf(" PRIMARY KEY");
	if (FIELD_UNIQUE & f->flags)
		printf(" UNIQUE");
	if (!(FIELD_ROWID & f->flags) &&
	    !(FIELD_NULL & f->flags))
		printf(" NOT NULL");
	*first = 0;
}

/*
 * Generate a table and all of its components: fields, foreign keys, and
 * unique statements.
 */
static void
gen_struct(const struct strct *p, int comments)
{
	const struct field 	*f;
	const struct unique 	*n;
	int	 		 first = 1;

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

static void
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

	/*
	 * Type change.
	 * We might change semantic types (e.g., enum to int), which is
	 * not technically wrong because we'd still be an INTEGER, but
	 * might have changed input expectations.
	 * Thus, we warn in those cases, and error otherwise.
	 */

	if (f->type != df->type) {
		if ((FTYPE_DATE == f->type ||
		     FTYPE_EPOCH == f->type ||
		     FTYPE_INT == f->type ||
		     FTYPE_BIT == f->type ||
		     FTYPE_ENUM == f->type ||
		     FTYPE_BITFIELD == f->type) &&
		    (FTYPE_DATE == df->type ||
		     FTYPE_EPOCH == df->type ||
		     FTYPE_INT == df->type ||
		     FTYPE_BIT == df->type ||
		     FTYPE_ENUM == df->type ||
		     FTYPE_BITFIELD == df->type)) {
			diff_warnx(&f->pos, &df->pos, 
				"change between integer "
				"alias types: %s to %s",
				realtypes[f->type], 
				realtypes[df->type]);
		} else if ((f->type == FTYPE_TEXT &&
			    df->type == FTYPE_EMAIL) ||
			   (f->type == FTYPE_EMAIL &&
			    df->type == FTYPE_TEXT)) {
			diff_warnx(&f->pos, &df->pos, 
				"change between text "
				"alias types: %s to %s",
				realtypes[f->type], 
				realtypes[df->type]);
		} else { 
			diff_errx(&f->pos, &df->pos, 
				"type change: %s to %s",
				realtypes[f->type], 
				realtypes[df->type]);
			rc = 0;
		}
	} 

	/* Only care about SQL-specific field differences. */

	if ((f->flags & FIELD_ROWID) != (df->flags & FIELD_ROWID) ||
	    (f->flags & FIELD_NULL) != (df->flags & FIELD_NULL) ||
	    (f->flags & FIELD_UNIQUE) != (df->flags & FIELD_UNIQUE)) {
		diff_errx(&f->pos, &df->pos, "attribute change");
		rc = 0;
	}

	if (f->actdel != df->actdel) {
		diff_errx(&f->pos, &df->pos, "delete action change");
		rc = 0;
	}
	if (f->actup != df->actup) {
		diff_errx(&f->pos, &df->pos, "update action change");
		rc = 0;
	}

	if ((NULL != f->ref && NULL == df->ref) ||
	    (NULL == f->ref && NULL != df->ref)) {
		diff_errx(&f->pos, &df->pos, 
			"foreign reference change");
		rc = 0;
	}

	if (NULL != f->ref && NULL != df->ref &&
	    (strcasecmp(f->ref->source->parent->name,
			df->ref->source->parent->name))) {
		diff_errx(&f->pos, &df->pos,
			"foreign reference source change");
		rc = 0;
	}

	return rc;
}

/*
 * Compare all fields in the old "ds" with the new "s" and see if any
 * columns have been added or removed.
 * Returns the number of errors (or zero).
 */
static size_t
gen_diff_fields_old(const struct strct *s, 
	const struct strct *ds, int destruct)
{
	const struct field *f, *df;
	size_t	 errors = 0;

	TAILQ_FOREACH(df, &ds->fq, entries) {
		TAILQ_FOREACH(f, &s->fq, entries)
			if (0 == strcasecmp(f->name, df->name))
				break;

		if (NULL == f && FTYPE_STRUCT == df->type) {
			gen_warnx(&df->pos, "old inner joined field");
		} else if (NULL == f && destruct) {
			/*
			 * TODO: we don't have a way to drop columns in
			 * sqlite3: the "approved" way is to do the
			 * whole rename, create, copy.
			 * Meanwhile, if we're in destruct mode, just
			 * ignore it as an error, since the column will
			 * simply go away.
			 * Of course, not if we re-add it later...
			 */
			printf("-- ALTER TABLE %s DROP COLUMN %s;\n", 
				df->parent->name, df->name);
		} else if (NULL == f) {
			gen_warnx(&df->pos, "column was dropped");
			errors++;
		} else if ( ! gen_diff_field(df, f))
			errors++;
	}

	return errors;
}

static int
gen_diff_fields_new(const struct strct *s, 
	const struct strct *ds, int *prologue)
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
			gen_prologue(prologue);
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
			if (UPACT_NONE != f->actup)
				printf(" ON UPDATE %s", 
					upacts[f->actup]);
			if (UPACT_NONE != f->actdel)
				printf(" ON DELETE %s", 
					upacts[f->actdel]);
			if (FIELD_HASDEF & f->flags) {
				switch (f->type) {
				case FTYPE_BIT:
				case FTYPE_BITFIELD:
				case FTYPE_DATE:
				case FTYPE_EPOCH:
				case FTYPE_INT:
					printf(" DEFAULT %" PRId64,
						f->def.integer);
					break;
				case FTYPE_REAL:
					printf(" DEFAULT %g",
						f->def.decimal);
					break;
				case FTYPE_EMAIL:
				case FTYPE_TEXT:
					printf(" DEFAULT '%s'",
						f->def.string);
					break;
				case FTYPE_ENUM:
					printf(" DEFAULT %" PRId64,
						f->def.eitem->value);
					break;
				default:
					abort();
					break;
				}
			}
			puts(";");
			count++;
		} else if ( ! gen_diff_field(f, df))
			errors++;
	}

	return(errors ? -1 : count ? 1 : 0);
}

/*
 * See if all of the fields in "u" are found in one of the unique
 * statements in "os".
 * Return zero if not found, non-zero if found.
 */
static int
gen_diff_uniques(const struct unique *u, const struct strct *os)
{
	const struct unique	*ou;
	const struct nref	*nf, *onf;
	size_t			 sz, osz;

	sz = 0;
	TAILQ_FOREACH(nf, &u->nq, entries)
		sz++;

	TAILQ_FOREACH(ou, &os->nq, entries) {
		TAILQ_FOREACH(nf, &u->nq, entries) {
			osz = 0;
			TAILQ_FOREACH(onf, &ou->nq, entries)
				osz++;
			if (osz != sz)
				break;
			TAILQ_FOREACH(onf, &ou->nq, entries)
				if (strcasecmp(onf->field->name,
				    nf->field->name) == 0)
					break;
			if (onf == NULL)
				break;
		}
		if (nf == NULL)
			return 1;
	}

	return 0;
}

/*
 * See if all the uniques in the new structure "s" may be found in the
 * old structure "ds".
 * A new unique field is an error because the existing data might
 * violate the unique constraint.
 */
static int
gen_diff_uniques_new(const struct strct *s, const struct strct *ds)
{
	const struct unique	*u;
	size_t		 	 errs = 0;

	TAILQ_FOREACH(u, &s->nq, entries) {
		if (gen_diff_uniques(u, ds))
			continue;
		gen_warnx(&u->pos, "new unique fields: existing "
			"data might violate these constraints");
		errs++;
	}

	return errs == 0;
}

/*
 * Look for old unique constraints that we've dropped.
 * This is only a warning because it means we're relaxing an existing
 * unique situation.
 */
static void
gen_diff_uniques_old(const struct strct *s, const struct strct *ds)
{
	const struct unique	*u;

	TAILQ_FOREACH(u, &ds->nq, entries) {
		if (gen_diff_uniques(u, s))
			continue;
		gen_warnx(&u->pos, "unique fields have disappeared");
	}
}

/*
 * See gen_diff_enums().
 * Same but for the bitfield types.
 */
static size_t
gen_diff_bits(const struct config *cfg,
	const struct config *dcfg, int destruct)
{
	const struct bitf *b, *db;
	const struct bitidx *bi, *dbi;
	size_t	 errors = 0;

	TAILQ_FOREACH(b, &cfg->bq, entries) {
		TAILQ_FOREACH(db, &dcfg->bq, entries)
			if (strcasecmp(b->name, db->name) == 0)
				break;
		if (db == NULL) {
			gen_warnx(&b->pos, "new bitfield");
			continue;
		}
		
		/* Compare current to old bit indexes. */

		TAILQ_FOREACH(bi, &b->bq, entries) {
			TAILQ_FOREACH(dbi, &db->bq, entries)
				if (strcasecmp(bi->name, dbi->name) == 0)
					break;
			if (dbi != NULL && bi->value != dbi->value) {
				diff_errx(&bi->pos, &dbi->pos,
					"item has changed value");
				errors++;
			} else if (dbi == NULL)
				gen_warnx(&bi->pos, "new item");
		}

		/* 
		 * Compare existence of old to current indexes. 
		 * This constitutes a deletion if the item is lost, so
		 * only report it as an error if we're not letting
		 * through deletions.
		 */

		TAILQ_FOREACH(dbi, &db->bq, entries) {
			TAILQ_FOREACH(bi, &b->bq, entries)
				if (strcasecmp(bi->name, dbi->name) == 0)
					break;
			if (bi != NULL)
				continue;
			gen_warnx(&dbi->pos, "lost old item");
			if (!destruct)
				errors++;
		}
	}

	/*
	 * Now we've compared bitfields that are new or already exist
	 * between the two, so make sure we didn't lose any.
	 * Report it as an error only if we're not allowing deletions.
	 */

	TAILQ_FOREACH(db, &dcfg->bq, entries) {
		TAILQ_FOREACH(b, &cfg->bq, entries)
			if (strcasecmp(b->name, db->name) == 0)
				break;
		if (b != NULL)
			continue;
		gen_warnx(&db->pos, "lost old bitfield");
		if (!destruct)
			errors++;
	}

	return errors;
}

/*
 * Compare the enumeration objects in both files.
 * This does the usual check of new <-> old, then old -> new.
 * Returns the number of errors.
 * If "destruct" is non-zero, allow dropped enumerations and enumeration
 * items.
 */
static size_t
gen_diff_enums(const struct config *cfg,
	const struct config *dcfg, int destruct)
{
	const struct enm *e, *de;
	const struct eitem *ei, *dei;
	size_t	 errors = 0;

	/*
	 * First, compare the current to the old enumerations.
	 * For enumerations found in both, error on disparities.
	 */

	TAILQ_FOREACH(e, &cfg->eq, entries) {
		TAILQ_FOREACH(de, &dcfg->eq, entries)
			if (strcasecmp(e->name, de->name) == 0)
				break;
		if (de == NULL) {
			gen_warnx(&e->pos, "new enumeration");
			continue;
		}
		
		/* Compare current to old entries. */

		TAILQ_FOREACH(ei, &e->eq, entries) {
			TAILQ_FOREACH(dei, &de->eq, entries)
				if (strcasecmp(ei->name, dei->name) == 0)
					break;
			if (dei != NULL && ei->value != dei->value) {
				diff_errx(&ei->pos, &dei->pos,
					"item has changed value");
				errors++;
			} else if (dei == NULL)
				gen_warnx(&ei->pos, "new item");
		}

		/* 
		 * Compare existence of old to current entries. 
		 * This constitutes a deletion if the item is lost, so
		 * only report it as an error if we're not letting
		 * through deletions.
		 */

		TAILQ_FOREACH(dei, &de->eq, entries) {
			TAILQ_FOREACH(ei, &e->eq, entries)
				if (strcasecmp(ei->name, dei->name) == 0)
					break;
			if (ei != NULL)
				continue;
			gen_warnx(&dei->pos, "lost old item");
			if (!destruct)
				errors++;
		}
	}

	/*
	 * Now we've compared enumerations that are new or already exist
	 * between the two, so make sure we didn't lose any.
	 * Report it as an error only if we're not allowing deletions.
	 */

	TAILQ_FOREACH(de, &dcfg->eq, entries) {
		TAILQ_FOREACH(e, &cfg->eq, entries)
			if (strcasecmp(e->name, de->name) == 0)
				break;
		if (e != NULL)
			continue;
		gen_warnx(&de->pos, "lost old enumeration");
		if (!destruct)
			errors++;
	}

	return errors;
}

/*
 * Generate an SQL diff with "cfg" being the new, "dfcg" being the old.
 * This returns zero on failure, non-zero on success.
 * "Failure" means that there were irreconcilable errors between the two
 * configurations, such as new tables or removed colunms or some such.
 * If "destruct" is non-zero, this allows for certain modifications that
 * would change the database, such as dropping tables.
 */
static int
gen_diff(const struct config *cfg, 
	const struct config *dcfg, int destruct)
{
	const struct strct *s, *ds;
	size_t	 errors = 0;
	int	 rc, prol = 0;

	errors += gen_diff_enums(cfg, dcfg, destruct);
	errors += gen_diff_bits(cfg, dcfg, destruct);

	/*
	 * Start by looking through all structures in the new queue and
	 * see if they exist in the old queue.
	 * If they don't exist in the old queue, we put out a CREATE
	 * TABLE for them.
	 * We do this first to handle ADD COLUMN dependencies.
	 */

	TAILQ_FOREACH(s, &cfg->sq, entries) {
		TAILQ_FOREACH(ds, &dcfg->sq, entries)
			if (strcasecmp(s->name, ds->name) == 0)
				break;
		if (ds == NULL) {
			gen_prologue(&prol);
			gen_struct(s, 0);
		}
	}

	/* 
	 * Now generate table differences.
	 * Do this afterward because we might reference the new tables
	 * that we've created.
	 * If the old configuration has different fields, exit with
	 * error.
	 */

	TAILQ_FOREACH(s, &cfg->sq, entries) {
		TAILQ_FOREACH(ds, &dcfg->sq, entries)
			if (0 == strcasecmp(s->name, ds->name))
				break;
		if (NULL == ds)
			continue;
		if ((rc = gen_diff_fields_new(s, ds, &prol)) < 0)
			errors++;
		else if (rc)
			puts("");
	}

	/*
	 * Now reverse and see if we should drop tables.
	 * Don't do this---just tell the user and return an error.
	 * Also see if there are old fields that are different.
	 */

	TAILQ_FOREACH(ds, &dcfg->sq, entries) {
		TAILQ_FOREACH(s, &cfg->sq, entries)
			if (0 == strcasecmp(s->name, ds->name))
				break;
		if (NULL == s && destruct) {
			printf("DROP TABLE %s;\n", ds->name);
		} else if (s == NULL) {
			gen_warnx(&ds->pos, "table was dropped");
			errors++;
		} else
			errors += gen_diff_fields_old(s, ds, destruct);
	}

	/*
	 * Test for old and new unique fields.
	 * We'll test both for newly added unique fields (all done by
	 * canonical name) and also lost uinque fields.
	 * Obviously this is only for matching table entries.
	 */

	TAILQ_FOREACH(s, &cfg->sq, entries) 
		TAILQ_FOREACH(ds, &dcfg->sq, entries) {
			if (strcasecmp(s->name, ds->name))
				continue;
			errors += !gen_diff_uniques_new(s, ds);
		}
	TAILQ_FOREACH(ds, &dcfg->sq, entries) 
		TAILQ_FOREACH(s, &cfg->sq, entries) {
			if (strcasecmp(s->name, ds->name))
				continue;
			gen_diff_uniques_old(s, ds);
		}

	return errors ? 0 : 1;
}

int
main(int argc, char *argv[])
{
	FILE		**confs = NULL, **dconfs = NULL;
	struct config	 *cfg = NULL, *dcfg = NULL;
	int		  rc = 0, diff = 0, c, destruct = 0;
	size_t		  confsz = 0, dconfsz = 0, i, j, 
			  confst = 0;

#if HAVE_PLEDGE
	if (pledge("stdio rpath", NULL) == -1)
		err(EXIT_FAILURE, "pledge");
#endif

	if (strcmp(getprogname(), "ort-sql") == 0) {
		if (getopt(argc, argv, "") != -1)
			goto usage;

		argc -= optind;
		argv += optind;

		confsz = (size_t)argc;
		if (confsz > 0 &&
		    (confs = calloc(confsz, sizeof(FILE *))) == NULL)
			err(EXIT_FAILURE, "calloc");

		for (i = 0; i < confsz; i++)
			if ((confs[i] = fopen(argv[i], "r")) == NULL)
				err(EXIT_FAILURE, "%s", argv[i]);
	} else if (strcmp(getprogname(), "ort-sqldiff") == 0) {
		diff = 1;
		while ((c = getopt(argc, argv, "d")) != -1)
			switch (c) {
			case 'd':
				destruct = 1;
				break;
			default:
				goto usage;
			}

		argc -= optind;
		argv += optind;

		/* Read up until "-f" (or argc) for old configs. */

		for (dconfsz = 0; dconfsz < (size_t)argc; dconfsz++)
			if (strcmp(argv[dconfsz], "-f") == 0)
				break;
		
		/* If we have >2 w/o -f, error (which old/new?). */

		if (dconfsz == (size_t)argc && argc > 2)
			goto usage;

		if ((i = dconfsz) < (size_t)argc)
			i++;

		confst = i;
		confsz = argc - i;

		/* If we have 2 w/o -f, it's old-new. */

		if (0 == confsz && 2 == argc)
			confsz = dconfsz = confst = 1;

		/* Need at least one configuration. */

		if (confsz + dconfsz == 0)
			goto usage;

		if (confsz > 0 &&
		    (confs = calloc(confsz, sizeof(FILE *))) == NULL)
			err(EXIT_FAILURE, "calloc");
		if (dconfsz > 0 &&
		    (dconfs = calloc(dconfsz, sizeof(FILE *))) == NULL)
			err(EXIT_FAILURE, "calloc");

		for (i = 0; i < dconfsz; i++)
			if ((dconfs[i] = fopen(argv[i], "r")) == NULL)
				err(EXIT_FAILURE, "%s", argv[i]);
		if (i < (size_t)argc && strcmp(argv[i], "-f") == 0)
			i++;
		for (j = 0; i < (size_t)argc; j++, i++) {
			assert(j < confsz);
			if ((confs[j] = fopen(argv[i], "r")) == NULL)
				err(EXIT_FAILURE, "%s", argv[i]);
		}
	}

#if HAVE_PLEDGE
	if (pledge("stdio", NULL) == -1)
		err(EXIT_FAILURE, "pledge");
#endif

	assert(confsz + dconfsz > 0 || ! diff);

	if ((cfg = ort_config_alloc()) == NULL ||
	    (dcfg = ort_config_alloc()) == NULL)
		goto out;

	/* Parse the input files themselves. */

	for (i = 0; i < confsz; i++)
		if (!ort_parse_file(cfg, confs[i], argv[confst + i]))
			goto out;
	for (i = 0; i < dconfsz; i++)
		if (!ort_parse_file(dcfg, dconfs[i], argv[i]))
			goto out;

	/* If we don't have input, parse from stdin. */

	if (confsz == 0 && !ort_parse_file(cfg, stdin, "<stdin>"))
		goto out;
	if (dconfsz == 0 && diff &&
	    !ort_parse_file(dcfg, stdin, "<stdin>"))
		goto out;

	/* Linker. */

	if (!ort_parse_close(cfg))
		goto out;
	if (diff && !ort_parse_close(dcfg))
		goto out;
	
	/* Generate output. */

	if (!diff) {
		gen_sql(&cfg->sq);
		rc = 1;
	} else 
		rc = gen_diff(cfg, dcfg, destruct);

out:
	for (i = 0; i < confsz; i++)
		if (fclose(confs[i]) == EOF)
			warn("%s", argv[confst + i]);
	for (i = 0; i < dconfsz; i++)
		if (fclose(dconfs[i]) == EOF)
			warn("%s", argv[i]);
	free(confs);
	free(dconfs);
	ort_config_free(cfg);
	ort_config_free(dcfg);

	return rc ? EXIT_SUCCESS : EXIT_FAILURE;

usage:
	if (!diff) {
		fprintf(stderr, 
			"usage: %s [config...]\n", getprogname());
		return EXIT_FAILURE;
	}

	fprintf(stderr, 
		"usage: %s [-d] oldconfig [config...]\n"
		"       %s [-d] [oldconfig...] -f [config...]\n",
		getprogname(), getprogname());
	return EXIT_FAILURE;
}
