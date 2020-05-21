/*	$Id$ */
/*
 * Copyright (c) 2017--2019 Kristaps Dzonsons <kristaps@bsd.lv>
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

/*
 * We use many queues.
 * Here they are...
 */
TAILQ_HEAD(aliasq, alias);
TAILQ_HEAD(bitfq, bitf);
TAILQ_HEAD(bitidxq, bitidx);
TAILQ_HEAD(eitemq, eitem);
TAILQ_HEAD(enmq, enm);
TAILQ_HEAD(fieldq, field);
TAILQ_HEAD(fvalidq, fvalid);
TAILQ_HEAD(labelq, label);
TAILQ_HEAD(nrefq, nref);
TAILQ_HEAD(ordq, ord);
TAILQ_HEAD(rolemapq, rolemap);
TAILQ_HEAD(roleq, role);
TAILQ_HEAD(rolesetq, roleset);
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

/*
 * A language-specific label.
 * The default language is always index 0.
 * See the "langs" array in struct config.
 */
struct	label {
	char		  *label; /* the label itself */
	size_t		   lang; /* the language */
	struct pos	   pos; /* parse point */
	TAILQ_ENTRY(label) entries;
};

/*
 * A single item within an enumeration.
 * This may be statically assigned a value in "value" or it may be
 * automatically done during linking.
 */
struct	eitem {
	char		  *name; /* item name */
	int64_t		   value; /* numeric value */
	char		  *doc; /* documentation */
	struct labelq	   labels; /* javascript labels */
	struct pos	   pos; /* parse point */
	struct enm	  *parent; /* parent enumeration */
	unsigned int	   flags;
#define	EITEM_AUTO	   0x01 /* auto-numbering */
	TAILQ_ENTRY(eitem) entries;
};

/*
 * An enumeration of a field's possible values.
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

/*
 * A single bit index within a bitfield.
 */
struct	bitidx {
	char		   *name;
	char		   *doc; /* documentation */
	struct labelq	    labels; /* javascript labels */
	int64_t		    value; /* bit 0--63 */
	struct bitf	   *parent;
	struct pos	    pos; /* parse point */
	TAILQ_ENTRY(bitidx) entries;
};

/*
 * A 64-bit bitfield (set of bit indices).
 */
struct	bitf {
	char		 *name; /* name of bitfield */
	char		 *cname; /* capitalised name */
	char		 *doc; /* documentation */
	struct labelq	  labels_unset; /* "isunset" js labels */
	struct labelq	  labels_null; /* "isnull" js labels */
	struct pos	  pos; /* parse point */
	struct bitidxq	  bq; /* bit indices */
	TAILQ_ENTRY(bitf) entries;
};

/*
 * Update/delete action.
 * Defaults to UPACT_NONE (no special action).
 */
enum	upact {
	UPACT_NONE = 0,
	UPACT_RESTRICT,
	UPACT_NULLIFY,
	UPACT_CASCADE,
	UPACT_DEFAULT,
	UPACT__MAX
};

/*
 * A field defining a database/struct mapping.
 * This can be either reflected in the database, in the C API, or both.
 */
