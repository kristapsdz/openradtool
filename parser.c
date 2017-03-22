#include "config.h"

#include <sys/queue.h>

#include <ctype.h>
#if HAVE_ERR
# include <err.h>
#endif
#include <inttypes.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "extern.h"

enum	tok {
	TOK_NONE, /* special case: just starting */
	TOK_IDENT, /* alphanumeric (starting with alpha) */
	TOK_INTEGER, /* integer */
	TOK_LBRACE, /* { */
	TOK_RBRACE, /* } */
	TOK_PERIOD, /* } */
	TOK_COLON, /* : */
	TOK_SEMICOLON, /* ; */
	TOK_ERR, /* error! */
	TOK_EOF /* end of file! */
};

struct	parse {
	union {
		char *string;
		int64_t integer;
	} last; /* last parsed if TOK_IDENT or TOK_INTEGER */
	enum tok	 lasttype; /* last parse type */
	char		*buf; /* buffer for storing up reads */
	size_t		 bufsz; /* length of buffer */
	size_t		 bufmax; /* maximum buffer size */
	size_t		 line; /* current line (from 1) */
	size_t		 column; /* current column (from 0) */
	const char	*fname; /* current filename */
};

static void parse_syntax_error(struct parse *, const char *, ...)
	__attribute__((format(printf, 2, 3)));

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
 * Trigger a "hard" error condition: stream error or EOF.
 * This sets the lasttype appropriately.
 */
static void
parse_error(struct parse *p, FILE *f)
{

	if (ferror(f)) {
		warn("%s", p->fname);
		p->lasttype = TOK_ERR;
	} else 
		p->lasttype = TOK_EOF;
}

/*
 * Trigger a "soft" error condition.
 * This sets the lasttype appropriately.
 */
static void
parse_syntax_error(struct parse *p, const char *fmt, ...)
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
}

/*
 * Get the next character and advance us within the file.
 * If we've already reached an error condition, this just keeps us
 * there.
 */
