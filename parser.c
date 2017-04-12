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
	TOK_COMMA, /* ; */
	TOK_LITERAL, /* "text" */
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

static void parse_err(struct parse *, const char *, ...)
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
 * Trigger a "hard" error condition: stream error or EOF.
 * This sets the lasttype appropriately.
 */
static void
parse_error(struct parse *p)
{

	if (ferror(p->f)) {
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
parse_err(struct parse *p, const char *fmt, ...)
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
	int		 c, last;
	const char	*ep = NULL;

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

	if (feof(p->f) || ferror(p->f)) {
		parse_error(p);
		return(p->lasttype);
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

		if (ferror(p->f)) {
			parse_error(p);
			return(p->lasttype);
		} 

		buf_push(p, '\0');
		p->last.string = p->buf;
		p->lasttype = TOK_LITERAL;
	} else if (isdigit(c)) {
		p->bufsz = 0;
		buf_push(p, c);
		do {
			c = parse_nextchar(p);
			if (isdigit(c))
				buf_push(p, c);
		} while (isdigit(c));

		if (ferror(p->f)) {
			parse_error(p);
			return(p->lasttype);
		} else if ( ! feof(p->f))
			parse_ungetc(p, c);

		buf_push(p, '\0');
		p->last.integer = strtonum
			(p->buf, -INT64_MAX, INT64_MAX, &ep);
		if (NULL != ep) {
			parse_err(p, "malformed integer");
			return(p->lasttype);
		}
		p->lasttype = TOK_INTEGER;
	} else if (isalpha(c)) {
		p->bufsz = 0;
		buf_push(p, c);
		do {
			c = parse_nextchar(p);
			if (isalnum(c))
				buf_push(p, c);
		} while (isalnum(c));

		if (ferror(p->f)) {
			parse_error(p);
			return(p->lasttype);
		} else if ( ! feof(p->f))
			parse_ungetc(p, c);

		buf_push(p, '\0');
		p->last.string = p->buf;
		p->lasttype = TOK_IDENT;
	} else
		parse_err(p, "unknown input token");

	return(p->lasttype);
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
		parse_err(p, "expected source field");
		return;
	} else if (NULL == (r->sfield = strdup(p->last.string)))
		err(EXIT_FAILURE, NULL);
	
	if (TOK_COLON != parse_next(p)) {
		parse_err(p, "expected colon");
		return;
	}

	if (TOK_IDENT != parse_next(p)) {
		parse_err(p, "expected struct table");
		return;
	} else if (NULL == (r->tstrct = strdup(p->last.string)))
		err(EXIT_FAILURE, NULL);

	if (TOK_PERIOD != parse_next(p)) {
		parse_err(p, "expected period");
		return;
	}

	if (TOK_IDENT != parse_next(p)) {
		parse_err(p, "expected struct field");
		return;
	} else if (NULL == (r->tfield = strdup(p->last.string)))
		err(EXIT_FAILURE, NULL);
}

/*
 * Read auxiliary information for a field.
 * Its syntax is:
 *
 *   ["rowid" | "unique" | "comment" string_literal]* ";"
 *
 * This will continue processing until the semicolon is reached.
 */
