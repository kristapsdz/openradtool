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
#ifndef EXTERN_H
#define EXTERN_H

enum	ftype {
	FTYPE_INT, /* native */
	FTYPE_TEXT, /* native */
	FTYPE_STRUCT /* only in C API (on reference) */
};

/*
 * An object reference into another table.
 * This is gathered during the syntax parse phase, then linked to an
 * actual table afterwards.
 */
struct	ref {
	char		*sfield; /* column with foreign key */
	char		*tstrct; /* target structure */
	char		*tfield; /* target field */
	/* 
	 * The following are "const" references that are only valid
	 * after linkage.
	 */
	struct field 	*target; /* target */
	struct field 	*source; /* source */
	struct field	*parent; /* parent reference */
};

/*
 * A field defining a database/struct mapping.
 * This can be either reflected in the database, in the C API, or both.
 */
struct	field {
	char		  *name; /* column name */
	struct ref	  *ref; /* "foreign key" reference */
	char		  *doc; /* documentation */
	enum ftype	   type; /* type of column */
	struct strct	  *parent; /* parent reference */
	unsigned int	   flags; /* flags */
#define	FIELD_ROWID	   0x01 /* this is a rowid field */
	TAILQ_ENTRY(field) entries;
};

TAILQ_HEAD(fieldq, field);

/*
 * A database/struct consisting of fields.
 * Structures depend upon other structures (see the FTYPE_REF in the
 * field), which is represented by the "height" value.
 */
struct	strct {
	char		  *name; /* name of structure */
	char		  *doc; /* documentation */
	size_t		   height; /* dependency order */
	size_t		   colour; /* used during linkage */
	struct fieldq	   fq; /* fields/columns/members */
	TAILQ_ENTRY(strct) entries;
};

TAILQ_HEAD(strctq, strct);

__BEGIN_DECLS

int		 parse_link(struct strctq *);
struct strctq	*parse_config(FILE *, const char *);
void		 parse_free(struct strctq *);

void		 gen_header(const struct strctq *);
void		 gen_source(const struct strctq *);
void		 gen_sql(const struct strctq *);

void		 gen_comment(const char *, size_t, 
			const char *, const char *,
			const char *);

__END_DECLS

#endif /* ! EXTERN_H */
