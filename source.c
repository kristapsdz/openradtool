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
#include "config.h"

#if HAVE_SYS_QUEUE
# include <sys/queue.h>
#endif
# include <sys/param.h>

#include <assert.h>
#include <ctype.h>
#if HAVE_ERR
# include <err.h>
#endif
#include <fcntl.h>
#include <inttypes.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "version.h"
#include "paths.h"
#include "ort.h"
#include "extern.h"

enum	external {
	EX_GENSALT, /* gensalt.c */
	EX_B64_NTOP, /* b64_ntop.c */
	EX_JSMN, /* jsmn.c */
	EX__MAX
};

static	const char *const externals[EX__MAX] = {
	FILE_GENSALT, /* EX_GENSALT */
	FILE_B64_NTOP, /* EX_B64_NTOP */
	FILE_JSMN /* EX_JSMN */
};

/*
 * SQL operators.
 * Some of these binary, some of these are unary.
 * Use the OPTYPE_ISUNARY or OPTYPE_ISBINARY macro to determine where
 * within the expressions this should sit.
 */
static	const char *const optypes[OPTYPE__MAX] = {
	"=", /* OPTYPE_EQUAL */
	">=", /* OPTYPE_GE */
	">", /* OPTYPE_GT */
	"<=", /* OPTYPE_LE */
	"<", /* OPTYPE_LT */
	"!=", /* OPTYPE_NEQUAL */
	"LIKE", /* OPTYPE_LIKE */
	"&", /* OPTYPE_AND */
	"|", /* OPTYPE_OR */
	"=", /* OPTYPE_STREQ */
	"!=", /* OPTYPE_STRNEQ */
	/* Unary types... */
	"ISNULL", /* OPTYPE_ISNULL */
	"NOTNULL", /* OPTYPE_NOTNULL */
};

/*
 * Functions extracting from a statement.
 * Note that FTYPE_TEXT and FTYPE_PASSWORD need a surrounding strdup.
 */
static	const char *const coltypes[FTYPE__MAX] = {
	"sqlbox_parm_int", /* FTYPE_BIT */
	"sqlbox_parm_int", /* FTYPE_DATE */
	"sqlbox_parm_int", /* FTYPE_EPOCH */
	"sqlbox_parm_int", /* FTYPE_INT */
	"sqlbox_parm_float", /* FTYPE_REAL */
	"sqlbox_parm_blob_alloc", /* FTYPE_BLOB (XXX: is special) */
	"sqlbox_parm_string_alloc", /* FTYPE_TEXT */
	"sqlbox_parm_string_alloc", /* FTYPE_PASSWORD */
	"sqlbox_parm_string_alloc", /* FTYPE_EMAIL */
	NULL, /* FTYPE_STRUCT */
	"sqlbox_parm_int", /* FTYPE_ENUM */
	"sqlbox_parm_int", /* FTYPE_BITFIELD */
};

static	const char *const puttypes[FTYPE__MAX] = {
	"kjson_putintp", /* FTYPE_BIT */
	"kjson_putintp", /* FTYPE_DATE */
	"kjson_putintp", /* FTYPE_EPOCH */
	"kjson_putintp", /* FTYPE_INT */
	"kjson_putdoublep", /* FTYPE_REAL */
	"kjson_putstringp", /* FTYPE_BLOB (XXX: is special) */
	"kjson_putstringp", /* FTYPE_TEXT */
	NULL, /* FTYPE_PASSWORD (don't print) */
	"kjson_putstringp", /* FTYPE_EMAIL */
	NULL, /* FTYPE_STRUCT */
	"kjson_putintp", /* FTYPE_ENUM */
	"kjson_putintp", /* FTYPE_BITFIELD */
};

static	const char *const bindtypes[FTYPE__MAX] = {
	"SQLBOX_PARM_INT", /* FTYPE_BIT */
	"SQLBOX_PARM_INT", /* FTYPE_DATE */
	"SQLBOX_PARM_INT", /* FTYPE_EPOCH */
	"SQLBOX_PARM_INT", /* FTYPE_INT */
	"SQLBOX_PARM_FLOAT", /* FTYPE_REAL */
	"SQLBOX_PARM_BLOB", /* FTYPE_BLOB (XXX: is special) */
	"SQLBOX_PARM_STRING", /* FTYPE_TEXT */
	"SQLBOX_PARM_STRING", /* FTYPE_PASSWORD */
	"SQLBOX_PARM_STRING", /* FTYPE_EMAIL */
	NULL, /* FTYPE_STRUCT */
	"SQLBOX_PARM_INT", /* FTYPE_ENUM */
	"SQLBOX_PARM_INT", /* FTYPE_BITFIELD */
};

static	const char *const bindvars[FTYPE__MAX] = {
	"iparm", /* FTYPE_BIT */
	"iparm", /* FTYPE_DATE */
	"iparm", /* FTYPE_EPOCH */
	"iparm", /* FTYPE_INT */
	"fparm", /* FTYPE_REAL */
	"bparm", /* FTYPE_BLOB (XXX: is special) */
	"sparm", /* FTYPE_TEXT */
	"sparm", /* FTYPE_PASSWORD */
	"sparm", /* FTYPE_EMAIL */
	NULL, /* FTYPE_STRUCT */
	"iparm", /* FTYPE_ENUM */
	"iparm", /* FTYPE_BITFIELD */
};

/*
 * Basic validation functions for given types.
 */
static	const char *const validtypes[FTYPE__MAX] = {
	"kvalid_bit", /* FTYPE_BIT */
	"kvalid_date", /* FTYPE_DATE */
	"kvalid_int", /* FTYPE_EPOCH */
	"kvalid_int", /* FTYPE_INT */
	"kvalid_double", /* FTYPE_REAL */
	NULL, /* FTYPE_BLOB */
	"kvalid_string", /* FTYPE_TEXT */
	"kvalid_string", /* FTYPE_PASSWORD */
	"kvalid_email", /* FTYPE_EMAIL */
	NULL, /* FTYPE_STRUCT */
	"kvalid_int", /* FTYPE_ENUM */
	"kvalid_int", /* FTYPE_BITFIELD */
};

/*
 * Binary relations for known validation types.
 * NOTE: THESE ARE THE NEGATED FORMS.
 * So VALIDATE_GE y means "greater-equal to y", which we render as "NOT"
 * greater-equal, which is less-than.
 */
static	const char *const validbins[VALIDATE__MAX] = {
	"<", /* VALIDATE_GE */
	">", /* VALIDATE_LE */
	"<=", /* VALIDATE_GT */
	">=", /* VALIDATE_LT */
	"!=", /* VALIDATE_EQ */
};

/*
 * Emit the function for checking a password.
 * This should be a conditional phrase that evalutes to FALSE if the
 * password does NOT match the given type, TRUE if the password does
 * match the given type.
 */
static void
gen_print_checkpass(int ptr, size_t pos, 
	const char *name, enum optype type)
{
	const char	*s = ptr ? "->" : ".";

	assert(type == OPTYPE_EQUAL || type == OPTYPE_NEQUAL);

#ifdef __OpenBSD__
	printf("(crypt_checkpass(v%zu, p%s%s) %c= -1)", 
		pos, s, name, 
		type == OPTYPE_EQUAL ? '=' : '!');
#else
	printf("(strcmp(crypt(v%zu, p%s%s), p%s%s) %c= 0)", 
		pos, s, name, s, name, 
		type == OPTYPE_EQUAL ? '!' : '=');
#endif
}

static void
gen_print_newpass(int ptr, size_t pos, size_t npos)
{

#ifdef __OpenBSD__
	printf("\tcrypt_newhash(%sv%zu, \"blowfish,a\", "
		"hash%zu, sizeof(hash%zu));\n",
		ptr ? "*" : "", npos, pos, pos);
#else
	printf("\tstrncpy(hash%zu, crypt(%sv%zu, _gensalt()), "
		"sizeof(hash%zu));\n",
		pos, ptr ? "*" : "", npos, pos);
#endif
}

/*
 * When accepting only given roles, print the roles rooted at "r".
 * Don't print out the ROLE_all, but continue through it.
 */
static void
gen_role(const struct role *r)
{
	const struct role *rr;

	if (strcmp(r->name, "all"))
		printf("\tcase ROLE_%s:\n", r->name);
	TAILQ_FOREACH(rr, &r->subrq, entries)
		gen_role(rr);
}

/*
 * Fill an individual field from the database.
 */
static void
gen_strct_fill_field(const struct field *f)
{
	size_t	 indent;

	/*
	 * By default, structs on possibly-null foreign keys are set as
	 * not existing.
	 * We'll change this in db_xxx_reffind.
	 */

	if (f->type == FTYPE_STRUCT &&
	    (f->ref->source->flags & FIELD_NULL)) {
		printf("\tp->has_%s = 0;\n", f->name);
		return;
	} else if (f->type == FTYPE_STRUCT)
		return;

	if ((f->flags & FIELD_NULL))
		print_src(1, "p->has_%s = set->ps[*pos].type != "
			"SQLBOX_PARM_NULL;", f->name);

	/*
	 * Blob types need to have space allocated (and the space
	 * variable set) before we extract from the database.
	 * This sequence is very different from the other types, so make
	 * it into its own conditional block for clarity.
	 */

	if ((f->flags & FIELD_NULL)) {
		printf("\tif (p->has_%s) {\n", f->name);
		indent = 2;
	} else
		indent = 1;

	switch (f->type) {
	case FTYPE_BLOB:
		print_src(indent, 
			"if (%s(&set->ps[(*pos)++],\n"
			"    &p->%s, &p->%s_sz) == -1)\n"
		        "\texit(EXIT_FAILURE);",
			coltypes[f->type], f->name, f->name);
		break;
	case FTYPE_ENUM:
		print_src(indent,
			"if (%s(&set->ps[(*pos)++], &tmpint) == -1)\n"
		        "\texit(EXIT_FAILURE);\n"
			"p->%s = tmpint;",
			coltypes[f->type], f->name);
		break;
	case FTYPE_BIT:
	case FTYPE_BITFIELD:
	case FTYPE_DATE:
	case FTYPE_EPOCH:
	case FTYPE_INT:
	case FTYPE_REAL:
		print_src(indent,
			"if (%s(&set->ps[(*pos)++], &p->%s) == -1)\n"
		        "\texit(EXIT_FAILURE);",
			coltypes[f->type], f->name);
		break;
	default:
		print_src(indent,
			"if (%s\n"
			"    (&set->ps[(*pos)++], &p->%s, NULL) == -1)\n"
		        "\texit(EXIT_FAILURE);",
			coltypes[f->type], f->name);
		break;
	}

	if ((f->flags & FIELD_NULL))
		puts("\t} else\n"
		     "\t\t(*pos)++;");
}

/*
 * Counts how many entries are required if later passed to
 * query_gen_bindfunc().
 * The ones we don't pass to query_gen_bindfunc() are passwords that are
 * using the hashing functions.
 */
static size_t
query_count_bindfuncs(enum ftype t, enum optype type)
{

	assert(t != FTYPE_STRUCT);
	return !(t == FTYPE_PASSWORD && 
		type != OPTYPE_STREQ && type != OPTYPE_STRNEQ);

}

/*
 * Generate the binding for a field of type "t" at field "pos" with a
 * tab offset of "tabs", using query_count_bindfuncs() to see if we
 * should skip the binding.
 * Returns zero if we did not print a binding 1 otherwise.
 */
static size_t
update_gen_bindfunc(enum ftype t, size_t pos,
	int ptr, size_t tabs, enum optype type)
{
	size_t	 i;

	if (!query_count_bindfuncs(t, type))
		return 0;
	for (i = 0; i < tabs; i++)
		putchar('\t');
	printf("parms[%zu].%s = %sv%zu;\n", 
		pos - 1, bindvars[t], ptr ? "*" : "", pos);
	for (i = 0; i < tabs; i++)
		putchar('\t');
	printf("parms[%zu].type = %s;\n", 
		pos - 1, bindtypes[t]);
	if (t == FTYPE_BLOB) {
		for (i = 0; i < tabs; i++)
			putchar('\t');
		printf("parms[%zu].sz = v%zu_sz;\n", pos - 1, pos);
	}
	return 1;
}

/*
 * Like update_gen_bindfunc() but with a fixed number of tabs and never
 * being a pointer.
 */
static size_t
query_gen_bindfunc(enum ftype t, size_t pos, enum optype type)
{

	return update_gen_bindfunc(t, pos, 0, 1, type);
}

/*
 * Like update_gen_bindfunc() but only for hashed passwords.
 * Accepts an additional "hpos", which is the index of the current
 * "hash" variable as emitted.
 */
static void
update_gen_bindhash(size_t pos, size_t hpos, size_t tabs)
{
	size_t	 i;

	for (i = 0; i < tabs; i++)
		putchar('\t');
	printf("parms[%zu].sparm = hash%zu;\n", pos - 1, hpos);
	for (i = 0; i < tabs; i++)
		putchar('\t');
	printf("parms[%zu].type = SQLBOX_PARM_STRING;\n", pos - 1);
}

