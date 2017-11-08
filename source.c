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
	/* Unary types... */
	"ISNULL", /* OPTYPE_ISNULL */
	"NOTNULL", /* OPTYPE_NOTNULL */
};

/*
 * Functions extracting from a statement.
 * Note that FTYPE_TEXT and FTYPE_PASSWORD need a surrounding strdup.
 */
static	const char *const coltypes[FTYPE__MAX] = {
	"ksql_stmt_int", /* FTYPE_BIT */
	"ksql_stmt_int", /* FTYPE_EPOCH */
	"ksql_stmt_int", /* FTYPE_INT */
	"ksql_stmt_double", /* FTYPE_REAL */
	"ksql_stmt_blob", /* FTYPE_BLOB (XXX: is special) */
	"ksql_stmt_str", /* FTYPE_TEXT */
	"ksql_stmt_str", /* FTYPE_PASSWORD */
	"ksql_stmt_str", /* FTYPE_EMAIL */
	NULL, /* FTYPE_STRUCT */
	"ksql_stmt_int", /* FTYPE_ENUM */
};

static	const char *const puttypes[FTYPE__MAX] = {
	"kjson_putintp", /* FTYPE_BIT */
	"kjson_putintp", /* FTYPE_EPOCH */
	"kjson_putintp", /* FTYPE_INT */
	"kjson_putdoublep", /* FTYPE_REAL */
	"kjson_putstringp", /* FTYPE_BLOB (XXX: is special) */
	"kjson_putstringp", /* FTYPE_TEXT */
	NULL, /* FTYPE_PASSWORD (don't print) */
	"kjson_putstringp", /* FTYPE_EMAIL */
	NULL, /* FTYPE_STRUCT */
	"kjson_putintp", /* FTYPE_ENUM */
};

/*
 * Functions binding an argument to a statement.
 */
static	const char *const bindtypes[FTYPE__MAX] = {
	"ksql_bind_int", /* FTYPE_BIT */
	"ksql_bind_int", /* FTYPE_EPOCH */
	"ksql_bind_int", /* FTYPE_INT */
	"ksql_bind_double", /* FTYPE_REAL */
	"ksql_bind_blob", /* FTYPE_BLOB (XXX: is special) */
	"ksql_bind_str", /* FTYPE_TEXT */
	"ksql_bind_str", /* FTYPE_PASSWORD */
	"ksql_bind_str", /* FTYPE_EMAIL */
	NULL, /* FTYPE_STRUCT */
	"ksql_bind_int", /* FTYPE_ENUM */
};

/*
 * Basic validation functions for given types.
 */
