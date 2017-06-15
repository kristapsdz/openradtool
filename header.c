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
#include "config.h"

#include <sys/queue.h>

#include <ctype.h>
#if HAVE_ERR
# include <err.h>
#endif
#include <stdio.h>
#include <stdlib.h>

#include "extern.h"

/*
 * Generate the C API for a given field.
 */
static void
gen_strct_field(const struct field *p)
{

	if (NULL != p->doc)
		print_commentt(1, COMMENT_C, p->doc);

	switch (p->type) {
	case (FTYPE_STRUCT):
		printf("\tstruct %s %s;\n", 
			p->ref->tstrct, p->name);
		break;
	case (FTYPE_REAL):
		printf("\tdouble\t %s;\n", p->name);
		break;
	case (FTYPE_BLOB):
		printf("\tvoid\t*%s;\n"
		       "\tsize_t\t %s_sz;\n",
		       p->name, p->name);
		break;
	case (FTYPE_EPOCH):
		printf("\ttime_t\t %s;\n", p->name);
		break;
	case (FTYPE_INT):
		printf("\tint64_t\t %s;\n", p->name);
		break;
	case (FTYPE_TEXT):
		/* FALLTHROUGH */
	case (FTYPE_EMAIL):
		/* FALLTHROUGH */
	case (FTYPE_PASSWORD):
		printf("\tchar\t*%s;\n", p->name);
		break;
	case (FTYPE_ENUM):
		printf("\tenum %s %s;\n", 
			p->eref->ename, p->name);
		break;
	default:
		break;
	}
}

/*
 * Generate the C API for a given structure.
 * This generates the TAILQ_ENTRY listing if the structure has any
 * listings declared on it.
 */
static void
gen_struct(const struct strct *p)
{
	const struct field *f;

	if (NULL != p->doc)
		print_commentt(0, COMMENT_C, p->doc);

	printf("struct\t%s {\n", p->name);

	TAILQ_FOREACH(f, &p->fq, entries)
		gen_strct_field(f);
	TAILQ_FOREACH(f, &p->fq, entries) {
		if ( ! (FIELD_NULL & f->flags))
			continue;
		print_commentv(1, COMMENT_C,
			"Non-zero if \"%s\" field is null/unset.",
			f->name);
		printf("\tint has_%s;\n", f->name);
	}

	if (STRCT_HAS_QUEUE & p->flags)
		printf("\tTAILQ_ENTRY(%s) _entries;\n", p->name);
	puts("};\n"
	     "");

	if (STRCT_HAS_QUEUE & p->flags) {
		print_commentv(0, COMMENT_C, 
			"Queue of %s for listings.", p->name);
		printf("TAILQ_HEAD(%s_q, %s);\n\n", p->name, p->name);
	}

	if (STRCT_HAS_ITERATOR & p->flags) {
		print_commentv(0, COMMENT_C, 
			"Callback of %s for iteration.\n"
			"The arg parameter is the opaque pointer "
			"passed into the iterate function.",
			p->name);
		printf("typedef void (*%s_cb)"
		       "(const struct %s *v, void *arg);\n\n", 
		       p->name, p->name);
	}
}

/*
 * Generate update/delete functions for a structure.
 */
