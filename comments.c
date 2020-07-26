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
#include "extern.h"
#include "comments.h"

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
static void
print_comment(const char *doc, size_t tabs, 
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
			putchar('\t');
		puts(pre);
	}

	/* Emit the documentation matter. */

	if (doc != NULL) {
		for (i = 0; i < tabs; i++)
			putchar('\t');
		printf("%s", in);

		for (curcol = 0, cp = doc; *cp != '\0'; cp++) {
			/*
			 * Newline check.
			 * If we're at a newline, then emit the leading
			 * in-comment marker.
			 */

			if (*cp == '\n') {
				putchar('\n');
				for (i = 0; i < tabs; i++)
					putchar('\t');
				printf("%s", in);
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
					putchar('\n');
					for (i = 0; i < tabs; i++)
						putchar('\t');
					printf("%s", in);
					curcol = 0;
				}
			}

			putchar(*cp);
			last = *cp;
			curcol++;
		}

		/* Newline-terminate. */

		if (last != '\n')
			putchar('\n');
	}

	/* Epilogue material. */

	if (post != NULL) {
		for (i = 0; i < tabs; i++)
			putchar('\t');
		puts(post);
	}
}

/*
 * Print comments with a fixed string.
 */
void
print_commentt(size_t tabs, enum cmtt type, const char *cp)
{
	size_t	maxcol, i;

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
			putchar('\t');
		printf("/* %s */\n", cp);
		return;
	}

	/*
	 * If we're a two-tab JavaScript comment, emit the whole
	 * production on one line.
	 * FIXME: make sure the comment doesn't have the end-of-comment
	 * marker itself.
	 */

#if 0
	if (type == COMMENT_JS && cp != NULL && tabs == 2 && 
	    strchr(cp, '\n') == NULL && strlen(cp) < maxcol) {
		printf("\t\t/** %s */\n", cp);
		return;
	}
#endif

	/* Multi-line (or sufficiently long) comment. */

	switch (type) {
	case COMMENT_C:
		print_comment(cp, tabs, "/*", " * ", " */");
		break;
	case COMMENT_JS:
		print_comment(cp, tabs, "/**", " * ", " */");
		break;
	case COMMENT_C_FRAG_CLOSE:
	case COMMENT_JS_FRAG_CLOSE:
		print_comment(cp, tabs, NULL, " * ", " */");
		break;
	case COMMENT_C_FRAG_OPEN:
		print_comment(cp, tabs, "/*", " * ", NULL);
		break;
	case COMMENT_JS_FRAG_OPEN:
		print_comment(cp, tabs, "/**", " * ", NULL);
		break;
	case COMMENT_C_FRAG:
	case COMMENT_JS_FRAG:
		print_comment(cp, tabs, NULL, " * ", NULL);
		break;
	case COMMENT_SQL:
		print_comment(cp, tabs, NULL, "-- ", NULL);
		break;
	}
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
static void
print_sql_stmt_schema(size_t tabs, enum langt lang,
	const struct strct *orig, int first, 
	const struct strct *p, const char *pname, size_t *col)
{
	const struct field	*f;
	const struct alias	*a = NULL;
	int			 c;
	char			*name = NULL;
	char			 delim;
	const char		*spacer;
	size_t			 i;

	delim = lang == LANG_JS ? '\'' : '"';
	spacer = lang == LANG_JS ? "+ " : "";

	if (first) {
		putchar(delim);
		(*col)++;
	} else
		*col += printf("%s%c,%c", spacer, delim, delim);

	putchar(' ');
	(*col)++;

	if (!first && *col >= 72) {
		puts("");
		for (i = 0; i < tabs + 1; i++)
			putchar('\t');
		*col = 8 * (tabs + 1);
	}

	/* 
	 * If applicable, looks up our alias and emit it as the alias
	 * for the table.
	 * Otherwise, use the table name itself.
	 */

	if (lang == LANG_C)
		*col += printf("DB_SCHEMA_%s(", p->name);
	else
		*col += printf("+ ort_schema_%s(", p->name);

	if (pname != NULL) {
		TAILQ_FOREACH(a, &orig->aq, entries)
			if (strcasecmp(a->name, pname) == 0)
				break;
		assert(a != NULL);
		*col += printf("%s%s%s) ",
			lang == LANG_JS ? "'" : "",
			a->alias,
			lang == LANG_JS ? "'" : "");
	} else
		*col += printf("%s%s%s) ", 
			lang == LANG_JS ? "'" : "",
			p->name,
			lang == LANG_JS ? "'" : "");

	/*
	 * Recursive step.
	 * Search through all of our fields for structures.
	 * If we find them, build up the canonical field reference and
	 * descend.
	 */

	TAILQ_FOREACH(f, &p->fq, entries) {
		if (f->type != FTYPE_STRUCT ||
		    (f->ref->source->flags & FIELD_NULL))
			continue;

		if (pname != NULL) {
			c = asprintf(&name, "%s.%s", pname, f->name);
			if (c < 0)
				err(EXIT_FAILURE, NULL);
		} else if ((name = strdup(f->name)) == NULL)
			err(EXIT_FAILURE, "strdup");
		print_sql_stmt_schema(tabs, lang, orig, 0,
			f->ref->target->parent, name, col);
		free(name);
	}
}

