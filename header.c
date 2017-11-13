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
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "extern.h"

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
	"is null", /* OPTYPE_ISNULL */
	"is not null" /* OPTYPE_NOTNULL */
};

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
	case (FTYPE_BIT):
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

static void
gen_bitfield(const struct bitf *b)
{
	const struct bitidx *bi;

	print_commentt(0, COMMENT_C_FRAG_OPEN, b->doc);
	print_commentt(0, COMMENT_C_FRAG_CLOSE,
		"This defines the bit indices for this bit-field.\n"
		"The BITI fields are the bit indices (0--63) and "
		"the BITF fields are the masked integer values.");

	printf("enum\t%s {\n", b->name);
	TAILQ_FOREACH(bi, &b->bq, entries) {
		if (NULL != bi->doc)
			print_commentt(1, COMMENT_C, bi->doc);
		printf("\tBITI_%s_%s = %" PRId64 ",\n",
			b->cname, bi->name, bi->value);
		printf("\tBITF_%s_%s = (1U << %" PRId64 ")%s\n",
			b->cname, bi->name, bi->value, 
			TAILQ_NEXT(bi, entries) ? "," : "");
	}
	puts("};\n"
	     "");

}

/*
 * Generate our enumerations.
 */
static void
gen_enum(const struct enm *e)
{
	const struct eitem *ei;

	if (NULL != e->doc)
		print_commentt(0, COMMENT_C, e->doc);
	printf("enum\t%s {\n", e->name);
	TAILQ_FOREACH(ei, &e->eq, entries) {
		if (NULL != ei->doc)
			print_commentt(1, COMMENT_C, ei->doc);
		printf("\t%s_%s = %" PRId64 "%s\n",
			e->cname, ei->name, ei->value, 
			TAILQ_NEXT(ei, entries) ? "," : "");
	}
	puts("};\n"
	     "");
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
		if (FTYPE_STRUCT == f->type &&
		    FIELD_NULL & f->ref->source->flags) {
			print_commentv(1, COMMENT_C,
				"Non-zero if \"%s\" has been set "
				"from \"%s\".", f->name, 
				f->ref->source->name);
			printf("\tint has_%s;\n", f->name);
			continue;
		} else if ( ! (FIELD_NULL & f->flags))
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
gen_func_update(const struct config *cfg, const struct update *up)
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
	} else {
		pos = 1;
		print_commentt(0, ct,
			"Constrains the deleted records to:");
	}

	TAILQ_FOREACH(ref, &up->crq, entries)
		if (OPTYPE_NOTNULL == ref->op) 
			print_commentv(0, COMMENT_C_FRAG,
				"\t%s (not an argument: "
				"checked not null)", ref->name);
		else if (OPTYPE_ISNULL == ref->op) 
			print_commentv(0, COMMENT_C_FRAG,
				"\t%s (not an argument: "
				"checked null)", ref->name);
		else
			print_commentv(0, COMMENT_C_FRAG,
				"\tv%zu: %s (%s)", pos++, 
				ref->name, optypes[ref->op]);

	print_commentt(0, COMMENT_C_FRAG_CLOSE,
		"Returns zero on failure, non-zero on "
		"constraint errors.");
	print_func_db_update(up, CFG_HAS_ROLES & cfg->flags, 1);
	puts("");
}

/*
 * Generate a custom search function declaration.
 */
static void
gen_func_search(const struct config *cfg, const struct search *s)
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

	if (STYPE_ITERATE == s->type)
		print_commentt(0, COMMENT_C_FRAG,
			"This callback function is called during an "
			"implicit transaction: thus, it should not "
			"invoke any database modifications or risk "
			"deadlock.");

	if (STRCT_HAS_NULLREFS & s->parent->flags) 
		print_commentt(0, COMMENT_C_FRAG,
			"This search involves nested null structure "
			"linking, which involves multiple database "
			"calls per invocation.\n"
			"Use this sparingly!");
	print_commentv(0, COMMENT_C_FRAG,
		"Uses the given fields in struct %s:",
		s->parent->name);

	TAILQ_FOREACH(sent, &s->sntq, entries) {
		sr = TAILQ_LAST(&sent->srq, srefq);
		if (OPTYPE_NOTNULL == sent->op)
			print_commentv(0, COMMENT_C_FRAG,
				"\t%s (not an argument: "
				"checked not null)", sent->fname);
		else if (OPTYPE_ISNULL == sent->op)
			print_commentv(0, COMMENT_C_FRAG,
				"\t%s (not an argument: "
				"checked is null)", sent->fname);
		else if (FTYPE_PASSWORD == sr->field->type)
			print_commentv(0, COMMENT_C_FRAG,
				"\tv%zu: %s (pre-hashed password)", 
				pos++, sent->fname);
		else
			print_commentv(0, COMMENT_C_FRAG,
				"\tv%zu: %s (%s)", pos++, 
				sent->fname, optypes[sent->op]);
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

	print_func_db_search(s, CFG_HAS_ROLES & cfg->flags, 1);
	puts("");
}

/*
 * Generate the function declarations for a given structure.
 */
static void
gen_funcs(const struct config *cfg,
	const struct strct *p, int json, int valids)
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

	if (STRCT_HAS_INSERT & p->flags) {
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
		print_func_db_insert(p, CFG_HAS_ROLES & cfg->flags, 1);
		puts("");
	}

	print_commentv(0, COMMENT_C,
	       "Free memory allocated by db_%s_fill().\n"
	       "Has not effect if \"p\" is NULL.",
	       p->name);
	print_func_db_unfill(p, 1);
	puts("");

	TAILQ_FOREACH(s, &p->sq, entries)
		gen_func_search(cfg, s);
	TAILQ_FOREACH(u, &p->uq, entries)
		gen_func_update(cfg, u);
	TAILQ_FOREACH(u, &p->dq, entries)
		gen_func_update(cfg, u);

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
 * We need to handle several different facets here: if the
 * system is operating in RBAC mode and whether the database is
 * being opened in split-process mode.
 * This changes the documentation for db_open, but we keep the
 * documentation for db_close to be symmetric.
 */
