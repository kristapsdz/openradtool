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

#include <assert.h>
#include <ctype.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ort.h"
#include "lang-c.h"

static	const char *const stypes[STYPE__MAX] = {
	"count", /* STYPE_COUNT */
	"get", /* STYPE_SEARCH */
	"list", /* STYPE_LIST */
	"iterate", /* STYPE_ITERATE */
};

static	const char *const utypes[UP__MAX] = {
	"update", /* UP_MODIFY */
	"delete", /* UP_DELETE */
};

static	const char *const ftypes[FTYPE__MAX] = {
	"int64_t ", /* FTYPE_BIT */
	"time_t ", /* FTYPE_DATE */
	"time_t ", /* FTYPE_EPOCH */
	"int64_t ", /* FTYPE_INT */
	"double ", /* FTYPE_REAL */
	"const void *", /* FTYPE_BLOB */
	"const char *", /* FTYPE_TEXT */
	"const char *", /* FTYPE_PASSWORD */
	"const char *", /* FTYPE_EMAIL */
	NULL, /* FTYPE_STRUCT */
	NULL, /* FTYPE_ENUM */
	"int64_t ", /* FTYPE_BITFIELD */
};

static	const char *const modtypes[MODTYPE__MAX] = {
	"cat", /* MODTYPE_CONCAT */
	"dec", /* MODTYPE_DEC */
	"inc", /* MODTYPE_INC */
	"set", /* MODTYPE_SET */
	"strset", /* MODTYPE_STRSET */
};

static	const char *const optypes[OPTYPE__MAX] = {
	"eq", /* OPTYPE_EQUAL */
	"ge", /* OPTYPE_GE */
	"gt", /* OPTYPE_GT */
	"le", /* OPTYPE_LE */
	"lt", /* OPTYPE_LT */
	"neq", /* OPTYPE_NEQUAL */
	"like", /* OPTYPE_LIKE */
	"and", /* OPTYPE_AND */
	"or", /* OPTYPE_OR */
	"streq", /* OPTYPE_STREQ */
	"strneq", /* OPTYPE_STRNEQ */
	/* Unary types... */
	"isnull", /* OPTYPE_ISNULL */
	"notnull", /* OPTYPE_NOTNULL */
};

/*
 * Generate the db_open function header.
 * If "decl" is non-zero, this is the declaration; otherwise, the
 * definition header.
 * Return zero on failure, non-zero on success.
 */
int
gen_func_db_open(FILE *f, int decl)
{

	return fprintf(f, "struct ort *%sdb_open"
		"(const char *file)%s\n",
		decl ? "" : "\n", decl ? ";" : "") > 0;
}

/*
 * Generate the db_open_logging function header.
 * If "decl" is non-zero, this is the declaration; otherwise, the
 * definition header.
 * Return zero on failure, non-zero on success.
 */
int
gen_func_db_open_logging(FILE *f, int decl)
{

	return fprintf(f, "struct ort *%sdb_open_logging"
		"(const char *file,\n"
		"\tvoid (*log)(const char *, void *),\n"
		"\tvoid (*log_short)(const char *, ...), "
		"void *log_arg)%s\n",
		decl ? "" : "\n", decl ? ";" : "") > 0;
}

/*
 * Generate the db_logging_data function header.
 * If "decl" is non-zero, this is the declaration; otherwise, the
 * definition header.
 * Return zero on failure, non-zero on success.
 */
int
gen_func_db_set_logging(FILE *f, int decl)
{

	return fprintf(f, "void%sdb_logging_data"
		"(struct ort *ort, const void *arg, size_t sz)%s\n",
		decl ? " " : "\n", decl ? ";" : "") > 0;
}

/*
 * Generate the db_role function header.
 * If "decl" is non-zero, this is the declaration; otherwise, the
 * definition header.
 * Return zero on failure, non-zero on success.
 */
int
gen_func_db_role(FILE *f, int decl)
{

	return fprintf(f, "void%sdb_role"
		"(struct ort *ctx, enum ort_role r)%s\n",
		decl ? " " : "\n", decl ? ";" : "") > 0;
}

/*
 * Generate the db_role_current function header.
 * If "decl" is non-zero, this is the declaration; otherwise, the
 * definition header.
 * Return zero on failure, non-zero on success.
 */
int
gen_func_db_role_current(FILE *f, int decl)
{

	return fprintf(f, "enum ort_role%sdb_role_current"
		"(struct ort *ctx)%s\n",
		decl ? " " : "\n", decl ? ";" : "") > 0;
}

