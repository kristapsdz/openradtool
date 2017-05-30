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

#include <assert.h>
#include <ctype.h>
#if HAVE_ERR
# include <err.h>
#endif
#include <errno.h>
#include <inttypes.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "extern.h"

enum	tok {
	TOK_NONE, /* special case: just starting */
	TOK_COLON, /* : */
	TOK_COMMA, /* ; */
	TOK_DECIMAL, /* decimal-valued number */
	TOK_EOF, /* end of file! */
	TOK_ERR, /* error! */
	TOK_IDENT, /* alphanumeric (starting with alpha) */
	TOK_INTEGER, /* integer */
	TOK_LBRACE, /* { */
	TOK_LITERAL, /* "text" */
	TOK_PERIOD, /* } */
	TOK_RBRACE, /* } */
	TOK_SEMICOLON /* ; */
};

#define	PARSE_STOP(_p) \
	(TOK_ERR == (_p)->lasttype || TOK_EOF == (_p)->lasttype)

struct	parse {
	union {
		char *string;
		int64_t integer;
		double decimal;
	} last; /* last parsed if TOK_IDENT or TOK_INTEGER */
	enum tok	 lasttype; /* last parse type */
	char		*buf; /* buffer for storing up reads */
	size_t		 bufsz; /* length of buffer */
	size_t		 bufmax; /* maximum buffer size */
	size_t		 line; /* current line (from 1) */
	size_t		 column; /* current column (from 1) */
	const char	*fname; /* current filename */
	FILE		*f; /* current parser */
};

/*
 * Disallowed field names.
 * The SQL ones are from https://sqlite.org/lang_keywords.html.
 */
static	const char *const badidents[] = {
	/* Things not allowed in C. */
	"auto",
	"break",
	"case",
	"char",
	"const",
	"continue",
	"default",
	"do",
	"double",
	"enum",
	"extern",
	"float",
	"goto",
	"long",
	"register",
	"short",
	"signed",
	"static",
	"struct",
	"typedef",
	"union",
	"unsigned",
	"void",
	"volatile",
	/* Things not allowed in SQLite. */
	"ABORT",
	"ACTION",
	"ADD",
	"AFTER",
	"ALL",
	"ALTER",
	"ANALYZE",
	"AND",
	"AS",
	"ASC",
	"ATTACH",
	"AUTOINCREMENT",
	"BEFORE",
	"BEGIN",
	"BETWEEN",
	"BY",
	"CASCADE",
	"CASE",
	"CAST",
	"CHECK",
	"COLLATE",
	"COLUMN",
	"COMMIT",
	"CONFLICT",
	"CONSTRAINT",
	"CREATE",
	"CROSS",
	"CURRENT_DATE",
	"CURRENT_TIME",
	"CURRENT_TIMESTAMP",
	"DATABASE",
	"DEFAULT",
	"DEFERRABLE",
	"DEFERRED",
	"DELETE",
	"DESC",
	"DETACH",
	"DISTINCT",
	"DROP",
	"EACH",
	"ELSE",
	"END",
	"ESCAPE",
	"EXCEPT",
	"EXCLUSIVE",
	"EXISTS",
	"EXPLAIN",
	"FAIL",
	"FOR",
	"FOREIGN",
	"FROM",
	"FULL",
	"GLOB",
	"GROUP",
	"HAVING",
	"IF",
	"IGNORE",
	"IMMEDIATE",
	"IN",
	"INDEX",
	"INDEXED",
	"INITIALLY",
	"INNER",
	"INSERT",
	"INSTEAD",
	"INTERSECT",
	"INTO",
	"IS",
	"ISNULL",
	"JOIN",
	"KEY",
	"LEFT",
	"LIKE",
	"LIMIT",
	"MATCH",
	"NATURAL",
	"NO",
	"NOT",
	"NOTNULL",
	"NULL",
	"OF",
	"OFFSET",
	"ON",
	"OR",
	"ORDER",
	"OUTER",
	"PLAN",
	"PRAGMA",
	"PRIMARY",
	"QUERY",
	"RAISE",
	"RECURSIVE",
	"REFERENCES",
	"REGEXP",
	"REINDEX",
	"RELEASE",
	"RENAME",
	"REPLACE",
	"RESTRICT",
	"RIGHT",
	"ROLLBACK",
	"ROW",
	"SAVEPOINT",
	"SELECT",
	"SET",
	"TABLE",
	"TEMP",
	"TEMPORARY",
	"THEN",
	"TO",
	"TRANSACTION",
	"TRIGGER",
	"UNION",
	"UNIQUE",
	"UPDATE",
	"USING",
	"VACUUM",
	"VALUES",
	"VIEW",
	"VIRTUAL",
	"WHEN",
	"WHERE",
	"WITH",
	"WITHOUT",
	NULL
};

static	const char *const optypes[OPTYPE__MAX] = {
	"eq", /* OPTYE_EQUAL */
	"ge", /* OPTYPE_GE */
	"gt", /* OPTYPE_GT */
	"le", /* OPTYPE_LE */
	"lt", /* OPTYPE_LT */
	"neq", /* OPTYE_NEQUAL */
	/* Unary types... */
	"isnull", /* OPTYE_ISNULL */
	"notnull", /* OPTYE_NOTNULL */
};

static enum tok parse_errx(struct parse *, const char *, ...)
	__attribute__((format(printf, 2, 3)));
static void parse_warnx(struct parse *p, const char *, ...)
	__attribute__((format(printf, 2, 3)));

/*
 * Push a single character into the retaining buffer.
 * This bails out on memory allocation failure.
 */
static void
buf_push(struct parse *p, char c)
{

	if (p->bufsz + 1 >= p->bufmax) {
		p->bufmax += 1024;
		p->buf = realloc(p->buf, p->bufmax);
		if (NULL == p->buf)
			err(EXIT_FAILURE, NULL);
	}
	p->buf[p->bufsz++] = c;
}

/*
 * Copy the current parse point into a saved position buffer.
 */
static void
parse_point(const struct parse *p, struct pos *pp)
{

	pp->fname = p->fname;
	pp->line = p->line;
	pp->column = p->column;
}

