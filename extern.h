#ifndef EXTERN_H
#define EXTERN_H

enum	ftype {
	FTYPE_INT,
	FTYPE_TEXT,
	FTYPE_REF
};

/*
 * An object reference into another table.
 * This is gathered during the syntax parse phase, then linked to an
 * actual table afterwards.
 */
struct	ref {
	struct field	*target; /* target (after linkage) */
	struct field	*source; /* source (after linkage) */
	char		*sfield; /* column with foreign key */
	char		*tstrct; /* target structure */
	char		*tfield; /* target field */
	struct field	*parent; /* parent reference */
};

/*
 * A field defining a database/struct mapping.
 */
struct	field {
	char		  *name; /* column name */
	struct ref	  *ref; /* if FTYPE_REF */
	enum ftype	   type; /* type of column */
	struct strct	  *parent; /* parent reference */
	TAILQ_ENTRY(field) entries;
};

TAILQ_HEAD(fieldq, field);

struct	strct {
	char		  *name;
	char		  *docs;
	struct fieldq	   fq;
	TAILQ_ENTRY(strct) entries;
};

TAILQ_HEAD(strctq, strct);

__BEGIN_DECLS

int		 parse_link(struct strctq *);
struct strctq	*parse_config(FILE *, const char *);
void		 parse_free(struct strctq *);

void		 gen_header(const struct strctq *);
void		 gen_source(const struct strctq *);

__END_DECLS

#endif /* ! EXTERN_H */
