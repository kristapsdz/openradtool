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

#include <assert.h>
#include <ctype.h>
#if HAVE_ERR
# include <err.h>
#endif
#include <inttypes.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "extern.h"

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
	/* Unary types... */
	"ISNULL", /* OPTYPE_ISNULL */
	"NOTNULL", /* OPTYPE_NOTNULL */
};

/*
 * Functions extracting from a statement.
 * Note that FTYPE_TEXT and FTYPE_PASSWORD need a surrounding strdup.
 */
static	const char *const coltypes[FTYPE__MAX] = {
	"ksql_result_int", /* FTYPE_BIT */
	"ksql_result_int", /* FTYPE_DATE */
	"ksql_result_int", /* FTYPE_EPOCH */
	"ksql_result_int", /* FTYPE_INT */
	"ksql_result_double", /* FTYPE_REAL */
	"ksql_result_blob_alloc", /* FTYPE_BLOB (XXX: is special) */
	"ksql_result_str_alloc", /* FTYPE_TEXT */
	"ksql_result_str_alloc", /* FTYPE_PASSWORD */
	"ksql_result_str_alloc", /* FTYPE_EMAIL */
	NULL, /* FTYPE_STRUCT */
	"ksql_result_int", /* FTYPE_ENUM */
	"ksql_result_int", /* FTYPE_BITFIELD */
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

/*
 * Functions binding an argument to a statement.
 */
static	const char *const bindtypes[FTYPE__MAX] = {
	"ksql_bind_int", /* FTYPE_BIT */
	"ksql_bind_int", /* FTYPE_DATE */
	"ksql_bind_int", /* FTYPE_EPOCH */
	"ksql_bind_int", /* FTYPE_INT */
	"ksql_bind_double", /* FTYPE_REAL */
	"ksql_bind_blob", /* FTYPE_BLOB (XXX: is special) */
	"ksql_bind_str", /* FTYPE_TEXT */
	"ksql_bind_str", /* FTYPE_PASSWORD */
	"ksql_bind_str", /* FTYPE_EMAIL */
	NULL, /* FTYPE_STRUCT */
	"ksql_bind_int", /* FTYPE_ENUM */
	"ksql_bind_int", /* FTYPE_BITFIELD */
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
 * Use this function to print ksql_stmt_alloc() invocations.
 * This is because we use a different calling style depending upon
 * whether we have roles or not.
 */
static void
gen_print_stmt_alloc(const struct config *cfg, 
	int tabsz, const char *fmt, ...)
{
	va_list	 	 ap;
	char		*buf;
	const char	*tabs = "\t\t\t\t\t";

	assert(tabsz > 0 && tabsz < 4);

	va_start(ap, fmt);
	if (vasprintf(&buf, fmt, ap) < 0)
		err(EXIT_FAILURE, NULL);
	va_end(ap);

	if (CFG_HAS_ROLES & cfg->flags)
		printf("%.*sksql_stmt_alloc(db, &stmt, "
			"NULL, %s);\n", tabsz, tabs, buf);
	else
		printf("%.*sksql_stmt_alloc(db, &stmt,\n%.*s"
			"stmts[%s],\n%.*s"
			"%s);\n", tabsz, tabs, tabsz + 1, tabs, 
			buf, tabsz + 1, tabs, buf);

	free(buf);
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

	if (FTYPE_STRUCT == f->type &&
	    FIELD_NULL & f->ref->source->flags) {
		printf("\tp->has_%s = 0;\n", f->name);
		return;
	} else if (FTYPE_STRUCT == f->type)
		return;

	if (FIELD_NULL & f->flags)
		printf("\tp->has_%s = ! "
			"ksql_stmt_isnull(stmt, *pos);\n",
			f->name);

	/*
	 * Blob types need to have space allocated (and the space
	 * variable set) before we extract from the database.
	 * This sequence is very different from the other types, so make
	 * it into its own conditional block for clarity.
	 */

	if (FIELD_NULL & f->flags) {
		printf("\tif (p->has_%s) {\n", f->name);
		indent = 2;
	} else
		indent = 1;

	if (FTYPE_BLOB == f->type)
		print_src(indent,
			"c = %s(stmt, "
			 "&p->%s, &p->%s_sz, (*pos)++);\n"
			"if (KSQL_OK != c)\n"
		        "\texit(EXIT_FAILURE);",
			coltypes[f->type], f->name, f->name);
	else if (FTYPE_ENUM == f->type)
		print_src(indent,
			"c = %s(stmt, &tmpint, (*pos)++);\n"
			"if (KSQL_OK != c)\n"
		        "\texit(EXIT_FAILURE);\n"
			"p->%s = tmpint;",
			coltypes[f->type], f->name);
	else
		print_src(indent,
			"c = %s(stmt, &p->%s, (*pos)++);\n"
			"if (KSQL_OK != c)\n"
		        "\texit(EXIT_FAILURE);",
			coltypes[f->type], f->name);

	if (FIELD_NULL & f->flags) 
		puts("\t} else\n"
		     "\t\t(*pos)++;");
}

/*
 * Generate the binding for a field of type "t" at field "pos".
 * Set "ptr" to be non-zero if this is passed in as a pointer.
 */
static void
gen_bindfunc(enum ftype t, size_t pos, int ptr)
{

	assert(FTYPE_STRUCT != t);
	if (FTYPE_BLOB == t)
		printf("\t%s(stmt, %zu, %sv%zu, v%zu_sz);\n",
			bindtypes[t], pos - 1, 
			ptr ? "*" : "", pos, pos);
	else if (FTYPE_PASSWORD != t)
		printf("\t%s(stmt, %zu, %sv%zu);\n", 
			bindtypes[t], pos - 1, 
			ptr ? "*" : "", pos);
}

/*
 * Print out a search function for an STYPE_ITERATE.
 * This calls a function pointer with the retrieved data.
 */
static void
gen_strct_func_iter(const struct config *cfg,
	const struct search *s, size_t num)
{
	const struct sent *sent;
	const struct sref *sr;
	const struct strct *retstr;
	size_t	 pos;

	assert(STYPE_ITERATE == s->type);

	retstr = NULL != s->dst ? 
		s->dst->strct : s->parent;

	print_func_db_search(s, CFG_HAS_ROLES & cfg->flags, 0);
	printf("\n"
	       "{\n"
	       "\tstruct ksqlstmt *stmt;\n"
	       "\tstruct %s p;\n", retstr->name);
	if (CFG_HAS_ROLES & cfg->flags)
		puts("\tstruct ksql *db = ctx->db;");

	puts("");

	gen_print_stmt_alloc(cfg, 1,
		"STMT_%s_BY_SEARCH_%zu", 
		s->parent->cname, num);

	pos = 1;
	TAILQ_FOREACH(sent, &s->sntq, entries)
		if (OPTYPE_ISBINARY(sent->op)) {
			sr = TAILQ_LAST(&sent->srq, srefq);
			gen_bindfunc(sr->field->type, pos++, 0);
		}

	printf("\twhile (KSQL_ROW == ksql_stmt_step(stmt)) {\n"
	       "\t\tdb_%s_fill_r(%s&p, stmt, NULL);\n",
	       retstr->name,
	       CFG_HAS_ROLES & cfg->flags ? "ctx, " : "");
	if (STRCT_HAS_NULLREFS & retstr->flags)
	       printf("\t\tdb_%s_reffind(%s&p, db);\n", 
		     retstr->name,
		     CFG_HAS_ROLES & cfg->flags ? 
		     "ctx, " : "");

	/*
	 * If we have any hashes, we're going to need to do the hash
	 * check after the field has already been extracted from the
	 * database.
	 * If the hash doesn't match, don't run the callback.
	 */

	pos = 1;
	TAILQ_FOREACH(sent, &s->sntq, entries) {
		if (OPTYPE_ISUNARY(sent->op))
			continue;
		sr = TAILQ_LAST(&sent->srq, srefq);
		if (FTYPE_PASSWORD != sr->field->type) {
			pos++;
			continue;
		}
		printf("\t\tif (crypt_checkpass(v%zu, p.%s) < 0) {\n"
		       "\t\t\tdb_%s_unfill_r(&p);\n"
		       "\t\t\tcontinue;\n"
		       "\t\t}\n",
		       pos, sent->fname, s->parent->name);
		pos++;
	}

	printf("\t\t(*cb)(&p, arg);\n"
	       "\t\tdb_%s_unfill_r(&p);\n"
	       "\t}\n"
	       "\tksql_stmt_free(stmt);\n"
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
	const struct sent *sent;
	const struct sref *sr;
	const struct strct *retstr;
	size_t	 pos;

	assert(STYPE_LIST == s->type);

	retstr = NULL != s->dst ? 
		s->dst->strct : s->parent;

	print_func_db_search(s, CFG_HAS_ROLES & cfg->flags, 0);
	printf("\n"
	       "{\n"
	       "\tstruct ksqlstmt *stmt;\n"
	       "\tstruct %s_q *q;\n"
	       "\tstruct %s *p;\n",
	       retstr->name, retstr->name);
	if (CFG_HAS_ROLES & cfg->flags)
		puts("\tstruct ksql *db = ctx->db;");

	puts("");

	printf("\tq = malloc(sizeof(struct %s_q));\n"
	       "\tif (NULL == q) {\n"
	       "\t\tperror(NULL);\n"
	       "\t\texit(EXIT_FAILURE);\n"
	       "\t}\n"
	       "\tTAILQ_INIT(q);\n"
	       "\n", retstr->name);
	gen_print_stmt_alloc(cfg, 1,
		"STMT_%s_BY_SEARCH_%zu", 
		s->parent->cname, num);

	/*
	 * If we have any hashes, we're going to need to do the hash
	 * check after the field has already been extracted from the
	 * database.
	 * If the hash doesn't match, don't insert into the tailq.
	 */

	pos = 1;
	TAILQ_FOREACH(sent, &s->sntq, entries)
		if (OPTYPE_ISBINARY(sent->op)) {
			sr = TAILQ_LAST(&sent->srq, srefq);
			gen_bindfunc(sr->field->type, pos++, 0);
		}

	printf("\twhile (KSQL_ROW == ksql_stmt_step(stmt)) {\n"
	       "\t\tp = malloc(sizeof(struct %s));\n"
	       "\t\tif (NULL == p) {\n"
	       "\t\t\tperror(NULL);\n"
	       "\t\t\texit(EXIT_FAILURE);\n"
	       "\t\t}\n"
	       "\t\tdb_%s_fill_r(%sp, stmt, NULL);\n",
	       retstr->name, retstr->name,
	       CFG_HAS_ROLES & cfg->flags ? "ctx, " : "");
	if (STRCT_HAS_NULLREFS & retstr->flags)
	       printf("\t\tdb_%s_reffind(%sp, db);\n",
		      retstr->name,
		      CFG_HAS_ROLES & cfg->flags ? 
		      "ctx, " : "");

	pos = 1;
	TAILQ_FOREACH(sent, &s->sntq, entries) {
		if (OPTYPE_ISUNARY(sent->op))
			continue;
		sr = TAILQ_LAST(&sent->srq, srefq);
		if (FTYPE_PASSWORD != sr->field->type) {
			pos++;
			continue;
		}
		printf("\t\tif (crypt_checkpass(v%zu, p->%s) < 0) {\n"
		       "\t\t\tdb_%s_free(p);\n"
		       "\t\t\tcontinue;\n"
		       "\t\t}\n",
		       pos, sent->fname, s->parent->name);
		pos++;
	}

	puts("\t\tTAILQ_INSERT_TAIL(q, p, _entries);\n"
	     "\t}\n"
	     "\tksql_stmt_free(stmt);\n"
	     "\treturn(q);\n"
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

/*
 * For any given role "role", statically allocate the permission and
 * statements list that we'll later assign to the role structures.
 * This is a recursive function that will invoke on all subroles.
 * It's passed "rolesz", which is the total number of roles.
 */
static void
gen_func_role_matrices(const struct role *role, size_t rolesz)
{
	const struct role *r;

	if (strcmp(role->name, "all"))
		printf("\tint role_perms_%s[%zu];\n"
		       "\tint role_stmts_%s[STMT__MAX];\n", 
		       role->name, rolesz, role->name);
	TAILQ_FOREACH(r, &role->subrq, entries)
		gen_func_role_matrices(r, rolesz);
}

/*
 * Assign the transition and statement declaration for the given role to
 * the configuration and initialise the arrays' values.
 */
static void
gen_func_role_zero(const struct role *role, size_t rolesz)
{
	const struct role *r;

	/* 
	 * Don't set role transition for default.
	 * This is because we do this for each role.
	 * For no other reason than to get the role name when we do the
	 * assignment instead of having a loop.
	 */

	if (0 == strcmp(role->name, "default")) 
		puts("\troles[ROLE_default].roles = "
				"role_perms_default;\n"
		     "\troles[ROLE_default].stmts = "
		     		"role_stmts_default;\n"
		     "\troles[ROLE_default].flags = "
		     		"KSQLROLE_OPEN;\n"
		     "\tmemset(role_stmts_default, 0, "
		      "sizeof(int) * STMT__MAX);");
	else if (strcmp(role->name, "all"))
		printf("\troles[ROLE_%s].roles = role_perms_%s;\n"
		       "\troles[ROLE_%s].stmts = role_stmts_%s;\n"
		       "\tmemset(role_perms_%s, 0, "
		        "sizeof(int) * %zu);\n"
		       "\tmemset(role_stmts_%s, 0, "
		        "sizeof(int) * STMT__MAX);\n",
		       role->name, role->name,
		       role->name, role->name,
		       role->name, rolesz, role->name);

	if (strcmp(role->name, "all"))
		printf("\trole_perms_default"
		       "[ROLE_%s] = 1;\n\n", role->name);

	TAILQ_FOREACH(r, &role->subrq, entries)
		gen_func_role_zero(r, rolesz);
}

/*
 * Recursively generate possible assignments.
 * This is tree-based: for each node, we can ascend to the root of the
 * role tree.
 * This describes that a child role can ascend to the parent, but a
 * parent cannot descend (gain more privileges).
 */
static void
gen_func_role_assign(const struct role *role)
{
	const struct role *r;

	/* "All" isn't a role and "default" can switch to anybody. */

	if (strcmp(role->name, "all") &&
	    strcmp(role->name, "default"))
		for (r = role; NULL != r; r = r->parent) {
			if (0 == strcmp(r->name, "all"))
				continue;
			printf("\trole_perms_%s[ROLE_%s] = 1;\n",
				role->name, r->name);
		}

	if (strcmp(role->name, "all") &&
	    strcmp(role->name, "default") &&
	    strcmp(role->name, "none"))
		printf("\trole_perms_%s[ROLE_none] = 1;\n", 
			role->name);

	TAILQ_FOREACH(r, &role->subrq, entries)
		gen_func_role_assign(r);
}

/*
 * Recursive descent checking whether "role" matches.
 * See check_rolemap().
 */
static int
check_rolemap_r(const struct role *role, const struct role *checkrole)
{
	const struct role *r;

	TAILQ_FOREACH(r, &checkrole->subrq, entries)
		if (role == r || check_rolemap_r(role, r))
			return(1);

	return(0);
}

/*
 * The rolemap defines all roles (in "setq") that are allowed to run
 * whatever we're looking at.
 * Since all children of a role inherit the parent's allowances, we need
 * to descend into all children of a role too.
 * The "role" is what we're checking can run whatever we're looking at.
 * Return zero if disallowed, non-zero if allowed.
 */
static int
check_rolemap(const struct role *role, const struct rolemap *rm)
{
	const struct roleset *rs;

	if (NULL == rm)
		return(0);

	TAILQ_FOREACH(rs, &rm->setq, entries) {
		assert(NULL != rs->role);
		if (rs->role == role ||
		    check_rolemap_r(role, rs->role))
			return(1);
	}

	return(0);
}

/*
 * For any given structure "p" (fixed), recursively descend into all
 * roles and mark which statements are allowed for "p".
 * Return the number of statements we've printed.
 */
static size_t
gen_func_role_stmts(const struct role *role, const struct strct *p)
{
	const struct search *s;
	const struct update *u;
	const struct field *f;
	const struct role *r;
	size_t	 pos, shown = 0;

	/* Ignore "all" and "none" virtual roles for assignment. */

	if (0 == strcmp(role->name, "none") ||
	    0 == strcmp(role->name, "all")) {
		TAILQ_FOREACH(r, &role->subrq, entries)
			shown += gen_func_role_stmts(r, p);
		return(shown);
	}

	/*
	 * FIXME: this is not acceptable.
	 * If our structure has any unique fields, allow each role to
	 * access this statement.
	 */

	TAILQ_FOREACH(f, &p->fq, entries) {
		if (FIELD_UNIQUE & f->flags ||
		    FIELD_ROWID & f->flags)
			printf("\trole_stmts_%s"
			       "[STMT_%s_BY_UNIQUE_%s] = 1;\n", 
				role->name, p->cname, f->name);
		shown++;
	}

	/*
	 * For all the operations on this structure, see if the given
	 * role allows it to run the corresponding statement.
	 * We use the "check_rolemap" function for this, which checks if
	 * a role is assigned to the structure.
	 * If it is, we allow it for the current role and all subroles.
	 */

	pos = 0;
	TAILQ_FOREACH(s, &p->sq, entries) {
		if (check_rolemap(role, s->rolemap))
			printf("\trole_stmts_%s"
			       "[STMT_%s_BY_SEARCH_%zu] = 1;\n", 
				role->name, p->cname, pos);
		pos++;
	}
	shown += pos;

	if (NULL != p->ins && 
	    check_rolemap(role, p->ins->rolemap)) {
		printf("\trole_stmts_%s[STMT_%s_INSERT] = 1;\n", 
			role->name, p->cname);
		shown++;
	}

	pos = 0;
	TAILQ_FOREACH(u, &p->uq, entries) {
		if (check_rolemap(role, u->rolemap))
			printf("\trole_stmts_%s"
		  	       "[STMT_%s_UPDATE_%zu] = 1;\n", 
				role->name, p->cname, pos);
		pos++;
	}
	shown += pos;

	pos = 0;
	TAILQ_FOREACH(u, &p->dq, entries) {
		if (check_rolemap(role, u->rolemap))
			printf("\trole_stmts_%s"
			       "[STMT_%s_DELETE_%zu] = 1;\n", 
				role->name, p->cname, pos);
		pos++;
	}
	shown += pos;

	TAILQ_FOREACH(r, &role->subrq, entries)
		shown += gen_func_role_stmts(r, p);

	return(shown);
}

/*
 * Generate database opening.
 * We don't use the generic invocation, as we want foreign keys.
 * If "splitproc" is specified, use ksql_alloc_child instead of the
 * usual allocation method.
 */
static void
gen_func_open(const struct config *cfg, int splitproc)
{
	const struct role *r;
	const struct strct *p;
	size_t	i, shown;

	print_func_db_open(CFG_HAS_ROLES & cfg->flags, 0);

	puts("{\n"
	     "\tstruct ksqlcfg cfg;\n"
	     "\tstruct ksql *db;");

	if (CFG_HAS_ROLES & cfg->flags) {
		/*
		 * We need an complete count of all roles except the
		 * "all" role, which cannot be entered or processed.
		 * So do this recursively.
		 */
		i = 0;
		TAILQ_FOREACH(r, &cfg->rq, entries)
			i += gen_func_role_count(r);
		assert(i > 0);
		TAILQ_FOREACH(r, &cfg->rq, entries)
			gen_func_role_matrices(r, i);
		printf("\tstruct ksqlrole roles[%zu];\n"
		       "\tstruct kwbp *ctx;\n"
		       "\n"
		       "\tmemset(roles, 0, sizeof(roles));\n"
		       "\tctx = malloc(sizeof(struct kwbp));\n"
		       "\tif (NULL == ctx)\n"
		       "\t\treturn(NULL);\n"
		       "\n", i);
		print_commentt(1, COMMENT_C,
			"Initialise our roles and statements: "
			"disallow all statements and role "
			"transitions except for ROLE_default, "
			"which can transition to anybody.");
		puts("");
		TAILQ_FOREACH(r, &cfg->rq, entries)
			gen_func_role_zero(r, i);
		print_commentt(1, COMMENT_C,
			"Assign roles.\n"
			"Everybody can transition to themselves "
			"(this is always allowed in ksql(3), so make "
			"it explicit for us).\n"
			"Furthermore, everybody is allowed to "
			"transition into ROLE_none.");
		puts("");
		TAILQ_FOREACH(r, &cfg->rq, entries)
			gen_func_role_assign(r);
		puts("");
		TAILQ_FOREACH(p, &cfg->sq, entries) {
			print_commentv(1, COMMENT_C, 
				"White-listing fields and "
				"operations for structure \"%s\".",
				p->name);
			puts("");
			shown = 0;
			TAILQ_FOREACH(r, &cfg->rq, entries)
				shown += gen_func_role_stmts(r, p);
			if (shown)
				puts("");
		}
		printf("\tksql_cfg_defaults(&cfg);\n"
		       "\tcfg.stmts.stmts = stmts;\n"
		       "\tcfg.stmts.stmtsz = STMT__MAX;\n"
		       "\tcfg.roles.roles = roles;\n"
		       "\tcfg.roles.rolesz = %zu;\n"
		       "\tcfg.roles.defrole = ROLE_default;\n"
		       "\n", i);
	} else 
		puts("\n"
		     "\tksql_cfg_defaults(&cfg);\n");

	if (splitproc)
		puts("\tdb = ksql_alloc_child(&cfg, NULL, NULL);");
	else
		puts("\tdb = ksql_alloc(&cfg);");

	if (CFG_HAS_ROLES & cfg->flags)
		puts("\tif (NULL == db) {\n"
		     "\t\tfree(ctx);\n"
		     "\t\treturn(NULL);\n"
		     "\t}\n"
		     "\tctx->db = db;");
	else
		puts("\tif (NULL == db)\n"
		     "\t\treturn(NULL);");

	puts("\tksql_open(db, file);");

	if (CFG_HAS_ROLES & cfg->flags) {
		puts("\tctx->role = ROLE_default;\n"
		     "\treturn(ctx);");
	} else
		puts("\treturn(db);");

	puts("}\n"
	     "");
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
gen_func_roles(const struct config *cfg)
{
	const struct role *r, *rr;

	TAILQ_FOREACH(r, &cfg->rq, entries)
		if (0 == strcmp(r->name, "all"))
			break;
	assert(NULL != r);

	print_func_db_role(0);
	puts("{\n"
	     "\tksql_role(ctx->db, r);\n"
	     "\tif (r == ctx->role)\n"
	     "\t\treturn;\n"
	     "\tif (ROLE_none == ctx->role)\n"
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
	     "\treturn(ctx->role);\n"
	     "}\n");
	print_func_db_role_stored(0);
	puts("{\n"
	     "\treturn(s->role);\n"
	     "}\n");
}

static void
gen_func_trans(const struct config *cfg)
{

	if (CFG_HAS_ROLES & cfg->flags) {
		print_func_db_trans_open(1, 0);
		puts("{\n"
		     "\tif (mode < 0)\n"
		     "\t\tksql_trans_exclopen(p->db, id);\n"
		     "\telse if (mode > 0)\n"
		     "\t\tksql_trans_singleopen(p->db, id);\n"
		     "\telse\n"
		     "\t\tksql_trans_open(p->db, id);\n"
		     "}\n"
		     "");
		print_func_db_trans_rollback(1, 0);
		puts("{\n"
		     "\tksql_trans_rollback(p->db, id);\n"
		     "}\n"
		     "");
		print_func_db_trans_commit(1, 0);
		puts("{\n"
		     "\tksql_trans_commit(p->db, id);\n"
		     "}\n"
		     "");
	} else {
		print_func_db_trans_open(0, 0);
		puts("{\n"
		     "\tif (mode < 0)\n"
		     "\t\tksql_trans_exclopen(p, id);\n"
		     "\telse if (mode > 0)\n"
		     "\t\tksql_trans_singleopen(p, id);\n"
		     "\telse\n"
		     "\t\tksql_trans_open(p, id);\n"
		     "}\n"
		     "");
		print_func_db_trans_rollback(0, 0);
		puts("{\n"
		     "\tksql_trans_rollback(p, id);\n"
		     "}\n"
		     "");
		print_func_db_trans_commit(0, 0);
		puts("{\n"
		     "\tksql_trans_commit(p, id);\n"
		     "}\n"
		     "");
	}
}

/*
 * Close and free the database context.
 * This is sensitive to whether we have roles.
 */
static void
gen_func_close(const struct config *cfg)
{

	print_func_db_close(CFG_HAS_ROLES & cfg->flags, 0);
	puts("{\n"
	     "\tif (NULL == p)\n"
	     "\t\treturn;");
	if (CFG_HAS_ROLES & cfg->flags) {
		puts("\tksql_close(p->db);\n"
	             "\tksql_free(p->db);\n"
		     "\tfree(p);");
	} else 
		puts("\tksql_close(p);\n"
	             "\tksql_free(p);");

	puts("}\n"
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
	const struct sent *sent;
	const struct sref *sr;
	const struct strct *retstr;
	size_t	 pos;

	assert(STYPE_SEARCH == s->type);

	retstr = NULL != s->dst ? 
		s->dst->strct : s->parent;

	print_func_db_search(s, CFG_HAS_ROLES & cfg->flags, 0);
	printf("\n"
	       "{\n"
	       "\tstruct ksqlstmt *stmt;\n"
	       "\tstruct %s *p = NULL;\n",
	       retstr->name);
	if (CFG_HAS_ROLES & cfg->flags)
		puts("\tstruct ksql *db = ctx->db;");

	puts("");

	gen_print_stmt_alloc(cfg, 1,
		"STMT_%s_BY_SEARCH_%zu", 
		s->parent->cname, num);

	pos = 1;
	TAILQ_FOREACH(sent, &s->sntq, entries) 
		if (OPTYPE_ISBINARY(sent->op)) {
			sr = TAILQ_LAST(&sent->srq, srefq);
			gen_bindfunc(sr->field->type, pos++, 0);
		}

	printf("\tif (KSQL_ROW == ksql_stmt_step(stmt)) {\n"
	       "\t\tp = malloc(sizeof(struct %s));\n"
	       "\t\tif (NULL == p) {\n"
	       "\t\t\tperror(NULL);\n"
	       "\t\t\texit(EXIT_FAILURE);\n"
	       "\t\t}\n"
	       "\t\tdb_%s_fill_r(%sp, stmt, NULL);\n",
	       retstr->name, retstr->name,
	       CFG_HAS_ROLES & cfg->flags ? "ctx, " : "");
	if (STRCT_HAS_NULLREFS & retstr->flags)
	       printf("\t\tdb_%s_reffind(%sp, db);\n",
		      retstr->name,
	 	      CFG_HAS_ROLES & cfg->flags ?
		      "ctx, " : "");

	/*
	 * If we have any hashes, we're going to need to do the hash
	 * check after the field has already been extracted from the
	 * database.
	 * If the hash doesn't match, nullify.
	 */

	pos = 1;
	TAILQ_FOREACH(sent, &s->sntq, entries) {
		if (OPTYPE_ISUNARY(sent->op))
			continue;
		sr = TAILQ_LAST(&sent->srq, srefq);
		if (FTYPE_PASSWORD != sr->field->type) {
			pos++;
			continue;
		}
		printf("\t\tif (NULL != p && "
			"crypt_checkpass(v%zu, p->%s) < 0) {\n"
		       "\t\t\tdb_%s_free(p);\n"
		       "\t\t\tp = NULL;\n"
		       "\t\t}\n",
		       pos, sent->fname, s->parent->name);
		pos++;
	}

	puts("\t}\n"
	     "\tksql_stmt_free(stmt);\n"
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
	       "\tif (NULL == q)\n"
	       "\t\treturn;\n"
	       "\twhile (NULL != (p = TAILQ_FIRST(q))) {\n"
	       "\t\tTAILQ_REMOVE(q, p, _entries);\n"
	       "\t\tdb_%s_free(p);\n"
	       "\t}\n\n"
	       "\tfree(q);\n"
	       "}\n"
	       "\n", 
	       p->name, p->name);
}

/*
 * Generate the "insert" function.
 */
static void
gen_func_insert(const struct config *cfg, const struct strct *p)
{
	const struct field *f;
	size_t	 pos, npos;

	if (NULL == p->ins)
		return;

	print_func_db_insert(p, CFG_HAS_ROLES & cfg->flags, 0);

	puts("\n"
	     "{\n"
	     "\tstruct ksqlstmt *stmt;\n"
	     "\tint64_t id = -1;");
	if (CFG_HAS_ROLES & cfg->flags)
		puts("\tstruct ksql *db = ctx->db;");

	/* We need temporary space for hash generation. */

	pos = 1;
	TAILQ_FOREACH(f, &p->fq, entries)
		if (FTYPE_PASSWORD == f->type)
			printf("\tchar hash%zu[64];\n", pos++);

	puts("");

	/* Actually generate hashes, if necessary. */

	pos = npos = 1;
	TAILQ_FOREACH(f, &p->fq, entries) {
		if (FTYPE_PASSWORD == f->type) {
			if (FIELD_NULL & f->flags)
				printf("\tif (NULL != v%zu)\n"
				       "\t", npos);
			printf("\tcrypt_newhash(%sv%zu, "
				"\"blowfish,a\", hash%zu, "
				"sizeof(hash%zu));\n",
				FIELD_NULL & f->flags ? "*" : "",
				npos, pos, pos);
			pos++;
		} 
		if (FTYPE_STRUCT == f->type ||
		    FIELD_ROWID & f->flags)
			continue;
		npos++;
	}
	if (pos > 1)
		puts("");

	gen_print_stmt_alloc(cfg, 1, "STMT_%s_INSERT", p->cname);

	pos = npos = 1;
	TAILQ_FOREACH(f, &p->fq, entries) {
		if (FTYPE_STRUCT == f->type ||
		    FIELD_ROWID & f->flags)
			continue;
		assert(FTYPE_STRUCT != f->type);
		if (FIELD_NULL & f->flags)
			printf("\tif (NULL == v%zu)\n"
			       "\t\tksql_bind_null(stmt, %zu);\n"
			       "\telse\n"
			       "\t", npos, npos - 1);
		if (FTYPE_PASSWORD == f->type) {
			printf("\t%s(stmt, %zu, hash%zu);\n",
				bindtypes[f->type],
				npos - 1, pos);
			npos++;
			pos++;
		} else
			gen_bindfunc(f->type, 
				npos++, FIELD_NULL & f->flags);
	}
	puts("\tif (KSQL_DONE == ksql_stmt_cstep(stmt))\n"
	     "\t\tksql_lastid(db, &id);\n"
	     "\tksql_stmt_free(stmt);\n"
	     "\treturn(id);\n"
	     "}\n"
	     "");
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

	print_func_db_unfill(p, CFG_HAS_ROLES & cfg->flags, 0);
	puts("\n"
	     "{\n"
	     "\tif (NULL == p)\n"
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
	if (CFG_HAS_ROLES & cfg->flags)
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
	       "\tif (NULL == p)\n"
	       "\t\treturn;\n"
	       "\n"
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

	if ( ! (STRCT_HAS_NULLREFS & p->flags))
		return;

	TAILQ_FOREACH(f, &p->fq, entries)
		if (FTYPE_STRUCT == f->type &&
		    FIELD_NULL & f->ref->source->flags)
			break;

	printf("static void\n"
	       "db_%s_reffind(%sstruct %s *p, struct ksql *db)\n"
	       "{\n", p->name, 
	       CFG_HAS_ROLES & cfg->flags ? 
	       "struct kwbp *ctx, " : "", p->name);
	if (NULL != f)
		puts("\tstruct ksqlstmt *stmt;\n"
		     "\tenum ksqlc c;");

	puts("");
	TAILQ_FOREACH(f, &p->fq, entries) {
		if (FTYPE_STRUCT != f->type)
			continue;
		if (FIELD_NULL & f->ref->source->flags) {
			printf("\tif (p->has_%s) {\n",
				f->ref->source->name);
			gen_print_stmt_alloc(cfg, 2,
				"STMT_%s_BY_UNIQUE_%s", 
				f->ref->target->parent->cname,
				f->ref->target->name);
			printf("\t\tksql_bind_int(stmt, 0, p->%s);\n",
			       f->ref->source->name);
			printf("\t\tc = ksql_stmt_step(stmt);\n"
			       "\t\tassert(KSQL_ROW == c);\n"
			       "\t\tdb_%s_fill_r(%s&p->%s, stmt, NULL);\n",
			       f->ref->target->parent->name,
			       CFG_HAS_ROLES & cfg->flags ?
			       "ctx, " : "", f->name);
			printf("\t\tp->has_%s = 1;\n",
				f->name);
			puts("\t\tksql_stmt_free(stmt);\n"
			     "\t}");
		} 
		if ( ! (STRCT_HAS_NULLREFS &
		    f->ref->target->parent->flags))
			continue;
		printf("\tdb_%s_reffind(%s&p->%s, db);\n", 
			f->ref->target->parent->name, 
			CFG_HAS_ROLES & cfg->flags ? 
			"ctx, " : "", f->name);
	}

	puts("}\n"
	     "");
}

/*
 * Generate the "fill" function.
 */
static void
gen_func_fill_r(const struct config *cfg, const struct strct *p)
{
	const struct field *f;

	printf("static void\n"
	       "db_%s_fill_r(%sstruct %s *p, "
	       "struct ksqlstmt *stmt, size_t *pos)\n"
	       "{\n"
	       "\tsize_t i = 0;\n"
	       "\n"
	       "\tif (NULL == pos)\n"
	       "\t\tpos = &i;\n"
	       "\tdb_%s_fill(%sp, stmt, pos);\n",
	       p->name, 
	       CFG_HAS_ROLES & cfg->flags ?
	       "struct kwbp *ctx, " : "",
	       p->name, p->name,
	       CFG_HAS_ROLES & cfg->flags ?
	       "ctx, " : "");
	TAILQ_FOREACH(f, &p->fq, entries)
		if (FTYPE_STRUCT == f->type &&
		    ! (FIELD_NULL & f->ref->source->flags))
			printf("\tdb_%s_fill_r(%s&p->%s, "
				"stmt, pos);\n", 
				f->ref->tstrct, 
				CFG_HAS_ROLES & cfg->flags ?
				"ctx, " : "", f->name);
	puts("}\n"
	     "");
}

/*
 * Generate the "fill" function.
 */
static void
gen_func_fill(const struct config *cfg, const struct strct *p)
{
	const struct field *f;
	int	 needint = 0;

	TAILQ_FOREACH(f, &p->fq, entries)
		if (FTYPE_ENUM == f->type)
			needint = 1;

	print_func_db_fill(p, CFG_HAS_ROLES & cfg->flags, 0);
	puts("\n"
	     "{\n"
	     "\tsize_t i = 0;\n"
	     "\tenum ksqlc c;");
	if (needint)
		puts("\tint64_t tmpint;");
	puts("\n"
	     "\tif (NULL == pos)\n"
	     "\t\tpos = &i;\n"
	     "\tmemset(p, 0, sizeof(*p));");
	TAILQ_FOREACH(f, &p->fq, entries)
		gen_strct_fill_field(f);
	if (CFG_HAS_ROLES & cfg->flags) {
		puts("\tp->priv_store = malloc"
		      "(sizeof(struct kwbp_store));\n"
		     "\tif (NULL == p->priv_store) {\n"
		     "\t\tperror(NULL);\n"
		     "\t\texit(EXIT_FAILURE);\n"
		     "\t}\n"
		     "\tp->priv_store->role = ctx->role;");
	}
	puts("}\n"
	     "");
}

/*
 * Generate an update or delete function.
 */
static void
gen_func_update(const struct config *cfg,
	const struct update *up, size_t num)
{
	const struct uref *ref;
	size_t	 pos, npos;

	print_func_db_update(up, CFG_HAS_ROLES & cfg->flags, 0);
	puts("\n"
	     "{\n"
	     "\tstruct ksqlstmt *stmt;\n"
	     "\tenum ksqlc c;");
	if (CFG_HAS_ROLES & cfg->flags)
		puts("\tstruct ksql *db = ctx->db;");

	/* Create hash buffer for modifying hashes. */

	pos = 1;
	TAILQ_FOREACH(ref, &up->mrq, entries)
		if (FTYPE_PASSWORD == ref->field->type)
			printf("\tchar hash%zu[64];\n", pos++);

	puts("");

	/* Create hash from password. */

	npos = pos = 1;
	TAILQ_FOREACH(ref, &up->mrq, entries) {
		if (FTYPE_PASSWORD == ref->field->type) {
			if (FIELD_NULL & ref->field->flags)
				printf("if (NULL != v%zu)\n"
				       "\t", npos);
			printf("\tcrypt_newhash(v%zu, "
				"\"blowfish,a\", hash%zu, "
				"sizeof(hash%zu));\n",
				npos, pos, pos);
			pos++;
		} 
		npos++;
	}

	if (pos > 1)
		puts("");

	if (UP_MODIFY == up->type)
		gen_print_stmt_alloc(cfg, 1,
			"STMT_%s_UPDATE_%zu", 
			up->parent->cname, num);
	else
		gen_print_stmt_alloc(cfg, 1,
			"STMT_%s_DELETE_%zu", 
			up->parent->cname, num);

	npos = pos = 1;
	TAILQ_FOREACH(ref, &up->mrq, entries) {
		assert(FTYPE_STRUCT != ref->field->type);
		if (FIELD_NULL & ref->field->flags)
			printf("\tif (NULL == v%zu)\n"
			       "\t\tksql_bind_null(stmt, %zu);\n"
			       "\telse\n"
			       "\t", npos, npos - 1);
		if (FTYPE_PASSWORD == ref->field->type)
			printf("\t%s(stmt, %zu, hash%zu);\n",
			       bindtypes[ref->field->type],
			       npos - 1, pos++);
		else
			gen_bindfunc(ref->field->type, npos,
				FIELD_NULL & ref->field->flags);
		npos++;
	}
	TAILQ_FOREACH(ref, &up->crq, entries) {
		assert(FTYPE_STRUCT != ref->field->type);
		assert(FTYPE_PASSWORD != ref->field->type);
		if (OPTYPE_ISUNARY(ref->op))
			continue;
		printf("\t%s(stmt, %zu, v%zu);\n", 
			bindtypes[ref->field->type],
			npos - 1, npos);
		npos++;
	}
	puts("\tc = ksql_stmt_cstep(stmt);\n"
	     "\tksql_stmt_free(stmt);\n"
	     "\treturn(KSQL_CONSTRAINT != c);\n"
	     "}\n"
	     "");
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
	case (FTYPE_BIT):
	case (FTYPE_ENUM):
	case (FTYPE_BITFIELD):
	case (FTYPE_DATE):
	case (FTYPE_EPOCH):
	case (FTYPE_INT):
		printf("\tif (p->parsed.i %s %" PRId64 ")\n"
		       "\t\treturn 0;\n",
		       validbins[v->type], v->d.value.integer);
		break;
	case (FTYPE_REAL):
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
	const struct field *f;
	const struct fvalid *v;
	const struct eitem *ei;

	TAILQ_FOREACH(f, &p->fq, entries) {
		if (FTYPE_STRUCT == f->type)
			continue;
		print_func_valid(f, 0);
		puts("{");
		if (NULL != validtypes[f->type])
			printf("\tif ( ! %s(p))\n"
			       "\t\treturn 0;\n",
			       validtypes[f->type]);

		/* Enumeration: check against knowns. */

		if (FTYPE_ENUM == f->type) {
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
		puts("}\n"
		     "");
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
			printf("%sif ( ! p->has_%s)\n"
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
		       "\tif (NULL == buf%zu) {\n"
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
	int json, int valids, int dbin)
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

	if (valids)
		gen_func_valids(p);

	if ( ! dbin)
		return;

	pos = 0;
	TAILQ_FOREACH(s, &p->sq, entries)
		if (STYPE_SEARCH == s->type)
			gen_strct_func_srch(cfg, s, pos++);
		else if (STYPE_LIST == s->type)
			gen_strct_func_list(cfg, s, pos++);
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
 * Generate a set of statements that will be used for this structure.
 */
static void
gen_enum(const struct strct *p)
{
	const struct search *s;
	const struct update *u;
	const struct field *f;
	size_t	 pos;

	TAILQ_FOREACH(f, &p->fq, entries)
		if (FIELD_UNIQUE & f->flags ||
		    FIELD_ROWID & f->flags)
			printf("\tSTMT_%s_BY_UNIQUE_%s,\n", 
				p->cname, f->name);

	pos = 0;
	TAILQ_FOREACH(s, &p->sq, entries)
		printf("\tSTMT_%s_BY_SEARCH_%zu,\n", p->cname, pos++);

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
 * See gen_stmt_joins().
 */
static void
gen_stmt_schema(const struct strct *orig, int first,
	const struct strct *p, const char *pname)
{
	const struct field *f;
	const struct alias *a = NULL;
	int	 c;
	char	*name = NULL;

	/* 
	 * If applicable, looks up our alias and emit it as the alias
	 * for the table.
	 * Otherwise, use the table name itself.
	 */

	printf("\"%s ", 0 == first ? ",\"" : "");

	if (NULL != pname) {
		TAILQ_FOREACH(a, &orig->aq, entries)
			if (0 == strcasecmp(a->name, pname))
				break;
		assert(NULL != a);
		printf("DB_SCHEMA_%s(%s) ", p->cname, a->alias);
	} else
		printf("DB_SCHEMA_%s(%s) ", p->cname, p->name);

	/*
	 * Recursive step.
	 * Search through all of our fields for structures.
	 * If we find them, build up the canonical field reference and
	 * descend.
	 */

	TAILQ_FOREACH(f, &p->fq, entries) {
		if (FTYPE_STRUCT != f->type ||
		    FIELD_NULL & f->ref->source->flags)
			continue;

		if (NULL != pname) {
			c = asprintf(&name, 
				"%s.%s", pname, f->name);
			if (c < 0)
				err(EXIT_FAILURE, NULL);
		} else if (NULL == (name = strdup(f->name)))
			err(EXIT_FAILURE, NULL);

		gen_stmt_schema(orig, 0,
			f->ref->target->parent, name);
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
 */
static void
gen_stmt(const struct strct *p)
{
	const struct search *s;
	const struct sent *sent;
	const struct sref *sr;
	const struct field *f;
	const struct update *up;
	const struct uref *ur;
	const struct oref *or;
	const struct ord *ord;
	int	 first, hastrail;
	size_t	 pos, rc;

	/* 
	 * We have a special query just for our unique fields.
	 * These are generated in the event of null foreign key
	 * reference lookups with the generated db_xxx_reffind()
	 * functions.
	 * TODO: figure out which ones we should be generating and only
	 * do this, as otherwise we're just wasting static space.
	 */

	TAILQ_FOREACH(f, &p->fq, entries) 
		if (FIELD_ROWID & f->flags ||
		    FIELD_UNIQUE & f->flags) {
			printf("\t/* STMT_%s_BY_UNIQUE_%s */\n"
			       "\t\"SELECT ",
				p->cname, f->name);
			gen_stmt_schema(p, 1, p, NULL);
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
		       "\t\"SELECT ",
			p->cname, pos++);

		if (s->dst) {
			printf("DISTINCT ");
			gen_stmt_schema(p, 1,
				s->dst->strct, 
				s->dst->cname);
		} else
			gen_stmt_schema(p, 1, p, NULL);

		hastrail = 
			(! TAILQ_EMPTY(&s->sntq)) ||
			(! TAILQ_EMPTY(&s->ordq)) ||
			(STYPE_SEARCH != s->type && s->limit > 0) ||
			(STYPE_SEARCH != s->type && s->offset > 0);

		printf("\" FROM %s", p->name);
		rc = 0;
		gen_stmt_joins(p, p, NULL, &rc);
		if ( ! hastrail) {
			if (0 == rc)
				putchar('"');
			puts(",");
			continue;
		}

		if (rc > 0) {
			printf("\n\t\t\"");
		} else {
			printf(" \"\n\t\t\"");
		}

		if ( ! TAILQ_EMPTY(&s->sntq))
			printf("WHERE");

		first = 1;
		TAILQ_FOREACH(sent, &s->sntq, entries) {
			sr = TAILQ_LAST(&sent->srq, srefq);
			if (FTYPE_PASSWORD == sr->field->type)
				continue;
			if ( ! first)
				printf(" AND");
			first = 0;
			if (OPTYPE_ISUNARY(sent->op))
				printf(" %s.%s %s",
					NULL == sent->alias ?
					p->name : sent->alias->alias,
					sr->name, optypes[sent->op]);
			else
				printf(" %s.%s %s ?", 
					NULL == sent->alias ?
					p->name : sent->alias->alias,
					sr->name, optypes[sent->op]);
		}

		first = 1;
		if ( ! TAILQ_EMPTY(&s->ordq))
			printf(" ORDER BY ");
		TAILQ_FOREACH(ord, &s->ordq, entries) {
			or = TAILQ_LAST(&ord->orq, orefq);
			if ( ! first)
				printf(", ");
			first = 0;
			printf("%s.%s %s",
				NULL == ord->alias ?
				p->name : ord->alias->alias,
				or->name, 
				ORDTYPE_ASC == ord->op ?
				"ASC" : "DESC");
		}

		if (STYPE_SEARCH != s->type && s->limit > 0)
			printf(" LIMIT %" PRId64, s->limit);
		if (STYPE_SEARCH != s->type && s->offset > 0)
			printf(" OFFSET %" PRId64, s->offset);

		puts("\",");
	}

	/* 
	 * Insertion of a new record.
	 * TODO: DEFAULT_VALUES.
	 */

	printf("\t/* STMT_%s_INSERT */\n"
	       "\t\"INSERT INTO %s ", p->cname, p->name);
	first = 1;
	TAILQ_FOREACH(f, &p->fq, entries) {
		if (FTYPE_STRUCT == f->type ||
		    FIELD_ROWID & f->flags)
			continue;
		if (first)
			putchar('(');
		printf("%s%s", first ? "" : ",", f->name);
		first = 0;
	}

	if (0 == first) {
		printf(") VALUES (");
		first = 1;
		TAILQ_FOREACH(f, &p->fq, entries) {
			if (FTYPE_STRUCT == f->type ||
			    FIELD_ROWID & f->flags)
				continue;
			printf("%s?", first ? "" : ",");
			first = 0;
		}
		puts(")\",");
	} else
		puts("DEFAULT VALUES\",");
	
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
			if (MODTYPE_INC == ur->mod) 
				printf("%s = %s + ?", 
					ur->name, ur->name);
			else if (MODTYPE_DEC == ur->mod) 
				printf("%s = %s - ?", 
					ur->name, ur->name);
			else
				printf("%s = ?", ur->name);
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

/*
 * Generate the C source file from "cfg"s structure objects.
 */
void
gen_c_source(const struct config *cfg, int json, 
	int valids, int splitproc, int dbin, 
	const char *header, const char *incls)
{
	const struct strct *p;
	const char	*start;
	size_t		 sz;
	int		 need_kcgi = 0, 
			 need_kcgijson = 0, 
			 need_ksql = 0;

	if (NULL == incls)
		incls = "";

	print_commentt(0, COMMENT_C, 
		"WARNING: automatically generated by "
		"kwebapp " VERSION ".\n"
		"DO NOT EDIT!");

	/* Start with all headers we'll need. */

	puts("#include <sys/queue.h>\n"
	     "\n"
	     "#include <assert.h>");

	TAILQ_FOREACH(p, &cfg->sq, entries) 
		if (STRCT_HAS_BLOB & p->flags) {
			print_commentt(0, COMMENT_C,
				"Required for b64_ntop().");
			puts("#include <netinet/in.h>\n"
			     "#include <resolv.h>");
			break;
		}

	if (valids || strchr(incls, 'v')) {
		puts("#include <stdarg.h>");
		puts("#include <stdint.h>");
	}

	puts("#include <stdio.h>\n"
	     "#include <stdlib.h>\n"
	     "#include <string.h>\n"
	     "#include <unistd.h>\n"
	     "");

	if (dbin || strchr(incls, 'b'))
		need_ksql = 1;
	if (valids || strchr(incls, 'v'))
		need_kcgi = 1;
	if (json || strchr(incls, 'j'))
		need_kcgi = need_kcgijson = 1;

	if (need_ksql)
		puts("#include <ksql.h>");
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

		if (CFG_HAS_ROLES & cfg->flags) {
			print_commentt(0, COMMENT_C,
				"Definition of our opaque \"kwbp\", "
				"which contains role information.");
			puts("struct\tkwbp {");
			print_commentt(1, COMMENT_C,
				"Hidden database connection");
			puts("\tstruct ksql *db;");
			print_commentt(1, COMMENT_C,
				"Current RBAC role.");
			puts("\tenum kwbp_role role;\n"
			     "};\n");

			print_commentt(0, COMMENT_C,
				"A saved role state attached to "
				"generated objects.\n"
				"We'll use this to make sure that "
				"we shouldn't export data that "
				"we've kept unexported in a given "
				"role (at the time of acquisition).");
			puts("struct\tkwbp_store {");
			print_commentt(1, COMMENT_C,
				"Role at the time of acquisition.");
			puts("\tenum kwbp_role role;\n"
			     "};\n");

			print_commentt(0, COMMENT_C,
				"Define our table columns.\n"
				"Since we're using roles, this is "
				"all internal to the source and not "
				"exported.");
			TAILQ_FOREACH(p, &cfg->sq, entries)
				print_define_schema(p);
			puts("");
		}

		print_commentt(0, COMMENT_C,
			"Our full set of SQL statements.\n"
			"We define these beforehand because "
			"that's how ksql(3) handles statement "
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
		gen_func_open(cfg, splitproc);
		gen_func_close(cfg);
		if (CFG_HAS_ROLES & cfg->flags)
			gen_func_roles(cfg);
	}

	TAILQ_FOREACH(p, &cfg->sq, entries)
		gen_funcs(cfg, p, json, valids, dbin);
}
