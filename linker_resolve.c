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
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ort.h"
#include "extern.h"
#include "linker.h"

/*
 * Resolve a reference chain to a single field: a "names" consisting of
 * "foo" "bar" "bar" would have been input as "foo.bar.baz" and resolves
 * to the field "baz" wherever that is.
 * Each non-terminal field must be a "struct" and not null.
 * Returns the resolved field (of any type) or NULL if not found.
 */
static struct field *
resolve_field_chain(struct config *cfg, const struct pos *pos,
	struct strct *s, const char **names, size_t namesz)
{
	size_t	 	 i;
	struct field	*f = NULL;

	for (i = 0; i < namesz; i++) {
		TAILQ_FOREACH(f, &s->fq, entries)
			if (strcasecmp(f->name, names[i]) == 0)
				break;
		if (f == NULL) {
			gen_errx(cfg, pos, "field "
				"not found: %s", names[i]);
			return NULL;
		}

		/* Terminal fields handled by caller. */

		if (i == namesz - 1)
			continue;

		/* Non-terminal must be non-null and a struct. */

		if (f->type != FTYPE_STRUCT) {
			gen_errx(cfg, pos, "non-terminal field must "
				"be a struct: %s", f->name);
			return NULL;
		}
		assert(f->ref->source != NULL);
		if (f->ref->source->flags & FIELD_NULL) {
			gen_errx(cfg, pos, "non-terminal field "
				"cannot be null: %s", f->name);
			return NULL;
		}

		assert(f->ref->target != NULL);
		s = f->ref->target->parent;
	}

	assert(f != NULL);
	return f;
}

static int
resolve_struct_order(struct config *cfg, struct struct_order *r)
{

	r->result->field = resolve_field_chain
		(cfg, &r->result->pos, r->result->parent->parent, 
		 (const char **)r->names, r->namesz);

	if (r->result->field != NULL &&
	    r->result->field->type == FTYPE_STRUCT) {
		gen_errx(cfg, &r->result->pos, 
			"terminal field cannot be a struct: %s",
			r->result->field->name);
		r->result->field = NULL;
	}

	return r->result->field != NULL;
}

static int
resolve_struct_aggr(struct config *cfg, struct struct_aggr *r)
{

	r->result->field = resolve_field_chain
		(cfg, &r->result->pos, r->result->parent->parent, 
		 (const char **)r->names, r->namesz);

	if (r->result->field != NULL &&
	    r->result->field->type == FTYPE_STRUCT) {
		gen_errx(cfg, &r->result->pos, 
			"terminal field cannot be a struct: %s",
			r->result->field->name);
		r->result->field = NULL;
	}

	return r->result->field != NULL;
}

static int
resolve_struct_grouprow(struct config *cfg, struct struct_grouprow *r)
{

	r->result->field = resolve_field_chain
		(cfg, &r->result->pos, r->result->parent->parent, 
		 (const char **)r->names, r->namesz);

	if (r->result->field != NULL &&
	    r->result->field->type == FTYPE_STRUCT) {
		gen_errx(cfg, &r->result->pos, 
			"terminal field cannot be a struct: %s",
			r->result->field->name);
		r->result->field = NULL;
	}

	return r->result->field != NULL;
}

static int
resolve_struct_sent(struct config *cfg, struct struct_sent *r)
{

	r->result->field = resolve_field_chain
		(cfg, &r->result->pos, r->result->parent->parent, 
		 (const char **)r->names, r->namesz);

	if (r->result->field != NULL &&
	    r->result->field->type == FTYPE_STRUCT) {
		gen_errx(cfg, &r->result->pos, 
			"terminal field cannot be a struct: %s",
			r->result->field->name);
		r->result->field = NULL;
	}

	return r->result->field != NULL;
}

static int
resolve_struct_distinct(struct config *cfg, struct struct_distinct *r)
{
	struct field	*f;

	f = resolve_field_chain
		(cfg, &r->result->pos, r->result->parent->parent, 
		 (const char **)r->names, r->namesz);

	if (f == NULL)
		return 0;

	if (f->type != FTYPE_STRUCT) {
		gen_errx(cfg, &r->result->pos, 
			"terminal field must be a struct: %s",
			f->name);
		return 0;
	} else if (f->ref->source->flags & FIELD_NULL) {
		gen_errx(cfg, &r->result->pos, 
			"terminal field cannot be null: %s",
			f->name);
		return 0;
	}

	r->result->strct = f->ref->target->parent;
	return 1;
}

