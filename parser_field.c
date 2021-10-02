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
#include "config.h"

#if HAVE_SYS_QUEUE
# include <sys/queue.h>
#endif

#include <assert.h>
#include <ctype.h>
#include <inttypes.h>
#include <limits.h> /* INT_MAX */
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "ort.h"
#include "extern.h"
#include "parser.h"

struct typemap {
	enum ftype	 type;
	const char	*name;
};

/* 
 * These are all names (or aliases) for field types.
 * Add as many aliases as needed.
 */

static const struct typemap ftypes[] = {
	{ FTYPE_BIT, "bit" },
	{ FTYPE_BITFIELD, "bitfield" },
	{ FTYPE_BITFIELD, "bits" },
	{ FTYPE_BLOB, "blob" },
	{ FTYPE_DATE, "date" },
	{ FTYPE_REAL, "double" },
	{ FTYPE_EMAIL, "email" },
	{ FTYPE_ENUM, "enum" },
	{ FTYPE_EPOCH, "epoch" },
	{ FTYPE_INT, "int" },
	{ FTYPE_INT, "integer" },
	{ FTYPE_PASSWORD, "passwd" },
	{ FTYPE_PASSWORD, "password" },
	{ FTYPE_REAL, "real" },
	{ FTYPE_STRUCT, "struct" },
	{ FTYPE_TEXT, "text" },
	{ FTYPE_TEXT, "txt" },
	{ FTYPE__MAX, NULL }
};

static const char *const vtypes[VALIDATE__MAX] = {
	"ge", /* VALIDATE_GE */
	"le", /* VALIDATE_LE */
	"gt", /* VALIDATE_GT */
	"lt", /* VALIDATE_LT */
	"eq", /* VALIDATE_EQ */
};

/*
 * Parse a limit statement.
 * These correspond to kcgi(3) validations.
 */
static void
parse_validate(struct parse *p, struct field *fd)
{
	enum vtype		 vt;
	enum tok		 tok;
	struct fvalid		*v;
	const struct fvalid	*vv;

	if (fd->type == FTYPE_STRUCT) {
		parse_errx(p, "no validation on structs");
		return;
	} else if (fd->type == FTYPE_ENUM) {
		/*
		 * FIXME: it should be possible to have an enumeration
		 * limit where the ge/le/eq accept an enumeration value.
		 */
		parse_errx(p, "no validation on enums");
		return;
	}

	if (parse_next(p) != TOK_IDENT) {
		parse_errx(p, "expected constraint type");
		return;
	}

	for (vt = 0; vt < VALIDATE__MAX; vt++)
		if (strcasecmp(p->last.string, vtypes[vt]) == 0)
			break;

	if (vt == VALIDATE__MAX) {
		parse_errx(p, "unknown constraint type");
		return;
	}

	if ((v = calloc(1, sizeof(struct fvalid))) == NULL) {
		parse_err(p);
		return;
	}
	v->type = vt;
	TAILQ_INSERT_TAIL(&fd->fvq, v, entries);

	/* Assign default and make sure we're unique. */

	switch (fd->type) {
	case FTYPE_BIT:
	case FTYPE_BITFIELD:
	case FTYPE_DATE:
	case FTYPE_EPOCH:
	case FTYPE_INT:
		if (parse_next(p) != TOK_INTEGER) {
			parse_errx(p, "expected integer");
			return;
		}
		v->d.value.integer = p->last.integer;
		TAILQ_FOREACH(vv, &fd->fvq, entries)
			if (v != vv &&
			    v->type == vv->type &&
			    v->d.value.integer == vv->d.value.integer) {
				parse_errx(p, "duplicate validation");
				return;
			}
		break;
	case FTYPE_REAL:
		tok = parse_next(p);
		if (tok != TOK_DECIMAL && tok != TOK_INTEGER) {
			parse_errx(p, "expected decimal or integer");
			return;
		}
		v->d.value.decimal = tok == TOK_DECIMAL ? 
			p->last.decimal : (double)p->last.integer;
		TAILQ_FOREACH(vv, &fd->fvq, entries)
			if (v != vv &&
			    v->type == vv->type &&
			    v->d.value.decimal == vv->d.value.decimal) {
				parse_errx(p, "duplicate validation");
				return;
			}
		break;
	case FTYPE_BLOB:
	case FTYPE_EMAIL:
	case FTYPE_TEXT:
	case FTYPE_PASSWORD:
		if (parse_next(p) != TOK_INTEGER) {
			parse_errx(p, "expected length");
			return;
		} 
		if (p->last.integer < 0 ||
		    (uint64_t)p->last.integer > SIZE_MAX) {
			parse_errx(p, "expected length");
			return;
		}
		v->d.value.len = (size_t)p->last.integer;
		TAILQ_FOREACH(vv, &fd->fvq, entries)
			if (v != vv && 
			    v->type == vv->type &&
			    v->d.value.len == vv->d.value.len) {
				parse_errx(p, "duplicate validation");
				return;
			}
		break;
	default:
		abort();
	}
}