/*
 * Generate the db_role_stored function header.
 * If "decl" is non-zero, this is the declaration; otherwise, the
 * definition header.
 * Return zero on failure, non-zero on success.
 */
int
gen_func_db_role_stored(FILE *f, int decl)
{

	return fprintf(f, "enum ort_role%sdb_role_stored"
		"(struct ort_store *s)%s\n", 
		decl ? " " : "\n", decl ? ";" : "") > 0;
}

/*
 * Generate the db_trans_rollback function header.
 * If a declaration, print on one line with a comma following, otherwise
 * two lines and no comma.
 */
int
gen_func_db_trans_rollback(FILE *f, int decl)
{

	return fprintf(f, "void%sdb_trans_rollback"
		"(struct ort *ctx, size_t id)%s\n",
		decl ? " " : "\n", decl ? ";" : "") > 0;
}

/*
 * Generate the db_trans_commit function header.
 * If a declaration, print on one line with a comma following, otherwise
 * two lines and no comma.
 */
int
gen_func_db_trans_commit(FILE *f, int decl)
{

	return fprintf(f, "void%sdb_trans_commit"
		"(struct ort *ctx, size_t id)%s\n",
		decl ? " " : "\n", decl ? ";" : "") > 0;
}

/*
 * Generate the db_trans_open function header.
 * If a declaration, print on one line with a comma following, otherwise
 * two lines and no comma.
 */
int
gen_func_db_trans_open(FILE *f, int decl)
{

	return fprintf(f, "void%sdb_trans_open"
		"(struct ort *ctx, size_t id, int mode)%s\n",
		decl ? " " : "\n", decl ? ";" : "") > 0;
}

/*
 * Generate the db_close function header.
 * If "decl" is non-zero, this is the declaration; otherwise, the
 * definition header.
 * Return zero on failure, non-zero on success.
 */
int
gen_func_db_close(FILE *f, int decl)
{

	return fprintf(f, "void%sdb_close(struct ort *p)%s\n",
		decl ? " " : "\n", decl ? ";" : "") > 0;
}

/*
 * Generate the variables in a function header, breaking the line at 72
 * characters to indent 5 spaces.  The "col" is the current position in
 * the output line.
 * Returns <0 on failure or the current position in the output line.
 */
static int
print_var(FILE *f, size_t pos, size_t col, 
	const struct field *fd, unsigned int flags)
{
	int	rc;

	if (fputc(',', f) == EOF)
		return -1;
	col++;

	if (col >= 72)
		rc = fprintf(f, "\n     ");
	else
		rc = fprintf(f, " ");

	if (rc < 0)
		return -1;
	col += rc;

	if (FTYPE_ENUM == fd->type) {
		rc = fprintf(f, "enum %s %sv%zu", fd->enm->name,
			(flags & FIELD_NULL) ? "*" : "", pos);
		if (rc < 0)
			return -1;
		return col + rc;
	}

	if (fd->type == FTYPE_BLOB) {
		if ((rc = fprintf(f, "size_t v%zu_sz, ", pos)) < 0)
			return -1;
		col += rc;
	}

	rc = fprintf(f, "%s%sv%zu", ftypes[fd->type], 
		(flags & FIELD_NULL) ?  "*" : "", pos);
	if (rc < 0)
		return -1;

	return col + rc;
}

/*
 * Generate the db_xxxx_update function header.
 * If "decl" is non-zero, this is the declaration; otherwise, the
 * definition header.
 * Return zero on failure, non-zero on success.
 */
