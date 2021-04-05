/*	$Id$ */
/*
 * Copyright (c) 2021 Kristaps Dzonsons <kristaps@bsd.lv>
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

static int
gen_doc_block(FILE *f, const char *cp, int tail, int head)
{
	size_t	 sz, lines = 0;

	for ( ; *cp != '\0'; cp++)
		if (!isspace((unsigned char)*cp))
			break;

	if (*cp == '\0')
		return 1;

	while (*cp != '\0') {
		if (*cp == '.' || *cp == '"')
			if (fputs("\\&", f) == EOF)
				return 0;
		for (sz = 0; *cp != '\0'; cp++) {
			if (*cp == '\n')
				break;
			if (*cp == '\\' && cp[1] == '"')
				cp++;
			if (head && sz == 0 && lines == 0)
				if (fputs(".Pp\n", f) == EOF)
					return 0;
			if (fputc(*cp, f) == EOF)
				return 0;
			sz++;
		}
		lines += sz > 0 ? 1 : 0;
		if (sz > 0 && fputc('\n', f) == EOF)
			return 0;
		for ( ; *cp != '\0'; cp++)
			if (!isspace((unsigned char)*cp))
				break;
	}

	if (lines && tail && fputs(".Pp\n", f) == EOF)
		return 0;

	return 1;
}

static int
gen_bitem(FILE *f, const struct bitidx *bi, const char *bitf)
{

	if (fprintf(f, ".It Dv BITF_%s_%s, BITI_%s_%s\n", 
	    bitf, bi->name, bitf, bi->name) < 0)
		return 0;
	if (bi->doc != NULL && !gen_doc_block(f, bi->doc, 0, 0))
		return 0;
	return 1;
}

/*
 * Return -1 on failure, 0 if nothing written, 1 if something written.
 */
static int
gen_bitfs(FILE *f, const struct config *cfg)
{
	const struct bitidx	*bi;
	const struct bitf	*b;
	char			*name, *cp;

	if (TAILQ_EMPTY(&cfg->bq))
		return 0;

	if (fprintf(f, 
	    "Bitfields define individual bits within 64-bit integer\n"
	    "values (bits 0\\(en63).\n"
	    "They're used for input validation and value access.\n"
	    "The following bitfields are available:\n"
	    ".Bl -tag -width Ds -offset indent\n") < 0)
		return -1;

	TAILQ_FOREACH(b, &cfg->bq, entries) {
		if (fprintf(f, ".It Vt enum %s\n", b->name) < 0)
			return -1;
		if (b->doc != NULL && !gen_doc_block(f, b->doc, 1, 0))
			return -1;
		if (fprintf(f, ".Bl -tag -width Ds\n") < 0)
			return -1;
		if ((name = strdup(b->name)) == NULL)
			return -1;
		for (cp = name; *cp != '\0'; cp++)
			*cp = toupper((unsigned char)*cp);
		TAILQ_FOREACH(bi, &b->bq, entries)
			if (!gen_bitem(f, bi, name)) {
				free(name);
				return -1;
		}
		free(name);
		if (fprintf(f, ".El\n") < 0)
			return -1;
	}

	return fprintf(f, ".El\n") < 0 ? -1 : 1;
}

static int
gen_eitem(FILE *f, const struct eitem *ei, const char *enm)
{

	if (fprintf(f, ".It Dv %s_%s\n", enm, ei->name) < 0)
		return 0;
	if (ei->doc != NULL && !gen_doc_block(f, ei->doc, 0, 0))
		return 0;
	return 1;
}

/*
 * Return -1 on failure, 0 if nothing written, 1 if something written.
 */
