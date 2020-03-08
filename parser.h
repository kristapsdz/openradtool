/*	$Id$ */
/*
 * Copyright (c) 2020 Kristaps Dzonsons <kristaps@bsd.lv>
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
#ifndef PARSER_H
#define PARSER_H

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

int		parse_check_badidents(struct parse *, const char *);
int		parse_check_dupetoplevel(struct parse *, const char *);

int		parse_comment(struct parse *, char **);
int		parse_label(struct parse *, struct labelq *);

void		parse_point(const struct parse *, struct pos *);

enum tok	parse_next(struct parse *);

enum tok	parse_err(struct parse *);
enum tok	parse_errx(struct parse *, const char *, ...)
			__attribute__((format(printf, 2, 3)));
void		parse_warnx(struct parse *, const char *, ...)
			__attribute__((format(printf, 2, 3)));

void		parse_bitfield(struct parse *);
void		parse_enum(struct parse *);
void		parse_roles(struct parse *);

#endif /* !PARSER_H */
