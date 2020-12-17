/*	$Id$ */
/*
 * Copyright (c) 2017, 2020 Kristaps Dzonsons <kristaps@bsd.lv>
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

#include <ctype.h>
#include <inttypes.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "version.h"
#include "ort.h"
#include "ort-lang-c.h"
#include "lang.h"
#include "lang-c.h"

static	const char *const optypes[OPTYPE__MAX] = {
	"equals", /* OPTYPE_EQUAL */
	"greater-than equals", /* OPTYPE_GE */
	"greater-than", /* OPTYPE_GT */
	"less-than equals", /* OPTYPE_LE */
	"less-than", /* OPTYPE_LT */
	"does not equal", /* OPTYPE_NEQUAL */
	"\"like\"", /* OPTYPE_LIKE */
	"logical and", /* OPTYPE_AND */
	"logical or", /* OPTYPE_OR */
	"string equals", /* OPTYPE_STREQ */
	"string does not equal", /* OPTYPE_STRNEQ */
	/* Unary types... */
	"is null", /* OPTYPE_ISNULL */
	"is not null" /* OPTYPE_NOTNULL */
};

/*
 * Emit all characters as uppercase.
 * Return zero on failure, non-zero on success.
 */
static int
gen_upper(FILE *f, const char *cp)
{

	for ( ; *cp != '\0'; cp++)
		if (fputc(toupper((unsigned char)*cp), f) == EOF)
			return 0;
	return 1;
}

/*
 * Generate the structure field and documentation for a given field.
 * Return zero on failure, non-zero on success.
 */
static int
gen_field(FILE *f, const struct field *p)
{
	int	 c = 0;

	if (!gen_comment(f, 1, COMMENT_C, p->doc))
		return 0;

	switch (p->type) {
	case FTYPE_STRUCT:
		c = fprintf(f, "\tstruct %s %s;\n", 
			p->ref->target->parent->name, p->name);
		break;
	case FTYPE_REAL:
		c = fprintf(f, "\tdouble\t %s;\n", p->name);
		break;
	case FTYPE_BLOB:
		c = fprintf(f, "\tvoid\t*%s;\n\tsize_t\t %s_sz;\n",
		       p->name, p->name);
		break;
	case FTYPE_DATE:
	case FTYPE_EPOCH:
		c = fprintf(f, "\ttime_t\t %s;\n", p->name);
		break;
	case FTYPE_BIT:
	case FTYPE_BITFIELD:
	case FTYPE_INT:
		c = fprintf(f, "\tint64_t\t %s;\n", p->name);
		break;
	case FTYPE_TEXT:
	case FTYPE_EMAIL:
	case FTYPE_PASSWORD:
		c = fprintf(f, "\tchar\t*%s;\n", p->name);
		break;
	case FTYPE_ENUM:
		c = fprintf(f, "\tenum %s %s;\n", 
			p->enm->name, p->name);
		break;
	default:
		break;
	}

	return c >= 0;
}

/*
 * Generate user-defined bit-field enumration.
 * These are either the field index (BITI) or the mask (BITF).
 * Return zero on failure, non-zero on success.
 */
static int
gen_bitfield(FILE *f, const struct bitf *b)
{
	const struct bitidx	*bi;
	int64_t			 maxv = -INT64_MAX;
	enum cmtt		 c = COMMENT_C;

	if (b->doc != NULL) {
		if (!gen_comment(f, 0, COMMENT_C_FRAG_OPEN, b->doc))
			return 0;
		c = COMMENT_C_FRAG_CLOSE;
	}

	if (!gen_comment(f, 0, c,
	    "This defines the bit indices for this bit-field.\n"
	    "The BITI fields are the bit indices (0--63) and "
	    "the BITF fields are the masked integer values."))
		return 0;

	if (fprintf(f, "enum\t%s {\n", b->name) < 0)
		return 0;

	TAILQ_FOREACH(bi, &b->bq, entries) {
		if (!gen_comment(f, 1, COMMENT_C, bi->doc))
			return 0;
		if (fputs("\tBITI_", f) == EOF)
			return 0;
		if (!gen_upper(f, b->name))
			return 0;
		if (fprintf(f, "_%s = %" PRId64 ",\n\tBITF_", 
		    bi->name, bi->value) < 0)
			return 0;
		if (!gen_upper(f, b->name))
			return 0;
		if (fprintf(f, "_%s = (1U << %" PRId64 "),\n", 
		    bi->name, bi->value) < 0)
			return 0;
		if (bi->value > maxv)
			maxv = bi->value;
	}

	if (fputs("\tBITI_", f) == EOF)
		return 0;
	if (!gen_upper(f, b->name))
		return 0;
	if (fprintf(f, "__MAX = %" PRId64 ",\n};\n\n", maxv + 1) < 0)
		return 0;

	return 1;
}