/*
 * Print out a search function for an STYPE_ITERATE.
 * This calls a function pointer with the retrieved data.
 */
static void
gen_strct_func_iter(const struct config *cfg,
	const struct search *s, size_t num)
{
	const struct sent	*sent;
	const struct sref	*sr;
	const struct strct 	*retstr;
	size_t			 pos, parms = 0;

	retstr = s->dst != NULL ? s->dst->strct : s->parent;

	/* Count all possible parameters to bind. */

	TAILQ_FOREACH(sent, &s->sntq, entries)
		if (OPTYPE_ISBINARY(sent->op)) {
			sr = TAILQ_LAST(&sent->srq, srefq);
			parms += query_count_bindfuncs
				(sr->field->type, sent->op);
		}

	/* Emit top of the function w/optional static parameters. */

	print_func_db_search(s, 0);
	printf("\n"
	       "{\n"
	       "\tstruct %s p;\n"
	       "\tconst struct sqlbox_parmset *res;\n"
	       "\tstruct sqlbox *db = ctx->db;\n",
	       retstr->name);
	if (parms > 0)
		printf("\tstruct sqlbox_parm parms[%zu];\n", parms);

	/* Emit parameter binding. */

	puts("");
	if (parms > 0)
		puts("\tmemset(parms, 0, sizeof(parms));");

	pos = 1;
	TAILQ_FOREACH(sent, &s->sntq, entries)
		if (OPTYPE_ISBINARY(sent->op)) {
			sr = TAILQ_LAST(&sent->srq, srefq);
			pos += query_gen_bindfunc
				(sr->field->type, pos, sent->op);
		}

	/* Stipulate multiple returned entries. */

	puts("");
	printf("\tif (!sqlbox_prepare_bind_async\n"
	       "\t    (db, 0, STMT_%s_BY_SEARCH_%zu,\n"
	       "\t     %zu, %s, SQLBOX_STMT_MULTI))\n"
	       "\t	exit(EXIT_FAILURE);\n",
	       s->parent->cname, num, parms,
	       parms > 0 ? "parms" : "NULL");

	/* Step til none left. */

	printf("\twhile ((res = sqlbox_step(db, 0)) "
			"!= NULL && res->psz) {\n"
	       "\t\tdb_%s_fill_r(ctx, &p, res, NULL);\n",
	       retstr->name);
	if ((retstr->flags & STRCT_HAS_NULLREFS))
	       printf("\t\tdb_%s_reffind(%s&p, db);\n", 
		     retstr->name,
		     (!TAILQ_EMPTY(&cfg->rq)) ? "ctx, " : "");

	/* Conditional post-query password check. */

	pos = 1;
	TAILQ_FOREACH(sent, &s->sntq, entries) {
		if (OPTYPE_ISUNARY(sent->op))
			continue;
		sr = TAILQ_LAST(&sent->srq, srefq);
		if (sr->field->type != FTYPE_PASSWORD ||
		    sent->op == OPTYPE_STREQ ||
		    sent->op == OPTYPE_STRNEQ) {
			pos++;
			continue;
		}
		printf("\t\tif ");
		gen_print_checkpass(0, pos, sent->fname, sent->op);
		printf(" {\n"
		       "\t\t\tdb_%s_unfill_r(&p);\n"
		       "\t\t\tcontinue;\n"
		       "\t\t}\n",
		       s->parent->name);
		pos++;
	}

	printf("\t\t(*cb)(&p, arg);\n"
	       "\t\tdb_%s_unfill_r(&p);\n"
	       "\t}\n"
	       "\tif (res == NULL)\n"
	       "\t\texit(EXIT_FAILURE);\n"
	       "\tif (!sqlbox_finalise(db, 0))\n"
	       "\t\texit(EXIT_FAILURE);\n"
	       "}\n"
	       "\n", retstr->name);
}

/*
 * Print out a search function for an STYPE_LIST.
 * This searches for a multiplicity of values.
 */
static void
gen_strct_func_list(const struct config *cfg, 
	const struct search *s, size_t num)
{
	const struct sent	*sent;
	const struct sref	*sr;
	const struct strct	*retstr;
	size_t	 		 pos, parms = 0;

	retstr = s->dst != NULL ? s->dst->strct : s->parent;

	/* Count all possible parameters to bind. */

	TAILQ_FOREACH(sent, &s->sntq, entries)
		if (OPTYPE_ISBINARY(sent->op)) {
			sr = TAILQ_LAST(&sent->srq, srefq);
			parms += query_count_bindfuncs
				(sr->field->type, sent->op);
		}

	/* Emit top of the function w/optional static parameters. */

	print_func_db_search(s, 0);
	printf("\n"
	       "{\n"
	       "\tstruct %s *p;\n"
	       "\tstruct %s_q *q;\n"
	       "\tconst struct sqlbox_parmset *res;\n"
	       "\tstruct sqlbox *db = ctx->db;\n",
	       retstr->name, retstr->name);
	if (parms > 0)
		printf("\tstruct sqlbox_parm parms[%zu];\n", parms);

	puts("");
	if (parms > 0)
		puts("\tmemset(parms, 0, sizeof(parms));");

	printf("\tq = malloc(sizeof(struct %s_q));\n"
	       "\tif (q == NULL) {\n"
	       "\t\tperror(NULL);\n"
	       "\t\texit(EXIT_FAILURE);\n"
	       "\t}\n"
	       "\tTAILQ_INIT(q);\n"
	       "\n", retstr->name);

	/* Emit parameter binding. */

	pos = 1;
	TAILQ_FOREACH(sent, &s->sntq, entries)
		if (OPTYPE_ISBINARY(sent->op)) {
			sr = TAILQ_LAST(&sent->srq, srefq);
			pos += query_gen_bindfunc
				(sr->field->type, pos, sent->op);
		}
	if (pos > 1)
		puts("");

	/* Stipulate multiple returned entries. */

	printf("\tif (!sqlbox_prepare_bind_async\n"
	       "\t    (db, 0, STMT_%s_BY_SEARCH_%zu,\n"
	       "\t     %zu, %s, SQLBOX_STMT_MULTI))\n"
	       "\t	exit(EXIT_FAILURE);\n",
	       s->parent->cname, num, parms,
	       parms > 0 ? "parms" : "NULL");

	/* Step til none left. */

	printf("\twhile ((res = sqlbox_step(db, 0)) != NULL "
			"&& res->psz) {\n"
	       "\t\tp = malloc(sizeof(struct %s));\n"
	       "\t\tif (p == NULL) {\n"
	       "\t\t\tperror(NULL);\n"
	       "\t\t\texit(EXIT_FAILURE);\n"
	       "\t\t}\n"
	       "\t\tdb_%s_fill_r(ctx, p, res, NULL);\n",
	       retstr->name, retstr->name);
	if (STRCT_HAS_NULLREFS & retstr->flags)
	       printf("\t\tdb_%s_reffind(%sp, db);\n",
		      retstr->name,
		      (!TAILQ_EMPTY(&cfg->rq)) ? 
		      "ctx, " : "");

	/* Conditional post-query password check. */

	pos = 1;
	TAILQ_FOREACH(sent, &s->sntq, entries) {
		if (OPTYPE_ISUNARY(sent->op))
			continue;
		sr = TAILQ_LAST(&sent->srq, srefq);
		if (sr->field->type != FTYPE_PASSWORD ||
		    sent->op == OPTYPE_STREQ ||
		    sent->op == OPTYPE_STRNEQ) {
			pos++;
			continue;
		}
		printf("\t\tif ");
		gen_print_checkpass(1, pos, sent->fname, sent->op);
		printf(" {\n"
		       "\t\t\tdb_%s_free(p);\n"
		       "\t\t\tp = NULL;\n"
		       "\t\t\tcontinue;\n"
		       "\t\t}\n",
		       s->parent->name);
		pos++;
	}

	puts("\t\tTAILQ_INSERT_TAIL(q, p, _entries);\n"
	     "\t}\n"
	     "\tif (res == NULL)\n"
	     "\t\texit(EXIT_FAILURE);\n"
	     "\tif (!sqlbox_finalise(db, 0))\n"
	     "\t\texit(EXIT_FAILURE);\n"
	     "\treturn q;\n"
	     "}\n"
	     "");
}

/*
 * Count all roles beneath a given role excluding "all".
 * Returns the number, which is never zero.
 */
static size_t
gen_func_role_count(const struct role *role)
{
	size_t	 i = 0;
	const struct role *r;

	if (strcmp(role->name, "all"))
		i++;
	TAILQ_FOREACH(r, &role->subrq, entries)
		i += gen_func_role_count(r);
	return(i);
}

static void
gen_func_roles(const struct role *r)
{
	const struct role	*rr;

	if (r->parent != NULL &&
	    strcmp(r->parent->name, "all") && 
	    strcmp(r->parent->name, "none"))
		printf("\tif (!sqlbox_role_hier_child"
			 "(hier, ROLE_%s, ROLE_%s))\n"
		       "\t\tgoto err;\n",
		       r->parent->name, r->name);

	TAILQ_FOREACH(rr, &r->subrq, entries)
		gen_func_roles(rr);
}

/*
 * Actually print the sqlbox_role_hier_stmt() function for the statement
 * enumeration in "stmt".
 * This does nothing if we're doing the "all" or "none" role.
 */
static void
gen_func_role_stmt(const struct role *r, const char *stmt)
{

	if (strcmp(r->name, "all") == 0 ||
	    strcmp(r->name, "none") == 0)
		return;
	printf("\tif (!sqlbox_role_hier_stmt(hier, ROLE_%s, %s))\n"
	       "\t\tgoto err;\n", r->name, stmt);
}

/*
 * Print the sqlbox_role_hier_stmt() for all roles.
 * This means that we print for the immediate children of the "all"
 * role and let the hierarchical algorithm handle the rest.
 */
static void
gen_func_role_stmts_all(const struct config *cfg, const char *stmt)
{
	const struct role	*r, *rr;

	TAILQ_FOREACH(r, &cfg->rq, entries)
		if (strcmp(r->name, "all") == 0)
			TAILQ_FOREACH(rr, &r->subrq, entries)
				gen_func_role_stmt(rr, stmt);
}

/*
 * For structure "p", print all roles capable of all operations.
 * Return >1 if we've printed statements, <0 on memory allocation
 * failure, and 0 otherwise (success, no statements).
 */
static int
gen_func_role_stmts(const struct config *cfg, const struct strct *p)
{
	const struct roleset	*rs;
	const struct search 	*s;
	const struct update 	*u;
	const struct field 	*f;
	size_t	 		 pos, shown = 0;
	char			*buf;

	/* 
	 * FIXME: only do this if the role needs access to this, which
	 * needs to be figured out by a recursive scan.
	 */

	TAILQ_FOREACH(f, &p->fq, entries)
		if ((f->flags & (FIELD_ROWID|FIELD_UNIQUE))) {
			if (asprintf(&buf, "STMT_%s_BY_UNIQUE_%s",
			    p->cname, f->name) < 0)
				return -1;
			gen_func_role_stmts_all(cfg, buf);
			shown++;
			free(buf);
		}

	/* Start with all query types. */

	pos = 0;
	TAILQ_FOREACH(s, &p->sq, entries) {
		pos++;
		if (s->rolemap == NULL)
			continue;
		if (asprintf(&buf, "STMT_%s_BY_SEARCH_%zu", 
		    p->cname, pos - 1) < 0)
			return -1;
		TAILQ_FOREACH(rs, &s->rolemap->setq, entries)
			if (strcmp(rs->name, "all") == 0)
				gen_func_role_stmts_all(cfg, buf);
			else
				gen_func_role_stmt(rs->role, buf);
		shown++;
		free(buf);
	}

	/* Next: insertions. */

	if (p->ins != NULL && p->ins->rolemap != NULL) {
		if (asprintf(&buf, "STMT_%s_INSERT", p->cname) < 0)
			return -1;
		TAILQ_FOREACH(rs, &p->ins->rolemap->setq, entries)
			if (strcmp(rs->name, "all") == 0)
				gen_func_role_stmts_all(cfg, buf);
			else
				gen_func_role_stmt(rs->role, buf);
		shown++;
		free(buf);
	}

	/* Next: updates. */

	pos = 0;
	TAILQ_FOREACH(u, &p->uq, entries) {
		pos++;
		if (u->rolemap == NULL)
			continue;
		if (asprintf(&buf, "STMT_%s_UPDATE_%zu", 
		    p->cname, pos - 1) < 0)
			return -1;
		TAILQ_FOREACH(rs, &u->rolemap->setq, entries)
			if (strcmp(rs->name, "all") == 0)
				gen_func_role_stmts_all(cfg, buf);
			else
				gen_func_role_stmt(rs->role, buf);
		shown++;
		free(buf);
	}

	/* Finally: deletions. */

	pos = 0;
	TAILQ_FOREACH(u, &p->dq, entries) {
		pos++;
		if (u->rolemap == NULL)
			continue;
		if (asprintf(&buf, "STMT_%s_DELETE_%zu", 
		    p->cname, pos - 1) < 0)
			return -1;
		TAILQ_FOREACH(rs, &u->rolemap->setq, entries)
			if (strcmp(rs->name, "all") == 0)
				gen_func_role_stmts_all(cfg, buf);
			else
				gen_func_role_stmt(rs->role, buf);
		shown++;
		free(buf);
	}

	return shown > 0;
}