static void
parse_config_field_info(struct parse *p, struct field *fd)
{

	for (;;) {
		if (TOK_SEMICOLON == parse_next(p))
			break;

		if (TOK_IDENT != p->lasttype) {
			parse_err(p, "unknown field info token");
			break;
		}

		if (0 == strcasecmp(p->last.string, "rowid")) {
			if (FTYPE_INT != fd->type) {
				parse_err(p, "rowid for non-int type");
				break;
			} else if (NULL != fd->ref) {
				parse_err(p, "rowid on reference");
				break;
			}
			fd->flags |= FIELD_ROWID;
			if (NULL != fd->parent->rowid) {
				parse_err(p, "already has rowid");
				break;
			}
			fd->parent->rowid = fd;
			continue;
		} else if (0 == strcasecmp(p->last.string, "unique")) {
			if (NULL != fd->ref) {
				parse_err(p, "unique on reference");
				break;
			}
			fd->flags |= FIELD_UNIQUE;
			continue;
		} else if (0 == strcasecmp(p->last.string, "comment")) {
			if (TOK_LITERAL != parse_next(p)) {
				parse_err(p, "expected comment string");
				break;
			}
			free(fd->doc);
			fd->doc = strdup(p->last.string);
			if (NULL == fd->doc)
				err(EXIT_FAILURE, NULL);
			continue;
		}
		parse_err(p, "unknown field info token");
		break;
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
			parse_err(p, "expected target field");
			return;
		}
		fd->ref->tstrct = strdup(p->last.string);
		if (NULL == fd->ref->tstrct)
			err(EXIT_FAILURE, NULL);

		if (TOK_PERIOD != parse_next(p)) {
			parse_err(p, "expected period");
			return;
		}

		if (TOK_IDENT != parse_next(p)) {
			parse_err(p, "expected field type");
			return;
		}
		fd->ref->tfield = strdup(p->last.string);
		if (NULL == fd->ref->tfield)
			err(EXIT_FAILURE, NULL);

		if (TOK_IDENT != parse_next(p)) {
			parse_err(p, "expected field type");
			return;
		}
	} else if (TOK_IDENT != p->lasttype) {
		parse_err(p, "expected field type");
		return;
	}

	/* int64_t */
	if (0 == strcasecmp(p->last.string, "int") ||
	    0 == strcasecmp(p->last.string, "integer")) {
		fd->type = FTYPE_INT;
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
	/* fall-through */
	if (strcasecmp(p->last.string, "struct")) {
		parse_err(p, "unknown field type");
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
		parse_err(p, "expected field identifier");
		return;
	}
	sref_alloc(p, p->last.string, sent);

	for (;;) {
		if (TOK_COMMA == parse_next(p) ||
		    TOK_SEMICOLON == p->lasttype ||
		    TOK_COLON == p->lasttype)
			break;

		if (TOK_PERIOD != p->lasttype) {
			parse_err(p, "expected field separator");
			return;
		} else if (TOK_IDENT != parse_next(p)) {
			parse_err(p, "expected field identifier");
			return;
		}
		sref_alloc(p, p->last.string, sent);
	}

	/*
	 * Now fill in the search field's canonical structure name.
	 * For example, if our fields are "user.company.name", this
	 * would be "user.company".
	 * For a singleton field (e.g., "userid"), this is NULL.
	 */

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

	for (;;) {
		if (TOK_IDENT != p->lasttype) {
			parse_err(p, "expected search specifier");
			break;
		}
		if (0 == strcasecmp("name", p->last.string)) {
			if (TOK_IDENT != parse_next(p)) {
				parse_err(p, "expected search name");
				break;
			}

			/* Disallow duplicate names. */

			TAILQ_FOREACH(ss, &s->parent->sq, entries) {
				if (NULL == ss->name ||
				    strcasecmp(ss->name, p->last.string))
					continue;
				parse_err(p, "duplicate search name");
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
			if (TOK_LITERAL != parse_next(p)) {
				parse_err(p, "expected comment");
				break;
			} 
			/* XXX: warn of prior */
			free(s->doc);
			s->doc = strdup(p->last.string);
			if (NULL == s->doc)
				err(EXIT_FAILURE, NULL);
			if (TOK_SEMICOLON == parse_next(p))
				break;
		} else {
			parse_err(p, "unknown search parameter");
			break;
		}
	}
}

static void
parse_config_update(struct parse *p, struct strct *s)
{

	for (;;) {
		if (TOK_COLON == parse_next(p))
			break;
		if (TOK_IDENT != p->lasttype) {
			parse_err(p, "expected fields to modify");
			return;
		}
	}
}

/*
 * Parse a search clause.
 * This has the following syntax:
 *
 *  "search" FIELD [, FIELD]* [":" search_terms ]? ";"
 *
 * The FIELD parts are parsed in parse_config_search_params().
 * Returns zero on failure, non-zero on success.
 * FIXME: have return value be void and use lasttype.
 */
static int
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

	/* Per-field. */

	sent = sent_alloc(p, srch);
	parse_config_search_terms(p, sent);

	if (TOK_COLON == p->lasttype) {
		parse_config_search_params(p, srch);
		return(1);
	} else if (TOK_SEMICOLON == p->lasttype)
		return(1);

	if (TOK_COMMA != p->lasttype)
		return(0);

	for (;;) {
		sent = sent_alloc(p, srch);
		parse_config_search_terms(p, sent);
		if (TOK_SEMICOLON == p->lasttype)
			return(1);
		else if (TOK_COLON == p->lasttype)
			break;
		else if (TOK_COMMA != p->lasttype)
			return(0);
	}

	parse_config_search_params(p, srch);
	return(1);
}

