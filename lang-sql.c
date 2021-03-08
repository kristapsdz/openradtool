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
#include <inttypes.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "ort.h"
#include "lang.h"
#include "ort-lang-sql.h"

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

static void gen_warnx(struct msgq *mq, 
	const struct pos *, const char *, ...)
	__attribute__((format(printf, 3, 4)));
static void diff_errx(struct msgq *mq, 
	const struct pos *, const struct pos *, const char *, ...)
	__attribute__((format(printf, 4, 5)));

static void
gen_warnx(struct msgq *mq, const struct pos *pos, const char *fmt, ...)
{
	va_list	 ap;

	va_start(ap, fmt);
	ort_msgv(mq, MSGTYPE_WARN, 0, pos, fmt, ap);
	va_end(ap);
}

static void
diff_errx(struct msgq *mq, const struct pos *posold, 
	const struct pos *posnew, const char *fmt, ...)
{
	va_list	 ap;
	char	 buf[1024];

	va_start(ap, fmt);
	vsnprintf(buf, sizeof(buf), fmt, ap);
	va_end(ap);
	ort_msg(mq, MSGTYPE_ERROR, 0, posold, "%s:%zu:%zu: %s",
		posnew->fname, posnew->line, posnew->column, 
		buf);
}

/*
 * Generate all PRAGMA prologue statements and sets "prol" if they've
 * been emitted or not.
 */
static int
gen_prologue(FILE *f, int *prol)
{

	if (*prol == 1)
		return 1;
	*prol = 1;
	return fputs("PRAGMA foreign_keys=ON;\n\n", f) != EOF;
}

/*
 * Generate the "UNIQUE" statements on this table.
 * Return zero on failure, non-zero on success.
 */
static int
gen_unique(FILE *f, const struct unique *n, int *first)
{
	struct nref	*ref;
	int		 ffirst = 1;

	if (fprintf(f, "%s\n\tUNIQUE(", *first ? "" : ",") < 0)
		return 0;

	TAILQ_FOREACH(ref, &n->nq, entries) {
		if (fprintf(f, "%s%s", 
		    ffirst ? "" : ", ", ref->field->name) < 0)
			return 0;
		ffirst = 0;
	}

	*first = 0;
	return fputc(')', f) != EOF;
}

/*
 * Generate the "FOREIGN KEY" statements on this table.
 * Return zero on failure, non-zero on success.
 */
static int
gen_fkeys(FILE *f, const struct field *fd, int *first)
{

	if (fd->type == FTYPE_STRUCT || fd->ref == NULL)
		return 1;

	if (fprintf(f, "%s\n\tFOREIGN KEY(%s) REFERENCES %s(%s)",
	    *first ? "" : ",",
	    fd->ref->source->name,
	    fd->ref->target->parent->name,
	    fd->ref->target->name) < 0)
		return 0;
	
	if (fd->actdel != UPACT_NONE &&
	    fprintf(f, " ON DELETE %s", upacts[fd->actdel]) < 0)
		return 0;
	if (fd->actup != UPACT_NONE &&
	    fprintf(f, " ON UPDATE %s", upacts[fd->actup]) < 0)
		return 0;

	*first = 0;
	return 1;
}

/*
 * Generate a column for this table.
 * Return zero on failure, non-zero on success.
 */
static int
gen_field(FILE *f, const struct field *fd, int *first, int comments)
{

	if (fd->type == FTYPE_STRUCT)
		return 1;

	if (fprintf(f, "%s\n", *first ? "" : ",") < 0)
		return 0;
	if (comments &&
	    !gen_comment(f, 1, COMMENT_SQL, fd->doc))
		return 0;
	if (fd->type == FTYPE_EPOCH || fd->type == FTYPE_DATE)
		if (!gen_comment(f, 1, COMMENT_SQL, 
		    "(Stored as a UNIX epoch value.)"))
			return 0;
	if (fprintf(f, "\t%s %s", fd->name, ftypes[fd->type]) < 0)
		return 0;
	if (fd->flags & FIELD_ROWID && fputs(" PRIMARY KEY", f) == EOF)
		return 0;
	if (fd->flags & FIELD_UNIQUE && fputs(" UNIQUE", f) == EOF)
		return 0;
	if (!(fd->flags & FIELD_ROWID) && 
	    !(fd->flags & FIELD_NULL) && fputs(" NOT NULL", f) == EOF)
		return 0;

	*first = 0;
	return 1;
}

/*
 * Generate a table and all of its components: fields, foreign keys, and
 * unique statements.
 */
static int
gen_struct(FILE *f, const struct strct *p, int comments)
{
	const struct field 	*fd;
	const struct unique 	*n;
	int	 		 first = 1;

	if (comments &&
	    !gen_comment(f, 0, COMMENT_SQL, p->doc))
		return 0;
	if (fprintf(f, "CREATE TABLE %s (", p->name) < 0)
		return 0;
	TAILQ_FOREACH(fd, &p->fq, entries)
		if (!gen_field(f, fd, &first, comments))
			return 0;
	TAILQ_FOREACH(fd, &p->fq, entries)
		if (!gen_fkeys(f, fd, &first))
			return 0;
	TAILQ_FOREACH(n, &p->nq, entries)
		if (!gen_unique(f, n, &first))
			return 0;
	return fputs("\n);\n\n", f) != EOF;
}

