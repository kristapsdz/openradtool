/*	$Id$ */
/*
 * Copyright (c) 2017--2020 Kristaps Dzonsons <kristaps@bsd.lv>
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
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "paths.h"
#include "ort.h"
#include "ort-lang-c.h"
#include "ort-version.h"
#include "lang.h"
#include "lang-c.h"

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
	"kjson_putintstrp", /* FTYPE_BIT */
	"kjson_putintstrp", /* FTYPE_DATE */
	"kjson_putintstrp", /* FTYPE_EPOCH */
	"kjson_putintstrp", /* FTYPE_INT */
	"kjson_putdoublep", /* FTYPE_REAL */
	"kjson_putstringp", /* FTYPE_BLOB (XXX: is special) */
	"kjson_putstringp", /* FTYPE_TEXT */
	NULL, /* FTYPE_PASSWORD (don't print) */
	"kjson_putstringp", /* FTYPE_EMAIL */
	NULL, /* FTYPE_STRUCT */
	"kjson_putintstrp", /* FTYPE_ENUM */
	"kjson_putintstrp", /* FTYPE_BITFIELD */
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

static	int print_src(FILE *, size_t, const char *, ...)
	__attribute__((format(printf, 3, 4)));

/*
 * Print the line of source code given by "fmt" and varargs.
 * This is indented according to "indent", which changes depending on
 * whether there are curly braces around a newline.
 * This is useful when printing the same text with different
 * indentations (e.g., when being surrounded by a conditional).
 * Return zero on failure, non-zero on success.
 */
static int
print_src(FILE *f, size_t indent, const char *fmt, ...)
{
	va_list	 ap;
	char	*cp;
	int	 ret;
	size_t	 i, pos;

	va_start(ap, fmt);
	ret = vasprintf(&cp, fmt, ap);
	va_end(ap);
	if (ret == -1)
		return 0;

	for (pos = 0; cp[pos] != '\0'; pos++) {
		if (pos == 0 || cp[pos] == '\n') {
			if (pos > 0 && cp[pos - 1] == '{')
				indent++;
			if (cp[pos + 1] == '}')
				indent--;
			if (pos > 0 && fputc('\n', f) == EOF)
				return 0;
			for (i = 0; i < indent; i++)
				if (fputc('\t', f) == EOF)
					return 0;
		} 

		if (cp[pos] != '\n' && fputc(cp[pos], f) == EOF)
			return 0;
	} 

	free(cp);
	return fputc('\n', f) != EOF;
}

/*
 * Generate the function for checking a password.
 * This should be a conditional phrase that evalutes to FALSE if the
 * password does NOT match the given type, TRUE if the password does
 * match the given type.
 * FIXME: use crypt_checkpass always.
 * Return zero on failure, non-zero on success.
 */
static int
gen_checkpass(FILE *f, int ptr, size_t pos,
	const char *name, enum optype type, const struct field *fd)
{
	const char	*s = ptr ? "->" : ".";

	assert(type == OPTYPE_EQUAL || type == OPTYPE_NEQUAL);

	if (fprintf(f, "(%s", type == OPTYPE_NEQUAL ? "!(" : "") < 0)
		return 0;

	if (fd->flags & FIELD_NULL) {
		if (fprintf(f, 
		    "(v%zu == NULL && p%shas_%s) ||\n\t\t    "
		    "(v%zu != NULL && !p%shas_%s) ||\n\t\t    "
		    "(v%zu != NULL && p%shas_%s && ",
		    pos, s, name, pos, s, name, pos, s, name) < 0)
			return 0;
#ifdef __OpenBSD__
		if (fprintf(f, 
		    "crypt_checkpass(v%zu, p%s%s) == -1)", 
		    pos, s, name) < 0)
			return 0;
#else
		if (fprintf(f, 
		    "strcmp(crypt(v%zu, p%s%s), p%s%s) != 0)", 
		    pos, s, name, s, name) < 0)
			return 0;
#endif
	} else {
		if (fprintf(f, "v%zu == NULL || ", pos) < 0)
			return 0;
#ifdef __OpenBSD__
		if (fprintf(f, 
		    "crypt_checkpass(v%zu, p%s%s) == -1", 
		    pos, s, name) < 0)
			return 0;
#else
		if (fprintf(f,
		    "strcmp(crypt(v%zu, p%s%s), p%s%s) != 0", 
		    pos, s, name, s, name) < 0)
			return 0;
#endif
	}

	return fprintf(f, "%s)", 
		type == OPTYPE_NEQUAL ? ")" : "") > 0;
}

/*
 * Generate the function for creating a password hash.
 * FIXME: use crypt_newhash() always.
 * Return zero on failure, non-zero on success.
 */
static int
gen_newpass(FILE *f, int ptr, size_t pos, size_t npos)
{

#ifdef __OpenBSD__
	if (fprintf(f,
	    "\tcrypt_newhash(%sv%zu, \"blowfish,a\", "
	    "hash%zu, sizeof(hash%zu));\n",
	    ptr ? "*" : "", npos, pos, pos) < 0)
		return 0;
#else
	if (fprintf(f,
	    "\tstrncpy(hash%zu, crypt(%sv%zu, _gensalt()), "
	    "sizeof(hash%zu));\n",
	    pos, ptr ? "*" : "", npos, pos) < 0)
		return 0;
#endif
	return 1;
}

/*
 * When accepting only given roles, print the roles rooted at "r".
 * Return zero on failure, non-zero on success.
 */
static int
gen_role(FILE *f, const struct role *r)
{
	const struct role	*rr;

	if (strcmp(r->name, "all") != 0 &&
	    fprintf(f, "\tcase ROLE_%s:\n", r->name) < 0)
		return 0;

	TAILQ_FOREACH(rr, &r->subrq, entries)
		if (!gen_role(f, rr))
			return 0;

	return 1;
}

/*
 * Fill an individual field from the database in gen_fill().
 * Return zero on failure, non-zero on success.
 */
static int
gen_fill_field(FILE *f, const struct field *fd)
{
	size_t	 		 indent;

	/*
	 * By default, structs on possibly-null foreign keys are set as
	 * not existing.
	 * We'll change this in db_xxx_reffind.
	 */

	if (fd->type == FTYPE_STRUCT &&
	    (fd->ref->source->flags & FIELD_NULL))
		return fprintf(f, "\tp->has_%s = 0;\n", fd->name) > 0;
	else if (fd->type == FTYPE_STRUCT)
		return 1;

	if ((fd->flags & FIELD_NULL) && !print_src(f, 1, 
	     "p->has_%s = set->ps[*pos].type != "
	     "SQLBOX_PARM_NULL;", fd->name))
		return 0;

	/*
	 * Blob types need to have space allocated (and the space
	 * variable set) before we extract from the database.
	 * This sequence is very different from the other types, so make
	 * it into its own conditional block for clarity.
	 */

	if ((fd->flags & FIELD_NULL)) {
		if (fprintf(f, "\tif (p->has_%s) {\n", fd->name) < 0)
			return 0;
		indent = 2;
	} else
		indent = 1;

	switch (fd->type) {
	case FTYPE_BLOB:
		if (!print_src(f, indent, 
		    "if (%s(&set->ps[(*pos)++],\n"
		    "    &p->%s, &p->%s_sz) == -1)\n"
		    "\texit(EXIT_FAILURE);", coltypes[fd->type], 
		    fd->name, fd->name))
			return 0;
		break;
	case FTYPE_DATE:
	case FTYPE_ENUM:
	case FTYPE_EPOCH:
		if (!print_src(f, indent,
		    "if (%s(&set->ps[(*pos)++], &tmpint) == -1)\n"
		    "\texit(EXIT_FAILURE);\n"
		    "p->%s = tmpint;",
		    coltypes[fd->type], fd->name))
			return 0;
		break;
	case FTYPE_BIT:
	case FTYPE_BITFIELD:
	case FTYPE_INT:
		if (!print_src(f, indent,
		    "if (%s(&set->ps[(*pos)++], &ORT_GET_%s_%s(p)) == -1)\n"
		    "\texit(EXIT_FAILURE);",
		    coltypes[fd->type], fd->parent->name, fd->name))
			return 0;
		break;
	case FTYPE_REAL:
		if (!print_src(f, indent,
		    "if (%s(&set->ps[(*pos)++], &p->%s) == -1)\n"
		    "\texit(EXIT_FAILURE);",
		    coltypes[fd->type], fd->name))
			return 0;
		break;
	default:
		if (!print_src(f, indent,
		    "if (%s\n"
		    "    (&set->ps[(*pos)++], &p->%s, NULL) == -1)\n"
		    "\texit(EXIT_FAILURE);",
		    coltypes[fd->type], fd->name))
			return 0;
		break;
	}

	if ((fd->flags & FIELD_NULL) &&
	    fputs("\t} else\n\t\t(*pos)++;\n", f) == EOF)
		return 0;

	return 1;
}

/*
 * Counts how many entries are required if later passed to gen_bind().
 * The ones we don't pass to gen_bind() are passwords that are using the
 * hashing functions.
 */
static size_t
count_bind(enum ftype t, enum optype type)
{

	assert(t != FTYPE_STRUCT);
	return !(t == FTYPE_PASSWORD && 
		type != OPTYPE_STREQ && type != OPTYPE_STRNEQ);

}

/*
 * Generate the binding for a field of type "t" at index "idx" referring
 * to variable "pos" with a tab offset of "tabs", using
 * count_bind() to see if we should skip the binding.
 * Return -1 on failure, 0 if not bound, 1 otherwise.
 */
static int
gen_bind(FILE *f, const struct field *fd, size_t idx,
	size_t pos, int ptr, size_t tabs, enum optype type)
{
	size_t	 		 i;

	if (count_bind(fd->type, type) == 0)
		return 0;

	for (i = 0; i < tabs; i++)
		if (fputc('\t', f) == EOF)
			return -1;

	switch (fd->type) {
	case FTYPE_BIT:
	case FTYPE_BITFIELD:
	case FTYPE_INT:
		if (fprintf(f, 
		    "parms[%zu].iparm = ORT_GETV_%s_%s(%sv%zu);\n",
		    idx - 1, fd->parent->name, fd->name,
		    ptr ? "*" : "", pos) < 0)
			return -1;
		break;
	default:
		if (fprintf(f, "parms[%zu].%s = %sv%zu;\n", idx - 1, 
		    bindvars[fd->type], ptr ? "*" : "", pos) < 0)
			return -1;
		break;
	}

	for (i = 0; i < tabs; i++)
		if (fputc('\t', f) == EOF)
			return -1;

	if (fprintf(f, "parms[%zu].type = %s;\n", 
	    idx - 1, bindtypes[fd->type]) < 0)
		return -1;

	if (fd->type == FTYPE_BLOB) {
		for (i = 0; i < tabs; i++)
			if (fputc('\t', f) == EOF)
				return 0;
		if (fprintf(f, "parms[%zu].sz = v%zu_sz;\n", 
		    idx - 1, pos) < 0)
			return -1;
	}
	return 1;
}

/*
 * Like gen_bind() but with a fixed number of tabs and never being a
 * pointer.
 */
static int
gen_bind_val(FILE *f, const struct field *fd,
	size_t idx, size_t pos, enum optype type)
{

	return gen_bind(f, fd, idx, pos, 0, 1, type);
}

/*
 * Like gen_bind() but only for hashed passwords.
 * Accepts an additional "hpos", which is the index of the current
 * "hash" variable as emitted.
 * Return zero on failure, non-zero on success.
 */
static int
gen_bind_hash(FILE *f, size_t pos, size_t hpos, size_t tabs)
{
	size_t	 i;

	for (i = 0; i < tabs; i++)
		if (fputc('\t', f) == EOF)
			return 0;
	if (fprintf(f, "parms[%zu].sparm = "
	    "hash%zu;\n", pos - 1, hpos) < 0)
		return 0;
	for (i = 0; i < tabs; i++)
		if (fputc('\t', f) == EOF)
			return 0;
	return fprintf(f, "parms[%zu].type = "
		"SQLBOX_PARM_STRING;\n", pos - 1) > 0;
}