static void
gen_func_update(const struct update *up)
{
	const struct uref *ref;
	enum cmtt	 ct = COMMENT_C_FRAG_OPEN;
	size_t		 pos;

	if (NULL != up->doc) {
		print_commentt(0, COMMENT_C_FRAG_OPEN, up->doc);
		print_commentt(0, COMMENT_C_FRAG, "");
		ct = COMMENT_C_FRAG;
	}

	/* Only update functions have this part. */

	if (UP_MODIFY == up->type) {
		print_commentv(0, ct,
			"Updates the given fields in struct %s:",
			up->parent->name);
		pos = 1;
		TAILQ_FOREACH(ref, &up->mrq, entries)
			if (FTYPE_PASSWORD == ref->field->type) 
				print_commentv(0, COMMENT_C_FRAG,
					"\tv%zu: %s (password)", 
					pos++, ref->name);
			else
				print_commentv(0, COMMENT_C_FRAG,
					"\tv%zu: %s", 
					pos++, ref->name);
		print_commentt(0, COMMENT_C_FRAG,
			"Constrains the updated records to:");
	} else
		print_commentt(0, ct,
			"Constrains the deleted records to:");

	pos = 1;
	TAILQ_FOREACH(ref, &up->crq, entries)
		if (OPTYPE_NOTNULL == ref->op) 
			print_commentv(0, COMMENT_C_FRAG,
				"\t%s (not null)", ref->name);
		else if (OPTYPE_ISNULL == ref->op) 
			print_commentv(0, COMMENT_C_FRAG,
				"\t%s (is null)", ref->name);
		else
			print_commentv(0, COMMENT_C_FRAG,
				"\tv%zu: %s", pos++, ref->name);

	print_commentt(0, COMMENT_C_FRAG_CLOSE,
		"Returns zero on failure, non-zero on "
		"constraint errors.");
	print_func_db_update(up, 1);
	puts("");
}

/*
 * Generate a custom search function declaration.
 */
static void
gen_func_search(const struct search *s)
{
	const struct sent *sent;
	const struct sref *sr;
	size_t	 pos = 1;

	if (NULL != s->doc)
		print_commentt(0, COMMENT_C_FRAG_OPEN, s->doc);
	else if (STYPE_SEARCH == s->type)
		print_commentv(0, COMMENT_C_FRAG_OPEN,
			"Search for a specific %s.", 
			s->parent->name);
	else
		print_commentv(0, COMMENT_C_FRAG_OPEN,
			"Search for a set of %s.", 
			s->parent->name);

	print_commentv(0, COMMENT_C_FRAG,
		"\nUses the given fields in struct %s:",
	       s->parent->name);

	TAILQ_FOREACH(sent, &s->sntq, entries) {
		sr = TAILQ_LAST(&sent->srq, srefq);
		if (OPTYPE_NOTNULL == sent->op)
			print_commentv(0, COMMENT_C_FRAG,
				"\t%s (not null)", sent->fname);
		else if (OPTYPE_ISNULL == sent->op)
			print_commentv(0, COMMENT_C_FRAG,
				"\t%s (is null)", sent->fname);
		else if (FTYPE_PASSWORD == sr->field->type)
			print_commentv(0, COMMENT_C_FRAG,
				"\tv%zu: %s (pre-hashed password)", 
				pos++, sent->fname);
		else
			print_commentv(0, COMMENT_C_FRAG,
				"\tv%zu: %s", pos++, 
				sent->fname);
	}

	if (STYPE_SEARCH == s->type)
		print_commentv(0, COMMENT_C_FRAG_CLOSE,
			"Returns a pointer or NULL on fail.\n"
			"Free the pointer with db_%s_free().",
			s->parent->name);
	else if (STYPE_LIST == s->type)
		print_commentv(0, COMMENT_C_FRAG_CLOSE,
			"Always returns a queue pointer.\n"
			"Free this with db_%s_freeq().",
			s->parent->name);
	else
		print_commentv(0, COMMENT_C_FRAG_CLOSE,
			"Invokes the given callback with "
			"retrieved data.");

	print_func_db_search(s, 1);
	puts("");
}

/*
 * Generate the function declarations for a given structure.
 */