static void
gen_func_open(const struct config *cfg, int splitproc)
{

	print_commentt(0, COMMENT_C_FRAG_OPEN,
		"Allocate and open the database in \"file\". "
		"This opens the database in \"safe exit\" mode "
		"(see ksql(3)).");
	if (splitproc)
		print_commentt(0, COMMENT_C_FRAG,
			"Note: the database has been opened in a "
			"child process, so the application may be "
			"sandboxed liberally.");
	else
		print_commentt(0, COMMENT_C_FRAG,
			"Note: if you're using a sandbox, you must "
			"accommodate for the SQLite database within "
			"process memory.");
	if (CFG_HAS_ROLES & cfg->flags)
		print_commentt(0, COMMENT_C_FRAG,
			"Returns an opaque pointer or NULL on "
			"memory exhaustion.");
	else
		print_commentt(0, COMMENT_C_FRAG,
			"Returns a pointer to the database or "
			"NULL on memory exhaustion.");
	print_commentt(0, COMMENT_C_FRAG_CLOSE,
		"The returned pointer must be closed with "
		"db_close().");

	print_func_db_open(CFG_HAS_ROLES & cfg->flags, 1);
	puts("");
}

static void
gen_func_roles(const struct config *cfg)
{

	print_commentt(0, COMMENT_C,
		"Drop into a new role.\n"
		"If the role is the same as the current one, "
		"this is a noop.\n"
		"We can only refine roles (i.e., descend the "
		"role tree), not ascend or move laterally.\n"
		"Attempting to do so causes abort(2) to be "
		"called.\n"
		"The only exceptions are when leaving ROLE_default "
		"or when entering ROLE_none.");
	print_func_db_role(1);
	puts("");
}


static void
gen_func_close(const struct config *cfg)
{

	print_commentt(0, COMMENT_C,
		"Close the context opened by db_open().\n"
		"Has no effect if \"p\" is NULL.");
	print_func_db_close(CFG_HAS_ROLES & cfg->flags, 1);
	puts("");
}

/*
 * Recursively list all of our roles as ROLE_xxx, where "xxx" is the
 * lowercased name of the role.
 * Don't print out anything for the "all" role, which may never be
 * entered per se.
 */
static void
gen_role(const struct role *r, int *nf)
{
	const struct role *rr;
	const char *cp;
	int	    suppress = 0;


	if (strcasecmp(r->name, "all"))  {
		if (*nf)
			puts(",");
		else
			*nf = 1;
	} else
		suppress = 1;

	if (0 == strcasecmp(r->name, "default"))
		print_commentt(1, COMMENT_C,
			"The default role.\n"
			"This is assigned when db_open() is called.\n"
			"It should be limited only to those "
			"functions required to narrow the role.");
	else if (0 == strcasecmp(r->name, "none"))
		print_commentt(1, COMMENT_C,
			"Role that isn't allowed to do anything.");

	if ( ! suppress) {
		printf("\tROLE_");
		for (cp = r->name; '\0' != *cp; cp++)
			putchar(tolower((unsigned char)*cp));
	}
	TAILQ_FOREACH(rr, &r->subrq, entries)
		gen_role(rr, nf);
}

/*
 * Generate the C header file.
 * If "json" is non-zero, this generates the JSON formatters.
 * If "valids" is non-zero, this generates the field validators.
 * For "splitproc", note that db_open uses ksql_alloc_child.
 */
void
gen_c_header(const struct config *cfg, 
	int json, int valids, int splitproc)
{
	const struct strct *p;
	const struct enm *e;
	const struct role *r;
	const struct bitf *bf;
	int	 i = 0;

	puts("#ifndef DB_H\n"
	     "#define DB_H\n"
	     "");
	print_commentt(0, COMMENT_C, 
	       "WARNING: automatically generated by "
	       "kwebapp " VERSION ".\n"
	       "DO NOT EDIT!");
	puts("");

	TAILQ_FOREACH(e, &cfg->eq, entries)
		gen_enum(e);
	TAILQ_FOREACH(bf, &cfg->bq, entries)
		gen_bitfield(bf);

	if ( ! TAILQ_EMPTY(&cfg->rq)) {
		print_commentt(0, COMMENT_C,
			"Our roles for access control.\n"
			"When the database is first opened, "
			"the system is set to ROLE_default.\n"
			"Roles may then be set using the "
			"kwbp_role() function.");
		puts("enum\tkwbp_role {");
		TAILQ_FOREACH(r, &cfg->rq, entries)
			gen_role(r, &i);
		puts("\n"
		     "};\n"
		     "");
	}

	TAILQ_FOREACH(p, &cfg->sq, entries)
		gen_struct(p);

	print_commentt(0, COMMENT_C,
		"Define our table columns.\n"
		"Use these when creating your own SQL statements, "
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

	gen_func_open(cfg, splitproc);
	gen_func_close(cfg);
	if (CFG_HAS_ROLES & cfg->flags)
		gen_func_roles(cfg);

	TAILQ_FOREACH(p, &cfg->sq, entries)
		gen_funcs(cfg, p, json, valids);

	puts("__END_DECLS\n"
	     "\n"
	     "#endif");
}