/*
 * Generate database opening.
 * We don't use the generic invocation, as we want foreign keys.
 * Returns TRUE on success, FALSE on memory allocation failure.
 */
static int
gen_func_open(const struct config *cfg)
{
	const struct role 	*r;
	const struct strct 	*p;
	size_t			 i;
	int			 c;

	print_func_db_set_logging(0);
	puts("{\n"
	     "\n"
	     "\tif (!sqlbox_msg_set_dat(ort->db, arg, sz))\n"
	     "\t\texit(EXIT_FAILURE);\n"
	     "}\n");
	print_func_db_open(0);
	puts("{\n"
	     "\n"
	     "\treturn db_open_logging(file, NULL, NULL, NULL);\n"
	     "}\n");
	print_func_db_open_logging(0);
	puts("{\n"
	     "\tsize_t i;\n"
	     "\tstruct ort *ctx = NULL;\n"
	     "\tstruct sqlbox_cfg cfg;\n"
	     "\tstruct sqlbox *db = NULL;\n"
	     "\tstruct sqlbox_pstmt pstmts[STMT__MAX];\n"
	     "\tstruct sqlbox_src srcs[1] = {\n"
	     "\t\t{ .fname = (char *)file,\n"
	     "\t\t  .mode = SQLBOX_SRC_RW }\n"
	     "\t};");
	if (!TAILQ_EMPTY(&cfg->rq))
		puts("\tstruct sqlbox_role_hier *hier = NULL;");
	puts("\n"
	     "\tmemset(&cfg, 0, sizeof(struct sqlbox_cfg));\n"
	     "\tcfg.msg.func = log;\n"
	     "\tcfg.msg.func_short = log_short;\n"
	     "\tcfg.msg.dat = log_arg;\n"
	     "\tcfg.srcs.srcs = srcs;\n"
	     "\tcfg.srcs.srcsz = 1;\n"
	     "\tcfg.stmts.stmts = pstmts;\n"
	     "\tcfg.stmts.stmtsz = STMT__MAX;\n"
	     "\n"
	     "\tfor (i = 0; i < STMT__MAX; i++)\n"
	     "\t\tpstmts[i].stmt = (char *)stmts[i];\n"
	     "\n"
	     "\tctx = malloc(sizeof(struct ort));\n"
	     "\tif (ctx == NULL)\n"
	     "\t\tgoto err;\n");

	if (!TAILQ_EMPTY(&cfg->rq)) {
		/*
		 * We need an complete count of all roles except the
		 * "all" role, which cannot be entered or processed.
		 * So do this recursively.
		 */

		i = 0;
		TAILQ_FOREACH(r, &cfg->rq, entries)
			i += gen_func_role_count(r);
		assert(i > 0);
		printf("\thier = sqlbox_role_hier_alloc(%zu);\n"
		       "\tif (hier == NULL)\n"
		       "\t\tgoto err;\n"
		       "\n", i);

		print_commentt(1, COMMENT_C, "Assign roles.");

		/* 
		 * FIXME: the default role should only be able to open
		 * the database once.
		 * With this, it's able to do so multiple times and
		 * that's not a permission it needs.
		 */

		puts("\n"
		     "\tif (!sqlbox_role_hier_sink(hier, ROLE_none))\n"
		     "\t\tgoto err;\n"
		     "\tif (!sqlbox_role_hier_start(hier, ROLE_default))\n"
		     "\t\tgoto err;\n"
		     "\tif (!sqlbox_role_hier_src(hier, ROLE_default, 0))\n"
		     "\t\tgoto err;");

		TAILQ_FOREACH(r, &cfg->rq, entries)
			gen_func_roles(r);

		puts("");
		TAILQ_FOREACH(p, &cfg->sq, entries) {
			print_commentv(1, COMMENT_C, 
				"White-listing fields and "
				"operations for structure \"%s\".",
				p->name);
			puts("");
			if ((c = gen_func_role_stmts(cfg, p)) < 0)
				return 0;
			else if (c > 0)
				puts("");
		}
		printf("\tif (!sqlbox_role_hier_gen"
			"(hier, &cfg.roles, ROLE_default))\n"
		       "\t\tgoto err;\n\n");
	}

	puts("\tif ((db = sqlbox_alloc(&cfg)) == NULL)\n"
	     "\t\tgoto err;\n"
	     "\tctx->db = db;");

	if (!TAILQ_EMPTY(&cfg->rq))
	        puts("\tctx->role = ROLE_default;\n"
		     "\n"
		     "\tsqlbox_role_hier_gen_free(&cfg.roles);\n"
		     "\tsqlbox_role_hier_free(hier);\n"
		     "\thier = NULL;\n");
	else
		puts("");

	print_commentv(1, COMMENT_C, 
		"Now actually open the database.\n"
		"If this succeeds, then we're good to go.");

	puts("\n"
	     "\tif (sqlbox_open_async(db, 0))\n"
	     "\t\treturn ctx;\n"
	     "err:");

	if (!TAILQ_EMPTY(&cfg->rq))
		puts("\tsqlbox_role_hier_gen_free(&cfg.roles);\n"
	     	     "\tsqlbox_role_hier_free(hier);");

	puts("\tsqlbox_free(db);\n"
	     "\tfree(ctx);\n"
	     "\treturn NULL;\n"
	     "}\n"
	     "");
	return 1;
}

static void
gen_func_rolecases(const struct role *r)
{
	const struct role *rp, *rr;

	assert(NULL != r->parent);

	printf("\tcase ROLE_%s:\n", r->name);

	/*
	 * If our parent is "all", then there's nowhere we can
	 * transition into, as we can only transition "up" the tree of
	 * roles (i.e., into roles with less specific privileges).
	 * Thus, every attempt to transition should fail.
	 */

	if (0 == strcmp(r->parent->name, "all")) {
		puts("\t\tabort();\n"
		     "\t\t/* NOTREACHED */");
		TAILQ_FOREACH(rr, &r->subrq, entries)
			gen_func_rolecases(rr);
		return;
	}

	/* Here, we can transition into lesser privileges. */

	puts("\t\tswitch (r) {");
	for (rp = r->parent; strcmp(rp->name, "all"); rp = rp->parent)
		printf("\t\tcase ROLE_%s:\n", rp->name);

	puts("\t\t\tctx->role = r;\n"
	     "\t\t\treturn;\n"
	     "\t\tdefault:\n"
	     "\t\t\tabort();\n"
	     "\t\t}\n"
	     "\t\tbreak;");

	TAILQ_FOREACH(rr, &r->subrq, entries)
		gen_func_rolecases(rr);
}

static void
gen_func_role_transitions(const struct config *cfg)
{
	const struct role *r, *rr;

	TAILQ_FOREACH(r, &cfg->rq, entries)
		if (0 == strcmp(r->name, "all"))
			break;
	assert(NULL != r);

	print_func_db_role(0);
	puts("{\n"
	     "\tif (!sqlbox_role(ctx->db, r))\n"
	     "\t\texit(EXIT_FAILURE);\n"
	     "\tif (r == ctx->role)\n"
	     "\t\treturn;\n"
	     "\tif (ctx->role == ROLE_none)\n"
	     "\t\tabort();\n"
	     "\n"
	     "\tswitch (ctx->role) {\n"
	     "\tcase ROLE_default:\n"
	     "\t\tctx->role = r;\n"
	     "\t\treturn;");
	TAILQ_FOREACH(rr, &r->subrq, entries)
		gen_func_rolecases(rr);
	puts("\tdefault:\n"
	     "\t\tabort();\n"
	     "\t}\n"
	     "}\n");
	print_func_db_role_current(0);
	puts("{\n"
	     "\treturn ctx->role;\n"
	     "}\n");
	print_func_db_role_stored(0);
	puts("{\n"
	     "\treturn s->role;\n"
	     "}\n");
}

static void
gen_func_trans(const struct config *cfg)
{

	print_func_db_trans_open(0);
	puts("{\n"
	     "\tstruct sqlbox *db = ctx->db;\n"
	     "\tint c;\n"
	     "\n"
	     "\tif (mode < 0)\n"
	     "\t\tc = sqlbox_trans_exclusive(db, 0, id);\n"
	     "\telse if (mode > 0)\n"
	     "\t\tc = sqlbox_trans_immediate(db, 0, id);\n"
	     "\telse\n"
	     "\t\tc = sqlbox_trans_deferred(db, 0, id);\n"
	     "\tif (!c)\n"
	     "\t\texit(EXIT_FAILURE);\n"
	     "}\n"
	     "");
	print_func_db_trans_rollback(0);
	puts("{\n"
	     "\tstruct sqlbox *db = ctx->db;\n"
	     "\n"
	     "\tif (!sqlbox_trans_rollback(db, 0, id))\n"
	     "\t\texit(EXIT_FAILURE);\n"
	     "}\n"
	     "");
	print_func_db_trans_commit(0);
	puts("{\n"
	     "\tstruct sqlbox *db = ctx->db;\n"
	     "\n"
	     "\tif (!sqlbox_trans_commit(db, 0, id))\n"
	     "\t\texit(EXIT_FAILURE);\n"
	     "}\n"
	     "");
}

/*
 * Close and free the database context.
 * This is sensitive to whether we have roles.
 */
static void
gen_func_close(const struct config *cfg)
{

	print_func_db_close(0);
	puts("{\n"
	     "\tif (p == NULL)\n"
	     "\t\treturn;\n"
             "\tsqlbox_free(p->db);\n"
	     "\tfree(p);\n"
	     "}\n"
	     "");
}

/*
 * Print out a counting/search function for an STYPE_COUNT.
 */
static void
gen_strct_func_count(const struct config *cfg,
	const struct search *s, size_t num)
{
	const struct sent  *sent;
	const struct sref  *sr;
	size_t	 	    pos, parms = 0;

	/* Count all possible parameters to bind. */

	TAILQ_FOREACH(sent, &s->sntq, entries) 
		if (OPTYPE_ISBINARY(sent->op)) {
			sr = TAILQ_LAST(&sent->srq, srefq);
			parms += query_count_bindfuncs
				(sr->field->type, sent->op);
		}

	print_func_db_search(s, 0);
	puts("\n"
	     "{\n"
	     "\tconst struct sqlbox_parmset *res;\n"
	     "\tint64_t val;\n"
	     "\tstruct sqlbox *db = ctx->db;");
	if (parms > 0)
		printf("\tstruct sqlbox_parm parms[%zu];\n", parms);

	/* Emit parameter binding. */

	puts("");
	pos = 1;
	TAILQ_FOREACH(sent, &s->sntq, entries) 
		if (OPTYPE_ISBINARY(sent->op)) {
			sr = TAILQ_LAST(&sent->srq, srefq);
			pos += query_gen_bindfunc
				(sr->field->type, pos, sent->op);
		}

	/* A single returned entry. */

	puts("");
	printf("\tif (!sqlbox_prepare_bind_async\n"
	       "\t    (db, 0, STMT_%s_BY_SEARCH_%zu, %zu, %s, 0))\n"
	       "\t	exit(EXIT_FAILURE);\n",
	       s->parent->cname, num, parms,
	       parms > 0 ? "parms" : "NULL");

	printf("\tif ((res = sqlbox_step(db, 0)) == NULL)\n"
	     "\t\texit(EXIT_FAILURE);\n"
	     "\telse if (res->psz != 1)\n"
	     "\t\texit(EXIT_FAILURE);\n"
	     "\tif (sqlbox_parm_int(&res->ps[0], &val) == -1)\n"
	     "\t\texit(EXIT_FAILURE);\n"
	     "\tsqlbox_finalise(db, 0);\n"
	     "\treturn (uint64_t)val;\n"
	     "}\n"
	     "");
}

/*
 * Print out a search function for an STYPE_SEARCH.
 * This searches for a singular value.
 */
