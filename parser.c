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
#include <time.h>

#include "ort.h"
#include "extern.h"

/*
 * These are our lexical tokens parsed from input.
 */
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

/*
 * Shorthand for checking whether we should stop parsing (we're at the
 * end of file or have an error).
 */
#define	PARSE_STOP(_p) \
	(TOK_ERR == (_p)->lasttype || TOK_EOF == (_p)->lasttype)

struct	parsefile {
	FILE		*f;
};

/*
 * Used for parsing from a buffer (PARSETYPE_BUF) instead of a file.
 */
struct	parsebuf {
	const char	*buf; /* the (possibly binary) buffer */
	size_t		 len; /* length of the buffer */
	size_t		 pos; /* position during the parse */
};

enum	parsetype {
	PARSETYPE_BUF, /* a character buffer */
	PARSETYPE_FILE /* a FILE stream */
};

/*
 * The current parse.
 * If we have multiple files to parse, we must reinitialise this
 * structure (including freeing resources, for now limited to "buf"), as
 * it only pertains to the current one.
 */
struct	parse {
	union {
		const char *string; /* last parsed string */
		int64_t integer; /* ...integer */
		double decimal; /* ...double */
	} last;
	enum tok	 lasttype; /* last parse type */
	char		*buf; /* buffer for storing up reads */
	size_t		 bufsz; /* length of buffer */
	size_t		 bufmax; /* maximum buffer size */
	size_t		 line; /* current line (from 1) */
	size_t		 column; /* current column (from 1) */
	const char	*fname; /* current filename */
	struct config	*cfg; /* current configuration */
	enum parsetype	 type; /* how we're getting input */
	union {
		struct parsebuf inbuf; /* from a buffer */
		struct parsefile infile; /* ...file */
	};
};

static	const char *const rolemapts[ROLEMAP__MAX] = {
	"all", /* ROLEMAP_ALL */
	"delete", /* ROLEMAP_DELETE */
	"insert", /* ROLEMAP_INSERT */
	"iterate", /* ROLEMAP_ITERATE */
	"list", /* ROLEMAP_LIST */
	"search", /* ROLEMAP_SEARCH */
	"update", /* ROLEMAP_UPDATE */
	"noexport", /* ROLEMAP_NOEXPORT */
};

/*
 * Disallowed field names.
 * The SQL ones are from https://sqlite.org/lang_keywords.html.
 * FIXME: think about this more carefully, as in SQL, there are many
 * things that we can put into string literals.
 * FIXME: this is in config.c now. 
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

static	const char *const modtypes[MODTYPE__MAX] = {
	"set", /* MODTYPE_SET */
	"inc", /* MODTYPE_INC */
	"dec", /* MODTYPE_DEC */
	"concat", /* MODTYPE_CONCAT */
};

static	const char *const optypes[OPTYPE__MAX] = {
	"eq", /* OPTYE_EQUAL */
	"ge", /* OPTYPE_GE */
	"gt", /* OPTYPE_GT */
	"le", /* OPTYPE_LE */
	"lt", /* OPTYPE_LT */
	"neq", /* OPTYE_NEQUAL */
	"like", /* OPTYPE_LIKE */
	"and", /* OPTYPE_AND */
	"or", /* OPTYPE_OR */
	/* Unary types... */
	"isnull", /* OPTYE_ISNULL */
	"notnull", /* OPTYE_NOTNULL */
};

static	const char *const vtypes[VALIDATE__MAX] = {
	"ge", /* VALIDATE_GE */
	"le", /* VALIDATE_LE */
	"gt", /* VALIDATE_GT */
	"lt", /* VALIDATE_LT */
	"eq", /* VALIDATE_EQ */
};

static	const char *const channel = "parser";

static enum tok parse_errx(struct parse *, const char *, ...)
	__attribute__((format(printf, 2, 3)));
static void parse_warnx(struct parse *p, const char *, ...)
	__attribute__((format(printf, 2, 3)));

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

static void
parse_warnx(struct parse *p, const char *fmt, ...)
{
	va_list	 	 ap;
	struct pos	 pos;

	pos.fname = p->fname;
	pos.line = p->line;
	pos.column = p->column;

	if (NULL != fmt) {
		va_start(ap, fmt);
		ort_config_msgv(p->cfg, MSGTYPE_WARN, 
			channel, 0, &pos, fmt, ap);
		va_end(ap);
	} else
		ort_config_msg(p->cfg, MSGTYPE_WARN, 
			channel, 0, &pos, NULL);
}

static enum tok
parse_err(struct parse *p)
{
	int	 	 er = errno;
	struct pos	 pos;

	pos.fname = p->fname;
	pos.line = p->line;
	pos.column = p->column;

	ort_config_msg(p->cfg, MSGTYPE_FATAL, 
		channel, er, &pos, NULL);

	p->lasttype = TOK_ERR;
	return p->lasttype;
}

static enum tok
parse_errx(struct parse *p, const char *fmt, ...)
{
	va_list	 	 ap;
	struct pos	 pos;

	pos.fname = p->fname;
	pos.line = p->line;
	pos.column = p->column;

	if (NULL != fmt) {
		va_start(ap, fmt);
		ort_config_msgv(p->cfg, MSGTYPE_ERROR, 
			channel, 0, &pos, fmt, ap);
		va_end(ap);
	} else
		ort_config_msg(p->cfg, MSGTYPE_ERROR, 
			channel, 0, &pos, NULL);

	p->lasttype = TOK_ERR;
	return p->lasttype;
}

/*
 * Push a single character into the retaining buffer.
 * XXX: creates a binary buffer.
 * Returns zero on memory failure, non-zero otherwise.
 */
static int
buf_push(struct parse *p, char c)
{
	void		*pp;
	const size_t	 inc = 1024;

	if (p->bufsz + 1 >= p->bufmax) {
		pp = realloc(p->buf, p->bufmax + inc);
		if (NULL == pp) {
			parse_err(p);
			return 0;
		}
		p->buf = pp;
		p->bufmax += inc;
	}
	p->buf[p->bufsz++] = c;
	return 1;
}

/*
 * Iterate through the list of reserved keywords and return non-zero if
 * the current string (case insensitively) matches, zero otherwise.
 * FIXME: this is in config.c now. 
 */
static int
check_badidents(struct parse *p, const char *s)
{
	const char *const *cp;

	for (cp = badidents; NULL != *cp; cp++)
		if (0 == strcasecmp(*cp, s)) {
			parse_errx(p, "illegal identifier");
			return(0);
		}

	return(1);
}

/* FIXME: this is in config.c now. */
static int
check_dupetoplevel(struct parse *p, const char *name)
{
	const struct enm *e;
	const struct bitf *b;
	const struct strct *s;

	TAILQ_FOREACH(e, &p->cfg->eq, entries)
		if (0 == strcasecmp(e->name, name)) {
			parse_errx(p, "duplicates enum name: "
				"%s:%zu:%zu", e->pos.fname, 
				e->pos.line, e->pos.column);
			return(0);
		}
	TAILQ_FOREACH(b, &p->cfg->bq, entries)
		if (0 == strcasecmp(b->name, name)) {
			parse_errx(p, "duplicates bitfield name: "
				"%s:%zu:%zu", b->pos.fname, 
				b->pos.line, b->pos.column);
			return(0);
		}
	TAILQ_FOREACH(s, &p->cfg->sq, entries) 
		if (0 == strcasecmp(s->name, name)) {
			parse_errx(p, "duplicates struct name: "
				"%s:%zu:%zu", s->pos.fname, 
				s->pos.line, s->pos.column);
			return(0);
		}
	
	return(1);
}

/*
 * Appends a reference "v" to "p".
 * If "p" is NULL, this will simply duplicate "p".
 * Else, "p" += "." "v".
 * Returns zero on allocation failure, non-zero otherwise.
 */
static int
ref_append(char **p, const char *v)
{
	size_t	 sz;
	void	*pp;

	if (NULL == *p)
		return(NULL != (*p = strdup(v)));

	assert(NULL != *p);
	sz = strlen(*p) + strlen(v) + 2;
	if (NULL == (pp = realloc(*p, sz)))
		return(0);

	*p = pp;
	strlcat(*p, ".", sz);
	strlcat(*p, v, sz);
	return(1);
}

/*
 * Allocate a unique reference and add it to the parent queue in order
 * by alpha.
 * Returns the created pointer or NULL.
 */
static struct nref *
nref_alloc(struct parse *p, const char *name, struct unique *up)
{
	struct nref	*ref, *n;

	if (NULL == (ref = calloc(1, sizeof(struct nref)))) {
		parse_err(p);
		return NULL;
	} else if (NULL == (ref->name = strdup(name))) {
		free(ref);
		parse_err(p);
		return NULL;
	}

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

	return ref;
}

/*
 * Allocate an update reference and add it to the parent queue.
 * Returns the created pointer or NULL.
 */
static struct uref *
uref_alloc(struct parse *p, const char *name, 
	struct update *up, struct urefq *q)
{
	struct uref	*ref;

	if (NULL == (ref = calloc(1, sizeof(struct uref)))) {
		parse_err(p);
		return NULL;
	} else if (NULL == (ref->name = strdup(name))) {
		free(ref);
		parse_err(p);
		return NULL;
	}

	ref->parent = up;
	parse_point(p, &ref->pos);
	TAILQ_INSERT_TAIL(q, ref, entries);
	return ref;
}

/*
 * Allocate a search reference and add it to the parent queue.
 * Returns the created pointer or NULL.
 */