/*
 * Read an individual structure.
 * This opens and closes the structure, then reads all of the field
 * elements within.
 * Its syntax is:
 * 
 *  "{" ["field" ident FIELD]+ "}"
 *
 * Where FIELD is defined in parse_config_field and ident is an
 * alphanumeric (starting with alpha) string.
 */
static void
parse_config_struct(struct parse *p, struct strct *s)
{
	struct field	  *fd;
	const char *const *cp;

	if (TOK_LBRACE != parse_next(p)) {
		parse_err(p, "expected left brace");
		return;
	}

	for (;;) {
		if (TOK_RBRACE == parse_next(p))
			break;

		if (TOK_IDENT != p->lasttype) {
			parse_err(p, "expected field entry type");
			return;
		}

		if (0 == strcasecmp(p->last.string, "comment")) {
			/*
			 * A comment.
			 * Attach it to the given structure, possibly
			 * clearing out any prior comments.
			 * XXX: augment comments?
			 */
			if (TOK_LITERAL != parse_next(p)) {
				parse_err(p, "expected comment string");
				return;
			}
			free(s->doc);
			s->doc = strdup(p->last.string);
			if (NULL == s->doc)
				err(EXIT_FAILURE, NULL);
			if (TOK_SEMICOLON != parse_next(p)) {
				parse_err(p, "expected end of comment");
				return;
			}
			continue;
		} else if (0 == strcasecmp(p->last.string, "search")) {
			if ( ! parse_config_search(p, s, STYPE_SEARCH)) 
				return;
			continue;
		} else if (0 == strcasecmp(p->last.string, "list")) {
			if ( ! parse_config_search(p, s, STYPE_LIST)) 
				return;
			continue;
		} else if (0 == strcasecmp(p->last.string, "iterate")) {
			if ( ! parse_config_search(p, s, STYPE_ITERATE)) 
				return;
			continue;
		}
		 
		
		if (strcasecmp(p->last.string, "field")) {
			parse_err(p, "expected field entry type");
			return;
		}

		/* Now we have a new field. */

		if (TOK_IDENT != parse_next(p)) {
			parse_err(p, "expected field name");
			return;
		}

		/* Disallow duplicate names. */

		TAILQ_FOREACH(fd, &s->fq, entries) {
			if (strcasecmp(fd->name, p->last.string))
				continue;
			parse_err(p, "duplicate name");
			return;
		}

		/* Disallow bad names. */

		for (cp = badidents; NULL != *cp; cp++)
			if (0 == strcasecmp(*cp, p->last.string)) {
				parse_err(p, "illegal identifier");
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
		TAILQ_INSERT_TAIL(&s->fq, fd, entries);
		parse_config_field(p, fd);
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
			parse_err(&p, "expected struct");
			goto error;
		}

		if (TOK_IDENT != parse_next(&p)) {
			parse_err(&p, NULL);
			goto error;
		}

		/* Disallow duplicate names. */

		TAILQ_FOREACH(s, q, entries) {
			if (strcasecmp(s->name, p.last.string))
				continue;
			parse_err(&p, "duplicate name");
			goto error;
		}

		/* Disallow bad names. */

		for (cp = badidents; NULL != *cp; cp++)
			if (0 == strcasecmp(*cp, p.last.string)) {
				parse_err(&p, "illegal identifier");
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

		TAILQ_INSERT_TAIL(q, s, entries);
		TAILQ_INIT(&s->fq);
		TAILQ_INIT(&s->sq);
		TAILQ_INIT(&s->aq);
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

	if (NULL == p)
		return;
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
		free(sent->name);
		free(sent);
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
		free(p->doc);
		free(p->name);
		free(p);
	}
	free(q);
}