/*
 * Allocate a unique reference and add it to the parent queue in order
 * by alpha.
 * Always returns the created pointer.
 */
static struct nref *
nref_alloc(const struct parse *p, const char *name, 
	struct unique *up)
{
	struct nref	*ref, *n;

	if (NULL == (ref = calloc(1, sizeof(struct nref))))
		err(EXIT_FAILURE, NULL);
	if (NULL == (ref->name = strdup(name)))
		err(EXIT_FAILURE, NULL);
	ref->parent = up;
	parse_point(p, &ref->pos);

	/* Order alphabetically. */

	TAILQ_FOREACH(n, &up->nq, entries)
		if (strcasecmp(n->name, ref->name) >= 0)
			break;
	if (NULL == n)
		TAILQ_INSERT_TAIL(&up->nq, ref, entries);
	else
		TAILQ_INSERT_BEFORE(n, ref, entries);

	return(ref);
}

/*
 * Allocate an update reference and add it to the parent queue.
 * Always returns the created pointer.
 */
static struct uref *
uref_alloc(const struct parse *p, const char *name, 
	struct update *up, struct urefq *q)
{
	struct uref	*ref;

	if (NULL == (ref = calloc(1, sizeof(struct uref))))
		err(EXIT_FAILURE, NULL);
	if (NULL == (ref->name = strdup(name)))
		err(EXIT_FAILURE, NULL);
	ref->parent = up;
	parse_point(p, &ref->pos);
	TAILQ_INSERT_TAIL(q, ref, entries);
	return(ref);
}

/*
 * Allocate a search reference and add it to the parent queue.
 * Always returns the created pointer.
 */
static struct sref *
sref_alloc(const struct parse *p, const char *name, struct sent *up)
{
	struct sref	*ref;

	if (NULL == (ref = calloc(1, sizeof(struct sref))))
		err(EXIT_FAILURE, NULL);
	if (NULL == (ref->name = strdup(name)))
		err(EXIT_FAILURE, NULL);
	ref->parent = up;
	parse_point(p, &ref->pos);
	TAILQ_INSERT_TAIL(&up->srq, ref, entries);
	return(ref);
}

/*
 * Allocate a search entity and add it to the parent queue.
 * Always returns the created pointer.
 */
static struct sent *
sent_alloc(const struct parse *p, struct search *up)
{
	struct sent	*sent;

	if (NULL == (sent = calloc(1, sizeof(struct sent))))
		err(EXIT_FAILURE, NULL);
	sent->parent = up;
	parse_point(p, &sent->pos);
	TAILQ_INIT(&sent->srq);
	TAILQ_INSERT_TAIL(&up->sntq, sent, entries);
	return(sent);
}

/*
 * Trigger a "hard" error condition: stream error or EOF.
 * This sets the lasttype appropriately.
 */
static enum tok
parse_err(struct parse *p)
{

	if (ferror(p->f)) {
		warn("%s", p->fname);
		p->lasttype = TOK_ERR;
	} else 
		p->lasttype = TOK_EOF;

	return(p->lasttype);
}

/*
 * Print a warning.
 * This doesn't do anything else (like set any error conditions).
 * XXX: limited to 1024 bytes of message content.
 */
static void
parse_warnx(struct parse *p, const char *fmt, ...)
{
	char	 buf[1024];
	va_list	 ap;

	if (NULL != fmt) {
		va_start(ap, fmt);
		vsnprintf(buf, sizeof(buf), fmt, ap);
		va_end(ap);
		warnx("%s:%zu:%zu: %s", 
			p->fname, p->line, p->column, buf);
	} else
		warnx("%s:%zu:%zu: syntax warning", 
			p->fname, p->line, p->column);
}

/*
 * Trigger a "soft" error condition.
 * This sets the lasttype appropriately.
 * XXX: limited to 1024 bytes of message content.
 */
static enum tok
parse_errx(struct parse *p, const char *fmt, ...)
{
	char	 buf[1024];
	va_list	 ap;

	if (NULL != fmt) {
		va_start(ap, fmt);
		vsnprintf(buf, sizeof(buf), fmt, ap);
		va_end(ap);
		warnx("%s:%zu:%zu: %s", 
			p->fname, p->line, p->column, buf);
	} else
		warnx("%s:%zu:%zu: syntax error", 
			p->fname, p->line, p->column);

	p->lasttype = TOK_ERR;
	return(p->lasttype);
}

static void
parse_ungetc(struct parse *p, int c)
{

	/* FIXME: puts us at last line, not column */

	if ('\n' == c)
		p->line--;
	else if (p->column > 0)
		p->column--;
	ungetc(c, p->f);
}

/*
 * Get the next character and advance us within the file.
 * If we've already reached an error condition, this just keeps us
 * there.
 */
static int
parse_nextchar(struct parse *p)
{
	int	 c;

	c = fgetc(p->f);

	if (feof(p->f) || ferror(p->f))
		return(EOF);

	p->column++;

	if ('\n' == c) {
		p->line++;
		p->column = 0;
	}

	return(c);
}

/*
 * Parse the next token from "f".
 * If we've already encountered an error or an EOF condition, this
 * doesn't do anything.
 * Otherwise, lasttype will be set to the last token type.
 */