/*
 * Print all of the inner join statements required for the references of
 * a given structure "p" using its aliases if applicable.
 * One statement is printed per line.
 * This is a recursive function and invokes itself for all foreign key
 * referenced structures.
 */
static void
print_sql_stmt_join(size_t tabs, enum langt lang,
	const struct strct *orig, const struct strct *p,
	const struct alias *parent, size_t *count)
{
	const struct field	*f;
	const struct alias	*a;
	char			*name;
	char			 delim;
	const char		*spacer;
	size_t			 i;

	delim = lang == LANG_JS ? '\'' : '"';
	spacer = lang == LANG_JS ? "+ " : "";

	TAILQ_FOREACH(f, &p->fq, entries) {
		if (f->type != FTYPE_STRUCT ||
		    (f->ref->source->flags & FIELD_NULL))
			continue;

		if (parent != NULL) {
			if (asprintf(&name, "%s.%s",
			    parent->name, f->name) == -1)
				err(EXIT_FAILURE, NULL);
		} else if ((name = strdup(f->name)) == NULL)
			err(EXIT_FAILURE, NULL);

		TAILQ_FOREACH(a, &orig->aq, entries)
			if (strcasecmp(a->name, name) == 0)
				break;

		assert(a != NULL);

		if (*count == 0)
			printf(" %c", delim);

		(*count)++;
		putchar('\n');
		for (i = 0; i < tabs + 1; i++)
			putchar('\t');
		printf("%s%cINNER JOIN %s AS %s ON %s.%s=%s.%s %c",
			spacer, delim,
			f->ref->target->parent->name, a->alias,
			a->alias, f->ref->target->name,
			NULL == parent ? p->name : parent->alias,
			f->ref->source->name, delim);
		print_sql_stmt_join(tabs, lang, orig, 
			f->ref->target->parent, a, count);
		free(name);
	}
}