/*
 * Generate user-defined enumeration.
 * Return zero on failure, non-zero on success.
 */
static int
gen_enum(FILE *f, const struct enm *e)
{
	const struct eitem *ei;

	if (!gen_comment(f, 0, COMMENT_C, e->doc))
		return 0;

	if (fprintf(f, "enum\t%s {\n", e->name) < 0)
		return 0;

	TAILQ_FOREACH(ei, &e->eq, entries) {
		if (!gen_comment(f, 1, COMMENT_C, ei->doc))
			return 0;
		if (fputc('\t', f) == EOF)
			return 0;
		if (!gen_upper(f, e->name))
			return 0;
		if (fprintf(f, "_%s = %" PRId64 "%s\n", ei->name, 
		    ei->value, TAILQ_NEXT(ei, entries) ? "," : "") < 0)
			return 0;
	}

	return fputs("};\n\n", f) != EOF;
}

/*
 * Generate the C API for a given structure.
 * This generates the TAILQ_ENTRY listing if the structure has any
 * listings declared on it.
 * Return zero on failure, non-zero on success.
 */
static int
gen_struct(FILE *f, const struct config *cfg, const struct strct *p)
{
	const struct field *fd;

	if (!gen_comment(f, 0, COMMENT_C, p->doc))
		return 0;

	if (fprintf(f, "struct\t%s {\n", p->name) < 0)
		return 0;

	TAILQ_FOREACH(fd, &p->fq, entries)
		if (!gen_field(f, fd))
			return 0;

	TAILQ_FOREACH(fd, &p->fq, entries) {
		if (fd->type == FTYPE_STRUCT &&
		    (fd->ref->source->flags & FIELD_NULL)) {
			if (!gen_commentv(f, 1, COMMENT_C,
			    "Non-zero if \"%s\" has been set "
			    "from \"%s\".", fd->name, 
			    fd->ref->source->name))
				return 0;
			if (fprintf(f, "\tint has_%s;\n", fd->name) < 0)
				return 0;
			continue;
		} else if (!(fd->flags & FIELD_NULL))
			continue;

		if (!gen_commentv(f, 1, COMMENT_C,
		    "Non-zero if \"%s\" field is null/unset.",
		    fd->name))
			return 0;
		if (fprintf(f, "\tint has_%s;\n", fd->name) < 0)
			return 0;
	}

	if ((p->flags & STRCT_HAS_QUEUE) &&
	    fprintf(f, "\tTAILQ_ENTRY(%s) _entries;\n", p->name) < 0)
		return 0;

	if (!TAILQ_EMPTY(&cfg->rq)) {
		if (!gen_comment(f, 1, COMMENT_C,
		    "Private data used for role analysis."))
			return 0;
		if (fputs("\tstruct ort_store *priv_store;\n", f) == EOF)
			return 0;
	}

	if (fputs("};\n\n", f) == EOF)
		return 0;

	if (p->flags & STRCT_HAS_QUEUE) {
		if (!gen_commentv(f, 0, COMMENT_C, 
		    "Queue of %s for listings.", p->name))
			return 0;
		if (fprintf(f, "TAILQ_HEAD(%s_q, %s);\n\n", 
		    p->name, p->name) < 0)
			return 0;
	}

	if (p->flags & STRCT_HAS_ITERATOR) {
		if (!gen_commentv(f, 0, COMMENT_C, 
		    "Callback of %s for iteration.\n"
		    "The arg parameter is the opaque pointer "
		    "passed into the iterate function.",
		    p->name))
			return 0;
		if (fprintf(f, "typedef void (*%s_cb)(const struct %s "
		    "*v, void *arg);\n\n", p->name, p->name) < 0)
			return 0;
	}

	return 1;
}

/*
 * Generate update/delete functions for a structure.
 * Returns zero on failure, non-zero on success.
 */
