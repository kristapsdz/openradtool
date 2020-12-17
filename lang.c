/*	$Id$ */
/*
 * Copyright (c) 2017, 2019 Kristaps Dzonsons <kristaps@bsd.lv>
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
#include <ctype.h>
#if HAVE_ERR
# include <err.h>
#endif
#include <inttypes.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ort.h"
#include "lang.h"

#define	MAXCOLS 70

/*
 * SQL operators.
 * Some of these binary, some of these are unary.
 * Use the OPTYPE_ISUNARY or OPTYPE_ISBINARY macro to determine where
 * within the expressions this should sit.
 */
static	const char *const optypes[OPTYPE__MAX] = {
	"=", /* OPTYPE_EQUAL */
	">=", /* OPTYPE_GE */
	">", /* OPTYPE_GT */
	"<=", /* OPTYPE_LE */
	"<", /* OPTYPE_LT */
	"!=", /* OPTYPE_NEQUAL */
	"LIKE", /* OPTYPE_LIKE */
	"&", /* OPTYPE_AND */
	"|", /* OPTYPE_OR */
	"=", /* OPTYPE_STREQ */
	"!=", /* OPTYPE_STRNEQ */
	/* Unary types... */
	"ISNULL", /* OPTYPE_ISNULL */
	"NOTNULL", /* OPTYPE_NOTNULL */
};


/*
 * Generate a (possibly) multi-line comment with "tabs" number of
 * preceding tab spaces.
 * This uses the standard comment syntax as seen in this comment itself.
 * FIXME: make sure that text we're ommitting doesn't clobber the
 * comment epilogue, like having star-slash in the text of a C comment.
 */
static int
print_comment(FILE *f, const char *doc, size_t tabs, 
	const char *pre, const char *in, const char *post)
{
	const char	*cp, *cpp;
	size_t		 i, curcol, maxcol;
	char		 last = '\0';

	assert(in != NULL);

	/*
	 * Maximum number of columns we show is MAXCOLS (utter maximum)
	 * less the number of tabs prior, which we'll match to 4 spaces.
	 */

	maxcol = (tabs >= 4) ? 40 : MAXCOLS - (tabs * 4);

	/* Emit the starting clause. */

	if (pre != NULL) {
		for (i = 0; i < tabs; i++)
			if (fputc('\t', f) == EOF)
				return 0;
		if (fprintf(f, "%s\n", pre) < 0)
			return 0;
	}

	/* Emit the documentation matter. */

	if (doc != NULL) {
		for (i = 0; i < tabs; i++)
			if (fputc('\t', f) == EOF)
				return 0;
		if (fputs(in, f) == EOF)
			return 0;

		for (curcol = 0, cp = doc; *cp != '\0'; cp++) {
			/*
			 * Newline check.
			 * If we're at a newline, then emit the leading
			 * in-comment marker.
			 */

			if (*cp == '\n') {
				if (fputc('\n', f) == EOF)
					return 0;
				for (i = 0; i < tabs; i++)
					if (fputc('\t', f) == EOF)
						return 0;
				if (fputs(in, f) == EOF)
					return 0;
				last = *cp;
				curcol = 0;
				continue;
			}

			/* Escaped quotation marks. */

			if (*cp  == '\\' && cp[1] == '"')
				cp++;

			/*
			 * If we're starting a word, see whether the
			 * word will extend beyond our line boundaries.
			 * If it does, and if the last character wasn't
			 * a newline, then emit a newline.
			 */

			if (isspace((unsigned char)last) &&
			    !isspace((unsigned char)*cp)) {
				for (cpp = cp; *cpp != '\0'; cpp++)
					if (isspace((unsigned char)*cpp))
						break;
				if (curcol + (cpp - cp) > maxcol) {
					if (fputc('\n', f) == EOF)
						return 0;
					for (i = 0; i < tabs; i++)
						if (fputc('\t', f) == EOF)
							return 0;
					if (fputs(in, f) == EOF)
						return 0;
					curcol = 0;
				}
			}

			if (fputc(*cp, f) == EOF)
				return 0;
			last = *cp;
			curcol++;
		}

		/* Newline-terminate. */

		if (last != '\n' && fputc('\n', f) == EOF)
			return 0;
	}

	/* Epilogue material. */

	if (post != NULL) {
		for (i = 0; i < tabs; i++)
			if (fputc('\t', f) == EOF)
				return 0;
		if (fprintf(f, "%s\n", post) < 0)
			return 0;
	}

	return 1;
}