static int
gen_enums(FILE *f, const struct config *cfg)
{
	const struct eitem	*ei;
	const struct enm	*e;
	char			*name, *cp;

	if (TAILQ_EMPTY(&cfg->eq))
		return 0;

	if (fprintf(f, 
	    "Enumerations constrain integer types to a known set\n"
	    "of values.\n"
	    "They're used for input validation and value comparison.\n"
	    "The following enumerations are available.\n"
	    ".Bl -tag -width Ds -offset indent\n") < 0)
		return -1;

	TAILQ_FOREACH(e, &cfg->eq, entries) {
		if (fprintf(f, ".It Vt enum %s\n", e->name) < 0)
			return -1;
		if (e->doc != NULL && !gen_doc_block(f, e->doc, 1, 0))
			return -1;
		if (fprintf(f, ".Bl -tag -width Ds -compact\n") < 0)
			return -1;
		if ((name = strdup(e->name)) == NULL)
			return -1;
		for (cp = name; *cp != '\0'; cp++)
			*cp = toupper((unsigned char)*cp);
		TAILQ_FOREACH(ei, &e->eq, entries)
			if (!gen_eitem(f, ei, name)) {
				free(name);
				return -1;
		}
		free(name);
		if (fprintf(f, ".El\n") < 0)
			return -1;
	}

	return fprintf(f, ".El\n") < 0 ? -1 : 1;
}

/*
 * Return -1 on failure, 0 if nothing written, 1 if something written.
 */
static int
gen_roles(FILE *f, const struct config *cfg)
{
	const struct role	*r;

	if (TAILQ_EMPTY(&cfg->rq))
		return 0;

	if (fprintf(f, 
	    "Roles define which operations and data are available to\n"
	    "running application and are set with\n"
	    ".Fn db_role .\n"
	    "It accepts one of the following roles:\n"
	    ".Pp\n"
	    ".Vt enum ort_role\n"
	    ".Bl -tag -width Ds -compact -offset indent\n") < 0)
		return -1;

	TAILQ_FOREACH(r, &cfg->arq, allentries) {
		if (fprintf(f, ".It Dv ROLE_%s\n", r->name) < 0)
			return -1;
		if (r->doc != NULL && !gen_doc_block(f, r->doc, 0, 0))
			return -1;
	}

	return fprintf(f, ".El\n") < 0 ? -1 : 1;
}

static int
gen_field(FILE *f, const struct field *fd)
{
	int	 c = 0;

	if (fprintf(f, ".It Va ") < 0)
		return 0;

	switch (fd->type) {
	case FTYPE_STRUCT:
		c = fprintf(f, "struct %s %s\n", 
			fd->ref->target->parent->name, fd->name);
		break;
	case FTYPE_REAL:
		c = fprintf(f, "double %s\n", fd->name);
		break;
	case FTYPE_BLOB:
		if (fprintf(f, "void *%s\n", fd->name) < 0)
			return 0;
		c = fprintf(f, ".It Va size_t %s_sz\n", fd->name);
		break;
	case FTYPE_DATE:
	case FTYPE_EPOCH:
		c = fprintf(f, "time_t %s\n", fd->name);
		break;
	case FTYPE_BIT:
	case FTYPE_BITFIELD:
	case FTYPE_INT:
		c = fprintf(f, "int64_t %s\n", fd->name);
		break;
	case FTYPE_TEXT:
	case FTYPE_EMAIL:
	case FTYPE_PASSWORD:
		c = fprintf(f, "char *%s\n", fd->name);
		break;
	case FTYPE_ENUM:
		c = fprintf(f, "enum %s %s\n", 
			fd->enm->name, fd->name);
		break;
	default:
		break;
	}

	if (c < 0)
		return 0;
	if (fd->doc != NULL && !gen_doc_block(f, fd->doc, 0, 0))
		return 0;
	return 1;
}

static int
gen_fields(FILE *f, const struct strct *s)
{
	const struct field	*fd;

	if (fprintf(f,
	    ".Bl -tag -width Ds -compact\n") < 0)
		return 0;
	TAILQ_FOREACH(fd, &s->fq, entries)
		if (!gen_field(f, fd))
			return 0;
	return fprintf(f, ".El\n") >= 0;
}

/*
 * Return -1 on failure, 0 if nothing written, 1 if something written.
 */