static enum tok
parse_next(struct parse *p)
{
	int		 c, last, hasdot, minus = 0;
	const char	*ep = NULL;
	char		*epp = NULL;

	if (TOK_ERR == p->lasttype || 
	    TOK_EOF == p->lasttype) 
		return(p->lasttype);

	/* 
	 * Read past any white-space.
	 * If we're at the EOF or error, just return.
	 */

	do {
		c = parse_nextchar(p);
	} while (isspace(c));

	if (feof(p->f) || ferror(p->f))
		return(parse_err(p));

	/* 
	 * The "-" appears before integers and doubles.
	 * It has no other syntactic function.
	 */

	if ('-' == c) {
		c = parse_nextchar(p);
		if (feof(p->f) || ferror(p->f))
			return(parse_err(p));
		if ( ! isdigit(c))
			return(parse_errx(p, "expected digit"));
		minus = 1;
	}

	/*
	 * Now we're at a hard token.
	 * It's either going to be a simple lexical token (like a
	 * right-brace) or part of a multi-character sequence.
	 */

	if ('}' == c) {
		p->lasttype = TOK_RBRACE;
	} else if ('{' == c) {
		p->lasttype = TOK_LBRACE;
	} else if (';' == c) {
		p->lasttype = TOK_SEMICOLON;
	} else if (',' == c) {
		p->lasttype = TOK_COMMA;
	} else if ('.' == c) {
		p->lasttype = TOK_PERIOD;
	} else if (':' == c) {
		p->lasttype = TOK_COLON;
	} else if ('"' == c) {
		p->bufsz = 0;
		last = ' ';
		for (;;) {
			/* 
			 * Continue until we get an unescaped quote or
			 * EOF. 
			 */
			c = parse_nextchar(p);
			if (EOF == c ||
			    ('"' == c && '\\' != last))
				break;
			if (isspace(last) && isspace(c)) {
				last = c;
				continue;
			}
			buf_push(p, c);
			last = c;
		} 

		if (ferror(p->f))
			return(parse_err(p));

		buf_push(p, '\0');
		p->last.string = p->buf;
		p->lasttype = TOK_LITERAL;
	} else if (isdigit(c)) {
		hasdot = 0;
		p->bufsz = 0;
		if (minus)
			buf_push(p, '-');
		buf_push(p, c);
		do {
			c = parse_nextchar(p);
			/*
			 * Check for a decimal number: if we encounter a
			 * full-stop within the number, we convert to a
			 * decimal value.
			 * But only for the first decimal point.
			 */
			if ('.' == c) {
				if (hasdot)
					break;
				hasdot = 1;
			}
			if (isdigit(c) || '.' == c)
				buf_push(p, c);
		} while (isdigit(c) || '.' == c);

		if (ferror(p->f))
			return(parse_err(p));
		else if ( ! feof(p->f))
			parse_ungetc(p, c);

		buf_push(p, '\0');
		if (hasdot) {
			p->last.decimal = strtod(p->buf, &epp);
			if (epp == p->buf || ERANGE == errno)
				return(parse_errx(p, 
					"malformed decimal"));
			p->lasttype = TOK_DECIMAL;
		} else {
			p->last.integer = strtonum
				(p->buf, -INT64_MAX, INT64_MAX, &ep);
			if (NULL != ep)
				return(parse_errx(p, 
					"malformed integer"));
			p->lasttype = TOK_INTEGER;
		}
	} else if (isalpha(c)) {
		p->bufsz = 0;
		buf_push(p, c);
		do {
			c = parse_nextchar(p);
			if (isalnum(c))
				buf_push(p, c);
		} while (isalnum(c));

		if (ferror(p->f))
			return(parse_err(p));
		else if ( ! feof(p->f))
			parse_ungetc(p, c);

		buf_push(p, '\0');
		p->last.string = p->buf;
		p->lasttype = TOK_IDENT;
	} else
		return(parse_errx(p, "unknown input token"));

	return(p->lasttype);
}

/*
 * Parse the quoted_string part following "comment".
 * Attach it to the given "doc", possibly clearing out any prior
 * comments.
 * If this is the case, emit a warning.
 */
static int
parse_comment(struct parse *p, char **doc)
{

	if (TOK_LITERAL != parse_next(p)) {
		parse_errx(p, "expected quoted string");
		return(0);
	} else if (NULL != *doc) {
		parse_warnx(p, "replaces prior comment");
		free(*doc);
	}

	if (NULL == (*doc = strdup(p->last.string)))
		err(EXIT_FAILURE, NULL);

	return(1);
}

/*
 * Parse the linkage for a structure.
 * Its syntax is:
 *
 *   source_field:target_struct.target_field
 *
 * These are queried later in the linkage phase.
 */
static void
parse_config_field_struct(struct parse *p, struct ref *r)
{

	if (TOK_IDENT != parse_next(p)) {
		parse_errx(p, "expected source field");
		return;
	} else if (NULL == (r->sfield = strdup(p->last.string)))
		err(EXIT_FAILURE, NULL);
	
	if (TOK_COLON != parse_next(p)) {
		parse_errx(p, "expected colon");
		return;
	}

	if (TOK_IDENT != parse_next(p)) {
		parse_errx(p, "expected struct table");
		return;
	} else if (NULL == (r->tstrct = strdup(p->last.string)))
		err(EXIT_FAILURE, NULL);

	if (TOK_PERIOD != parse_next(p)) {
		parse_errx(p, "expected period");
		return;
	}

	if (TOK_IDENT != parse_next(p)) {
		parse_errx(p, "expected struct field");
		return;
	} else if (NULL == (r->tfield = strdup(p->last.string)))
		err(EXIT_FAILURE, NULL);
}

static void
parse_validate(struct parse *p, struct field *fd)
{
	enum vtype	 vt;
	enum tok	 tok;
	struct fvalid	*v;

	if (FTYPE_STRUCT == fd->type) {
		parse_errx(p, "no validation on structs");
		return;
	}

	if (TOK_IDENT != parse_next(p)) {
		parse_errx(p, "expected constraint type");
		return;
	}

	vt = VALIDATE__MAX;
	if (0 == strcasecmp(p->last.string, "gt"))
		vt = VALIDATE_GT;
	else if (0 == strcasecmp(p->last.string, "ge"))
		vt = VALIDATE_GE;
	else if (0 == strcasecmp(p->last.string, "lt"))
		vt = VALIDATE_LT;
	else if (0 == strcasecmp(p->last.string, "le"))
		vt = VALIDATE_LE;

	if (VALIDATE__MAX == vt) {
		parse_errx(p, "unknown constraint type");
		return;
	}

	if (NULL == (v = calloc(1, sizeof(struct fvalid))))
		err(EXIT_FAILURE, NULL);
	v->type = vt;
	TAILQ_INSERT_TAIL(&fd->fvq, v, entries);

	/*
	 * For now, we have only a scalar value.
	 * In the future, this will have some conditionalising.
	 */

	switch (fd->type) {
	case (FTYPE_EPOCH):
	case (FTYPE_INT):
		if (TOK_INTEGER != parse_next(p)) {
			parse_errx(p, "expected integer");
			return;
		}
		v->d.value.integer = p->last.integer;
		break;
	case (FTYPE_REAL):
		tok = parse_next(p);
		if (TOK_DECIMAL != tok && TOK_INTEGER != tok) {
			parse_errx(p, "expected decimal");
			return;
		}
		v->d.value.decimal = 
			TOK_DECIMAL == tok ? p->last.decimal :
			p->last.integer;
		break;
	case (FTYPE_BLOB):
	case (FTYPE_TEXT):
	case (FTYPE_PASSWORD):
		if (TOK_INTEGER != parse_next(p)) {
			parse_errx(p, "expected length");
			return;
		} 
		if (p->last.integer < 0 ||
		    (uint64_t)p->last.integer > SIZE_MAX) {
			parse_errx(p, "length out of range");
			return;
		}
		v->d.value.len = p->last.integer;
		break;
	default:
		abort();
	}
}