static void
gen_funcs(const struct strct *p, int json, int valids)
{
	const struct search *s;
	const struct field *f;
	const struct update *u;
	size_t	 pos;

	print_commentt(0, COMMENT_C,
	       "Unfill resources and free \"p\".\n"
	       "Has no effect if \"p\" is NULL.");
	print_func_db_free(p, 1);
	puts("");

	if (STRCT_HAS_QUEUE & p->flags) {
		print_commentv(0, COMMENT_C,
		     "Unfill and free all queue members.\n"
		     "Has no effect if \"q\" is NULL.");
		print_func_db_freeq(p, 1);
		puts("");
	}

	print_commentv(0, COMMENT_C, 
	       "Fill in a %s from an open statement \"stmt\".\n"
	       "This starts grabbing results from \"pos\", "
	       "which may be NULL to start from zero.\n"
	       "This follows DB_SCHEMA_%s's order for columns.",
	       p->name, p->cname);
	print_func_db_fill(p, 1);
	puts("");

	print_commentt(0, COMMENT_C_FRAG_OPEN,
		"Insert a new row into the database.\n"
		"Only native (and non-rowid) fields may "
		"be set.");
	pos = 1;
	TAILQ_FOREACH(f, &p->fq, entries) {
		if (FTYPE_STRUCT == f->type ||
		    FIELD_ROWID & f->flags)
			continue;
		if (FTYPE_PASSWORD == f->type) 
			print_commentv(0, COMMENT_C_FRAG,
				"\tv%zu: %s (pre-hashed password)", 
				pos++, f->name);
		else
			print_commentv(0, COMMENT_C_FRAG,
				"\tv%zu: %s", 
				pos++, f->name);
	}
	print_commentt(0, COMMENT_C_FRAG_CLOSE,
		"Returns the new row's identifier on "
		"success or <0 otherwise.");
	print_func_db_insert(p, 1);
	puts("");

	print_commentv(0, COMMENT_C,
	       "Free memory allocated by db_%s_fill().\n"
	       "Has not effect if \"p\" is NULL.",
	       p->name);
	print_func_db_unfill(p, 1);
	puts("");

	TAILQ_FOREACH(s, &p->sq, entries)
		gen_func_search(s);
	TAILQ_FOREACH(u, &p->uq, entries)
		gen_func_update(u);
	TAILQ_FOREACH(u, &p->dq, entries)
		gen_func_update(u);

	if (json) {
		print_commentv(0, COMMENT_C,
			"Print out the fields of a %s in JSON "
			"including nested structures.\n"
			"Omits any password entries or those "
			"marked \"noexport\".\n"
			"See json_%s_obj() for the full object.",
			p->name, p->name);
		print_func_json_data(p, 1);
		puts("");
		print_commentv(0, COMMENT_C,
			"Emit the JSON key-value pair for the "
			"object:\n"
			"\t\"%s\" : { [data]+ }\n"
			"See json_%s_data() for the data.",
			p->name, p->name);
		print_func_json_obj(p, 1);
		puts("");
		if (STRCT_HAS_QUEUE & p->flags) {
			print_commentv(0, COMMENT_C,
				"Emit the JSON key-value pair for the "
				"array:\n"
				"\t\"%s_q\" : [ [{data}]+ ]\n"
				"See json_%s_data() for the data.",
				p->name, p->name);
			print_func_json_array(p, 1);
			puts("");
		}
		if (STRCT_HAS_ITERATOR & p->flags) {
			print_commentv(0, COMMENT_C,
				"Emit the object as a standalone "
				"part of (presumably) an array:\n"
				"\t\"{ data }\n"
				"See json_%s_data() for the data.\n"
				"The \"void\" argument is taken "
				"to be a kjsonreq as if were invoked "
				"from an iterator.", p->name);
			print_func_json_iterate(p, 1);
			puts("");
		}
	}

	if (valids) {
		TAILQ_FOREACH(f, &p->fq, entries) {
			print_commentv(0, COMMENT_C,
				"Validation routines for the %s "
				"field in struct %s.", 
				f->name, p->name);
			print_func_valid(f, 1);
			puts("");
		}
	}
}

/*
 * Generate the schema for a given table.
 * This macro accepts a single parameter that's given to all of the
 * members so that a later SELECT can use INNER JOIN xxx AS yyy and have
 * multiple joins on the same table.
 */