static int
gen_strcts(FILE *f, const struct config *cfg)
{
	const struct strct	*s;

	if (TAILQ_EMPTY(&cfg->sq))
		return 0;

	if (fprintf(f, 
	    "Structures are the mainstay of the application.\n"
	    "They correspond to tables in the database.\n"
	    "The following structures are available:\n"
	    ".Bl -tag -width Ds -offset indent\n") < 0)
		return -1;

	TAILQ_FOREACH(s, &cfg->sq, entries) {
		if (fprintf(f, ".It Vt struct %s\n", s->name) < 0)
			return -1;
		if (s->doc != NULL && !gen_doc_block(f, s->doc, 1, 0))
			return -1;
		if (!gen_fields(f, s))
			return -1;
	}

	return fprintf(f, ".El\n") < 0 ? -1 : 1;
}

static int
gen_search(FILE *f, const struct search *sr)
{
	const char		*retname;
	const struct sent	*sent;
	int		 	 c, hasunary = 0;

	if (fputs(".It Ft \"", f) == EOF)
		return 0;

	retname = sr->dst != NULL ? 
		sr->dst->strct->name : sr->parent->name;
	if (sr->type == STYPE_COUNT)
		c = fprintf(f, "uint64_t");
	else if (sr->type == STYPE_SEARCH)
		c = fprintf(f, "struct %s *", retname);
	else if (sr->type == STYPE_LIST)
		c = fprintf(f, "struct %s_q *", retname);
	else
		c = fprintf(f, "void");
	if (c < 0)
		return 0;

	if (fprintf(f, "\" Fn db_%s_%s", 
	    sr->parent->name, get_stype_str(sr->type)) < 0)
		return 0;

	if (sr->name == NULL && !TAILQ_EMPTY(&sr->sntq)) {
		if (fputs("_by", f) == EOF)
			return 0;
		TAILQ_FOREACH(sent, &sr->sntq, entries)
			if (fprintf(f, "_%s_%s", sent->uname,
			    get_optype_str(sent->op)) < 0)
				return 0;
	} else if (sr->name != NULL)
		if (fprintf(f, "_%s", sr->name) < 0)
			return 0;

	if (fputs(
	    "\n"
	    ".TS\n"
	    "lw6 l l.\n"
            "-\t\\fIstruct ort *\\fR\t\\fIctx\\fR\n", f) == EOF)
		return 0;

	if (sr->type == STYPE_ITERATE && fprintf(f, 
	    "-\t\\fI%s_cb\\fR\t\\fIcb\\fR\n"
	    "-\t\\fIvoid *\\fR\t\\fIarg\\fR\n", retname) < 0)
		return 0;

	TAILQ_FOREACH(sent, &sr->sntq, entries) {
		if (OPTYPE_ISUNARY(sent->op)) {
			hasunary = 1;
			continue;
		}
		if (sent->field->type == FTYPE_BLOB)
			if (fprintf(f, 
			    "-\t\\fIsize_t\\fR\t\\fI%s\\fR (size)\n", 
			    sent->field->name) < 0)
				return 0;
		if (fprintf(f, "%s\t", get_optype_str(sent->op)) < 0)
			return 0;
		if (fputs("\\fI", f) == EOF)
			return 0;
		if (sent->field->type == FTYPE_ENUM)
			c = fprintf(f, "enum %s", 
				sent->field->enm->name);
		else 
			c = fprintf(f, "%s%s", 
				get_ftype_str(sent->field->type), 
				(sent->field->flags & FIELD_NULL) ?
				"*" : "");
		if (c < 0)
			return 0;
		if (fprintf(f, "\\fR\t\\fI%s\\fR\n", 
		    sent->field->name) < 0)
			return 0;

	}

	if (hasunary) {
		if (fputs(".TE\n", f) == EOF)
			return 0;
		if (fputs(".Pp\n"
		    "Unary operations:\n"
		    ".Pp\n"
		    ".TS\n"
		    "lw6 lw12 l.\n", f) == EOF)
			return 0;
		TAILQ_FOREACH(sent, &sr->sntq, entries) {
			if (!OPTYPE_ISUNARY(sent->op))
				continue;
			if (fprintf(f, "%s\t",
			    get_optype_str(sent->op)) < 0)
				return 0;
			if (fputs("\\fI", f) == EOF)
				return 0;
			if (sent->field->type == FTYPE_ENUM)
				c = fprintf(f, "enum %s", 
					sent->field->enm->name);
			else 
				c = fprintf(f, "%s", get_ftype_str
					(sent->field->type));
			if (c < 0)
				return 0;
			if (fprintf(f, "\\fR\t\\fI%s\\fR\n", 
			    sent->field->name) < 0)
				return 0;
		}
	}

	if (fputs(".TE\n", f) == EOF)
		return 0;
	if (sr->doc != NULL && !gen_doc_block(f, sr->doc, 0, 1))
		return 0;
	return 1;

}