static void
gen_strct_func_srch(const struct config *cfg,
	const struct search *s, size_t num)
{
	const struct sent	*sent;
	const struct sref	*sr;
	const struct strct	*retstr;
	size_t			 pos, parms = 0;

	retstr = s->dst != NULL ? s->dst->strct : s->parent;

	/* Count all possible parameters to bind. */

	pos = 1;
	TAILQ_FOREACH(sent, &s->sntq, entries) 
		if (OPTYPE_ISBINARY(sent->op)) {
			sr = TAILQ_LAST(&sent->srq, srefq);
			parms += query_count_bindfuncs
				(sr->field->type, sent->op);
		}

	print_func_db_search(s, 0);
	printf("\n"
	       "{\n"
	       "\tstruct %s *p = NULL;\n"
	       "\tconst struct sqlbox_parmset *res;\n"
	       "\tstruct sqlbox *db = ctx->db;\n",
	       retstr->name);
	if (parms > 0)
		printf("\tstruct sqlbox_parm parms[%zu];\n", parms);

	/* Emit parameter binding. */

	puts("");
	if (parms > 0)
		puts("\tmemset(parms, 0, sizeof(parms));");

	pos = 1;
	TAILQ_FOREACH(sent, &s->sntq, entries) 
		if (OPTYPE_ISBINARY(sent->op)) {
			sr = TAILQ_LAST(&sent->srq, srefq);
			pos += query_gen_bindfunc
				(sr->field->type, pos, sent->op);
		}

	puts("");
	printf("\tif (!sqlbox_prepare_bind_async\n"
	       "\t    (db, 0, STMT_%s_BY_SEARCH_%zu, %zu, %s, 0))\n"
	       "\t	exit(EXIT_FAILURE);\n",
	       s->parent->cname, num, parms,
	       parms > 0 ? "parms" : "NULL");

	printf("\tif ((res = sqlbox_step(db, 0)) != NULL "
			"&& res->psz) {\n"
	       "\t\tp = malloc(sizeof(struct %s));\n"
	       "\t\tif (p == NULL) {\n"
	       "\t\t\tperror(NULL);\n"
	       "\t\t\texit(EXIT_FAILURE);\n"
	       "\t\t}\n"
	       "\t\tdb_%s_fill_r(ctx, p, res, NULL);\n",
	       retstr->name, retstr->name);
	if (STRCT_HAS_NULLREFS & retstr->flags)
	       printf("\t\tdb_%s_reffind(%sp, db);\n",
		      retstr->name,
	 	      (!TAILQ_EMPTY(&cfg->rq)) ? "ctx, " : "");

	/* Conditional post-query password check. */

	pos = 1;
	TAILQ_FOREACH(sent, &s->sntq, entries) {
		if (OPTYPE_ISUNARY(sent->op))
			continue;
		sr = TAILQ_LAST(&sent->srq, srefq);
		if (sr->field->type != FTYPE_PASSWORD ||
		    sent->op == OPTYPE_STREQ ||
		    sent->op == OPTYPE_STRNEQ) {
			pos++;
			continue;
		}
		printf("\t\tif ");
		gen_print_checkpass(1, pos, sent->fname, sent->op);
		printf(" {\n"
		       "\t\t\tdb_%s_free(p);\n"
		       "\t\t\tp = NULL;\n"
		       "\t\t}\n", 
		       s->parent->name);
		pos++;
	}

	puts("\t}\n"
	     "\tif (res == NULL)\n"
	     "\t\texit(EXIT_FAILURE);\n"
	     "\tif (!sqlbox_finalise(db, 0))\n"
	     "\t\texit(EXIT_FAILURE);\n"
	     "\treturn p;\n"
	     "}\n"
	     "");
}

/*
 * Generate the "freeq" function.
 * This must have STRCT_HAS_QUEUE defined in its flags, otherwise the
 * function does nothing.
 */
static void
gen_func_freeq(const struct strct *p)
{

	if ( ! (STRCT_HAS_QUEUE & p->flags))
		return;

	print_func_db_freeq(p, 0);
	printf("\n"
	       "{\n"
	       "\tstruct %s *p;\n\n"
	       "\tif (q == NULL)\n"
	       "\t\treturn;\n"
	       "\twhile ((p = TAILQ_FIRST(q)) != NULL) {\n"
	       "\t\tTAILQ_REMOVE(q, p, _entries);\n"
	       "\t\tdb_%s_free(p);\n"
	       "\t}\n"
	       "\tfree(q);\n"
	       "}\n"
	       "\n", 
	       p->name, p->name);
}

/*
 * Generate the "insert" function.
 * This does nothing if we don't have an insert function.
 */
static void
gen_func_insert(const struct config *cfg, const struct strct *p)
{
	const struct field	*f;
	size_t			 pos, npos, parms = 0, tabs;

	if (p->ins == NULL)
		return;

	/* Count non-struct non-rowid parameters to bind. */

	TAILQ_FOREACH(f, &p->fq, entries)
		if (f->type != FTYPE_STRUCT && 
		    !(f->flags & FIELD_ROWID))
			parms++;

	print_func_db_insert(p, 0);
	puts("\n"
	     "{\n"
	     "\tint rc;\n"
	     "\tint64_t id = -1;\n"
	     "\tstruct sqlbox *db = ctx->db;");
	if (parms > 0)
		printf("\tstruct sqlbox_parm parms[%zu];\n", parms);

	/* Start by generating password hashes. */

	pos = 1;
	TAILQ_FOREACH(f, &p->fq, entries)
		if (f->type == FTYPE_PASSWORD)
			printf("\tchar hash%zu[64];\n", pos++);
	puts("");
	pos = npos = 1;
	TAILQ_FOREACH(f, &p->fq, entries) {
		if (f->type == FTYPE_STRUCT ||
		    (f->flags & FIELD_ROWID))
			continue;
		if (f->type != FTYPE_PASSWORD) {
			npos++;
			continue;
		}
		if ((f->flags & FIELD_NULL))
			printf("\tif (v%zu != NULL)\n\t", npos);
		gen_print_newpass((f->flags & FIELD_NULL), pos, npos);
		pos++;
		npos++;
	}
	if (pos > 1)
		puts("");

	if (parms > 0)
		puts("\tmemset(parms, 0, sizeof(parms));");

	pos = npos = 1;
	TAILQ_FOREACH(f, &p->fq, entries) {
		if (f->type == FTYPE_STRUCT ||
		    (f->flags & FIELD_ROWID))
			continue;
		tabs = 1;
		if ((f->flags & FIELD_NULL)) {
			printf("\tif (v%zu == NULL) {\n"
			       "\t\tparms[%zu].type = "
					"SQLBOX_PARM_NULL;\n"
			       "\t} else {\n", npos, npos - 1);
			tabs++;
		}
		if (f->type == FTYPE_PASSWORD)
			update_gen_bindhash(npos, pos++, tabs);
		else
			update_gen_bindfunc(f->type, npos, 
				(f->flags & FIELD_NULL), 
				tabs, OPTYPE_EQUAL /* XXX */);
		if ((f->flags & FIELD_NULL))
			puts("\t}");
		npos++;
	}

	if (parms > 0)
		puts("");

	printf("\trc = sqlbox_exec(db, 0, STMT_%s_INSERT, \n"
	       "\t     %zu, %s, SQLBOX_STMT_CONSTRAINT);\n"
	       "\tif (rc == SQLBOX_CODE_ERROR)\n"
	       "\t\texit(EXIT_FAILURE);\n"
	       "\telse if (rc != SQLBOX_CODE_OK)\n"
	       "\t\treturn (-1);\n"
	       "\tif (!sqlbox_lastid(db, 0, &id))\n"
	       "\t\texit(EXIT_FAILURE);\n"
	       "\treturn id;\n"
	       "}\n"
	       "\n",
	       p->cname, parms,
	       parms > 0 ? "parms" : "NULL");
}

/*
 * Generate the "free" function.
 */
static void
gen_func_free(const struct strct *p)
{

	print_func_db_free(p, 0);
	printf("\n"
	       "{\n"
	       "\tdb_%s_unfill_r(p);\n"
	       "\tfree(p);\n"
	       "}\n"
	       "\n", 
	       p->name);
}

/*
 * Generate the "unfill" function.
 */
static void
gen_func_unfill(const struct config *cfg, const struct strct *p)
{
	const struct field *f;

	print_commentt(0, COMMENT_C,
	       "Free resources from \"p\" and all nested objects.\n"
	       "Does not free the \"p\" pointer itself.\n"
	       "Has no effect if \"p\" is NULL.");

	printf("static void\n"
	       "db_%s_unfill(struct %s *p)\n",
	       p->name, p->name);
	puts("{\n"
	     "\tif (p == NULL)\n"
	     "\t\treturn;");
	TAILQ_FOREACH(f, &p->fq, entries)
		switch(f->type) {
		case (FTYPE_BLOB):
		case (FTYPE_PASSWORD):
		case (FTYPE_TEXT):
		case (FTYPE_EMAIL):
			printf("\tfree(p->%s);\n", f->name);
			break;
		default:
			break;
		}
	if (!TAILQ_EMPTY(&cfg->rq))
		puts("\tfree(p->priv_store);");
	puts("}\n"
	     "");
}

/*
 * Generate the nested "unfill" function.
 */
static void
gen_func_unfill_r(const struct strct *p)
{
	const struct field *f;

	printf("static void\n"
	       "db_%s_unfill_r(struct %s *p)\n"
	       "{\n"
	       "\tif (p == NULL)\n"
	       "\t\treturn;\n"
	       "\tdb_%s_unfill(p);\n",
	       p->name, p->name, p->name);
	TAILQ_FOREACH(f, &p->fq, entries) {
		if (FTYPE_STRUCT != f->type)
			continue;
		if (FIELD_NULL & f->ref->source->flags)
			printf("\tif (p->has_%s)\n"
			       "\t\tdb_%s_unfill_r(&p->%s);\n",
				f->ref->source->name, 
				f->ref->tstrct, f->name);
		else
			printf("\tdb_%s_unfill_r(&p->%s);\n",
				f->ref->tstrct, f->name);
	}
	puts("}\n"
	     "");
}

/*
 * If a structure has possible null foreign keys, we need to fill in the
 * null keys after the lookup has taken place IFF they aren't null.
 * Do that here, using the STMT_XXX_BY_UNIQUE_yyy fields we generated
 * earlier.
 * This is only done for structures that have (or have nested)
 * structures with null foreign keys.
 */
static void
gen_func_reffind(const struct config *cfg, const struct strct *p)
{
	const struct field *f;

	if (!(p->flags & STRCT_HAS_NULLREFS))
		return;

	TAILQ_FOREACH(f, &p->fq, entries)
		if ((f->type == FTYPE_STRUCT) &&
		    (f->ref->source->flags & FIELD_NULL))
			break;

	printf("static void\n"
	       "db_%s_reffind(%sstruct %s *p, struct sqlbox *db)\n"
	       "{\n", p->name, (!TAILQ_EMPTY(&cfg->rq)) ? 
	       "struct ort *ctx, " : "", p->name);
	if (f != NULL)
		puts("\tconst struct sqlbox_parmset *res;\n"
		     "\tstruct sqlbox_parm parm;");

	puts("");
	TAILQ_FOREACH(f, &p->fq, entries) {
		if (f->type != FTYPE_STRUCT)
			continue;
		if ((f->ref->source->flags & FIELD_NULL))
			printf("\tif (p->has_%s) {\n"
			       "\t\tparm.type = SQLBOX_PARM_INT;\n"
			       "\t\tparm.iparm = p->%s;\n"
			       "\t\tif (!sqlbox_prepare_bind_async\n"
			       "\t\t    (db, 0, STMT_%s_BY_UNIQUE_%s, 1, &parm, 0))\n"
			       "\t\t\texit(EXIT_FAILURE);\n"
			       "\t\tif ((res = sqlbox_step(db, 0)) == NULL)\n"
			       "\t\t\texit(EXIT_FAILURE);\n"
			       "\t\tdb_%s_fill_r(ctx, &p->%s, res, NULL);\n"
			       "\t\tif (!sqlbox_finalise(db, 0))\n"
			       "\t\t\texit(EXIT_FAILURE);\n"
			       "\t\tp->has_%s = 1;\n"
			       "\t}\n",
			       f->ref->source->name,
			       f->ref->source->name,
			       f->ref->target->parent->cname,
			       f->ref->target->name,
			       f->ref->target->parent->name,
			       f->name, f->name);
		if (!(f->ref->target->parent->flags & 
		    STRCT_HAS_NULLREFS))
			continue;
		printf("\tdb_%s_reffind(%s&p->%s, db);\n", 
			f->ref->target->parent->name, 
			(!TAILQ_EMPTY(&cfg->rq)) ? 
			"ctx, " : "", f->name);
	}
	puts("}\n");
}

/*
 * Generate the recursive "fill" function.
 * This simply calls to the underlying "fill" function for all
 * strutcures in the object.
 */
static void
gen_func_fill_r(const struct config *cfg, const struct strct *p)
{
	const struct field *f;

	printf("static void\n"
	       "db_%s_fill_r(struct ort *ctx, struct %s *p,\n"
	       "\tconst struct sqlbox_parmset *res, size_t *pos)\n"
	       "{\n"
	       "\tsize_t i = 0;\n"
	       "\n"
	       "\tif (pos == NULL)\n"
	       "\t\tpos = &i;\n"
	       "\tdb_%s_fill(ctx, p, res, pos);\n",
	       p->name, p->name, p->name);

	TAILQ_FOREACH(f, &p->fq, entries)
		if (f->type == FTYPE_STRUCT &&
		    !(f->ref->source->flags & FIELD_NULL))
			printf("\tdb_%s_fill_r(ctx, &p->%s, "
				"res, pos);\n", 
				f->ref->tstrct, f->name);
	puts("}\n");
}

/*
 * Generate the "fill" function.
 */