int
gen_func_db_update(FILE *f, const struct update *u, int decl)
{
	const struct uref	*ur;
	size_t			 pos = 1, col = 0, sz;
	int			 rc;
	const char		*type;

	type = u->type == UP_MODIFY ? "int" : "void";

	/* Start with return value. */

	if (!decl) {
		if (fprintf(f, "%s\n", type) < 0)
			return 0;
	} else {
		if ((rc = fprintf(f, "%s ", type)) < 0)
			return 0;
		col = rc;
	}

	/* Now function name. */

	rc = fprintf(f, "db_%s_%s",
		u->parent->name, utypes[u->type]);
	if (rc < 0)
		return 0;
	sz = rc;

	if (u->name == NULL && u->type == UP_MODIFY) {
		if (!(u->flags & UPDATE_ALL))
			TAILQ_FOREACH(ur, &u->mrq, entries) {
				rc = fprintf(f, "_%s_%s", 
					ur->field->cname, 
					modtypes[ur->mod]);
				if (rc < 0)
					return 0;
				sz += rc;
			}
		if (!TAILQ_EMPTY(&u->crq)) {
			if (fputs("_by", f) == EOF)
				return 0;
			sz += 3;
			TAILQ_FOREACH(ur, &u->crq, entries) {
				rc = fprintf(f, "_%s_%s", 
					ur->field->cname, 
					optypes[ur->op]);
				if (rc < 0)
					return 0;
				sz += rc;
			}
		}
	} else if (u->name == NULL) {
		if (!TAILQ_EMPTY(&u->crq)) {
			if (fputs("_by", f) == EOF)
				return 0;
			sz += 3;
			TAILQ_FOREACH(ur, &u->crq, entries) {
				rc = fprintf(f, "_%s_%s", 
					ur->field->cname, 
					optypes[ur->op]);
				if (rc < 0)
					return 0;
				sz += rc;
			}
		}
	} else {
		if ((rc = fprintf(f, "_%s", u->name)) < 0)
			return 0;
		sz += rc;
	}

	if ((col += sz) >= 72) {
		if (fputs("\n    ", f) == EOF)
			return 0;
		col = 4;
	}

	/* Arguments starting with database pointer. */

	if (fputs("(struct ort *ctx", f) == EOF)
		return 0;
	col += 16;

	TAILQ_FOREACH(ur, &u->mrq, entries) {
		if ((rc = print_var(f, pos++, col, 
		    ur->field, ur->field->flags)) < 0)
			return 0;
		col = rc;
	}

	TAILQ_FOREACH(ur, &u->crq, entries)
		if (!OPTYPE_ISUNARY(ur->op)) {
			if ((rc = print_var(f, 
			    pos++, col, ur->field, 0)) < 0)
				return 0;
			col = rc;
		}

	return fprintf(f, ")%s", decl ? ";\n" : "") > 0;
}

/*
 * Generate the db_xxxx_{count,get,list,iterate} function header.
 * If "decl" is non-zero, this is the declaration; otherwise, the
 * definition header.
 * Return zero on failure, non-zero on success.
 */
int
gen_func_db_search(FILE *f, const struct search *s, int decl)
{
	const struct sent	*sent;
	const struct strct	*retstr;
	size_t			 pos = 1, col = 0, sz = 0;
	int			 rc;

	/* 
	 * If we have a "distinct" clause, we use that to generate
	 * responses, not the structure itself.
	 */

	retstr = s->dst != NULL ? s->dst->strct : s->parent;

	/* Start with return value. */

	if (s->type == STYPE_SEARCH)
		rc = fprintf(f, "struct %s *", retstr->name);
	else if (s->type == STYPE_LIST)
		rc = fprintf(f, "struct %s_q *", retstr->name);
	else if (s->type == STYPE_ITERATE)
		rc = fprintf(f, "void");
	else
		rc = fprintf(f, "uint64_t");

	if (rc < 0)
		return 0;
	col += rc;

	if (!decl) {
		if (fputc('\n', f) == EOF)
			return 0;
		col = 0;
	} else if (s->type != STYPE_SEARCH && s->type != STYPE_LIST) {
		if (fputc(' ', f) == EOF)
			return 0;
		col++;
	}

	/* Now function name. */

	rc = fprintf(f, "db_%s_%s", s->parent->name, stypes[s->type]);
	if (rc < 0)
		return 0;
	sz += rc;
	if (s->name == NULL && !TAILQ_EMPTY(&s->sntq)) {
		if (fputs("_by", f) == EOF)
			return 0;
		sz += 3;
		TAILQ_FOREACH(sent, &s->sntq, entries) {
			rc = fprintf(f, "_%s_%s", 
				sent->uname, optypes[sent->op]);
			if (rc < 0)
				return 0;
			sz += rc;
		}
	} else if (s->name != NULL) {
		if ((rc = fprintf(f, "_%s", s->name)) < 0)
			return 0;
		sz += rc;
	}

	if ((col += sz) >= 72) {
		if (fputs("\n    ", f) == EOF)
			return 0;
		col = 4;
	}

	/* Arguments starting with database pointer. */

	if (fputs("(struct ort *ctx", f) == EOF)
		return 0;
	col += 16;

	if (s->type == STYPE_ITERATE) {
		if ((rc = fprintf(f, 
		    ", %s_cb cb, void *arg", retstr->name)) < 0)
			return 0;
		col += rc;
	}

	TAILQ_FOREACH(sent, &s->sntq, entries)
		if (!OPTYPE_ISUNARY(sent->op)) {
			if ((rc = print_var
			    (f, pos++, col, sent->field, 0)) < 0)
				return 0;
			col = rc;
		}

	return fprintf(f, ")%s", decl ? ";\n" : "") > 0;
}