/*
 * Return -1 on failure, 0 if nothing written, 1 if something written.
 */
static int
gen_searches(FILE *f, const struct config *cfg)
{
	const struct strct	*s;
	const struct search	*sr;

	TAILQ_FOREACH(s, &cfg->sq, entries)
		if (!TAILQ_EMPTY(&s->sq))
			break;
	if (s == NULL)
		return 0;

	if (fputs
	    ("The following queries are available,\n"
	     "which allow accepted roles to extract\n"
	     "data from the database:\n"
	     ".Bl -tag -width Ds -offset indent\n", f) == EOF)
		return -1;

	TAILQ_FOREACH(s, &cfg->sq, entries)
		TAILQ_FOREACH(sr, &s->sq, entries)
			if (!gen_search(f, sr))
				return -1;

	return fputs(".El\n", f) == EOF ? -1 : 1;
}

static int
gen_update(FILE *f, const struct update *up)
{
	const struct uref	*ur;
	const char		*rettype, *functype;
	int			 c, hasunary = 0;

	rettype = up->type == UP_MODIFY ? "int" : "void";
	functype = up->type == UP_MODIFY ? "update" : "delete";

	if (fprintf(f, ".It Ft %s Fn db_%s_%s",
	    rettype, up->parent->name, functype) < 0)
		return 0;

	if (up->name == NULL && up->type == UP_MODIFY) {
		if (!(up->flags & UPDATE_ALL))
			TAILQ_FOREACH(ur, &up->mrq, entries)
				if (fprintf(f, "_%s_%s", 
				    ur->field->name, 
				    get_modtype_str(ur->mod)) < 0)
					return 0;
		if (!TAILQ_EMPTY(&up->crq)) {
			if (fputs("_by", f) == EOF)
				return 0;
			TAILQ_FOREACH(ur, &up->crq, entries)
				if (fprintf(f, "_%s_%s", 
				    ur->field->name, 
				    get_optype_str(ur->op)) < 0)
					return 0;
		}
	} else if (up->name == NULL) {
		if (!TAILQ_EMPTY(&up->crq)) {
			if (fputs("_by", f) == EOF)
				return 0;
			TAILQ_FOREACH(ur, &up->crq, entries)
				if (fprintf(f, "_%s_%s", 
				    ur->field->name, 
				    get_optype_str(ur->op)) < 0)
					return 0;
		}
	} else if (fprintf(f, "_%s", up->name) < 0)
		return 0;

	if (fputs(
	    "\n"
	    ".TS\n"
	    "l lw6 l l.\n"
	    "-\t-\t\\fIstruct ort *\\fR\t\\fIctx\\fR\n", f) == EOF)
		return 0;

	TAILQ_FOREACH(ur, &up->mrq, entries) {
		if (ur->field->type == FTYPE_BLOB)
			if (fprintf(f, "\\(<-\t-\t"
			    "\\fIsize_t\\fR\t\\fI%s\\fR (size)\n", 
			    ur->field->name) < 0)
				return 0;
		if (fprintf(f, "\\(<-\t%s\t",
		    get_modtype_str(ur->mod)) < 0)
			return 0;
		if (fputs("\\fI", f) == EOF)
			return 0;
		if (ur->field->type == FTYPE_ENUM)
			c = fprintf(f, "enum %s", 
				ur->field->enm->name);
		else 
			c = fprintf(f, "%s%s", 
				get_ftype_str(ur->field->type), 
				(ur->field->flags & FIELD_NULL) ?
				"*" : "");
		if (c < 0)
			return 0;
		if (fprintf(f, "\\fR\t\\fI%s\\fR\n", 
		    ur->field->name) < 0)
			return 0;

	}

	TAILQ_FOREACH(ur, &up->crq, entries) {
		if (OPTYPE_ISUNARY(ur->op)) {
			hasunary = 1;
			continue;
		}
		if (ur->field->type == FTYPE_BLOB)
			if (fprintf(f, "\\(->\t-\t"
			    "\\fIsize_t\\fR\t\\fI%s\\fR (size)\n", 
			    ur->field->name) < 0)
				return 0;
		if (fprintf(f, "\\(->\t%s\t",
		    get_optype_str(ur->op)) < 0)
			return 0;
		if (fputs("\\fI", f) == EOF)
			return 0;
		if (ur->field->type == FTYPE_ENUM)
			c = fprintf(f, "enum %s", 
				ur->field->enm->name);
		else 
			c = fprintf(f, "%s%s", 
				get_ftype_str(ur->field->type), 
				(ur->field->flags & FIELD_NULL) ?
				"*" : "");
		if (c < 0)
			return 0;
		if (fprintf(f, "\\fR\t\\fI%s\\fR\n", 
		    ur->field->name) < 0)
			return 0;

	}

	if (hasunary) {
		if (fputs(".TE\n", f) == EOF)
			return 0;
		if (fputs(".Pp\n"
		    "Unary operations:\n"
		    ".Pp\n"
		    ".TS\n"
		    "l lw6 l l.\n", f) == EOF)
			return 0;
		TAILQ_FOREACH(ur, &up->crq, entries) {
			if (!OPTYPE_ISUNARY(ur->op))
				continue;
			if (fprintf(f, "\\(->\t%s\t",
			    get_optype_str(ur->op)) < 0)
				return 0;
			if (fputs("\\fI", f) == EOF)
				return 0;
			if (ur->field->type == FTYPE_ENUM)
				c = fprintf(f, "enum %s", 
					ur->field->enm->name);
			else 
				c = fprintf(f, "%s", get_ftype_str
					(ur->field->type));
			if (c < 0)
				return 0;
			if (fprintf(f, "\\fR\t\\fI%s\\fR\n", 
			    ur->field->name) < 0)
				return 0;
		}
	}

	if (fputs(".TE\n", f) == EOF)
		return 0;
	if (up->doc != NULL && !gen_doc_block(f, up->doc, 0, 1))
		return 0;
	return 1;
}