/*
 * Read auxiliary information for a field.
 * Its syntax is:
 *
 *   [options | "comment" string_literal]* ";"
 *
 * The options are any of "rowid", "unique", or "noexport".
 * This will continue processing until the semicolon is reached.
 */
static void
parse_config_field_info(struct parse *p, struct field *fd)
{

	while (TOK_ERR != p->lasttype && TOK_EOF != p->lasttype) {
		if (TOK_SEMICOLON == parse_next(p))
			break;
		if (TOK_IDENT != p->lasttype) {
			parse_errx(p, "unknown field info token");
			break;
		}

		if (0 == strcasecmp(p->last.string, "rowid")) {
			/*
			 * This must be on an integer type, must not be
			 * on a foreign key reference, must not have its
			 * parent already having a rowid, must not take
			 * null values, and must not already be
			 * specified.
			 */

			if (FTYPE_INT != fd->type) {
				parse_errx(p, "rowid for non-int type");
				break;
			} else if (NULL != fd->ref) {
				parse_errx(p, "rowid on reference");
				break;
			} else if (NULL != fd->parent->rowid) {
				parse_errx(p, "struct already has rowid");
				break;
			} else if (FIELD_NULL & fd->flags) {
				parse_errx(p, "rowid can't be null");
				break;
			} 
			
			if (FIELD_UNIQUE & fd->flags) {
				parse_warnx(p, "unique is redundant");
				fd->flags &= ~FIELD_UNIQUE;
			}

			fd->flags |= FIELD_ROWID;
			fd->parent->rowid = fd;
		} else if (0 == strcasecmp(p->last.string, "noexport")) {
			if (FTYPE_PASSWORD == fd->type)
				parse_warnx(p, "noexport is redundant");
			fd->flags |= FIELD_NOEXPORT;
		} else if (0 == strcasecmp(p->last.string, "limit")) {
			parse_validate(p, fd);
		} else if (0 == strcasecmp(p->last.string, "unique")) {
			/* 
			 * This must not be on a foreign key reference
			 * and is ignored for rowids.
			 */

			if (FTYPE_STRUCT == fd->type) {
				parse_errx(p, "unique on struct");
				break;
			} else if (FIELD_ROWID & fd->flags) {
				parse_warnx(p, "unique is redunant");
				continue;
			}

			fd->flags |= FIELD_UNIQUE;
		} else if (0 == strcasecmp(p->last.string, "null")) {
			/*
			 * These fields can't be rowids, nor can they be
			 * struct types.
			 */

			if (FIELD_ROWID & fd->flags) {
				parse_errx(p, "rowid can't be null");
				break;
			} else if (FTYPE_STRUCT == fd->type) {
				parse_errx(p, "struct types can't be null");
				break;
			}

			fd->flags |= FIELD_NULL;
		} else if (0 == strcasecmp(p->last.string, "comment")) {
			parse_comment(p, &fd->doc);
		} else
			parse_errx(p, "unknown field info token");
	}
}

/*
 * Read an individual field declaration.
 * Its syntax is:
 *
 *   [:refstruct.reffield] TYPE TYPEINFO
 *
 * By default, fields are integers.  TYPE can be "int", "integer",
 * "text", or "txt".  
 * A reference clause triggers a foreign key reference.
 * The TYPEINFO depends upon the type and is processed by the
 * parse_config_field_struct() (for structs) but is always followed by
 * parse_config_field_info().
 */
static void
parse_config_field(struct parse *p, struct field *fd)
{

	if (TOK_SEMICOLON == parse_next(p))
		return;

	/* Check if this is a reference. */

	if (TOK_COLON == p->lasttype) {
		fd->ref = calloc(1, sizeof(struct ref));
		if (NULL == fd->ref)
			err(EXIT_FAILURE, NULL);

		fd->ref->parent = fd;
		fd->ref->sfield = strdup(fd->name);
		if (NULL == fd->ref->sfield)
			err(EXIT_FAILURE, NULL);

		if (TOK_IDENT != parse_next(p)) {
			parse_errx(p, "expected target field");
			return;
		}
		fd->ref->tstrct = strdup(p->last.string);
		if (NULL == fd->ref->tstrct)
			err(EXIT_FAILURE, NULL);

		if (TOK_PERIOD != parse_next(p)) {
			parse_errx(p, "expected period");
			return;
		}

		if (TOK_IDENT != parse_next(p)) {
			parse_errx(p, "expected field type");
			return;
		}
		fd->ref->tfield = strdup(p->last.string);
		if (NULL == fd->ref->tfield)
			err(EXIT_FAILURE, NULL);

		if (TOK_IDENT != parse_next(p)) {
			parse_errx(p, "expected field type");
			return;
		}
	} else if (TOK_IDENT != p->lasttype) {
		parse_errx(p, "expected field type");
		return;
	}

	/* int64_t */
	if (0 == strcasecmp(p->last.string, "int") ||
	    0 == strcasecmp(p->last.string, "integer")) {
		fd->type = FTYPE_INT;
		parse_config_field_info(p, fd);
		return;
	}
	/* epoch */
	if (0 == strcasecmp(p->last.string, "epoch")) {
		fd->type = FTYPE_EPOCH;
		parse_config_field_info(p, fd);
		return;
	}
	/* double */
	if (0 == strcasecmp(p->last.string, "real") ||
	    0 == strcasecmp(p->last.string, "double")) {
		fd->type = FTYPE_REAL;
		parse_config_field_info(p, fd);
		return;
	}
	/* char-array */
	if (0 == strcasecmp(p->last.string, "text") ||
	    0 == strcasecmp(p->last.string, "txt")) {
		fd->type = FTYPE_TEXT;
		parse_config_field_info(p, fd);
		return;
	}
	/* blob */
	if (0 == strcasecmp(p->last.string, "blob")) {
		fd->type = FTYPE_BLOB;
		fd->parent->flags |= STRCT_HAS_BLOB;
		parse_config_field_info(p, fd);
		return;
	}
	/* hash */
	if (0 == strcasecmp(p->last.string, "password") ||
	    0 == strcasecmp(p->last.string, "passwd")) {
		fd->type = FTYPE_PASSWORD;
		parse_config_field_info(p, fd);
		return;
	}
	/* fall-through */
	if (strcasecmp(p->last.string, "struct")) {
		parse_errx(p, "unknown field type");
		return;
	}

	fd->type = FTYPE_STRUCT;
	fd->ref = calloc(1, sizeof(struct ref));
	if (NULL == fd->ref)
		err(EXIT_FAILURE, NULL);

	fd->ref->parent = fd;

	parse_config_field_struct(p, fd->ref);
	parse_config_field_info(p, fd);
}