static int
gen_update(FILE *f, const struct config *cfg, const struct update *up)
{
	const struct uref	*ref;
	enum cmtt		 ct = COMMENT_C_FRAG_OPEN;
	size_t			 pos = 1;

	if (up->doc != NULL) {
		if (!gen_comment(f, 0, COMMENT_C_FRAG_OPEN, up->doc))
			return 0;
		ct = COMMENT_C_FRAG;
	}

	/* Only update functions have this part. */

	if (up->type == UP_MODIFY) {
		if (!gen_commentv(f, 0, ct,
		    "Update fields in struct %s.\n"
		    "Updated fields:", up->parent->name))
			return 0;
		TAILQ_FOREACH(ref, &up->mrq, entries)
			if (ref->field->type == FTYPE_PASSWORD)  {
				if (!gen_commentv(f, 0, COMMENT_C_FRAG,
				    "\tv%zu: %s (password)", 
				    pos++, ref->field->name))
					return 0;
			} else {
				if (!gen_commentv(f, 0, COMMENT_C_FRAG,
				    "\tv%zu: %s", 
				    pos++, ref->field->name))
					return 0;
			}
	} else {
		if (!gen_commentv(f, 0, ct, 
		    "Delete fields in struct %s.\n",
		    up->parent->name))
			return 0;
	}

	if (!gen_comment(f, 0, COMMENT_C_FRAG, "Constraint fields:"))
		return 0;

	TAILQ_FOREACH(ref, &up->crq, entries)
		if (ref->op == OPTYPE_NOTNULL) {
			if (!gen_commentv(f, 0, COMMENT_C_FRAG, 
			    "\t%s (not an argument: "
			    "checked not null)", ref->field->name))
				return 0;
		} else if (ref->op == OPTYPE_ISNULL) {
			if (!gen_commentv(f, 0, COMMENT_C_FRAG,
			    "\t%s (not an argument: "
			    "checked null)", ref->field->name))
				return 0;
		} else {
			if (!gen_commentv(f, 0, COMMENT_C_FRAG,
			     "\tv%zu: %s (%s)", pos++, 
			     ref->field->name, optypes[ref->op]))
				return 0;
		}

	if (!gen_comment(f, 0, COMMENT_C_FRAG_CLOSE,
	    "Returns zero on constraint violation, "
	    "non-zero on success."))
		return 0;
	print_func_db_update(up, 1);

	return fputs("", f) != EOF;
}

/*
 * Generate query (list, iterate, etc.) function declaration.
 * Returns zero on failure, non-zero on success.
 */
static int
gen_search(FILE *f, const struct config *cfg, const struct search *s)
{
	const struct sent	*sent;
	const struct strct	*rc;
	size_t			 pos = 1;

	rc = s->dst != NULL ? s->dst->strct : s->parent;

	if (s->doc != NULL) {
		if (!gen_comment(f, 0, COMMENT_C_FRAG_OPEN, s->doc))
			return 0;
	} else if (s->type == STYPE_SEARCH) {
		if (!gen_commentv(f, 0, COMMENT_C_FRAG_OPEN,
		    "Search for a specific %s.", rc->name))
			return 0;
	} else if (s->type == STYPE_LIST) {
		if (!gen_commentv(f, 0, COMMENT_C_FRAG_OPEN,
		    "Search for a set of %s.", rc->name))
			return 0;
	} else if (s->type == STYPE_COUNT) {
		if (!gen_commentv(f, 0, COMMENT_C_FRAG_OPEN,
		    "Count results of a search in %s.", rc->name))
			return 0;
	} else {
		if (!gen_commentv(f, 0, COMMENT_C_FRAG_OPEN,
		    "Iterate over results in %s.", rc->name))
			return 0;
	}

	if (s->dst != NULL) {
		if (!gen_commentv(f, 0, COMMENT_C_FRAG,
		    "This %s distinct query results.",
		    s->type == STYPE_ITERATE ?
		    "iterates over" : 
		    s->type == STYPE_COUNT ? 
		    "counts" : "returns"))
			return 0;
		if (s->dst->strct != s->parent) 
			if (!gen_commentv(f, 0, COMMENT_C_FRAG,
			    "The results are limited "
			    "to the nested structure of \"%s\" "
			    "within %s.", s->dst->fname,
			    s->parent->name))
				return 0;
	}

	if (s->type == STYPE_ITERATE && !gen_comment
	    (f, 0, COMMENT_C_FRAG,
	     "This callback function is called during an "
	     "implicit transaction: thus, it should not "
	     "invoke any database modifications or risk "
	     "deadlock."))
		return 0;

	if ((rc->flags & STRCT_HAS_NULLREFS) && !gen_comment
	    (f, 0, COMMENT_C_FRAG,
	     "This search involves nested null structure "
	     "linking, which involves multiple database "
	     "calls per invocation.\n"
	     "Use this sparingly!"))
		return 0;

	if (!gen_commentv(f, 0, COMMENT_C_FRAG,
	    "Queries on the following fields in struct %s:",
	    s->parent->name))
		return 0;

	TAILQ_FOREACH(sent, &s->sntq, entries)
		if (sent->op == OPTYPE_NOTNULL) {
			if (!gen_commentv(f, 0, COMMENT_C_FRAG,
			    "\t%s (not an argument: "
			    "checked not null)", sent->fname))
				return 0;
		} else if (sent->op == OPTYPE_ISNULL) {
			if (!gen_commentv(f, 0, COMMENT_C_FRAG,
			    "\t%s (not an argument: "
			    "checked is null)", sent->fname))
				return 0;
		} else {
			if (!gen_commentv(f, 0, COMMENT_C_FRAG,
			    "\tv%zu: %s (%s%s)", pos++, 
			    sent->fname, 
			    sent->field->type == FTYPE_PASSWORD ?
			    "pre-hashed password, " : "",
			    optypes[sent->op]))
				return 0;
		}

	if (s->type == STYPE_SEARCH) {
		if (!gen_commentv(f, 0, COMMENT_C_FRAG_CLOSE,
		    "Returns a pointer or NULL on fail.\n"
		    "Free the pointer with db_%s_free().",
		    rc->name))
			return 0;
	} else if (s->type == STYPE_LIST) {
		if (!gen_commentv(f, 0, COMMENT_C_FRAG_CLOSE,
		    "Always returns a queue pointer.\n"
		    "Free this with db_%s_freeq().",
		    rc->name))
			return 0;
	} else if (s->type == STYPE_COUNT) {
		if (!gen_comment(f, 0, COMMENT_C_FRAG_CLOSE,
		    "Returns the count of results."))
			return 0;
	} else {
		if (!gen_comment(f, 0, COMMENT_C_FRAG_CLOSE,
		    "Invokes the given callback with "
		    "retrieved data."))
			return 0;
	}

	print_func_db_search(s, 1);
	return fputs("", f) != EOF;
}