/*
 * Look up the constraint part of an "update" or "delete" statement
 * (e.g., delete ->foo<-" and make sure that the field allows for it.
 */
static int
resolve_up_const(struct config *cfg, struct struct_up_const *r)
{
	struct field	*f;
	size_t		 errs = 0;

	TAILQ_FOREACH(f, &r->result->parent->parent->fq, entries)
		if (strcasecmp(f->name, r->name) == 0) {
			r->result->field = f;
			break;
		}

	if (r->result->field == NULL) {
		gen_errx(cfg, &r->result->pos, 
			"constraint field not found");
		errs++;
	} 
	
	if (r->result->field != NULL &&
	    r->result->field->type == FTYPE_STRUCT) {
		gen_errx(cfg, &r->result->pos, 
			"constraint field may not be a struct");
		errs++;
	}

	/*
	 * FIXME: this needs to allow for notnull or isnull constraint
	 * checks on passwords, which are fine.
	 */

	if (r->result->field != NULL &&
	    r->result->field->type == FTYPE_PASSWORD &&
	    r->result->op != OPTYPE_STREQ &&
	    r->result->op != OPTYPE_STRNEQ &&
	    !OPTYPE_ISUNARY(r->result->op)) {
		gen_errx(cfg, &r->result->pos, 
			"constraint field may not be a "
			"password in hashing mode");
		errs++;
	}
	
	/* Warning: isnot or notnull on non-null fields. */

	if (r->result->field != NULL &&
	    (r->result->op == OPTYPE_NOTNULL ||
	     r->result->op == OPTYPE_ISNULL) &&
	    !(r->result->field->flags & FIELD_NULL))
		gen_warnx(cfg, &r->result->pos, 
			"notnull or isnull operator "
			"on field that's never null");

	/* 
	 * "like" operator needs text.
	 * FIXME: useful for binary as well?
	 */

	if (r->result->field != NULL &&
	    r->result->op == OPTYPE_LIKE &&
	    r->result->field->type != FTYPE_TEXT &&
	    r->result->field->type != FTYPE_EMAIL) {
		gen_errx(cfg, &r->result->pos, 
			"LIKE operator on non-textual field.");
		errs++;
	}

	return errs == 0;
}

/*
 * Look up the modifier part of an "update" statement (e.g., update
 * ->foo<-: id" and make sure that the field allows for it.
 */
static int
resolve_up_mod(struct config *cfg, struct struct_up_mod *r)
{
	struct field	*f;
	size_t		 errs = 0;

	TAILQ_FOREACH(f, &r->result->parent->parent->fq, entries)
		if (strcasecmp(f->name, r->name) == 0) {
			r->result->field = f;
			break;
		}

	if (r->result->field == NULL) {
		gen_errx(cfg, &r->result->pos, 
			"modifier field not found");
		errs++;
	} 
	
	if (r->result->field != NULL &&
	    r->result->field->type == FTYPE_STRUCT) {
		gen_errx(cfg, &r->result->pos, 
			"modifier field may not be a struct");
		errs++;
	}

	/* Check that our mod type is appropriate to the field. */

	if (r->result->field != NULL)
		switch (r->result->mod) {
		case MODTYPE_CONCAT:
			if (r->result->field->type == FTYPE_BLOB ||
			    r->result->field->type == FTYPE_TEXT ||
			    r->result->field->type == FTYPE_EMAIL)
				break;
			gen_errx(cfg, &r->result->pos, "concatenate "
				"modification on non-textual and "
				"non-binary field");
			errs++;
			break;
		case MODTYPE_SET:
		case MODTYPE_STRSET:
			/* Can be done with anything. */
			break;
		case MODTYPE_INC:
		case MODTYPE_DEC:
			if (r->result->field->type == FTYPE_BIT ||
			    r->result->field->type == FTYPE_BITFIELD ||
			    r->result->field->type == FTYPE_DATE ||
			    r->result->field->type == FTYPE_ENUM ||
			    r->result->field->type == FTYPE_EPOCH ||
			    r->result->field->type == FTYPE_INT ||
			    r->result->field->type == FTYPE_REAL)
				break;
			gen_errx(cfg, &r->result->pos, "increment or "
				"decrement modification on non-"
				"numeric field");
			errs++;
			break;
		default:
			abort();
		}

	return errs == 0;
}

