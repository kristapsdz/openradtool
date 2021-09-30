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
TAILQ_HEAD(auditq, audit);
TAILQ_HEAD(bitfq, bitf);
TAILQ_HEAD(bitidxq, bitidx);
TAILQ_HEAD(diffq, diff);
TAILQ_HEAD(eitemq, eitem);
TAILQ_HEAD(enmq, enm);
TAILQ_HEAD(fieldq, field);
TAILQ_HEAD(fvalidq, fvalid);
TAILQ_HEAD(labelq, label);
TAILQ_HEAD(msgq, msg);
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

struct	ref {
	struct field 	*target;
	struct field 	*source;
	struct field	*parent;
};

enum	vtype {
	VALIDATE_GE = 0, /* greater than-eq length or value */
	VALIDATE_LE, /* less than-eq length or value */
	VALIDATE_GT, /* greater than length or value */
	VALIDATE_LT, /* less than length or value */
	VALIDATE_EQ, /* equal length or value */
	VALIDATE__MAX
};

struct	fvalid {
	enum vtype	 type;
	union {
		union {
			int64_t integer;
			double decimal;
			size_t len;
		} value;
	} d;
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
#define	FIELD_ROWID	   0x01u
#define	FIELD_UNIQUE	   0x02u
#define FIELD_NULL	   0x04u
#define	FIELD_NOEXPORT	   0x08u
#define FIELD_HASDEF	   0x10u
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

struct	rref {
	struct role	 *role;
	struct pos	  pos;
	struct rolemap	 *parent;
	TAILQ_ENTRY(rref) entries;
};

struct	sent {
	struct field	**chain;
	size_t		  chainsz;
	struct pos	  pos;
	struct search	 *parent;
	struct field	 *field;
	enum optype	  op;
	char		 *name;
	char		 *fname;
	char		 *uname;
	struct alias	 *alias;
	TAILQ_ENTRY(sent) entries;
};

enum	ordtype {
	ORDTYPE_ASC,
	ORDTYPE_DESC
};

struct	ord {
	struct field	**chain;
	size_t		  chainsz;
	struct field	*field;
	char		*name;
	char		*fname;
	enum ordtype	 op;
	struct pos	 pos;
	struct search	*parent;
	struct alias	*alias;
	TAILQ_ENTRY(ord) entries;
};

enum	aggrtype {
	AGGR_MAXROW,
	AGGR_MINROW
};

struct	group {
	struct field	**chain;
	size_t		  chainsz;
	struct field	  *field;
	char		  *name;
	char		  *fname;
	struct pos	   pos;
	struct search	  *parent;
	struct alias	  *alias;
};

struct	aggr {
	struct field	**chain;
	size_t		  chainsz;
	struct field	 *field;
	char		 *name;
	char		 *fname;
	enum aggrtype	  op;
	struct pos	  pos;
	struct search	 *parent;
	struct alias	 *alias;
};

enum	stype {
	STYPE_COUNT = 0,
	STYPE_SEARCH,
	STYPE_LIST,
	STYPE_ITERATE,
	STYPE__MAX
};

struct	dstnct {
	struct field	**chain;
	size_t		  chainsz;
	char		 *fname;
	struct pos	  pos;
	struct strct	 *strct;
	struct search	 *parent;
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

struct	uref {
	enum optype	  op;
	enum modtype	  mod;
	struct field	 *field;
	struct pos	  pos;
	struct update	 *parent;
	TAILQ_ENTRY(uref) entries;
};

struct	nref {
	struct field	 *field;
	struct pos	  pos;
	struct unique	 *parent;
	TAILQ_ENTRY(nref) entries;
};

struct	unique {
	struct nrefq	    nq;
	struct strct	   *parent;
	struct pos	    pos;
	TAILQ_ENTRY(unique) entries;
};

enum	upt {
	UP_MODIFY = 0,
	UP_DELETE,
	UP__MAX
};

struct	update {
	struct urefq	    mrq;
	struct urefq	    crq;
	char		   *name;
	char		   *doc;
	enum upt	    type;
	struct pos	    pos;
	struct strct	   *parent;
	struct rolemap	   *rolemap;
	unsigned int	    flags;
#define	UPDATE_ALL	    0x01
	TAILQ_ENTRY(update) entries;
};

struct	insert {
	struct rolemap	*rolemap;
	struct strct	*parent;
	struct pos	 pos;
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
	struct rolemap	  *arolemap; /* during linkage (XXX: remove) */
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
	TAILQ_ENTRY(role)  allentries;
};

enum	msgtype {
	MSGTYPE_WARN,
	MSGTYPE_ERROR,
	MSGTYPE_FATAL
};

struct	msg {
	char		*fname;
	size_t		 line;
	size_t		 column;
	enum msgtype	 type;
	char		*buf;
	int		 er;
	TAILQ_ENTRY(msg) entries;
};

struct	config {
	struct strctq	  sq;
	struct enmq	  eq;
	struct bitfq	  bq;
	struct roleq	  rq;
	struct roleq	  arq;
	char		**langs;
	size_t		  langsz;
	char		**fnames;
	size_t		  fnamesz;
	struct msgq	  mq;
	struct config_private *priv;
};

enum	difftype {
	DIFF_ADD_BITF,
	DIFF_ADD_BITIDX,
	DIFF_ADD_EITEM,
	DIFF_ADD_ENM,
	DIFF_ADD_FIELD,
	DIFF_ADD_INSERT,
	DIFF_ADD_ROLE,
	DIFF_ADD_ROLES,
	DIFF_ADD_SEARCH,
	DIFF_ADD_STRCT,
	DIFF_ADD_UNIQUE,
	DIFF_ADD_UPDATE,
	DIFF_DEL_BITF,
	DIFF_DEL_BITIDX,
	DIFF_DEL_EITEM,
	DIFF_DEL_ENM,
	DIFF_DEL_FIELD,
	DIFF_DEL_INSERT,
	DIFF_DEL_ROLE,
	DIFF_DEL_ROLES,
	DIFF_DEL_SEARCH,
	DIFF_DEL_STRCT,
	DIFF_DEL_UNIQUE,
	DIFF_DEL_UPDATE,
	DIFF_MOD_BITF,
	DIFF_MOD_BITF_COMMENT,
	DIFF_MOD_BITF_LABELS,
	DIFF_MOD_BITIDX,
	DIFF_MOD_BITIDX_COMMENT,
	DIFF_MOD_BITIDX_LABELS,
	DIFF_MOD_BITIDX_VALUE,
	DIFF_MOD_EITEM,
	DIFF_MOD_EITEM_COMMENT,
	DIFF_MOD_EITEM_LABELS,
	DIFF_MOD_EITEM_VALUE,
	DIFF_MOD_ENM,
	DIFF_MOD_ENM_COMMENT,
	DIFF_MOD_ENM_LABELS,
	DIFF_MOD_FIELD,
	DIFF_MOD_FIELD_ACTIONS,
	DIFF_MOD_FIELD_BITF,
	DIFF_MOD_FIELD_COMMENT,
	DIFF_MOD_FIELD_DEF,
	DIFF_MOD_FIELD_ENM,
	DIFF_MOD_FIELD_FLAGS,
	DIFF_MOD_FIELD_REFERENCE,
	DIFF_MOD_FIELD_ROLEMAP,
	DIFF_MOD_FIELD_TYPE,
	DIFF_MOD_FIELD_VALIDS,
	DIFF_MOD_INSERT,
	DIFF_MOD_INSERT_PARAMS,
	DIFF_MOD_INSERT_ROLEMAP,
	DIFF_MOD_ROLE,
	DIFF_MOD_ROLE_CHILDREN,
	DIFF_MOD_ROLE_COMMENT,
	DIFF_MOD_ROLE_PARENT,
	DIFF_MOD_ROLES,
	DIFF_MOD_SEARCH,
	DIFF_MOD_SEARCH_AGGR,
	DIFF_MOD_SEARCH_COMMENT,
	DIFF_MOD_SEARCH_DISTINCT,
	DIFF_MOD_SEARCH_GROUP,
	DIFF_MOD_SEARCH_LIMIT,
	DIFF_MOD_SEARCH_OFFSET,
	DIFF_MOD_SEARCH_ORDER,
	DIFF_MOD_SEARCH_PARAMS,
	DIFF_MOD_SEARCH_ROLEMAP,
	DIFF_MOD_STRCT,
	DIFF_MOD_STRCT_COMMENT,
	DIFF_MOD_UPDATE,
	DIFF_MOD_UPDATE_COMMENT,
	DIFF_MOD_UPDATE_FLAGS,
	DIFF_MOD_UPDATE_PARAMS,
	DIFF_MOD_UPDATE_ROLEMAP,
	DIFF_SAME_BITF,
	DIFF_SAME_BITIDX,
	DIFF_SAME_EITEM,
	DIFF_SAME_ENM,
	DIFF_SAME_FIELD,
	DIFF_SAME_INSERT,
	DIFF_SAME_ROLE,
	DIFF_SAME_ROLES,
	DIFF_SAME_SEARCH,
	DIFF_SAME_STRCT,
	DIFF_SAME_UPDATE,
	DIFF__MAX
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

struct	diff_field {
	const struct field	*from;
	const struct field	*into;
};

struct	diff_update {
	const struct update	*from;
	const struct update	*into;
};

struct	diff_role {
	const struct role	*from;
	const struct role	*into;
};

struct	diff_strct {
	const struct strct	*from;
	const struct strct	*into;
};

struct	diff_search {
	const struct search	*from;
	const struct search	*into;
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
		const struct field	*field;
		struct diff_field	 field_pair;
		const struct eitem	*eitem;
		struct diff_eitem	 eitem_pair; 
		const struct role	*role;
		struct diff_role	 role_pair; 
		const struct search	*search;
		struct diff_search	 search_pair; 
		const struct strct	*strct;
		struct diff_strct	 strct_pair; 
		const struct unique	*unique;
		struct diff_update	 update_pair;
		const struct update	*update;
	};
	TAILQ_ENTRY(diff) 	 	 entries;
};

enum	auditt {
	AUDIT_INSERT,
	AUDIT_UPDATE,
	AUDIT_QUERY,
	AUDIT_REACHABLE,
};

struct	auditpaths {
	const struct search	*sr;
	char			*path;
	int			 exported;
};

struct	auditfield {
	const struct field	*fd;
	int			 exported;
};

struct	auditreach {
	const struct strct	 *st;
	struct auditpaths	 *srs;
	size_t			  srsz;
	struct auditfield	 *fds;
	size_t			  fdsz;
	int			  exported;
};

struct	audit {
	union {
		const struct strct	*st;
		const struct update	*up;
		const struct search	*sr;
		struct auditreach	 ar;
	};
	enum auditt		 type;
	TAILQ_ENTRY(audit)	 entries;
};

__BEGIN_DECLS

struct config	*ort_config_alloc(void);
void		 ort_config_free(struct config *);
int		 ort_parse_close(struct config *);
int		 ort_parse_file(struct config *, FILE *, const char *);
int		 ort_write_file(FILE *, const struct config *);
int		 ort_write_msg_file(FILE *f, const struct msgq *);
struct diffq	*ort_diff(const struct config *, const struct config *);
void		 ort_diffq_free(struct diffq *);
int		 ort_write_diff_file(FILE *, const struct diffq *,
			const char *const *, size_t, const char *const *, size_t);
void	 	 ort_msgv(struct msgq *, enum msgtype, int, 
			const struct pos *, const char *, va_list);
void	 	 ort_msg(struct msgq *, enum msgtype, int, 
			const struct pos *, const char *, ...);
void		 ort_msgq_free(struct msgq *);
struct auditq	*ort_audit(const struct role *, const struct config *);
void		 ort_auditq_free(struct auditq *);

__END_DECLS

#endif /* !ORT_H */