/*
 * Top-level functions for interfacing with the database.
 * Returns zero on failure, non-zero on success.
 */
static int
gen_database(FILE *f, const struct config *cfg, const struct strct *p)
{
	const struct search	*s;
	const struct field	*fd;
	const struct update	*u;
	size_t			 pos;

	if (!gen_comment(f, 0, COMMENT_C,
	    "Clear resources and free \"p\".\n"
	    "Has no effect if \"p\" is NULL."))
		return 0;

	print_func_db_free(p, 1);
	if (fputs("\n", f) == EOF)
		return 0;

	if (STRCT_HAS_QUEUE & p->flags) {
		if (!gen_comment(f, 0, COMMENT_C,
		    "Unfill and free all queue members.\n"
		    "Has no effect if \"q\" is NULL."))
			return 0;
		print_func_db_freeq(p, 1);
		if (fputs("\n", f) == EOF)
			return 0;
	}

	if (p->ins != NULL) {
		if (!gen_comment(f, 0, COMMENT_C_FRAG_OPEN,
		    "Insert a new row into the database.\n"
		    "Only native (and non-rowid) fields may "
		    "be set."))
			return 0;
		pos = 1;
		TAILQ_FOREACH(fd, &p->fq, entries) {
			if (fd->type == FTYPE_STRUCT ||
			    (fd->flags & FIELD_ROWID))
				continue;
			if (fd->type == FTYPE_PASSWORD) {
				if (!gen_commentv(f, 0, COMMENT_C_FRAG,
				    "\tv%zu: %s (pre-hashed password)", 
				    pos++, fd->name))
					return 0;
			} else {
				if (!gen_commentv(f, 0, COMMENT_C_FRAG,
				    "\tv%zu: %s", pos++, fd->name))
					return 0;
			}
		}
		if (!gen_comment(f, 0, COMMENT_C_FRAG_CLOSE,
		    "Returns the new row's identifier on "
		    "success or <0 otherwise."))
			return 0;
		print_func_db_insert(p, 1);
		if (fputs("\n", f) == EOF)
			return 0;
	}

	TAILQ_FOREACH(s, &p->sq, entries)
		if (!gen_search(f, cfg, s))
			return 0;
	TAILQ_FOREACH(u, &p->uq, entries)
		if (!gen_update(f, cfg, u))
			return 0;
	TAILQ_FOREACH(u, &p->dq, entries)
		if (!gen_update(f, cfg, u))
			return 0;

	return 1;
}

/*
 * Emit sections for JSMN parsing of JSON.
 * Return zero on failure, non-zero on success.
 */
static int
gen_json_parse(FILE *f, const struct config *cfg, const struct strct *p)
{

	if (!gen_comment(f, 0, COMMENT_C,
	    "Deserialise the parsed JSON buffer \"buf\", which "
	    "need not be NUL terminated, with parse tokens "
	    "\"t\" of length \"toksz\", into \"p\".\n"
	    "Returns 0 on parse failure, <0 on memory allocation "
	    "failure, or the count of tokens parsed on success."))
		return 0;
	if (!gen_func_json_parse(f, p, 1))
		return 0;
	if (fputs("\n", f) == EOF)
		return 0;

	if (!gen_commentv(f, 0, COMMENT_C,
	    "Deserialise the parsed JSON buffer \"buf\", which "
	    "need not be NUL terminated, with parse tokens "
	    "\"t\" of length \"toksz\", into an array \"p\" "
	    "allocated with \"sz\" elements.\n"
	    "The array must be freed with jsmn_%s_free_array().\n"
	    "Returns 0 on parse failure, <0 on memory allocation "
	    "failure, or the count of tokens parsed on success.",
	    p->name))
		return 0;
	if (!gen_func_json_parse_array(f, p, 1))
		return 0;
	if (fputs("\n", f) == EOF)
		return 0;

	if (!gen_commentv(f, 0, COMMENT_C,
	    "Free an array from jsmn_%s_array(). "
	    "Frees the pointer as well.\n"
	    "May be passed NULL.", p->name))
		return 0;
	if (!gen_func_json_free_array(f, p, 1))
		return 0;
	if (fputs("\n", f) == EOF)
		return 0;

	if (!gen_commentv(f, 0, COMMENT_C,
	    "Clear memory from jsmn_%s(). "
	    "Does not touch the pointer itself.\n"
	    "May be passed NULL.", p->name))
		return 0;
	if (!gen_func_json_clear(f, p, 1))
		return 0;

	return fputs("\n", f) != EOF;
}