/*
 * Generate a search function for an STYPE_ITERATE.
 * Return zero on failure, non-zero on success.
 */
static int
gen_iterator(FILE *f, const struct config *cfg,
	const struct search *s, size_t num)
{
	const struct sent	*sent;
	const struct strct 	*retstr;
	size_t			 pos, idx, parms = 0;
	int			 c;

	retstr = s->dst != NULL ? s->dst->strct : s->parent;

	/* Count all possible parameters to bind. */

	TAILQ_FOREACH(sent, &s->sntq, entries)
		if (OPTYPE_ISBINARY(sent->op))
			parms += count_bind
				(sent->field->type, sent->op);

	/* Emit top of the function w/optional static parameters. */

	if (!gen_func_db_search(f, s, 0))
		return 0;
	if (fprintf(f, "\n"
  	    "{\n"
	    "\tstruct %s p;\n"
	    "\tconst struct sqlbox_parmset *res;\n"
	    "\tstruct sqlbox *db = ctx->db;\n",
	    retstr->name) < 0)
		return 0;
	if (parms > 0 && fprintf(f, 
	    "\tstruct sqlbox_parm parms[%zu];\n", parms) < 0)
		return 0;

	/* Emit parameter binding. */

	if (fputc('\n', f) == EOF)
		return 0;
	if (parms > 0 &&
	    fputs("\tmemset(parms, 0, sizeof(parms));\n", f) == EOF)
		return 0;

	pos = idx = 1;
	TAILQ_FOREACH(sent, &s->sntq, entries)
		if (OPTYPE_ISBINARY(sent->op)) {
			c = gen_bind_val(f, sent->field, 
				idx, pos, sent->op);
			if (c < 0)
				return 0;
			pos += (size_t)c;
			idx++;
		}

	/* Prepare and step. */

	if (fprintf(f, "\n"
	    "\tif (!sqlbox_prepare_bind_async\n"
	    "\t    (db, 0, STMT_%s_BY_SEARCH_%zu,\n"
	    "\t     %zu, %s, SQLBOX_STMT_MULTI))\n"
	    "\t\texit(EXIT_FAILURE);\n"
	    "\twhile ((res = sqlbox_step(db, 0)) "
	    "!= NULL && res->psz) {\n"
	    "\t\tdb_%s_fill_r(ctx, &p, res, NULL);\n",
	    s->parent->name, num, parms,
	    parms > 0 ? "parms" : "NULL", retstr->name) < 0)
		return 0;

	/* Conditional post-query null lookup. */

	if ((retstr->flags & STRCT_HAS_NULLREFS) && fprintf(f,
	     "\t\tdb_%s_reffind(ctx, &p);\n", retstr->name) < 0)
		return 0;

	/* Conditional post-query password check. */

	pos = 1;
	TAILQ_FOREACH(sent, &s->sntq, entries) {
		if (OPTYPE_ISUNARY(sent->op))
			continue;
		if (sent->field->type != FTYPE_PASSWORD ||
		    sent->op == OPTYPE_STREQ ||
		    sent->op == OPTYPE_STRNEQ) {
			pos++;
			continue;
		}
		if (fputs("\t\tif ", f) == EOF)
			return 0;
		if (!gen_checkpass(f, 0, pos, 
		    sent->fname, sent->op, sent->field))
			return 0;
		if (fprintf(f, " {\n"
		    "\t\t\tdb_%s_unfill_r(&p);\n"
		    "\t\t\tcontinue;\n"
		    "\t\t}\n",
		    s->parent->name) < 0)
			return 0;
		pos++;
	}

	return fprintf(f, "\t\t(*cb)(&p, arg);\n"
		"\t\tdb_%s_unfill_r(&p);\n"
	       "\t}\n"
	       "\tif (res == NULL)\n"
	       "\t\texit(EXIT_FAILURE);\n"
	       "\tif (!sqlbox_finalise(db, 0))\n"
	       "\t\texit(EXIT_FAILURE);\n"
	       "}\n"
	       "\n", retstr->name) > 0;
}

/*
 * Generate search function for an STYPE_LIST.
 * Return zero on failure, non-zero on success.
 */
static int
gen_list(FILE *f, const struct config *cfg, 
	const struct search *s, size_t num)
{
	const struct sent	*sent;
	const struct strct	*retstr;
	size_t	 		 pos, parms = 0, idx;
	int			 c;

	retstr = s->dst != NULL ? s->dst->strct : s->parent;

	/* Count all possible parameters to bind. */

	TAILQ_FOREACH(sent, &s->sntq, entries)
		if (OPTYPE_ISBINARY(sent->op))
			parms += count_bind
				(sent->field->type, sent->op);

	/* Emit top of the function w/optional static parameters. */

	if (!gen_func_db_search(f, s, 0))
		return 0;
	if (fprintf(f, "\n"
	    "{\n"
	    "\tstruct %s *p;\n"
	    "\tstruct %s_q *q;\n"
	    "\tconst struct sqlbox_parmset *res;\n"
	    "\tstruct sqlbox *db = ctx->db;\n",
	    retstr->name, retstr->name) < 0)
		return 0;
	if (parms > 0 && fprintf(f, 
	    "\tstruct sqlbox_parm parms[%zu];\n", parms) < 0)
		return 0;
	if (fputc('\n', f) == EOF)
		return 0;
	if (parms > 0 && fputs
	    ("\tmemset(parms, 0, sizeof(parms));\n", f) == EOF)
		return 0;

	/* Allocate for result queue. */

	if (fprintf(f, "\tq = malloc(sizeof(struct %s_q));\n"
	    "\tif (q == NULL) {\n"
	    "\t\tperror(NULL);\n"
	    "\t\texit(EXIT_FAILURE);\n"
	    "\t}\n"
	    "\tTAILQ_INIT(q);\n"
	    "\n", retstr->name) < 0)
		return 0;

	/* Emit parameter binding. */

	pos = idx = 1;
	TAILQ_FOREACH(sent, &s->sntq, entries)
		if (OPTYPE_ISBINARY(sent->op)) {
			c = gen_bind_val(f, sent->field, 
				idx, pos, sent->op);
			if (c < 0)
				return 0;
			idx += (size_t)c;
			pos++;
		}

	if (pos > 1 && fputc('\n', f) == EOF)
		return 0;

	/* Bind and step. */

	if (fprintf(f, 
	    "\tif (!sqlbox_prepare_bind_async\n"
	    "\t    (db, 0, STMT_%s_BY_SEARCH_%zu,\n"
	    "\t     %zu, %s, SQLBOX_STMT_MULTI))\n"
	    "\t	exit(EXIT_FAILURE);\n"
	    "\twhile ((res = sqlbox_step(db, 0)) != NULL "
	    "&& res->psz) {\n"
	    "\t\tp = malloc(sizeof(struct %s));\n"
	    "\t\tif (p == NULL) {\n"
	    "\t\t\tperror(NULL);\n"
	    "\t\t\texit(EXIT_FAILURE);\n"
	    "\t\t}\n"
	    "\t\tdb_%s_fill_r(ctx, p, res, NULL);\n",
	    s->parent->name, num, parms,
	    parms > 0 ? "parms" : "NULL",
	    retstr->name, retstr->name) < 0)
		return 0;

	/* Conditional post-query to fill null refs. */

	if (retstr->flags & STRCT_HAS_NULLREFS && fprintf(f, 
            "\t\tdb_%s_reffind(ctx, p);\n", retstr->name) < 0)
		return 0;

	/* Conditional post-query password check. */

	pos = 1;
	TAILQ_FOREACH(sent, &s->sntq, entries) {
		if (OPTYPE_ISUNARY(sent->op))
			continue;
		if (sent->field->type != FTYPE_PASSWORD ||
		    sent->op == OPTYPE_STREQ ||
		    sent->op == OPTYPE_STRNEQ) {
			pos++;
			continue;
		}
		if (fputs("\t\tif ", f) == EOF)
			return 0;
		if (!gen_checkpass(f, 1, pos, 
		    sent->fname, sent->op, sent->field))
			return 0;
		if (fprintf(f, " {\n"
		    "\t\t\tdb_%s_free(p);\n"
		    "\t\t\tp = NULL;\n"
		    "\t\t\tcontinue;\n"
		    "\t\t}\n",
		    s->parent->name) < 0)
			return 0;
		pos++;
	}

	return fputs("\t\tTAILQ_INSERT_TAIL(q, p, _entries);\n"
	     "\t}\n"
	     "\tif (res == NULL)\n"
	     "\t\texit(EXIT_FAILURE);\n"
	     "\tif (!sqlbox_finalise(db, 0))\n"
	     "\t\texit(EXIT_FAILURE);\n"
	     "\treturn q;\n"
	     "}\n\n", f) != EOF;
}

/*
 * Count all roles beneath a given role excluding "all".
 * Returns the number, which is never zero.
 */
static size_t
count_roles(const struct role *role)
{
	size_t			 i = 0;
	const struct role	*r;

	if (strcmp(role->name, "all") != 0)
		i++;
	TAILQ_FOREACH(r, &role->subrq, entries)
		i += count_roles(r);
	return i;
}

/*
 * Create the role hierarchy.
 * Returns zero on failure, non-zero on success.
 */
static int
gen_role_hier(FILE *f, const struct role *r)
{

	if (r->parent != NULL && 
	    strcmp(r->parent->name, "all") && 
	    strcmp(r->parent->name, "none") && fprintf(f, 
	    "\tif (!sqlbox_role_hier_child(hier, ROLE_%s, ROLE_%s))\n"
	    "\t\tgoto err;\n",
	    r->parent->name, r->name) < 0)
		return 0;

	return 1;
}

/*
 * Actually print the sqlbox_role_hier_stmt() function for the statement
 * enumeration in "stmt".
 * This does nothing if we're doing the "all" or "none" role.
 * Return zero on failure, non-zero on success.
 */
static int
gen_role_stmt(FILE *f, const struct role *r, const char *stmt)
{

	if (strcmp(r->name, "all") == 0 ||
	    strcmp(r->name, "none") == 0)
		return 1;
	return fprintf(f, "\tif (!sqlbox_role_hier_stmt"
		"(hier, ROLE_%s, %s))\n\t\tgoto err;\n", 
		r->name, stmt) > 0;
}

/*
 * Print the sqlbox_role_hier_stmt() for all roles.
 * This means that we print for the immediate children of the "all"
 * role and let the hierarchical algorithm handle the rest.
 * Return zero on failure, non-zero on success.
 */
static int
gen_role_stmt_all(FILE *f,
	const struct config *cfg, const char *stmt)
{
	const struct role	*r, *rr;

	TAILQ_FOREACH(r, &cfg->rq, entries) {
		if (strcmp(r->name, "all") != 0)
			continue;
		TAILQ_FOREACH(rr, &r->subrq, entries)
			if (!gen_role_stmt(f, rr, stmt))
				return 0;
		break;
	}
	return 1;
}

/*
 * For structure "p", print all roles capable of all operations.
 * Return >0 on success w/statements, <0 on failure, 0 on success w/o
 * statements.
 */
