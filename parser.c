/*	$Id$ */
/*
 * Copyright (c) 2017--2020 Kristaps Dzonsons <kristaps@bsd.lv>
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
#include <errno.h>
#include <inttypes.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ort.h"
#include "extern.h"
#include "parser.h"

/*
 * Disallowed field names.
 * The SQL ones are from https://sqlite.org/lang_keywords.html.
 * FIXME: think about this more carefully, as in SQL, there are many
 * things that we can put into string literals.
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

static	const char *const channel = "parser";

/*
 * Copy the current parse point into a saved position buffer.
 */
void
parse_point(const struct parse *p, struct pos *pp)
{

	pp->fname = p->fname;
	pp->line = p->line;
	pp->column = p->column;
}

void
parse_warnx(struct parse *p, const char *fmt, ...)
{
	va_list	 	 ap;
	struct pos	 pos;

	pos.fname = p->fname;
	pos.line = p->line;
	pos.column = p->column;

	if (fmt != NULL) {
		va_start(ap, fmt);
		ort_msgv(p->cfg, MSGTYPE_WARN, 
			channel, 0, &pos, fmt, ap);
		va_end(ap);
	} else
		ort_msg(p->cfg, MSGTYPE_WARN, 
			channel, 0, &pos, NULL);
}

enum tok
parse_err(struct parse *p)
{
	int	 	 er = errno;
	struct pos	 pos;

	pos.fname = p->fname;
	pos.line = p->line;
	pos.column = p->column;

	ort_msg(p->cfg, MSGTYPE_FATAL, 
		channel, er, &pos, NULL);

	p->lasttype = TOK_ERR;
	return p->lasttype;
}