static int
resolve_struct_unique(struct config *cfg, struct struct_unique *r)
{
	struct field	*f;
	struct nref	*nf;

	TAILQ_FOREACH(f, &r->result->parent->parent->fq, entries) {
		if (strcasecmp(f->name, r->name) != 0)
			continue;
		if (f->type != FTYPE_STRUCT)
			break;
		gen_errx(cfg, &r->result->pos, "unique field "
			"may not be a struct: %s", f->name);
		return 0;
	}

	if (f == NULL) {
		gen_errx(cfg, &r->result->pos, "unknown field");
		return 0;
	}

	/* Disallow duplicates. */

	TAILQ_FOREACH(nf, &r->result->parent->nq, entries)
		if (f == nf->field) {
			gen_errx(cfg, &r->result->pos, 
				"duplicate field: %s", f->name);
			return 0;
		}

	r->result->field = f;

	return 1;
}

/*
 * Look up the enum type by its name.
 */
static int
resolve_field_enum(struct config *cfg, struct field_enum *r)
{
	struct enm	*e;

	/* Straightforward scan. */

	TAILQ_FOREACH(e, &cfg->eq, entries)
		if (strcasecmp(e->name, r->name) == 0) {
			r->result->enm = e;
			return 1;
		}

	gen_errx(cfg, &r->result->parent->pos, "unknown enum type");
	return 0;
}

/*
 * Recursively look up "name" in the queue "rq" and all of its
 * descendents.
 * Returns the role or NULL if none were found.
 */
static struct role *
role_lookup(struct roleq *rq, const char *name)
{
	struct role	*r, *res;

	TAILQ_FOREACH(r, rq, entries)
		if (strcasecmp(r->name, name) == 0)
			return r;
		else if ((res = role_lookup(&r->subrq, name)) != NULL)
			return res;

	return NULL;
}

static int
resolve_struct_role(struct config *cfg, struct struct_role *r)
{

	if ((r->result->role = role_lookup(&cfg->rq, r->name)) == NULL)
		gen_errx(cfg, &r->result->pos, "unknown role");
	return r->result->role != NULL;
}

static void
resolve_struct_rolemap_all(struct config *cfg, struct struct_rolemap *r)
{

	assert(r->result->parent->arolemap == NULL);
	r->result->parent->arolemap = r->result;
}

static int
resolve_struct_rolemap_insert(struct config *cfg, struct struct_rolemap *r)
{

	if (r->result->parent->ins == NULL) 
		return 0;
	assert(r->result->parent->ins->rolemap == NULL);
	r->result->parent->ins->rolemap = r->result;
	return 1;
}

static int
resolve_struct_rolemap_update(struct config *cfg, struct struct_rolemap *r)
{
	struct updateq	*q = NULL;
	struct update	*u;

	if (r->type == ROLEMAP_DELETE)
		q = &r->result->parent->dq;
	else if (r->type == ROLEMAP_UPDATE)
		q = &r->result->parent->uq;

	assert(q != NULL);
	TAILQ_FOREACH(u, q, entries) 
		if (u->name != NULL &&
		    strcasecmp(u->name, r->name) == 0) {
			assert(u->rolemap == NULL);
			u->rolemap = r->result;
			return 1;
		}

	return 0;
}

static int
resolve_struct_rolemap_query(struct config *cfg, struct struct_rolemap *r)
{
	enum stype	 type = STYPE__MAX;
	struct search	*s;

	if (r->type == ROLEMAP_SEARCH)
		type = STYPE_SEARCH;
	else if (r->type == ROLEMAP_ITERATE)
		type = STYPE_ITERATE;
	else if (r->type == ROLEMAP_LIST)
		type = STYPE_LIST;
	else if (r->type == ROLEMAP_COUNT)
		type = STYPE_COUNT;

	assert(type != STYPE__MAX);

	TAILQ_FOREACH(s, &r->result->parent->sq, entries)
		if (type == s->type && 
		    s->name != NULL &&
		    strcasecmp(s->name, r->name) == 0) {
			assert(s->rolemap == NULL);
			s->rolemap = r->result;
			return 1;
		}

	return 0;
}

/*
 * Apply the noexport roles of rolemap "r" to the field "f".
 * It is not an error for the field to already have the role specified
 * for it: it's just skipped.
 * Returns zero on allocation failure, non-zero on success.
 */
static int
resolve_struct_rolemap_field(struct config *cfg,
	struct struct_rolemap *r, struct field *f)
{
	struct rref	*rdst, *rsrc;