static int
sref_alloc(struct parse *p, const char *name, struct srefq *q)
{
	struct sref	*ref;

	if (NULL == (ref = calloc(1, sizeof(struct sref)))) {
		parse_err(p);
		return 0;
	} else if (NULL == (ref->name = strdup(name))) {
		free(ref);
		parse_err(p);
		return 0;
	}

	parse_point(p, &ref->pos);
	TAILQ_INSERT_TAIL(q, ref, entries);
	return 1;
}

/*
 * Allocate a search entity and add it to the parent queue.
 * Returns the created pointer or NULL.
 */
static struct sent *
sent_alloc(struct parse *p, struct search *up)
{
	struct sent	*sent;

	if (NULL == (sent = calloc(1, sizeof(struct sent)))) {
		parse_err(p);
		return NULL;
	}

	sent->parent = up;
	parse_point(p, &sent->pos);
	TAILQ_INIT(&sent->srq);
	TAILQ_INSERT_TAIL(&up->sntq, sent, entries);
	return sent;
}

static int
parse_check_eof(struct parse *p)
{

	if (PARSETYPE_BUF == p->type)
		return p->inbuf.pos >= p->inbuf.len;

	assert(PARSETYPE_FILE == p->type);
	assert(NULL != p->infile.f);
	return feof(p->infile.f);
}

static int
parse_check_error(struct parse *p)
{

	if (PARSETYPE_BUF == p->type)
		return 0;

	assert(PARSETYPE_FILE == p->type);
	assert(NULL != p->infile.f);
	return ferror(p->infile.f);
}

static int
parse_check_error_eof(struct parse *p)
{

	if (PARSETYPE_BUF == p->type)
		return p->inbuf.pos >= p->inbuf.len;

	assert(PARSETYPE_FILE == p->type);
	assert(NULL != p->infile.f);
	return feof(p->infile.f) || ferror(p->infile.f);
}

static void
parse_ungetc(struct parse *p, int c)
{

	/* FIXME: puts us at last line, not column */

	if ('\n' == c)
		p->line--;
	else if (p->column > 0)
		p->column--;

	if (PARSETYPE_BUF == p->type) {
		assert(p->inbuf.pos > 0);
		p->inbuf.pos--;
		return;
	}

	assert(PARSETYPE_FILE == p->type);
	ungetc(c, p->infile.f);
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

	if (PARSETYPE_BUF == p->type)
		c = p->inbuf.pos >= p->inbuf.len ?
			EOF : p->inbuf.buf[p->inbuf.pos++];
	else
		c = fgetc(p->infile.f);

	if (parse_check_error_eof(p))
		return EOF;

	p->column++;

	if ('\n' == c) {
		p->line++;
		p->column = 0;
	}

	return c;
}

/*
 * Trigger a "hard" error condition at stream error or EOF.
 * This sets the lasttype appropriately.
 */
static enum tok
parse_read_err(struct parse *p)
{

	if (parse_check_error(p))
		parse_err(p);
	else 
		p->lasttype = TOK_EOF;

	return p->lasttype;
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

again:
	if (PARSE_STOP(p))
		return(p->lasttype);

	/* 
	 * Read past any white-space.
	 * If we're at the EOF or error, just return.
	 */

	do {
		c = parse_nextchar(p);
	} while (isspace(c));

	if (parse_check_error_eof(p))
		return(parse_read_err(p));

	if ('#' == c) {
		do {
			c = parse_nextchar(p);
		} while (EOF != c && '\n' != c);
		if ('\n' != c)
			return(parse_read_err(p));
		goto again;
	}

	/* 
	 * The "-" appears before integers and doubles.
	 * It has no other syntactic function.
	 */

	if ('-' == c) {
		c = parse_nextchar(p);
		if (parse_check_error_eof(p))
			return(parse_read_err(p));
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
			if ( ! buf_push(p, c))
				return TOK_ERR;
			last = c;
		} 

		if (parse_check_error(p))
			return(parse_read_err(p));

		if ( ! buf_push(p, '\0'))
			return TOK_ERR;
		p->last.string = p->buf;
		p->lasttype = TOK_LITERAL;
	} else if (isdigit(c)) {
		hasdot = 0;
		p->bufsz = 0;
		if (minus && ! buf_push(p, '-'))
			return TOK_ERR;
		if ( ! buf_push(p, c))
			return TOK_ERR;

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
				if ( ! buf_push(p, c))
					return TOK_ERR;
		} while (isdigit(c) || '.' == c);

		if (parse_check_error(p))
			return(parse_read_err(p));
		else if ( ! parse_check_eof(p))
			parse_ungetc(p, c);

		if ( ! buf_push(p, '\0'))
			return TOK_ERR;
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
		if ( ! buf_push(p, c))
			return TOK_ERR;
		do {
			c = parse_nextchar(p);
			if (isalnum(c) && ! buf_push(p, c))
				return TOK_ERR;
		} while (isalnum(c));

		if (parse_check_error(p))
			return(parse_read_err(p));
		else if ( ! parse_check_eof(p))
			parse_ungetc(p, c);

		if ( ! buf_push(p, '\0'))
			return TOK_ERR;
		p->last.string = p->buf;
		p->lasttype = TOK_IDENT;
	} else
		return(parse_errx(p, "unknown input token"));

	return(p->lasttype);
}

/*
 * Parse the quoted_string part following "jslabel".
 * Its syntax is:
 *
 *   jslabel ["." language] string_literal
 *
 * Attach it to the given queue of labels, possibly clearing out any
 * prior labels of the same language.
 * If this is the case, emit a warning.
 * Return zero on failure, non-zero on success.
 */
static int
parse_label(struct parse *p, struct labelq *q)
{
	size_t	 	 lang = 0;
	struct label	*l;
	const char	*cp;
	void		*pp;

	/* 
	 * If we have a period (like jslabel.en), then interpret the
	 * word after the period as a language code for which we'll use
	 * the label.
	 */

	if (TOK_PERIOD == parse_next(p)) {
		if (TOK_IDENT != parse_next(p)) {
			parse_errx(p, "expected language");
			return 0;
		}
		assert('\0' != p->last.string[0]);
		for ( ; lang < p->cfg->langsz; lang++) 
			if (0 == strcmp
			    (p->cfg->langs[lang], p->last.string))
				break;
		if (lang == p->cfg->langsz) {
			pp = reallocarray
				(p->cfg->langs,
				 p->cfg->langsz + 1,
				 sizeof(char *));
			if (NULL == pp) {
				parse_err(p);
				return 0;
			}
			p->cfg->langs = pp;
			p->cfg->langs[p->cfg->langsz] =
				strdup(p->last.string);
			if (NULL == p->cfg->langs[p->cfg->langsz]) {
				parse_err(p);
				return 0;
			}
			p->cfg->langsz++;
		}
		parse_next(p);
	} else if (TOK_LITERAL != p->lasttype) {
		parse_errx(p, "expected period or quoted string");
		return 0;
	}

	/* Now for the label itself. */

	if (TOK_LITERAL != p->lasttype) {
		parse_errx(p, "expected quoted string");
		return 0;
	}

	/* 
	 * Sanity: don't allow HTML '<' in output because it may not be
	 * safely serialised into the resulting page.
	 * Should escape as \u003c.
	 */

	for (cp = p->last.string; '\0' != *cp; cp++) 
		if ('<' == *cp) {
			parse_errx(p, "illegal html character");
			return 0;
		}

	TAILQ_FOREACH(l, q, entries)
		if (lang == l->lang) {
			parse_warnx(p, "replacing prior label");
			free(l->label);
			l->label = strdup(p->last.string);
			if (NULL == l->label) {
				parse_err(p);
				return 0;
			}
			return 1;
		}

	l = calloc(1, sizeof(struct label));
	if (NULL == l) {
		parse_err(p);
		return 0;
	}
	TAILQ_INSERT_TAIL(q, l, entries);
	parse_point(p, &l->pos);
	l->lang = lang;
	l->label = strdup(p->last.string);
	if (NULL == l->label) {
		parse_err(p);
		return 0;
	}
	return 1;
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
		return 0;
	} else if (NULL != *doc) {
		parse_warnx(p, "replaces prior comment");
		free(*doc);
	}

	if (NULL == (*doc = strdup(p->last.string))) {
		parse_err(p);
		return 0;
	}

	return 1;
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
	} else if (FTYPE_ENUM == fd->type) {
		parse_errx(p, "no validation on enums");
		return;
	}

	if (TOK_IDENT != parse_next(p)) {
		parse_errx(p, "expected constraint type");
		return;
	}

	for (vt = 0; vt < VALIDATE__MAX; vt++)
		if (0 == strcasecmp(p->last.string, vtypes[vt]))
			break;

	if (VALIDATE__MAX == vt) {
		parse_errx(p, "unknown constraint type");
		return;
	}

	if (NULL == (v = calloc(1, sizeof(struct fvalid)))) {
		parse_err(p);
		return;
	}
	v->type = vt;
	TAILQ_INSERT_TAIL(&fd->fvq, v, entries);

	/*
	 * For now, we have only a scalar value.
	 * In the future, this will have some conditionalising.
	 */

	switch (fd->type) {
	case (FTYPE_BIT):
	case (FTYPE_BITFIELD):
	case (FTYPE_DATE):
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
	case (FTYPE_EMAIL):
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
 * Parse the action taken on a foreign key's delete or update.
 * This can be one of none, restrict, nullify, cascade, or default.
 */
static void
parse_action(struct parse *p, enum upact *act)
{

	*act = UPACT_NONE;

	if (TOK_IDENT != parse_next(p))
		parse_errx(p, "expected action");
	else if (0 == strcasecmp(p->last.string, "none"))
		*act = UPACT_NONE;
	else if (0 == strcasecmp(p->last.string, "restrict"))
		*act = UPACT_RESTRICT;
	else if (0 == strcasecmp(p->last.string, "nullify"))
		*act = UPACT_NULLIFY;
	else if (0 == strcasecmp(p->last.string, "cascade"))
		*act = UPACT_CASCADE;
	else if (0 == strcasecmp(p->last.string, "default"))
		*act = UPACT_DEFAULT;
	else
		parse_errx(p, "unknown action");
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
	struct tm	 tm;

	while ( ! PARSE_STOP(p)) {
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
		} else if (0 == strcasecmp(p->last.string, "actup")) {
			if (NULL == fd->ref || FTYPE_STRUCT == fd->type) {
				parse_errx(p, "action on non-reference");
				break;
			}
			parse_action(p, &fd->actup);
		} else if (0 == strcasecmp(p->last.string, "actdel")) {
			if (NULL == fd->ref || FTYPE_STRUCT == fd->type) {
				parse_errx(p, "action on non-reference");
				break;
			}
			parse_action(p, &fd->actdel);
		} else if (0 == strcasecmp(p->last.string, "default")) {
			switch (fd->type) {
			case FTYPE_DATE:
				/* We want a date. */
				memset(&tm, 0, sizeof(struct tm));
				if (TOK_INTEGER != parse_next(p)) {
					parse_errx(p, "expected year (integer)");
					break;
				}
				tm.tm_year = p->last.integer - 1900;
				if (TOK_INTEGER != parse_next(p)) {
					parse_errx(p, "expected month (integer)");
					break;
				} else if (p->last.integer >= 0) {
					parse_errx(p, "invalid month");
					break;
				}
				tm.tm_mon = -p->last.integer - 1;
				if (TOK_INTEGER != parse_next(p)) {
					parse_errx(p, "expected day (integer)");
					break;
				} else if (p->last.integer >= 0) {
					parse_errx(p, "invalid day");
					break;
				}
				tm.tm_mday = -p->last.integer;
				tm.tm_isdst = -1;
				fd->flags |= FIELD_HASDEF;
				fd->def.integer = mktime(&tm); 
				break;
			case FTYPE_BIT:
			case FTYPE_BITFIELD:
			case FTYPE_EPOCH:
			case FTYPE_INT:
				if (TOK_INTEGER != parse_next(p)) {
					parse_errx(p, "expected integer");
					break;
				}
				fd->flags |= FIELD_HASDEF;
				fd->def.integer = p->last.integer;
				break;
			case FTYPE_REAL:
				parse_next(p);
				if (TOK_DECIMAL != p->lasttype &&
				    TOK_INTEGER != p->lasttype) {
					parse_errx(p, "expected "
						"real or integer");
					break;
				}
				fd->flags |= FIELD_HASDEF;
				fd->def.decimal = 
					TOK_DECIMAL == p->lasttype ?
					p->last.decimal : p->last.integer;
				break;
			case FTYPE_EMAIL:
			case FTYPE_TEXT:
				if (TOK_LITERAL != parse_next(p)) {
					parse_errx(p, "expected literal");
					break;
				}
				fd->flags |= FIELD_HASDEF;
				fd->def.string = strdup(p->last.string);
				if (NULL == fd->def.string) {
					parse_err(p);
					return;
				}
				break;
			default:
				parse_errx(p, "defaults not "
					"available for type");
				break;
			}
		} else
			parse_errx(p, "unknown field info token");
	}
}