struct	field {
	char		  *name; /* column name */
	struct ref	  *ref; /* "foreign key" ref (or null) */
	struct enm	  *enm;  /* enumeration ref (or null) */
	struct bitf	  *bitf;  /* bitfield ref (or null) */
	char		  *doc; /* documentation */
	struct pos	   pos; /* parse point */
	union {
		int64_t integer;
		double decimal;
		char *string;
	} def; /* a default value */
	enum ftype	   type; /* type of column */
	enum upact	   actdel; /* delete action */
	struct rolemap	  *rolemap; /* roles for not exporting */
	enum upact	   actup; /* update action */
	struct strct	  *parent; /* parent reference */
	struct fvalidq	   fvq; /* validation */
	unsigned int	   flags; /* flags */
#define	FIELD_ROWID	   0x01 /* this is a rowid field */
#define	FIELD_UNIQUE	   0x02 /* this is a unique field */
#define FIELD_NULL	   0x04 /* can be null */
#define	FIELD_NOEXPORT	   0x08 /* don't export the field (JSON) */
#define FIELD_HASDEF	   0x10 /* has a default value */
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
 * Most functions (all except for "insert") are also tagged with a name.
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

/*
 * Maps a given operation (like an insert named "foo") with a set of
 * roles with permission to perform the operation (setq).
 */
struct	rolemap {
	char		    *name; /* name of operation */
	enum rolemapt	     type; /* type */
	struct rolesetq	     setq; /* allowed roles */
	struct pos	     pos; /* position */
	TAILQ_ENTRY(rolemap) entries;
};

/*
 * One of a set of roles allows to perform the given parent operation.
 * A roleset (after linkage) will map into an actual role.
 */
struct	roleset {
	char		    *name; /* name of role */
	struct role	    *role; /* post-linkage association */
	struct rolemap	    *parent; /* which operation */
	TAILQ_ENTRY(roleset) entries;
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
	char		 *uname; /* underscore-form lowercase form */
	struct alias	 *alias; /* resolved alias */
	unsigned int	  flags; 
#define	SENT_IS_UNIQUE	  0x01 /* has a rowid/unique in its refs */
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

/*
 * Type of search.
 * We have many different kinds of search functions, each represented by
 * the same "struct search", without different semantics.
 */
enum	stype {
	STYPE_COUNT = 0, /* single counting response */
	STYPE_SEARCH, /* singular response */
	STYPE_LIST, /* queue of responses */
	STYPE_ITERATE, /* iterator of responses */
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

/*
 * A set of fields to search by and return results.
 * A "search" implies zero or more responses given a query; for example,
 * a unique response to the set of sets "user.company.name, userid",
 * which has two search entities (struct sent).
 */
struct	search {
	struct sentq	    sntq; /* nested reference chain */
	struct ordq	    ordq; /* ordering chains */
	struct aggr	   *aggr; /* aggregate chain or NULL */
	struct group	   *group; /* grouping chain or NULL */
	struct pos	    pos; /* parse point */
	struct dstnct	   *dst; /* distinct constraint or NULL */
	char		   *name; /* named or NULL */
	char		   *doc; /* documentation */
	struct strct	   *parent; /* up-reference */
	enum stype	    type; /* type of search */
	int64_t		    limit; /* query limit or zero (unset) */
	int64_t		    offset; /* query offset or zero (unset) */
	struct rolemap	   *rolemap; /* roles assigned to search */
	unsigned int	    flags; 
#define	SEARCH_IS_UNIQUE    0x01 /* has a rowid or unique somewhere */
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
	struct rolemapq	   rq; /* role assignments */
	struct insert	  *ins; /* insert function */
	struct rolemap	  *arolemap; /* catcha-all rolemap */
	unsigned int	   flags;
#define	STRCT_HAS_QUEUE	   0x01 /* needs a queue interface */
#define	STRCT_HAS_ITERATOR 0x02 /* needs iterator interface */
#define	STRCT_HAS_BLOB	   0x04 /* needs resolv.h */
#define STRCT_HAS_NULLREFS 0x10 /* has nested null fkeys */
	TAILQ_ENTRY(strct) entries;
	struct config	  *cfg; /* up-reference */
};

/*
 * Roles are used in the RBAC mechanism of the system.
 * It's just a name possibly nested within another role.
 * In a structure, a function can be associated with a "rolemap" that
 * maps back into roles permitted for the function.
 */
struct	role {
	char		  *name; /* unique lowercase name of role */
	char		  *doc; /* documentation */
	struct role	  *parent; /* parent (or NULL) */
	struct roleq	   subrq; /* sub-roles */
	struct pos	   pos; /* parse point */
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

/*
 * Hold entire parse sequence results.
 */
struct	config {
	struct strctq	  sq; /* all structures */
	struct enmq	  eq; /* all enumerations */
	struct bitfq	  bq; /* all bitfields */
	struct roleq	  rq; /* all roles (this is a tree) */
	char		**langs; /* known label langs */
	size_t		  langsz; /* number of langs */
	char		**fnames; /* filenames referenced */
	size_t		  fnamesz; /* number of fnames */
	struct msg	 *msgs; /* warning/error messages */
	size_t		  msgsz; /* count of msgs */
	struct config_private *priv; /* non-exported data */
};

__BEGIN_DECLS

struct config	*ort_config_alloc(void);
void		 ort_config_free(struct config *);
int		 ort_parse_close(struct config *);
int		 ort_parse_file_r(struct config *, FILE *, const char *);
struct config	*ort_parse_file(FILE *, const char *);
int		 ort_write_file(FILE *, const struct config *);

__END_DECLS

#endif /* !ORT_H */