/*
 * Emit functions for JSON output via kcgi.
 * Return zero on failure, non-zero on success.
 */
static int
gen_json_out(FILE *f, const struct config *cfg, const struct strct *p)
{

	if (!gen_commentv(f, 0, COMMENT_C,
	    "Print out the fields of a %s in JSON "
	    "including nested structures.\n"
	    "Omits any password entries or those "
	    "marked \"noexport\".\n"
	    "See json_%s_obj() for the full object.",
	    p->name, p->name))
		return 0;
	if (!gen_func_json_data(f, p, 1))
		return 0;
	if (fputs("\n", f) == EOF)
		return 0;

	if (!gen_commentv(f, 0, COMMENT_C,
	    "Emit the JSON key-value pair for the "
	    "object:\n"
	    "\t\"%s\" : { [data]+ }\n"
	    "See json_%s_data() for the data.",
	    p->name, p->name))
		return 0;
	if (!gen_func_json_obj(f, p, 1))
		return 0;
	if (fputs("\n", f) == EOF)
		return 0;

	if (STRCT_HAS_QUEUE & p->flags) {
		if (!gen_commentv(f, 0, COMMENT_C,
		    "Emit the JSON key-value pair for the "
		    "array:\n"
		    "\t\"%s_q\" : [ [{data}]+ ]\n"
		    "See json_%s_data() for the data.",
		    p->name, p->name))
			return 0;
		if (!gen_func_json_array(f, p, 1))
			return 0;
		if (fputs("\n", f) == EOF)
			return 0;
	}

	if (STRCT_HAS_ITERATOR & p->flags) {
		if (!gen_commentv(f, 0, COMMENT_C,
		    "Emit the object as a standalone "
		    "part of (presumably) an array:\n"
		    "\t\"{ data }\n"
		    "See json_%s_data() for the data.\n"
		    "The \"void\" argument is taken "
		    "to be a kjsonreq as if were invoked "
		    "from an iterator.", p->name))
			return 0;
		if (!gen_func_json_iterate(f, p, 1))
			return 0;
		if (fputs("\n", f) == EOF)
			return 0;
	}

	return 1;
}

/*
 * Generate the validation function for all fields in the structure.
 * Return zero on failure, non-zero on success.
 */
static int
gen_valids(FILE *f, const struct config *cfg, const struct strct *p)
{
	const struct field	*fd;

	TAILQ_FOREACH(fd, &p->fq, entries) {
		if (!gen_commentv(f, 0, COMMENT_C,
		    "Validation routines for the %s "
		    "field in struct %s.", fd->name, p->name))
			return 0;
		if (!gen_func_valid(f, fd, 1))
			return 0;
		if (fputs("\n", f) == EOF)
			return 0;
	}

	return 1;
}

/*
 * Generate validation enum names for the structure.
 * Return zero on failure, non-zero on success.
 */
static int
gen_valid_enums(FILE *f, const struct strct *p)
{
	const struct field	*fd;

	TAILQ_FOREACH(fd, &p->fq, entries) {
		if (fd->type == FTYPE_STRUCT)
			continue;
		if (fputs("\tVALID_", f) == EOF)
			return 0;
		if (!gen_upper(f, p->name))
			return 0;
		if (fputc('_', f) == EOF)
			return 0;
		if (!gen_upper(f, fd->name))
			return 0;
		if (fputs(",\n", f) == EOF)
			return 0;
	}

	return 1;
}

/*
 * Generate database transaction functions.
 * Return zero on failure, non-zero on success.
 */