static void
gen_func_fill(const struct config *cfg, const struct strct *p)
{
	const struct field	*f;
	int	 		 needint = 0;

	TAILQ_FOREACH(f, &p->fq, entries)
		if (f->type == FTYPE_ENUM)
			needint = 1;

	print_commentv(0, COMMENT_C, 
	       "Fill in a %s from an open statement \"stmt\".\n"
	       "This starts grabbing results from \"pos\", "
	       "which may be NULL to start from zero.\n"
	       "This follows DB_SCHEMA_%s's order for columns.",
	       p->name, p->cname);
	printf("static void\n"
	       "db_%s_fill(struct ort *ctx, struct %s *p, "
		"const struct sqlbox_parmset *set, size_t *pos)\n",
	       p->name, p->name);
	puts("{\n"
	     "\tsize_t i = 0;");
	if (needint)
		puts("\tint64_t tmpint;");
	puts("\n"
	     "\tif (pos == NULL)\n"
	     "\t\tpos = &i;\n"
	     "\tmemset(p, 0, sizeof(*p));");
	TAILQ_FOREACH(f, &p->fq, entries)
		gen_strct_fill_field(f);
	if (!TAILQ_EMPTY(&cfg->rq)) {
		puts("\tp->priv_store = malloc"
		      "(sizeof(struct ort_store));\n"
		     "\tif (p->priv_store == NULL) {\n"
		     "\t\tperror(NULL);\n"
		     "\t\texit(EXIT_FAILURE);\n"
		     "\t}\n"
		     "\tp->priv_store->role = ctx->role;");
	}
	puts("}\n");
}

/*
 * Generate an update or delete function.
 */
static void
gen_func_update(const struct config *cfg,
	const struct update *up, size_t num)
{
	const struct uref	*ref;
	size_t	 		 pos, npos, parms = 0, tabs;

	/* Count all possible (modify & constrain) parameters. */

	TAILQ_FOREACH(ref, &up->mrq, entries) {
		assert(ref->field->type != FTYPE_STRUCT);
		parms++;
	}
	TAILQ_FOREACH(ref, &up->crq, entries) {
		assert(ref->field->type != FTYPE_STRUCT);
		if (!OPTYPE_ISUNARY(ref->op))
			parms++;
	}

	/* Emit function prologue. */

	print_func_db_update(up, 0);
	puts("\n"
	     "{\n"
	     "\tenum sqlbox_code c;\n"
	     "\tstruct sqlbox *db = ctx->db;");
	if (parms > 0)
		printf("\tstruct sqlbox_parm parms[%zu];\n", parms);

	/* 
	 * Handle case of hashing first.
	 * If setting a password, we need to hash it.
	 * Create a temporary buffer into which we're going to hash the
	 * password.
	 */

	pos = 1;
	TAILQ_FOREACH(ref, &up->mrq, entries)
		if (ref->field->type == FTYPE_PASSWORD &&
		    ref->mod != MODTYPE_STRSET)
			printf("\tchar hash%zu[64];\n", pos++);

	puts("");
	npos = pos = 1;
	TAILQ_FOREACH(ref, &up->mrq, entries) {
		if (ref->field->type == FTYPE_PASSWORD &&
		    ref->mod != MODTYPE_STRSET) {
			if ((ref->field->flags & FIELD_NULL))
				printf("if (v%zu != NULL)\n"
				       "\t", npos);
			gen_print_newpass
				((FIELD_NULL & ref->field->flags), 
				 pos, npos);
			pos++;
		} 
		npos++;
	}
	if (pos > 1)
		puts("");

	/* Bind parameters with special attention on hashing. */

	puts("\tmemset(parms, 0, sizeof(parms));");
	npos = pos = 1;
	TAILQ_FOREACH(ref, &up->mrq, entries) {
		tabs = 1;
		if ((ref->field->flags & FIELD_NULL)) {
			printf("\tif (v%zu == NULL)\n"
			       "\t\tparms[%zu].type = "
				"SQLBOX_PARM_NULL;\n"
			       "\telse {\n"
			       "\t", npos, npos - 1);
			tabs++;
		}
		if (ref->field->type == FTYPE_PASSWORD &&
		    ref->mod != MODTYPE_STRSET)
			update_gen_bindhash(npos, pos++, tabs);
		else
			update_gen_bindfunc
				(ref->field->type, npos,
				 (ref->field->flags & FIELD_NULL), 
				 tabs, OPTYPE_STREQ /* XXX */);
		if ((ref->field->flags & FIELD_NULL))
			puts("\t}");
		npos++;
	}
	TAILQ_FOREACH(ref, &up->crq, entries) {
		assert(ref->field->type != FTYPE_STRUCT);
		if (OPTYPE_ISUNARY(ref->op))
			continue;
		npos += update_gen_bindfunc
			(ref->field->type, npos, 0, 1, ref->op);
	}

	printf("\n"
	       "\tc = sqlbox_exec\n"
	       "\t\t(db, 0, STMT_%s_%s_%zu,\n"
	       "\t\t %zu, %s, SQLBOX_STMT_CONSTRAINT);\n"
	       "\tif (c == SQLBOX_CODE_ERROR)\n"
	       "\t\texit(EXIT_FAILURE);\n"
	       "\treturn (c == SQLBOX_CODE_OK) ? 1 : 0;\n"
	       "}\n"
	       "\n",
	       up->parent->cname, 
	       up->type == UP_MODIFY ? "UPDATE" : "DELETE",
	       num, parms, parms > 0 ? "parms" : "NULL");
}

/*
 * For the given validation field "v", generate the clause that results
 * in failure of the validation.
 * Assume that the basic type validation has passed.
 */
static void
gen_func_valid_types(const struct field *f, const struct fvalid *v)
{

	assert(v->type < VALIDATE__MAX);
	switch (f->type) {
	case FTYPE_BIT:
	case FTYPE_ENUM:
	case FTYPE_BITFIELD:
	case FTYPE_DATE:
	case FTYPE_EPOCH:
	case FTYPE_INT:
		printf("\tif (p->parsed.i %s %" PRId64 ")\n"
		       "\t\treturn 0;\n",
		       validbins[v->type], v->d.value.integer);
		break;
	case FTYPE_REAL:
		printf("\tif (p->parsed.d %s %g)\n"
		       "\t\treturn 0;\n",
		       validbins[v->type], v->d.value.decimal);
		break;
	default:
		printf("\tif (p->valsz %s %zu)\n"
		       "\t\treturn 0;\n",
		       validbins[v->type], v->d.value.len);
		break;
	}
}

/*
 * Generate the validation function for the given field.
 * This does not apply to structs.
 * It first validates the basic type (e.g., string or int), then runs
 * the custom validators.
 */
static void
gen_func_valids(const struct strct *p)
{
	const struct field	*f;
	const struct fvalid	*v;
	const struct eitem	*ei;

	TAILQ_FOREACH(f, &p->fq, entries) {
		if (f->type == FTYPE_STRUCT)
			continue;
		print_func_valid(f, 0);
		puts("{");
		if (NULL != validtypes[f->type])
			printf("\tif (!%s(p))\n"
			       "\t\treturn 0;\n",
			       validtypes[f->type]);

		/* Enumeration: check against knowns. */

		if (f->type == FTYPE_ENUM) {
			puts("\tswitch(p->parsed.i) {");
			TAILQ_FOREACH(ei, &f->eref->enm->eq, entries)
				printf("\tcase %" PRId64 ":\n",
					ei->value);
			puts("\t\tbreak;\n"
			     "\tdefault:\n"
			     "\t\treturn 0;\n"
			     "\t}");
		}

		TAILQ_FOREACH(v, &f->fvq, entries) 
			gen_func_valid_types(f, v);
		puts("\treturn 1;");
		puts("}\n");
	}
}

static void
gen_func_json_obj(const struct strct *p)
{

	print_func_json_obj(p, 0);
	printf("{\n"
	       "\tkjson_objp_open(r, \"%s\");\n"
	       "\tjson_%s_data(r, p);\n"
	       "\tkjson_obj_close(r);\n"
	       "}\n\n", p->name, p->name);

	if (STRCT_HAS_QUEUE & p->flags) {
		print_func_json_array(p, 0);
		printf("{\n"
		       "\tstruct %s *p;\n"
		       "\n"
		       "\tkjson_arrayp_open(r, \"%s_q\");\n"
		       "\tTAILQ_FOREACH(p, q, _entries) {\n"
		       "\t\tkjson_obj_open(r);\n"
		       "\t\tjson_%s_data(r, p);\n"
		       "\t\tkjson_obj_close(r);\n"
		       "\t}\n"
		       "\tkjson_array_close(r);\n"
		       "}\n\n", p->name, p->name, p->name);
	}

	if (STRCT_HAS_ITERATOR & p->flags) {
		print_func_json_iterate(p, 0);
		printf("{\n"
		       "\tstruct kjsonreq *r = arg;\n"
		       "\n"
		       "\tkjson_obj_open(r);\n"
		       "\tjson_%s_data(r, p);\n"
		       "\tkjson_obj_close(r);\n"
		       "}\n\n", p->name);
	}
}

/*
 * Export a field in a structure.
 * This needs to handle whether the field is a blob, might be null, is a
 * structure, and so on.
 */
static void
gen_field_json_data(const struct field *f, size_t *pos, int *sp)
{
	char		 tabs[] = "\t\t";
	struct roleset	*rs;
	int		 hassp = *sp;

	*sp = 0;

	if (FIELD_NOEXPORT & f->flags) {
		if ( ! hassp)
			puts("");
		print_commentv(1, COMMENT_C, "Omitting %s: "
			"marked no export.", f->name);
		puts("");
		*sp = 1;
		return;
	} else if (FTYPE_PASSWORD == f->type) {
		if ( ! hassp)
			puts("");
		print_commentv(1, COMMENT_C, "Omitting %s: "
			"is a password hash.", f->name);
		puts("");
		*sp = 1;
		return;
	}

	if (NULL != f->rolemap) {
		if ( ! hassp)
			puts("");
		puts("\tswitch (db_role_stored(p->priv_store)) {");
		TAILQ_FOREACH(rs, &f->rolemap->setq, entries)
			gen_role(rs->role);
		print_commentt(2, COMMENT_C, 
			"Don't export field to noted roles.");
		puts("\t\tbreak;\n"
		     "\tdefault:");
		*sp = 1;
	} else
		tabs[1] = '\0';

	if (FTYPE_STRUCT != f->type) {
		if (FIELD_NULL & f->flags) {
			if ( ! hassp && ! *sp)
				puts("");
			printf("%sif (!p->has_%s)\n"
			       "%s\tkjson_putnullp(r, \"%s\");\n"
			       "%selse\n"
			       "%s\t", tabs, f->name, tabs, 
			       f->name, tabs, tabs);
		} else
			printf("%s", tabs);
		if (FTYPE_BLOB == f->type)
			printf("%s(r, \"%s\", buf%zu);\n",
				puttypes[f->type], 
				f->name, ++(*pos));
		else
			printf("%s(r, \"%s\", p->%s);\n", 
				puttypes[f->type], 
				f->name, f->name);
		if (FIELD_NULL & f->flags && ! *sp) {
			puts("");
			*sp = 1;
		}
	} else if (FIELD_NULL & f->ref->source->flags) {
		if ( ! hassp && ! *sp)
			puts("");
		printf("%sif (p->has_%s) {\n"
		       "%s\tkjson_objp_open(r, \"%s\");\n"
		       "%s\tjson_%s_data(r, &p->%s);\n"
		       "%s\tkjson_obj_close(r);\n"
		       "%s} else\n"
		       "%s\tkjson_putnullp(r, \"%s\");\n",
			tabs, f->name, tabs, f->name,
			tabs, f->ref->tstrct, f->name, 
			tabs, tabs, tabs, f->name);
		if ( ! *sp) {
			puts("");
			*sp = 1;
		}
	} else
		printf("%skjson_objp_open(r, \"%s\");\n"
		       "%sjson_%s_data(r, &p->%s);\n"
		       "%skjson_obj_close(r);\n",
			tabs, f->name, tabs, 
			f->ref->tstrct, f->name, tabs);

	if (NULL != f->rolemap) {
		puts("\t\tbreak;\n"
		     "\t}\n");
		*sp = 1;
	}
}

#if 0
/*
 * Count the maximum number of JSON tokens that can be exported by any
 * given structure.
 * Each key-pair has two, the whole object has one, and each sub-struct
 * is recursively examined.
 */
static size_t
count_json_tokens_r(const struct strct *p)
{
	const struct field *f;
	size_t	 tok = 0;

	TAILQ_FOREACH(f, &p->fq, entries)
		if ( ! (FIELD_NOEXPORT & f->flags) &&
		    FTYPE_STRUCT == f->type)
			tok += count_json_tokens_r
				(f->ref->target->parent);
		else if ( ! (FIELD_NOEXPORT & f->flags))
			tok += 2;

	return 1 + tok;
}
#endif