static int
gen_insert(FILE *f, const struct strct *s)
{
	const struct field	*fd;
	int			 c;

	if (fprintf(f, ".It Ft int64_t Fn db_%s_insert\n", s->name) < 0)
		return 0;

	if (fputs(".TS\nl l.\n", f) == EOF)
		return 0;
	if (fputs("\\fIstruct ort *\\fR\t\\fIctx\\fR\n", f) == EOF)
		return 0;

	TAILQ_FOREACH(fd, &s->fq, entries) {
		if (fd->type == FTYPE_STRUCT || 
		    (fd->flags & FIELD_ROWID))
			continue;
		if (fd->type == FTYPE_BLOB)
			if (fprintf(f,
			    "\\fIsize_t\\fR\t\\fI%s\\fR (size)\n", 
			    fd->name) < 0)
				return 0;
		if (fputs("\\fI", f) == EOF)
			return 0;
		if (fd->type == FTYPE_ENUM)
			c = fprintf(f, "enum %s", fd->enm->name);
		else 
			c = fprintf(f, "%s%s", get_ftype_str(fd->type), 
				(fd->flags & FIELD_NULL) ?  "*" : "");
		if (c < 0)
			return 0;
		if (fprintf(f, "\\fR\t\\fI%s\\fR\n", fd->name) < 0)
			return 0;

	}

	return fputs(".TE\n", f) != EOF;

}

/*
 * Return -1 on failure, 0 if nothing written, 1 if something written.
 */
