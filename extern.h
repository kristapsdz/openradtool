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
	FTYPE_EPOCH, /* epoch (time_t) */
	FTYPE_INT, /* native integer */
	FTYPE_REAL, /* native real-value */
	FTYPE_BLOB, /* native blob */
	FTYPE_TEXT, /* native nil-terminated string */
	FTYPE_PASSWORD, /* hashed password (text) */
	FTYPE_EMAIL, /* email (text) */
	FTYPE_STRUCT, /* only in C API (on reference) */
	FTYPE_ENUM, /* enumeration (integer alias) */
	FTYPE__MAX
};

/*
 * A saved parsing position.
 * (This can be for many reasons.)
 */
struct	pos {
	const char	*fname; /* file-name */
	size_t		 line; /* line number (from 1) */
	size_t		 column; /* column number (from 1) */
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

enum	vtype {
	VALIDATE_GE = 0, /* greater than-eq length or value */
	VALIDATE_LE, /* less than-eq length or value */
	VALIDATE_GT, /* greater than length or value */
	VALIDATE_LT, /* less than length or value */
	VALIDATE_EQ, /* equal length or value */
	VALIDATE__MAX
};

/*
 * A field validation clause.
 * By default, fields are validated only as to their type.
 * This allows for more advanced validation.
 */
struct	fvalid {
	enum vtype	 type; /* type of validation */
	union {
		union {
			int64_t integer;
			double decimal;
			size_t len;
		} value; /* a length/value */
	} d; /* data associated with validation */
	TAILQ_ENTRY(fvalid) entries;
};

TAILQ_HEAD(fvalidq, fvalid);

/*
 * A single item within an enumeration.
 */
struct	eitem {
	char		  *name; /* item name */
	int64_t		   value; /* numeric value */
	char		  *doc; /* documentation */
	struct pos	   pos; /* parse point */
	struct enm	  *parent; /* parent enumeration */
	TAILQ_ENTRY(eitem) entries;
};

TAILQ_HEAD(eitemq, eitem);

/*
 * An enumeration of possible values.
 * These are used as field types.
 */
struct	enm {
	char		*name; /* name of enumeration */
	char		*cname; /* capitalised name */
	char		*doc; /* documentation */
	struct pos	 pos; /* parse point */
	struct eitemq	 eq; /* items in enumeration */
	TAILQ_ENTRY(enm) entries;
};

TAILQ_HEAD(enmq, enm);

/*
 * If a field is an enumeration type, this records the name of the
 * enumeration; then, during linkage, the enumeration itself.
 */
struct	eref {
	char		*ename; /* name of enumeration */
	struct enm	*enm; /* enumeration (after linkage) */
	struct field	*parent; /* up-reference */
};

/*
 * A field defining a database/struct mapping.
 * This can be either reflected in the database, in the C API, or both.
 */
struct	field {
	char		  *name; /* column name */
	struct ref	  *ref; /* "foreign key" ref (or null) */
	struct eref	  *eref;  /* enumeration ref (or null) */
	char		  *doc; /* documentation */
	struct pos	   pos; /* parse point */
	enum ftype	   type; /* type of column */
	struct strct	  *parent; /* parent reference */
	struct fvalidq	   fvq; /* validation */
	unsigned int	   flags; /* flags */
#define	FIELD_ROWID	   0x01 /* this is a rowid field */
#define	FIELD_UNIQUE	   0x02 /* this is a unique field */
#define FIELD_NULL	   0x04 /* can be null */
#define	FIELD_NOEXPORT	   0x08 /* don't export the field (JSON) */
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
	struct pos	  pos; /* parse point */
	struct field	 *field; /* field (after link) */
	struct sent	 *parent; /* up-reference */
	TAILQ_ENTRY(sref) entries;
};

TAILQ_HEAD(srefq, sref);

/*
 * SQL operator to use.
 */
enum	optype {
	OPTYPE_EQUAL = 0, /* equality: x = ? */
	OPTYPE_GE, /* x >= ? */
	OPTYPE_GT, /* x > ? */
	OPTYPE_LE, /* x <= ? */
	OPTYPE_LT, /* x < ? */
	OPTYPE_NEQUAL, /* non-equality: x != ? */
	/* Unary types... */
	OPTYPE_ISNULL, /* nullity: x isnull */
	OPTYPE_NOTNULL, /* non-nullity: x notnull */
	OPTYPE__MAX
};

#define	OPTYPE_ISBINARY(_x) ((_x) < OPTYPE_ISNULL)
#define	OPTYPE_ISUNARY(_x) ((_x) >= OPTYPE_ISNULL)

enum	modtype {
	MODTYPE_SET = 0, /* direct set (default) */
	MODTYPE_INC, /* x = x + ? */
	MODTYPE_DEC, /* x = x - ? */
	MODTYPE__MAX
};

/*
 * A search entity.
 * For example, in a set of search criteria "user.company.name, userid",
 * this would be one of "user.company.name" or "userid", both of which
 * are represented by queues of srefs.
 * One or more "struct sent" consist of a single "struct search".
 *
 */
struct	sent {
	struct srefq	  srq; /* queue of search fields */
	struct pos	  pos; /* parse point */
	struct search	 *parent; /* up-reference */
	enum optype	  op; /* operator */
	char		 *name; /* sub-strutcure dot-form name or NULL */
	char		 *fname; /* canonical dot-form name */
	struct alias	 *alias; /* resolved alias */
	unsigned int	  flags; 
#define	SENT_IS_UNIQUE	  0x01 /* has a rowid/unique in its refs */
	TAILQ_ENTRY(sent) entries;
};

TAILQ_HEAD(sentq, sent);

enum	stype {
	STYPE_SEARCH, /* singular response */
	STYPE_LIST, /* queue of responses */
	STYPE_ITERATE, /* iterator of responses */
};

/*
 * A set of fields to search by and return results.
 * A "search" implies zero or more responses given a query; for example,
 * a unique response to the set of sets "user.company.name, userid",
 * which has two search entities (struct sent) with at least one search
 * reference (sref).
 */
struct	search {
	struct sentq	    sntq; /* nested reference chain */
	struct pos	    pos; /* parse point */
	char		   *name; /* named or NULL */
	char		   *doc; /* documentation */
	struct strct	   *parent; /* up-reference */
	enum stype	    type; /* type of search */
	unsigned int	    flags; 
#define	SEARCH_IS_UNIQUE    0x01 /* has a rowid or unique somewhere */
	TAILQ_ENTRY(search) entries;
};

TAILQ_HEAD(searchq, search);

/*
 * An update reference.
 * This resolves to be a native field in a structure for which update
 * commands will be generated.
 */
struct	uref {
	char		 *name; /* name of field */
	enum optype	  op; /* for constraints, SQL operator */
	enum modtype	  mod; /* for modifiers */
	struct field	 *field; /* resolved field */
	struct pos	  pos; /* position in parse */
	struct update	 *parent; /* up-reference */
	TAILQ_ENTRY(uref) entries;
};

TAILQ_HEAD(urefq, uref);

/*
 * A single field in the local structure that will be part of a unique
 * chain of fields.
 */
struct	nref {
	char		 *name; /* name of field */
	struct field	 *field; /* resolved field */
	struct pos	  pos; /* position in parse */
	struct unique	 *parent; /* up-reference */
	TAILQ_ENTRY(nref) entries;
};

TAILQ_HEAD(nrefq, nref);

/*
 * Define a sequence of fields in the given structure that combine to
 * form a unique clause.
 */
struct	unique {
	struct nrefq	    nq; /* constraint chain */
	struct strct	   *parent; /* up-reference */
	struct pos	    pos; /* position in parse */
	char		   *cname; /* canonical name */
	TAILQ_ENTRY(unique) entries;
};

TAILQ_HEAD(uniqueq, unique);

/*
 * Type of modifier.
 */
enum	upt {
	UP_MODIFY, /* generate an "update" entry */
	UP_DELETE /* generate a "delete" entry */
};

/*
 * A single update clause consisting of multiple fields to be modified
 * depending upon the constraint fields.
 */
struct	update {
	struct urefq	    mrq; /* modified fields or empty for del */
	struct urefq	    crq; /* constraint chain */
	char		   *name; /* named or NULL */
	char		   *doc; /* documentation */
	enum upt	    type; /* type of update */
	struct strct	   *parent; /* up-reference */
	TAILQ_ENTRY(update) entries;
};

TAILQ_HEAD(updateq, update);

/*
 * A database/struct consisting of fields.
 * Structures depend upon other structures (see the FTYPE_REF in the
 * field), which is represented by the "height" value.
 */
struct	strct {
	char		  *name; /* name of structure */
	char		  *cname; /* name of structure (capitals) */
	char		  *doc; /* documentation */
	size_t		   height; /* dependency order */
	struct pos	   pos; /* parse point */
	size_t		   colour; /* used during linkage */
	struct field	  *rowid; /* optional rowid */
	struct fieldq	   fq; /* fields/columns/members */
	struct searchq	   sq; /* search fields */
	struct aliasq	   aq; /* join aliases */
	struct updateq	   uq; /* update conditions */
	struct updateq	   dq; /* delete constraints */
	struct uniqueq	   nq; /* unique constraints */
	unsigned int	   flags;
#define	STRCT_HAS_QUEUE	   0x01 /* needs a queue interface */
#define	STRCT_HAS_ITERATOR 0x02 /* needs iterator interface */
#define	STRCT_HAS_BLOB	   0x04 /* needs resolv.h */
	TAILQ_ENTRY(strct) entries;
};

TAILQ_HEAD(strctq, strct);

/*
 * Hold entire parse sequence results.
 */
struct	config {
	struct strctq	sq; /* all structures */
	struct enmq	eq; /* all enumerations */
};

/*
 * Type of comment.
 */
enum	cmtt {
	COMMENT_C, /* self-contained C comment */
	COMMENT_C_FRAG, /* C w/o open or close */
	COMMENT_C_FRAG_CLOSE, /* C w/o open */
	COMMENT_C_FRAG_OPEN, /* C w/o close */
	COMMENT_JS, /* self-contained jsdoc */
	COMMENT_JS_FRAG, /* jsdoc w/o open or close */
	COMMENT_JS_FRAG_CLOSE, /* jsdoc w/o open */
	COMMENT_JS_FRAG_OPEN, /* jsdoc w/o close */
	COMMENT_SQL /* self-contained SQL comment */
};

__BEGIN_DECLS

int		 parse_link(struct config *);
struct config	*parse_config(FILE *, const char *);
void		 parse_free(struct config *);

void		 gen_c_header(const struct config *, int, int);
void		 gen_c_source(const struct strctq *, 
			int, int, const char *);
void		 gen_sql(const struct strctq *);
int		 gen_diff(const struct config *,
			const struct config *);
void		 gen_javascript(const struct strctq *);

void		 print_commentt(size_t, enum cmtt, const char *);
void		 print_commentv(size_t, enum cmtt, const char *, ...)
			__attribute__((format(printf, 3, 4)));

void		 print_src(size_t, const char *, ...);

void		 print_func_db_close(int);
void		 print_func_db_open(int);
void		 print_func_db_insert(const struct strct *, int);
void		 print_func_db_fill(const struct strct *, int);
void		 print_func_db_free(const struct strct *, int);
void		 print_func_db_freeq(const struct strct *, int);
void		 print_func_db_search(const struct search *, int);
void		 print_func_db_unfill(const struct strct *, int);
void		 print_func_db_update(const struct update *, int);

void		 print_func_json_array(const struct strct *, int);
void		 print_func_json_data(const struct strct *, int);
void		 print_func_json_obj(const struct strct *, int);
void		 print_func_json_iterate(const struct strct *, int);

void		 print_func_valid(const struct field *, int);

__END_DECLS

#endif /* ! EXTERN_H */