static int
gen_transaction(FILE *f, const struct config *cfg)
{

	if (!gen_comment(f, 0, COMMENT_C,
	    "Open a transaction with identifier \"id\".\n"
	    "If \"mode\" is 0, the transaction is opened in "
	    "\"deferred\" mode, meaning that the database is "
	    "read-locked (no writes allowed) on the first read "
	    "operation, and write-locked on the first write "
	    "(only the current process can write).\n"
	    "If \"mode\" is >0, the transaction immediately "
	    "starts a write-lock.\n"
	    "If \"mode\" is <0, the transaction starts in a "
	    "write-pending, where no other locks can be held "
	    "at the same time.\n"
	    "The DB_TRANS_OPEN_IMMEDIATE, "
	    "DB_TRANS_OPEN_DEFERRED, and "
	    "DB_TRANS_OPEN_EXCLUSIVE macros accomplish the "
	    "same but with the \"mode\" being explicit in the "
	    "name and not needing to be specified."))
		return 0;
	print_func_db_trans_open(1);
	if (fputs("\n", f) == EOF)
		return 0;

	if (fputs("#define DB_TRANS_OPEN_IMMEDIATE(_ctx, _id) \\\n"
		  "\tdb_trans_open((_ctx), (_id), 1)\n"
		  "#define DB_TRANS_OPEN_DEFERRED(_ctx, _id)\\\n"
		  "\tdb_trans_open((_ctx), (_id), 0)\n"
		  "#define DB_TRANS_OPEN_EXCLUSIVE(_ctx, _id)\\\n"
		  "\tdb_trans_open((_ctx), (_id), -1)\n\n", f) == EOF)
		return 0;

	if (!gen_comment(f, 0, COMMENT_C,
	    "Roll-back an open transaction."))
		return 0;
	print_func_db_trans_rollback(1);
	if (fputs("\n", f) == EOF)
		return 0;

	if (!gen_comment(f, 0, COMMENT_C,
	    "Commit an open transaction."))
		return 0;
	print_func_db_trans_commit(1);
	return fputs("\n", f) != EOF;
}

/*
 * Generate open and logging open (and auxiliary) functions.
 * Return zero on failure, non-zero on success.
 */
static int
gen_open(FILE *f, const struct config *cfg)
{

	if (!gen_comment(f, 0, COMMENT_C,
	    "Forward declaration of opaque pointer."))
		return 0;
	if (fputs("struct ort;\n\n", f) == EOF)
		return 0;

	if (!gen_comment(f, 0, COMMENT_C,
	    "Set the argument given to the logging function "
	    "specified to db_open_logging().\n"
	    "Has no effect if no logging function has been "
	    "set.\n"
	    "The buffer is copied into a child process, so "
	    "serialised objects may not have any pointers "
	    "in the current address space or they will fail "
	    "(at best).\n"
	    "Set length to zero to unset the logging function "
	    "callback argument."))
		return 0;
	print_func_db_set_logging(1);
	if (fputs("\n", f) == EOF)
		return 0;

	if (!gen_comment(f, 0, COMMENT_C,
	    "Allocate and open the database in \"file\".\n"
	    "Returns an opaque pointer or NULL on "
	    "memory exhaustion.\n"
	    "The returned pointer must be closed with "
	    "db_close().\n"
	    "See db_open_logging() for the equivalent "
	    "function that accepts logging callbacks.\n"
	    "This function starts a child with fork(), "
	    "the child of which opens the database, so "
	    "a constraint environment (e.g., with pledge) "
	    "must take this into account.\n"
	    "Subsequent this function, all database "
	    "operations take place over IPC."))
		return 0;
	print_func_db_open(1);
	if (fputs("\n", f) == EOF)
		return 0;

	if (!gen_comment(f, 0, COMMENT_C,
	    "Like db_open() but accepts a function for "
	    "logging.\n"
	    "If both are provided, the \"long\" form overrides "
	    "the \"short\" form.\n"
	    "The logging function is run both in a child "
	    "and parent process, so it must not have side "
	    "effects.\n"
	    "The optional pointer is passed to the long "
	    "form logging function and is inherited by the "
	    "child process as-is, without being copied "
	    "by value.\n"
	    "See db_logging_data() to set the pointer "
	    "after initialisation."))
		return 0;
	print_func_db_open_logging(1);
	return fputs("\n", f) != EOF;
}

/*
 * Generate auxiliary role functions.
 * Return zero on failure, non-zero on success.
 */
static int
gen_roles(FILE *f, const struct config *cfg)
{

	if (!gen_comment(f, 0, COMMENT_C,
	    "Drop into a new role.\n"
	    "If the role is the same as the current one, "
	    "this is a noop.\n"
	    "We can only refine roles (i.e., descend the "
	    "role tree), not ascend or move laterally.\n"
	    "Attempting to do so causes abort(2) to be "
	    "called.\n"
	    "The only exceptions are when leaving ROLE_default "
	    "or when entering ROLE_none."))
		return 0;
	print_func_db_role(1);
	if (fputs("\n", f) == EOF)
		return 0;

	if (!gen_comment(f, 0, COMMENT_C, "Get the current role."))
		return 0;
	print_func_db_role_current(1);
	if (fputs("\n", f) == EOF)
		return 0;

	if (!gen_comment(f, 0, COMMENT_C,
	    "Get the role stored into \"s\".\n"
	    "This role is set when the object containing the "
	    "stored role is created, such as when a \"search\" "
	    "query function is called."))
		return 0;
	print_func_db_role_stored(1);
	return fputs("\n", f) != EOF;
}