static void
gen_func_json_parse(const struct strct *p)
{
	int		 hasenum = 0, hasstruct = 0, hasblob = 0;
	const struct field *f;

	/* Whether we need conversion space. */

	TAILQ_FOREACH(f, &p->fq, entries) {
		if (FIELD_NOEXPORT & f->flags)
			continue;
		if (FTYPE_ENUM == f->type) 
			hasenum = 1;
		else if (FTYPE_BLOB == f->type) 
			hasblob = 1;
		else if (FTYPE_STRUCT == f->type) 
			hasstruct = 1;
	}

	print_func_json_parse(p, 0);
	puts("{\n"
	     "\tint i;\n"
	     "\tsize_t j;");
	if (hasenum)
		puts("\tint64_t tmpint;");
	if (hasblob || hasstruct)
		puts("\tint rc;");
	if (hasblob)
		puts("\tchar *tmpbuf;");

	puts("\n"
	     "\tif (toksz < 1 || t[0].type != JSMN_OBJECT)\n"
	     "\t\treturn 0;\n"
	     "\n"
	     "\tfor (i = 0, j = 0; i < t[0].size; i++) {");

	TAILQ_FOREACH(f, &p->fq, entries) {
		if (FIELD_NOEXPORT & f->flags)
			continue;
		printf("\t\tif (jsmn_eq(buf, &t[j+1], \"%s\")) {\n"
		       "\t\t\tj++;\n", f->name);

		/* Check correct kind of token. */

		if (FIELD_NULL & f->flags)
			printf("\t\t\tif (t[j+1].type == "
				"JSMN_PRIMITIVE &&\n"
			       "\t\t\t    \'n\' == buf[t[j+1].start]) {\n"
			       "\t\t\t\tp->has_%s = 0;\n"
			       "\t\t\t\tj++;\n"
			       "\t\t\t\tcontinue;\n"
			       "\t\t\t} else\n"
			       "\t\t\t\tp->has_%s = 1;\n",
			       f->name, f->name);

		switch (f->type) {
		case FTYPE_BIT:
		case FTYPE_BITFIELD:
		case FTYPE_DATE:
		case FTYPE_ENUM:
		case FTYPE_EPOCH:
		case FTYPE_INT:
		case FTYPE_REAL:
			puts("\t\t\tif (t[j+1].type != "
			      "JSMN_PRIMITIVE ||\n"
			     "\t\t\t    (\'-\' != buf[t[j+1].start] &&\n"
			     "\t\t\t    ! isdigit((unsigned int)"
			      "buf[t[j+1].start])))\n"
			     "\t\t\t\treturn 0;");
			break;
		case FTYPE_BLOB:
		case FTYPE_TEXT:
		case FTYPE_PASSWORD:
		case FTYPE_EMAIL:
			puts("\t\t\tif (t[j+1].type != JSMN_STRING)\n"
			     "\t\t\t\treturn 0;");
			break;
		case FTYPE_STRUCT:
			puts("\t\t\tif (t[j+1].type != JSMN_OBJECT)\n"
			     "\t\t\t\treturn 0;");
			break;
		default:
			abort();
		}

		switch (f->type) {
		case FTYPE_BIT:
		case FTYPE_BITFIELD:
		case FTYPE_DATE:
		case FTYPE_EPOCH:
		case FTYPE_INT:
			printf("\t\t\tif (!jsmn_parse_int("
				"buf + t[j+1].start,\n"
			       "\t\t\t    t[j+1].end - t[j+1].start, "
			        "&p->%s))\n"
			       "\t\t\t\treturn 0;\n"
			       "\t\t\tj++;\n",
			       f->name);
			break;
		case FTYPE_ENUM:
			printf("\t\t\tif (!jsmn_parse_int("
				"buf + t[j+1].start,\n"
			       "\t\t\t    t[j+1].end - t[j+1].start, "
			        "&tmpint))\n"
			       "\t\t\t\treturn 0;\n"
			       "\t\t\tp->%s = tmpint;\n"
			       "\t\t\tj++;\n",
			       f->name);
			break;
		case FTYPE_REAL:
			printf("\t\t\tif (!jsmn_parse_real("
				"buf + t[j+1].start,\n"
			       "\t\t\t    t[j+1].end - t[j+1].start, "
			        "&p->%s))\n"
			       "\t\t\t\treturn 0;\n"
			       "\t\t\tj++;\n",
			       f->name);
			break;
		case FTYPE_BLOB:
			printf("\t\t\ttmpbuf = strndup\n"
			       "\t\t\t\t(buf + t[j+1].start,\n"
			       "\t\t\t\t t[j+1].end - t[j+1].start);\n"
			       "\t\t\tif (tmpbuf == NULL)\n"
			       "\t\t\t\treturn -1;\n"
			       "\t\t\tp->%s = malloc((t[j+1].end - "
			       	"t[j+1].start) + 1);\n"
			       "\t\t\tif (p->%s == NULL) {\n"
			       "\t\t\t\tfree(tmpbuf);\n"
			       "\t\t\t\treturn -1;\n"
			       "\t\t\t}\n"
			       "\t\t\trc = b64_pton(tmpbuf, p->%s,\n"
			       "\t\t\t\t(t[j+1].end - t[j+1].start) + 1);\n"
			       "\t\t\tfree(tmpbuf);\n"
			       "\t\t\tif (rc < 0)\n"
			       "\t\t\t\treturn -1;\n"
			       "\t\t\tp->%s_sz = rc;\n"
			       "\t\t\tj++;\n",
			       f->name, f->name, f->name, f->name);
			break;
		case FTYPE_TEXT:
		case FTYPE_PASSWORD:
		case FTYPE_EMAIL:
			printf("\t\t\tp->%s = strndup\n"
			       "\t\t\t\t(buf + t[j+1].start,\n"
			       "\t\t\t\t t[j+1].end - t[j+1].start);\n"
			       "\t\t\tif (p->%s == NULL)\n"
			       "\t\t\t\treturn -1;\n"
			       "\t\t\tj++;\n",
			       f->name, f->name);
			break;
		case FTYPE_STRUCT:
			printf("\t\t\trc = jsmn_%s\n"
			       "\t\t\t\t(&p->%s, buf,\n"
			       "\t\t\t\t &t[j+1], toksz - j);\n"
			       "\t\t\tif (rc <= 0)\n"
			       "\t\t\t\treturn rc;\n"
			       "\t\t\tj += rc;\n",
			       f->ref->target->parent->name,
			       f->name);
			break;
		default:
			abort();
		}
		printf("\t\t\tcontinue;\n"
		       "\t\t}\n");
	}

	puts("");
	print_commentt(2, COMMENT_C,
		"Anything else is unexpected.");

	puts("\n"
	     "\t\treturn 0;\n"
	     "\t}\n"
	     "\treturn j+1;\n"
	     "}\n"
	     "");

	print_func_json_clear(p, 0);
	puts("\n"
	     "{\n"
	     "\tif (p == NULL)\n"
	     "\t\treturn;");
	TAILQ_FOREACH(f, &p->fq, entries)
		switch(f->type) {
		case (FTYPE_BLOB):
		case (FTYPE_PASSWORD):
		case (FTYPE_TEXT):
		case (FTYPE_EMAIL):
			printf("\tfree(p->%s);\n", f->name);
			break;
		case (FTYPE_STRUCT):
			if (FIELD_NULL & f->ref->source->flags)
				printf("\tif (p->has_%s)\n"
				       "\t\tjsmn_%s_clear(&p->%s);\n",
					f->ref->source->name, 
					f->ref->tstrct, f->name);
			else
				printf("\tjsmn_%s_clear(&p->%s);\n",
					f->ref->tstrct, f->name);
			break;
		default:
			break;
		}
	puts("}\n"
	     "");

	print_func_json_free_array(p, 0);
	printf("{\n"
	       "\tsize_t i;\n"
	       "\tfor (i = 0; i < sz; i++)\n"
	       "\t\tjsmn_%s_clear(&p[i]);\n"
	       "\tfree(p);\n"
	       "}\n"
	       "\n", p->name);

	print_func_json_parse_array(p, 0);
	printf("{\n"
	       "\tsize_t i, j;\n"
	       "\tint rc;\n"
	       "\n"
	       "\t*sz = 0;\n"
	       "\t*p = NULL;\n"
	       "\n"
	       "\tif (toksz < 1 || t[0].type != JSMN_ARRAY)\n"
	       "\t\treturn 0;\n"
	       "\n"
	       "\t*sz = t[0].size;\n"
	       "\tif ((*p = calloc(*sz, sizeof(struct %s))) == NULL)\n"
	       "\t\treturn -1;\n"
	       "\n"
	       "\tfor (i = j = 0; i < *sz; i++) {\n"
	       "\t\trc = jsmn_%s(&(*p)[i], buf, &t[j+1], toksz - j);\n"
	       "\t\tif (rc <= 0)\n"
	       "\t\t\treturn rc;\n"
	       "\t\tj += rc;\n"
	       "\t}\n"
	       "\treturn j + 1;\n"
	       "}\n"
	       "\n", p->name, p->name);
}

static void
gen_func_json_data(const struct strct *p)
{
	const struct field *f;
	size_t	 pos;
	int	 sp;

	print_func_json_data(p, 0);
	puts("\n"
	     "{");

	/* 
	 * Declare our base64 buffers. 
	 * FIXME: have the buffer only be allocated if we have the value
	 * being serialised (right now it's allocated either way).
	 */

	pos = 0;
	TAILQ_FOREACH(f, &p->fq, entries)
		if (FTYPE_BLOB == f->type &&
		    ! (FIELD_NOEXPORT & f->flags)) 
			printf("\tchar *buf%zu;\n", ++pos);

	if (pos > 0) {
		puts("\tsize_t sz;\n");
		print_commentt(1, COMMENT_C,
			"We need to base64 encode the binary "
			"buffers prior to serialisation.\n"
			"Allocate space for these buffers and do "
			"so now.\n"
			"We\'ll free the buffers at the epilogue "
			"of the function.");
		puts("");
	}

	pos = 0;
	TAILQ_FOREACH(f, &p->fq, entries) {
		if (FTYPE_BLOB != f->type || 
		    FIELD_NOEXPORT & f->flags)
			continue;
		pos++;
		printf("\tsz = (p->%s_sz + 2) / 3 * 4 + 1;\n"
		       "\tbuf%zu = malloc(sz);\n"
		       "\tif (buf%zu == NULL) {\n"
		       "\t\tperror(NULL);\n"
		       "\t\texit(EXIT_FAILURE);\n"
		       "\t}\n", f->name, pos, pos);
		if (FIELD_NULL & f->flags)
			printf("\tif (p->has_%s)\n"
			       "\t", f->name);
		printf("\tb64_ntop(p->%s, p->%s_sz, buf%zu, sz);\n",
		       f->name, f->name, pos);
	}

	if ((sp = (pos > 0)))
		puts("");

	pos = 0;
	TAILQ_FOREACH(f, &p->fq, entries)
		gen_field_json_data(f, &pos, &sp);

	/* Free our temporary base64 buffers. */

	pos = 0;
	TAILQ_FOREACH(f, &p->fq, entries) {
		if (FIELD_NOEXPORT & f->flags)
			continue;
		if (FTYPE_BLOB == f->type && 0 == pos)
			puts("");
		if (FTYPE_BLOB == f->type) 
			printf("\tfree(buf%zu);\n", ++pos);
	}

	puts("}\n"
	     "");
}

/*
 * Generate all of the functions we've defined in our header for the
 * given structure "s".
 */
static void
gen_funcs(const struct config *cfg, const struct strct *p, 
	int json, int jsonparse, int valids, int dbin)
{
	const struct search *s;
	const struct update *u;
	size_t	 pos;

	if (dbin) {
		gen_func_fill(cfg, p);
		gen_func_fill_r(cfg, p);
		gen_func_unfill(cfg, p);
		gen_func_unfill_r(p);
		gen_func_reffind(cfg, p);
		gen_func_free(p);
		gen_func_freeq(p);
		gen_func_insert(cfg, p);
	}

	if (json) {
		gen_func_json_data(p);
		gen_func_json_obj(p);
	}

	if (jsonparse) 
		gen_func_json_parse(p);

	if (valids)
		gen_func_valids(p);

	if ( ! dbin)
		return;

	pos = 0;
	TAILQ_FOREACH(s, &p->sq, entries)
		if (s->type == STYPE_SEARCH)
			gen_strct_func_srch(cfg, s, pos++);
		else if (s->type == STYPE_LIST)
			gen_strct_func_list(cfg, s, pos++);
		else if (s->type == STYPE_COUNT)
			gen_strct_func_count(cfg, s, pos++);
		else
			gen_strct_func_iter(cfg, s, pos++);

	pos = 0;
	TAILQ_FOREACH(u, &p->uq, entries)
		gen_func_update(cfg, u, pos++);
	pos = 0;
	TAILQ_FOREACH(u, &p->dq, entries)
		gen_func_update(cfg, u, pos++);
}

/*
 * Generate a set of statements as an enumeration that will be used for
 * this structure.
 * Does not print the opening "enum xxx {" parts or trailing.
 */
