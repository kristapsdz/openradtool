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

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ort.h"
#include "extern.h"
#include "linker.h"

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
		case RESOLVE_UP_CONSTRAINT:
			fail += !resolve_up_const
				(cfg, &r->struct_up_const);
			break;
		case RESOLVE_UP_MODIFIER:
			fail += !resolve_up_mod
				(cfg, &r->struct_up_mod);
			break;
		default:
			abort();
		}

	/* These depend upon prior resolutions. */

	TAILQ_FOREACH(r, &cfg->priv->rq, entries)
		switch (r->type) {
		case RESOLVE_FIELD_STRUCT:
			fail += !resolve_field_struct
				(cfg, &r->field_struct);
			break;
		default:
			break;
		}

	return fail == 0;
}