static int
gen_deletes(FILE *f, const struct config *cfg)
{
	const struct strct	*s;
	const struct update	*up;

	TAILQ_FOREACH(s, &cfg->sq, entries)
		if (!TAILQ_EMPTY(&s->dq))
			break;
	if (s == NULL)
		return 0;

	if (fputs
	    ("Deletes allow for accepted roles to\n"
	     "delete data from the database.\n"
	     "The following deletes are available:\n"
	     ".Bl -tag -width Ds -offset indent\n", f) == EOF)
		return -1;

	TAILQ_FOREACH(s, &cfg->sq, entries)
		TAILQ_FOREACH(up, &s->dq, entries)
			if (!gen_update(f, up))
				return -1;

	return fputs(".El\n", f) == EOF ? -1 : 1;
}

/*
 * Return -1 on failure, 0 if nothing written, 1 if something written.
 */
static int
gen_updates(FILE *f, const struct config *cfg)
{
	const struct strct	*s;
	const struct update	*up;

	TAILQ_FOREACH(s, &cfg->sq, entries)
		if (!TAILQ_EMPTY(&s->uq))
			break;
	if (s == NULL)
		return 0;

	if (fputs
	    ("Updates allow for accepted roles to\n"
	     "modify data in the database.\n"
	     "The following updates are available:\n"
	     ".Bl -tag -width Ds -offset indent\n", f) == EOF)
		return -1;

	TAILQ_FOREACH(s, &cfg->sq, entries)
		TAILQ_FOREACH(up, &s->uq, entries)
			if (!gen_update(f, up))
				return -1;

	return fputs(".El\n", f) == EOF ? -1 : 1;
}

/*
 * Return -1 on failure, 0 if nothing written, 1 if something written.
 */
static int
gen_inserts(FILE *f, const struct config *cfg)
{
	const struct strct	*s;

	TAILQ_FOREACH(s, &cfg->sq, entries)
		if (s->ins != NULL)
			break;
	if (s == NULL)
		return 0;

	if (fputs
	    ("Inserts allow accepted roles to add\n"
	     "new data to the database.\n"
	     "The following inserts are available:\n"
	     ".Bl -tag -width Ds -offset indent\n", f) == EOF)
		return -1;

	TAILQ_FOREACH(s, &cfg->sq, entries) 
		if (s->ins != NULL && !gen_insert(f, s))
			return -1;

	return fputs(".El\n", f) == EOF ? -1 : 1;
}

static int
gen_json_input(FILE *f, const struct strct *s)
{

	return fprintf(f,
		".It Ft int Fn jsmn_%s\n"
		".TS\n"
		"l l.\n"
		"\\fIstruct %s *\\fR\t\\fIp\\fR\n"
		"\\fIconst char *\\fR\t\\fIbuf\\fR\n"
		"\\fIconst jsmntok_t *\\fR\t\\fItoks\\fR\n"
		"\\fIsize_t\\fR\t\\fItoksz\\fR\n"
		".TE\n"
		".It Ft int Fn jsmn_%s_array\n"
		".TS\n"
		"l l.\n"
		"\\fIstruct %s **\\fR\t\\fIp\\fR\n"
		"\\fIsize_t *\\fR\t\\fIpsz\\fR\n"
		"\\fIconst char *\\fR\t\\fIbuf\\fR\n"
		"\\fIconst jsmntok_t *\\fR\t\\fItoks\\fR\n"
		"\\fIsize_t\\fR\t\\fItoksz\\fR\n"
		".TE\n"
		".It Ft int Fn jsmn_%s_clear\n"
		".TS\n"
		"l l.\n"
		"\\fIstruct %s *\\fR\t\\fIp\\fR\n"
		".TE\n"
		".It Ft int Fn jsmn_%s_free_array\n"
		".TS\n"
		"l l.\n"
		"\\fIstruct %s *\\fR\t\\fIp\\fR\n"
		"\\fIsize_t\\fR\t\\fIpsz\\fR\n"
		".TE\n", 
		s->name, s->name, s->name, s->name, s->name,
		s->name, s->name, s->name) >= 0;
}