enum tok
parse_errx(struct parse *p, const char *fmt, ...)
{
	va_list	 	 ap;
	struct pos	 pos;

	pos.fname = p->fname;
	pos.line = p->line;
	pos.column = p->column;

	if (fmt != NULL) {
		va_start(ap, fmt);
		ort_msgv(p->cfg, MSGTYPE_ERROR, 
			channel, 0, &pos, fmt, ap);
		va_end(ap);
	} else
		ort_msg(p->cfg, MSGTYPE_ERROR, 
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
		if (pp == NULL) {
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
 * Check to see whether "s" is a reserved identifier or not.
 * Returns zero if this is a reserved identifier, non-zero if it's ok
 */
int
parse_check_badidents(struct parse *p, const char *s)
{
	const char *const *cp;

	for (cp = badidents; *cp != NULL; cp++)
		if (strcasecmp(*cp, s) == 0) {
			parse_errx(p, "reserved identifier");
			return 0;
		}

	return 1;
}

/*
 * For all structs, enumerations, and bitfields, make sure that we don't
 * have any of the same top-level names.
 * That's because the C output for these will be, given "name", "struct
 * name", "enum name", which can't be the same.
 */
int
parse_check_dupetoplevel(struct parse *p, const char *name)
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

static int
parse_check_eof(struct parse *p)
{

	return feof(p->f);
}

static int
parse_check_error(struct parse *p)
{

	return ferror(p->f);
}

static int
parse_check_error_eof(struct parse *p)
{

	return feof(p->f) || ferror(p->f);
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
enum tok
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
			c = parse_nextchar(p);

			/* Stop at eof or unescaped quote.  */

			if (c == EOF || (c == '"' && last != '\\'))
				break;

			/* Strip out CRLF. */

			if (last == '\r' && c == '\n') {
				p->bufsz--;
				last = p->bufsz == 0 ? 
					' ' : p->buf[p->bufsz];
			}

			if (!buf_push(p, c))
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
 * Parse the quoted_string part following "jslabel":
 *
 *   jslabel ["." language] string_literal
 *
 * Attach it to the given queue of labels, possibly clearing out any
 * prior labels of the same language.
 * If this is the case, emit a warning.
 * Return zero on failure, non-zero on success.
 */
int
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

	if (parse_next(p) == TOK_PERIOD) {
		if (parse_next(p) != TOK_IDENT) {
			parse_errx(p, "expected language");
			return 0;
		}
		assert(p->last.string[0] != '\0');
		for ( ; lang < p->cfg->langsz; lang++) 
			if (strcmp
			    (p->cfg->langs[lang], p->last.string) == 0)
				break;
		if (lang == p->cfg->langsz) {
			pp = reallocarray
				(p->cfg->langs,
				 p->cfg->langsz + 1,
				 sizeof(char *));
			if (pp == NULL) {
				parse_err(p);
				return 0;
			}
			p->cfg->langs = pp;
			p->cfg->langs[p->cfg->langsz] =
				strdup(p->last.string);
			if (p->cfg->langs[p->cfg->langsz] == NULL) {
				parse_err(p);
				return 0;
			}
			p->cfg->langsz++;
		}
		parse_next(p);
	} else if (p->lasttype != TOK_LITERAL) {
		parse_errx(p, "expected period or quoted string");
		return 0;
	}

	/* Now for the label itself. */

	if (p->lasttype != TOK_LITERAL) {
		parse_errx(p, "expected quoted string");
		return 0;
	} else if (p->last.string[0] == '\0') {
		parse_errx(p, "label must be non-empty");
		return 0;
	}

	/* 
	 * Don't allow < in the input for now.
	 * This is because we'll put this label in an XLIFF file and
	 * it's easier to disallow it here than to jump through hoops in
	 * translating to and from special characters.
	 */

	for (cp = p->last.string; *cp != '\0'; cp++) 
		if (*cp == '<') {
			parse_errx(p, "invalid character in label");
			return 0;
		}

	/* Disallow duplicates. */

	TAILQ_FOREACH(l, q, entries)
		if (lang == l->lang) {
			parse_errx(p, "duplicate label");
			return 0;
		}

	l = calloc(1, sizeof(struct label));
	if (l == NULL) {
		parse_err(p);
		return 0;
	}
	TAILQ_INSERT_TAIL(q, l, entries);
	parse_point(p, &l->pos);
	l->lang = lang;
	l->label = strdup(p->last.string);
	if (l->label == NULL) {
		parse_err(p);
		return 0;
	}
	return 1;
}

/*
 * Parse the quoted_string part following "comment".
 * Attach it to the given "doc".
 */
int
parse_comment(struct parse *p, char **doc)
{

	if (parse_next(p) != TOK_LITERAL) {
		parse_errx(p, "expected quoted string");
		return 0;
	} else if (*doc != NULL) {
		parse_errx(p, "duplicate comment");
		return 0;
	}

	if ((*doc = strdup(p->last.string)) == NULL) {
		parse_err(p);
		return 0;
	}

	return 1;
}

/*
 * Re-entrant top-level parse with syntax:
 *
 *  [ "struct" ident STRUCT | 
 *    "enum" ident ENUM |
 *    "bitfield" ident BITFIELD |
 *    "roles" ident ROLESET ]*
 *
 * Returns zero on failure, non-zero otherwise.
 * This doesn't guarantee that anything has actually been parsed: this
 * must be done after invoking parse_root() for as many input blocks as
 * possible.
 */
static int
parse_root(struct parse *p)
{

	for (;;) {
		if (parse_next(p) == TOK_ERR)
			return 0;
		else if (p->lasttype == TOK_EOF)
			break;

		/* Our top-level identifier. */

		if (p->lasttype != TOK_IDENT) {
			parse_errx(p, "expected top-level type");
			continue;
		}

		if (strcasecmp(p->last.string, "roles") == 0)
			parse_roles(p);
		else if (strcasecmp(p->last.string, "struct") == 0)
			parse_struct(p);
		else if (strcasecmp(p->last.string, "enum") == 0)
			parse_enum(p);
		else if (strcasecmp(p->last.string, "bits") == 0 ||
		         strcasecmp(p->last.string, "bitfield") == 0)
			parse_bitfield(p);
		else
			parse_errx(p, "unknown top-level type");
	}

	return 1;
}

/*
 * Parse file "f", augmenting the configuration already in "cfg".
 * Returns zero on failure, non-zero on success.
 */
int
ort_parse_file(struct config *cfg, FILE *f, const char *fname)
{
	struct parse	 p;
	int		 rc = 0;
	void		*pp;

	pp = reallocarray(cfg->fnames, 
		cfg->fnamesz + 1, sizeof(char *));
	if (pp == NULL) {
		ort_msg(cfg, MSGTYPE_FATAL, 
			channel, errno, NULL, NULL);
		return 0;
	}

	cfg->fnames = pp;
	cfg->fnamesz++;
	cfg->fnames[cfg->fnamesz - 1] = strdup(fname);
	if (cfg->fnames[cfg->fnamesz - 1] == NULL) {
		ort_msg(cfg, MSGTYPE_FATAL, 
			channel, errno, NULL, NULL);
		return 0;
	}

	memset(&p, 0, sizeof(struct parse));
	p.column = 0;
	p.line = 1;
	p.fname = cfg->fnames[cfg->fnamesz - 1];
	p.f = f;
	p.cfg = cfg;
	rc = parse_root(&p);
	free(p.buf);
	return rc;
}