	if (f->rolemap == NULL) {
		f->rolemap = r->result;
		return 1;
	}

	TAILQ_FOREACH(rsrc, &r->result->rq, entries) {
		TAILQ_FOREACH(rdst, &f->rolemap->rq, entries)
			if (rdst->role == rsrc->role)
				break;

		/* Already specified! */

		if (rdst != NULL)
			return 1;

		/* Source doesn't exist in destination. */

		rdst = calloc(1, sizeof(struct rref));
		if (rdst == NULL) {
			gen_err(cfg, &rsrc->pos);
			return 0;
		}
		TAILQ_INSERT_TAIL(&f->rolemap->rq, rdst, entries);
		rdst->parent = f->rolemap;
		rdst->pos = rsrc->pos;
		rdst->role = rsrc->role;
	}

	return 1;
}

/*
 * Noexport can handle named fields and all-fields.
 * Returns -1 on allocation failure, 0 if the field was not found
 * (obviously only for named fields), 1 on success (applied to all
 * fields).
 */
static int
resolve_struct_rolemap_noexport(struct config *cfg,
	struct struct_rolemap *r)
{
	struct field	*f;

	/* Start by applying noexport to all fields. */

	if (r->name == NULL) {
		TAILQ_FOREACH(f, &r->result->parent->fq, entries) {
			if (!resolve_struct_rolemap_field(cfg, r, f))
				return -1;
		}
		return 1;
	}

	TAILQ_FOREACH(f, &r->result->parent->fq, entries)
		if (strcasecmp(f->name, r->name) == 0) {
			if (!resolve_struct_rolemap_field(cfg, r, f))
				return -1;
			return 1;
		}

	gen_errx(cfg, &r->result->parent->pos,
		"field not found: %s", r->name);
	return 0;
}

static int
resolve_struct_rolemap_post_cover(struct config *cfg,
	struct rolemap *dst, const struct rolemap *src)
{
	const struct rref	*srcr;
	struct rref		*dstr;

	TAILQ_FOREACH(srcr, &src->rq, entries) {
		TAILQ_FOREACH(dstr, &dst->rq, entries)
			if (dstr->role == srcr->role)
				break;
		if (dstr != NULL)
			continue;
		if ((dstr = calloc(1, sizeof(struct rref))) == NULL) {
			gen_err(cfg, &srcr->pos);
			return 0;
		}
		TAILQ_INSERT_TAIL(&dst->rq, dstr, entries);
		dstr->role = srcr->role;
		dstr->pos = srcr->pos;
		dstr->parent = dst;
	}

	return 1;
}

/*
 * For "all" rolemaps, add the assigned roles to all possible operations
 * except for "noexport".
 * Returns zero on allocation failure, non-zero on success.
 */
static int
resolve_struct_rolemap_post(struct config *cfg, struct struct_rolemap *r)
{
	struct strct	*p;
	struct update	*u;
	struct search	*s;

	if (r->type != ROLEMAP_ALL)
		return 1;

	p = r->result->parent;
	assert(p->arolemap != NULL);

	TAILQ_FOREACH(u, &p->dq, entries) {
		if (u->rolemap == NULL) {
			u->rolemap = p->arolemap;
			continue;
		}
		if (!resolve_struct_rolemap_post_cover
		    (cfg, u->rolemap, p->arolemap))
			return 0;
	}
	TAILQ_FOREACH(u, &p->uq, entries) {
		if (u->rolemap == NULL) {
			u->rolemap = p->arolemap;
			continue;
		}
		if (!resolve_struct_rolemap_post_cover
		    (cfg, u->rolemap, p->arolemap))
			return 0;
	}

	TAILQ_FOREACH(s, &p->sq, entries) {
		if (s->rolemap == NULL) {
			s->rolemap = p->arolemap;
			continue;
		}
		if (!resolve_struct_rolemap_post_cover
		    (cfg, s->rolemap, p->arolemap))
			return 0;
	}

	if (p->ins != NULL && p->ins->rolemap == NULL) {
		p->ins->rolemap = p->arolemap;
	} else if (p->ins != NULL) {
		if (!resolve_struct_rolemap_post_cover
		    (cfg, p->ins->rolemap, p->arolemap))
			return 0;
	}
	
	return 1;
}