/*
 * Print comment on a line of its own.
 * If "cp" is NULL, this does nothing.
 * Returns zero on failure, non-zero on success.
 */
int
gen_comment(FILE *f, size_t tabs, enum cmtt type, const char *cp)
{
	size_t	 maxcol, i;
	int	 c;

	if (cp == NULL)
		return 1;

	maxcol = (tabs >= 4) ? 40 : MAXCOLS - (tabs * 4);

	/*
	 * If we're a C comment and are sufficiently small, then print
	 * us on a one-line comment block.
	 * FIXME: make sure the comment doesn't have the end-of-comment
	 * marker itself.
	 */

	if (type == COMMENT_C && cp != NULL && tabs >= 1 && 
	    strchr(cp, '\n') == NULL && strlen(cp) < maxcol) {
		for (i = 0; i < tabs; i++) 
			if (fputc('\t', f) == EOF)
				return 0;
		return fprintf(f, "/* %s */\n", cp) > 0;
	}

	/* Multi-line (or sufficiently long) comment. */

	switch (type) {
	case COMMENT_C:
		c = print_comment(f, cp, tabs, "/*", " * ", " */");
		break;
	case COMMENT_JS:
		c = print_comment(f, cp, tabs, "/**", " * ", " */");
		break;
	case COMMENT_C_FRAG_CLOSE:
	case COMMENT_JS_FRAG_CLOSE:
		c = print_comment(f, cp, tabs, NULL, " * ", " */");
		break;
	case COMMENT_C_FRAG_OPEN:
		c = print_comment(f, cp, tabs, "/*", " * ", NULL);
		break;
	case COMMENT_JS_FRAG_OPEN:
		c = print_comment(f, cp, tabs, "/**", " * ", NULL);
		break;
	case COMMENT_C_FRAG:
	case COMMENT_JS_FRAG:
		c = print_comment(f, cp, tabs, NULL, " * ", NULL);
		break;
	case COMMENT_SQL:
		c = print_comment(f, cp, tabs, NULL, "-- ", NULL);
		break;
	}

	return c;
}

int
gen_commentv(FILE *f, size_t tabs, enum cmtt t, const char *fmt, ...)
{
	va_list	 ap;
	char	*cp;
	int	 c;

	va_start(ap, fmt);
	if (vasprintf(&cp, fmt, ap) == -1)
		return 0;
	va_end(ap);
	c = gen_comment(f, tabs, t, cp);
	free(cp);
	return c;
}

void
print_commentt(size_t tabs, enum cmtt type, const char *cp)
{

	gen_comment(stdout, tabs, type, cp);
}

/*
 * Print comments with varargs.
 * See print_commentt().
 */
void
print_commentv(size_t tabs, enum cmtt type, const char *fmt, ...)
{
	va_list	 ap;
	char	*cp;

	va_start(ap, fmt);
	if (vasprintf(&cp, fmt, ap) == -1)
		err(EXIT_FAILURE, "vasprintf");
	va_end(ap);
	print_commentt(tabs, type, cp);
	free(cp);
}

/*
 * Print all of the columns that a select statement wants.
 * This uses the macro/function for enumerating columns.
 */