/*
 * Generate the db_xxxx_insert function header.
 * If "decl" is non-zero, this is the declaration; otherwise, the
 * definition header.
 * Return zero on failure, non-zero on success.
 */
int
gen_func_db_insert(FILE *f, const struct strct *p, int decl)
{
	const struct field *fd;
	size_t	 	    pos = 1, col = 0;
	int		    rc;

	/* Start with return value. */

	if (!decl) {
		if (fputs("int64_t\n", f) == EOF)
			return 0;
	} else {
		if (fputs("int64_t ", f) == EOF)
			return 0;
		col += 8;
	}

	/* Now function name. */

	if ((rc = fprintf(f, "db_%s_insert", p->name)) < 0)
		return 0;
	col += rc;

	if (col >= 72) {
		if (fputc('\n', f) == EOF)
			return 0;
		if ((rc = fprintf(f, "    ")) < 0)
			return 0;
		col = rc;
	}

	/* Arguments starting with database pointer. */

	if ((rc = fprintf(f, "(struct ort *ctx")) < 0)
		return 0;
	col += rc;

	TAILQ_FOREACH(fd, &p->fq, entries)
		if (!(fd->type == FTYPE_STRUCT || 
		    (fd->flags & FIELD_ROWID))) {
			rc = print_var(f, pos++, col, fd, fd->flags);
			if (rc < 0)
				return 0;
			col = rc;
		}

	return fprintf(f, ")%s", decl ? ";\n" : "") > 0;
}

/*
 * Generate the db_xxxx_freeq function header.
 * If "decl" is non-zero, this is the declaration; otherwise, the
 * definition header.
 * Return zero on failure, non-zero on success.
 */
int
gen_func_db_freeq(FILE *f, const struct strct *p, int decl)
{

	return fprintf(f, "void%sdb_%s_freeq(struct %s_q *q)%s",
	       decl ? " " : "\n", p->name, p->name,
	       decl ? ";\n" : "") > 0;
}

/*
 * Generate the db_xxxx_free function header.
 * If "decl" is non-zero, this is the declaration; otherwise, the
 * definition header.
 * Return zero on failure, non-zero on success.
 */
int
gen_func_db_free(FILE *f, const struct strct *p, int decl)
{

	return fprintf(f, "void%sdb_%s_free(struct %s *p)%s",
	       decl ? " " : "\n", p->name, p->name,
	       decl ? ";\n" : "") > 0;
}

/*
 * Generate the valid_xxx_yyy function header.
 * If "decl" is non-zero, this is the declaration; otherwise, the
 * definition header.
 * Return zero on failure, non-zero on success.
 */
int
gen_func_valid(FILE *f, const struct field *fd, int decl)
{

	return fprintf(f, "int%svalid_%s_%s(struct kpair *p)%s",
		decl ? " " : "\n", 
		fd->parent->name, fd->cname,
		decl ? ";\n" : "\n") > 0;
}

/*
 * Generate the jsmn_xxxx_clear function header.
 * If "decl" is non-zero, this is the declaration; otherwise, the
 * definition header.
 * Return zero on failure, non-zero on success.
 */
int
gen_func_json_clear(FILE *f, const struct strct *p, int decl)
{

	return fprintf(f, "void%sjsmn_%s_clear(struct %s *p)%s",
		decl ? " " : "\n", p->name, p->name, 
		decl ? ";\n" : "\n") > 0;
}

/*
 * Generate the jsmn_xxxx_free_array function header.
 * If "decl" is non-zero, this is the declaration; otherwise, the
 * definition header.
 * Return zero on failure, non-zero on success.
 */
int
gen_func_json_free_array(FILE *f, const struct strct *p, int decl)
{

	return fprintf(f, "void%sjsmn_%s_free_array"
		"(struct %s *p, size_t sz)%s",
		decl ? " " : "\n", p->name, p->name, 
		decl ? ";\n" : "\n") > 0;
}

/*
 * Generate the jsmn_xxx_array function header.
 * If "decl" is non-zero, this is the declaration; otherwise, the
 * definition header.
 * Return zero on failure, non-zero on success.
 */