static void
gen_enum(const struct strct *p)
{
	const struct search	*s;
	const struct update	*u;
	const struct field	*f;
	size_t			 pos;

	TAILQ_FOREACH(f, &p->fq, entries)
		if ((f->flags & (FIELD_UNIQUE|FIELD_ROWID)))
			printf("\tSTMT_%s_BY_UNIQUE_%s,\n", 
				p->cname, f->name);

	pos = 0;
	TAILQ_FOREACH(s, &p->sq, entries)
		printf("\tSTMT_%s_BY_SEARCH_%zu,\n", p->cname, pos++);

	if (p->ins != NULL)
		printf("\tSTMT_%s_INSERT,\n", p->cname);

	pos = 0;
	TAILQ_FOREACH(u, &p->uq, entries)
		printf("\tSTMT_%s_UPDATE_%zu,\n", p->cname, pos++);

	pos = 0;
	TAILQ_FOREACH(u, &p->dq, entries)
		printf("\tSTMT_%s_DELETE_%zu,\n", p->cname, pos++);
}

/*
 * Recursively generate a series of DB_SCHEMA_xxx statements for getting
 * data on a structure.
 * This will specify the schema of the top-level structure (pname is
 * NULL), then generate aliased schemas for all of the recursive
 * structures.
 * It breaks lines depending upon the output position in "col".
 * See gen_stmt_joins().
 */
static void
gen_stmt_schema(const struct strct *orig, int first,
	const struct strct *p, const char *pname, size_t *col)
{
	const struct field *f;
	const struct alias *a = NULL;
	int	 	    c;
	char		   *name = NULL;

	*col += printf("\"%s ", !first ? ",\"" : "");
	if (!first && *col >= 72) {
		printf("\n\t\t");
		*col = 16;
	}

	/* 
	 * If applicable, looks up our alias and emit it as the alias
	 * for the table.
	 * Otherwise, use the table name itself.
	 */

	if (pname != NULL) {
		TAILQ_FOREACH(a, &orig->aq, entries)
			if (strcasecmp(a->name, pname) == 0)
				break;
		assert(a != NULL);
		*col += printf("DB_SCHEMA_%s(%s) ", p->cname, a->alias);
	} else
		*col += printf("DB_SCHEMA_%s(%s) ", p->cname, p->name);

	/*
	 * Recursive step.
	 * Search through all of our fields for structures.
	 * If we find them, build up the canonical field reference and
	 * descend.
	 */

	TAILQ_FOREACH(f, &p->fq, entries) {
		if (f->type != FTYPE_STRUCT ||
		    (f->ref->source->flags & FIELD_NULL))
			continue;

		if (pname != NULL) {
			c = asprintf(&name, "%s.%s", pname, f->name);
			if (c < 0)
				err(EXIT_FAILURE, NULL);
		} else if ((name = strdup(f->name)) == NULL)
			err(EXIT_FAILURE, "strdup");
		gen_stmt_schema(orig, 0,
			f->ref->target->parent, name, col);
		free(name);
	}
}

/*
 * Recursively generate a series of INNER JOIN statements for any
 * structure object.
 * If the structure object has no inner nested components, this will not
 * do anything.
 * See gen_stmt_schema().
 */
static void
gen_stmt_joins(const struct strct *orig, const struct strct *p, 
	const struct alias *parent, size_t *count)
{
	const struct field *f;
	const struct alias *a;
	int	 c;
	char	*name;

	TAILQ_FOREACH(f, &p->fq, entries) {
		if (FTYPE_STRUCT != f->type ||
		    FIELD_NULL & f->ref->source->flags)
			continue;

		if (NULL != parent) {
			c = asprintf(&name, "%s.%s", 
				parent->name, f->name);
			if (c < 0)
				err(EXIT_FAILURE, NULL);
		} else if (NULL == (name = strdup(f->name)))
			err(EXIT_FAILURE, NULL);

		TAILQ_FOREACH(a, &orig->aq, entries)
			if (0 == strcasecmp(a->name, name))
				break;
		assert(NULL != a);

		if (0 == *count)
			printf(" \"");

		(*count)++;
		printf("\n\t\t\"INNER JOIN %s AS %s ON %s.%s=%s.%s \"",
			f->ref->tstrct, a->alias,
			a->alias, f->ref->tfield,
			NULL == parent ? p->name : parent->alias,
			f->ref->sfield);
		gen_stmt_joins(orig, 
			f->ref->target->parent, a, count);
		free(name);
	}
}

/*
 * Fill in the statements noted in gen_enum().
 * This function is an important one because we're going to create the
 * raw SQL that's passed into sqlbox.
 */
static void
gen_stmt(const struct strct *p)
{
	const struct search *s;
	const struct sent   *sent;
	const struct sref   *sr;
	const struct field  *f, *ff;
	const struct update *up;
	const struct uref   *ur;
	const struct ord    *ord;
	int		     first, hastrail, needquot;
	size_t		     pos, rc, col;

	/* 
	 * We have a special query just for our unique fields.
	 * These are generated in the event of null foreign key
	 * reference lookups with the generated db_xxx_reffind()
	 * functions.
	 * TODO: figure out which ones we should be generating and only
	 * do this, as otherwise we're just wasting static space.
	 */

	TAILQ_FOREACH(f, &p->fq, entries) 
		if ((f->flags & (FIELD_ROWID|FIELD_UNIQUE))) {
			printf("\t/* STMT_%s_BY_UNIQUE_%s */\n"
			       "\t\"SELECT ", p->cname, f->name);
			col = 16;
			gen_stmt_schema(p, 1, p, NULL, &col);
			printf("\" FROM %s", p->name);
			rc = 0;
			gen_stmt_joins(p, p, NULL, &rc);
			if (rc > 0)
				printf("\n\t\t\"");
			else
				printf(" ");
			printf("WHERE %s.%s = ?\",\n",
				p->name, f->name);
		}

	/* 
	 * Print custom search queries.
	 * This also uses the recursive selection.
	 */

	pos = 0;
	TAILQ_FOREACH(s, &p->sq, entries) {
		printf("\t/* STMT_%s_BY_SEARCH_%zu */\n"
		       "\t\"SELECT ", p->cname, pos++);
		col = 16;
		needquot = 0;

		/* 
		 * Juggle around the possibilities of...
		 *   select count(*)
		 *   select count(distinct --gen_stmt_schema-- )
		 *   select --gen_stmt_schema--
		 */

		if (s->type == STYPE_COUNT)
			col += printf("COUNT(");
		if (s->dst) {
			col += printf("DISTINCT ");
			gen_stmt_schema(p, 1, s->dst->strct, 
				s->dst->cname, &col);
			needquot = 1;
		} else if (s->type != STYPE_COUNT) {
			gen_stmt_schema(p, 1, p, NULL, &col);
			needquot = 1;
		} else
			printf("*");

		if (needquot)
			putchar('"');
		if (s->type == STYPE_COUNT)
			putchar(')');

		printf(" FROM %s", p->name);

		/* 
		 * Whether anything is coming after the "FROM" clause,
		 * which includes all ORDER, WHERE, GROUP, LIMIT, and
		 * OFFSET commands.
		 */

		hastrail = 
			(s->aggr != NULL && s->group != NULL) ||
			(!TAILQ_EMPTY(&s->sntq)) ||
			(!TAILQ_EMPTY(&s->ordq)) ||
			(s->type != STYPE_SEARCH && s->limit > 0) ||
			(s->type != STYPE_SEARCH && s->offset > 0);
		
		rc = 0;
		gen_stmt_joins(p, p, NULL, &rc);

		/* 
		 * We need to have a special JOIN command for aggregate
		 * groupings: we LEFT OUTER JOIN the grouped set to
		 * itself, conditioning upon the aggregate inequality.
		 * We'll filter NULL joinings in the WHERE statement.
		 */

		if (NULL != s->aggr && NULL != s->group) {
			f = TAILQ_LAST(&s->aggr->arq, srefq)->field;
			assert(NULL != f);
			ff = TAILQ_LAST(&s->group->grq, srefq)->field;
			assert(NULL != ff);
			assert(f->parent == ff->parent);
			printf("\n\t\t\"LEFT OUTER JOIN %s as _custom "
				"ON %s.%s = _custom.%s "
				"AND %s.%s %s _custom.%s \"",
				ff->parent->name, 
				NULL == s->group->alias ?
				ff->parent->name : 
				s->group->alias->alias,
				ff->name, ff->name,
				NULL == s->group->alias ?
				ff->parent->name : 
				s->group->alias->alias, f->name, 
				AGGR_MAXROW == s->aggr->op ?  "<" : ">",
				f->name);
		}

		if ( ! hastrail) {
			if (0 == rc)
				putchar('"');
			puts(",");
			continue;
		}

		if (rc > 0)
			printf("\n\t\t\"");
		else
			printf(" \"\n\t\t\"");

		if ( ! TAILQ_EMPTY(&s->sntq) || 
		    (NULL != s->aggr && NULL != s->group))
			printf("WHERE");

		first = 1;

		/* 
		 * If we're grouping, filter out all of the joins that
		 * failed and aren't part of the results.
		 */

		if (NULL != s->group) {
			f = TAILQ_LAST(&s->group->grq, srefq)->field;
			assert(NULL != f);
			printf(" _custom.%s IS NULL", f->name);
			first = 0;
		}

		/* Continue with our proper WHERE clauses. */

		TAILQ_FOREACH(sent, &s->sntq, entries) {
			sr = TAILQ_LAST(&sent->srq, srefq);
			if (sr->field->type == FTYPE_PASSWORD &&
			    sent->op != OPTYPE_STREQ &&
			    sent->op != OPTYPE_STRNEQ)
				continue;
			if (!first)
				printf(" AND");
			first = 0;
			if (OPTYPE_ISUNARY(sent->op))
				printf(" %s.%s %s",
					sent->alias == NULL ?
					p->name : sent->alias->alias,
					sr->name, optypes[sent->op]);
			else
				printf(" %s.%s %s ?", 
					sent->alias == NULL ?
					p->name : sent->alias->alias,
					sr->name, optypes[sent->op]);
		}

		first = 1;
		if (!TAILQ_EMPTY(&s->ordq))
			printf(" ORDER BY ");
		TAILQ_FOREACH(ord, &s->ordq, entries) {
			sr = TAILQ_LAST(&ord->orq, srefq);
			if ( ! first)
				printf(", ");
			first = 0;
			printf("%s.%s %s",
				NULL == ord->alias ?
				p->name : ord->alias->alias,
				sr->name, 
				ORDTYPE_ASC == ord->op ?
				"ASC" : "DESC");
		}

		if (STYPE_SEARCH != s->type && s->limit > 0)
			printf(" LIMIT %" PRId64, s->limit);
		if (STYPE_SEARCH != s->type && s->offset > 0)
			printf(" OFFSET %" PRId64, s->offset);

		puts("\",");
	}

	/* Insertion of a new record. */

	if (p->ins != NULL) {
		printf("\t/* STMT_%s_INSERT */\n", p->cname);
		col = printf("\t\"INSERT INTO %s ", p->name);

		first = 1;
		TAILQ_FOREACH(f, &p->fq, entries) {
			if (f->type == FTYPE_STRUCT ||
			    (f->flags & FIELD_ROWID))
				continue;
			if (col >= 72) {
				printf("%s\"\n\t\t\"%s", 
					first ? "" : ",",
					first ? "(" : " ");
				col = 16;
			} else
				putchar(first ? '(' : ',');
			col += 1 + printf("%s", f->name);
			first = 0;
		}

		if (first == 0) {
			if ((col += printf(") ")) >= 72) {
				printf("\"\n\t\t\"");
				col = 17;
			}
			col += printf("VALUES ");
			first = 1;
			TAILQ_FOREACH(f, &p->fq, entries) {
				if (f->type == FTYPE_STRUCT ||
				    (f->flags & FIELD_ROWID))
					continue;
				if (col >= 72) {
					printf("%s\"\n\t\t\"%s", 
						first ? "" : ",",
						first ? "(" : " ");
					col = 16;
				} else
					putchar(first ? '(' : ',');
				putchar('?');
				col += 2;
				first = 0;
			}
			puts(")\",");
		} else
			puts("DEFAULT VALUES\",");
	}
	
	/* 
	 * Custom update queries. 
	 * Our updates can have modifications where they modify the
	 * given field (instead of setting it externally).
	 */

	pos = 0;
	TAILQ_FOREACH(up, &p->uq, entries) {
		printf("\t/* STMT_%s_UPDATE_%zu */\n"
		       "\t\"UPDATE %s SET",
		       p->cname, pos++, p->name);
		first = 1;
		TAILQ_FOREACH(ur, &up->mrq, entries) {
			putchar(first ? ' ' : ',');
			first = 0;
			switch (ur->mod) {
			case MODTYPE_INC:
				printf("%s = %s + ?", 
					ur->name, ur->name);
				break;
			case MODTYPE_DEC:
				printf("%s = %s - ?", 
					ur->name, ur->name);
				break;
			case MODTYPE_CONCAT:
				printf("%s = ", ur->name);

				/*
				 * If we concatenate a NULL with a
				 * non-NULL, we'll always get a NULL
				 * value, which isn't what we want.
				 * This will wrap possibly-null values
				 * so that they're always strings.
				 */

				if ((ur->field->flags & FIELD_NULL))
					printf("COALESCE(%s,'')", ur->name);
				else
					printf("%s", ur->name);
				printf(" || ?");
				break;
			default:
				printf("%s = ?", ur->name);
				break;
			}
		}
		first = 1;
		TAILQ_FOREACH(ur, &up->crq, entries) {
			printf(" %s ", first ? "WHERE" : "AND");
			if (OPTYPE_ISUNARY(ur->op))
				printf("%s %s", ur->name, 
					optypes[ur->op]);
			else
				printf("%s %s ?", ur->name,
					optypes[ur->op]);
			first = 0;
		}
		puts("\",");
	}

	/* Custom delete queries. */

	pos = 0;
	TAILQ_FOREACH(up, &p->dq, entries) {
		printf("\t/* STMT_%s_DELETE_%zu */\n"
		       "\t\"DELETE FROM %s",
		       p->cname, pos++, p->name);
		first = 1;
		TAILQ_FOREACH(ur, &up->crq, entries) {
			printf(" %s ", first ? "WHERE" : "AND");
			if (OPTYPE_ISUNARY(ur->op))
				printf("%s %s", ur->name, 
					optypes[ur->op]);
			else
				printf("%s %s ?", ur->name,
					optypes[ur->op]);
			first = 0;
		}
		puts("\",");
	}
}