/*
 * Parse the field used in a search.  This is the FIELD designation in
 * parse_config_search().
 * This may consist of nested structures, which uses dot-notation to
 * signify the field within a field's reference structure.
 *
 *  field.[FIELD] | field
 *
 * FIXME: disallow name "rowid".
 */
static void
parse_config_search_terms(struct parse *p, struct sent *sent)
{
	struct sref	*sf;
	size_t		 sz;

	if (TOK_IDENT != parse_next(p)) {
		parse_errx(p, "expected field identifier");
		return;
	}
	sref_alloc(p, p->last.string, sent);

	while (TOK_ERR != p->lasttype && TOK_EOF != p->lasttype) {
		if (TOK_COMMA == parse_next(p) ||
		    TOK_SEMICOLON == p->lasttype ||
		    TOK_COLON == p->lasttype)
			break;

		/* 
		 * Parse the optional operator.
		 * After the operator, we'll have no more fields.
		 */

		if (TOK_IDENT == p->lasttype) {
			for (sent->op = 0; 
			     OPTYPE__MAX != sent->op; sent->op++)
				if (0 == strcasecmp(p->last.string, 
				    optypes[sent->op]))
					break;
			if (OPTYPE__MAX == sent->op) {
				parse_errx(p, "unknown operator");
				return;
			}
			if (TOK_COMMA == parse_next(p) ||
			    TOK_SEMICOLON == p->lasttype ||
			    TOK_COLON == p->lasttype)
				break;
			parse_errx(p, "expected field separator");
			return;
		}

		/* Next in field chain... */

		if (TOK_PERIOD != p->lasttype) {
			parse_errx(p, "expected field separator");
			return;
		} else if (TOK_IDENT != parse_next(p)) {
			parse_errx(p, "expected field identifier");
			return;
		}
		sref_alloc(p, p->last.string, sent);
	}

	/*
	 * Now fill in the search field's canonical and partial
	 * structure name.
	 * For example of the latter, if our fields are
	 * "user.company.name", this would be "user.company".
	 * For a singleton field (e.g., "userid"), this is NULL.
	 */

	TAILQ_FOREACH(sf, &sent->srq, entries) {
		if (NULL == sent->fname) {
			sent->fname = strdup(sf->name);
			if (NULL == sent->fname)
				err(EXIT_FAILURE, NULL);
			continue;
		}
		sz = strlen(sent->fname) +
		     strlen(sf->name) + 2;
		sent->fname = realloc(sent->fname, sz);
		strlcat(sent->fname, ".", sz);
		strlcat(sent->fname, sf->name, sz);
	}

	TAILQ_FOREACH(sf, &sent->srq, entries) {
		if (NULL == TAILQ_NEXT(sf, entries))
			break;
		if (NULL == sent->name) {
			sent->name = strdup(sf->name);
			if (NULL == sent->name)
				err(EXIT_FAILURE, NULL);
			continue;
		}
		sz = strlen(sent->name) +
		     strlen(sf->name) + 2;
		sent->name = realloc(sent->name, sz);
		strlcat(sent->name, ".", sz);
		strlcat(sent->name, sf->name, sz);
	}
}

/*
 * Parse the search parameters following the search fields:
 *
 *  "search" search_fields ":" [key val]* ";"
 *
 * The "key" can be "name" or "comment"; and the name, a unique function
 * name or a comment literal.
 */
static void
parse_config_search_params(struct parse *p, struct search *s)
{
	struct search	*ss;

	if (TOK_SEMICOLON == parse_next(p))
		return;

	while (TOK_ERR != p->lasttype && TOK_EOF != p->lasttype) {
		if (TOK_IDENT != p->lasttype) {
			parse_errx(p, "expected search specifier");
			break;
		}

		if (0 == strcasecmp("name", p->last.string)) {
			if (TOK_IDENT != parse_next(p)) {
				parse_errx(p, "expected search name");
				break;
			}

			/* Disallow duplicate names. */

			TAILQ_FOREACH(ss, &s->parent->sq, entries) {
				if (NULL == ss->name ||
				    strcasecmp(ss->name, p->last.string))
					continue;
				parse_errx(p, "duplicate search name");
				break;
			}

			/* XXX: warn of prior */
			free(s->name);
			s->name = strdup(p->last.string);
			if (NULL == s->name)
				err(EXIT_FAILURE, NULL);
			if (TOK_SEMICOLON == parse_next(p))
				break;
		} else if (0 == strcasecmp("comment", p->last.string)) {
			if ( ! parse_comment(p, &s->doc))
				break;
			if (TOK_SEMICOLON == parse_next(p))
				break;
		} else {
			parse_errx(p, "unknown search parameter");
			break;
		}
	}

	assert(TOK_SEMICOLON == p->lasttype ||
	       (TOK_ERR == p->lasttype || 
		TOK_EOF == p->lasttype));
}