/*
 * Parse the action taken on a foreign key's delete or update.
 * This can be one of none, restrict, nullify, cascade, or default.
 */
static void
parse_action(struct parse *p, enum upact *act)
{

	*act = UPACT_NONE;

	if (parse_next(p) != TOK_IDENT)
		parse_errx(p, "expected action");
	else if (strcasecmp(p->last.string, "none") == 0)
		*act = UPACT_NONE;
	else if (strcasecmp(p->last.string, "restrict") == 0)
		*act = UPACT_RESTRICT;
	else if (strcasecmp(p->last.string, "nullify") == 0)
		*act = UPACT_NULLIFY;
	else if (strcasecmp(p->last.string, "cascade") == 0)
		*act = UPACT_CASCADE;
	else if (strcasecmp(p->last.string, "default") == 0)
		*act = UPACT_DEFAULT;
	else
		parse_errx(p, "unknown action");
}

/*
 * Read auxiliary information for a field.
 * Its syntax is:
 *
 *   [options | "comment" string_literal]* ";"
 *
 * The options are any of "rowid", "unique", or "noexport".
 * This will continue processing until the semicolon is reached.
 */
static void
parse_field_info(struct parse *p, struct field *fd)
{
	struct tm	 tm;
	struct resolve	*r;

	while (!PARSE_STOP(p)) {
		if (parse_next(p) == TOK_SEMICOLON)
			break;
		if (p->lasttype != TOK_IDENT) {
			parse_errx(p, "unknown field info token");
			break;
		}

		if (strcasecmp(p->last.string, "rowid") == 0) {
			if (fd->parent->rowid != NULL) {
				parse_errx(p, "multiple rowids");
				break;
			}

			/*
			 * This must be on an integer type, must not be
			 * on a foreign key reference, must not have its
			 * parent already having a rowid, must not take
			 * null values, and must not already be
			 * specified.
			 */

			if (fd->type != FTYPE_INT) {
				parse_errx(p, "rowid for non-int type");
				break;
			} else if (fd->ref != NULL) {
				parse_errx(p, "rowid on reference");
				break;
			} else if (fd->parent->rowid != NULL) {
				parse_errx(p, "struct already has rowid");
				break;
			} else if (fd->flags & FIELD_NULL) {
				parse_errx(p, "rowid can't be null");
				break;
			} 
			
			if (fd->flags & FIELD_UNIQUE) {
				parse_warnx(p, "unique is redundant");
				fd->flags &= ~FIELD_UNIQUE;
			}

			fd->flags |= FIELD_ROWID;
			fd->parent->rowid = fd;
		} else if (strcasecmp(p->last.string, "noexport") == 0) {
			if (fd->type == FTYPE_PASSWORD)
				parse_warnx(p, "noexport is redundant");
			fd->flags |= FIELD_NOEXPORT;
		} else if (strcasecmp(p->last.string, "limit") == 0) {
			parse_validate(p, fd);
		} else if (strcasecmp(p->last.string, "unique") == 0) {
			/* 
			 * This must not be on a foreign key reference
			 * and is ignored for rowids.
			 */

			if (fd->type == FTYPE_STRUCT) {
				parse_errx(p, "unique on struct");
				break;
			} else if (fd->flags & FIELD_ROWID) {
				parse_warnx(p, "unique is redunant");
				continue;
			}

			fd->flags |= FIELD_UNIQUE;
		} else if (strcasecmp(p->last.string, "null") == 0) {
			/*
			 * These fields can't be rowids, nor can they be
			 * struct types.
			 */

			if (fd->flags & FIELD_ROWID) {
				parse_errx(p, "rowid can't be null");
				break;
			} else if (fd->type == FTYPE_STRUCT) {
				parse_errx(p, "struct types can't be null");
				break;
			}

			fd->flags |= FIELD_NULL;
		} else if (strcasecmp(p->last.string, "comment") == 0) {
			parse_comment(p, &fd->doc);
		} else if (strcasecmp(p->last.string, "actup") == 0) {
			if (fd->ref == NULL || 
			    fd->type == FTYPE_STRUCT) {
				parse_errx(p, "action on non-reference");
				break;
			}
			parse_action(p, &fd->actup);
		} else if (strcasecmp(p->last.string, "actdel") == 0) {
			if (fd->ref == NULL || 
			    fd->type == FTYPE_STRUCT) {
				parse_errx(p, "action on non-reference");
				break;
			}
			parse_action(p, &fd->actdel);
		} else if (strcasecmp(p->last.string, "default") == 0) {
			switch (fd->type) {
			case FTYPE_DATE:
				/* We want a date. */
				memset(&tm, 0, sizeof(struct tm));
				if (parse_next(p) != TOK_INTEGER) {
					parse_errx(p, "expected year (integer)");
					break;
				} else if (p->last.integer > INT_MAX) {
					parse_errx(p, "invalid year");
					break;
				}
				tm.tm_year = (int)p->last.integer - 1900;
				if (parse_next(p) != TOK_INTEGER) {
					parse_errx(p, "expected month (integer)");
					break;
				} else if (p->last.integer >= 0) {
					parse_errx(p, "invalid month");
					break;
				} else if (p->last.integer < INT_MIN) {
					parse_errx(p, "invalid month");
					break;
				}
				tm.tm_mon = (int)-p->last.integer - 1;
				if (parse_next(p) != TOK_INTEGER) {
					parse_errx(p, "expected day (integer)");
					break;
				} else if (p->last.integer >= 0) {
					parse_errx(p, "invalid day");
					break;
				} else if (p->last.integer < INT_MIN) {
					parse_errx(p, "invalid day");
					break;
				}
				tm.tm_mday = (int)-p->last.integer;
				tm.tm_isdst = -1;
				fd->flags |= FIELD_HASDEF;
				fd->def.integer = mktime(&tm); 
				break;
			case FTYPE_BIT:
			case FTYPE_BITFIELD:
			case FTYPE_EPOCH:
			case FTYPE_INT:
				if (parse_next(p) != TOK_INTEGER) {
					parse_errx(p, "expected integer");
					break;
				}
				fd->flags |= FIELD_HASDEF;
				fd->def.integer = p->last.integer;
				break;
			case FTYPE_REAL:
				parse_next(p);
				if (p->lasttype != TOK_DECIMAL &&
				    p->lasttype != TOK_INTEGER) {
					parse_errx(p, "expected "
						"real or integer");
					break;
				}
				fd->flags |= FIELD_HASDEF;
				fd->def.decimal = 
					p->lasttype == TOK_DECIMAL ?
					p->last.decimal : 
					(double)p->last.integer;
				break;
			case FTYPE_EMAIL:
			case FTYPE_TEXT:
				if (parse_next(p) != TOK_LITERAL) {
					parse_errx(p, "expected literal");
					break;
				}
				fd->flags |= FIELD_HASDEF;
				fd->def.string = strdup(p->last.string);
				if (fd->def.string == NULL) {
					parse_err(p);
					return;
				}
				break;
			case FTYPE_ENUM:
				if (parse_next(p) != TOK_IDENT) {
					parse_errx(p, "expected identifier");
					break;
				}
				fd->flags |= FIELD_HASDEF;
				fd->def.eitem = NULL;
				r = calloc(1, sizeof(struct resolve));
				if (r == NULL) {
					parse_err(p);
					return;
				}
				TAILQ_INSERT_TAIL
					(&p->cfg->priv->rq, r, entries);
				r->type = RESOLVE_FIELD_DEFAULT_EITEM;
				r->field_def_eitem.result = fd;
				r->field_def_eitem.name = 
					strdup(p->last.string);
				if (r->field_def_eitem.name == NULL) {
					parse_err(p);
					return;
				}
				break;
			default:
				parse_errx(p, "defaults not "
					"available for type");
				break;
			}
		} else
			parse_errx(p, "unknown field info token");
	}
}