void
print_sql_stmts(size_t tabs, const struct strct *p, enum langt lang)
{
	const struct search	*s;
	const struct sent	*sent;
	const struct field	*f;
	const struct update	*up;
	const struct uref	*ur;
	const struct ord	*ord;
	int			 first, hastrail, needquot;
	size_t			 i, pos, rc, col;
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

	TAILQ_FOREACH(f, &p->fq, entries)  {
		if (!(f->flags & (FIELD_ROWID|FIELD_UNIQUE)))
			continue;
		for (i = 0; i < tabs; i++)
			putchar('\t');
		printf("/* STMT_%s_BY_UNIQUE_%s */\n", 
			p->name, f->name);
		for (i = 0; i < tabs; i++)
			putchar('\t');
		col = tabs * 8;
		col += printf("%cSELECT ", delim);
		print_sql_stmt_schema
			(tabs, lang, p, 1, p, NULL, &col);

		printf("%s%c FROM %s", spacer, delim, p->name);
		rc = 0;
		print_sql_stmt_join(tabs, lang, p, p, NULL, &rc);
		if (rc > 0) {
			putchar('\n');
			for (i = 0; i < tabs + 1; i++)
				putchar('\t');
			printf("%s%c", spacer, delim);
		} else
			printf(" ");

		printf("WHERE %s.%s = ?%c,\n", 
			p->name, f->name, delim);
	}

	/* Print custom search queries. */

	pos = 0;
	TAILQ_FOREACH(s, &p->sq, entries) {
		for (i = 0; i < tabs; i++)
			putchar('\t');
		printf("/* STMT_%s_BY_SEARCH_%zu */\n", p->name, pos++);
		for (i = 0; i < tabs; i++)
			putchar('\t');
		printf("%cSELECT ", delim);
		col = 16;
		needquot = 0;

		/* 
		 * Juggle around the possibilities of...
		 *   select count(*)
		 *   select count(distinct --print_sql_stmt_schema--)
		 *   select --print_sql_stmt_schema--
		 */

		if (s->type == STYPE_COUNT)
			col += printf("COUNT(");
		if (s->dst) {
			col += printf("DISTINCT ");
			print_sql_stmt_schema(tabs, lang, p, 1, 
				s->dst->strct, s->dst->fname, &col);
			needquot = 1;
		} else if (s->type != STYPE_COUNT) {
			print_sql_stmt_schema
				(tabs, lang, p, 1, p, NULL, &col);
			needquot = 1;
		} else
			printf("*");

		if (needquot)
			printf("%s%c", spacer, delim);
		if (s->type == STYPE_COUNT)
			putchar(')');

		printf(" FROM %s", p->name);

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
		
		rc = 0;
		print_sql_stmt_join(tabs, lang, p, p, NULL, &rc);

		/* 
		 * We need to have a special JOIN command for aggregate
		 * groupings: we LEFT OUTER JOIN the grouped set to
		 * itself, conditioning upon the aggregate inequality.
		 * We'll filter NULL joinings in the WHERE statement.
		 */

		if (NULL != s->aggr && NULL != s->group) {
			assert(s->aggr->field->parent == 
			       s->group->field->parent);
			putchar('\n');
			for (i = 0; i < tabs + 1; i++)
				putchar('\t');
			printf("%s%cLEFT OUTER JOIN %s as _custom "
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
				delim);
		}

		if (!hastrail) {
			if (rc == 0)
				putchar(delim);
			puts(",");
			continue;
		}

		if (rc > 0)
			printf("\n");
		else
			printf(" %c\n", delim);
		for (i = 0; i < tabs + 1; i++)
			putchar('\t');

		printf("%s%c", spacer, delim);

		if (!TAILQ_EMPTY(&s->sntq) || 
		    (s->aggr != NULL && s->group != NULL))
			printf("WHERE");

		first = 1;

		/* 
		 * If we're grouping, filter out all of the joins that
		 * failed and aren't part of the results.
		 */

		if (s->group != NULL) {
			printf(" _custom.%s IS NULL", 
				s->group->field->name);
			first = 0;
		}

		/* Continue with our proper WHERE clauses. */

		TAILQ_FOREACH(sent, &s->sntq, entries) {
			if (sent->field->type == FTYPE_PASSWORD &&
			    !OPTYPE_ISUNARY(sent->op) &&
			    sent->op != OPTYPE_STREQ &&
			    sent->op != OPTYPE_STRNEQ)
				continue;
			if (!first)
				printf(" AND");
			first = 0;
			if (OPTYPE_ISUNARY(sent->op))
				printf(" %s.%s %s",
					sent->alias == NULL ?
					p->name : sent->alias->alias,
					sent->field->name, 
					optypes[sent->op]);
			else
				printf(" %s.%s %s ?", 
					sent->alias == NULL ?
					p->name : sent->alias->alias,
					sent->field->name, 
					optypes[sent->op]);
		}

		first = 1;
		if (!TAILQ_EMPTY(&s->ordq))
			printf(" ORDER BY ");
		TAILQ_FOREACH(ord, &s->ordq, entries) {
			if ( ! first)
				printf(", ");
			first = 0;
			printf("%s.%s %s",
				NULL == ord->alias ?
				p->name : ord->alias->alias,
				ord->field->name, 
				ORDTYPE_ASC == ord->op ?
				"ASC" : "DESC");
		}

		if (STYPE_SEARCH != s->type && s->limit > 0)
			printf(" LIMIT %" PRId64, s->limit);
		if (STYPE_SEARCH != s->type && s->offset > 0)
			printf(" OFFSET %" PRId64, s->offset);

		printf("%c,\n", delim);
	}

	/* Insertion of a new record. */
	/* FIXME: cols += printf(...) is wrong. */

	if (p->ins != NULL) {
		for (i = 0; i < tabs; i++)
			putchar('\t');
		printf("/* STMT_%s_INSERT */\n", p->name);
		for (i = 0; i < tabs; i++)
			putchar('\t');

		col = tabs * 8;
		col += printf("%cINSERT INTO %s ", delim, p->name);

		first = 1;
		TAILQ_FOREACH(f, &p->fq, entries) {
			if (f->type == FTYPE_STRUCT ||
			    (f->flags & FIELD_ROWID))
				continue;

			if (col >= 72) {
				printf("%s%c\n", 
					first ? "" : ",", delim);
				for (i = 0; i < tabs + 1; i++)
					putchar('\t');
				printf("%s%c%s", spacer,
					delim, first ? "(" : " ");
				col = (tabs + 1) * 8;
			} else
				putchar(first ? '(' : ',');

			col += 1 + printf("%s", f->name);
			first = 0;
		}

		if (first == 0) {
			if ((col += printf(") ")) >= 72) {
				printf("%c\n", delim);
				for (i = 0; i < tabs + 1; i++)
					putchar('\t');
				col = (tabs + 1) * 8;
				col += printf("%s%c", spacer, delim);
			}

			first = 1;
			col += printf("VALUES ");
			TAILQ_FOREACH(f, &p->fq, entries) {
				if (f->type == FTYPE_STRUCT ||
				    (f->flags & FIELD_ROWID))
					continue;
				if (col >= 72) {
					printf("%s%c\n", 
						first ? "" : ",", delim);
					for (i = 0; i < tabs + 1; i++)
						putchar('\t');
					col = (tabs + 1) * 8;
					col += printf("%s%c%s", spacer,
						delim, first ? "(" : " ");
				} else
					putchar(first ? '(' : ',');

				putchar('?');
				col += 2;
				first = 0;
			}
			printf(")%c,\n", delim);
		} else
			printf("DEFAULT VALUES%c,\n", delim);
	}
	
	/* 
	 * Custom update queries. 
	 * Our updates can have modifications where they modify the
	 * given field (instead of setting it externally).
	 */

	pos = 0;
	TAILQ_FOREACH(up, &p->uq, entries) {
		for (i = 0; i < tabs; i++)
			putchar('\t');
		printf("/* STMT_%s_UPDATE_%zu */\n", p->name, pos++);
		for (i = 0; i < tabs; i++)
			putchar('\t');
		printf("%cUPDATE %s SET", delim, p->name);

		first = 1;
		TAILQ_FOREACH(ur, &up->mrq, entries) {
			putchar(first ? ' ' : ',');
			first = 0;
			switch (ur->mod) {
			case MODTYPE_INC:
				printf("%s = %s + ?", 
					ur->field->name, 
					ur->field->name);
				break;
			case MODTYPE_DEC:
				printf("%s = %s - ?", 
					ur->field->name, 
					ur->field->name);
				break;
			case MODTYPE_CONCAT:
				printf("%s = ", ur->field->name);

				/*
				 * If we concatenate a NULL with a
				 * non-NULL, we'll always get a NULL
				 * value, which isn't what we want.
				 * This will wrap possibly-null values
				 * so that they're always strings.
				 */

				if ((ur->field->flags & FIELD_NULL))
					printf("COALESCE(%s,'')",
						ur->field->name);
				else
					printf("%s", ur->field->name);
				printf(" || ?");
				break;
			default:
				printf("%s = ?", ur->field->name);
				break;
			}
		}
		first = 1;
		TAILQ_FOREACH(ur, &up->crq, entries) {
			printf(" %s ", first ? "WHERE" : "AND");
			if (OPTYPE_ISUNARY(ur->op))
				printf("%s %s", ur->field->name, 
					optypes[ur->op]);
			else
				printf("%s %s ?", ur->field->name,
					optypes[ur->op]);
			first = 0;
		}
		printf("%c,\n", delim);
	}

	/* Custom delete queries. */

	pos = 0;
	TAILQ_FOREACH(up, &p->dq, entries) {
		for (i = 0; i < tabs; i++)
			putchar('\t');
		printf("/* STMT_%s_DELETE_%zu */\n", p->name, pos++);
		for (i = 0; i < tabs; i++)
			putchar('\t');
		printf("%cDELETE FROM %s", delim, p->name);

		first = 1;
		TAILQ_FOREACH(ur, &up->crq, entries) {
			printf(" %s ", first ? "WHERE" : "AND");
			if (OPTYPE_ISUNARY(ur->op))
				printf("%s %s", ur->field->name, 
					optypes[ur->op]);
			else
				printf("%s %s ?", ur->field->name,
					optypes[ur->op]);
			first = 0;
		}
		printf("%c,\n", delim);
	}
}

