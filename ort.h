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
#ifndef ORT_H
#define ORT_H

TAILQ_HEAD(aliasq, alias);
TAILQ_HEAD(bitfq, bitf);
TAILQ_HEAD(bitidxq, bitidx);
TAILQ_HEAD(diffq, diff);
TAILQ_HEAD(eitemq, eitem);
TAILQ_HEAD(enmq, enm);
TAILQ_HEAD(fieldq, field);
TAILQ_HEAD(fvalidq, fvalid);
TAILQ_HEAD(labelq, label);
TAILQ_HEAD(nrefq, nref);
TAILQ_HEAD(ordq, ord);
TAILQ_HEAD(rolemapq, rolemap);
TAILQ_HEAD(roleq, role);
TAILQ_HEAD(rrefq, rref);
TAILQ_HEAD(searchq, search);
TAILQ_HEAD(sentq, sent);
TAILQ_HEAD(strctq, strct);
TAILQ_HEAD(uniqueq, unique);
TAILQ_HEAD(updateq, update);
TAILQ_HEAD(urefq, uref);

enum	ftype {
	FTYPE_BIT, /* bit (index) */
	FTYPE_DATE, /* date (epoch, date is only for valid) (time_t) */
	FTYPE_EPOCH, /* epoch (time_t) */
	FTYPE_INT, /* native integer */
	FTYPE_REAL, /* native real-value */
	FTYPE_BLOB, /* native blob */
	FTYPE_TEXT, /* native nil-terminated string */
	FTYPE_PASSWORD, /* hashed password (text) */
	FTYPE_EMAIL, /* email (text) */
	FTYPE_STRUCT, /* only in C API (on reference) */
	FTYPE_ENUM, /* enumeration (integer alias) */
	FTYPE_BITFIELD, /* bitfield (integer alias) */
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
 */
struct	ref {
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

struct	label {
	char		  *label;
	size_t		   lang;
	struct pos	   pos;
	TAILQ_ENTRY(label) entries;
};

struct	eitem {
	char		  *name;
	int64_t		   value;
	char		  *doc;
	struct labelq	   labels;
	struct pos	   pos;
	struct enm	  *parent;
	unsigned int	   flags;
#define	EITEM_AUTO	   0x01
	TAILQ_ENTRY(eitem) entries;
};

struct	enm {
	char			*name;
	char			*doc;
	struct pos		 pos;
	struct labelq		 labels_null;
	struct eitemq		 eq;
	TAILQ_ENTRY(enm)	 entries;
};

struct	bitidx {
	char		   *name;
	char		   *doc;
	struct labelq	    labels;
	int64_t		    value;
	struct bitf	   *parent;
	struct pos	    pos;
	TAILQ_ENTRY(bitidx) entries;
};

struct	bitf {
	char		 *name;
	char		 *doc;
	struct labelq	  labels_unset;
	struct labelq	  labels_null;
	struct pos	  pos;
	struct bitidxq	  bq;
	TAILQ_ENTRY(bitf) entries;
};

enum	upact {
	UPACT_NONE = 0,
	UPACT_RESTRICT,
	UPACT_NULLIFY,
	UPACT_CASCADE,
	UPACT_DEFAULT,
	UPACT__MAX
};

struct	field {
	char		  *name;
	struct ref	  *ref;
	struct enm	  *enm;
	struct bitf	  *bitf;
	char		  *doc;
	struct pos	   pos;
	union {
		int64_t integer;
		double decimal;
		char *string;
		struct eitem *eitem;
	} def;
	enum ftype	   type;
	enum upact	   actdel;
	struct rolemap	  *rolemap;
	enum upact	   actup;
	struct strct	  *parent;
	struct fvalidq	   fvq;
	unsigned int	   flags;
#define	FIELD_ROWID	   0x01
#define	FIELD_UNIQUE	   0x02
#define FIELD_NULL	   0x04
#define	FIELD_NOEXPORT	   0x08
#define FIELD_HASDEF	   0x10
	TAILQ_ENTRY(field) entries;
};

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
	OPTYPE_LIKE, /* like */
	OPTYPE_AND, /* logical (bitwise) and */
	OPTYPE_OR, /* logical (bitwise) or */
	OPTYPE_STREQ, /* string equality */
	OPTYPE_STRNEQ, /* string non-equality */
	/* Unary types... */
	OPTYPE_ISNULL, /* nullity: x isnull */
	OPTYPE_NOTNULL, /* non-nullity: x notnull */
	OPTYPE__MAX
};

#define	OPTYPE_ISBINARY(_x) ((_x) < OPTYPE_ISNULL)
#define	OPTYPE_ISUNARY(_x) ((_x) >= OPTYPE_ISNULL)

/*
 * Modification types for updating.
 * This defines "how" we update a column.
 */
enum	modtype {
	MODTYPE_CONCAT = 0, /* x = x || ? */
	MODTYPE_DEC, /* x = x - ? */
	MODTYPE_INC, /* x = x + ? */
	MODTYPE_SET, /* direct set (default) */
	MODTYPE_STRSET, /* set but for passwords too */
	MODTYPE__MAX
};

/*
 * The type of function that a rolemap is associated with.
 */
enum	rolemapt {
	ROLEMAP_ALL = 0, /* all */
	ROLEMAP_COUNT, /* count */
	ROLEMAP_DELETE, /* delete */
	ROLEMAP_INSERT, /* insert */
	ROLEMAP_ITERATE, /* iterate */
	ROLEMAP_LIST, /* list */
	ROLEMAP_SEARCH, /* search */
	ROLEMAP_UPDATE, /* update */
	ROLEMAP_NOEXPORT, /* noexport */
	ROLEMAP__MAX
};

struct	rolemap {
	enum rolemapt	     type;
	struct rrefq	     rq;
	struct strct	    *parent;
	union {
		struct field	*f;
		struct search	*s;
		struct update	*u;
	};
	TAILQ_ENTRY(rolemap) entries;
};

/*
 * A role reference.
 */
struct	rref {
	struct role	 *role; /* role in question */
	struct pos	  pos; /* parse point */
	struct rolemap	 *parent; /* which operation */
	TAILQ_ENTRY(rref) entries;
};

/*
 * A search entity.
 * For example, in a set of search criteria "user.company.name, userid",
 * this would be one of "user.company.name" or "userid".
 * One or more "struct sent" consist of a single "struct search".
 *
 */
struct	sent {
	struct pos	  pos; /* parse point */
	struct search	 *parent; /* up-reference */
	struct field	 *field; /* target of search entity */
	enum optype	  op; /* operator */
	char		 *name; /* fname w/o last field or NULL */
	char		 *fname; /* dot-form lowercase of all fields */
	char		 *uname; /* fname but with underscores */
	struct alias	 *alias; /* resolved alias */
	TAILQ_ENTRY(sent) entries;
};

enum	ordtype {
	ORDTYPE_ASC, /* ascending order */
	ORDTYPE_DESC /* descending order */
};

/*
 * An order reference.
 * This will resolve to a native field in a structure for which query
 * commands will be generated.
 * It will be produced as, for example, "ORDER BY a.b.c ASC" in
 * specifying the SQL order of a query.
 */
struct	ord {
	struct field	*field; /* resolved order field */
	char		*name; /* fname w/o last field or NULL */
	char		*fname; /* canonical dot-form name */
	enum ordtype	 op; /* type of ordering */
	struct pos	 pos; /* position in parse */
	struct search	*parent; /* up-reference */
	struct alias	*alias; /* resolved alias */
	TAILQ_ENTRY(ord) entries;
};

/*
 * Possible row-wide aggregation functions.
 */
enum	aggrtype {
	AGGR_MAXROW, /* row with maximum of all values */
	AGGR_MINROW /* row with minimum of all values */
};

/*
 * A row grouping reference.
 * This will resolve to a native field in a structure for which query
 * commands will be generated.
 * It may not be equal to any aggregate functions within the same
 * structure.
 */
struct	group {
	struct field	  *field; /* resolved group field */
	char		  *name; /* fname w/o last field or NULL */
	char		  *fname; /* canonical dot-form name */
	struct pos	   pos; /* position in parse */
	struct search	  *parent; /* up-reference */
	struct alias	  *alias; /* resolved alias */
};

/*
 * An aggregate reference.
 * This will resolve to a native field in a structure for which query
 * commands will be generated.
 * It will be produced as, for example, "MAX(a.b.c)" in the column
 * specification of the generated query.
 */
struct	aggr {
	struct field	 *field; /* resolved aggregate field */
	char		 *name; /* fname w/o last field or NULL */
	char		 *fname; /* canonical dot-form name */
	enum aggrtype	  op; /* type of aggregation */
	struct pos	  pos; /* position in parse */
	struct search	 *parent; /* up-reference */
	struct alias	 *alias; /* resolved alias */
};

enum	stype {
	STYPE_COUNT = 0,
	STYPE_SEARCH,
	STYPE_LIST,
	STYPE_ITERATE,
	STYPE__MAX
};

/*
 * A "distinct" clause set of fields.
 * This is set for search fields that are returning distinct rows.
 */
struct	dstnct {
	char		*fname; /* canonical (dotted) name */
	struct pos	 pos; /* parse point */
	struct strct	*strct; /* resolved struct */
	struct search	*parent; /* search entry */
};

struct	search {
	struct sentq	    sntq;
	struct ordq	    ordq;
	struct aggr	   *aggr;
	struct group	   *group;
	struct pos	    pos;
	struct dstnct	   *dst;
	char		   *name;
	char		   *doc;
	struct strct	   *parent;
	enum stype	    type;
	int64_t		    limit;
	int64_t		    offset;
	struct rolemap	   *rolemap;
	unsigned int	    flags; 
#define	SEARCH_IS_UNIQUE    0x01
	TAILQ_ENTRY(search) entries;
};

/*
 * An update or delete reference.
 */
struct	uref {
	enum optype	  op; /* for constraints, SQL operator */
	enum modtype	  mod; /* for modifiers */
	struct field	 *field; /* resolved field */
	struct pos	  pos; /* position in parse */
	struct update	 *parent; /* up-reference */
	TAILQ_ENTRY(uref) entries;
};

/*
 * A single field in the local structure that will be part of a unique
 * chain of fields.
 */
struct	nref {
	struct field	 *field; /* resolved field */
	struct pos	  pos; /* position in parse */
	struct unique	 *parent; /* up-reference */
	TAILQ_ENTRY(nref) entries;
};

/*
 * Define a sequence of fields in the given structure that combine to
 * form a unique clause.
 */
struct	unique {
	struct nrefq	    nq; /* constraint chain */
	struct strct	   *parent; /* up-reference */
	struct pos	    pos; /* position in parse */
	TAILQ_ENTRY(unique) entries;
};

/*
 * Type of modifier.
 */
enum	upt {
	UP_MODIFY = 0, /* generate an "update" entry */
	UP_DELETE, /* generate a "delete" entry */
	UP__MAX
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
	struct pos	    pos; /* parse point */
	struct strct	   *parent; /* up-reference */
	struct rolemap	   *rolemap; /* roles assigned to function */
	unsigned int	    flags;
#define	UPDATE_ALL	    0x01 /* UP_MODIFY for all w/"set" */
	TAILQ_ENTRY(update) entries;
};

/*
 * Simply an insertion.
 * There's nothing complicated here: it indicates that a structure can
 * have the "insert" operation.
 */
struct	insert {
	struct rolemap	*rolemap; /* roles assigned to function */
	struct strct	*parent; /* up-reference */
	struct pos	 pos; /* parse point */
};

struct	strct {
	char		  *name;
	char		  *doc;
	size_t		   height; /* dep order (XXX: remove) */
	struct pos	   pos;
	size_t		   colour; /* during linkage (XXX: remove) */
	struct field	  *rowid;
	struct fieldq	   fq;
	struct searchq	   sq;
	struct aliasq	   aq;
	struct updateq	   uq;
	struct updateq	   dq;
	struct uniqueq	   nq;
	struct rolemapq	   rq;
	struct insert	  *ins;
	struct rolemap	  *arolemap;
	unsigned int	   flags;
#define	STRCT_HAS_QUEUE	   0x01
#define	STRCT_HAS_ITERATOR 0x02
#define	STRCT_HAS_BLOB	   0x04
#define STRCT_HAS_NULLREFS 0x10
	struct config	  *cfg;
	TAILQ_ENTRY(strct) entries;
};

struct	role {
	char		  *name;
	char		  *doc;
	struct role	  *parent;
	struct roleq	   subrq;
	struct pos	   pos;
	TAILQ_ENTRY(role)  entries;
};

enum	msgtype {
	MSGTYPE_WARN, /* recoverable warning */
	MSGTYPE_ERROR, /* fatal non-system error */
	MSGTYPE_FATAL /* fatal system error */
};

/*
 * A single message emitted during the parse.
 */
struct	msg {
	struct pos	 pos; /* position (or zero/NULL) */
	enum msgtype	 type; /* type of message */
	char		*buf; /* custom buffer or NULL */
	int		 er; /* if MSGTYPE_FATAL, errno */
};

struct	config {
	struct strctq	  sq;
	struct enmq	  eq;
	struct bitfq	  bq;
	struct roleq	  rq;
	char		**langs;
	size_t		  langsz;
	char		**fnames;
	size_t		  fnamesz;
	struct msg	 *msgs;
	size_t		  msgsz;
	struct config_private *priv;
};

enum	difftype {
	DIFF_ADD_BITF,
	DIFF_ADD_BITIDX,
	DIFF_ADD_EITEM,
	DIFF_ADD_ENM,
	DIFF_DEL_BITF,
	DIFF_DEL_BITIDX,
	DIFF_DEL_EITEM,
	DIFF_DEL_ENM,
	DIFF_MOD_BITF,
	DIFF_MOD_BITF_COMMENT,
	DIFF_MOD_BITIDX,
	DIFF_MOD_BITIDX_COMMENT,
	DIFF_MOD_BITIDX_VALUE,
	DIFF_MOD_EITEM,
	DIFF_MOD_EITEM_COMMENT,
	DIFF_MOD_EITEM_VALUE,
	DIFF_MOD_ENM,
	DIFF_MOD_ENM_COMMENT,
	DIFF_SAME_BITF,
	DIFF_SAME_BITIDX,
	DIFF_SAME_EITEM,
	DIFF_SAME_ENM,
};

struct	diff_eitem {
	const struct eitem	*from;
	const struct eitem	*into;
};

struct	diff_enm {
	const struct enm	*from;
	const struct enm	*into;
};

struct	diff_bitf {
	const struct bitf	*from;
	const struct bitf	*into;
};

struct	diff_bitidx {
	const struct bitidx	*from;
	const struct bitidx	*into;
};

struct	diff {
	enum difftype	   	 	 type;
	union {
		const struct bitf	*bitf;
		struct diff_bitf	 bitf_pair;
		const struct bitidx	*bitidx;
		struct diff_bitidx	 bitidx_pair;
		const struct enm	*enm;
		struct diff_enm		 enm_pair;
		const struct eitem	*eitem;
		struct diff_eitem	 eitem_pair; 
	};
	TAILQ_ENTRY(diff) 	 	 entries;
};

__BEGIN_DECLS

struct config	*ort_config_alloc(void);
void		 ort_config_free(struct config *);
int		 ort_parse_close(struct config *);
int		 ort_parse_file(struct config *, FILE *, const char *);
int		 ort_write_file(FILE *, const struct config *);
struct diffq	*ort_diff(const struct config *, const struct config *);
void		 ort_diff_free(struct diffq *);
int		 ort_write_diff_file(FILE *, const struct diffq *,
			const char **, size_t, const char **, size_t);

__END_DECLS

#endif /* !ORT_H */