static int
gen_json_output(FILE *f, const struct strct *s)
{

	if (fprintf(f,
	    ".It Ft void Fn json_%s_data , Fn json_%s_obj\n"
	    ".TS\n"
	    "l l.\n"
	    "\\fIstruct kjsonreq *\\fR\t\\fIr\\fR\n"
	    "\\fIconst struct %s *\\fR\t\\fIp\\fR\n"
	    ".TE\n", s->name, s->name, s->name) < 0)
		return 0;
	if ((s->flags & STRCT_HAS_QUEUE) && fprintf(f,
	    ".It Ft void Fn json_%s_array\n"
	    ".TS\n"
	    "l l.\n"
	    "\\fIstruct kjsonreq *\\fR\t\\fIr\\fR\n"
	    "\\fIconst struct %s_q *\\fR\t\\fIq\\fR\n"
	    ".TE\n", s->name, s->name) < 0)
		return 0;
	if ((s->flags & STRCT_HAS_ITERATOR) && fprintf(f,
	    ".It Ft void Fn json_%s_iterate\n"
	    ".TS\n"
	    "l l.\n"
	    "\\fIconst struct %s *\\fR\t\\fIp\\fR\n"
	    "\\fIvoid *\\fR\t\\fIarg\\fR (really \\fIstruct kjsonreq *\\fR)\n"
	    ".TE\n", s->name, s->name) < 0)
		return 0;
	return 1;
}

/*
 * Return -1 on failure, 0 if nothing written, 1 if something written.
 */
static int
gen_json_outputs(FILE *f, const struct config *cfg)
{
	const struct strct	*s;

	if (TAILQ_EMPTY(&cfg->sq))
		return 0;

	if (fputs
	    ("These print out the fields of a structure in JSON "
	     "including nested structures.\n"
	     "They omit any passwords, those marked \"noexport\",\n"
	     "or those disallowed by the current role.\n"
	     ".Bl -tag -width Ds -offset indent\n", f) == EOF)
		return 0;

	TAILQ_FOREACH(s, &cfg->sq, entries)
		if (!gen_json_output(f, s))
			return -1;

	return fputs(".El\n", f) != EOF;
}

/*
 * Return -1 on failure, 0 if nothing written, 1 if something written.
 */
static int
gen_json_inputs(FILE *f, const struct config *cfg)
{
	const struct strct	*s;

	if (TAILQ_EMPTY(&cfg->sq))
		return 0;

	if (fputs
	    ("Allow for JSON objects and arrays, such as\n"
	     "those produced by the JSON export functions\n"
	     "(if defined), to be re-imported.\n"
	     "These deserialise parsed JSON buffers\n"
	     ".Fa buf ,\n"
	     "which need not be NUL terminated, with parse\n"
	     "tokens\n"
	     ".Fa toks\n"
	     "of length\n"
	     ".Fa toksz ,\n"
	     "into\n"
	     ".Fa p ,\n"
	     "for arrays of count\n"
	     ".Fa psz .\n"
	     "They return 0 on parse failure, <0 on memory\n"
	     "allocation failure, or the count of tokens\n"
	     "parsed on success.\n"
	     ".Bl -tag -width Ds -offset indent\n", f) == EOF)
		return 0;

	/* Start with the utility functions. */

	if (fputs
	    (".It Ft void Fn jsmn_init\n"
	     ".TS\n"
	     "l l.\n"
	     "\\fIjsmn_parser *\\fR\t\\fIp\\fR\n"
	     ".TE\n"
	     ".Pp\n"
	     "Initialise a parser\n"
	     ".Fa p\n"
	     "for use in\n"
	     ".Fn jsmn_parse .\n", f) == EOF)
		return 0;
	if (fputs
	    (".It Ft int Fn jsmn_parse\n"
	     ".TS\n"
	     "l l.\n"
	     "\\fIjsmn_parser *\\fR\t\\fIp\\fR\n"
	     "\\fIconst char *\\fR\t\\fIbuf\\fR\n"
	     "\\fIsize_t\\fR\t\\fIsz\\fR\n"
	     "\\fIjsmntok_t *\\fR\t\\fItoks\\fR\n"
	     "\\fIjunsigned int\\fR\t\\fItoksz\\fR\n"
	     ".TE\n"
	     ".Pp\n"
	     "Parse a buffer\n"
	     ".Fa buf\n"
	     "of length\n"
	     ".Fa bufsz\n"
	     "with the parser\n"
	     ".Fa p .\n"
	     "Returns the number of tokens parsed or less than zero\n"
	     "on failure.\n"
	     "If\n"
	     ".Fa toks\n"
	     "is\n"
	     ".Dv NULL ,\n"
	     "simply returns the number of tokens without parsing.\n"
	     "In this case,\n"
	     ".Fa toksz\n"
	     "is ignored.\n", f) == EOF)
		return 0;
	if (fputs
	    (".It Ft int Fn jsmn_eq\n"
	     ".TS\n"
	     "l l.\n"
	     "\\fIconst char *\\fR\t\\fIjson\\fR\n"
	     "\\fIconst jsmntok_t *\\fR\t\\fItok\\fR\n"
	     "\\fIconst char *\\fR\t\\fIs\\fR\n"
	     ".TE\n"
	     ".Pp\n"
	     "Check whether the current token in a parse sequence\n"
	     ".Fa tok\n"
	     "parsed from\n"
	     ".Fa json\n"
	     "is equal to a string\n"
	     ".Fa s .\n"
	     "Used when checking for key equality.\n", f) == EOF)
		return 0;

	TAILQ_FOREACH(s, &cfg->sq, entries)
		if (!gen_json_input(f, s))
			return -1;

	return fputs(".El\n", f) == EOF ? -1 : 1;
}