static void
gen_schema(const struct strct *p)
{
	const struct field *f;

	printf("#define DB_SCHEMA_%s(_x) \\\n", p->cname);
	TAILQ_FOREACH(f, &p->fq, entries) {
		if (FTYPE_STRUCT == f->type)
			continue;
		printf("\t#_x \".%s\"", f->name);
		if (TAILQ_NEXT(f, entries))
			puts(" \",\" \\");
	}
	puts("");
}

/*
 * List valid keys for all native fields of a structure.
 */
static void
gen_valid_enums(const struct strct *p)
{
	const struct field *f;
	const char	*cp;

	TAILQ_FOREACH(f, &p->fq, entries) {
		if (FTYPE_STRUCT == f->type)
			continue;
		printf("\tVALID_");
		for (cp = p->name; '\0' != *cp; cp++)
			putchar(toupper((int)*cp));
		putchar('_');
		for (cp = f->name; '\0' != *cp; cp++)
			putchar(toupper((int)*cp));
		puts(",");
	}
}

/*
 * Generate the C header file.
 * If "json" is non-zero, this generates the JSON formatters.
 * If "valids" is non-zero, this generates the field validators.
 */
void
gen_c_header(const struct config *cfg, int json, int valids)
{
	const struct strct *p;

	puts("#ifndef DB_H\n"
	     "#define DB_H\n"
	     "");
	print_commentt(0, COMMENT_C, 
	       "WARNING: automatically generated by "
	       "kwebapp " VERSION ".\n"
	       "DO NOT EDIT!");
	puts("");

	TAILQ_FOREACH(p, &cfg->sq, entries)
		gen_struct(p);

	print_commentt(0, COMMENT_C,
		"Define our table columns.\n"
		"Use these when creating your own SQL statements,\n"
		"combined with the db_xxxx_fill functions.\n"
		"Each macro must be given a unique alias name.\n"
		"This allows for doing multiple inner joins on the "
		"same table.");
	TAILQ_FOREACH(p, &cfg->sq, entries)
		gen_schema(p);

	if (valids) {
		puts("");
		print_commentt(0, COMMENT_C,
			"All of the fields we validate.\n"
			"These are as VALID_XXX_YYY, where XXX is "
			"the structure and YYY is the field.\n"
			"Only native types are listed.");
		puts("enum\tvalid_keys {");
		TAILQ_FOREACH(p, &cfg->sq, entries)
			gen_valid_enums(p);
		puts("\tVALID__MAX");
		puts("};\n"
		     "");
		print_commentt(0, COMMENT_C,
			"Validation fields.\n"
			"Pass this directly into khttp_parse(3) "
			"to use them as-is.\n"
			"The functions are \"valid_xxx_yyy\", "
			"where \"xxx\" is the struct and \"yyy\" "
			"the field, and can be used standalone.\n"
			"The form inputs are named \"xxx-yyy\".");
		puts("extern const struct kvalid "
		      "valid_keys[VALID__MAX];");
	}

	puts("\n"
	     "__BEGIN_DECLS\n"
	     "");

	print_commentt(0, COMMENT_C,
		"Allocate and open the database in \"file\".\n"
		"This returns a pointer to the database "
		"in \"safe exit\" mode (see ksql(3)).\n"
		"It returns NULL on memory allocation failure.\n"
		"The returned pointer must be closed with "
		"db_close().");
	print_func_db_open(1);
	puts("");

	print_commentt(0, COMMENT_C,
		"Close the database opened by db_open().\n"
		"Has no effect if \"p\" is NULL.");
	print_func_db_close(1);
	puts("");

	TAILQ_FOREACH(p, &cfg->sq, entries)
		gen_funcs(p, json, valids);

	puts("__END_DECLS\n"
	     "\n"
	     "#endif");
}