static int
parse_nextchar(struct parse *p, FILE *f)
{
	int	 c;

	c = fgetc(f);

	if (feof(f) || ferror(f))
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
static void
parse_next(struct parse *p, FILE *f)
{
	int		 c;
	const char	*ep = NULL;

	if (TOK_ERR == p->lasttype || 
	    TOK_EOF == p->lasttype) 
		return;

	/* 
	 * Read past any white-space.
	 * If we're at the EOF or error, just return.
	 */

	do {
		c = parse_nextchar(p, f);
	} while (isspace(c));

	if (feof(f) || ferror(f)) {
		parse_error(p, f);
		return;
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
	} else if ('.' == c) {
		p->lasttype = TOK_PERIOD;
	} else if (':' == c) {
		p->lasttype = TOK_COLON;
	} else if (isdigit(c)) {
		p->bufsz = 0;
		buf_push(p, c);
		do {
			c = parse_nextchar(p, f);
			if (isdigit(c))
				buf_push(p, c);
		} while (isdigit(c));

		if (ferror(f)) {
			parse_error(p, f);
			return;
		} else if ( ! feof(f))
			ungetc(c, f);

		buf_push(p, '\0');
		p->last.integer = strtonum
			(p->buf, -INT64_MAX, INT64_MAX, &ep);
		if (NULL != ep) {
			parse_syntax_error(p, "malformed integer");
			return;
		}
		p->lasttype = TOK_INTEGER;
	} else if (isalpha(c)) {
		p->bufsz = 0;
		buf_push(p, c);
		do {
			c = parse_nextchar(p, f);
			if (isalnum(c))
				buf_push(p, c);
		} while (isalnum(c));

		if (ferror(f)) {
			parse_error(p, f);
			return;
		} else if ( ! feof(f))
			ungetc(c, f);

		buf_push(p, '\0');
		p->last.string = p->buf;
		p->lasttype = TOK_IDENT;
	} else
		parse_syntax_error(p, "unknown input token");
}

static void
parse_config_field_struct(struct parse *p, FILE *f, struct ref *r)
{
	
	parse_next(p, f);
	if (TOK_IDENT != p->lasttype) {
		parse_syntax_error(p, "expected struct table");
		return;
	}
	if (NULL == (r->tstrct = strdup(p->last.string)))
		err(EXIT_FAILURE, NULL);

	parse_next(p, f);
	if (TOK_PERIOD != p->lasttype) {
		parse_syntax_error(p, "expected period");
		return;
	}

	parse_next(p, f);
	if (TOK_IDENT != p->lasttype) {
		parse_syntax_error(p, "expected struct field");
		return;
	}
	if (NULL == (r->tfield = strdup(p->last.string)))
		err(EXIT_FAILURE, NULL);

	parse_next(p, f);
	if (TOK_COLON != p->lasttype) {
		parse_syntax_error(p, "expected colon");
		return;
	}

	parse_next(p, f);
	if (TOK_IDENT != p->lasttype) {
		parse_syntax_error(p, "expected source field");
		return;
	}
	if (NULL == (r->sfield = strdup(p->last.string)))
		err(EXIT_FAILURE, NULL);
}

/*
 * Read an individual field declaration.
 * Its syntax is:
 *
 *   TYPE | ";"
 *
 * By default, fields are integers.  TYPE can be "int", "integer",
 * "text", or "txt".
 */
static void
parse_config_field(struct parse *p, FILE *f, struct field *fd)
{

	for (;;) {
		parse_next(p, f);
		if (TOK_SEMICOLON == p->lasttype)
			break;
		if (TOK_IDENT == p->lasttype) {
			/* int64_t */
			if (0 == strcasecmp(p->last.string, "int") ||
			    0 == strcasecmp(p->last.string, "integer")) {
				fd->type = FTYPE_INT;
				continue;
			}
			/* char-array */
			if (0 == strcasecmp(p->last.string, "text") ||
			    0 == strcasecmp(p->last.string, "txt")) {
				fd->type = FTYPE_TEXT;
				continue;
			}
			/* fall-through */
			if (strcasecmp(p->last.string, "struct")) {
				parse_syntax_error(p, "unknown field type");
				break;
			}

			fd->type = FTYPE_REF;
			fd->ref = calloc(1, sizeof(struct ref));
			if (NULL == fd->ref)
				err(EXIT_FAILURE, NULL);

			fd->ref->parent = fd;
			parse_config_field_struct(p, f, fd->ref);
			continue;
		}
		if (TOK_SEMICOLON != p->lasttype) {
			parse_syntax_error(p, "unknown field token");
			break;
		}
		break;
	}
}

/*
 * Read an individual structure.
 * This opens and closes the structure, then reads all of the field
 * elements within.
 * Its syntax is:
 * 
 *  "{" [ident FIELD]* "}"
 *
 * Where FIELD is defined in parse_config_field and ident is an
 * alphanumeric (starting with alpha) string.
 */
static void
parse_config_struct(struct parse *p, FILE *f, struct strct *s)
{
	struct field	*fd;

	parse_next(p, f);
	if (TOK_LBRACE != p->lasttype) {
		parse_syntax_error(p, "expected left brace");
		return;
	}

	for (;;) {
		parse_next(p, f);
		if (TOK_RBRACE == p->lasttype)
			break;
		if (TOK_IDENT != p->lasttype) {
			parse_syntax_error(p, "expected "
				"structure or right brace");
			return;
		}
		if (NULL == (fd = calloc(1, sizeof(struct field))))
			err(EXIT_FAILURE, NULL);
		if (NULL == (fd->name = strdup(p->last.string)))
			err(EXIT_FAILURE, NULL);

		fd->type = FTYPE_INT;
		fd->parent = s;
		TAILQ_INSERT_TAIL(&s->fq, fd, entries);
		parse_config_field(p, f, fd);
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
 *  [ ident STRUCT ]*
 *
 * Where STRUCT is defined in parse_config_struct and ident is an
 * alphanumeric (starting with alpha) string.
 */
struct strctq *
parse_config(FILE *f, const char *fname)
{
	struct strctq	*q;
	struct strct	*s;
	struct parse	 p;

	if (NULL == (q = malloc(sizeof(struct strctq))))
		err(EXIT_FAILURE, NULL);

	TAILQ_INIT(q);

	memset(&p, 0, sizeof(struct parse));
	p.column = 0;
	p.line = 1;
	p.fname = fname;

	for (;;) {
		parse_next(&p, f);
		if (TOK_ERR == p.lasttype)
			goto error;
		else if (TOK_EOF == p.lasttype)
			break;

		if (TOK_IDENT != p.lasttype) {
			parse_syntax_error(&p, NULL);
			goto error;
		}

		if (NULL == (s = calloc(1, sizeof(struct strct))))
			err(EXIT_FAILURE, NULL);
		if (NULL == (s->name = strdup(p.last.string)))
			err(EXIT_FAILURE, NULL);

		TAILQ_INSERT_TAIL(q, s, entries);
		TAILQ_INIT(&s->fq);
		parse_config_struct(&p, f, s);
	}

	if (TAILQ_EMPTY(q)) {
		warnx("%s: no structures", fname);
		goto error;
	}

	return(q);
error:
	parse_free(q);
	return(NULL);
}

void
parse_free(struct strctq *q)
{
	struct strct	*p;
	struct field	*f;

	if (NULL == q)
		return;

	while (NULL != (p = TAILQ_FIRST(q))) {
		TAILQ_REMOVE(q, p, entries);
		while (NULL != (f = TAILQ_FIRST(&p->fq))) {
			TAILQ_REMOVE(&p->fq, f, entries);
			free(f->name);
			free(f);
		}
		free(p->name);
		free(p);
	}
	free(p);
}