/*
 * Generate the database close function.
 * Return zero on failure, non-zero on success.
 */
static int
gen_close(FILE *f, const struct config *cfg)
{

	if (!gen_comment(f, 0, COMMENT_C,
	    "Close the context opened by db_open().\n"
	    "Has no effect if \"p\" is NULL."))
		return 0;
	print_func_db_close(1);
	return fputs("\n", f) != EOF;
}

/*
 * Generate "r" as ROLE_xxx, where "xxx" is the lowercased name of the
 * role.  Don't print out anything for the "all" role.
 * Return zero on failure, non-zero on success.
 */
static int
gen_role(FILE *f, const struct role *r, int *nf)
{

	if (strcmp(r->name, "all") == 0)
		return 1;

	if (*nf) {
		if (fputc(',', f) == EOF)
			return 0;
	} else
		*nf = 1;

	if (strcmp(r->name, "default") == 0) {
		if (!gen_comment(f, 1, COMMENT_C,
		    "The default role.\n"
		    "This is assigned when db_open() is called.\n"
		    "It should be limited only to those "
		    "functions required to narrow the role."))
			return 0;
	} else if (strcmp(r->name, "none") == 0) {
		if (!gen_comment(f, 1, COMMENT_C,
		    "Role that isn't allowed to do anything."))
			return 0;
	}

	return fprintf(f, "\tROLE_%s", r->name) >= 0;
}