static int
gen_roles(FILE *f, const struct config *cfg, const struct strct *p)
{
	const struct rref	*rs;
	const struct search 	*s;
	const struct update 	*u;
	const struct field 	*fd;
	size_t	 		 pos, shown = 0;
	char			*buf;

	/* 
	 * FIXME: only do this if the role needs access to this, which
	 * needs to be figured out by a recursive scan.
	 */

	TAILQ_FOREACH(fd, &p->fq, entries)
		if ((fd->flags & (FIELD_ROWID|FIELD_UNIQUE))) {
			if (asprintf(&buf, "STMT_%s_BY_UNIQUE_%s",
			    p->name, fd->name) < 0)
				return -1;
			if (!gen_role_stmt_all(f, cfg, buf))
				return -1;
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
		    p->name, pos - 1) < 0)
			return -1;
		TAILQ_FOREACH(rs, &s->rolemap->rq, entries)
			if (strcmp(rs->role->name, "all") == 0) {
				if (!gen_role_stmt_all(f, cfg, buf))
					return -1;
			} else if (!gen_role_stmt(f, rs->role, buf))
				return -1;
		shown++;
		free(buf);
	}

	/* Next: insertions. */

	if (p->ins != NULL && p->ins->rolemap != NULL) {
		if (asprintf(&buf, "STMT_%s_INSERT", p->name) < 0)
			return -1;
		TAILQ_FOREACH(rs, &p->ins->rolemap->rq, entries)
			if (strcmp(rs->role->name, "all") == 0) {
				if (!gen_role_stmt_all(f, cfg, buf))
					return -1;
			} else if (!gen_role_stmt(f, rs->role, buf))
				return -1;
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
		    p->name, pos - 1) < 0)
			return -1;
		TAILQ_FOREACH(rs, &u->rolemap->rq, entries)
			if (strcmp(rs->role->name, "all") == 0) {
				if (!gen_role_stmt_all(f, cfg, buf))
					return -1;
			} else if (!gen_role_stmt(f, rs->role, buf))
				return -1;
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
		    p->name, pos - 1) < 0)
			return -1;
		TAILQ_FOREACH(rs, &u->rolemap->rq, entries)
			if (strcmp(rs->role->name, "all") == 0) {
				if (!gen_role_stmt_all(f, cfg, buf))
					return -1;
			} else if (!gen_role_stmt(f, rs->role, buf))
				return -1;
		shown++;
		free(buf);
	}

	return shown > 0;
}

/*
 * Generate database opening.
 * Returns zero on failure, non-zero on success.
 */
static int
gen_open(FILE *f, const struct config *cfg)
{
	const struct role 	*r;
	const struct strct 	*p;
	size_t			 i;
	int			 c;

	if (!gen_func_db_set_logging(f, 0))
		return 0;
	if (fputs("{\n"
	    "\n"
	    "\tif (!sqlbox_msg_set_dat(ort->db, arg, sz))\n"
	    "\t\texit(EXIT_FAILURE);\n"
	    "}\n\n", f) == EOF)
		return 0;

	if (!gen_func_db_open(f, 0))
		return 0;
	if (fputs("{\n"
	    "\n"
	    "\treturn db_open_logging(file, NULL, NULL, NULL);\n"
	    "}\n\n", f) == EOF)
		return 0;

	if (!gen_func_db_open_logging(f, 0))
		return 0;
	if (fputs("{\n"
	     "\tsize_t i;\n"
	     "\tstruct ort *ctx = NULL;\n"
	     "\tstruct sqlbox_cfg cfg;\n"
	     "\tstruct sqlbox *db = NULL;\n"
	     "\tstruct sqlbox_pstmt pstmts[STMT__MAX];\n"
	     "\tstruct sqlbox_src srcs[1] = {\n"
	     "\t\t{ .fname = (char *)file,\n"
	     "\t\t  .mode = SQLBOX_SRC_RW }\n"
	     "\t};\n", f) == EOF)
		return 0;
	if (!TAILQ_EMPTY(&cfg->rq) && fputs
	    ("\tstruct sqlbox_role_hier *hier = NULL;\n", f) == EOF)
		return 0;
	if (fputs("\n"
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
	    "\t\tgoto err;\n\n", f) == EOF)
		return 0;

	if (!TAILQ_EMPTY(&cfg->rq)) {
		/*
		 * We need an complete count of all roles except the
		 * "all" role, which cannot be entered or processed.
		 * So do this recursively.
		 */

		i = 0;
		TAILQ_FOREACH(r, &cfg->rq, entries)
			i += count_roles(r);
		assert(i > 0);
		if (fprintf(f, "\thier = sqlbox_role_hier_alloc(%zu);\n"
		    "\tif (hier == NULL)\n"
		    "\t\tgoto err;\n"
		    "\n", i) < 0)
			return 0;

		if (!gen_comment(f, 1, COMMENT_C, "Assign roles."))
			return 0;

		/* 
		 * FIXME: the default role should only be able to open
		 * the database once.
		 * With this, it's able to do so multiple times and
		 * that's not a permission it needs.
		 */

		if (fputs("\n"
		    "\tif (!sqlbox_role_hier_sink(hier, ROLE_none))\n"
		    "\t\tgoto err;\n"
		    "\tif (!sqlbox_role_hier_start(hier, ROLE_default))\n"
		    "\t\tgoto err;\n"
		    "\tif (!sqlbox_role_hier_src(hier, ROLE_default, 0))\n"
		    "\t\tgoto err;\n", f) == EOF)
			return 0;

		TAILQ_FOREACH(r, &cfg->arq, allentries)
			if (!gen_role_hier(f, r))
				return 0;

		if (fputc('\n', f) == EOF)
			return 0;
		TAILQ_FOREACH(p, &cfg->sq, entries) {
			if (!gen_commentv(f, 1, COMMENT_C, 
			    "White-listing fields and "
			    "operations for structure \"%s\".",
			    p->name))
				return 0;
			if (fputc('\n', f) == EOF)
				return 0;
			if ((c = gen_roles(f, cfg, p)) < 0)
				return 0;
			else if (c > 0 && fputc('\n', f) == EOF)
				return 0;
		}
		if (fputs("\tif (!sqlbox_role_hier_gen"
		    "(hier, &cfg.roles, ROLE_default))\n"
		    "\t\tgoto err;\n\n", f) == EOF)
			return 0;
	}

	if (fputs("\tif ((db = sqlbox_alloc(&cfg)) == NULL)\n"
	    "\t\tgoto err;\n"
	    "\tctx->db = db;\n", f) == EOF)
		return 0;

	if (!TAILQ_EMPTY(&cfg->rq)) {
	        if (fputs("\tctx->role = ROLE_default;\n"
		    "\n"
		    "\tsqlbox_role_hier_gen_free(&cfg.roles);\n"
		    "\tsqlbox_role_hier_free(hier);\n"
		    "\thier = NULL;\n\n", f) == EOF)
			return 0;
	} else if (fputc('\n', f) == EOF)
		return 0;

	if (!gen_comment(f, 1, COMMENT_C, 
	    "Now actually open the database.\n"
	    "If this succeeds, then we're good to go."))
		return 0;

	if (fputs("\n"
	    "\tif (sqlbox_open_async(db, 0))\n"
	    "\t\treturn ctx;\n"
	    "err:\n", f) == EOF)
		return 0;

	if (!TAILQ_EMPTY(&cfg->rq) && fputs
	    ("\tsqlbox_role_hier_gen_free(&cfg.roles);\n"
	     "\tsqlbox_role_hier_free(hier);\n", f) == EOF)
		return 0;

	return fputs("\tsqlbox_free(db);\n"
	     "\tfree(ctx);\n"
	     "\treturn NULL;\n"
	     "}\n\n", f) != EOF;
}

/*
 * Generate the rules for how we can switch between roles.
 * FIXME: this is not necessary with sqlbox_role().
 */
static int
gen_func_rolecases(FILE *f, const struct role *r)
{
	const struct role	*rp, *rr;

	assert(NULL != r->parent);

	if (fprintf(f, "\tcase ROLE_%s:\n", r->name) < 0)
		return 0;

	/*
	 * If our parent is "all", then there's nowhere we can
	 * transition into, as we can only transition "up" the tree of
	 * roles (i.e., into roles with less specific privileges).
	 * Thus, every attempt to transition should fail.
	 */

	if (0 == strcmp(r->parent->name, "all")) {
		if (fputs("\t\tabort();\n"
		    "\t\t/* NOTREACHED */\n", f) == EOF)
			return 0;
		TAILQ_FOREACH(rr, &r->subrq, entries)
			if (!gen_func_rolecases(f, rr))
				return 0;
		return 1;
	}

	/* Here, we can transition into lesser privileges. */

	if (fputs("\t\tswitch (r) {\n", f) == EOF)
		return 0;

	for (rp = r->parent; strcmp(rp->name, "all"); rp = rp->parent)
		if (fprintf(f, "\t\tcase ROLE_%s:\n", rp->name) < 0)
			return 0;

	if (fputs("\t\t\tctx->role = r;\n"
	    "\t\t\treturn;\n"
	    "\t\tdefault:\n"
	    "\t\t\tabort();\n"
	    "\t\t}\n"
	    "\t\tbreak;\n", f) == EOF)
		return 0;

	TAILQ_FOREACH(rr, &r->subrq, entries)
		if (!gen_func_rolecases(f, rr))
			return 0;

	return 1;
}

/*
 * FIXME: most of this is no longer necessary with sqlbox_role().
 */
static int
gen_func_role_transitions(FILE *f, const struct config *cfg)
{
	const struct role	*r, *rr;

	TAILQ_FOREACH(r, &cfg->rq, entries)
		if (strcmp(r->name, "all") == 0)
			break;
	assert(r != NULL);

	if (!gen_func_db_role(f, 0))
		return 0;
	if (fputs("{\n"
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
	    "\t\treturn;\n", f) == EOF)
		return 0;
	TAILQ_FOREACH(rr, &r->subrq, entries)
		if (!gen_func_rolecases(f, rr))
			return 0;
	if (fputs("\tdefault:\n"
	    "\t\tabort();\n"
	    "\t}\n"
	    "}\n\n", f) == EOF)
		return 0;

	if (!gen_func_db_role_current(f, 0))
		return 0;
	if (fputs("{\n"
	    "\treturn ctx->role;\n"
	    "}\n\n", f) == EOF)
		return 0;

	if (!gen_func_db_role_stored(f, 0))
		return 0;
	return fputs("{\n"
	     "\treturn s->role;\n"
	     "}\n\n", f) != EOF;
}

/*
 * Generate the transaction open and close functions.
 * Return zero on failure, non-zero on success.
 */
static int
gen_transactions(FILE *f, const struct config *cfg)
{

	if (!gen_func_db_trans_open(f, 0))
		return 0;
	if (fputs("{\n"
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
	    "}\n\n", f) == EOF)
		return 0;

	if (!gen_func_db_trans_rollback(f, 0))
		return 0;
	if (fputs("{\n"
	    "\tstruct sqlbox *db = ctx->db;\n"
	    "\n"
	    "\tif (!sqlbox_trans_rollback(db, 0, id))\n"
	    "\t\texit(EXIT_FAILURE);\n"
	    "}\n\n", f) == EOF)
		return 0;

	if (!gen_func_db_trans_commit(f, 0))
		return 0;
	return fputs("{\n"
	     "\tstruct sqlbox *db = ctx->db;\n"
	     "\n"
	     "\tif (!sqlbox_trans_commit(db, 0, id))\n"
	     "\t\texit(EXIT_FAILURE);\n"
	     "}\n\n", f) != EOF;
}

/*
 * Generate the database close function.
 * Return zero on failure, non-zero on success.
 */
static int
gen_close(FILE *f, const struct config *cfg)
{

	if (!gen_func_db_close(f, 0))
		return 0;
	return fputs("{\n"
	     "\tif (p == NULL)\n"
	     "\t\treturn;\n"
             "\tsqlbox_free(p->db);\n"
	     "\tfree(p);\n"
	     "}\n\n", f) != EOF;
}

/*
 * Generate a query function for an STYPE_COUNT.
 * Return zero on failure, non-zero on success.
 */
