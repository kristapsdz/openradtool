#ifndef EXTERN_H
#define EXTERN_H

enum	ftype {
	FTYPE_INT,
	FTYPE_TEXT
};

struct	field {
	char		  *name;
	enum ftype	   type;
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

struct strctq	*parse_config(FILE *, const char *);
void		 parse_free(struct strctq *);

void		 gen_header(const struct strctq *);
void		 gen_source(const struct strctq *);

__END_DECLS

#endif /* ! EXTERN_H */