void
print_sql_enums(size_t tabs, const struct strct *p, enum langt lang)
{
	const struct search	*s;
	const struct update	*u;
	const struct field	*f;
	size_t			 i, pos;

	TAILQ_FOREACH(f, &p->fq, entries)
		if (f->flags & (FIELD_UNIQUE|FIELD_ROWID)) {
			for (i = 0; i < tabs; i++)
				putchar('\t');
			printf("STMT_%s_BY_UNIQUE_%s,\n", 
				p->name, f->name);
		}

	pos = 0;
	TAILQ_FOREACH(s, &p->sq, entries) {
		for (i = 0; i < tabs; i++)
			putchar('\t');
		printf("STMT_%s_BY_SEARCH_%zu,\n", p->name, pos++);
	}

	if (p->ins != NULL) {
		for (i = 0; i < tabs; i++)
			putchar('\t');
		printf("STMT_%s_INSERT,\n", p->name);
	}

	pos = 0;
	TAILQ_FOREACH(u, &p->uq, entries) {
		for (i = 0; i < tabs; i++)
			putchar('\t');
		printf("STMT_%s_UPDATE_%zu,\n", p->name, pos++);
	}

	pos = 0;
	TAILQ_FOREACH(u, &p->dq, entries) {
		for (i = 0; i < tabs; i++)
			putchar('\t');
		printf("STMT_%s_DELETE_%zu,\n", p->name, pos++);
	}
}