static int
gen_count(FILE *f, const struct config *cfg,
	const struct search *s, size_t num)
{
	const struct sent	*sent;
	size_t			 pos, parms = 0, idx;
	int			 c;

	/* Count all possible parameters to bind. */

	TAILQ_FOREACH(sent, &s->sntq, entries) 
		if (OPTYPE_ISBINARY(sent->op))
			parms += count_bind
				(sent->field->type, sent->op);

	if (!gen_func_db_search(f, s, 0))
		return 0;
	if (fputs("\n"
	    "{\n"
	    "\tconst struct sqlbox_parmset *res;\n"
	    "\tint64_t val;\n"
	    "\tstruct sqlbox *db = ctx->db;\n", f) == EOF)
		return 0;
	if (parms > 0 && fprintf(f, 
	    "\tstruct sqlbox_parm parms[%zu];\n", parms) < 0)
		return 0;
	if (fputc('\n', f) == EOF)
		return 0;

	/* Emit parameter binding. */

	pos = idx = 1;
	TAILQ_FOREACH(sent, &s->sntq, entries) 
		if (OPTYPE_ISBINARY(sent->op)) {
			c = gen_bind_val(f, sent->field, 
				idx, pos, sent->op);
			if (c < 0)
				return 0;
			idx += (size_t)c;
			pos++;
		}

	/* A single returned entry. */

	return fprintf(f, "\n"
		"\tif (!sqlbox_prepare_bind_async\n"
		"\t    (db, 0, STMT_%s_BY_SEARCH_%zu, %zu, %s, 0))\n"
		"\t	exit(EXIT_FAILURE);\n"
		"\tif ((res = sqlbox_step(db, 0)) == NULL)\n"
		"\t\texit(EXIT_FAILURE);\n"
		"\telse if (res->psz != 1)\n"
		"\t\texit(EXIT_FAILURE);\n"
		"\tif (sqlbox_parm_int(&res->ps[0], &val) == -1)\n"
		"\t\texit(EXIT_FAILURE);\n"
		"\tsqlbox_finalise(db, 0);\n"
		"\treturn (uint64_t)val;\n"
		"}\n\n", s->parent->name, num, parms,
		parms > 0 ? "parms" : "NULL") > 0;
}

/*
 * Generate query function for an STYPE_SEARCH.
 * Return zero on failure, non-zero on success.
 */
static int
gen_search(FILE *f, const struct config *cfg,
	const struct search *s, size_t num)
{
	const struct sent	*sent;
	const struct strct	*retstr;
	size_t			 pos, parms = 0, idx;
	int			 c;

	retstr = s->dst != NULL ? s->dst->strct : s->parent;

	/* Count all possible parameters to bind. */

	TAILQ_FOREACH(sent, &s->sntq, entries) 
		if (OPTYPE_ISBINARY(sent->op))
			parms += count_bind
				(sent->field->type, sent->op);

	if (!gen_func_db_search(f, s, 0))
		return 0;
	if (fprintf(f, "\n"
	    "{\n"
	    "\tstruct %s *p = NULL;\n"
	    "\tconst struct sqlbox_parmset *res;\n"
	    "\tstruct sqlbox *db = ctx->db;\n",
	    retstr->name) < 0)
		return 0;
	if (parms > 0 && fprintf(f, 
	    "\tstruct sqlbox_parm parms[%zu];\n", parms) < 0)
		return 0;
	if (fputc('\n', f) == EOF)
		return 0;

	/* Emit parameter binding. */

	if (parms > 0 && fputs
	    ("\tmemset(parms, 0, sizeof(parms));\n", f) == EOF)
		return 0;
	pos = idx = 1;
	TAILQ_FOREACH(sent, &s->sntq, entries) 
		if (OPTYPE_ISBINARY(sent->op)) {
			c = gen_bind_val(f, sent->field, 
				idx, pos, sent->op);
			if (c < 0)
				return 0;
			idx += (size_t)c;
			pos++;
		} 

	if (fprintf(f, "\n"
	    "\tif (!sqlbox_prepare_bind_async\n"
	    "\t    (db, 0, STMT_%s_BY_SEARCH_%zu, %zu, %s, 0))\n"
	    "\t	exit(EXIT_FAILURE);\n"
	    "\tif ((res = sqlbox_step(db, 0)) != NULL "
	    "&& res->psz) {\n"
	    "\t\tp = malloc(sizeof(struct %s));\n"
	    "\t\tif (p == NULL) {\n"
	    "\t\t\tperror(NULL);\n"
	    "\t\t\texit(EXIT_FAILURE);\n"
	    "\t\t}\n"
	    "\t\tdb_%s_fill_r(ctx, p, res, NULL);\n",
	    s->parent->name, num, parms,
	    parms > 0 ? "parms" : "NULL",
	    retstr->name, retstr->name) < 0)
		return 0;

	/* Conditional post-query reference lookup. */

	if (retstr->flags & STRCT_HAS_NULLREFS && fprintf(f, 
	    "\t\tdb_%s_reffind(ctx, p);\n", retstr->name) < 0)
		return 0;

	/* Conditional post-query password check. */

	pos = 1;
	TAILQ_FOREACH(sent, &s->sntq, entries) {
		if (OPTYPE_ISUNARY(sent->op))
			continue;
		if (sent->field->type != FTYPE_PASSWORD ||
		    sent->op == OPTYPE_STREQ ||
		    sent->op == OPTYPE_STRNEQ) {
			pos++;
			continue;
		}
		if (fputs("\t\tif ", f) == EOF)
			return 0;
		if (!gen_checkpass(f, 1, pos,
		    sent->fname, sent->op, sent->field))
			return 0;
		if (fprintf(f, " {\n"
		    "\t\t\tdb_%s_free(p);\n"
		    "\t\t\tp = NULL;\n"
		    "\t\t}\n", 
		    s->parent->name) < 0)
			return 0;
		pos++;
	}

	return fputs("\t}\n"
		"\tif (res == NULL)\n"
		"\t\texit(EXIT_FAILURE);\n"
		"\tif (!sqlbox_finalise(db, 0))\n"
		"\t\texit(EXIT_FAILURE);\n"
		"\treturn p;\n"
		"}\n\n", f) != EOF;
}

/*
 * Generate the "freeq" function.
 * This must have STRCT_HAS_QUEUE defined in its flags, otherwise the
 * function does nothing and returns success.
 * Return zero on failure, non-zero on success.
 */
static int
gen_freeq(FILE *f, const struct strct *p)
{

	if (!(p->flags & STRCT_HAS_QUEUE))
		return 1;

	if (!gen_func_db_freeq(f, p, 0))
		return 0;
	return fprintf(f, "\n"
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
	       p->name, p->name) > 0;
}

/*
 * Generate the "insert" function.
 * If we don't have an insert, does nothing and return success.
 * Return zero on failure, non-zero on success.
 */
static int
gen_insert(FILE *f, const struct config *cfg, const struct strct *p)
{
	const struct field	*fd;
	size_t			 hpos, idx, parms = 0, tabs, pos;

	if (p->ins == NULL)
		return 1;

	/* Count non-struct non-rowid parameters to bind. */

	TAILQ_FOREACH(fd, &p->fq, entries)
		if (fd->type != FTYPE_STRUCT && 
		    !(fd->flags & FIELD_ROWID))
			parms++;

	if (!gen_func_db_insert(f, p, 0))
		return 0;
	if (fputs("\n"
	    "{\n"
	    "\tint rc;\n"
	    "\tint64_t id = -1;\n"
	    "\tstruct sqlbox *db = ctx->db;\n", f) == EOF)
		return 0;

	if (parms > 0 && fprintf(f, 
	    "\tstruct sqlbox_parm parms[%zu];\n", parms) < 0)
		return 0;

	/* Start by generating password hashes. */

	hpos = 1;
	TAILQ_FOREACH(fd, &p->fq, entries)
		if (fd->type == FTYPE_PASSWORD && fprintf(f , 
		    "\tchar hash%zu[64];\n", hpos++) < 0)
			return 0;

	if (fputc('\n', f) == EOF)
		return 0;

	hpos = idx = 1;
	TAILQ_FOREACH(fd, &p->fq, entries) {
		if (fd->type == FTYPE_STRUCT ||
		    (fd->flags & FIELD_ROWID))
			continue;
		if (fd->type != FTYPE_PASSWORD) {
			idx++;
			continue;
		}
		if ((fd->flags & FIELD_NULL) && fprintf(f, 
		    "\tif (v%zu != NULL)\n\t", idx) < 0)
			return 0;
		if (!gen_newpass(f,
		    fd->flags & FIELD_NULL, hpos, idx))
			return 0;
		hpos++;
		idx++;
	}
	if (hpos > 1 && fputc('\n', f) == EOF)
		return 0;
	if (parms > 0 &&
	    fputs("\tmemset(parms, 0, sizeof(parms));\n", f) == EOF)
		return 0;

	/* 
	 * Advance hash position (hpos), index in parameters array
	 * (idx), and position in function arguments (pos).
	 */

	hpos = pos = idx = 1;
	TAILQ_FOREACH(fd, &p->fq, entries) {
		if (fd->type == FTYPE_STRUCT ||
		    (fd->flags & FIELD_ROWID))
			continue;

		tabs = 1;
		if (fd->flags & FIELD_NULL) {
			if (fprintf(f, "\tif (v%zu == NULL) {\n"
		  	    "\t\tparms[%zu].type = "
			    "SQLBOX_PARM_NULL;\n"
			    "\t} else {\n", pos, idx - 1) < 0)
				return 0;
			tabs++;
		}

		if (fd->type == FTYPE_PASSWORD) {
			if (!gen_bind_hash(f, idx, hpos++, tabs))
				return 0;
		} else {
			if (gen_bind(f, fd, idx, pos,
			    (fd->flags & FIELD_NULL), tabs, 
			    OPTYPE_EQUAL /* XXX */) < 0)
				return 0;
		}

		if ((fd->flags & FIELD_NULL) &&
		    fputs("\t}\n", f) == EOF)
			return 0;
		idx++;
		pos++;
	}
	if (parms > 0 && fputc('\n', f) == EOF)
		return 0;

	return fprintf(f, 
		"\trc = sqlbox_exec(db, 0, STMT_%s_INSERT, \n"
		"\t     %zu, %s, SQLBOX_STMT_CONSTRAINT);\n"
		"\tif (rc == SQLBOX_CODE_ERROR)\n"
		"\t\texit(EXIT_FAILURE);\n"
		"\telse if (rc != SQLBOX_CODE_OK)\n"
		"\t\treturn (-1);\n"
		"\tif (!sqlbox_lastid(db, 0, &id))\n"
		"\t\texit(EXIT_FAILURE);\n"
		"\treturn id;\n"
		"}\n\n", p->name, parms,
	       parms > 0 ? "parms" : "NULL") > 0;
}

/*
 * Generate the "free" function.
 * Return zero on failure, non-zero on success.
 */
static int
gen_free(FILE *f, const struct strct *p)
{

	if (!gen_func_db_free(f, p, 0))
		return 0;
	return fprintf(f, "\n{\n"
		"\tdb_%s_unfill_r(p);\n"
		"\tfree(p);\n"
		"}\n\n", p->name) > 0;
}

/*
 * Generate the "unfill" function.
 * Return zero on failure, non-zero on success.
 */
static int
gen_unfill(FILE *f, const struct config *cfg, const struct strct *p)
{
	const struct field	*fd;

	if (!gen_comment(f, 0, COMMENT_C,
	    "Free resources from \"p\" and all nested objects.\n"
	    "Does not free the \"p\" pointer itself.\n"
	    "Has no effect if \"p\" is NULL."))
		return 0;

	if (fprintf(f, 
  	    "static void\n"
	    "db_%s_unfill(struct %s *p)\n"
	    "{\n"
	    "\tif (p == NULL)\n"
	    "\t\treturn;\n", p->name, p->name) < 0)
		return 0;

	TAILQ_FOREACH(fd, &p->fq, entries)
		switch(fd->type) {
		case FTYPE_BLOB:
		case FTYPE_PASSWORD:
		case FTYPE_TEXT:
		case FTYPE_EMAIL:
			if (fprintf(f, 
			    "\tfree(p->%s);\n", fd->name) < 0)
				return 0;
			break;
		default:
			break;
		}

	if (!TAILQ_EMPTY(&cfg->rq) &&
	    fputs("\tfree(p->priv_store);\n", f) == EOF)
		return 0;

	return fputs("}\n\n", f) != EOF;
}

/*
 * Generate the nested "unfill" function.
 * Return zero on failure, non-zero on success.
 */