/*
 * Resolve the operation in a role-map.
 * Some operations are named; others (like "insert") aren't.
 * This could just as easily have a resolver type for each rolemap type,
 * but it boils down to the same thing.
 * Returns -1 on allocation failure, 0 on failure, 1 on success.
 */
static int
resolve_struct_rolemap(struct config *cfg, struct struct_rolemap *r)
{
	int	 rc;

	switch (r->type) {
	case ROLEMAP_ALL:
		resolve_struct_rolemap_all(cfg, r);
		return 1;
	case ROLEMAP_DELETE:
	case ROLEMAP_UPDATE:
		if (resolve_struct_rolemap_update(cfg, r))
			return 1;
		gen_errx(cfg, &r->result->parent->pos,
			"%s operation not found: %s",
			r->type == ROLEMAP_DELETE ?
			"delete" : "update", r->name);
		break;
	case ROLEMAP_INSERT:
		if (resolve_struct_rolemap_insert(cfg, r))
			return 1;
		gen_errx(cfg, &r->result->parent->pos,
			"insert operation not specified");
		break;
	case ROLEMAP_COUNT:
	case ROLEMAP_ITERATE:
	case ROLEMAP_LIST:
	case ROLEMAP_SEARCH:
		if (resolve_struct_rolemap_query(cfg, r))
			return 1;
		gen_errx(cfg, &r->result->parent->pos,
			"%s operation not found: %s", 
			r->type == ROLEMAP_COUNT ? "count" : 
			r->type == ROLEMAP_ITERATE ? "iterate" : 
			r->type == ROLEMAP_LIST ? "list" : 
			"search", r->name);
		break;
	case ROLEMAP_NOEXPORT:
		rc = resolve_struct_rolemap_noexport(cfg, r);
		if (rc != 0)
			return rc;
		break;
	default:
		abort();
	}

	return 0;
}

/*
 * Look up the bitfield type by its name.
 */
static int
resolve_field_bits(struct config *cfg, struct field_bits *r)
{
	struct bitf	*b;

	/* Straightforward scan. */

	TAILQ_FOREACH(b, &cfg->bq, entries)
		if (strcasecmp(b->name, r->name) == 0) {
			r->result->bitf = b;
			return 1;
		}

	gen_errx(cfg, &r->result->parent->pos, "unknown bitfield type");
	return 0;
}

/*
 * The local key refers to another field that should be a foreign
 * reference resolved in resolve_field_foreign().
 * So this must be run *after* all RESOLVE_FIELD_FOREIGN or else it
 * won't be able to find the target.
 */
static int
resolve_field_struct(struct config *cfg, struct field_struct *r)
{
	struct field	*f;
	size_t		 errs = 0;
	struct fieldq	*fq;

	/* Look up the source on our structure. */

	fq = &r->result->parent->parent->fq;

	TAILQ_FOREACH(f, fq, entries) {
		if (strcasecmp(f->name, r->sfield))
			continue;
		r->result->source = f;
		break;
	}

	/* 
	 * Assign the target of the source, which must be a reference
	 * (e.g., "field foo:bar.baz").
	 * These would already be resolved by resolve_field_foreign()
	 * unless there were errors.
	 */

	if (r->result->source != NULL &&
	    (r->result->source->type == FTYPE_STRUCT ||
	     r->result->source->ref == NULL)) {
		gen_errx(cfg, &r->result->parent->pos, 
			"struct source is not a reference");
		errs++;
	} else if (r->result->source != NULL)
		r->result->target = r->result->source->ref->target;
	    
	/* Are the source and target defined? */

	if (r->result->source == NULL) {
		gen_errx(cfg, &r->result->parent->pos, 
			"unknown struct source");
		errs++;
	}
	if (r->result->target == NULL) {
		gen_errx(cfg, &r->result->parent->pos, 
			"struct source's reference was not resolved");
		errs++;
	}

	return errs == 0;
}

/*
 * Resolve a foreign-key reference "field x:y.z".
 * This looks up both "x" (local) and "y.z" (foreign).
 */