static int
gen_sql_stmt_schema(FILE *f, size_t tabs, enum langt lang,
	const struct strct *orig, int first, 
	const struct strct *p, const char *pname, size_t *col)
{
	const struct field	*fd;
	const struct alias	*a = NULL;
	int			 rc;
	char			*name = NULL;
	char			 delim;
	const char		*spacer;
	size_t			 i;

	delim = lang == LANG_JS ? '\'' : '"';
	spacer = lang == LANG_JS ? "+ " : "";

	if (first) {
		if (fputc(delim, f) == EOF)
			return 0;
		(*col)++;
	} else {
		rc = fprintf(f, "%s%c,%c", spacer, delim, delim);
		if (rc < 0)
			return 0;
		*col += rc;
	}

	if (fputc(' ', f) == EOF)
		return 0;
	(*col)++;

	if (!first && *col >= 72) {
		if (fputc('\n', f) == EOF)
			return 0;
		for (i = 0; i < tabs + 1; i++)
			if (fputc('\t', f) == EOF)
				return 0;
		*col = 8 * (tabs + 1);
	}

	/* 
	 * If applicable, looks up our alias and emit it as the alias
	 * for the table.
	 * Otherwise, use the table name itself.
	 */

	if (lang == LANG_C)
		rc = fprintf(f, "DB_SCHEMA_%s(", p->name);
	else
		rc = fprintf(f, "+ ort_schema_%s(", p->name);
	if (rc < 0)
		return 0;
	*col += rc;

	if (pname != NULL) {
		TAILQ_FOREACH(a, &orig->aq, entries)
			if (strcasecmp(a->name, pname) == 0)
				break;
		assert(a != NULL);
		rc = fprintf(f, "%s%s%s) ",
			lang == LANG_JS ? "'" : "",
			a->alias,
			lang == LANG_JS ? "'" : "");
	} else
		rc = fprintf(f, "%s%s%s) ", 
			lang == LANG_JS ? "'" : "",
			p->name,
			lang == LANG_JS ? "'" : "");
	if (rc < 0)
		return 0;
	*col += rc;

	/*
	 * Recursive step.
	 * Search through all of our fields for structures.
	 * If we find them, build up the canonical field reference and
	 * descend.
	 */

	TAILQ_FOREACH(fd, &p->fq, entries) {
		if (fd->type != FTYPE_STRUCT ||
		    (fd->ref->source->flags & FIELD_NULL))
			continue;

		if (pname != NULL) {
			if (asprintf(&name, 
			    "%s.%s", pname, fd->name) == -1)
				return 0;
		} else if ((name = strdup(fd->name)) == NULL)
			return 0;
		if (!gen_sql_stmt_schema(f, tabs, lang, orig, 0,
		    fd->ref->target->parent, name, col))
			return 0;
		free(name);
	}

	return 1;
}

/*
 * Print all of the inner join statements required for the references of
 * a given structure "p" using its aliases if applicable.
 * One statement is printed per line.
 * This is a recursive function and invokes itself for all foreign key
 * referenced structures.
 */
static int
gen_sql_stmt_join(FILE *f, size_t tabs, enum langt lang,
	const struct strct *orig, const struct strct *p,
	const struct alias *parent, size_t *count)
{
	const struct field	*fd;
	const struct alias	*a;
	char			*name;
	char			 delim;
	const char		*spacer;
	size_t			 i;

	delim = lang == LANG_JS ? '\'' : '"';
	spacer = lang == LANG_JS ? "+ " : "";

	TAILQ_FOREACH(fd, &p->fq, entries) {
		if (fd->type != FTYPE_STRUCT ||
		    (fd->ref->source->flags & FIELD_NULL))
			continue;

		if (parent != NULL) {
			if (asprintf(&name, "%s.%s",
			    parent->name, fd->name) == -1)
				return 0;
		} else if ((name = strdup(fd->name)) == NULL)
			return 0;

		TAILQ_FOREACH(a, &orig->aq, entries)
			if (strcasecmp(a->name, name) == 0)
				break;

		assert(a != NULL);

		if (*count == 0 && fprintf(f, " %c", delim) < 0)
			return 0;

		(*count)++;
		if (fputc('\n', f) == EOF)
			return 0;
		for (i = 0; i < tabs + 1; i++)
			if (fputc('\t', f) == EOF)
				return 0;
		if (fprintf(f, 
		    "%s%cINNER JOIN %s AS %s ON %s.%s=%s.%s %c",
		    spacer, delim,
		    fd->ref->target->parent->name, a->alias,
		    a->alias, fd->ref->target->name,
		    NULL == parent ? p->name : parent->alias,
		    fd->ref->source->name, delim) < 0)
			return 0;
		if (!gen_sql_stmt_join(f, tabs, lang, orig, 
		    fd->ref->target->parent, a, count))
			return 0;
		free(name);
	}

	return 1;
}