static int
gen_unfill_r(FILE *f, const struct strct *p)
{
	const struct field	*fd;

	if (fprintf(f, "static void\n"
	    "db_%s_unfill_r(struct %s *p)\n"
	    "{\n"
	    "\tif (p == NULL)\n"
	    "\t\treturn;\n"
	    "\tdb_%s_unfill(p);\n",
	    p->name, p->name, p->name) < 0)
		return 0;

	TAILQ_FOREACH(fd, &p->fq, entries) {
		if (fd->type != FTYPE_STRUCT)
			continue;
		if (fd->ref->source->flags & FIELD_NULL) {
			if (fprintf(f, "\tif (p->has_%s)\n"
			    "\t\tdb_%s_unfill_r(&p->%s);\n",
			    fd->ref->source->name, 
			    fd->ref->target->parent->name, 
			    fd->name) < 0)
				return 0;
		} else {
			if (fprintf(f, "\tdb_%s_unfill_r(&p->%s);\n",
			    fd->ref->target->parent->name, 
			    fd->name) < 0)
				return 0;
		}
	}

	return fputs("}\n\n", f) != EOF;
}

/*
 * If a structure has possible null foreign keys, we need to fill in the
 * null keys after the lookup has taken place IFF they aren't null.
 * Do that here, using the STMT_XXX_BY_UNIQUE_yyy fields we generated
 * earlier.
 * This is only done for structures that have (or have nested)
 * structures with null foreign keys.
 * Return zero on failure, non-zero on success.
 */
static int
gen_reffind(FILE *f, const struct config *cfg, const struct strct *p)
{
	const struct field	*fd;

	if (!(p->flags & STRCT_HAS_NULLREFS))
		return 1;

	/* 
	 * Do we have any null-ref fields in this?
	 * (They might be in target references.)
	 */

	TAILQ_FOREACH(fd, &p->fq, entries)
		if ((fd->type == FTYPE_STRUCT) &&
		    (fd->ref->source->flags & FIELD_NULL))
			break;

	if (fprintf(f, "static void\n"
	    "db_%s_reffind(struct ort *ctx, struct %s *p)\n"
	    "{\n"
	    "\tstruct sqlbox *db = ctx->db;\n",
	    p->name, p->name) < 0)
		return 0;

	if (fd != NULL && fputs
	    ("\tconst struct sqlbox_parmset *res;\n"
	     "\tstruct sqlbox_parm parm;\n", f) == EOF)
		return 0;

	if (fputc('\n', f) == EOF)
		return 0;

	TAILQ_FOREACH(fd, &p->fq, entries) {
		if (fd->type != FTYPE_STRUCT)
			continue;
		if ((fd->ref->source->flags & FIELD_NULL))
			if (fprintf(f, "\tif (p->has_%s) {\n"
			    "\t\tparm.type = SQLBOX_PARM_INT;\n"
			    "\t\tparm.iparm = ORT_GET_%s_%s(p);\n"
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
			    fd->ref->source->name,
			    fd->ref->source->parent->name,
			    fd->ref->source->name,
			    fd->ref->target->parent->name,
			    fd->ref->target->name,
			    fd->ref->target->parent->name,
			    fd->name, fd->name) < 0)
				return 0;
		if (!(fd->ref->target->parent->flags & 
		    STRCT_HAS_NULLREFS))
			continue;
		if (fprintf(f, "\tdb_%s_reffind(ctx, &p->%s);\n", 
		    fd->ref->target->parent->name, fd->name) < 0)
			return 0;
	}

	return fputs("}\n\n", f) != EOF;
}

/*
 * Generate the recursive "fill" function.
 * This simply calls to the underlying "fill" function for all
 * strutcures in the object.
 * Return zero on failure, non-zero on success.
 */
static int
gen_fill_r(FILE *f, const struct config *cfg, const struct strct *p)
{
	const struct field	*fd;

	if (fprintf(f, "static void\n"
	    "db_%s_fill_r(struct ort *ctx, struct %s *p,\n"
	    "\tconst struct sqlbox_parmset *res, size_t *pos)\n"
	    "{\n"
	    "\tsize_t i = 0;\n"
	    "\n"
	    "\tif (pos == NULL)\n"
	    "\t\tpos = &i;\n"
	    "\tdb_%s_fill(ctx, p, res, pos);\n",
	    p->name, p->name, p->name) < 0)
		return 0;

	TAILQ_FOREACH(fd, &p->fq, entries)
		if (fd->type == FTYPE_STRUCT &&
		    !(fd->ref->source->flags & FIELD_NULL))
			if (fprintf(f, "\tdb_%s_fill_r(ctx, "
			    "&p->%s, res, pos);\n", 
			    fd->ref->target->parent->name, 
			    fd->name) < 0)
				return 0;

	return fputs("}\n\n", f) != EOF;
}

/*
 * Generate the "fill" function.
 * Return zero on failure, non-zero on success.
 */
static int
gen_fill(FILE *f, const struct config *cfg, const struct strct *p)
{
	const struct field	*fd;
	int	 		 needint = 0;

	/*
	 * Determine if we need to cast into a temporary 64-bit integer.
	 * This applies to enums, which can be any integer size, and
	 * epochs which may be 32 bits on some willy wonka systems.
	 */

	TAILQ_FOREACH(fd, &p->fq, entries)
		if (fd->type == FTYPE_ENUM || 
		    fd->type == FTYPE_DATE ||
		    fd->type == FTYPE_EPOCH) {
			needint = 1;
			break;
		}

	if (!gen_commentv(f, 0, COMMENT_C, 
	    "Fill in a %s from an open statement \"stmt\".\n"
	    "This starts grabbing results from \"pos\", "
	    "which may be NULL to start from zero.\n"
	    "This follows DB_SCHEMA_%s's order for columns.",
	    p->name, p->name))
		return 0;
	if (fprintf(f, "static void\n"
	    "db_%s_fill(struct ort *ctx, struct %s *p, "
	    "const struct sqlbox_parmset *set, size_t *pos)\n"
	    "{\n"
	    "\tsize_t i = 0;\n",
	    p->name, p->name) < 0)
		return 0;
	if (needint && fputs("\tint64_t tmpint;\n", f) == EOF)
		return 0;
	if (fputs("\n"
	     "\tif (pos == NULL)\n"
	     "\t\tpos = &i;\n"
	     "\tmemset(p, 0, sizeof(*p));\n", f) == EOF)
		return 0;
	TAILQ_FOREACH(fd, &p->fq, entries)
		if (!gen_fill_field(f, fd))
			return 0;
	if (!TAILQ_EMPTY(&cfg->rq)) {
		if (fputs("\tp->priv_store = malloc"
		    "(sizeof(struct ort_store));\n"
		    "\tif (p->priv_store == NULL) {\n"
		    "\t\tperror(NULL);\n"
		    "\t\texit(EXIT_FAILURE);\n"
		    "\t}\n"
		    "\tp->priv_store->role = ctx->role;\n", f) == EOF)
			return 0;
	}

	return fputs("}\n\n", f) != EOF;
}

/*
 * Generate an update or delete function.
 * Return zero on failure, non-zero on success.
 */
static int
gen_update(FILE *f, const struct config *cfg,
	const struct update *up, size_t num)
{
	const struct uref	*ref;
	size_t	 		 pos, idx, hpos, parms = 0, tabs;
	int			 c;

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

	if (!gen_func_db_update(f, up, 0))
		return 0;
	if (fputs("\n"
	    "{\n"
	    "\tenum sqlbox_code c;\n"
	    "\tstruct sqlbox *db = ctx->db;\n", f) == EOF)
		return 0;
	if (parms > 0 && fprintf
	    (f, "\tstruct sqlbox_parm parms[%zu];\n", parms) < 0)
		return 0;

	/* 
	 * Handle case of hashing first.
	 * If setting a password, we need to hash it.
	 * Create a temporary buffer into which we're going to hash the
	 * password.
	 */

	hpos = 1;
	TAILQ_FOREACH(ref, &up->mrq, entries)
		if (ref->field->type == FTYPE_PASSWORD &&
		    ref->mod != MODTYPE_STRSET)
			if (fprintf(f, 
			    "\tchar hash%zu[64];\n", hpos++) < 0)
				return 0;
	if (fputc('\n', f) == EOF)
		return 0;

	idx = hpos = 1;
	TAILQ_FOREACH(ref, &up->mrq, entries) {
		if (ref->field->type == FTYPE_PASSWORD &&
		    ref->mod != MODTYPE_STRSET) {
			if ((ref->field->flags & FIELD_NULL))
				if (fprintf(f, 
				    "\tif (v%zu != NULL)\n\t", idx) < 0)
					return 0;
			if (!gen_newpass(f, 
			    (ref->field->flags & FIELD_NULL), hpos, idx))
				return 0;
			hpos++;
		} 
		idx++;
	}
	if (hpos > 1 && fputc('\n', f) == EOF)
		return 0;
	if (parms > 0 &&
	    fputs("\tmemset(parms, 0, sizeof(parms));\n", f) == EOF)
		return 0;

	/*
	 * Advance hash position (hpos), index in parameters array
	 * (idx), and position in function arguments (pos).
	 */

	idx = pos = hpos = 1;
	TAILQ_FOREACH(ref, &up->mrq, entries) {
		tabs = 1;
		if ((ref->field->flags & FIELD_NULL)) {
			if (fprintf(f, "\tif (v%zu == NULL)\n"
			    "\t\tparms[%zu].type = "
			    "SQLBOX_PARM_NULL;\n"
			    "\telse {\n"
			    "\t", idx, idx - 1) < 0)
				return 0;
			tabs++;
		}

		if (ref->field->type == FTYPE_PASSWORD &&
		    ref->mod != MODTYPE_STRSET) {
			if (!gen_bind_hash(f, idx, hpos++, tabs))
				return 0;
		} else {
			if (gen_bind(f, ref->field, idx, pos,
			     (ref->field->flags & FIELD_NULL), 
			     tabs, OPTYPE_STREQ /* XXX */) < 0)
				return 0;
		}

		if (ref->field->flags & FIELD_NULL)
			if (fputs("\t}\n", f) == EOF)
				return 0;
		pos++;
		idx++;
	}

	/* Now the constraints: no password business here. */

	TAILQ_FOREACH(ref, &up->crq, entries) {
		assert(ref->field->type != FTYPE_STRUCT);
		if (OPTYPE_ISUNARY(ref->op))
			continue;
		c = gen_bind(f, ref->field, 
			idx, pos, 0, 1, ref->op);
		if (c < 0)
			return 0;
		idx += (size_t)c;
		pos++;
	}

	if (fputc('\n', f) == EOF)
		return 0;

	if (up->type == UP_MODIFY) {
		if (fprintf(f, "\tc = sqlbox_exec\n"
		    "\t\t(db, 0, STMT_%s_UPDATE_%zu,\n"
		    "\t\t %zu, %s, SQLBOX_STMT_CONSTRAINT);\n"
		    "\tif (c == SQLBOX_CODE_ERROR)\n"
		    "\t\texit(EXIT_FAILURE);\n"
		    "\treturn (c == SQLBOX_CODE_OK) ? 1 : 0;\n"
		    "}\n"
		    "\n",
		    up->parent->name, num, parms, 
		    parms > 0 ? "parms" : "NULL") < 0)
			return 0;
	} else {
		if (fprintf(f, "\tc = sqlbox_exec\n"
		    "\t\t(db, 0, STMT_%s_DELETE_%zu, %zu, %s, 0);\n"
		    "\tif (c != SQLBOX_CODE_OK)\n"
		    "\t\texit(EXIT_FAILURE);\n"
		    "}\n"
		    "\n",
		    up->parent->name, num, parms, 
		    parms > 0 ? "parms" : "NULL") < 0)
			return 0;
	}

	return 1;
}

/*
 * For the given validation field "v", generate the clause that results
 * in failure of the validation.
 * Assume that the basic type validation has passed.
 * Return zero on failure, non-zero on success.
 */