/*
 * Parse a unique clause.
 * This has the following syntax:
 *
 *  "unique" [field]2+ ";"
 *
 * The fields are within the current structure.
 */
static void
parse_config_unique(struct parse *p, struct strct *s)
{
	struct unique	*up, *upp;
	struct nref	*n;
	size_t		 sz, num = 0;

	if (NULL == (up = calloc(1, sizeof(struct unique))))
		err(EXIT_FAILURE, NULL);

	up->parent = s;
	parse_point(p, &up->pos);
	TAILQ_INIT(&up->nq);
	TAILQ_INSERT_TAIL(&s->nq, up, entries);

	while (TOK_ERR != p->lasttype && TOK_EOF != p->lasttype) {
		if (TOK_IDENT != parse_next(p)) {
			parse_errx(p, "expected unique field");
			break;
		}
		nref_alloc(p, p->last.string, up);
		num++;
		if (TOK_SEMICOLON == parse_next(p))
			break;
		if (TOK_COMMA == p->lasttype)
			continue;
		parse_errx(p, "unknown unique token");
	}

	if (num < 2) {
		parse_errx(p, "at least two fields "
			"required for unique constraint");
		return;
	}

	/* Establish canonical name of search. */

	sz = 0;
	TAILQ_FOREACH(n, &up->nq, entries) {
		sz += strlen(n->name) + 1; /* comma */
		up->cname = realloc(up->cname, sz + 1);
		if (NULL == up->cname)
			err(EXIT_FAILURE, NULL);
		strlcat(up->cname, n->name, sz + 1);
		strlcat(up->cname, ",", sz + 1);
	}
	assert(sz > 0);
	up->cname[sz - 1] = '\0';

	/* Check for duplicate unique constraint. */

	TAILQ_FOREACH(upp, &s->nq, entries) {
		if (upp == up || strcasecmp(upp->cname, up->cname)) 
			continue;
		parse_errx(p, "duplicate unique constraint");
		return;
	}
}

/*
 * Parse an update clause.
 * This has the following syntax:
 *
 *  "update" [ ufield [,ufield]* ]?
 *       ":" sfield [,sfield]*
 *     [ ":" [ "name" name | "comment" quoted_string ]* ] ?
 *       ";"
 *
 * The fields ("ufield" for update field and "sfield" for select field)
 * are within the current structure.
 * These are only for UPT_MODIFY parses.
 * Note that "sfield" also contains an optional operator, just like in
 * the search parameters.
 */
static void
parse_config_update(struct parse *p, struct strct *s, enum upt type)
{
	struct update	*up;
	struct uref	*ur;

	if (NULL == (up = calloc(1, sizeof(struct update))))
		err(EXIT_FAILURE, NULL);
	up->parent = s;
	up->type = type;
	TAILQ_INIT(&up->mrq);
	TAILQ_INIT(&up->crq);

	if (UP_MODIFY == up->type)
		TAILQ_INSERT_TAIL(&s->uq, up, entries);
	else
		TAILQ_INSERT_TAIL(&s->dq, up, entries);

	/* 
	 * For modifiers, start with the fields that will be updated.
	 * (At least one field will be updated.)
	 * This is followed by a colon.
	 */

	if (UP_MODIFY == up->type) {
		if (TOK_IDENT != parse_next(p)) {
			parse_errx(p, "expected field to modify");
			return;
		}
		uref_alloc(p, p->last.string, up, &up->mrq);
		while (TOK_COLON != parse_next(p)) {
			if (TOK_COMMA != p->lasttype) {
				parse_errx(p, "expected separator");
				return;
			} else if (TOK_IDENT != parse_next(p)) {
				parse_errx(p, "expected modify field");
				return;
			}
			uref_alloc(p, p->last.string, up, &up->mrq);
		}
	}

	/*
	 * Now the fields that will be used to constrain the update
	 * mechanism.
	 * Usually this will be a rowid.
	 * This is followed by a semicolon or colon.
	 */

	if (TOK_IDENT != parse_next(p)) {
		parse_errx(p, "expected select field");
		return;
	}
	ur = uref_alloc(p, p->last.string, up, &up->crq);

	while (TOK_COLON != parse_next(p)) {
		if (TOK_SEMICOLON == p->lasttype)
			return;

		/* Parse optional operator. */

		if (TOK_IDENT == p->lasttype) {
			for (ur->op = 0; 
			     OPTYPE__MAX != ur->op; ur->op++)
				if (0 == strcasecmp(p->last.string, 
				    optypes[ur->op]))
					break;
			if (OPTYPE__MAX == ur->op) {
				parse_errx(p, "unknown operator");
				return;
			}
			if (TOK_COLON == parse_next(p))
				break;
			else if (TOK_SEMICOLON == p->lasttype)
				return;
		}

		if (TOK_COMMA != p->lasttype) {
			parse_errx(p, "expected fields separator");
			return;
		} else if (TOK_IDENT != parse_next(p)) {
			parse_errx(p, "expected select field");
			return;
		}
		ur = uref_alloc(p, p->last.string, up, &up->crq);
	}

	/*
	 * Lastly, process update terms.
	 * This now consists of "name" and "comment".
	 */

	while (TOK_ERR != p->lasttype && TOK_EOF != p->lasttype) {
		if (TOK_SEMICOLON == parse_next(p))
			break;
		if (TOK_IDENT != p->lasttype) {
			parse_errx(p, "expected update modifier");
			return;
		}

		if (0 == strcasecmp(p->last.string, "name")) {
			if (TOK_IDENT != parse_next(p)) {
				parse_errx(p, "expected update name");
				return;
			}
			/* FIXME: warn of prior */
			free(up->name);
			up->name = strdup(p->last.string);
			if (NULL == up->name)
				err(EXIT_FAILURE, NULL);
		} else if (0 == strcasecmp(p->last.string, "comment")) {
			parse_comment(p, &up->doc);
		} else
			parse_errx(p, "unknown update parameter");
	}
}

/*
 * Parse a search clause.
 * This has the following syntax:
 *
 *  "search" [ search_terms ]+ [":" search_params ]? ";"
 *
 * The terms (searchable field) parts are parsed in
 * parse_config_search_terms().
 * The params are in parse_config_search_params().
 */