/*
 * Creates empty "bref" on field and requests RESOLVE_FIELD_BITS.
 */
static void
parse_field_bits(struct parse *p, struct field *fd)
{
	struct resolve	*r;

	if ((r = calloc(1, sizeof(struct resolve))) == NULL) {
		parse_err(p);
		return;
	}

	TAILQ_INSERT_TAIL(&p->cfg->priv->rq, r, entries);
	r->type = RESOLVE_FIELD_BITS;
	r->field_bits.result = fd;

	if (parse_next(p) != TOK_IDENT) {
		parse_errx(p, "expected bitfield name");
		return;
	} 
	r->field_bits.name = strdup(p->last.string);
	if (r->field_bits.name == NULL)
		parse_err(p);
}

/*
 * Creates empty "eref" on field and requests RESOLVE_FIELD_ENUM.
 */
static void
parse_field_enum(struct parse *p, struct field *fd)
{
	struct resolve	*r;

	if ((r = calloc(1, sizeof(struct resolve))) == NULL) {
		parse_err(p);
		return;
	}

	TAILQ_INSERT_TAIL(&p->cfg->priv->rq, r, entries);
	r->type = RESOLVE_FIELD_ENUM;
	r->field_enum.result = fd;

	if (parse_next(p) != TOK_IDENT) {
		parse_errx(p, "expected enum name");
		return;
	} 
	r->field_enum.name = strdup(p->last.string);
	if (r->field_enum.name == NULL)
		parse_err(p);
}