static int
gen_valids_field(FILE *f,
	const struct field *fd, const struct fvalid *v)
{

	assert(v->type < VALIDATE__MAX);
	switch (fd->type) {
	case FTYPE_BIT:
	case FTYPE_BITFIELD:
	case FTYPE_DATE:
	case FTYPE_EPOCH:
	case FTYPE_INT:
		if (fprintf(f, 
		    "\tif (p->parsed.i %s %" PRId64 ")\n"
		    "\t\treturn 0;\n",
		    validbins[v->type], v->d.value.integer) < 0)
			return 0;
		break;
	case FTYPE_REAL:
		if (fprintf(f, 
		    "\tif (p->parsed.d %s %g)\n"
		    "\t\treturn 0;\n",
		    validbins[v->type], v->d.value.decimal) < 0)
			return 0;
		break;
	default:
		if (fprintf(f,
		    "\tif (p->valsz %s %zu)\n"
		    "\t\treturn 0;\n",
		    validbins[v->type], v->d.value.len) < 0)
			return 0;
		break;
	}

	return 1;
}

/*
 * Generate the validation function for the given field.
 * Exceptions: struct fields don't have validators, blob fields always
 * validate, and non-enumerations without limits use the native kcgi(3)
 * validator function.
 * Return zero on failure, non-zero on success.
 */
static int
gen_valids(FILE *f, const struct strct *p)
{
	const struct field	*fd;
	const struct fvalid	*v;
	const struct eitem	*ei;

	TAILQ_FOREACH(fd, &p->fq, entries) {
		if (fd->type == FTYPE_STRUCT || 
		    fd->type == FTYPE_BLOB)
			continue;
		if (fd->type != FTYPE_ENUM && 
		    TAILQ_EMPTY(&fd->fvq))
			continue;

		assert(validtypes[fd->type] != NULL);

		if (!gen_func_valid(f, fd, 0))
			return 0;
		if (fprintf(f, "{\n"
		    "\tif (!%s(p))\n"
		    "\t\treturn 0;\n", validtypes[fd->type]) < 0)
			return 0;

		/* Enumeration: check against knowns. */

		if (fd->type == FTYPE_ENUM) {
			if (fputs("\tswitch"
			    "(p->parsed.i) {\n", f) == EOF)
				return 0;
			TAILQ_FOREACH(ei, &fd->enm->eq, entries)
				if (fprintf(f, "\tcase %" PRId64 
				    ":\n", ei->value) < 0)
					return 0;
			if (fputs("\t\tbreak;\n"
			    "\tdefault:\n"
			    "\t\treturn 0;\n"
			    "\t}\n", f) == EOF)
				return 0;
		}

		TAILQ_FOREACH(v, &fd->fvq, entries) 
			if (!gen_valids_field(f, fd, v))
				return 0;
		if (fputs("\treturn 1;\n}\n\n", f) == EOF)
			return 0;
	}

	return 1;
}

/*
 * Export a field in a structure.
 * This needs to handle whether the field is a blob, might be null, is a
 * structure, and so on.
 * Return zero on failure, non-zero on success.
 */
static int
gen_json_out_field(FILE *f,
	const struct field *fd, size_t *pos, int *sp)
{
	char		 	 tabs[] = "\t\t";
	const struct rref	*rs;
	int		 	 hassp = *sp;

	*sp = 0;

	if (fd->flags & FIELD_NOEXPORT) {
		if (!hassp && fputc('\n', f) == EOF)
			return 0;
		if (!gen_commentv(f, 1, COMMENT_C,
		    "Omitting %s: marked no export.", fd->name))
			return 0;
		if (fputc('\n', f) == EOF)
			return 0;
		*sp = 1;
		return 1;
	} else if (fd->type == FTYPE_PASSWORD) {
		if (!hassp && fputc('\n', f) == EOF)
			return 0;
		if (!gen_commentv(f, 1, COMMENT_C, 
		    "Omitting %s: is a password hash.", fd->name))
			return 0;
		if (fputc('\n', f) == EOF)
			return 0;
		*sp = 1;
		return 1;
	}

	if (fd->rolemap != NULL) {
		if (!hassp && fputc('\n', f) == EOF)
			return 0;
		if (fputs("\tswitch (db_role_stored"
		    "(p->priv_store)) {\n", f) == EOF)
			return 0;
		TAILQ_FOREACH(rs, &fd->rolemap->rq, entries)
			if (!gen_role(f, rs->role))
				return 0;
		if (!gen_comment(f, 2, COMMENT_C, 
		    "Don't export field to noted roles."))
			return 0;
		if (fputs("\t\tbreak;\n\tdefault:\n", f) == EOF)
			return 0;
		*sp = 1;
	} else
		tabs[1] = '\0';

	if (fd->type != FTYPE_STRUCT) {
		if (fd->flags & FIELD_NULL) {
			if (!hassp && !*sp && fputc('\n', f) == EOF)
				return 0;
			if (fprintf(f, "%sif (!p->has_%s)\n"
			       "%s\tkjson_putnullp(r, \"%s\");\n"
			       "%selse\n"
			       "%s\t", tabs, fd->name, tabs, 
			       fd->name, tabs, tabs) < 0)
				return 0;
		} else if (fputs(tabs, f) == EOF)
			return 0;

		switch (fd->type) {
		case FTYPE_BLOB:
			if (fprintf(f, "%s(r, \"%s\", buf%zu);\n",
			    puttypes[fd->type], 
			    fd->name, ++(*pos)) < 0)
				return 0;
			break;
		case FTYPE_BIT:
		case FTYPE_BITFIELD:
		case FTYPE_INT:
			if (fprintf(f, 
			    "%s(r, \"%s\", ORT_GET_%s_%s(p));\n", 
			    puttypes[fd->type], fd->name, 
			    fd->parent->name, fd->name) < 0)
				return 0;
			break;
		default:
			if (fprintf(f, "%s(r, \"%s\", p->%s);\n", 
			    puttypes[fd->type], 
			    fd->name, fd->name) < 0)
				return 0;
			break;
		}
		if ((fd->flags & FIELD_NULL) && !*sp) {
			if (fputc('\n', f) == EOF)
				return 0;
			*sp = 1;
		}
	} else if (fd->ref->source->flags & FIELD_NULL) {
		if (!hassp && !*sp && fputc('\n', f) == EOF)
			return 0;
		if (fprintf(f, "%sif (p->has_%s) {\n"
		    "%s\tkjson_objp_open(r, \"%s\");\n"
		    "%s\tjson_%s_data(r, &p->%s);\n"
		    "%s\tkjson_obj_close(r);\n"
		    "%s} else\n"
		    "%s\tkjson_putnullp(r, \"%s\");\n",
		    tabs, fd->name, tabs, fd->name,
		    tabs, fd->ref->target->parent->name, fd->name, 
		    tabs, tabs, tabs, fd->name) < 0)
			return 0;
		if (!*sp) {
			if (fputc('\n', f) == EOF)
				return 0;
			*sp = 1;
		}
	} else
		if (fprintf(f, "%skjson_objp_open(r, \"%s\");\n"
		    "%sjson_%s_data(r, &p->%s);\n"
		    "%skjson_obj_close(r);\n",
		    tabs, fd->name, tabs, 
		    fd->ref->target->parent->name, fd->name, tabs) < 0)
			return 0;

	if (fd->rolemap != NULL) {
		if (fputs("\t\tbreak;\n\t}\n\n", f) == EOF)
			return 0;
		*sp = 1;
	}

	return 1;
}

/*
 * Generate JSON parsing functions.
 * Return zero on failure, non-zero on success.
 */