int
gen_sql_stmts(FILE *f, size_t tabs, 
	const struct strct *p, enum langt lang)
{
	const struct search	*s;
	const struct sent	*sent;
	const struct field	*fd;
	const struct update	*up;
	const struct uref	*ur;
	const struct ord	*ord;
	int			 first, hastrail, needquot, rc;
	size_t			 i, pos, nc, col;
	char			 delim;
	const char		*spacer;

	delim = lang == LANG_JS ? '\'' : '"';
	spacer = lang == LANG_JS ? "+ " : "";

	/* 
	 * We have a special query just for our unique fields.
	 * These are generated in the event of null foreign key
	 * reference lookups with the generated db_xxx_reffind()
	 * functions.
	 * TODO: figure out which ones we should be generating and only
	 * do this, as otherwise we're just wasting static space.
	 */

	TAILQ_FOREACH(fd, &p->fq, entries)  {
		if (!(fd->flags & (FIELD_ROWID|FIELD_UNIQUE)))
			continue;
		for (i = 0; i < tabs; i++)
			if (fputc('\t', f) == EOF)
				return 0;
		if (fprintf(f, "/* STMT_%s_BY_UNIQUE_%s */\n", 
		    p->name, fd->name) < 0)
			return 0;
		for (i = 0; i < tabs; i++)
			if (fputc('\t', f) == EOF)
				return 0;
		col = tabs * 8;
		if ((rc = fprintf(f, "%cSELECT ", delim)) < 0)
			return 0;
		col += rc;
		if (!gen_sql_stmt_schema(f, 
		    tabs, lang, p, 1, p, NULL, &col))
			return 0;

		if (fprintf(f, "%s%c FROM %s", 
		    spacer, delim, p->name) < 0)
			return 0;
		nc = 0;
		if (!gen_sql_stmt_join
		    (f, tabs, lang, p, p, NULL, &nc))
			return 0;
		if (nc > 0) {
			if (fputc('\n', f) == EOF)
				return 0;
			for (i = 0; i < tabs + 1; i++)
				if (fputc('\t', f) == EOF)
					return 0;
			if (fprintf(f, "%s%c", spacer, delim) < 0)
				return 0;
		} else {
			if (fputc(' ', f) == EOF)
				return 0;
		}

		if (fprintf(f, "WHERE %s.%s = ?%c,\n", 
		    p->name, fd->name, delim) < 0)
			return 0;
	}

	/* Print custom search queries. */

	pos = 0;
	TAILQ_FOREACH(s, &p->sq, entries) {
		for (i = 0; i < tabs; i++)
			if (fputc('\t', f) == EOF)
				return 0;
		if (fprintf(f, "/* STMT_%s_BY_SEARCH_%zu */\n",
		    p->name, pos++) < 0)
			return 0;
		for (i = 0; i < tabs; i++)
			if (fputc('\t', f) == EOF)
				return 0;
		if (fprintf(f, "%cSELECT ", delim) < 0)
			return 0;
		col = 16;
		needquot = 0;

		/* 
		 * Juggle around the possibilities of...
		 *   select count(*)
		 *   select count(distinct --gen_sql_stmt_schema--)
		 *   select --gen_sql_stmt_schema--
		 */

		if (s->type == STYPE_COUNT) {
			if ((rc = fprintf(f, "COUNT(")) < 0)
				return 0;
			col += rc;
		}
		if (s->dst) {
			if ((rc = fprintf(f, "DISTINCT ")) < 0)
				return 0;
			col += rc;
			if (!gen_sql_stmt_schema(f, tabs, lang, 
			    p, 1, s->dst->strct, s->dst->fname, &col))
				return 0;
			needquot = 1;
		} else if (s->type != STYPE_COUNT) {
			if (!gen_sql_stmt_schema(f, tabs, lang,
			    p, 1, p, NULL, &col))
				return 0;
			needquot = 1;
		} else
			if (fputc('*', f) == EOF)
				return 0;

		if (needquot && fprintf(f, "%s%c", spacer, delim) < 0)
			return 0;
		if (s->type == STYPE_COUNT && fputc(')', f) == EOF)
			return 0;
		if (fprintf(f, " FROM %s", p->name) < 0)
			return 0;

		/* 
		 * Whether anything is coming after the "FROM" clause,
		 * which includes all ORDER, WHERE, GROUP, LIMIT, and
		 * OFFSET commands.
		 */

		hastrail = 
			(s->aggr != NULL && s->group != NULL) ||
			(!TAILQ_EMPTY(&s->sntq)) ||
			(!TAILQ_EMPTY(&s->ordq)) ||
			(s->type != STYPE_SEARCH && s->limit > 0) ||
			(s->type != STYPE_SEARCH && s->offset > 0);
		
		nc = 0;
		if (!gen_sql_stmt_join
		    (f, tabs, lang, p, p, NULL, &nc))
			return 0;

		/* 
		 * We need to have a special JOIN command for aggregate
		 * groupings: we LEFT OUTER JOIN the grouped set to
		 * itself, conditioning upon the aggregate inequality.
		 * We'll filter NULL joinings in the WHERE statement.
		 */

		if (NULL != s->aggr && NULL != s->group) {
			assert(s->aggr->field->parent == 
			       s->group->field->parent);
			if (nc == 0 &&
			    fprintf(f, " %c", delim) < 0)
				return 0;
			if (fputc('\n', f) == EOF)
				return 0;
			for (i = 0; i < tabs + 1; i++)
				if (fputc('\t', f) == EOF)
					return 0;
			if (fprintf(f, 
			    "%s%cLEFT OUTER JOIN %s as _custom "
			    "ON %s.%s = _custom.%s "
			    "AND %s.%s %s _custom.%s %c",
			    spacer, delim,
			    s->group->field->parent->name, 
			    s->group->alias == NULL ?
			    s->group->field->parent->name : 
			    s->group->alias->alias,
			    s->group->field->name, 
			    s->group->field->name,
			    s->group->alias == NULL ?
			    s->group->field->parent->name : 
			    s->group->alias->alias, 
			    s->aggr->field->name, 
			    AGGR_MAXROW == s->aggr->op ?  "<" : ">",
			    s->aggr->field->name,
			    delim) < 0)
				return 0;
			nc = 1;
		}

		if (!hastrail) {
			if (nc == 0 && fputc(delim, f) == EOF)
				return 0;
			if (fputs(",\n", f) == EOF)
				return 0;
			continue;
		}

		if (nc == 0 && fprintf(f, " %c", delim) < 0)
			return 0;
		if (fputc('\n', f) == EOF)
			return 0;
		for (i = 0; i < tabs + 1; i++)
			if (fputc('\t', f) == EOF)
				return 0;
		if (fprintf(f, "%s%c", spacer, delim) < 0)
			return 0;

		if (!TAILQ_EMPTY(&s->sntq) || 
		    (s->aggr != NULL && s->group != NULL))
			if (fputs("WHERE", f) == EOF)
				return 0;

		first = 1;

		/* 
		 * If we're grouping, filter out all of the joins that
		 * failed and aren't part of the results.
		 */

		if (s->group != NULL) {
			if (fprintf(f, " _custom.%s IS NULL", 
			    s->group->field->name) < 0)
				return 0;
			first = 0;
		}

		/* Continue with our proper WHERE clauses. */

		TAILQ_FOREACH(sent, &s->sntq, entries) {
			if (sent->field->type == FTYPE_PASSWORD &&
			    !OPTYPE_ISUNARY(sent->op) &&
			    sent->op != OPTYPE_STREQ &&
			    sent->op != OPTYPE_STRNEQ)
				continue;
			if (!first && fputs(" AND", f) == EOF)
				return 0;
			first = 0;
			if (OPTYPE_ISUNARY(sent->op)) {
				if (fprintf(f, " %s.%s %s",
				    sent->alias == NULL ?
				    p->name : sent->alias->alias,
				    sent->field->name, 
				    optypes[sent->op]) < 0)
					return 0;
			} else {
				if (fprintf(f, " %s.%s %s ?", 
				    sent->alias == NULL ?
				    p->name : sent->alias->alias,
				    sent->field->name, 
				    optypes[sent->op]) < 0)
					return 0;
			}
		}

		first = 1;
		if (!TAILQ_EMPTY(&s->ordq) &&
		    fputs(" ORDER BY ", f) == EOF)
			return 0;
		TAILQ_FOREACH(ord, &s->ordq, entries) {
			if (!first && fputs(", ", f) == EOF)
				return 0;
			first = 0;
			if (fprintf(f, "%s.%s %s",
			    NULL == ord->alias ?
			    p->name : ord->alias->alias,
			    ord->field->name, 
			    ORDTYPE_ASC == ord->op ?
			    "ASC" : "DESC") < 0)
				return 0;
		}

		if (STYPE_SEARCH != s->type && s->limit > 0 &&
		    fprintf(f, " LIMIT %" PRId64, s->limit) < 0)
			return 0;
		if (STYPE_SEARCH != s->type && s->offset > 0 &&
		    fprintf(f, " OFFSET %" PRId64, s->offset) < 0)
			return 0;
		if (fprintf(f, "%c,\n", delim) < 0)
			return 0;
	}

	/* Insertion of a new record. */

	if (p->ins != NULL) {
		for (i = 0; i < tabs; i++)
			if (fputc('\t', f) == EOF)
				return 0;
		if (fprintf(f, 
	  	    "/* STMT_%s_INSERT */\n", p->name) < 0)
			return 0;
		for (i = 0; i < tabs; i++)
			if (fputc('\t', f) == EOF)
				return 0;

		col = tabs * 8;
		if ((rc = fprintf(f, 
		    "%cINSERT INTO %s ", delim, p->name)) < 0)
			return 0;
		col += rc;

		first = 1;
		TAILQ_FOREACH(fd, &p->fq, entries) {
			if (fd->type == FTYPE_STRUCT ||
			    (fd->flags & FIELD_ROWID))
				continue;
			if (col >= 72) {
				if (fprintf(f, "%s%c\n", 
				    first ? "" : ",", delim) < 0)
					return 0;
				for (i = 0; i < tabs + 1; i++)
					if (fputc('\t', f) == EOF)
						return 0;
				if (fprintf(f, "%s%c%s", spacer, 
				    delim, first ? "(" : " ") < 0)
					return 0;
				col = (tabs + 1) * 8;
			} else if (fputc(first ? '(' : ',', f) == EOF)
				return 0;

			if ((rc = fprintf(f, "%s", fd->name)) < 0)
				return 0;
			col += 1 + rc;
			first = 0;
		}

		if (first == 0) {
			if ((rc = fprintf(f, ") ")) < 0)
				return 0;
			if ((col += rc) >= 72) {
				if (fprintf(f, "%c\n", delim) < 0)
					return 0;
				for (i = 0; i < tabs + 1; i++)
					if (fputc('\t', f) == EOF)
						return 0;
				col = (tabs + 1) * 8;
				if ((rc = fprintf(f, 
				    "%s%c", spacer, delim)) < 0)
					return 0;
				col += rc;
			}

			first = 1;
			if ((rc = fprintf(f, "VALUES ")) < 0)
				return 0;
			col += rc;
			TAILQ_FOREACH(fd, &p->fq, entries) {
				if (fd->type == FTYPE_STRUCT ||
				    (fd->flags & FIELD_ROWID))
					continue;
				if (col >= 72) {
					if (fprintf(f, "%s%c\n", 
					    first ? "" : ",", 
					    delim) < 0)
						return 0;
					for (i = 0; i < tabs + 1; i++)
						if (fputc('\t', f) == EOF)
							return 0;
					col = (tabs + 1) * 8;
					rc = fprintf(f, "%s%c%s", spacer,
						delim, first ? "(" : " ");
					if (rc < 0)
						return 0;
					col += rc;
				} else {
					if (fputc(first ? 
					    '(' : ',', f) == EOF)
						return 0;
				}

				if (fputc('?', f) == EOF)
					return 0;
				col += 2;
				first = 0;
			}
			if (fprintf(f, ")%c,\n", delim) < 0)
				return 0;
		} else {
			if (fprintf(f, 
			    "DEFAULT VALUES%c,\n", delim) < 0)
				return 0;
		}
	}
	
	/* 
	 * Custom update queries. 
	 * Our updates can have modifications where they modify the
	 * given field (instead of setting it externally).
	 */

	pos = 0;
	TAILQ_FOREACH(up, &p->uq, entries) {
		for (i = 0; i < tabs; i++)
			if (fputc('\t', f) == EOF)
				return 0;
		if (fprintf(f, 
		    "/* STMT_%s_UPDATE_%zu */\n", p->name, pos++) < 0)
			return 0;
		for (i = 0; i < tabs; i++)
			if (fputc('\t', f) == EOF)
				return 0;
		if (fprintf(f, "%cUPDATE %s SET", delim, p->name) < 0)
			return 0;

		first = 1;
		TAILQ_FOREACH(ur, &up->mrq, entries) {
			if (fputc(first ? ' ' : ',', f) == EOF)
				return 0;

			first = 0;
			switch (ur->mod) {
			case MODTYPE_INC:
				if (fprintf(f, "%s = %s + ?", 
				    ur->field->name, 
				    ur->field->name) < 0)
					return 0;
				break;
			case MODTYPE_DEC:
				if (fprintf(f, "%s = %s - ?", 
				    ur->field->name, 
				    ur->field->name) < 0)
					return 0;
				break;
			case MODTYPE_CONCAT:
				if (fprintf(f, "%s = ", 
				    ur->field->name) < 0)
					return 0;

				/*
				 * If we concatenate a NULL with a
				 * non-NULL, we'll always get a NULL
				 * value, which isn't what we want.
				 * This will wrap possibly-null values
				 * so that they're always strings.
				 */

				if ((ur->field->flags & FIELD_NULL)) {
					if (fprintf(f, 
					    "COALESCE(%s,'')",
					    ur->field->name) < 0)
						return 0;
				} else {
					if (fprintf(f, "%s", 
					    ur->field->name) < 0)
						return 0;
				}
				if (fputs(" || ?", f) == EOF)
					return 0;
				break;
			default:
				if (fprintf(f, "%s = ?", 
				    ur->field->name) < 0)
					return 0;
				break;
			}
		}

		first = 1;
		TAILQ_FOREACH(ur, &up->crq, entries) {
			if (fprintf(f, " %s ", 
			    first ? "WHERE" : "AND") < 0)
				return 0;
			if (OPTYPE_ISUNARY(ur->op)) {
				if (fprintf(f, "%s %s", 
				    ur->field->name, 
				    optypes[ur->op]) < 0)
					return 0;
			} else {
				if (fprintf(f, "%s %s ?", 
				    ur->field->name,
				    optypes[ur->op]) < 0)
					return 0;
			}
			first = 0;
		}
		if (fprintf(f, "%c,\n", delim) < 0)
			return 0;
	}

	/* Custom delete queries. */

	pos = 0;
	TAILQ_FOREACH(up, &p->dq, entries) {
		for (i = 0; i < tabs; i++)
			if (fputc('\t', f) == EOF)
				return 0;
		if (fprintf(f, 
		    "/* STMT_%s_DELETE_%zu */\n", p->name, pos++) < 0)
			return 0;
		for (i = 0; i < tabs; i++)
			if (fputc('\t', f) == EOF)
				return 0;
		if (fprintf(f, "%cDELETE FROM %s", delim, p->name) < 0)
			return 0;

		first = 1;
		TAILQ_FOREACH(ur, &up->crq, entries) {
			if (fprintf(f, 
			    " %s ", first ? "WHERE" : "AND") < 0)
				return 0;
			if (OPTYPE_ISUNARY(ur->op)) {
				if (fprintf(f, 
				    "%s %s", ur->field->name, 
				    optypes[ur->op]) < 0)
					return 0;
			} else {
				if (fprintf(f, 
				    "%s %s ?", ur->field->name,
				    optypes[ur->op]) < 0)
					return 0;
			}
			first = 0;
		}
		if (fprintf(f, "%c,\n", delim) < 0)
			return 0;
	}

	return 1;
}