int
ort_lang_c_header(const struct ort_lang_c *args,
	const struct config *cfg, FILE *f)
{
	const struct strct	*p;
	const struct enm	*e;
	const struct role	*r;
	const struct bitf	*bf;
	int			 i = 0;

	/* If the guard is NULL, we don't emit any guarding. */

	if (args->guard != NULL && fprintf(f, 
	    "#ifndef %s\n#define %s\n\n", args->guard, args->guard) < 0)
		return 0;

	if (!gen_commentv(f, 0, COMMENT_C, 
	    "WARNING: automatically generated by %s %s.\n"
	    "DO NOT EDIT!", __func__, VERSION))
		return 0;
	if (fputc('\n', f) == EOF)
		return 0;

	if (fprintf(f, "#ifndef KWBP_VERSION\n"
            "# define KWBP_VERSION \"%s\"\n"
            "#endif\n"
            "#ifndef KWBP_VSTAMP\n"
            "# define KWBP_VSTAMP %lld\n"
            "#endif\n\n", VERSION, (long long)VSTAMP) < 0)
		return 0;
	
	if ((args->flags & ORT_LANG_C_DB_SQLBOX) && 
	    !TAILQ_EMPTY(&cfg->rq)) {
		if (!gen_comment(f, 0, COMMENT_C,
		    "Our roles for access control.\n"
		    "When the database is first opened, "
		    "the system is set to ROLE_default.\n"
		    "Roles may then be set using the "
		    "ort_role() function."))
			return 0;
		if (fputs("enum\tort_role {\n", f) == EOF)
			return 0;
		TAILQ_FOREACH(r, &cfg->arq, allentries)
			if (!gen_role(f, r, &i))
				return 0;
		if (fputs("\n};\n\n", f) == EOF)
			return 0;
	}

	if (args->flags & ORT_LANG_C_CORE) {
		TAILQ_FOREACH(e, &cfg->eq, entries)
			if (!gen_enum(f, e))
				return 0;
		TAILQ_FOREACH(bf, &cfg->bq, entries)
			if (!gen_bitfield(f, bf))
				return 0;
		TAILQ_FOREACH(p, &cfg->sq, entries)
			if (!gen_struct(f, cfg, p))
				return 0;
	}

	if (args->flags & ORT_LANG_C_VALID_KCGI) {
		if (!gen_comment(f, 0, COMMENT_C,
		    "All of the fields we validate.\n"
		    "These are as VALID_XXX_YYY, where XXX is "
		    "the structure and YYY is the field.\n"
		    "Only native types are listed."))
			return 0;
		if (fputs("enum\tvalid_keys {\n", f) == EOF)
			return 0;
		TAILQ_FOREACH(p, &cfg->sq, entries)
			if (!gen_valid_enums(f, p))
				return 0;
		if (fputs("\tVALID__MAX\n};\n\n", f) == EOF)
			return 0;
		if (!gen_comment(f, 0, COMMENT_C,
		    "Validation fields.\n"
		    "Pass this directly into khttp_parse(3) "
		    "to use them as-is.\n"
		    "The functions are \"valid_xxx_yyy\", "
		    "where \"xxx\" is the struct and \"yyy\" "
		    "the field, and can be used standalone.\n"
		    "The form inputs are named \"xxx-yyy\"."))
			return 0;
		if (fputs("extern const struct kvalid "
			   "valid_keys[VALID__MAX];\n\n", f) == EOF)
			return 0;
	}

	if (args->flags & ORT_LANG_C_JSON_JSMN) {
		if (!gen_comment(f, 0, COMMENT_C,
		    "Possible error returns from jsmn_parse(), "
		    "if returning a <0 error code."))
			return 0;
		if (fputs("enum jsmnerr_t {\n"
		          "\tJSMN_ERROR_NOMEM = -1,\n"
		          "\tJSMN_ERROR_INVAL = -2,\n"
		          "\tJSMN_ERROR_PART = -3\n"
		          "};\n\n", f) == EOF)
			return 0;
		if (!gen_comment(f, 0, COMMENT_C, 
		    "Type of JSON token"))
			return 0;
		if (fputs("typedef enum {\n"
		          "\tJSMN_UNDEFINED = 0,\n"
		          "\tJSMN_OBJECT = 1,\n"
		          "\tJSMN_ARRAY = 2,\n"
		          "\tJSMN_STRING = 3,\n"
		          "\tJSMN_PRIMITIVE = 4\n"
		          "} jsmntype_t;\n\n", f) == EOF)
			return 0;
		if (!gen_comment(f, 0, COMMENT_C,
		    "JSON token description."))
			return 0;
		if (fputs("typedef struct {\n"
		          "\tjsmntype_t type;\n"
		          "\tint start;\n"
		          "\tint end;\n"
		          "\tint size;\n"
		          "} jsmntok_t;\n\n", f) == EOF)
			return 0;
		if (!gen_comment(f, 0, COMMENT_C,
		    "JSON parser. Contains an array of token "
		    "blocks available. Also stores the string "
		    "being parsed now and current position in "
		    "that string."))
			return 0;
		if (fputs("typedef struct {\n"
		          "\tunsigned int pos;\n"
		          "\tunsigned int toknext;\n"
		          "\tint toksuper;\n"
		          "} jsmn_parser;\n\n", f) == EOF)
			return 0;
	}

	if (fputs("__BEGIN_DECLS\n\n", f) == EOF)
		return 0;

	if (args->flags & ORT_LANG_C_DB_SQLBOX) {
		if (!gen_open(f, cfg))
			return 0;
		if (!gen_transaction(f, cfg))
			return 0;
		if (!gen_close(f, cfg))
			return 0;
		if (!TAILQ_EMPTY(&cfg->rq))
			if (!gen_roles(f, cfg))
				return 0;
		TAILQ_FOREACH(p, &cfg->sq, entries)
			if (!gen_database(f, cfg, p))
				return 0;
	}

	if (args->flags & ORT_LANG_C_JSON_KCGI)
		TAILQ_FOREACH(p, &cfg->sq, entries)
			if (!gen_json_out(f, cfg, p))
				return 0;
	if (args->flags & ORT_LANG_C_JSON_JSMN) {
		if (!gen_comment(f, 0, COMMENT_C,
		    "Check whether the current token in a "
		    "JSON parse sequence \"tok\" parsed from "
		    "\"json\" is equal to a string.\n"
		    "Usually used when checking for key "
		    "equality.\n"
		    "Returns non-zero on equality, zero "
		    "otherwise."))
			return 0;
		if (fputs("int jsmn_eq(const char *json,\n"
		          "\tconst jsmntok_t *tok, "
			  "const char *s);\n\n", f) == EOF)
			return 0;
		if (!gen_comment(f, 0, COMMENT_C,
		    "Initialise a JSON parser sequence \"p\"."))
			return 0;
		if (fputs("void jsmn_init"
			  "(jsmn_parser *p);\n\n", f) == EOF)
			return 0;
		if (!gen_comment(f, 0, COMMENT_C,
		    "Parse a buffer \"buf\" of length \"sz\" "
		    "into tokens \"toks\" of length \"toksz\" "
		    "with parser \"p\".\n"
		    "Returns the number of tokens parsed or "
		    "<0 on failure (possible errors described "
		    "in enum jsmnerr_t).\n"
		    "If passed NULL \"toks\", simply computes "
		    "the number of tokens required."))
			return 0;
		if (fputs("int jsmn_parse(jsmn_parser *p, "
			  "const char *buf,\n"
			  "\tsize_t sz, jsmntok_t *toks, "
		          "unsigned int toksz);\n\n", f) == EOF)
			return 0;
		TAILQ_FOREACH(p, &cfg->sq, entries)
			if (!gen_json_parse(f, cfg, p))
				return 0;
	}
	if (args->flags & ORT_LANG_C_VALID_KCGI)
		TAILQ_FOREACH(p, &cfg->sq, entries)
			if (!gen_valids(f, cfg, p))
				return 0;

	if (fputs("__END_DECLS\n", f) == EOF)
		return 0;

	if (args->guard != NULL &&
	    fputs("\n#endif\n", f) == EOF)
		return 0;

	return 1;
}