int
ort_lang_c_manpage(const struct ort_lang_c *args,
	const struct config *cfg, FILE *f)
{
	int	 		 c;
	struct ort_lang_c	 nargs;

	memset(&nargs, 0, sizeof(struct ort_lang_c));

	if (args == NULL)
		args = &nargs;

	if (fprintf(f, 
	    ".\\\" WARNING: automatically generated by ort-%s.\n"
	    ".\\\" DO NOT EDIT!\n", VERSION) < 0)
		return 0;

	if (fprintf(f,
	    ".Dd $" "Mdocdate" "$\n"
	    ".Dt ORT 3\n"
	    ".Os\n"
	    ".Sh NAME\n"
	    ".Nm ort\n"
	    ".Nd C API for your openradtool data model\n"
	    ".Sh DESCRIPTION\n"
	    ".Ss Data structures\n") < 0)
		return 0;

	if ((c = gen_roles(f, cfg)) < 0)
		return 0;
	else if (c > 0 && fputs(".Pp\n", f) == EOF)
		return 0;
	if ((c = gen_enums(f, cfg)) < 0)
		return 0;
	else if (c > 0 && fputs(".Pp\n", f) == EOF)
		return 0;
	if ((c = gen_bitfs(f, cfg)) < 0)
		return 0;
	else if (c > 0 && fputs(".Pp\n", f) == EOF)
		return 0;
	if (gen_strcts(f, cfg) < 0)
		return 0;
	if (fputs(".Ss Database input\n", f) == EOF)
		return 0;
	if ((c = gen_searches(f, cfg)) < 0)
		return 0;
	else if (c > 0 && fputs(".Pp\n", f) == EOF)
		return 0;
	if ((c = gen_updates(f, cfg)) < 0)
		return 0;
	else if (c > 0 && fputs(".Pp\n", f) == EOF)
		return 0;
	if ((c = gen_deletes(f, cfg)) < 0)
		return 0;
	else if (c > 0 && fputs(".Pp\n", f) == EOF)
		return 0;
	if (gen_inserts(f, cfg) < 0)
		return 0;

	if (args->flags & ORT_LANG_C_JSON_JSMN) {
		if (fputs(".Ss JSON input\n", f) == EOF)
			return 0;
		if (gen_json_inputs(f, cfg) < 0)
			return 0;
	}

	if (args->flags & ORT_LANG_C_JSON_KCGI) {
		if (fputs(".Ss JSON output\n", f) == EOF)
			return 0;
		if (gen_json_outputs(f, cfg) < 0)
			return 0;
	}

	return 1;
}