void
print_sql_stmts(size_t tabs, const struct strct *p, enum langt lang)
{

	gen_sql_stmts(stdout, tabs, p, lang);
}

int
gen_sql_enums(FILE *f, size_t tabs,
	const struct strct *p, enum langt lang)
{
	const struct search	*s;
	const struct update	*u;
	const struct field	*fd;
	size_t			 i, pos;

	TAILQ_FOREACH(fd, &p->fq, entries)
		if (fd->flags & (FIELD_UNIQUE|FIELD_ROWID)) {
			for (i = 0; i < tabs; i++)
				if (fputc('\t', f) == EOF)
					return 0;
			if (fprintf(f, "STMT_%s_BY_UNIQUE_%s,\n", 
			    p->name, fd->name) < 0)
				return 0;
		}

	pos = 0;
	TAILQ_FOREACH(s, &p->sq, entries) {
		for (i = 0; i < tabs; i++)
			if (fputc('\t', f) == EOF)
				return 0;
		if (fprintf(f, 
		    "STMT_%s_BY_SEARCH_%zu,\n", p->name, pos++) < 0)
			return 0;
	}

	if (p->ins != NULL) {
		for (i = 0; i < tabs; i++)
			if (fputc('\t', f) == EOF)
				return 0;
		if (fprintf(f, 
		    "STMT_%s_INSERT,\n", p->name) < 0)
			return 0;
	}

	pos = 0;
	TAILQ_FOREACH(u, &p->uq, entries) {
		for (i = 0; i < tabs; i++)
			if (fputc('\t', f) == EOF)
				return 0;
		if (fprintf(f, 
		    "STMT_%s_UPDATE_%zu,\n", p->name, pos++) < 0)
			return 0;
	}

	pos = 0;
	TAILQ_FOREACH(u, &p->dq, entries) {
		for (i = 0; i < tabs; i++)
			if (fputc('\t', f) == EOF)
				return 0;
		if (fprintf(f, 
		    "STMT_%s_DELETE_%zu,\n", p->name, pos++) < 0)
			return 0;
	}

	return 1;
}

void
print_sql_enums(size_t tabs, const struct strct *p, enum langt lang)
{

	gen_sql_enums(stdout, tabs, p, lang);
}
