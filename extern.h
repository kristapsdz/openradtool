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
	FTYPE_STRUCT, /* only in C API (on reference) */
	FTYPE__MAX
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
 * An alias gives a unique name to each *possible* search entity.
 * For any structure, this will consist of all possible paths into
 * substructures.
 * This is used later to easily link a search entity (for example,
 * "user.company.name") into an alias (e.g., "_b") that's used in the AS
 * clause when joining.
 * These are unique within a given structure root.
 */
struct 	alias {
	char		  *name; /* canonical dot-separated name */
	char		  *alias; /* unique alias */
	TAILQ_ENTRY(alias) entries;
};

TAILQ_HEAD(aliasq, alias);

/*
 * A single field reference within a chain.
 * For example, in a chain of "user.company.name", which presumes
 * structures "user" and "company", then a "name" in the latter, the
 * fields would be "user", "company", an "name".
 * These compose search entities, "struct sent".
 */
struct	sref {
	char		 *name; /* field name */
	struct field	 *field; /* field (after link) */
	struct sent	 *parent; /* up-reference */
	TAILQ_ENTRY(sref) entries;
};

TAILQ_HEAD(srefq, sref);

/*
 * A search entity.
 * For example, in a set of search criteria "user.company.name, userid",
 * this would be one of "user.company.name" or "userid", both of which
 * are represented by queues of srefs.
 * One or more "struct sent" consist of a single "struct search".
 *
 */
struct	sent {
	struct srefq	  srq;
	struct search	 *parent; /* up-reference */
	char		 *name; /* canonical dot-form name or NULL */
	struct alias	 *alias; /* resolved alias */
	TAILQ_ENTRY(sent) entries;
};

TAILQ_HEAD(sentq, sent);

/*
 * A set of fields to search by.
 * A "search" implies a unique response given a query, for example,
 * the set of sets "user.company.name, userid", which has two search
 * entities (struct sent) with at least one search reference (sref).
 */
struct	search {
	struct sentq	    sntq; /* nested reference chain */
	char		   *name; /* named or NULL */
	char		   *doc; /* documentation */
	struct strct	   *parent; /* up-reference */
	TAILQ_ENTRY(search) entries;
};

TAILQ_HEAD(searchq, search);

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
	struct field	  *rowid; /* optional rowid */
	struct fieldq	   fq; /* fields/columns/members */
	struct searchq	   sq; /* search fields */
	struct aliasq	   aq; /* join aliases */
	TAILQ_ENTRY(strct) entries;
};

TAILQ_HEAD(strctq, strct);

__BEGIN_DECLS

int		 parse_link(struct strctq *);
struct strctq	*parse_config(FILE *, const char *);
void		 parse_free(struct strctq *);

void		 gen_header(const struct strctq *);
void		 gen_source(const struct strctq *, const char *);
void		 gen_sql(const struct strctq *);
int		 gen_diff(const struct strctq *,
			const struct strctq *);

void		 print_comment(const char *, size_t, 
			const char *, const char *,
			const char *);
void		 print_func_search(const struct search *, int);

__END_DECLS

#endif /* ! EXTERN_H */