/*
 * Creates empty "ref" on field and requests RESOLVE_FIELD_FOREIGN.
 */
static void
parse_field_foreign(struct parse *p, struct field *fd)
{
	struct resolve	*r;

	if ((fd->ref = calloc(1, sizeof(struct ref))) == NULL) {
		parse_err(p);
		return;
	}
	fd->ref->parent = fd;
	fd->ref->source = fd;
	
	if ((r = calloc(1, sizeof(struct resolve))) == NULL) {
		parse_err(p);
		return;
	}

	TAILQ_INSERT_TAIL(&p->cfg->priv->rq, r, entries);
	r->type = RESOLVE_FIELD_FOREIGN;
	r->field_foreign.result = fd->ref;

	if (parse_next(p) != TOK_IDENT)
		parse_errx(p, "expected target struct");
	else if ((r->field_foreign.tstrct =
		 strdup(p->last.string)) == NULL)
		parse_err(p);
	else if (parse_next(p) != TOK_PERIOD)
		parse_errx(p, "expected period");
	else if (parse_next(p) != TOK_IDENT)
		parse_errx(p, "expected target field");
	else if ((r->field_foreign.tfield = 
	         strdup(p->last.string)) == NULL)
		parse_err(p);
}

/*
 * Creates empty "ref" on field and requests RESOLVE_FIELD_STRUCT.
 * This can't already have a reference.
 */
static void
parse_field_struct(struct parse *p, struct field *fd)
{
	struct resolve	*r;

	/* This is already a foreign key reference. */

	if (fd->ref != NULL) {
		parse_errx(p, "reference cannot be a struct");
		return;
	} else if ((fd->ref = calloc(1, sizeof(struct ref))) == NULL) {
		parse_err(p);
		return;
	}
	fd->ref->parent = fd;

	if ((r = calloc(1, sizeof(struct resolve))) == NULL) {
		parse_err(p);
		return;
	}

	TAILQ_INSERT_TAIL(&p->cfg->priv->rq, r, entries);
	r->type = RESOLVE_FIELD_STRUCT;
	r->field_struct.result = fd->ref;

	if (parse_next(p) != TOK_IDENT) {
		parse_errx(p, "expected struct source field");
		return;
	}
	if ((r->field_struct.sfield = strdup(p->last.string)) == NULL)
		parse_err(p);
}

/*
 * Allocate and initialise a field "name" in struct "s", returning the
 * new field or NULL on memory allocation failure or bad name.
 */