static int
gen_json_parse(FILE *f, const struct strct *p)
{
	const struct field	*fd;
	int			 hasenum = 0, hasstruct = 0, 
				 hasblob = 0;

	/* Whether we need conversion space. */

	TAILQ_FOREACH(fd, &p->fq, entries) {
		if (fd->flags & FIELD_NOEXPORT)
			continue;
		if (fd->type == FTYPE_ENUM)
			hasenum = 1;
		else if (fd->type == FTYPE_BLOB)
			hasblob = 1;
		else if (fd->type == FTYPE_STRUCT)
			hasstruct = 1;
	}

	if (!gen_func_json_parse(f, p, 0))
		return 0;
	if (fputs("{\n"
	    "\tint i;\n"
	    "\tsize_t j;\n", f) == EOF)
		return 0;
	if (hasenum && fputs("\tint64_t tmpint;\n", f) == EOF)
		return 0;
	if ((hasblob || hasstruct) && fputs("\tint rc;\n", f) == EOF)
		return 0;
	if (hasblob && fputs("\tchar *tmpbuf;\n", f) == EOF)
		return 0;

	if (fputs("\n"
	    "\tif (toksz < 1 || t[0].type != JSMN_OBJECT)\n"
	    "\t\treturn 0;\n\n"
	    "\tfor (i = 0, j = 0; i < t[0].size; i++) {\n", f) == EOF)
		return 0;

	TAILQ_FOREACH(fd, &p->fq, entries) {
		if (fd->flags & FIELD_NOEXPORT)
			continue;
		if (fprintf(f, "\t\tif "
		    "(jsmn_eq(buf, &t[j+1], \"%s\")) {\n"
		    "\t\t\tj++;\n", fd->name) < 0)
			return 0;

		/* Check correct kind of token. */

		if ((fd->flags & FIELD_NULL) && fprintf
		    (f, "\t\t\tif (t[j+1].type == "
		     "JSMN_PRIMITIVE &&\n"
		     "\t\t\t    buf[t[j+1].start] == \'n\') {\n"
		     "\t\t\t\tp->has_%s = 0;\n"
		     "\t\t\t\tj++;\n"
		     "\t\t\t\tcontinue;\n"
		     "\t\t\t} else\n"
		     "\t\t\t\tp->has_%s = 1;\n",
		     fd->name, fd->name) < 0)
			return 0;

		switch (fd->type) {
		case FTYPE_DATE:
		case FTYPE_ENUM:
		case FTYPE_EPOCH:
		case FTYPE_INT:
		case FTYPE_REAL:
			if (fputs("\t\t\tif ((t[j+1].type != JSMN_STRING && "
			    "t[j+1].type != JSMN_PRIMITIVE) ||\n"
			    "\t\t\t    (buf[t[j+1].start] != \'-\' &&\n"
			    "\t\t\t    !isdigit((unsigned int)"
			    "buf[t[j+1].start])))\n"
			    "\t\t\t\treturn 0;\n", f) == EOF)
				return 0;
			break;
		case FTYPE_BIT:
		case FTYPE_BITFIELD:
			if (fputs("\t\t\tif ((t[j+1].type != JSMN_STRING && "
			    "t[j+1].type != JSMN_PRIMITIVE) ||\n"
			    "\t\t\t    !isdigit((unsigned int)"
			    "buf[t[j+1].start]))\n"
			    "\t\t\t\treturn 0;\n", f) == EOF)
				return 0;
			break;
		case FTYPE_BLOB:
		case FTYPE_TEXT:
		case FTYPE_PASSWORD:
		case FTYPE_EMAIL:
			if (fputs("\t\t\tif (t[j+1].type != JSMN_STRING)\n"
			    "\t\t\t\treturn 0;\n", f) == EOF)
				return 0;
			break;
		case FTYPE_STRUCT:
			if (fputs("\t\t\tif (t[j+1].type != JSMN_OBJECT)\n"
			    "\t\t\t\treturn 0;\n", f) == EOF)
				return 0;
			break;
		default:
			abort();
		}

		switch (fd->type) {
		case FTYPE_BIT:
		case FTYPE_BITFIELD:
		case FTYPE_INT:
			if (fprintf(f, "\t\t\tif (!jsmn_parse_int("
			    "buf + t[j+1].start,\n"
			    "\t\t\t    t[j+1].end - t[j+1].start, "
			    "&ORT_GET_%s_%s(p)))\n"
			    "\t\t\t\treturn 0;\n"
			    "\t\t\tj++;\n", 
			    fd->parent->name, fd->name) < 0)
				return 0;
			break;
		case FTYPE_DATE:
		case FTYPE_EPOCH:
			if (fprintf(f, "\t\t\tif (!jsmn_parse_int("
			    "buf + t[j+1].start,\n"
			    "\t\t\t    t[j+1].end - t[j+1].start, "
			    "&p->%s))\n"
			    "\t\t\t\treturn 0;\n"
			    "\t\t\tj++;\n", fd->name) < 0)
				return 0;
			break;
		case FTYPE_ENUM:
			if (fprintf(f, "\t\t\tif (!jsmn_parse_int("
			    "buf + t[j+1].start,\n"
			    "\t\t\t    t[j+1].end - t[j+1].start, "
			    "&tmpint))\n"
			    "\t\t\t\treturn 0;\n"
			    "\t\t\tp->%s = tmpint;\n"
			    "\t\t\tj++;\n", fd->name) < 0)
				return 0;
			break;
		case FTYPE_REAL:
			if (fprintf(f, "\t\t\tif (!jsmn_parse_real("
			    "buf + t[j+1].start,\n"
			    "\t\t\t    t[j+1].end - t[j+1].start, "
			    "&p->%s))\n"
			    "\t\t\t\treturn 0;\n"
			    "\t\t\tj++;\n", fd->name) < 0)
				return 0;
			break;
		case FTYPE_BLOB:
			if (fprintf(f, "\t\t\ttmpbuf = strndup\n"
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
			    "\t\t\tj++;\n", fd->name, fd->name, 
			    fd->name, fd->name) < 0)
				return 0;
			break;
		case FTYPE_TEXT:
		case FTYPE_PASSWORD:
		case FTYPE_EMAIL:
			if (fprintf(f, "\t\t\tp->%s = strndup\n"
			    "\t\t\t\t(buf + t[j+1].start,\n"
			    "\t\t\t\t t[j+1].end - t[j+1].start);\n"
			    "\t\t\tif (p->%s == NULL)\n"
			    "\t\t\t\treturn -1;\n"
			    "\t\t\tj++;\n", 
			    fd->name, fd->name) < 0)
				return 0;
			break;
		case FTYPE_STRUCT:
			if (fprintf(f, "\t\t\trc = jsmn_%s\n"
			    "\t\t\t\t(&p->%s, buf,\n"
			    "\t\t\t\t &t[j+1], toksz - j);\n"
			    "\t\t\tif (rc <= 0)\n"
			    "\t\t\t\treturn rc;\n"
			    "\t\t\tj += rc;\n",
			    fd->ref->target->parent->name,
			    fd->name) < 0)
				return 0;
			break;
		default:
			abort();
		}

		if (fputs("\t\t\tcontinue;\n\t\t}\n", f) == EOF)
			return 0;
	}

	if (fputc('\n', f) == EOF)
		return 0;
	if (!gen_comment(f, 2, COMMENT_C,
	    "Anything else is unexpected."))
		return 0;

	if (fputs("\n"
	    "\t\treturn 0;\n"
	    "\t}\n"
	    "\treturn j+1;\n"
	    "}\n\n", f) == EOF)
		return 0;

	if (!gen_func_json_clear(f, p, 0))
		return 0;
	if (fputs("\n"
	    "{\n"
	    "\tif (p == NULL)\n"
	    "\t\treturn;\n", f) == EOF)
		return 0;

	TAILQ_FOREACH(fd, &p->fq, entries)
		switch(fd->type) {
		case FTYPE_BLOB:
		case FTYPE_PASSWORD:
		case FTYPE_TEXT:
		case FTYPE_EMAIL:
			if (fprintf(f,
			    "\tfree(p->%s);\n", fd->name) < 0)
				return 0;
			break;
		case FTYPE_STRUCT:
			if (fd->ref->source->flags & FIELD_NULL) {
				if (fprintf(f, "\tif (p->has_%s)\n"
			            "\t\tjsmn_%s_clear(&p->%s);\n",
				    fd->ref->source->name, 
				    fd->ref->target->parent->name, 
				    fd->name) < 0)
					return 0;
				break;
			}
			if (fprintf(f, "\tjsmn_%s_clear(&p->%s);\n",
			    fd->ref->target->parent->name, 
			    fd->name) < 0)
				return 0;
			break;
		default:
			break;
		}

	if (fputs("}\n\n", f) == EOF)
		return 0;

	if (!gen_func_json_free_array(f, p, 0))
		return 0;
	if (fprintf(f, "{\n"
	    "\tsize_t i;\n"
	    "\tfor (i = 0; i < sz; i++)\n"
	    "\t\tjsmn_%s_clear(&p[i]);\n"
	    "\tfree(p);\n"
	    "}\n"
	    "\n", p->name) < 0)
		return 0;

	if (!gen_func_json_parse_array(f, p, 0))
		return 0;
	if (fprintf(f, "{\n"
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
	    "\n", p->name, p->name) < 0)
		return 0;

	return 1;
}

/*
 * Generate JSON output functions via kcgi(3).
 * Return zero on failure, non-zero on success.
 */
static int
gen_json_out(FILE *f, const struct strct *p)
{
	const struct field	*fd;
	size_t		 	 pos;
	int			 sp;

	if (!gen_func_json_data(f, p, 0))
		return 0;
	if (fputs("\n{\n", f) == EOF)
		return 0;

	/* 
	 * Declare our base64 buffers. 
	 * FIXME: have the buffer only be allocated if we have the value
	 * being serialised (right now it's allocated either way).
	 */

	pos = 0;
	TAILQ_FOREACH(fd, &p->fq, entries)
		if (fd->type == FTYPE_BLOB &&
		    !(fd->flags & FIELD_NOEXPORT) &&
		    fprintf(f, "\tchar *buf%zu;\n", ++pos) < 0)
			return 0;

	if (pos > 0) {
		if (fputs("\tsize_t sz;\n\n", f) == EOF)
			return 0;
		if (!gen_comment(f, 1, COMMENT_C,
		    "We need to base64 encode the binary "
		    "buffers prior to serialisation.\n"
		    "Allocate space for these buffers and do "
		    "so now.\n"
		    "We\'ll free the buffers at the epilogue "
		    "of the function."))
			return 0;
		if (fputc('\n', f) == EOF)
			return 0;
	}

	pos = 0;
	TAILQ_FOREACH(fd, &p->fq, entries) {
		if (fd->type != FTYPE_BLOB || 
		    (fd->flags & FIELD_NOEXPORT))
			continue;
		pos++;
		if (fprintf(f, "\tsz = (p->%s_sz + 2) / 3 * 4 + 1;\n"
		    "\tbuf%zu = malloc(sz);\n"
		    "\tif (buf%zu == NULL) {\n"
		    "\t\tperror(NULL);\n"
		    "\t\texit(EXIT_FAILURE);\n"
		    "\t}\n", fd->name, pos, pos) < 0)
			return 0;
		if ((fd->flags & FIELD_NULL) &&
		    fprintf(f, "\tif (p->has_%s)\n\t", fd->name) < 0)
			return 0;
		if (fprintf(f, "\tb64_ntop(p->%s, p->%s_sz, "
		    "buf%zu, sz);\n", fd->name, fd->name, pos) < 0)
			return 0;
	}

	if ((sp = (pos > 0)) && fputc('\n', f) == EOF)
		return 0;

	pos = 0;
	TAILQ_FOREACH(fd, &p->fq, entries)
		if (!gen_json_out_field(f, fd, &pos, &sp))
			return 0;

	/* Free our temporary base64 buffers. */

	pos = 0;
	TAILQ_FOREACH(fd, &p->fq, entries) {
		if (fd->flags & FIELD_NOEXPORT)
			continue;
		if (fd->type == FTYPE_BLOB && 
		    pos == 0 && fputc('\n', f) == EOF)
			return 0;
		if (fd->type == FTYPE_BLOB &&
		    fprintf(f, "\tfree(buf%zu);\n", ++pos) < 0)
			return 0;
	}

	if (fputs("}\n\n", f) == EOF)
		return 0;

	if (!gen_func_json_obj(f, p, 0))
		return 0;
	if (fprintf(f, "{\n"
	    "\tkjson_objp_open(r, \"%s\");\n"
	    "\tjson_%s_data(r, p);\n"
	    "\tkjson_obj_close(r);\n"
	    "}\n\n", p->name, p->name) < 0)
		return 0;

	if (p->flags & STRCT_HAS_QUEUE) {
		if (!gen_func_json_array(f, p, 0))
			return 0;
		if (fprintf(f, "{\n"
		    "\tstruct %s *p;\n"
		    "\n"
		    "\tkjson_arrayp_open(r, \"%s_q\");\n"
		    "\tTAILQ_FOREACH(p, q, _entries) {\n"
		    "\t\tkjson_obj_open(r);\n"
		    "\t\tjson_%s_data(r, p);\n"
		    "\t\tkjson_obj_close(r);\n"
		    "\t}\n"
		    "\tkjson_array_close(r);\n"
		    "}\n\n", p->name, p->name, p->name) < 0)
			return 0;
	}

	if (p->flags & STRCT_HAS_ITERATOR) {
		if (!gen_func_json_iterate(f, p, 0))
			return 0;
		if (fprintf(f, "{\n"
		    "\tstruct kjsonreq *r = arg;\n"
		    "\n"
		    "\tkjson_obj_open(r);\n"
		    "\tjson_%s_data(r, p);\n"
		    "\tkjson_obj_close(r);\n"
		    "}\n\n", p->name) < 0)
			return 0;
	}

	return 1;
}

/*
 * Generate all of the functions we've defined in our header for the
 * given structure "s".
 * Return zero on failure, non-zero on success.
 */
static int
gen_functions(FILE *f, const struct config *cfg, const struct strct *p, 
	int json, int jsonparse, int valids, int dbin,
	const struct filldepq *fq)
{
	const struct search 	*s;
	const struct update 	*u;
	const struct filldep	*fd;
	size_t	 		 pos;

	fd = get_filldep(fq, p);

	if (dbin) {
		if (fd != NULL && !gen_fill(f, cfg, p))
			return 0;
		if (fd != NULL && 
		   (fd->need & FILLDEP_FILL_R) && 
		   !gen_fill_r(f, cfg, p))
			return 0;
		if (!gen_unfill(f, cfg, p))
			return 0;
		if (!gen_unfill_r(f, p))
			return 0;
		if (!gen_reffind(f, cfg, p))
			return 0;
		if (!gen_free(f, p))
			return 0;
		if (!gen_freeq(f, p))
			return 0;
		if (!gen_insert(f, cfg, p))
			return 0;
	}

	if (json && !gen_json_out(f, p))
		return 0;
	if (jsonparse && !gen_json_parse(f, p))
		return 0;
	if (valids && !gen_valids(f, p))
		return 0;

	if (dbin) {
		pos = 0;
		TAILQ_FOREACH(s, &p->sq, entries)
			if (s->type == STYPE_SEARCH) {
				if (!gen_search(f, cfg, s, pos++))
					return 0;
			} else if (s->type == STYPE_LIST) {
				if (!gen_list(f, cfg, s, pos++))
					return 0;
			} else if (s->type == STYPE_COUNT) {
				if (!gen_count(f, cfg, s, pos++))
					return 0;
			} else
				if (!gen_iterator(f, cfg, s, pos++))
					return 0;
		pos = 0;
		TAILQ_FOREACH(u, &p->uq, entries)
			if (!gen_update(f, cfg, u, pos++))
				return 0;
		pos = 0;
		TAILQ_FOREACH(u, &p->dq, entries)
			if (!gen_update(f, cfg, u, pos++))
				return 0;
	}

	return 1;
}

/*
 * Generate a single "struct kvalid" with the given validation function
 * and the form name, which we have as "struct-field".
 * Return zero on failure, non-zero on success.
 */