int
gen_func_json_parse_array(FILE *f, const struct strct *p, int decl)
{

	return fprintf(f, "int%sjsmn_%s_array"
		"(struct %s **p, size_t *sz, const char *buf, "
		"const jsmntok_t *t, size_t toksz)%s",
		decl ? " " : "\n", p->name, p->name, 
		decl ? ";\n" : "\n") > 0;
}

/*
 * Generate the jsmn_xxxx function header.
 * If "decl" is non-zero, this is the declaration; otherwise, the
 * definition header.
 * Return zero on failure, non-zero on success.
 */
int
gen_func_json_parse(FILE *f, const struct strct *p, int decl)
{

	return fprintf(f, "int%sjsmn_%s"
		"(struct %s *p, const char *buf, "
		"const jsmntok_t *t, size_t toksz)%s",
		decl ? " " : "\n", p->name, p->name, 
		decl ? ";\n" : "\n") > 0;
}

/*
 * Generate the json_xxxx_data function header.
 * If "decl" is non-zero, this is the declaration; otherwise, the
 * definition header.
 * Return zero on failure, non-zero on success.
 */
int
gen_func_json_data(FILE *f, const struct strct *p, int decl)
{

	return fprintf(f, "void%sjson_%s_data"
		"(struct kjsonreq *r, const struct %s *p)%s",
		decl ? " " : "\n", p->name, 
		p->name, decl ? ";\n" : "") > 0;
}

/*
 * Generate the json_xxxx_array function header.
 * If "decl" is non-zero, this is the declaration; otherwise, the
 * definition header.
 * Return zero on failure, non-zero on success.
 */
int
gen_func_json_array(FILE *f, const struct strct *p, int decl)
{

	return fprintf(f, "void%sjson_%s_array"
		"(struct kjsonreq *r, const struct %s_q *q)%s\n",
		decl ? " " : "\n", p->name, 
		p->name, decl ? ";" : "") > 0;
}

/*
 * Generate the json_xxx_obj function header.
 * If "decl" is non-zero, this is the declaration; otherwise, the
 * definition header.
 * Return zero on failure, non-zero on success.
 */
int
gen_func_json_obj(FILE *f, const struct strct *p, int decl)
{

	return fprintf(f, "void%sjson_%s_obj"
		"(struct kjsonreq *r, const struct %s *p)%s\n",
		decl ? " " : "\n", p->name, 
		p->name, decl ? ";" : "") > 0;
}

/*
 * Generate the json_xxx_iterate function header.
 * If "decl" is non-zero, this is the declaration; otherwise, the
 * definition header.
 * Return zero on failure, non-zero on success.
 */
int
gen_func_json_iterate(FILE *f, const struct strct *p, int decl)
{

	return fprintf(f, "void%sjson_%s_iterate"
		"(const struct %s *p, void *arg)%s\n",
		decl ? " " : "\n", p->name, 
		p->name, decl ? ";" : "") > 0;
}

/*
 * This recursively adds all structures to "fq" for which we need to
 * generate fill or fill_r and reffind functions (as defined by "need",
 * which is a bitfield with definition in struct filldep).
 * The former case is met if the structure is directly referenced by a
 * query or comes off a possibly-null reference.
 * The latter is met if the structure is indirectly referenced by a
 * query and comes off a possibly-null reference.
 */
int
gen_filldep(struct filldepq *fq, const struct strct *p, unsigned int need)
{
	struct filldep		*fill;
	const struct field	*fd;

	TAILQ_FOREACH(fill, fq, entries)
		if (fill->p == p) {
			fill->need |= need;
			return 1;
		}

	if ((fill = calloc(1, sizeof(struct filldep))) == NULL)
		return 0;

	TAILQ_INSERT_TAIL(fq, fill, entries);
	fill->p = p;
	fill->need = need;

	/* 
	 * Recursively add all children.
	 * If they may be null, we'll need to generate a reffind and a
	 * fill_r for them.
	 */

	TAILQ_FOREACH(fd, &p->fq, entries) {
		if (fd->type != FTYPE_STRUCT)
			continue;
		if (!gen_filldep(fq, fd->ref->target->parent,
		    (fd->ref->source->flags & FIELD_NULL) ?
		    (FILLDEP_FILL_R |FILLDEP_REFFIND) : 
		    FILLDEP_FILL_R))
			return 0;
	}

	return 1;
}

const struct filldep *
get_filldep(const struct filldepq *fq, const struct strct *s)
{
	const struct filldep	*fill;

	TAILQ_FOREACH(fill, fq, entries)
		if (fill->p == s)
			return fill;

	return NULL;
}