static void
parse_config_search(struct parse *p, struct strct *s, enum stype stype)
{
	struct search	*srch;
	struct sent	*sent;

	if (NULL == (srch = calloc(1, sizeof(struct search))))
		err(EXIT_FAILURE, NULL);
	srch->parent = s;
	srch->type = stype;
	parse_point(p, &srch->pos);
	TAILQ_INIT(&srch->sntq);
	TAILQ_INSERT_TAIL(&s->sq, srch, entries);

	if (STYPE_LIST == stype)
		s->flags |= STRCT_HAS_QUEUE;
	else if (STYPE_ITERATE == stype)
		s->flags |= STRCT_HAS_ITERATOR;

	/* We need at least one search term. */

	sent = sent_alloc(p, srch);
	parse_config_search_terms(p, sent);

	/* Now for remaining terms... */

	while (TOK_COMMA == p->lasttype) {
		sent = sent_alloc(p, srch);
		parse_config_search_terms(p, sent);
	}

	if (TOK_COLON == p->lasttype)
		parse_config_search_params(p, srch);

	assert(TOK_SEMICOLON == p->lasttype ||
	       (TOK_ERR == p->lasttype || 
		TOK_EOF == p->lasttype));
}

/*
 * Read an individual structure.
 * This opens and closes the structure, then reads all of the field
 * elements within.
 * Its syntax is:
 * 
 *  "{" 
 *    ["field" ident FIELD]+ 
 *    [["iterate" | "search" | "list" ] search_fields]*
 *    ["update" update_fields]*
 *    ["comment" quoted_string]?
 *  "};"
 */
static void
parse_config_struct(struct parse *p, struct strct *s)
{
	struct field	  *fd;
	const char *const *cp;

	if (TOK_LBRACE != parse_next(p)) {
		parse_errx(p, "expected left brace");
		return;
	}

	while ( ! PARSE_STOP(p)) {
		if (TOK_RBRACE == parse_next(p))
			break;
		if (TOK_IDENT != p->lasttype) {
			parse_errx(p, "expected field entry type");
			return;
		}

		if (0 == strcasecmp(p->last.string, "comment")) {
			if ( ! parse_comment(p, &s->doc))
				return;
			if (TOK_SEMICOLON != parse_next(p)) {
				parse_errx(p, "expected end of comment");
				return;
			}
			continue;
		} else if (0 == strcasecmp(p->last.string, "search")) {
			parse_config_search(p, s, STYPE_SEARCH);
			continue;
		} else if (0 == strcasecmp(p->last.string, "list")) {
			parse_config_search(p, s, STYPE_LIST);
			continue;
		} else if (0 == strcasecmp(p->last.string, "iterate")) {
			parse_config_search(p, s, STYPE_ITERATE);
			continue;
		} else if (0 == strcasecmp(p->last.string, "update")) {
			parse_config_update(p, s, UP_MODIFY);
			continue;
		} else if (0 == strcasecmp(p->last.string, "delete")) {
			parse_config_update(p, s, UP_DELETE);
			continue;
		} else if (0 == strcasecmp(p->last.string, "unique")) {
			parse_config_unique(p, s);
			continue;
		}
		
		if (strcasecmp(p->last.string, "field")) {
			parse_errx(p, "expected field entry type");
			return;
		}

		/* Now we have a new field. */

		if (TOK_IDENT != parse_next(p)) {
			parse_errx(p, "expected field name");
			return;
		}

		/* Disallow duplicate names. */

		TAILQ_FOREACH(fd, &s->fq, entries) {
			if (strcasecmp(fd->name, p->last.string))
				continue;
			parse_errx(p, "duplicate name");
			return;
		}

		/* Disallow bad names. */

		for (cp = badidents; NULL != *cp; cp++)
			if (0 == strcasecmp(*cp, p->last.string)) {
				parse_errx(p, "illegal identifier");
				return;
			}

		/*
		 * Allocate the field entry by name and then continue
		 * parsing the field attributes.
		 */

		if (NULL == (fd = calloc(1, sizeof(struct field))))
			err(EXIT_FAILURE, NULL);
		if (NULL == (fd->name = strdup(p->last.string)))
			err(EXIT_FAILURE, NULL);

		fd->type = FTYPE_INT;
		fd->parent = s;
		parse_point(p, &fd->pos);
		TAILQ_INIT(&fd->fvq);
		TAILQ_INSERT_TAIL(&s->fq, fd, entries);
		parse_config_field(p, fd);
	}

	if (PARSE_STOP(p))
		return;

	if (TOK_SEMICOLON != parse_next(p)) {
		parse_errx(p, "expected semicolon");
		return;
	}

	if (TAILQ_EMPTY(&s->fq)) {
		warnx("%s:%zu:%zu: no fields",
			p->fname, p->line, p->column);
		p->lasttype = TOK_ERR;
	}
}

/*
 * Top-level parse.
 * Read until we reach an identifier for a structure.
 * Once we reach one, parse for that structure.
 * Then continue until we've read all structures.
 * Its syntax is:
 *
 *  [ "struct" ident STRUCT ]+
 *
 * Where STRUCT is defined in parse_config_struct and ident is a unique,
 * alphanumeric (starting with alpha), non-reserved string.
 */