static	const char *const validtypes[FTYPE__MAX] = {
	"kvalid_bit", /* FTYPE_BIT */
	"kvalid_int", /* FTYPE_EPOCH */
	"kvalid_int", /* FTYPE_INT */
	"kvalid_double", /* FTYPE_REAL */
	NULL, /* FTYPE_BLOB */
	"kvalid_string", /* FTYPE_TEXT */
	"kvalid_string", /* FTYPE_PASSWORD */
	"kvalid_email", /* FTYPE_EMAIL */
	NULL, /* FTYPE_STRUCT */
	"kvalid_int", /* FTYPE_ENUM */
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
 * When accepting only given roles, print the roles rooted at "r".
 * Don't print out the ROLE_all, but continue through it.
 */
static void
gen_role(const struct role *r)
{
	const struct role *rr;
	const char	*cp;

	if (strcasecmp(r->name, "all")) {
		printf("\tcase ROLE_");
		for (cp = r->name; '\0' != *cp; cp++)
			putchar(tolower((unsigned char)*cp));
		puts(":");
	}
	TAILQ_FOREACH(rr, &r->subrq, entries)
		gen_role(rr);
}

/*
 * When accepting only given roles, recursively print all of those roles
 * out now.
 * Also print a comment stating what we're doing.
 */
static void
gen_rolemap(const struct rolemap *rm)
{
	const struct roleset *rs;

	if (NULL == rm) {
		puts("");
		print_commentt(1, COMMENT_C, 
			"No roles were defined for this function "
			"and we're in RBAC mode, so abort() if "
			"this function is called.\n"
			"XXX: this is probably not what you want.");
		puts("\n"
		     "\tabort();\n");
		return;
	}

	puts("");
	print_commentt(1, COMMENT_C, 
		"Restrict to allowed roles.");
	puts("\n"
	     "\tswitch (ctx->role) {");
	TAILQ_FOREACH(rs, &rm->setq, entries)
		gen_role(rs->role);
	puts("\t\tbreak;\n"
	     "\tdefault:\n"
	     "\t\tabort();\n"
	     "\t}\n");
}

/*
 * Fill an individual field from the database.
 */
static void
gen_strct_fill_field(const struct field *f)
{
	size_t	 indent;

	if (FTYPE_STRUCT == f->type)
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

	if (FTYPE_BLOB == f->type) {
		/* 
		 * First, prepare for the data.
		 * We get the data size, then allocate space enough to
		 * fill with that blob.
		 * Error-check the allocation, of course.
		 */

		if (FIELD_NULL & f->flags) {
			printf("\tif (p->has_%s) {\n", f->name);
			indent = 2;
		} else
			indent = 1;

		print_src(indent,
			"p->%s_sz = ksql_stmt_bytes(stmt, *pos);\n"
		        "p->%s = malloc(p->%s_sz);\n"
		        "if (NULL == p->%s) {\n"
		        "perror(NULL);\n"
		        "exit(EXIT_FAILURE);\n"
		        "}\n"
			"memcpy(p->%s, %s(stmt, (*pos)++), p->%s_sz);",
			f->name, f->name, f->name, f->name,
			f->name, coltypes[f->type], f->name);

		if (FIELD_NULL & f->flags) 
			puts("\t} else\n"
			     "\t\t(*pos)++;");
	} else {
		/*
		 * Assign from the database.
		 * Text fields use a strdup for the allocation.
		 */

		if (FIELD_NULL & f->flags)
			printf("\tif (p->has_%s)\n"
			       "\t\tp->%s = ", f->name, f->name);
		else 
			printf("\tp->%s = ", f->name);

		if (FTYPE_TEXT == f->type || 
		    FTYPE_PASSWORD == f->type ||
		    FTYPE_EMAIL == f->type)
			printf("strdup(%s(stmt, (*pos)++));\n", 
				coltypes[f->type]);
		else
			printf("%s(stmt, (*pos)++);\n", 
				coltypes[f->type]);

		/* NULL check for allocation. */

		if (FIELD_NULL & f->flags) {
			puts("\telse\n"
			     "\t\t(*pos)++;");
			if (FTYPE_TEXT == f->type || 
			    FTYPE_PASSWORD == f->type ||
			    FTYPE_EMAIL == f->type)
				printf("\tif (p->has_%s && "
					"NULL == p->%s) {\n"
				       "\t\tperror(NULL);\n"
				       "\t\texit(EXIT_FAILURE);\n"
				       "\t}\n", f->name, f->name);
		} else if (FTYPE_TEXT == f->type || 
			   FTYPE_PASSWORD == f->type ||
			   FTYPE_EMAIL == f->type) 
			printf("\tif (NULL == p->%s) {\n"
			       "\t\tperror(NULL);\n"
			       "\t\texit(EXIT_FAILURE);\n"
			       "\t}\n", f->name);
	}
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
	size_t	 pos;

	assert(STYPE_ITERATE == s->type);

	print_func_db_search(s, CFG_HAS_ROLES & cfg->flags, 0);
	printf("\n"
	       "{\n"
	       "\tstruct ksqlstmt *stmt;\n"
	       "\tstruct %s p;\n",
	       s->parent->name);
	if (CFG_HAS_ROLES & cfg->flags)
		puts("\tstruct ksql *db = ctx->db;");

	if (CFG_HAS_ROLES & cfg->flags)
		gen_rolemap(s->rolemap);
	else
		puts("");

	printf("\tksql_stmt_alloc(db, &stmt,\n"
	       "\t\tstmts[STMT_%s_BY_SEARCH_%zu],\n"
	       "\t\tSTMT_%s_BY_SEARCH_%zu);\n",
	       s->parent->cname, num, 
	       s->parent->cname, num);

	pos = 1;
	TAILQ_FOREACH(sent, &s->sntq, entries)
		if (OPTYPE_ISBINARY(sent->op)) {
			sr = TAILQ_LAST(&sent->srq, srefq);
			gen_bindfunc(sr->field->type, pos++, 0);
		}

	printf("\twhile (KSQL_ROW == ksql_stmt_step(stmt)) {\n"
	       "\t\tdb_%s_fill_r(&p, stmt, NULL);\n",
	       s->parent->name);

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
	       "\n",
	       s->parent->name);
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
	size_t	 pos;

	assert(STYPE_LIST == s->type);

	print_func_db_search(s, CFG_HAS_ROLES & cfg->flags, 0);
	printf("\n"
	       "{\n"
	       "\tstruct ksqlstmt *stmt;\n"
	       "\tstruct %s_q *q;\n"
	       "\tstruct %s *p;\n",
	       s->parent->name, s->parent->name);
	if (CFG_HAS_ROLES & cfg->flags)
		puts("\tstruct ksql *db = ctx->db;");

	if (CFG_HAS_ROLES & cfg->flags)
		gen_rolemap(s->rolemap);
	else
		puts("");

	printf("\tq = malloc(sizeof(struct %s_q));\n"
	       "\tif (NULL == q) {\n"
	       "\t\tperror(NULL);\n"
	       "\t\texit(EXIT_FAILURE);\n"
	       "\t}\n"
	       "\tTAILQ_INIT(q);\n"
	       "\n"
	       "\tksql_stmt_alloc(db, &stmt,\n"
	       "\t\tstmts[STMT_%s_BY_SEARCH_%zu],\n"
	       "\t\tSTMT_%s_BY_SEARCH_%zu);\n",
	       s->parent->name, s->parent->cname, num, 
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
	       "\t\tp = calloc(1, sizeof(struct %s));\n"
	       "\t\tif (NULL == p) {\n"
	       "\t\t\tperror(NULL);\n"
	       "\t\t\texit(EXIT_FAILURE);\n"
	       "\t\t}\n"
	       "\t\tdb_%s_fill_r(p, stmt, NULL);\n",
	       s->parent->name, s->parent->name);

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
 * Generate database opening.
 * We don't use the generic invocation, as we want foreign keys.
 * If "splitproc" is specified, use ksql_alloc_child instead of the
 * usual allocation method.
 */
static void
gen_func_open(const struct config *cfg, int splitproc)
{

	print_func_db_open(CFG_HAS_ROLES & cfg->flags, 0);

	puts("{\n"
	     "\tstruct ksqlcfg cfg;\n"
	     "\tstruct ksql *db;");

	if (CFG_HAS_ROLES & cfg->flags)
		puts("\tstruct kwbp *ctx;\n"
		     "\n"
		     "\tctx = malloc(sizeof(struct kwbp));\n"
		     "\tif (NULL == ctx)\n"
		     "\t\treturn(NULL);");

	puts("\n"
	     "\tmemset(&cfg, 0, sizeof(struct ksqlcfg));\n"
	     "\tcfg.flags = KSQL_EXIT_ON_ERR |\n"
	     "\t\tKSQL_FOREIGN_KEYS | KSQL_SAFE_EXIT;\n"
	     "\tcfg.err = ksqlitemsg;\n"
	     "\tcfg.dberr = ksqlitedbmsg;\n"
	     "");
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
	const char *cp;
	const struct role *rp, *rr;
	size_t has = 0;

	printf("\tcase ROLE_");
	for (cp = r->name; '\0' != *cp; cp++) 
		putchar(tolower((unsigned char)*cp));
	puts(":");
	puts("\t\tswitch (ctx->role) {");
	for (rp = r->parent; NULL != rp; rp = rp->parent) {
		if (NULL == rp->parent) {
			puts("\t\tcase ROLE_default:");
			has++;
			break;
		}
		printf("\t\tcase ROLE_");
		for (cp = rp->name; '\0' != *cp; cp++) 
			putchar(tolower((unsigned char)*cp));
		puts(":");
		has++;
	}
	if (has)
		puts("\t\t\tctx->role = r;\n"
		     "\t\t\treturn;");
	puts("\t\tdefault:\n"
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
		if (0 == strcasecmp(r->name, "all"))
			break;
	assert(NULL != r);

	print_func_db_role(0);
	puts("{\n"
	     "\n"
	     "\tif (r == ctx->role)\n"
	     "\t\treturn;\n"
	     "\tif (ROLE_none == ctx->role)\n"
	     "\t\tabort();\n"
	     "\n"
	     "\tswitch (r) {\n"
	     "\tcase ROLE_none:\n"
	     "\t\tctx->role = r;\n"
	     "\t\treturn;");
	TAILQ_FOREACH(rr, &r->subrq, entries)
		gen_func_rolecases(rr);
	puts("\tdefault:\n"
	     "\t\tabort();\n"
	     "\t}\n"
	     "}\n");
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
	size_t	 pos;

	assert(STYPE_SEARCH == s->type);

	print_func_db_search(s, CFG_HAS_ROLES & cfg->flags, 0);
	printf("\n"
	       "{\n"
	       "\tstruct ksqlstmt *stmt;\n"
	       "\tstruct %s *p = NULL;\n",
	       s->parent->name);
	if (CFG_HAS_ROLES & cfg->flags)
		puts("\tstruct ksql *db = ctx->db;");

	if (CFG_HAS_ROLES & cfg->flags)
		gen_rolemap(s->rolemap);
	else
		puts("");

	printf("\tksql_stmt_alloc(db, &stmt,\n"
	       "\t\tstmts[STMT_%s_BY_SEARCH_%zu],\n"
	       "\t\tSTMT_%s_BY_SEARCH_%zu);\n",
	       s->parent->cname, num, s->parent->cname, num);

	pos = 1;
	TAILQ_FOREACH(sent, &s->sntq, entries) 
		if (OPTYPE_ISBINARY(sent->op)) {
			sr = TAILQ_LAST(&sent->srq, srefq);
			gen_bindfunc(sr->field->type, pos++, 0);
		}

	printf("\tif (KSQL_ROW == ksql_stmt_step(stmt)) {\n"
	       "\t\tp = calloc(1, sizeof(struct %s));\n"
	       "\t\tif (NULL == p) {\n"
	       "\t\t\tperror(NULL);\n"
	       "\t\t\texit(EXIT_FAILURE);\n"
	       "\t\t}\n"
	       "\t\tdb_%s_fill_r(p, stmt, NULL);\n",
	       s->parent->name, s->parent->name);

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
	     "\treturn(p);\n"
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

	/* Check our roles. */

	if (CFG_HAS_ROLES & cfg->flags)
		gen_rolemap(p->irolemap);
	else
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

	printf("\tksql_stmt_alloc(db, &stmt,\n"
	       "\t\tstmts[STMT_%s_INSERT],\n"
	       "\t\tSTMT_%s_INSERT);\n",
	       p->cname, p->cname);

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
gen_func_unfill(const struct strct *p)
{
	const struct field *f;

	print_func_db_unfill(p, 0);
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
	TAILQ_FOREACH(f, &p->fq, entries)
		if (FTYPE_STRUCT == f->type)
			printf("\tdb_%s_unfill_r(&p->%s);\n",
				f->ref->tstrct, f->name);
	puts("}\n"
	     "");
}

/*
 * Generate the "fill" function.
 */
static void
gen_func_fill_r(const struct strct *p)
{
	const struct field *f;

	printf("static void\n"
	       "db_%s_fill_r(struct %s *p, "
	       "struct ksqlstmt *stmt, size_t *pos)\n"
	       "{\n"
	       "\tsize_t i = 0;\n"
	       "\n"
	       "\tif (NULL == pos)\n"
	       "\t\tpos = &i;\n"
	       "\tdb_%s_fill(p, stmt, pos);\n",
	       p->name, p->name, p->name);
	TAILQ_FOREACH(f, &p->fq, entries)
		if (FTYPE_STRUCT == f->type)
			printf("\tdb_%s_fill_r(&p->%s, "
				"stmt, pos);\n", 
				f->ref->tstrct, f->name);
	puts("}\n"
	     "");
}

/*
 * Generate the "fill" function.
 */
static void
gen_func_fill(const struct strct *p)
{
	const struct field *f;

	print_func_db_fill(p, 0);
	puts("\n"
	     "{\n"
	     "\tsize_t i = 0;\n"
	     "\n"
	     "\tif (NULL == pos)\n"
	     "\t\tpos = &i;\n"
	     "\tmemset(p, 0, sizeof(*p));");
	TAILQ_FOREACH(f, &p->fq, entries)
		gen_strct_fill_field(f);
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

	if (CFG_HAS_ROLES & cfg->flags)
		gen_rolemap(up->rolemap);
	else
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
		printf("\tksql_stmt_alloc(db, &stmt,\n"
		       "\t\tstmts[STMT_%s_UPDATE_%zu],\n"
		       "\t\tSTMT_%s_UPDATE_%zu);\n",
		       up->parent->cname, num, 
		       up->parent->cname, num);
	else
		printf("\tksql_stmt_alloc(db, &stmt,\n"
		       "\t\tstmts[STMT_%s_DELETE_%zu],\n"
		       "\t\tSTMT_%s_DELETE_%zu);\n",
		       up->parent->cname, num, 
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
	case (FTYPE_EPOCH):
	case (FTYPE_INT):
		printf("\tif (p->parsed.i %s %" PRId64 ")\n"
		       "\t\treturn(0);\n",
		       validbins[v->type], v->d.value.integer);
		break;
	case (FTYPE_REAL):
		printf("\tif (p->parsed.d %s %g)\n"
		       "\t\treturn(0);\n",
		       validbins[v->type], v->d.value.decimal);
		break;
	default:
		printf("\tif (p->valsz %s %zu)\n"
		       "\t\treturn(0);\n",
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
			       "\t\treturn(0);\n",
			       validtypes[f->type]);

		/* Enumeration: check against knowns. */

		if (FTYPE_ENUM == f->type) {
			puts("\tswitch(p->parsed.i) {");
			TAILQ_FOREACH(ei, &f->eref->enm->eq, entries)
				printf("\t\tcase %" PRId64 ":\n",
					ei->value);
			puts("\t\t\tbreak;\n"
			     "\t\tdefault:\n"
			     "\t\t\treturn(0);\n"
			     "\t}");
		}

		TAILQ_FOREACH(v, &f->fvq, entries) 
			gen_func_valid_types(f, v);
		puts("\treturn(1);");
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

static void
gen_func_json_data(const struct strct *p)
{
	const struct field *f;
	size_t	 pos;

	print_func_json_data(p, 0);
	puts("\n"
	     "{");

	/* Declare our base64 buffers. */

	pos = 0;
	TAILQ_FOREACH(f, &p->fq, entries)
		if (FTYPE_BLOB == f->type &&
		    ! (FIELD_NOEXPORT & f->flags)) 
			printf("\tchar *buf%zu;\n", ++pos);

	if (pos > 0)
		puts("\tsize_t sz;\n");

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

	if (pos > 0)
		puts("");

	pos = 0;
	TAILQ_FOREACH(f, &p->fq, entries) {
		if (FIELD_NOEXPORT & f->flags) {
			print_commentv(1, COMMENT_C, "Omitting %s: "
				"marked no export.", f->name);
			continue;
		} else if (FTYPE_PASSWORD == f->type) {
			print_commentv(1, COMMENT_C, "Omitting %s: "
				"is a password hash.", f->name);
			continue;
		}
		if (FIELD_NULL & f->flags)
			printf("\tif ( ! p->has_%s)\n"
			       "\t\tkjson_putnullp(r, \"%s\");\n"
			       "\telse\n"
			       "\t",
			       f->name, f->name);
		if (FTYPE_BLOB == f->type)
			printf("\t%s(r, \"%s\", buf%zu);\n",
				puttypes[f->type], f->name, ++pos);
		else if (FTYPE_STRUCT == f->type)
			printf("\tkjson_objp_open(r, \"%s\");\n"
			       "\tjson_%s_data(r, &p->%s);\n"
			       "\tkjson_obj_close(r);\n",
				f->name, f->ref->tstrct, f->name);
		else
			printf("\t%s(r, \"%s\", p->%s);\n", 
				puttypes[f->type], f->name, f->name);
	}

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
gen_funcs(const struct config *cfg, 
	const struct strct *p, int json, int valids)
{
	const struct search *s;
	const struct update *u;
	size_t	 pos;

	gen_func_fill_r(p);
	gen_func_fill(p);
	gen_func_unfill_r(p);
	gen_func_unfill(p);
	gen_func_free(p);
	gen_func_freeq(p);

	if (STRCT_HAS_INSERT & p->flags)
		gen_func_insert(cfg, p);

	if (json) {
		gen_func_json_data(p);
		gen_func_json_obj(p);
	}

	if (valids)
		gen_func_valids(p);

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
	size_t	 pos;

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
gen_stmt_schema(const struct strct *orig, 
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

	if (NULL != pname) {
		TAILQ_FOREACH(a, &orig->aq, entries)
			if (0 == strcasecmp(a->name, pname))
				break;
		assert(NULL != a);
		printf("\",\" DB_SCHEMA_%s(%s) ", p->cname, a->alias);
	} else
		printf("\" DB_SCHEMA_%s(%s) ", p->cname, p->name);

	/*
	 * Recursive step.
	 * Search through all of our fields for structures.
	 * If we find them, build up the canonical field reference and
	 * descend.
	 */

	TAILQ_FOREACH(f, &p->fq, entries) {
		if (FTYPE_STRUCT != f->type)
			continue;

		if (NULL != pname) {
			c = asprintf(&name, 
				"%s.%s", pname, f->name);
			if (c < 0)
				err(EXIT_FAILURE, NULL);
		} else if (NULL == (name = strdup(f->name)))
			err(EXIT_FAILURE, NULL);

		gen_stmt_schema(orig, 
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
gen_stmt_joins(const struct strct *orig, 
	const struct strct *p, const struct alias *parent)
{
	const struct field *f;
	const struct alias *a;
	int	 c;
	char	*name;

	TAILQ_FOREACH(f, &p->fq, entries) {
		if (FTYPE_STRUCT != f->type)
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

		printf(" INNER JOIN %s AS %s ON %s.%s=%s.%s",
			f->ref->tstrct, a->alias,
			a->alias, f->ref->tfield,
			NULL == parent ? p->name : parent->alias,
			f->ref->sfield);
		gen_stmt_joins(orig, 
			f->ref->target->parent, 
			a);
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
	int	 first;
	size_t	 pos;

	/* 
	 * Print custom search queries.
	 * This also uses the recursive selection.
	 */

	pos = 0;
	TAILQ_FOREACH(s, &p->sq, entries) {
		printf("\t/* STMT_%s_BY_SEARCH_%zu */\n"
		       "\t\"SELECT ",
			p->cname, pos++);
		gen_stmt_schema(p, p, NULL);
		printf("\" FROM %s", p->name);
		gen_stmt_joins(p, p, NULL);
		if (TAILQ_EMPTY(&s->sntq)) {
			puts("\",");
			continue;
		}
		printf(" WHERE");
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
		if (STYPE_SEARCH != s->type && s->limit > 0)
			printf(" LIMIT %" PRId64, s->limit);
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
		printf(" WHERE");
		first = 1;
		TAILQ_FOREACH(ur, &up->crq, entries) {
			printf("%s", first ? " " : " AND ");
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
		       "\t\"DELETE FROM %s WHERE",
		       p->cname, pos++, p->name);
		first = 1;
		TAILQ_FOREACH(ur, &up->crq, entries) {
			printf("%s", first ? " " : " AND ");
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
 * If "json" is non-zero, this generates the JSON formatters.
 * If "valids" is non-zero, this generates the field validators.
 * The "header" is what's noted as an inclusion.
 * (Your header file, see gen_c_header, should have the same name.)
 */
void
gen_c_source(const struct config *cfg, int json, 
	int valids, int splitproc, const char *header)
{
	const struct strct *p;

	print_commentt(0, COMMENT_C, 
		"WARNING: automatically generated by "
		"kwebapp " VERSION ".\n"
		"DO NOT EDIT!");

	/* Start with all headers we'll need. */

	puts("#include <sys/queue.h>");

	TAILQ_FOREACH(p, &cfg->sq, entries) 
		if (STRCT_HAS_BLOB & p->flags) {
			print_commentt(0, COMMENT_C,
				"Required for b64_ntop().");
			puts("#include <netinet/in.h>\n"
			     "#include <resolv.h>");
			break;
		}

	puts("");
	if (valids) {
		puts("#include <stdarg.h>");
		puts("#include <stdint.h>");
	}
	puts("#include <stdio.h>\n"
	     "#include <stdlib.h>\n"
	     "#include <string.h>\n"
	     "#include <unistd.h>\n"
	     "\n"
	     "#include <ksql.h>");
	if (valids)
		puts("#include <kcgi.h>");
	if (json)
		puts("#include <kcgijson.h>");


	printf("\n"
	       "#include \"%s\"\n"
	       "\n",
	       header);

	/* Enumeration for statements. */

	print_commentt(0, COMMENT_C,
		"All SQL statements we'll define in \"stmts\".");
	puts("enum\tstmt {");
	TAILQ_FOREACH(p, &cfg->sq, entries)
		gen_enum(p);
	puts("\tSTMT__MAX\n"
	     "};\n"
	     "");

	if (CFG_HAS_ROLES & cfg->flags) {
		print_commentt(0, COMMENT_C,
			"Definition of our opaque \"kwbp\", which "
			"contains role information.");
		puts("struct\tkwbp {");
		print_commentt(1, COMMENT_C,
			"Hidden database connection");
		puts("\tstruct ksql *db;");
		print_commentt(1, COMMENT_C,
			"Current RBAC role.");
		puts("\tenum kwbp_role role;\n"
		     "};\n");
	}

	/* Now the array of all statement. */

	print_commentt(0, COMMENT_C,
		"Our full set of SQL statements.\n"
		"We define these beforehand because that's how ksql "
		"handles statement generation.\n"
		"Notice the \"AS\" part: this allows for multiple "
		"inner joins without ambiguity.");
	puts("static\tconst char *const stmts[STMT__MAX] = {");
	TAILQ_FOREACH(p, &cfg->sq, entries)
		gen_stmt(p);
	puts("};");
	puts("");

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

	gen_func_open(cfg, splitproc);
	gen_func_close(cfg);
	if (CFG_HAS_ROLES & cfg->flags)
		gen_func_roles(cfg);

	TAILQ_FOREACH(p, &cfg->sq, entries)
		gen_funcs(cfg, p, json, valids);
}