static int
resolve_field_foreign(struct config *cfg, struct field_foreign *r)
{
	struct strct	*p;
	struct field	*f;
	size_t		 errs = 0;

	/* Look up the target on all structures. */

	TAILQ_FOREACH(p, &cfg->sq, entries) {
		if (strcasecmp(p->name, r->tstrct))
			continue;
		TAILQ_FOREACH(f, &p->fq, entries) {
			if (strcasecmp(f->name, r->tfield))
				continue;
			r->result->target = f;
			break;
		}
	}

	/* Are the source and target defined? */

	if (r->result->source == NULL) {
		gen_errx(cfg, &r->result->parent->pos, 
			"unknown reference source");
		errs++;
	}
	if (r->result->target == NULL) {
		gen_errx(cfg, &r->result->parent->pos, 
			"unknown reference target");
		errs++;
	}

	/* Do they have the same type? */

	if (r->result->source != NULL && r->result->target != NULL &&
	    r->result->source->type != r->result->target->type) {
		gen_errx(cfg, &r->result->parent->pos, 
			"source and target reference type mismatch");
		errs++;
	}

	/* Is the reference on a unique row? */

	if (r->result->target != NULL &&
	    !(r->result->target->flags & FIELD_ROWID) &&
	    !(r->result->target->flags & FIELD_UNIQUE)) {
		gen_errx(cfg, &r->result->parent->pos, 
			"target reference not a rowid or unique");
		errs++;
	}

	return errs == 0;
}

int
linker_resolve(struct config *cfg)
{
	struct resolve	*r;
	size_t		 fail = 0;
	int		 rc;

	TAILQ_FOREACH(r, &cfg->priv->rq, entries)
		switch (r->type) {
		case RESOLVE_FIELD_STRUCT:
			/*
			 * Skip this: we run it after
			 * RESOLVE_FIELD_FOREIGN because we're going to
			 * reference the resolved target of that.
			 */
			break;
		case RESOLVE_FIELD_FOREIGN:
			fail += !resolve_field_foreign
				(cfg, &r->field_foreign);
			break;
		case RESOLVE_FIELD_BITS:
			fail += !resolve_field_bits
				(cfg, &r->field_bits);
			break;
		case RESOLVE_FIELD_ENUM:
			fail += !resolve_field_enum
				(cfg, &r->field_enum);
			break;
		case RESOLVE_AGGR:
		case RESOLVE_DISTINCT:
		case RESOLVE_GROUPROW:
		case RESOLVE_ORDER:
		case RESOLVE_SENT:
			/*
			 * These require both RESOLVE_FIELD_FOREIGN and
			 * the later RESOLVE_FIELD_STRUCT.
			 */
			break;
		case RESOLVE_ROLE:
			fail += !resolve_struct_role
				(cfg, &r->struct_role);
			break;
		case RESOLVE_ROLEMAP:
			/* This requires RESOLVE_ROLE. */
			break;
		case RESOLVE_UNIQUE:
			fail += !resolve_struct_unique
				(cfg, &r->struct_unique);
			break;
		case RESOLVE_UP_CONSTRAINT:
			fail += !resolve_up_const
				(cfg, &r->struct_up_const);
			break;
		case RESOLVE_UP_MODIFIER:
			fail += !resolve_up_mod
				(cfg, &r->struct_up_mod);
			break;
		}

	if (fail > 0)
		return 0;

	/* These depend upon prior resolutions. */

	TAILQ_FOREACH(r, &cfg->priv->rq, entries)
		switch (r->type) {
		case RESOLVE_FIELD_STRUCT:
			fail += !resolve_field_struct
				(cfg, &r->field_struct);
			break;
		case RESOLVE_ROLEMAP:
			rc = resolve_struct_rolemap
				(cfg, &r->struct_rolemap);
			if (rc < 0)
				return 0;
			fail += !rc;
			break;
		default:
			break;
		}

	if (fail > 0)
		return 0;

	/* Lastly, these depend on full struct target links. */

	TAILQ_FOREACH(r, &cfg->priv->rq, entries)
		switch (r->type) {
		case RESOLVE_AGGR:
			fail += !resolve_struct_aggr
				(cfg, &r->struct_aggr);
			break;
		case RESOLVE_DISTINCT:
			fail += !resolve_struct_distinct
				(cfg, &r->struct_distinct);
			break;
		case RESOLVE_GROUPROW:
			fail += !resolve_struct_grouprow
				(cfg, &r->struct_grouprow);
			break;
		case RESOLVE_ORDER:
			fail += !resolve_struct_order
				(cfg, &r->struct_order);
			break;
		case RESOLVE_ROLEMAP:
			if (!resolve_struct_rolemap_post
			    (cfg, &r->struct_rolemap))
				return 0;
			break;
		case RESOLVE_SENT:
			fail += !resolve_struct_sent
				(cfg, &r->struct_sent);
			break;
		default:
			break;
		}

	return fail == 0;
}