static int
gen_valid(FILE *f, const struct strct *p)
{
	const struct field	*fd;

	/*
	 * Don't generate an entry for structs.
	 * Blobs always succeed (NULL validator).
	 * Non-enums without limits use the default function.
	 */

	TAILQ_FOREACH(fd, &p->fq, entries) {
		if (fd->type == FTYPE_BLOB) {
			if (fprintf(f, "\t{ NULL, \"%s-%s\" },\n",
			    p->name, fd->name) < 0)
				return 0;
			continue;
		} else if (fd->type == FTYPE_STRUCT)
			continue;

		if (fd->type != FTYPE_ENUM && TAILQ_EMPTY(&fd->fvq)) {
			if (fprintf(f, "\t{ %s, \"%s-%s\" },\n",
			    validtypes[fd->type], p->name, 
			    fd->name) < 0)
				return 0;
			continue;
		}
		if (fprintf(f, "\t{ valid_%s_%s, \"%s-%s\" },\n",
		    p->name, fd->name, p->name, fd->name) < 0)
			return 0;
	}

	return 1;
}

/*
 * Generate the schema for a given table.
 * This macro accepts a single parameter that's given to all of the
 * members so that a later SELECT can use INNER JOIN xxx AS yyy and have
 * multiple joins on the same table.
 * Return zero on failure, non-zero on success.
 */
static int
gen_schema(FILE *f, const struct strct *p)
{
	const struct field	*fd;
	const char		*s = "";

	if (fprintf(f, "#define DB_SCHEMA_%s(_x) \\", p->name) < 0)
		return 0;

	TAILQ_FOREACH(fd, &p->fq, entries) {
		if (fd->type == FTYPE_STRUCT)
			continue;
		if (fprintf(f, "%s\n", s) < 0)
			return 0;
		if (fprintf(f, "\t#_x \".%s\"", fd->name) < 0)
			return 0;
		s = " \",\" \\";
	}

	return fputc('\n', f) != EOF;
}

int
ort_lang_c_source(const struct ort_lang_c *args,
	const struct config *cfg, FILE *f)
{
	const struct strct 	*p;
	const struct search	*s;
	const char		*start, *cp;
	size_t			 sz;
	int			 need_kcgi = 0, 
				 need_kcgijson = 0, 
				 need_sqlbox = 0,
				 need_b64 = 0;
	struct filldepq		 fq;
	struct filldep		*fd;

#if !HAVE_B64_NTOP
	need_b64 = 1;
#endif

	if (!gen_commentv(f, 0, COMMENT_C, 
	    "WARNING: automatically generated by ort %s.\n"
	    "DO NOT EDIT!", ORT_VERSION))
		return 0;

#if defined(__linux__)
	if (fputs("#define _GNU_SOURCE\n"
	    "#define _DEFAULT_SOURCE\n", f) == EOF)
		return 0;
#endif
#if defined(__sun)
	if (fputs("#ifndef _XOPEN_SOURCE\n"
	    "# define _XOPEN_SOURCE\n"
	    "#endif\n"
	    "#define _XOPEN_SOURCE_EXTENDED 1\n"
	    "#ifndef __EXTENSIONS__\n"
	    "# define __EXTENSIONS__\n"
	    "#endif\n", f) == EOF)
		return 0;
#endif

	/* Start with all headers we'll need. */

	/* FIXME: HAVE_SYS_QUEUE pulled in from compat for Linux. */

	if (need_b64 && fputs
 	    ("#include <sys/types.h> /* b64_ntop() */\n", f) == EOF)
		return 0;

	if (fputs("#include <sys/queue.h>\n\n"
	    "#include <assert.h>\n", f) == EOF)
		return 0;

	if (!need_b64) {
		TAILQ_FOREACH(p, &cfg->sq, entries)  {
			if (!(p->flags & STRCT_HAS_BLOB))
				continue;
			if (!gen_comment(f, 0, COMMENT_C,
			     "Required for b64_ntop()."))
				return 0;
			if (!(args->flags & ORT_LANG_C_JSON_JSMN))
				if (fputs("#include "
				    "<ctype.h>\n", f) == EOF)
					return 0;
			if (fputs("#include <netinet/in.h>\n"
			    "#include <resolv.h>\n", f) == EOF)
				return 0;
			break;
		}
	} else 
		if (fputs("#include <ctype.h> "
		    "/* b64_ntop() */\n", f) == EOF)
			return 0;

	if ((args->includes & ORT_LANG_C_DB_SQLBOX) ||
	    (args->flags & ORT_LANG_C_DB_SQLBOX))
		need_sqlbox = 1;
	if ((args->includes & ORT_LANG_C_VALID_KCGI) ||
	    (args->flags & ORT_LANG_C_VALID_KCGI))
		need_kcgi = 1;
	if ((args->includes & ORT_LANG_C_JSON_KCGI) ||
	    (args->flags & ORT_LANG_C_JSON_KCGI))
		need_kcgi = need_kcgijson = 1;

	if (args->flags & ORT_LANG_C_JSON_JSMN) {
		if (!need_b64 && 
		    fputs("#include <ctype.h>\n", f) == EOF)
			return 0;
		if (fputs("#include <inttypes.h>\n", f) == EOF)
			return 0;
	}

	if (need_kcgi &&
	    fputs("#include <stdarg.h>\n", f) == EOF)
		return 0;

	if (fputs("#include <stdio.h>\n"
	    "#include <stdint.h> /* int64_t */\n"
	    "#include <stdlib.h>\n"
	    "#include <string.h>\n"
	    "#include <time.h> /* _XOPEN_SOURCE and gmtime_r()*/\n"
	    "#include <unistd.h>\n\n", f) == EOF)
		return 0;

	if (need_sqlbox &&
	    fputs("#include <sqlbox.h>\n", f) == EOF)
		return 0;
	if (need_kcgi &&
	    fputs("#include <kcgi.h>\n", f) == EOF)
		return 0;
	if (need_kcgijson &&
	    fputs("#include <kcgijson.h>\n", f) == EOF)
		return 0;

	if (fputc('\n', f) == EOF)
		return 0;

	if ((cp = args->header) != NULL) {
		while (*cp != '\0') {
			while (isspace((unsigned char)*cp))
				cp++;
			if (*cp == '\0')
				continue;
			start = cp;
			for (sz = 0; '\0' != *cp; sz++, cp++)
				if (*cp == ',' ||
				    isspace((unsigned char)*cp))
					break;
			if (sz && fprintf(f, 
			    "#include \"%.*s\"\n", (int)sz, start) < 0)
				return 0;
			while (*cp == ',')
				cp++;
		}
		if (fputc('\n', f) == EOF)
			return 0;
	}

#ifndef __OpenBSD__
	if (fprintf(f, "%s\n", args->ext_gensalt) < 0)
		return 0;
#endif

	if (need_b64 &&
	    fprintf(f, "%s\n", args->ext_b64_ntop) < 0)
		return 0;
	if ((args->flags & ORT_LANG_C_JSON_JSMN) &&
	    fprintf(f, "%s\n", args->ext_jsmn) < 0)
		return 0;

	if (args->flags & ORT_LANG_C_DB_SQLBOX) {
		if (!gen_comment(f, 0, COMMENT_C,
		    "All SQL statements we'll later "
		    "define in \"stmts\"."))
			return 0;
		if (fputs("enum\tstmt {\n", f) == EOF)
			return 0;
		TAILQ_FOREACH(p, &cfg->sq, entries)
			if (!gen_sql_enums(f, 1, p, LANG_C))
				return 0;
		if (fputs("\tSTMT__MAX\n};\n\n", f) == EOF)
			return 0;

		if (!gen_comment(f, 0, COMMENT_C,
		    "Definition of our opaque \"ort\", "
		    "which contains role information."))
			return 0;
		if (fputs("struct\tort {\n", f) == EOF)
			return 0;
		if (!gen_comment(f, 1, COMMENT_C,
		    "Hidden database connection"))
			return 0;
		if (fputs("\tstruct sqlbox *db;\n", f) == EOF)
			return 0;

		if (!TAILQ_EMPTY(&cfg->rq)) {
			if (!gen_comment(f, 1, COMMENT_C,
			    "Current RBAC role."))
				return 0;
			if (fputs("\tenum ort_role "
			    "role;\n};\n\n", f) == EOF)
				return 0;
			if (!gen_comment(f, 0, COMMENT_C,
			    "A saved role state attached to "
			    "generated objects.\n"
			    "We'll use this to make sure that "
			    "we shouldn't export data that "
			    "we've kept unexported in a given "
			    "role (at the time of acquisition)."))
				return 0;
			if (fputs("struct\tort_store {\n", f) == EOF)
				return 0;
			if (!gen_comment(f, 1, COMMENT_C,
			    "Role at the time of acquisition."))
				return 0;
			if (fputs("\tenum ort_role role;\n", f) == EOF)
				return 0;
		}

		if (fputs("};\n\n", f) == EOF)
			return 0;

		if (!gen_comment(f, 0, COMMENT_C, 
		    "Table columns.\n"
		    "The macro accepts a table name because "
		    "we use AS statements a lot.\n"
		    "This is because tables can appear multiple "
		    "times in a single query and need aliasing."))
			return 0;
		TAILQ_FOREACH(p, &cfg->sq, entries)
			if (!gen_schema(f, p))
				return 0;
		if (fputc('\n', f) == EOF)
			return 0;

		if (!gen_comment(f, 0, COMMENT_C,
		    "Our full set of SQL statements.\n"
		    "We define these beforehand because "
		    "that's how sqlbox(3) handles statement "
		    "generation.\n"
		    "Notice the \"AS\" part: this allows "
		    "for multiple inner joins without "
		    "ambiguity."))
			return 0;
		if (fputs("static\tconst char "
		    "*const stmts[STMT__MAX] = {\n", f) == EOF)
			return 0;
		TAILQ_FOREACH(p, &cfg->sq, entries)
			if (!gen_sql_stmts(f, 1, p, LANG_C))
				return 0;
		if (fputs("};\n\n", f) == EOF)
			return 0;
	}

	/*
	 * Validation array.
	 * This is declared in the header file, but we define it now.
	 * All of the functions have been defined in the header file.
	 */

	if (args->flags & ORT_LANG_C_VALID_KCGI) {
		if (fputs("const struct kvalid "
		    "valid_keys[VALID__MAX] = {\n", f) == EOF)
			return 0;
		TAILQ_FOREACH(p, &cfg->sq, entries)
			if (!gen_valid(f, p))
				return 0;
		if (fputs("};\n\n", f) == EOF)
			return 0;
	}

	/* Define our functions. */

	if (!gen_comment(f, 0, COMMENT_C,
	    "Finally, all of the functions we'll use.\n"
	    "All of the non-static functions are documented "
	    "in the associated header file."))
		return 0;
	if (fputc('\n', f) == EOF)
		return 0;

	if (args->flags & ORT_LANG_C_DB_SQLBOX) {
		if (!gen_transactions(f, cfg))
			return 0;
		if (!gen_open(f, cfg))
			return 0;
		if (!gen_close(f, cfg))
			return 0;
		if (!TAILQ_EMPTY(&cfg->rq) &&
		    !gen_func_role_transitions(f, cfg))
			return 0;
	}

	/*
	 * Before we generate our functions, we need to decide which
	 * "fill" functions we're going to generate.
	 * The gen_filldep() function keeps track of which structures
	 * are referenced directly or indirectly from queries.
	 * If we don't do this, we might emit superfluous functions.
	 */

	TAILQ_INIT(&fq);

	TAILQ_FOREACH(p, &cfg->sq, entries)
		TAILQ_FOREACH(s, &p->sq, entries)
			if (!gen_filldep(&fq, p, FILLDEP_FILL_R))
				return 0;

	TAILQ_FOREACH(p, &cfg->sq, entries)
		gen_functions(f, cfg, p, 
			args->flags & ORT_LANG_C_JSON_KCGI, 
			args->flags & ORT_LANG_C_JSON_JSMN, 
			args->flags & ORT_LANG_C_VALID_KCGI, 
			args->flags & ORT_LANG_C_DB_SQLBOX, &fq);

	while ((fd = TAILQ_FIRST(&fq)) != NULL) {
		TAILQ_REMOVE(&fq, fd, entries);
		free(fd);
	}

	return 1;
}