/*
 * Parse information about bitfield type.
 * This just includes the bitfield name.
 */
static void
parse_field_bitfield(struct parse *p, struct field *fd)
{

	if (TOK_IDENT != parse_next(p)) {
		parse_errx(p, "expected bitfield name");
		return;
	}
	if (NULL == (fd->bref = calloc(1, sizeof(struct bref)))) {
		parse_err(p);
		return;
	} 
	fd->bref->parent = fd;
	if (NULL != (fd->bref->name = strdup(p->last.string))) 
		return;

	parse_err(p);
	free(fd->bref);
	fd->bref = NULL;
}

/*
 * Parse information about an enumeration type.
 * This just includes the enumeration name.
 */
static void
parse_field_enum(struct parse *p, struct field *fd)
{

	if (TOK_IDENT != parse_next(p)) {
		parse_errx(p, "expected enum name");
		return;
	}

	if (NULL == (fd->eref = calloc(1, sizeof(struct eref)))) {
		parse_err(p);
		return;
	}
	fd->eref->parent = fd;
	if (NULL != (fd->eref->ename = strdup(p->last.string)))
		return;

	parse_err(p);
	free(fd->eref);
	fd->eref = NULL;
}

/*
 * Read an individual field declaration with syntax
 *
 *   [:refstruct.reffield] TYPE TYPEINFO
 *
 * By default, fields are integers.  TYPE can be "int", "integer",
 * "text", "txt", etc.
 * A reference clause triggers a foreign key reference.
 * A "struct" type triggers a local key reference.
 * The TYPEINFO depends upon the type and is processed by
 * parse_config_field_info(), which must always be run.
 */