int
ort_lang_sql(const struct ort_lang_sql *args,
	const struct config *cfg, FILE *f)
{
	const struct strct *p;

	if (fputs("PRAGMA foreign_keys=ON;\n\n", f) == EOF)
		return 0;

	TAILQ_FOREACH(p, &cfg->sq, entries)
		if (!gen_struct(f, p, 1))
			return 0;

	return 1;
}

/*
 * This is the ALTER TABLE version of the field generators in
 * gen_struct().
 */
static int
gen_diff_field_new(FILE *f, const struct field *fd)
{
	int	 rc;

	if (fprintf(f, "ALTER TABLE %s ADD COLUMN %s %s",
	    fd->parent->name, fd->name, ftypes[fd->type]) < 0)
		return 0;

	if (fd->flags & FIELD_ROWID && fputs(" PRIMARY KEY", f) == EOF)
		return 0;
	if (fd->flags & FIELD_UNIQUE && fputs(" UNIQUE", f) == EOF)
		return 0;
	if (!(fd->flags & FIELD_ROWID) && !(fd->flags & FIELD_NULL) &&
	    fputs(" NOT NULL", f) == EOF)
		return 0;

	if (fd->ref != NULL && fprintf(f, " REFERENCES %s(%s)",
	    fd->ref->target->parent->name, fd->ref->target->name) < 0)
		return 0;

	if (fd->actup != UPACT_NONE &&
	    fprintf(f, " ON UPDATE %s", upacts[fd->actup]) < 0)
		return 0;
	if (fd->actdel != UPACT_NONE &&
	    fprintf(f, " ON DELETE %s", upacts[fd->actdel]) < 0)
		return 0;

	if (fd->flags & FIELD_HASDEF) {
		if (fputs(" DEFAULT ", f) == EOF)
			return 0;
		switch (fd->type) {
		case FTYPE_BIT:
		case FTYPE_BITFIELD:
		case FTYPE_DATE:
		case FTYPE_EPOCH:
		case FTYPE_INT:
			rc = fprintf(f, "%" PRId64, fd->def.integer);
			break;
		case FTYPE_REAL:
			rc = fprintf(f, "%g", fd->def.decimal);
			break;
		case FTYPE_EMAIL:
		case FTYPE_TEXT:
			rc = fprintf(f, "'%s'", fd->def.string);
			break;
		case FTYPE_ENUM:
			rc = fprintf(f, "%" PRId64, 
				fd->def.eitem->value);
			break;
		default:
			abort();
			break;
		}
		if (rc < 0)
			return 0;
	}

	return fputs(";\n", f) != EOF;
}

static size_t
gen_check_fields(struct msgq *mq, const struct diffq *q, int destruct)
{
	const struct diff	*d;
	const struct field	*f, *df;
	size_t	 		 errors = 0;
	unsigned int		 mask;

	mask = FIELD_ROWID | FIELD_NULL | FIELD_UNIQUE;

	TAILQ_FOREACH(d, q, entries) {
		switch (d->type) {
		case DIFF_DEL_FIELD:
			if (destruct || d->field->type == FTYPE_STRUCT)
				break;
			gen_warnx(mq, &d->field->pos, 
				"field column was dropped");
			errors++;
			break;
		case DIFF_MOD_FIELD_BITF:
		case DIFF_MOD_FIELD_ENM:
		case DIFF_MOD_FIELD_TYPE:
			f = d->field_pair.into;
			df = d->field_pair.from;
			diff_errx(mq, &df->pos, &f->pos, 
				"field type has changed");
			errors++;
			break;
		case DIFF_MOD_FIELD_FLAGS:
			f = d->field_pair.into;
			df = d->field_pair.from;

			/* We only care about SQL flags. */

			if ((f->flags & mask) == (df->flags & mask))
				break;
			diff_errx(mq, &df->pos, &f->pos, 
				"field flag has changed");
			errors++;
			break;
		case DIFF_MOD_FIELD_ACTIONS:
			f = d->field_pair.into;
			df = d->field_pair.from;
			diff_errx(mq, &df->pos, &f->pos, 
				"field action has changed");
			errors++;
			break;
		case DIFF_MOD_FIELD_REFERENCE:
			f = d->field_pair.into;
			df = d->field_pair.from;

			/* We only care about remote references. */

			if (f->type == FTYPE_STRUCT ||
			    df->type == FTYPE_STRUCT)
				break;
			diff_errx(mq, &df->pos, &f->pos, 
				"field reference has changed");
			errors++;
			break;
		default:
			break;
		}
	}

	return errors;
}

/*
 * See gen_check_enms().
 * Same but for the bitfield types.
 */