/*
 * Generate a single "struct kvalid" with the given validation function
 * and the form name, which we have as "struct-field".
 */
static void
gen_valid_struct(const struct strct *p)
{
	const struct field *f;

	TAILQ_FOREACH(f, &p->fq, entries) {
		if (FTYPE_STRUCT == f->type)
			continue;
		printf("\t{ valid_%s_%s, \"%s-%s\" },\n",
			p->name, f->name,
			p->name, f->name);
	}
}

static int
genfile(const char *file, int fd)
{
	ssize_t	 ssz;
	char	 buf[BUFSIZ];

	print_commentv(0, COMMENT_C,
		"File imported from %s.", file);

	while ((ssz = read(fd, buf, sizeof(buf))) > 0)
		fwrite(buf, 1, ssz, stdout);

	if (ssz < 0)
		warn("%s", file);

	return 0 == ssz;
}

/*
 * Generate the schema for a given table.
 * This macro accepts a single parameter that's given to all of the
 * members so that a later SELECT can use INNER JOIN xxx AS yyy and have
 * multiple joins on the same table.
 */
static void
gen_define_schema(const struct strct *p)
{
	const struct field *f;
	const char *s = "";

	printf("#define DB_SCHEMA_%s(_x) \\", p->cname);
	TAILQ_FOREACH(f, &p->fq, entries) {
		if (FTYPE_STRUCT == f->type)
			continue;
		puts(s);
		printf("\t#_x \".%s\"", f->name);
		s = " \",\" \\";
	}
	puts("");
}

/*
 * Generate the C source file from "cfg"s structure objects.
 * Returns TRUE on success, FALSE on failure (memory allocation failure
 * or failure to open a template file).
 */
static int
gen_c_source(const struct config *cfg, int json, int jsonparse,
	int valids, int dbin, const char *header, 
	const char *incls, const int *exs)
{
	const struct strct *p;
	const char	*start;
	size_t		 sz;
	int		 need_kcgi = 0, 
			 need_kcgijson = 0, 
			 need_sqlbox = 0;
#if HAVE_B64_NTOP
	int		 need_b64 = 0;
#else
	int		 need_b64 = 1;
#endif

	if (incls == NULL)
		incls = "";

	print_commentv(0, COMMENT_C, 
		"WARNING: automatically generated by "
		"%s " VERSION ".\n"
		"DO NOT EDIT!", getprogname());

#ifndef __OpenBSD__
	puts("#define _GNU_SOURCE /* strndup */\n"
	     "#define _XOPEN_SOURCE /* crypt */\n"
	     "");
#endif

	/* Start with all headers we'll need. */

	/* FIXME: HAVE_SYS_QUEUE pulled in from compat. */

	if (need_b64)
		puts("#include <sys/types.h> /* b64_ntop() */");

	puts("#include <sys/queue.h>\n"
	     "\n"
	     "#include <assert.h>");

	if ( ! need_b64) {
		TAILQ_FOREACH(p, &cfg->sq, entries) 
			if (STRCT_HAS_BLOB & p->flags) {
				print_commentt(0, COMMENT_C,
					"Required for b64_ntop().");
				if ( ! jsonparse)
					puts("#include <ctype.h>");
				puts("#include <netinet/in.h>\n"
				     "#include <resolv.h>");
				break;
			}
	} else
		puts("#include <ctype.h> /* b64_ntop() */");

	if (dbin || strchr(incls, 'd'))
		need_sqlbox = 1;
	if (valids || strchr(incls, 'v'))
		need_kcgi = 1;
	if (json || strchr(incls, 'j'))
		need_kcgi = need_kcgijson = 1;

	if (jsonparse) {
		if ( ! need_b64)
			puts("#include <ctype.h>");
		puts("#include <inttypes.h>");
	}

	if (need_kcgi)
		puts("#include <stdarg.h>");

	puts("#include <stdio.h>\n"
	     "#include <stdint.h> /* int64_t */\n"
	     "#include <stdlib.h>\n"
	     "#include <string.h>\n"
	     "#include <time.h> /* _XOPEN_SOURCE and gmtime_r()*/\n"
	     "#include <unistd.h>\n"
	     "");

	if (need_sqlbox)
		puts("#include <sqlbox.h>");
	if (need_kcgi)
		puts("#include <kcgi.h>");
	if (need_kcgijson)
		puts("#include <kcgijson.h>");

	if (NULL == header)
		header = "db.h";

	puts("");
	while ('\0' != *header) {
		while (isspace((unsigned char)*header))
			header++;
		if ('\0' == *header)
			continue;
		start = header;
		for (sz = 0; '\0' != *header; sz++, header++)
			if (',' == *header ||
			    isspace((unsigned char)*header))
				break;
		if (sz)
			printf("#include \"%.*s\"\n", (int)sz, start);
		while (',' == *header)
			header++;
	}
	puts("");

#ifndef __OpenBSD__
	if ( ! genfile(FILE_GENSALT, exs[EX_GENSALT]))
		return 0;
#endif
	if (need_b64 && ! genfile(FILE_B64_NTOP, exs[EX_B64_NTOP]))
		return 0;
	if (jsonparse && ! genfile(FILE_JSMN, exs[EX_JSMN]))
		return 0;

	if (dbin) {
		print_commentt(0, COMMENT_C,
			"All SQL statements we'll later "
			"define in \"stmts\".");
		puts("enum\tstmt {");
		TAILQ_FOREACH(p, &cfg->sq, entries)
			gen_enum(p);
		puts("\tSTMT__MAX\n"
		     "};\n"
		     "");

		print_commentt(0, COMMENT_C,
			"Definition of our opaque \"ort\", "
			"which contains role information.");
		puts("struct\tort {");
		print_commentt(1, COMMENT_C,
			"Hidden database connection");
		puts("\tstruct sqlbox *db;");

		if (!TAILQ_EMPTY(&cfg->rq)) {
			print_commentt(1, COMMENT_C,
				"Current RBAC role.");
			puts("\tenum ort_role role;\n"
			     "};\n");
			print_commentt(0, COMMENT_C,
				"A saved role state attached to "
				"generated objects.\n"
				"We'll use this to make sure that "
				"we shouldn't export data that "
				"we've kept unexported in a given "
				"role (at the time of acquisition).");
			puts("struct\tort_store {");
			print_commentt(1, COMMENT_C,
				"Role at the time of acquisition.");
			puts("\tenum ort_role role;");
		}

		puts("};\n");

		print_commentt(0, COMMENT_C,
			"Define our table columns.\n"
			"Since we're using roles, this is "
			"all internal to the source and not "
			"exported.");
		TAILQ_FOREACH(p, &cfg->sq, entries)
			gen_define_schema(p);
		puts("");

		print_commentt(0, COMMENT_C,
			"Our full set of SQL statements.\n"
			"We define these beforehand because "
			"that's how sqlbox(3) handles statement "
			"generation.\n"
			"Notice the \"AS\" part: this allows "
			"for multiple inner joins without "
			"ambiguity.");
		puts("static\tconst char *const stmts[STMT__MAX] = {");
		TAILQ_FOREACH(p, &cfg->sq, entries)
			gen_stmt(p);
		puts("};");
		puts("");
	}

	/*
	 * Validation array.
	 * This is declared in the header file, but we define it now.
	 * All of the functions have been defined in the header file.
	 */

	if (valids) {
		puts("const struct kvalid valid_keys[VALID__MAX] = {");
		TAILQ_FOREACH(p, &cfg->sq, entries)
			gen_valid_struct(p);
		puts("};\n"
		     "");
	}

	/* Define our functions. */

	print_commentt(0, COMMENT_C,
		"Finally, all of the functions we'll use.");
	puts("");

	if (dbin) {
		gen_func_trans(cfg);
		if (!gen_func_open(cfg))
			return 0;
		gen_func_close(cfg);
		if (!TAILQ_EMPTY(&cfg->rq))
			gen_func_role_transitions(cfg);
	}

	TAILQ_FOREACH(p, &cfg->sq, entries)
		gen_funcs(cfg, p, json, jsonparse, valids, dbin);

	return 1;
}

int
main(int argc, char *argv[])
{
	const char	 *header = NULL, *incls = NULL, 
	     		 *sharedir = SHAREDIR;
	struct config	 *cfg = NULL;
	int		  c, json = 0, jsonparse = 0, valids = 0,
			  dbin = 1, rc = 0;
	FILE		**confs = NULL;
	size_t		  i, confsz;
	int		  exs[EX__MAX], sz;
	char		  buf[MAXPATHLEN];

#if HAVE_PLEDGE
	if (pledge("stdio rpath", NULL) == -1)
		err(EXIT_FAILURE, "pledge");
#endif

	while ((c = getopt(argc, argv, "h:I:jJN:sS:v")) != -1)
		switch (c) {
		case 'h':
			header = optarg;
			break;
		case 'I':
			incls = optarg;
			break;
		case 'j':
			json = 1;
			break;
		case 'J':
			jsonparse = 1;
			break;
		case 'N':
			if (strchr(optarg, 'd') != NULL)
				dbin = 0;
			break;
		case 's':
			/* Ignore. */
			break;
		case 'S':
			sharedir = optarg;
			break;
		case 'v':
			valids = 1;
			break;
		default:
			goto usage;
		}

	argc -= optind;
	argv += optind;
	confsz = (size_t)argc;
	
	/* Read in all of our files now so we can repledge. */

	if (confsz > 0) {
		if ((confs = calloc(confsz, sizeof(FILE *))) == NULL)
			err(EXIT_FAILURE, NULL);
		for (i = 0; i < confsz; i++)
			if ((confs[i] = fopen(argv[i], "r")) == NULL)
				err(EXIT_FAILURE, "%s", argv[i]);
	}

	/* 
	 * Open all of the source files we might optionally embed in the
	 * output source code.
	 */

	for (i = 0; i < EX__MAX; i++) {
		sz = snprintf(buf, sizeof(buf), 
			"%s/%s", sharedir, externals[i]);
		if (sz < 0 || (size_t)sz >= sizeof(buf))
			errx(EXIT_FAILURE, "%s/%s: too long",
				sharedir, externals[i]);
		if ((exs[i] = open(buf, O_RDONLY, 0)) == -1)
			err(EXIT_FAILURE, "%s/%s", sharedir, externals[i]);
	}

#if HAVE_PLEDGE
	if (pledge("stdio", NULL) == -1)
		err(EXIT_FAILURE, "pledge");
#endif

	if ((cfg = ort_config_alloc()) == NULL)
		goto out;

	for (i = 0; i < confsz; i++)
		if (!ort_parse_file_r(cfg, confs[i], argv[i]))
			goto out;

	if (confsz == 0 && 
	    !ort_parse_file_r(cfg, stdin, "<stdin>"))
		goto out;

	if ((rc = ort_parse_close(cfg)))
		rc = gen_c_source(cfg, json, jsonparse, 
			valids, dbin, header, incls, exs);

out:
	for (i = 0; i < EX__MAX; i++)
		if (close(exs[i]) == -1)
			warn("%s: close", externals[i]);
	for (i = 0; i < confsz; i++)
		if (fclose(confs[i]) == EOF)
			warn("%s: close", argv[i]);
	free(confs);
	ort_config_free(cfg);
	return rc ? EXIT_SUCCESS : EXIT_FAILURE;
usage:
	fprintf(stderr, 
		"usage: %s "
		"[-jJsv] "
		"[-h header[,header...] "
		"[-I bjJv] "
		"[-N b] "
		"[config...]\n",
		getprogname());
	return EXIT_FAILURE;
}