static void
parse_field(struct parse *p, struct field *fd)
{
	struct pos	 pos;
	char		*sn;

	if (TOK_SEMICOLON == parse_next(p))
		return;

	/* Check if this is a reference. */

	if (TOK_COLON == p->lasttype) {
		parse_point(p, &pos);
		if (TOK_IDENT != parse_next(p)) {
			parse_errx(p, "expected target struct");
			return;
		} else if (NULL == (sn = strdup(p->last.string))) {
			parse_err(p);
			return;
		} else if (TOK_PERIOD != parse_next(p)) {
			parse_errx(p, "expected period");
			free(sn);
			return;
		} else if (TOK_IDENT != parse_next(p)) {
			parse_errx(p, "expected target field");
			free(sn);
			return;
		} 

		if ( ! ort_field_set_ref_foreign
		    (p->cfg, &pos, fd, sn, p->last.string)) {
			free(sn);
			return;
		}
		free(sn);
		if (TOK_SEMICOLON == parse_next(p))
			return;
	} 
	
	/* Now we're on to the "type" field. */

	if (TOK_IDENT != p->lasttype) {
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
	/* bit */
	if (0 == strcasecmp(p->last.string, "bit")) {
		fd->type = FTYPE_BIT;
		parse_config_field_info(p, fd);
		return;
	}
	/* date */
	if (0 == strcasecmp(p->last.string, "date")) {
		fd->type = FTYPE_DATE;
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
	/* email */
	if (0 == strcasecmp(p->last.string, "email")) {
		fd->type = FTYPE_EMAIL;
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
	/* bitfield */
	if (0 == strcasecmp(p->last.string, "bits")) {
		fd->type = FTYPE_BITFIELD;
		parse_field_bitfield(p, fd);
		parse_config_field_info(p, fd);
		return;
	}
	/* enum */
	if (0 == strcasecmp(p->last.string, "enum")) {
		fd->type = FTYPE_ENUM;
		parse_field_enum(p, fd);
		parse_config_field_info(p, fd);
		return;
	}
	/* fall-through */
	if (strcasecmp(p->last.string, "struct")) {
		parse_errx(p, "unknown field type");
		return;
	}

	/* 
	 * struct needs special handling because we need to link the
	 * name of the field, which may be "below" us in the parse.
	 * So simply store the name here and we'll resolve it when we
	 * running the link phase.
	 */

	parse_point(p, &pos);
	if (TOK_IDENT != parse_next(p)) {
		parse_errx(p, "expected source field");
		return;
	} 
	if ( ! ort_field_set_ref_struct
	    (p->cfg, &pos, fd, p->last.string))
		return;

	parse_config_field_info(p, fd);
}

/*
 * Like parse_config_search_terms() but for distinction terms.
 * If just a period, the distinction is for the whole result set.
 * Otherwise, it's for a specific field we'll look up later.
 *
 *  "." | field["." field]*
 */
static void
parse_config_distinct_term(struct parse *p, struct search *srch)
{
	struct dref	*df;
	struct dstnct	*d;
	size_t		 sz = 0, nsz;
	void		*pp;

	if (NULL == (d = calloc(1, sizeof(struct dstnct)))) {
		parse_err(p);
		return;
	}

	srch->dst = d;
	d->parent = srch;
	parse_point(p, &d->pos);
	TAILQ_INIT(&d->drefq);

	if (TOK_PERIOD == p->lasttype) {
		parse_next(p);
		return;
	}

	while ( ! PARSE_STOP(p)) {
		if (TOK_IDENT != p->lasttype) {
			parse_errx(p, "expected distinct field");
			return;
		}
		if (NULL == (df = calloc(1, sizeof(struct dref)))) {
			parse_err(p);
			return;
		}
		TAILQ_INSERT_TAIL(&d->drefq, df, entries);
		if (NULL == (df->name = strdup(p->last.string))) {
			parse_err(p);
			return;
		}
		parse_point(p, &df->pos);
		df->parent = d;

		/* 
		 * New size of the canonical name: if we're nested, then
		 * we need the full stop in there as well.
		 */

		nsz = sz + strlen(df->name) + 
			(0 == sz ? 0 : 1) + 1;
		if (NULL == (pp = realloc(d->cname, nsz))) {
			parse_err(p);
			return;
		}
		d->cname = pp;
		if (0 == sz) 
			d->cname[0] = '\0';
		else
			strlcat(d->cname, ".", nsz);
		strlcat(d->cname, df->name, nsz);
		sz = nsz;

		if (TOK_PERIOD != parse_next(p)) 
			break;
		parse_next(p);
	}
}

/*
 * Like parse_config_search_terms() but for aggregate terms.
 *
 *   field[.field]* 
 */
static void
parse_config_aggr_terms(struct parse *p,
	enum aggrtype type, struct search *srch)
{
	struct sref	*af;
	struct aggr	*aggr;

	if (TOK_IDENT != p->lasttype) {
		parse_errx(p, "expected aggregate identifier");
		return;
	} else if (NULL == (aggr = calloc(1, sizeof(struct aggr)))) {
		parse_err(p);
		return;
	}

	aggr->parent = srch;
	aggr->op = type;
	parse_point(p, &aggr->pos);
	TAILQ_INIT(&aggr->arq);
	TAILQ_INSERT_TAIL(&srch->aggrq, aggr, entries);

	if ( ! sref_alloc(p, p->last.string, &aggr->arq))
		return;

	for (;;) {
		if (PARSE_STOP(p))
			return;
		if (TOK_COMMA == parse_next(p) ||
		    TOK_SEMICOLON == p->lasttype ||
		    TOK_IDENT == p->lasttype)
			break;
		if (TOK_PERIOD != p->lasttype)
			parse_errx(p, "expected field separator");
		else if (TOK_IDENT != parse_next(p))
			parse_errx(p, "expected field identifier");
		else if (sref_alloc(p, p->last.string, &aggr->arq))
			continue;
		return;
	}

	/* Canonical names. */

	TAILQ_FOREACH(af, &aggr->arq, entries)
		if ( ! ref_append(&aggr->fname, af->name)) {
			parse_err(p);
			return;
		}
	TAILQ_FOREACH(af, &aggr->arq, entries) {
		if (NULL == TAILQ_NEXT(af, entries))
			break;
		if ( ! ref_append(&aggr->name, af->name)) {
			parse_err(p);
			return;
		}
	}
}

/*
 * Like parse_config_search_terms() but for grouping terms.
 *
 *  field[.field]*
 */
static void
parse_config_group_terms(struct parse *p, struct search *srch)
{
	struct sref	*of;
	struct group	*grp;

	if (TOK_IDENT != p->lasttype) {
		parse_errx(p, "expected group identifier");
		return;
	} else if (NULL == (grp = calloc(1, sizeof(struct group)))) {
		parse_err(p);
		return;
	}

	grp->parent = srch;
	parse_point(p, &grp->pos);
	TAILQ_INIT(&grp->grq);
	TAILQ_INSERT_TAIL(&srch->groupq, grp, entries);

	if ( ! sref_alloc(p, p->last.string, &grp->grq))
		return;

	while ( ! PARSE_STOP(p)) {
		if (TOK_COMMA == parse_next(p) ||
		    TOK_SEMICOLON == p->lasttype ||
		    TOK_IDENT == p->lasttype)
			break;
		if (TOK_PERIOD != p->lasttype)
			parse_errx(p, "expected field separator");
		else if (TOK_IDENT != parse_next(p))
			parse_errx(p, "expected field identifier");
		else if (sref_alloc(p, p->last.string, &grp->grq))
			continue;
		return;
	}

	if (PARSE_STOP(p))
		return;

	TAILQ_FOREACH(of, &grp->grq, entries)
		if ( ! ref_append(&grp->fname, of->name)) {
			parse_err(p);
			return;
		}
	TAILQ_FOREACH(of, &grp->grq, entries) {
		if (NULL == TAILQ_NEXT(of, entries))
			break;
		if ( ! ref_append(&grp->name, of->name)) {
			parse_err(p);
			return;
		}
	}
}

/*
 * Like parse_config_search_terms() but for order terms.
 *
 *  field[.field]* ["asc"|"desc"]?
 */
static void
parse_config_order_terms(struct parse *p, struct search *srch)
{
	struct sref	*of;
	struct ord	*ord;

	if (TOK_IDENT != p->lasttype) {
		parse_errx(p, "expected order identifier");
		return;
	} else if (NULL == (ord = calloc(1, sizeof(struct ord)))) {
		parse_err(p);
		return;
	}

	ord->parent = srch;
	ord->op = ORDTYPE_ASC;
	parse_point(p, &ord->pos);
	TAILQ_INIT(&ord->orq);
	TAILQ_INSERT_TAIL(&srch->ordq, ord, entries);

	if ( ! sref_alloc(p, p->last.string, &ord->orq))
		return;

	while ( ! PARSE_STOP(p)) {
		if (TOK_COMMA == parse_next(p) ||
		    TOK_SEMICOLON == p->lasttype)
			break;

		if (TOK_IDENT == p->lasttype) {
			if (0 == strcasecmp(p->last.string, "asc"))
				ord->op = ORDTYPE_ASC;
			if (0 == strcasecmp(p->last.string, "desc"))
				ord->op = ORDTYPE_DESC;

			if (0 == strcasecmp(p->last.string, "asc") ||
			    0 == strcasecmp(p->last.string, "desc"))
				parse_next(p);
			break;
		}

		if (TOK_PERIOD != p->lasttype)
			parse_errx(p, "expected field separator");
		else if (TOK_IDENT != parse_next(p))
			parse_errx(p, "expected field identifier");
		else if (sref_alloc(p, p->last.string, &ord->orq))
			continue;

		return;
	}

	if (PARSE_STOP(p))
		return;

	TAILQ_FOREACH(of, &ord->orq, entries)
		if ( ! ref_append(&ord->fname, of->name)) {
			parse_err(p);
			return;
		}
	TAILQ_FOREACH(of, &ord->orq, entries) {
		if (NULL == TAILQ_NEXT(of, entries))
			break;
		if ( ! ref_append(&ord->name, of->name)) {
			parse_err(p);
			return;
		}
	}
}

/*
 * Parse the field used in a search.
 * This is the search_terms designation in parse_config_search().
 * This may consist of nested structures, which uses dot-notation to
 * signify the field within a field's reference structure.
 *
 *  field.[field]* [operator]?
 */
static void
parse_config_search_terms(struct parse *p, struct search *srch)
{
	struct sref	*sf;
	struct sent	*sent;

	if (TOK_IDENT != p->lasttype) {
		parse_errx(p, "expected field identifier");
		return;
	}

	if (NULL == (sent = sent_alloc(p, srch)))
		return;
	if ( ! sref_alloc(p, p->last.string, &sent->srq))
		return;

	while ( ! PARSE_STOP(p)) {
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

		if (TOK_PERIOD != p->lasttype)
			parse_errx(p, "expected field separator");
		else if (TOK_IDENT != parse_next(p))
			parse_errx(p, "expected field identifier");
		else if (sref_alloc(p, p->last.string, &sent->srq))
			continue;

		return;
	}

	/*
	 * Now fill in the search field's canonical and partial
	 * structure name.
	 * For example of the latter, if our fields are
	 * "user.company.name", this would be "user.company".
	 * For a singleton field (e.g., "userid"), this is NULL.
	 */

	TAILQ_FOREACH(sf, &sent->srq, entries)
		if ( ! ref_append(&sent->fname, sf->name)) {
			parse_err(p);
			return;
		}
	TAILQ_FOREACH(sf, &sent->srq, entries) {
		if (NULL == TAILQ_NEXT(sf, entries))
			break;
		if ( ! ref_append(&sent->name, sf->name)) {
			parse_err(p);
			return;
		}
	}
}

/*
 * Parse the search parameters following the search fields:
 *
 *   [ "name" name |
 *     "comment" quoted_string |
 *     "distinct" distinct_struct |
 *     "min"|"max" aggr_fields ]* |
 *     "group" group_fields |
 *     "order" order_fields ]* ";"
 */
static void
parse_config_search_params(struct parse *p, struct search *s)
{
	struct search	*ss;

	if (TOK_SEMICOLON == parse_next(p))
		return;

	while ( ! PARSE_STOP(p)) {
		if (TOK_IDENT != p->lasttype) {
			parse_errx(p, "expected query parameter name");
			break;
		}
		if (0 == strcasecmp("name", p->last.string)) {
			if (TOK_IDENT != parse_next(p)) {
				parse_errx(p, "expected query name");
				break;
			}

			/* Disallow duplicate names. */

			TAILQ_FOREACH(ss, &s->parent->sq, entries) {
				if (NULL == ss->name ||
				    strcasecmp(ss->name, p->last.string))
					continue;
				if (s->type != ss->type)
					continue;
				parse_errx(p, "duplicate query name");
				break;
			}

			free(s->name);
			s->name = strdup(p->last.string);
			if (NULL == s->name) {
				parse_err(p);
				return;
			}
			parse_next(p);
		} else if (0 == strcasecmp("comment", p->last.string)) {
			if ( ! parse_comment(p, &s->doc))
				break;
			parse_next(p);
		} else if (0 == strcasecmp("limit", p->last.string)) {
			if (TOK_INTEGER != parse_next(p)) {
				parse_errx(p, "expected limit value");
				break;
			} else if (p->last.integer <= 0) {
				parse_errx(p, "expected limit >0");
				break;
			}
			s->limit = p->last.integer;
			parse_next(p);
			if (TOK_COMMA == p->lasttype) {
				if (TOK_INTEGER != parse_next(p)) {
					parse_errx(p, "expected offset value");
					break;
				} else if (p->last.integer <= 0) {
					parse_errx(p, "expected offset >0");
					break;
				}
				s->offset = p->last.integer;
				parse_next(p);
			}
		} else if (0 == strcasecmp("min", p->last.string)) {
			parse_next(p);
			parse_config_aggr_terms(p, AGGR_MIN, s);
			while (TOK_COMMA == p->lasttype) {
				parse_next(p);
				parse_config_aggr_terms(p, AGGR_MIN, s);
			}
		} else if (0 == strcasecmp("max", p->last.string)) {
			parse_next(p);
			parse_config_aggr_terms(p, AGGR_MAX, s);
			while (TOK_COMMA == p->lasttype) {
				parse_next(p);
				parse_config_aggr_terms(p, AGGR_MAX, s);
			}
		} else if (0 == strcasecmp("order", p->last.string)) {
			parse_next(p);
			parse_config_order_terms(p, s);
			while (TOK_COMMA == p->lasttype) {
				parse_next(p);
				parse_config_order_terms(p, s);
			}
		} else if (0 == strcasecmp("group", p->last.string)) {
			parse_next(p);
			parse_config_group_terms(p, s);
			while (TOK_COMMA == p->lasttype) {
				parse_next(p);
				parse_config_group_terms(p, s);
			}
		} else if (0 == strcasecmp("distinct", p->last.string)) {
			if (NULL != s->dst) {
				parse_errx(p, "distinct already set");
				return;
			}
			parse_next(p);
			parse_config_distinct_term(p, s);
		} else {
			parse_errx(p, "unknown search parameter");
			break;
		}
		if (TOK_SEMICOLON == p->lasttype)
			break;
	}

	assert(TOK_SEMICOLON == p->lasttype || PARSE_STOP(p));
}

/*
 * Parse a unique clause.
 * This has the following syntax:
 *
 *  "unique" field ["," field]+ ";"
 *
 * The fields are within the current structure.
 */
static void
parse_config_unique(struct parse *p, struct strct *s)
{
	struct unique	*up, *upp;
	struct nref	*n;
	size_t		 sz, num = 0;
	void		*pp;

	if (NULL == (up = calloc(1, sizeof(struct unique)))) {
		parse_err(p);
		return;
	}

	up->parent = s;
	parse_point(p, &up->pos);
	TAILQ_INIT(&up->nq);
	TAILQ_INSERT_TAIL(&s->nq, up, entries);

	while ( ! PARSE_STOP(p)) {
		if (TOK_IDENT != parse_next(p)) {
			parse_errx(p, "expected unique field");
			break;
		}
		if (NULL == nref_alloc(p, p->last.string, up))
			return;
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
		if (NULL == up->cname) {
			up->cname = calloc(sz + 1, 1);
			if (NULL == up->cname) {
				parse_err(p);
				return;
			}
		} else {
			pp = realloc(up->cname, sz + 1);
			if (NULL == pp) {
				parse_err(p);
				return;
			}
			up->cname = pp;
		}
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
 *     [ ":" sfield [,sfield]*
 *       [ ":" [ "name" name | "comment" quot | "action" action ]* ]? 
 *     ]? ";"
 *
 * The fields ("ufield" for update field and "sfield" for select field)
 * are within the current structure.
 * The former are only for UPT_MODIFY parses.
 * Note that "sfield" also contains an optional operator, just like in
 * the search parameters.
 */
static void
parse_config_update(struct parse *p, struct strct *s, enum upt type)
{
	struct update	*up;
	struct uref	*ur;

	if (NULL == (up = calloc(1, sizeof(struct update)))) {
		parse_err(p);
		return;
	}
	up->parent = s;
	up->type = type;
	parse_point(p, &up->pos);
	TAILQ_INIT(&up->mrq);
	TAILQ_INIT(&up->crq);

	if (UP_MODIFY == up->type)
		TAILQ_INSERT_TAIL(&s->uq, up, entries);
	else
		TAILQ_INSERT_TAIL(&s->dq, up, entries);

	/* 
	 * For "update" statements, start with the fields that will be
	 * updated (from the self-same structure).
	 * This is followed by a colon (continue) or a semicolon (end).
	 */

	parse_next(p);

	if (UP_MODIFY == up->type) {
		if (TOK_COLON == p->lasttype) {
			parse_next(p);
			goto crq;
		} else if (TOK_SEMICOLON == p->lasttype)
			return;
		if (TOK_IDENT != p->lasttype) {
			parse_errx(p, "expected field to modify");
			return;
		}
		ur = uref_alloc(p, p->last.string, up, &up->mrq);
		if (NULL == ur)
			return;
		while (TOK_COLON != parse_next(p)) {
			if (TOK_SEMICOLON == p->lasttype)
				return;

			/*
			 * See if we're going to accept a modifier,
			 * which defaults to "set".
			 * We only allow non-setters for numeric types,
			 * but we'll check that during linking.
			 */

			if (TOK_IDENT == p->lasttype) {
				for (ur->mod = 0; 
				     MODTYPE__MAX != ur->mod; ur->mod++)
					if (0 == strcasecmp
					    (p->last.string, 
					     modtypes[ur->mod]))
						break;
				if (MODTYPE__MAX == ur->mod) {
					parse_errx(p, "bad modifier");
					return;
				} 
				parse_next(p);
				if (TOK_COLON == p->lasttype)
					break;
				if (TOK_SEMICOLON == p->lasttype)
					return;
			}

			if (TOK_COMMA != p->lasttype) {
				parse_errx(p, "expected separator");
				return;
			} else if (TOK_IDENT != parse_next(p)) {
				parse_errx(p, "expected modify field");
				return;
			}
			ur = uref_alloc(p, p->last.string, up, &up->mrq);
			if (NULL == ur)
				return;
		}
		assert(TOK_COLON == p->lasttype);
		parse_next(p);
	}

	/*
	 * Now the fields that will be used to constrain the update
	 * mechanism.
	 * Usually this will be a rowid.
	 * This is followed by a semicolon or colon.
	 * If it's left empty, we either have a semicolon or colon.
	 */
crq:
	if (TOK_COLON == p->lasttype)
		goto terms;
	else if (TOK_SEMICOLON == p->lasttype)
		return;

	if (TOK_IDENT != p->lasttype) {
		parse_errx(p, "expected constraint field");
		return;
	}

	ur = uref_alloc(p, p->last.string, up, &up->crq);
	if (NULL == ur)
		return;

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
			parse_errx(p, "expected constraint field");
			return;
		}
		ur = uref_alloc(p, p->last.string, up, &up->crq);
		if (NULL == ur)
			return;
	}
	assert(TOK_COLON == p->lasttype);

	/*
	 * Lastly, process update terms.
	 * This now consists of "name" and "comment".
	 */
terms:
	parse_next(p);

	while ( ! PARSE_STOP(p)) {
		if (TOK_SEMICOLON == p->lasttype)
			break;
		if (TOK_IDENT != p->lasttype) {
			parse_errx(p, "expected terms");
			return;
		}

		if (0 == strcasecmp(p->last.string, "name")) {
			if (TOK_IDENT != parse_next(p)) {
				parse_errx(p, "expected term name");
				return;
			}
			free(up->name);
			up->name = strdup(p->last.string);
			if (NULL == up->name) {
				parse_err(p);
				return;
			}
		} else if (0 == strcasecmp(p->last.string, "comment")) {
			if ( ! parse_comment(p, &up->doc))
				return;
		} else
			parse_errx(p, "unknown term: %s", p->last.string);

		parse_next(p);
	}
}

/*
 * Parse a search clause as follows:
 *
 *  "search" [ search_terms ]* [":" search_params ]? ";"
 *
 * The optional terms (searchable field) parts are parsed in
 * parse_config_search_terms().
 * The optional params are in parse_config_search_params().
 */
static void
parse_config_search(struct parse *p, struct strct *s, enum stype stype)
{
	struct search	*srch;

	if (NULL == (srch = calloc(1, sizeof(struct search)))) {
		parse_err(p);
		return;
	}
	srch->parent = s;
	srch->type = stype;
	parse_point(p, &srch->pos);
	TAILQ_INIT(&srch->sntq);
	TAILQ_INIT(&srch->ordq);
	TAILQ_INIT(&srch->aggrq);
	TAILQ_INIT(&srch->groupq);
	TAILQ_INSERT_TAIL(&s->sq, srch, entries);

	if (STYPE_LIST == stype)
		s->flags |= STRCT_HAS_QUEUE;
	else if (STYPE_ITERATE == stype)
		s->flags |= STRCT_HAS_ITERATOR;

	/*
	 * If we have an identifier up next, then consider it the
	 * prelude to a set of search terms.
	 * If we don't, we either have a semicolon (end), a colon (start
	 * of info), or error.
	 */

	if (TOK_IDENT == parse_next(p)) {
		parse_config_search_terms(p, srch);
		/* Now for remaining terms... */
		while (TOK_COMMA == p->lasttype) {
			parse_next(p);
			parse_config_search_terms(p, srch);
		}
	} else {
		if (TOK_SEMICOLON == p->lasttype || PARSE_STOP(p))
			return;
		if (TOK_COLON != p->lasttype) {
			parse_errx(p, "expected field identifier");
			return;
		}
	}

	if (TOK_COLON == p->lasttype)
		parse_config_search_params(p, srch);

	assert(TOK_SEMICOLON == p->lasttype || PARSE_STOP(p));
}

/*
 * Parse an enumeration item whose value may be defined or automatically
 * assigned at link time.  Its syntax is:
 *
 *  "item" ident [value]? ["comment" quoted_string]? ";"
 *
 * The identifier has already been parsed: this starts at the value.
 * Both the identifier and the value (if provided) must be unique within
 * the parent enumeration.
 */
static void
parse_enum_item(struct parse *p, struct eitem *ei)
{
	struct eitem	*eei;
	const char	*next;

	if (TOK_INTEGER == parse_next(p)) {
		ei->value = p->last.integer;
		TAILQ_FOREACH(eei, &ei->parent->eq, entries) {
			if (ei == eei || (EITEM_AUTO & eei->flags) ||
			    ei->value != eei->value) 
				continue;
			parse_errx(p, "duplicate enum item value");
			return;
		}
		parse_next(p);
	} else {
		ei->flags |= EITEM_AUTO;
		ei->parent->flags |= ENM_AUTO;
	}

	while ( ! PARSE_STOP(p) && TOK_IDENT == p->lasttype) {
		next = p->last.string;
		if (0 == strcasecmp(next, "comment")) {
			if ( ! parse_comment(p, &ei->doc))
				return;
			parse_next(p);
		} else if (0 == strcasecmp(next, "jslabel")) {
			parse_label(p, &ei->labels);
			parse_next(p);
		} else
			parse_errx(p, "unknown enum item attribute");
	}

	if ( ! PARSE_STOP(p) && TOK_SEMICOLON != p->lasttype)
		parse_errx(p, "expected semicolon");
}

/*
 * Read an individual enumeration.
 * This opens and closes the enumeration, then reads all of the enum
 * data within.
 * Its syntax is:
 * 
 *  "{" 
 *    ["item" ident ITEM]+ 
 *    ["comment" quoted_string]?
 *  "};"
 */
static void
parse_enum_data(struct parse *p, struct enm *e)
{
	struct eitem	*ei;

	if (TOK_LBRACE != parse_next(p)) {
		parse_errx(p, "expected left brace");
		return;
	}

	while ( ! PARSE_STOP(p)) {
		if (TOK_RBRACE == parse_next(p))
			break;
		if (TOK_IDENT != p->lasttype) {
			parse_errx(p, "expected enum attribute");
			return;
		}

		if (0 == strcasecmp(p->last.string, "comment")) {
			if ( ! parse_comment(p, &e->doc))
				return;
			if (TOK_SEMICOLON != parse_next(p)) {
				parse_errx(p, "expected semicolon");
				return;
			}
			continue;
		} else if (strcasecmp(p->last.string, "item")) {
			parse_errx(p, "unknown enum attribute");
			return;
		}

		/* Now we have a new item: validate and parse it. */

		if (TOK_IDENT != parse_next(p)) {
			parse_errx(p, "expected enum item name");
			return;
		} else if ( ! check_badidents(p, p->last.string))
			return;

		if (0 == strcasecmp(p->last.string, "format")) {
			parse_errx(p, "cannot use reserved name");
			return;
		}

		TAILQ_FOREACH(ei, &e->eq, entries) {
			if (strcasecmp(ei->name, p->last.string))
				continue;
			parse_errx(p, "duplicate enum item name");
			return;
		}

		if (NULL == (ei = calloc(1, sizeof(struct eitem)))) {
			parse_err(p);
			return;
		}
		TAILQ_INIT(&ei->labels);
		TAILQ_INSERT_TAIL(&e->eq, ei, entries);
		parse_point(p, &ei->pos);
		ei->parent = e;
		if (NULL == (ei->name = strdup(p->last.string))) {
			parse_err(p);
			return;
		}
		parse_enum_item(p, ei);
	}

	if (PARSE_STOP(p))
		return;

	if (TOK_SEMICOLON != parse_next(p))
		parse_errx(p, "expected semicolon");
	else if (TAILQ_EMPTY(&e->eq))
		parse_errx(p, "no items in enum");
}

static int
roleset_alloc(struct parse *p, struct rolesetq *rq, 
	const char *name, struct rolemap *parent)
{
	struct roleset	*rs;

	if (NULL == (rs = calloc(1, sizeof(struct roleset)))) {
		parse_err(p);
		return 0;
	} else if (NULL == (rs->name = strdup(name))) {
		parse_err(p);
		free(rs);
		return 0;
	}

	rs->parent = parent;
	TAILQ_INSERT_TAIL(rq, rs, entries);
	return 1;
}

/*
 * Look up a rolemap of the given type with the give name, e.g.,
 * "noexport foo", where "noexport" is the type and "foo" is the name.
 * Each structure has a single rolemap that's distributed for all roles
 * that have access to that role.
 * For example, "noexport foo" might be assigned to roles 1, 2, and 3.
 */
static int
roleset_assign(struct parse *p, struct strct *s, 
	struct rolesetq *rq, enum rolemapt type, const char *name)
{
	struct roleset	*rs, *rrs;
	struct rolemap	*rm;

	TAILQ_FOREACH(rm, &s->rq, entries) {
		if (rm->type != type)
			continue;
		if ((NULL == name && NULL != rm->name) ||
		    (NULL != name && NULL == rm->name))
			continue;
		if (NULL != name && strcasecmp(rm->name, name))
			continue;
		break;
	}

	if (NULL == rm) {
		rm = calloc(1, sizeof(struct rolemap));
		if (NULL == rm) {
			parse_err(p);
			return 0;
		}
		TAILQ_INIT(&rm->setq);
		rm->type = type;
		parse_point(p, &rm->pos);
		TAILQ_INSERT_TAIL(&s->rq, rm, entries);
		if (NULL != name) {
			rm->name = strdup(name);
			if (NULL == rm->name) {
				parse_err(p);
				return 0;
			}
		} 
	}

	/*
	 * Now go through the rolemap's set and append the new
	 * set entries if not already specified.
	 * We deep-copy the roleset.
	 */

	TAILQ_FOREACH(rs, rq, entries) {
		TAILQ_FOREACH(rrs, &rm->setq, entries) {
			if (strcasecmp(rrs->name, rs->name))
				continue;
			parse_warnx(p, "duplicate role "
				"assigned to constraint");
			break;
		}
		if (NULL != rrs)
			continue;
		if ( ! roleset_alloc(p, &rm->setq, rs->name, rm))
			return 0;
	}

	return 1;
}

/*
 * For a given structure "s", allow access to functions (insert, delete,
 * etc.) based on a set of roles.
 * This is invoked within parse_struct_date().
 * Its syntax is as follows:
 *
 *   "roles" name ["," name ]* "{" [ROLE ";"]* "};"
 *
 * The roles ("ROLE") can be as follows, where "NAME" is the name of the
 * item as described in the structure.
 *
 *   "all"
 *   "delete" NAME
 *   "insert"
 *   "iterate" NAME
 *   "list" NAME
 *   "noexport" [NAME]
 *   "search" NAME
 *   "update" NAME
 */
static void
parse_config_roles(struct parse *p, struct strct *s)
{
	struct roleset	*rs;
	struct rolesetq	 rq;
	enum rolemapt	 type;

	TAILQ_INIT(&rq);

	/*
	 * First, gather up all of the roles that we're going to
	 * associate with whatever comes next and put them in "rq".
	 * If anything happens during any of these routines, we enter
	 * the "cleanup" label, which cleans up the "rq" queue.
	 */

	if (TOK_IDENT != parse_next(p)) {
		parse_errx(p, "expected role name");
		return;
	} else if (0 == strcasecmp("none", p->last.string)) {
		parse_errx(p, "cannot assign \"none\" role");
		return;
	} 
	
	if ( ! roleset_alloc(p, &rq, p->last.string, NULL))
		goto cleanup;

	while ( ! PARSE_STOP(p) && TOK_LBRACE != parse_next(p)) {
		if (TOK_COMMA != p->lasttype) {
			parse_errx(p, "expected comma");
			goto cleanup;
		} else if (TOK_IDENT != parse_next(p)) {
			parse_errx(p, "expected role name");
			goto cleanup;
		} else if (0 == strcasecmp(p->last.string, "none")) {
			parse_errx(p, "cannot assign \"none\" role");
			goto cleanup;
		}
		TAILQ_FOREACH(rs, &rq, entries) {
			if (strcasecmp(rs->name, p->last.string))
				continue;
			parse_errx(p, "duplicate role name");
			goto cleanup;
		}
		if ( ! roleset_alloc(p, &rq, p->last.string, NULL))
			goto cleanup;
	}

	/* If something bad has happened, clean up. */

	if (PARSE_STOP(p) || TOK_LBRACE != p->lasttype)
		goto cleanup;

	/* 
	 * Next phase: read through the constraints.
	 * Apply the roles above to each of the constraints, possibly
	 * making them along the way.
	 * We need to deep-copy the constraints instead of copying the
	 * pointer because we might be applying the same roleset to
	 * different constraint types.
	 */

	while ( ! PARSE_STOP(p) && TOK_RBRACE != parse_next(p)) {
		if (TOK_IDENT != p->lasttype) {
			parse_errx(p, "expected role constraint type");
			goto cleanup;
		}
		for (type = 0; type < ROLEMAP__MAX; type++) 
			if (0 == strcasecmp
			    (p->last.string, rolemapts[type]))
				break;
		if (ROLEMAP__MAX == type) {
			parse_errx(p, "unknown role constraint type");
			goto cleanup;
		}

		parse_next(p);

		/* Some constraints are named; some aren't. */

		if (TOK_IDENT == p->lasttype) {
			if (ROLEMAP_INSERT == type ||
			    ROLEMAP_ALL == type) {
				parse_errx(p, "unexpected "
					"role constraint name");
				goto cleanup;
			}
			if ( ! roleset_assign(p, s, 
			    &rq, type, p->last.string))
				goto cleanup;
			parse_next(p);
		} else if (TOK_SEMICOLON == p->lasttype) {
			if (ROLEMAP_INSERT != type &&
			    ROLEMAP_NOEXPORT != type &&
			    ROLEMAP_ALL != type) {
				parse_errx(p, "expected "
					"role constraint name");
				goto cleanup;
			}
			if ( ! roleset_assign(p, s, &rq, type, NULL))
				goto cleanup;
		} else {
			parse_errx(p, "expected role constraint "
				"name or semicolon");
			goto cleanup;
		} 

		if (TOK_SEMICOLON != p->lasttype) {
			parse_errx(p, "expected semicolon");
			goto cleanup;
		}
	}

	if ( ! PARSE_STOP(p) && TOK_RBRACE == p->lasttype)
		if (TOK_SEMICOLON != parse_next(p))
			parse_errx(p, "expected semicolon");

cleanup:
	while (NULL != (rs = TAILQ_FIRST(&rq))) {
		TAILQ_REMOVE(&rq, rs, entries);
		free(rs->name);
		free(rs);
	}
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
 *    ["delete" delete_fields]*
 *    ["insert"]*
 *    ["unique" unique_fields]*
 *    ["comment" quoted_string]?
 *    ["roles" role_fields]*
 *  "};"
 */
static void
parse_struct_data(struct parse *p, struct strct *s)
{
	struct field	*fd;
	struct pos	 pos;

	if (TOK_LBRACE != parse_next(p)) {
		parse_errx(p, "expected left brace");
		return;
	}

	while ( ! PARSE_STOP(p)) {
		if (TOK_RBRACE == parse_next(p))
			break;
		if (TOK_IDENT != p->lasttype) {
			parse_errx(p, "expected struct data type");
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
		} else if (0 == strcasecmp(p->last.string, "insert")) {
			if (NULL != s->ins) {
				parse_errx(p, "insert already defined");
				return;
			}
			s->ins = calloc(1, sizeof(struct insert));
			if (NULL == s->ins) {
				parse_err(p);
				return;
			}
			s->ins->parent = s;
			parse_point(p, &s->ins->pos);
			if (TOK_SEMICOLON != parse_next(p)) {
				parse_errx(p, "expected semicolon");
				return;
			}
			continue;
		} else if (0 == strcasecmp(p->last.string, "unique")) {
			parse_config_unique(p, s);
			continue;
		} else if (0 == strcasecmp(p->last.string, "roles")) {
			parse_config_roles(p, s);
			continue;
		} else if (strcasecmp(p->last.string, "field")) {
			parse_errx(p, "unknown struct data type: %s",
				p->last.string);
			return;
		}

		/* Now we have a new field: validate and parse. */

		if (TOK_IDENT != parse_next(p)) {
			parse_errx(p, "expected field name");
			return;
		} 
		
		parse_point(p, &pos);
		fd = ort_field_alloc(p->cfg, s, &pos, p->last.string);
		if (NULL == fd)
			return;
		parse_field(p, fd);
	}

	if (PARSE_STOP(p))
		return;

	if (TOK_SEMICOLON != parse_next(p)) 
		parse_errx(p, "expected semicolon");
	else if (TAILQ_EMPTY(&s->fq))
		parse_errx(p, "no fields in struct");
}

/*
 * Parse a bitfield item with syntax:
 *
 *  NUMBER ["comment" quoted_string]? ";"
 */
static void
parse_bitidx_item(struct parse *p, struct bitidx *bi)
{
	struct bitidx	*bbi;

	if (TOK_INTEGER != parse_next(p)) {
		parse_errx(p, "expected item value");
		return;
	}

	bi->value = p->last.integer;
	if (bi->value < 0 || bi->value >= 64) {
		parse_errx(p, "bit index out of range");
		return;
	} else if (bi->value >= 32)
		parse_warnx(p, "bit index will not work with "
			"JavaScript applications (32-bit)");

	TAILQ_FOREACH(bbi, &bi->parent->bq, entries) 
		if (bi != bbi && bi->value == bbi->value) {
			parse_errx(p, "duplicate item value");
			return;
		}

	while ( ! PARSE_STOP(p) && TOK_IDENT == parse_next(p))
		if (0 == strcasecmp(p->last.string, "comment"))
			parse_comment(p, &bi->doc);
		else if (0 == strcasecmp(p->last.string, "jslabel"))
			parse_label(p, &bi->labels);
		else
			parse_errx(p, "unknown item data type");

	if ( ! PARSE_STOP(p) && TOK_SEMICOLON != p->lasttype)
		parse_errx(p, "expected semicolon");
}

/*
 * Parse semicolon-terminated labels of special phrase.
 * Return zero on failure, non-zero on success.
 */
static int
parse_bitidx_label(struct parse *p, struct labelq *q)
{

	for (;;) {
		if (TOK_SEMICOLON == parse_next(p))
			return 1;
		if (TOK_IDENT != p->lasttype ||
		    strcasecmp(p->last.string, "jslabel")) {
			parse_errx(p, "expected \"jslabel\"");
			return 0;
		} 
		if ( ! parse_label(p, q))
			return 0;
	}
}

/*
 * Parse a full bitfield index.
 * Its syntax is:
 *
 *  "{" 
 *    ["item" ident ITEM]+ 
 *    ["comment" quoted_string]?
 *  "};"
 *
 *  The "ITEM" clause is handled by parse_bididx_item.
 */
static void
parse_bitidx(struct parse *p, struct bitf *b)
{
	struct bitidx	*bi;

	if (TOK_LBRACE != parse_next(p)) {
		parse_errx(p, "expected left brace");
		return;
	}

	while ( ! PARSE_STOP(p)) {
		if (TOK_RBRACE == parse_next(p))
			break;
		if (TOK_IDENT != p->lasttype) {
			parse_errx(p, "expected bitfield data type");
			return;
		}

		if (0 == strcasecmp(p->last.string, "comment")) {
			if ( ! parse_comment(p, &b->doc))
				return;
			if (TOK_SEMICOLON != parse_next(p)) {
				parse_errx(p, "expected end of comment");
				return;
			}
			continue;
		} else if (0 == strcasecmp(p->last.string, "isunset") ||
		  	   0 == strcasecmp(p->last.string, "unset")) {
			if (0 == strcasecmp(p->last.string, "unset"))
				parse_warnx(p, "\"unset\" is "
					"deprecated: use \"isunset\"");
			if ( ! parse_bitidx_label(p, &b->labels_unset))
				return;
			continue;
		} else if (0 == strcasecmp(p->last.string, "isnull")) {
			if ( ! parse_bitidx_label(p, &b->labels_null))
				return;
			continue;
		} else if (strcasecmp(p->last.string, "item")) {
			parse_errx(p, "unknown bitfield data type");
			return;
		}

		/* Now we have a new item: validate and parse it. */

		if (TOK_IDENT != parse_next(p)) {
			parse_errx(p, "expected item name");
			return;
		} else if ( ! check_badidents(p, p->last.string))
			return;

		TAILQ_FOREACH(bi, &b->bq, entries) {
			if (strcasecmp(bi->name, p->last.string))
				continue;
			parse_errx(p, "duplicate item name");
			return;
		}

		if (NULL == (bi = calloc(1, sizeof(struct bitidx)))) {
			parse_err(p);
			return;
		}
		TAILQ_INIT(&bi->labels);
		parse_point(p, &bi->pos);
		TAILQ_INSERT_TAIL(&b->bq, bi, entries);
		bi->parent = b;
		if (NULL == (bi->name = strdup(p->last.string))) {
			parse_err(p);
			return;
		}
		parse_bitidx_item(p, bi);
	}

	if (PARSE_STOP(p))
		return;

	if (TOK_SEMICOLON != parse_next(p))
		parse_errx(p, "expected semicolon");
	else if (TAILQ_EMPTY(&b->bq))
		parse_errx(p, "no items in bitfield");
}

/*
 * Parse a "bitf", which is a named set of bit indices.
 * Its syntax is:
 *
 *  "bitf" name "{" ... "};"
 */
static void
parse_bitfield(struct parse *p)
{
	struct bitf	*b;
	char		*caps;

	/* 
	 * Disallow duplicate and bad names.
	 * Duplicates are for both structures and enumerations.
	 */

	if ( ! check_dupetoplevel(p, p->last.string) ||
	     ! check_badidents(p, p->last.string))
		return;

	if (NULL == (b = calloc(1, sizeof(struct bitf)))) {
		parse_err(p);
		return;
	}
	TAILQ_INIT(&b->labels_unset);
	TAILQ_INIT(&b->labels_null);
	parse_point(p, &b->pos);
	TAILQ_INSERT_TAIL(&p->cfg->bq, b, entries);
	TAILQ_INIT(&b->bq);
	if (NULL == (b->name = strdup(p->last.string))) {
		parse_err(p);
		return;
	}
	if (NULL == (b->cname = strdup(b->name))) {
		parse_err(p);
		return;
	}
	for (caps = b->cname; '\0' != *caps; caps++)
		*caps = toupper((int)*caps);

	parse_bitidx(p, b);
}

/*
 * Verify and allocate an enum, then start parsing it.
 */
static void
parse_enum(struct parse *p)
{
	struct enm	*e;
	char		*caps;

	/* 
	 * Disallow duplicate and bad names.
	 * Duplicates are for both structures and enumerations.
	 */

	if ( ! check_dupetoplevel(p, p->last.string) ||
	     ! check_badidents(p, p->last.string))
		return;

	if (NULL == (e = calloc(1, sizeof(struct enm)))) {
		parse_err(p);
		return;
	}
	parse_point(p, &e->pos);
	TAILQ_INSERT_TAIL(&p->cfg->eq, e, entries);
	TAILQ_INIT(&e->eq);
	if (NULL == (e->name = strdup(p->last.string))) {
		parse_err(p);
		return;
	}
	if (NULL == (e->cname = strdup(e->name))) {
		parse_err(p);
		return;
	}
	for (caps = e->cname; '\0' != *caps; caps++)
		*caps = toupper((int)*caps);

	parse_enum_data(p, e);
}

/*
 * Return zero if the name already exists, non-zero otherwise.
 * This is a recursive function.
 */
static int
check_rolename(const struct roleq *rq, const char *name) 
{
	const struct role *r;

	TAILQ_FOREACH(r, rq, entries) {
		if (0 == strcmp(r->name, name))
			return(0);
		if ( ! check_rolename(&r->subrq, name))
			return(0);
	}

	return(1);
}

static struct role *
role_alloc(struct parse *p, const char *name, struct role *parent)
{
	struct role	*r;
	char		*cp;

	if (NULL == (r = calloc(1, sizeof(struct role)))) {
		parse_err(p);
		return NULL;
	}
	if (NULL == (r->name = strdup(name))) {
		parse_err(p);
		free(r);
		return NULL;
	}

	/* Add a lowercase version. */

	for (cp = r->name; '\0' != *cp; cp++)
		*cp = tolower((unsigned char)*cp);
	r->parent = parent;
	parse_point(p, &r->pos);
	TAILQ_INIT(&r->subrq);
	if (NULL != parent)
		TAILQ_INSERT_TAIL(&parent->subrq, r, entries);
	return r;
}

/*
 * Parse an individual role, which may be a subset of another role
 * designation, and possibly its documentation.
 * It may not be a reserved role.
 * Its syntax is:
 *
 *  "role" name ["comment" quoted_string]? ["{" [ ROLE ]* "}"]? ";"
 */
static void
parse_role(struct parse *p, struct role *parent)
{
	struct role	*r;

	if (TOK_IDENT != p->lasttype ||
	    strcasecmp(p->last.string, "role")) {
		parse_errx(p, "expected \"role\"");
		return;
	} else if (TOK_IDENT != parse_next(p)) {
		parse_errx(p, "expected role name");
		return;
	}

	if (0 == strcasecmp(p->last.string, "default") ||
	    0 == strcasecmp(p->last.string, "none") ||
	    0 == strcasecmp(p->last.string, "all")) {
		parse_errx(p, "reserved role name");
		return;
	} else if ( ! check_rolename(&p->cfg->rq, p->last.string)) {
		parse_errx(p, "duplicate role name");
		return;
	} else if ( ! check_badidents(p, p->last.string))
		return;

	if (NULL == (r = role_alloc(p, p->last.string, parent)))
		return;

	if (TOK_IDENT == parse_next(p)) {
		if (strcasecmp(p->last.string, "comment")) {
			parse_errx(p, "expected comment");
			return;
		}
		if ( ! parse_comment(p, &r->doc))
			return;
		if (PARSE_STOP(p))
			return;
		parse_next(p);
	}

	if (TOK_LBRACE == p->lasttype) {
		while ( ! PARSE_STOP(p)) {
			if (TOK_RBRACE == parse_next(p))
				break;
			parse_role(p, r);
		}
		parse_next(p);
	}

	if (PARSE_STOP(p))
		return;
	if (TOK_SEMICOLON != p->lasttype)
		parse_errx(p, "expected semicolon");
}

/*
 * This means that we're a role-based system.
 * Parse out our role tree.
 * See parse_role() for the ROLE sequence;
 * Its syntax is:
 *
 *  "roles" "{" [ ROLE ]* "}" ";"
 */
static void
parse_roles(struct parse *p)
{
	struct role	*r;

	if (CFG_HAS_ROLES & p->cfg->flags) {
		parse_errx(p, "roles already specified");
		return;
	} 
	p->cfg->flags |= CFG_HAS_ROLES;

	/*
	 * Start by allocating the reserved roles.
	 * These are "none", "default", and "all".
	 * Make the "all" one the parent of everything.
	 */

	if (NULL == (r = role_alloc(p, "none", NULL)))
		return;
	TAILQ_INSERT_TAIL(&p->cfg->rq, r, entries);

	if (NULL == (r = role_alloc(p, "default", NULL)))
		return;
	TAILQ_INSERT_TAIL(&p->cfg->rq, r, entries);

	if (NULL == (r = role_alloc(p, "all", NULL)))
		return;
	TAILQ_INSERT_TAIL(&p->cfg->rq, r, entries);

	if (TOK_LBRACE != parse_next(p)) {
		parse_errx(p, "expected left brace");
		return;
	}

	while ( ! PARSE_STOP(p)) {
		if (TOK_RBRACE == parse_next(p))
			break;
		/* Pass in "all" role as top-level. */
		parse_role(p, r);
	}

	if (PARSE_STOP(p))
		return;
	if (TOK_SEMICOLON != parse_next(p)) 
		parse_errx(p, "expected semicolon");
}

/*
 * Verify and allocate a struct, then start parsing its fields and
 * ancillary entries.
 */
static void
parse_struct(struct parse *p)
{
	struct strct	*s;
	struct pos	 pos;

	parse_point(p, &pos);
	s = ort_strct_alloc(p->cfg, &pos, p->last.string);
	if (NULL == s)
		return;
	parse_struct_data(p, s);
}

/*
 * Top-level parse.
 * Read until we reach an identifier for a structure.
 * Once we reach one, parse for that structure.
 * Then continue until we've read all structures.
 * Its syntax is:
 *
 *  [ "struct" ident STRUCT | "enum" ident ENUM ]+
 *
 * Where STRUCT is defined in parse_struct_data and ident is a unique,
 * alphanumeric (starting with alpha), non-reserved string.
 *
 * Returns zero on failure, non-zero otherwise.
 */
static int
ort_parse_r(struct parse *p)
{

	for (;;) {
		if (TOK_ERR == parse_next(p))
			return 0;
		else if (TOK_EOF == p->lasttype)
			break;

		/* Our top-level identifier. */

		if (TOK_IDENT != p->lasttype) {
			parse_errx(p, "expected top-level type");
			continue;
		}

		/* Parse whether we're struct, enum, or roles. */

		if (0 == strcasecmp(p->last.string, "roles")) {
			parse_roles(p);
			continue;
		} else if (0 == strcasecmp(p->last.string, "struct")) {
			if (TOK_IDENT == parse_next(p)) {
				parse_struct(p);
				continue;
			}
			parse_errx(p, "expected struct name");
		} else if (0 == strcasecmp(p->last.string, "enum")) {
			if (TOK_IDENT == parse_next(p)) {
				parse_enum(p);
				continue;
			}
			parse_errx(p, "expected enum name");
		} else if (0 == strcasecmp(p->last.string, "bits") ||
		           0 == strcasecmp(p->last.string, "bitfield")) {
			if (TOK_IDENT == parse_next(p)) {
				parse_bitfield(p);
				continue;
			}
			parse_errx(p, "expected bitfield name");
		} else
			parse_errx(p, "unknown top-level type");
	}

	return 1;
}

int
ort_parse_file_r(struct config *cfg, FILE *f, const char *fname)
{
	struct parse	 p;
	int		 rc = 0;
	void		*pp;

	pp = reallocarray(cfg->fnames, 
		cfg->fnamesz + 1, sizeof(char *));
	if (NULL == pp) {
		ort_config_msg(cfg, MSGTYPE_FATAL, 
			channel, errno, NULL, NULL);
		return 0;
	}
	cfg->fnames = pp;
	cfg->fnamesz++;

	cfg->fnames[cfg->fnamesz - 1] = strdup(fname);
	if (NULL == cfg->fnames[cfg->fnamesz - 1]) {
		ort_config_msg(cfg, MSGTYPE_FATAL, 
			channel, errno, NULL, NULL);
		return 0;
	}

	memset(&p, 0, sizeof(struct parse));
	p.column = 0;
	p.line = 1;
	p.fname = cfg->fnames[cfg->fnamesz - 1];
	p.infile.f = f;
	p.type = PARSETYPE_FILE;
	p.cfg = cfg;
	rc = ort_parse_r(&p);
	free(p.buf);
	return rc;
}

struct config *
ort_parse_buf(const char *buf, size_t len)
{
	struct parse	 p;
	int		 rc = 0;
	struct config	*cfg;
	void		*pp;

	if (NULL == (cfg = ort_config_alloc()))
		return NULL;

	pp = reallocarray(cfg->fnames, 
		 cfg->fnamesz + 1, sizeof(char *));

	if (NULL == pp) {
		ort_config_msg(cfg, MSGTYPE_FATAL, 
			channel, errno, NULL, NULL);
		ort_config_free(cfg);
		return NULL;
	}

	cfg->fnames = pp;
	cfg->fnamesz++;

	cfg->fnames[cfg->fnamesz - 1] = strdup("<buffer>");
	if (NULL == cfg->fnames[cfg->fnamesz - 1]) {
		ort_config_msg(cfg, MSGTYPE_FATAL, 
			channel, errno, NULL, NULL);
		ort_config_free(cfg);
		return NULL;
	}

	memset(&p, 0, sizeof(struct parse));
	p.column = 0;
	p.line = 1;
	p.fname = cfg->fnames[cfg->fnamesz - 1];
	p.inbuf.buf = buf;
	p.inbuf.len = len;
	p.type = PARSETYPE_BUF;
	p.cfg = cfg;

	rc = ort_parse_r(&p);
	free(p.buf);

	if (0 == rc || 0 == ort_parse_close(cfg)) {
		ort_config_free(cfg);
		return NULL;
	}
	return cfg;
}

struct config *
ort_parse_file(FILE *f, const char *fname)
{
	struct config	*cfg;

	if (NULL == (cfg = ort_config_alloc()))
		return NULL;

	if (0 == ort_parse_file_r(cfg, f, fname) ||
	    0 == ort_parse_close(cfg)) {
		ort_config_free(cfg);
		return NULL;
	}

	return cfg;
}