struct strctq *
parse_config(FILE *f, const char *fname)
{
	struct strctq	  *q;
	struct strct	  *s;
	struct parse	   p;
	const char *const *cp;
	char		  *caps;

	if (NULL == (q = malloc(sizeof(struct strctq))))
		err(EXIT_FAILURE, NULL);

	TAILQ_INIT(q);

	memset(&p, 0, sizeof(struct parse));
	p.column = 0;
	p.line = 1;
	p.fname = fname;
	p.f = f;

	for (;;) {
		if (TOK_ERR == parse_next(&p))
			goto error;
		else if (TOK_EOF == p.lasttype)
			break;

		if (TOK_IDENT != p.lasttype ||
		    strcasecmp(p.last.string, "struct")) {
			parse_errx(&p, "expected struct");
			goto error;
		}

		if (TOK_IDENT != parse_next(&p)) {
			parse_errx(&p, NULL);
			goto error;
		}

		/* Disallow duplicate names. */

		TAILQ_FOREACH(s, q, entries) {
			if (strcasecmp(s->name, p.last.string))
				continue;
			parse_errx(&p, "duplicate name");
			goto error;
		}

		/* Disallow bad names. */

		for (cp = badidents; NULL != *cp; cp++)
			if (0 == strcasecmp(*cp, p.last.string)) {
				parse_errx(&p, "illegal identifier");
				goto error;
			}

		/*
		 * Create the new structure with the given name and add
		 * it to the queue of structures.
		 * Then parse its fields.
		 */

		if (NULL == (s = calloc(1, sizeof(struct strct))))
			err(EXIT_FAILURE, NULL);
		if (NULL == (s->name = strdup(p.last.string)))
			err(EXIT_FAILURE, NULL);
		if (NULL == (s->cname = strdup(s->name)))
			err(EXIT_FAILURE, NULL);
		for (caps = s->cname; '\0' != *caps; caps++)
			*caps = toupper((int)*caps);

		parse_point(&p, &s->pos);
		TAILQ_INSERT_TAIL(q, s, entries);
		TAILQ_INIT(&s->fq);
		TAILQ_INIT(&s->sq);
		TAILQ_INIT(&s->aq);
		TAILQ_INIT(&s->uq);
		TAILQ_INIT(&s->nq);
		TAILQ_INIT(&s->dq);
		parse_config_struct(&p, s);
	}

	if (TAILQ_EMPTY(q)) {
		warnx("%s: no structures", fname);
		goto error;
	}

	free(p.buf);
	return(q);
error:
	free(p.buf);
	parse_free(q);
	return(NULL);
}

/*
 * Free a field reference.
 * Does nothing if "p" is NULL.
 */
static void
parse_free_ref(struct ref *p)
{

	if (NULL == p)
		return;
	free(p->sfield);
	free(p->tfield);
	free(p->tstrct);
	free(p);
}

/*
 * Free a field entity.
 * Does nothing if "p" is NULL.
 */
static void
parse_free_field(struct field *p)
{
	struct fvalid *fv;

	if (NULL == p)
		return;
	while (NULL != (fv = TAILQ_FIRST(&p->fvq))) {
		TAILQ_REMOVE(&p->fvq, fv, entries);
		free(fv);
	}
	parse_free_ref(p->ref);
	free(p->doc);
	free(p->name);
	free(p);
}

/*
 * Free a search series.
 * Does nothing if "p" is NULL.
 */
static void
parse_free_search(struct search *p)
{
	struct sref	*s;
	struct sent	*sent;

	if (NULL == p)
		return;

	while (NULL != (sent = TAILQ_FIRST(&p->sntq))) {
		TAILQ_REMOVE(&p->sntq, sent, entries);
		while (NULL != (s = TAILQ_FIRST(&sent->srq))) {
			TAILQ_REMOVE(&sent->srq, s, entries);
			free(s->name);
			free(s);
		}
		free(sent->fname);
		free(sent->name);
		free(sent);
	}
	free(p->doc);
	free(p->name);
	free(p);
}

/*
 * Free a unique reference.
 * Does nothing if "p" is NULL.
 */
static void
parse_free_nref(struct nref *u)
{

	if (NULL == u)
		return;
	free(u->name);
	free(u);
}

/*
 * Free an update reference.
 * Does nothing if "p" is NULL.
 */
static void
parse_free_uref(struct uref *u)
{

	if (NULL == u)
		return;
	free(u->name);
	free(u);
}

/*
 * Free a unique series.
 * Does nothing if "p" is NULL.
 */
static void
parse_free_unique(struct unique *p)
{
	struct nref	*u;

	if (NULL == p)
		return;

	while (NULL != (u = TAILQ_FIRST(&p->nq))) {
		TAILQ_REMOVE(&p->nq, u, entries);
		parse_free_nref(u);
	}

	free(p);
}

/*
 * Free an update series.
 * Does nothing if "p" is NULL.
 */
static void
parse_free_update(struct update *p)
{
	struct uref	*u;

	if (NULL == p)
		return;

	while (NULL != (u = TAILQ_FIRST(&p->mrq))) {
		TAILQ_REMOVE(&p->mrq, u, entries);
		parse_free_uref(u);
	}
	while (NULL != (u = TAILQ_FIRST(&p->crq))) {
		TAILQ_REMOVE(&p->crq, u, entries);
		parse_free_uref(u);
	}

	free(p->doc);
	free(p->name);
	free(p);
}

/*
 * Free all resources from the queue of structures.
 * Does nothing if "q" is empty or NULL.
 */
void
parse_free(struct strctq *q)
{
	struct strct	*p;
	struct field	*f;
	struct search	*s;
	struct alias	*a;
	struct update	*u;
	struct unique	*n;

	if (NULL == q)
		return;

	while (NULL != (p = TAILQ_FIRST(q))) {
		TAILQ_REMOVE(q, p, entries);
		while (NULL != (f = TAILQ_FIRST(&p->fq))) {
			TAILQ_REMOVE(&p->fq, f, entries);
			parse_free_field(f);
		}
		while (NULL != (s = TAILQ_FIRST(&p->sq))) {
			TAILQ_REMOVE(&p->sq, s, entries);
			parse_free_search(s);
		}
		while (NULL != (a = TAILQ_FIRST(&p->aq))) {
			TAILQ_REMOVE(&p->aq, a, entries);
			free(a->name);
			free(a->alias);
			free(a);
		}
		while (NULL != (u = TAILQ_FIRST(&p->uq))) {
			TAILQ_REMOVE(&p->uq, u, entries);
			parse_free_update(u);
		}
		while (NULL != (u = TAILQ_FIRST(&p->dq))) {
			TAILQ_REMOVE(&p->dq, u, entries);
			parse_free_update(u);
		}
		while (NULL != (n = TAILQ_FIRST(&p->nq))) {
			TAILQ_REMOVE(&p->nq, n, entries);
			parse_free_unique(n);
		}
		free(p->doc);
		free(p->name);
		free(p->cname);
		free(p);
	}
	free(q);
}