static size_t
gen_check_bitfs(struct msgq *mq, const struct diffq *q, int destruct)
{
	const struct diff	*d;
	size_t	 		 errors = 0;

	TAILQ_FOREACH(d, q, entries) {
		switch (d->type) {
		case DIFF_DEL_BITF:
			if (destruct)
				break;
			gen_warnx(mq, &d->bitf->pos, 
				"deleted bitfield");
			errors++;
			break;
		case DIFF_MOD_BITIDX_VALUE:
			diff_errx(mq, &d->bitidx_pair.from->pos, 
			  	&d->bitidx_pair.into->pos,
				"bitfield item has changed value");
			errors++;
			break;
		case DIFF_DEL_BITIDX:
			if (destruct)
				break;
			gen_warnx(mq, &d->bitidx->pos, 
				"deleted bitfield item");
			errors++;
			break;
		default:
			break;
		}
	}

	return errors;
}

/*
 * Compare enumeration object return the number of errors.
 * If "destruct" is non-zero, allow dropped enumerations and enumeration
 * items.
 */
static size_t
gen_check_enms(struct msgq *mq, const struct diffq *q, int destruct)
{
	const struct diff	*d;
	size_t	 		 errors = 0;

	TAILQ_FOREACH(d, q, entries) {
		switch (d->type) {
		case DIFF_DEL_ENM:
			if (destruct)
				break;
			gen_warnx(mq, &d->enm->pos, 
				"deleted enumeration");
			errors++;
			break;
		case DIFF_MOD_EITEM_VALUE:
			diff_errx(mq, &d->eitem_pair.from->pos, 
			  	&d->eitem_pair.into->pos,
				"item has changed value");
			errors++;
			break;
		case DIFF_DEL_EITEM:
			if (destruct)
				break;
			gen_warnx(mq, &d->eitem->pos, 
				"deleted enumeration item");
			errors++;
			break;
		default:
			break;
		}
	}

	return errors;
}

static size_t
gen_check_strcts(struct msgq *mq, const struct diffq *q, int destruct)
{
	const struct diff	*d;
	size_t			 errors = 0;

	TAILQ_FOREACH(d, q, entries) 
		switch (d->type) {
		case DIFF_DEL_STRCT:
			if (destruct)
				break;
			gen_warnx(mq, &d->strct->pos, "deleted table");
			errors++;
			break;
		default:
			break;
		}

	return errors;
}

static size_t
gen_check_uniques(struct msgq *mq, const struct diffq *q, int destruct)
{
	const struct diff	*d;
	size_t			 errors = 0;

	TAILQ_FOREACH(d, q, entries)
		switch (d->type) {
		case DIFF_ADD_UNIQUE:
			gen_warnx(mq, &d->unique->pos, "new unique field");
			errors++;
			break;
		default:
			break;
		}

	return errors;
}

/*
 * Generate an SQL diff.
 * This returns zero on failure, non-zero on success.
 * "Failure" means that there were irreconcilable errors between the two
 * configurations, such as new tables or removed colunms or some such.
 * If "destruct" is non-zero, this allows for certain modifications that
 * would change the database, such as dropping tables.
 */
int
ort_lang_diff_sql(const struct ort_lang_sql *args, 
	const struct diffq *q, int destruct, FILE *f, struct msgq *mq)
{
	const struct diff	*d;
	size_t	 		 errors = 0;
	int	 		 rc = 0, prol = 0;
	struct msgq		 tmpq = TAILQ_HEAD_INITIALIZER(tmpq);

	if (mq == NULL)
		mq = &tmpq;

	errors += gen_check_enms(mq, q, destruct);
	errors += gen_check_bitfs(mq, q, destruct);
	errors += gen_check_fields(mq, q, destruct);
	errors += gen_check_strcts(mq, q, destruct);
	errors += gen_check_uniques(mq, q, destruct);

	if (errors)
		goto out;

	rc = -1;

	TAILQ_FOREACH(d, q, entries)
		if (d->type == DIFF_ADD_STRCT) {
			if (!gen_prologue(f, &prol))
				goto out;
			if (!gen_struct(f, d->strct, 0))
				goto out;
		}

	TAILQ_FOREACH(d, q, entries)
		if (d->type == DIFF_ADD_FIELD) {
			if (!gen_prologue(f, &prol))
				goto out;
			if (!gen_diff_field_new(f, d->field))
				goto out;
		}

	TAILQ_FOREACH(d, q, entries) 
		if (d->type == DIFF_DEL_STRCT && destruct) {
			if (!gen_prologue(f, &prol))
				goto out;
			if (fprintf(f,
			    "DROP TABLE %s;\n", d->strct->name) < 0)
				goto out;
		}

	TAILQ_FOREACH(d, q, entries)
		if (d->type == DIFF_DEL_FIELD && destruct) {
			if (!gen_prologue(f, &prol))
				goto out;
			if (fprintf(f, 
			    "-- ALTER TABLE %s DROP COLUMN %s;\n", 
			    d->field->parent->name, 
			    d->field->name) < 0)
				goto out;
		}

	rc = 1;
out:
	if (mq == &tmpq)
		ort_msgq_free(mq);
	return rc;
}