static struct field *
field_alloc(struct parse *p, struct strct *s, const char *name)
{
	struct field	*fd;
	char		*cp;

	/* Check reserved identifiers. */

	if (!parse_check_badidents(p, name))
		return NULL;

	/* Check other fields in struct having same name. */

	TAILQ_FOREACH(fd, &s->fq, entries) {
		if (strcasecmp(fd->name, name))
			continue;
		parse_errx(p, "duplicate field name: "
			"%s:%zu:%zu", fd->pos.fname, 
			fd->pos.line, fd->pos.column);
		return NULL;
	}

	/* Now the actual allocation. */

	if (NULL == (fd = calloc(1, sizeof(struct field)))) {
		parse_err(p);
		return NULL;
	} else if (NULL == (fd->name = strdup(name))) {
		parse_err(p);
		free(fd);
		return NULL;
	}

	for (cp = fd->name; *cp != '\0'; cp++)
		*cp = (char)tolower((unsigned char)*cp);

	parse_point(p, &fd->pos);
	fd->type = FTYPE_INT;
	fd->parent = s;
	TAILQ_INIT(&fd->fvq);
	TAILQ_INSERT_TAIL(&s->fq, fd, entries);
	return fd;
}


/*
 * Read an individual field declaration with syntax
 *
 *   [:refstruct.reffield] TYPE TYPEINFO
 *
 * By default, fields are integers.  TYPE can be "int", "integer",
 * "text", "txt", etc.
 * A reference clause (:refstruct) triggers a foreign key reference.
 * A "struct" type triggers a local key reference: it must point to a
 * local foreign key field.
 * The TYPEINFO depends upon the type and is processed by
 * parse_field_info(), which must always be run.
 */
void
parse_field(struct parse *p, struct strct *s)
{
	size_t	 	 i;
	struct field	*fd;

	if (parse_next(p) != TOK_IDENT) {
		parse_errx(p, "expected field name");
		return;
	} else if ((fd = field_alloc(p, s, p->last.string)) == NULL)
		return;

	if (parse_next(p) == TOK_SEMICOLON)
		return;

	/* Check if this is a reference. */

	if (p->lasttype == TOK_COLON) {
		parse_field_foreign(p, fd);
		if (parse_next(p) == TOK_SEMICOLON)
			return;
	} 
	
	/* Now we're on to the "type" field. */

	if (p->lasttype != TOK_IDENT) {
		parse_errx(p, "expected field type");
		return;
	}

	for (i = 0; ftypes[i].name != NULL; i++)
		if (strcasecmp(p->last.string, ftypes[i].name) == 0)
			break;

	switch (ftypes[i].type) {
	case FTYPE_BIT:
	case FTYPE_BITFIELD:
	case FTYPE_BLOB:
	case FTYPE_DATE:
	case FTYPE_EMAIL:
	case FTYPE_ENUM:
	case FTYPE_EPOCH:
	case FTYPE_INT:
	case FTYPE_PASSWORD:
	case FTYPE_REAL:
	case FTYPE_STRUCT:
	case FTYPE_TEXT:
		fd->type = ftypes[i].type;
		if (fd->type == FTYPE_BITFIELD)
			parse_field_bits(p, fd);
		else if (fd->type == FTYPE_ENUM)
			parse_field_enum(p, fd);
		else if (fd->type == FTYPE_STRUCT)
			parse_field_struct(p, fd);
		else if (fd->type == FTYPE_BLOB)
			fd->parent->flags |= STRCT_HAS_BLOB;
		parse_field_info(p, fd);
		if ((fd->actup == UPACT_DEFAULT ||
		     fd->actdel == UPACT_DEFAULT) &&
		    !(fd->flags & FIELD_NULL) &&
		    !(fd->flags & FIELD_HASDEF))
			parse_errx(p, "default action without "
				"default value or null");
		if ((fd->actup == UPACT_NULLIFY ||
		     fd->actdel == UPACT_NULLIFY) &&
		    !(fd->flags & FIELD_NULL)) 
			parse_errx(p, "nullify action without "
				"allowing for null");
		return;
	case FTYPE__MAX:
		break;
	}

	parse_errx(p, "unknown field type");
}
