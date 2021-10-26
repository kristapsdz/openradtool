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

#include <assert.h>
#include <ctype.h>
#include <inttypes.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ort.h"
#include "ort-lang-c.h"
#include "ort-version.h"
#include "lang.h"
#include "lang-c.h"

static int
gen_field_type(FILE *f, const struct field *fd)
{
	const struct field	*rfd;
	int			 c;

	switch (fd->type) {
	case FTYPE_ENUM:
		assert(fd->enm != NULL);
		c = fprintf(f, "enum %s", fd->enm->name);
		break;
	case FTYPE_BIT:
	case FTYPE_BITFIELD:
	case FTYPE_DATE:
	case FTYPE_EPOCH:
	case FTYPE_INT:
		rfd = fd->ref != NULL ? fd->ref->target : fd;
		c = fprintf(f, "%s_%s", rfd->parent->name, rfd->name);
		break;
	default:
		c = fprintf(f, "%s", get_ftype_str(fd->type));
		break;
	}

	if (c < 0)
		return 0;
	if ((fd->flags & FIELD_NULL) && fputc('*', f) == EOF)
		return 0;

	return 1;
}

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
	    ".Bl -tag -width Ds\n") < 0)
		return -1;

	TAILQ_FOREACH(b, &cfg->bq, entries) {
		if (fprintf(f, ".It Vt enum %s\n", b->name) < 0)
			return -1;
		if (b->doc != NULL && !gen_doc_block(f, b->doc, 1, 0))
			return -1;
		if (fprintf(f, ".Bl -tag -width Ds -compact\n") < 0)
			return -1;
		if ((name = strdup(b->name)) == NULL)
			return -1;
		for (cp = name; *cp != '\0'; cp++)
			*cp = (char)toupper((unsigned char)*cp);
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
	    ".Bl -tag -width Ds\n") < 0)
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
			*cp = (char)toupper((unsigned char)*cp);
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

/*
 * Return FALSE on failure, TRUE on success.
 */
static int
gen_role_r(FILE *f, const struct role *r, int *first)
{
	const struct role	*rr;

	if (strcmp(r->name, "all")) {
		if (fprintf(f, "%s\n.Dv ROLE_%s",
		    *first ? "" : " ,", r->name) < 0)
			return 0;
		*first = 0;
	}

	TAILQ_FOREACH(rr, &r->subrq, entries)
		if (!gen_role_r(f, rr, first))
			return 0;
	return 1;
}

/*
 * Return FALSE on failure, TRUE on success.
 */
static int
gen_rolemap(FILE *f, const struct rolemap *map)
{
	const struct rref	*rr;
	int			 first = 1;

	TAILQ_FOREACH(rr, &map->rq, entries)
		if (!gen_role_r(f, rr->role, &first))
			return 0;
	return 1;
}


static int
gen_field(FILE *f, const struct field *fd)
{
	int	 	 	 c = 0;
	const struct field	*rfd;

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
	case FTYPE_BIT:
	case FTYPE_BITFIELD:
	case FTYPE_DATE:
	case FTYPE_EPOCH:
	case FTYPE_INT:
		rfd = fd->ref != NULL ? fd->ref->target : fd;
		c = fprintf(f, "%s_%s %s\n", 
			rfd->parent->name, rfd->name, fd->name);
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
	    ".Bl -tag -width Ds\n") < 0)
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

/*
 * Return FALSE on failure, TRUE on success.
 */
static int
gen_query(FILE *f, const struct search *sr, int syn)
{
	const char		*retname;
	const struct sent	*sent;
	int		 	 c;

	if (syn && fputs(".Ft \"", f) == EOF)
		return 0;
	if (!syn && fputs(".It Ft \"", f) == EOF)
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

	if (syn && fprintf(f, "\"\n"
	    ".Fo db_%s_%s",
	    sr->parent->name, get_stype_str(sr->type)) < 0)
		return 0;
	if (!syn && fprintf(f, "\" Fn db_%s_%s",
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

	if (syn && fputs(
            "\n"
	    ".Fa \"struct ort *ort\"\n", f) == EOF)
		return 0;
	if (!syn && fputs(
	    "\n"
	    ".TS\n"
	    "l l l.\n"
            "-\tort\tstruct ort *\n", f) == EOF)
		return 0;

	if (sr->type == STYPE_ITERATE) {
		if (syn && fprintf(f,
		    ".Fa \"%s_cb cb\"\n"
		    ".Fa \"void *arg\"\n", retname) < 0)
			return 0;
		if (!syn && fprintf(f, 
		    "-\tcb\t%s_cb\n"
		    "-\targ\tvoid *\n", retname) < 0)
			return 0;
	}

	TAILQ_FOREACH(sent, &sr->sntq, entries) {
		if (syn) {
			if (OPTYPE_ISUNARY(sent->op))
				continue;
			if (sent->field->type == FTYPE_BLOB)
				if (fprintf(f,
				    ".Fa \"size_t %s_sz\n",
				    sent->field->name) < 0)
					return 0;
			if (fputs(".Fa \"", f) == EOF)
				return 0;
			if (!gen_field_type(f, sent->field))
				return 0;
			if (fprintf(f, " %s\"\n",
			    sent->field->name) < 0)
				return 0;
			continue;
		}
		if (sent->field->type == FTYPE_BLOB &&
		    !OPTYPE_ISUNARY(sent->op))
			if (fprintf(f, "-\t(%s size)\tsize_t\n", 
			    sent->field->name) < 0)
				return 0;
		if (fprintf(f, "%s\t", get_optype_str(sent->op)) < 0)
			return 0;
		if (fprintf(f, "%s\t", sent->field->name) < 0)
			return 0;
		if (!gen_field_type(f, sent->field))
			return 0;
		if (fputc('\n', f) == EOF)
			return 0;
	}

	if (syn) {
		if (fputs(".Fc\n", f) == EOF)
			return 0;
	} else {
		if (fputs(".TE\n", f) == EOF)
			return 0;
		if (sr->doc != NULL && !gen_doc_block(f, sr->doc, 0, 1))
			return 0;
		if (sr->rolemap) {
			if (fputs(
			    ".Pp\n"
			    "Only allowed to the following:", f) == EOF)
				return 0;
			if (!gen_rolemap(f, sr->rolemap))
				return 0;
			if (fputs(" .\n", f) == EOF)
				return 0;
		}
	}
	return 1;
}

/*
 * Return FALSE on failure, TRUE on success.
 */
static int
gen_general(FILE *f, const struct config *cfg, int syn)
{

	if (syn == 1) {
		if (fputs(
		    ".Ft \"struct ort *\"\n"
		    ".Fo db_open_logging\n"
		    ".Fa \"const char *file\"\n"
		    ".Fa \"(void *log)(const char *, void *)\"\n"
		    ".Fa \"(void *log_short)(const char *, ...)\"\n"
		    ".Fa \"void *arg\"\n"
		    ".Fc\n", f) == EOF)
			return 0;
		if (fputs(
		    ".Ft \"struct ort *\"\n"
		    ".Fo db_open\n"
		    ".Fa \"const char *file\"\n"
		    ".Fc\n", f) == EOF)
			return 0;
	    	if (fputs(
		    ".Ft void\n"
		    ".Fo db_logging_data\n"
		    ".Fa \"struct ort *ort\"\n"
		    ".Fa \"const void *arg\"\n"
		    ".Fa \"size_t argsz\"\n"
		    ".Fc\n", f) == EOF)
			return 0;
		if (fputs(
		    ".Ft void\n"
		    ".Fo db_close\n"
		    ".Fa \"struct ort *ort\"\n"
		    ".Fc\n", f) == EOF)
			return 0;
		return 1;
	}

	if (fputs(
	    ".Ss Database management\n"
	    "Allow opening, closing, and manipulating databases "
	    "(roles, logging, etc.).\n"
	    ".Bl -tag -width Ds\n", f) == EOF)
		return 0;
	if (fputs(
	    ".It Ft \"struct ort *\" Fn db_open_logging\n"
	    ".TS\n"
	    "l l.\n"
	    "file\tconst char\t*\n"
	    "log\t(void *)(const char *, void *)\n"
	    "log_short\t(void *)(const char *, ...)\n"
	    "void *\targ\n"
	    ".TE\n"
	    ".Pp\n"
	    "Open a database\n"
	    ".Fa file\n"
	    "in a child process with logging enabled.\n"
	    "Returns\n"
	    ".Dv NULL\n"
	    "on failure.\n"
	    "If both callbacks are provided,\n"
	    ".Fa log\n"
	    "overrides\n"
	    ".Fa log_short .\n"
	    "The logging function is run both in a child and\n"
	    "parent process, so it must not have side effects.\n"
	    ".Fa arg\n"
	    "is passed to\n"
	    ".Fa log\n"
	    "as it is inherited by the child process.\n"
	    "The context must be closed by\n"
	    ".Fn db_close .\n"
	    "See\n"
	    ".Fn db_logging_data\n"
	    "to set the pointer after initialisation.\n", f) == EOF)
		return 0;
	if (fputs(
	    ".It Ft \"struct ort *\" Fn db_open\n"
	    ".TS\n"
	    "l l.\n"
	    "file\tconst char *\n"
	    ".TE\n"
	    ".Pp\n"
	    "Like\n"
	    ".Fn db_open_logging\n"
	    "but without logging enabled.\n", f) == EOF)
		return 0;
	if (fputs(
	    ".It Ft void Fn db_logging_data\n"
	    ".TS\n"
	    "l l.\n"
	    "ort\tstruct ort *\n"
	    "arg\tconst void *\n"
	    "argsz\tsize_t\n"
	    ".TE\n"
	    ".Pp\n"
	    "Sets the argument giving to the logging functions (if\n"
	    "enabled) to the contents of\n"
	    ".Fa arg ,\n"
	    "of length\n"
	    ".Fa argsz ,\n"
	    "which is copied into the child process.\n"
	    "Has no effect if logging is not enabled.\n"
	    "If\n"
	    ".Fa argsz\n"
	    "is zero, nothing is passed to the logger.\n", f) == EOF)
		return 0;
	if (fputs(
	    ".It Ft void Fn db_close\n"
	    ".TS\n"
	    "l l.\n"
	    "ort\tstruct ort *\n"
	    ".TE\n"
	    ".Pp\n"
	    "Close a database opened with\n"
	    ".Fn db_open\n"
	    "or\n"
	    ".Fn db_open_logging .\n"
	    "Does nothing if\n"
	    ".Fa ort\n"
	    "is\n"
	    ".Dv NULL .\n", f) == EOF)
		return 0;

	return fputs(".El\n.Pp\n", f) != EOF;
}

/*
 * Return FALSE on failure, TRUE on success.
 */
static int
gen_queries(FILE *f, const struct config *cfg, int syn)
{
	const struct strct	*s;
	const struct search	*sr;

	TAILQ_FOREACH(s, &cfg->sq, entries)
		if (!TAILQ_EMPTY(&s->sq))
			break;

	if (s == NULL)
		return 1;

	if (!syn && fputs(
	    ".Ss Database queries\n"
	    "Queries extract data from the database.\n"
	    "They either return an individual structures\n"
	    ".Pq Qq get ,\n"
	    "iterate over a set of structures\n"
	    ".Pq Qq iterate ,\n"
	    "return a list of structures\n"
	    ".Pq Qq list ,\n"
	    "or return a count of matched structures\n"
	    ".Pq Qq count .\n"
	    ".Bl -tag -width Ds\n", f) == EOF)
		return 0;

	TAILQ_FOREACH(s, &cfg->sq, entries)
		TAILQ_FOREACH(sr, &s->sq, entries)
			if (!gen_query(f, sr, syn))
				return 0;
	
	if (!syn && fputs(".El\n.Pp\n", f) == EOF)
		return 0;
	return 1;
}

/*
 * Return FALSE on failure, TRUE on success.
 */
static int
gen_update(FILE *f, const struct update *up, int syn)
{
	const struct uref	*ur;
	const char		*rettype, *functype;

	rettype = up->type == UP_MODIFY ? "int" : "void";
	functype = up->type == UP_MODIFY ? "update" : "delete";

	if (syn && fprintf(f,
	    ".Ft %s\n"
	    ".Fo db_%s_%s",
	    rettype, up->parent->name, functype) < 0)
		return 0;
	if (!syn && fprintf(f, ".It Ft %s Fn db_%s_%s",
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

	if (syn && fputs(
	    "\n"
	    ".Fa \"struct ort *ort\"\n", f) == EOF)
		return 0;
	if (!syn && fprintf(f,
	    "\n"
	    ".TS\n"
	    "%sl l l.\n"
	    "%s-\tort\tstruct ort *\n",
	    up->type == UP_MODIFY ? "l " : "",
	    up->type == UP_MODIFY ? "-\t" : "") < 0)
		return 0;

	TAILQ_FOREACH(ur, &up->mrq, entries) {
		if (syn) {
			if (ur->field->type == FTYPE_BLOB)
				if (fprintf(f,
				    ".Fa \"size_t %s_sz\n",
				    ur->field->name) < 0)
					return 0;
			if (fputs(".Fa \"", f) == EOF)
				return 0;
			if (!gen_field_type(f, ur->field))
				return 0;
			if (fprintf(f, " %s\"\n", ur->field->name) < 0)
				return 0;
			continue;
		}
		if (fputs("\\(<-\t", f) == EOF)
			return 0;
		if (ur->field->type == FTYPE_BLOB)
			if (fprintf(f, "-\t%s (size)\tsize_t\n\\(<-\t",
			    ur->field->name) < 0)
				return 0;
		if (fprintf(f, "%s\t%s\t",
		    get_modtype_str(ur->mod), ur->field->name) < 0)
			return 0;
		if (!gen_field_type(f, ur->field))
			return 0;
		if (fputc('\n', f) == EOF)
			return 0;
	}

	TAILQ_FOREACH(ur, &up->crq, entries) {
		if (syn && OPTYPE_ISUNARY(ur->op))
			continue;
		if (syn) {
			if (ur->field->type == FTYPE_BLOB)
				if (fprintf(f,
				    ".Fa \"size_t %s_sz\n",
				    ur->field->name) < 0)
					return 0;
			if (fputs(".Fa \"", f) == EOF)
				return 0;
			if (!gen_field_type(f, ur->field))
				return 0;
			if (fprintf(f, " %s\"\n", ur->field->name) < 0)
				return 0;
			continue;
		}
		if (up->type == UP_MODIFY &&
		    fputs("\\(->\t", f) == EOF)
			return 0;
		if (ur->field->type == FTYPE_BLOB &&
		    !OPTYPE_ISUNARY(ur->op)) {
			if (fprintf(f, "-\t%s (size)\tsize_t\n",
			    ur->field->name) < 0)
				return 0;
			if (up->type == UP_MODIFY &&
			    fputs("\\(->\t", f) == EOF)
				return 0;
		}
		if (fprintf(f, "%s\t%s\t",
		    get_optype_str(ur->op), ur->field->name) < 0)
			return 0;
		if (!gen_field_type(f, ur->field))
			return 0;
		if (fputc('\n', f) == EOF)
			return 0;
	}

	if (syn) {
		if (fputs(".Fc\n", f) == EOF)
			return 0;
	} else {
		if (fputs(".TE\n", f) == EOF)
			return 0;
		if (up->doc != NULL && !gen_doc_block(f, up->doc, 0, 1))
			return 0;
		if (up->rolemap) {
			if (fputs(
			    ".Pp\n"
			    "Only allowed to the following:", f) == EOF)
				return 0;
			if (!gen_rolemap(f, up->rolemap))
				return 0;
			if (fputs(" .\n", f) == EOF)
				return 0;
		}
	}
	return 1;
}

/*
 * Return FALSE on failure, TRUE on success.
 */
static int
gen_insert(FILE *f, const struct strct *s, int syn)
{
	const struct field	*fd;

	if (syn && fprintf(f,
	    ".Ft int64_t\n"
	    ".Fo db_%s_insert\n", s->name) < 0)
		return 0;
	if (!syn && fprintf(f,
	    ".It Ft int64_t Fn db_%s_insert\n", s->name) < 0)
		return 0;
	if (syn && fputs(
	    ".Fa \"struct ort *ort\"\n", f) == EOF)
		return 0;
	if (!syn && fputs(
	    ".TS\nl l.\n"
	    "ort\tstruct ort *\n", f) == EOF)
		return 0;

	TAILQ_FOREACH(fd, &s->fq, entries) {
		if (fd->type == FTYPE_STRUCT || 
		    (fd->flags & FIELD_ROWID))
			continue;
		if (syn) {
			if (fd->type == FTYPE_BLOB)
				if (fprintf(f,
				    ".Fa \"size_t %s_sz\"\n",
				    fd->name) < 0)
					return 0;
			if (fputs(".Fa \"", f) == EOF)
				return 0;
			if (!gen_field_type(f, fd))
				return 0;
			if (fprintf(f, " %s\"\n", fd->name) < 0)
				return 0;
			continue;
		}
		if (fd->type == FTYPE_BLOB)
			if (fprintf(f, "%s (size)\tsize_t\n",
			    fd->name) < 0)
				return 0;
		if (fprintf(f, "%s\t", fd->name) < 0)
			return 0;
		if (!gen_field_type(f, fd))
			return 0;
		if (fputc('\n', f) == EOF)
			return 0;

	}

	if (syn && fputs(".Fc\n", f) == EOF)
		return 0;
	if (!syn && fputs(".TE\n", f) == EOF)
		return 0;
	if (!syn && s->ins->rolemap) {
		if (fputs(
		    ".Pp\n"
		    "Only allowed to the following:", f) == EOF)
			return 0;
		if (!gen_rolemap(f, s->ins->rolemap))
			return 0;
		if (fputs(" .\n", f) == EOF)
			return 0;
	}
	return 1;
}

/*
 * Return FALSE on failure, TRUE on success.
 */
static int
gen_deletes(FILE *f, const struct config *cfg, int syn)
{
	const struct strct	*s;
	const struct update	*up;

	TAILQ_FOREACH(s, &cfg->sq, entries)
		if (!TAILQ_EMPTY(&s->dq))
			break;
	if (s == NULL)
		return 1;

	if (!syn && fputs(
	    ".Ss Database deletions\n"
	    "Deletes from the databsae given constraint satisfaction.\n"
	    ".Bl -tag -width Ds\n", f) == EOF)
		return 0;

	TAILQ_FOREACH(s, &cfg->sq, entries)
		TAILQ_FOREACH(up, &s->dq, entries)
			if (!gen_update(f, up, syn))
				return 0;

	if (!syn && fputs(".El\n.Pp\n", f) == EOF)
		return 0;
	return 1;
}

/*
 * Return FALSE on failure, TRUE on success.
 */
static int
gen_updates(FILE *f, const struct config *cfg, int syn)
{
	const struct strct	*s;
	const struct update	*up;

	TAILQ_FOREACH(s, &cfg->sq, entries)
		if (!TAILQ_EMPTY(&s->uq))
			break;

	if (s == NULL)
		return 1;

	if (!syn && fputs(
	    ".Ss Database updates\n"
	    "In-place modification of the database.\n"
	    "Values constraining the update are labelled\n"
	    ".Qq \\(-> ,\n"
	    "while values used for updating are labelled\n"
	    ".Qq \\(<- .\n"
	    ".Bl -tag -width Ds\n", f) == EOF)
		return 0;

	TAILQ_FOREACH(s, &cfg->sq, entries)
		TAILQ_FOREACH(up, &s->uq, entries)
			if (!gen_update(f, up, syn))
				return 0;

	if (!syn && fputs(".El\n.Pp\n", f) == EOF)
		return 0;

	return 1;
}

/*
 * Return FALSE on failure, TRUE on success.
 */
static int
gen_inserts(FILE *f, const struct config *cfg, int syn)
{
	const struct strct	*s;

	TAILQ_FOREACH(s, &cfg->sq, entries)
		if (s->ins != NULL)
			break;
	if (s == NULL)
		return 0;

	if (!syn && fputs(
	    ".Ss Database inserts\n"
	    "Add new data to the database.\n"
	    "All functions return -1 on constraint failure or the new\n"
	    "row identifier on success.\n"
	    ".Bl -tag -width Ds\n", f) == EOF)
		return -1;

	TAILQ_FOREACH(s, &cfg->sq, entries) 
		if (s->ins != NULL && !gen_insert(f, s, syn))
			return -1;

	if (!syn && fputs(".El\n", f) == EOF)
		return 0;
	return 1;
}

/*
 * Return FALSE on failure, TRUE on success.
 */
static int
gen_json_input(FILE *f, const struct strct *s, int syn)
{

	if (syn) {
		return fprintf(f,
			".Ft int\n"
			".Fo jsmn_%s\n"
			".Fa \"struct %s *p\"\n"
			".Fa \"const char *buf\"\n"
			".Fa \"const jsmntok_t *toks\"\n"
			".Fa \"size_t toksz\"\n"
			".Fc\n"
			".Ft int\n"
			".Fo jsmn_%s_array\n"
			".Fa \"struct %s **ps\"\n"
			".Fa \"size_t *psz\"\n"
			".Fa \"const char *buf\"\n"
			".Fa \"const jsmntok_t *toks\"\n"
			".Fa \"size_t toksz\"\n"
			".Fc\n"
			".Ft int\n"
			".Fo jsmn_%s_clear\n"
			".Fa \"struct %s *p\"\n"
			".Fc\n"
			".Ft int\n"
			".Fo jsmn_%s_free_array\n"
			".Fa \"struct %s *ps\"\n"
			".Fa \"size_t psz\"\n"
			".Fc\n", 
			s->name, s->name, s->name, s->name, s->name,
			s->name, s->name, s->name) >= 0;
	}

	return fprintf(f,
		".It Ft int Fn jsmn_%s\n"
		".TS\n"
		"l l.\n"
		"p\tstruct %s *\n"
		"buf\tconst char *\n"
		"toks\tconst jsmntok_t *\n"
		"toksz\tsize_t\n"
		".TE\n"
		".Pp\n"
		"Parse a single structure and any nested structures\n"
		"from the JSON string.\n"
		"All fields must be specified.\n"
		"On success, free with\n"
		".Fn db_%s_free .\n"
		".It Ft int Fn jsmn_%s_array\n"
		".TS\n"
		"l l.\n"
		"ps\tstruct %s **\n"
		"psz\tsize_t *\n"
		"buf\tconst char *\n"
		"toks\tconst jsmntok_t *\n"
		"toksz\tsize_t\n"
		".TE\n"
		".Pp\n"
		"Parse an array of structures and any nested\n"
		"structures from the JSON string.\n"
		"All fields must be specified.\n"
		"On success, free with\n"
		".Fn jsmn_%s_free_array .\n"
		".It Ft int Fn jsmn_%s_clear\n"
		".TS\n"
		"l l.\n"
		"p\tstruct %s *\n"
		".TE\n"
		".It Ft int Fn jsmn_%s_free_array\n"
		".TS\n"
		"l l.\n"
		"ps\tstruct %s *\n"
		"psz\tsize_t\n"
		".TE\n"
		".Pp\n"
		"Free an array created by\n"
		".Fn jsmn_%s_array .\n",
		s->name, s->name, s->name, s->name, s->name, s->name,
		s->name, s->name, s->name, s->name, s->name) >= 0;
}

/*
 * Return FALSE on failure, TRUE on success.
 */
static int
gen_json_output(FILE *f, const struct strct *s, int syn)
{

	if (syn) {
		if (fprintf(f,
		    ".Ft void\n"
		    ".Fo json_%s_data\n"
		    ".Fa \"struct kjsonreq *r\"\n"
		    ".Fa \"const struct %s *p\"\n"
		    ".Fc\n"
		    ".Ft void\n"
		    ".Fo json_%s_obj\n"
		    ".Fa \"struct kjsonreq *r\"\n"
		    ".Fa \"const struct %s *p\"\n"
		    ".Fc\n", s->name, s->name, s->name, s->name) < 0)
			return 0;
		if ((s->flags & STRCT_HAS_QUEUE) && fprintf(f,
		    ".Ft void\n"
		    ".Fo json_%s_array\n"
		    ".Fa \"struct kjsonreq *r\"\n"
		    ".Fa \"const struct %s_q *q\"\n"
		    ".Fc\n", s->name, s->name) < 0)
			return 0;
		if ((s->flags & STRCT_HAS_ITERATOR) && fprintf(f,
		    ".Ft void\n"
		    ".Fo json_%s_iterate\n"
		    ".Fa \"const struct %s *p\"\n"
		    ".Fa \"void *arg\"\n"
		    ".Fc\n", s->name, s->name) < 0)
			return 0;
		return 1;
	}

	if (fprintf(f,
	    ".It Ft void Fn json_%s_data , Fn json_%s_obj\n"
	    ".TS\n"
	    "l l.\n"
	    "r\tstruct kjsonreq *\n"
	    "p\tconst struct %s *\n"
	    ".TE\n", s->name, s->name, s->name) < 0)
		return 0;
	if ((s->flags & STRCT_HAS_QUEUE) && fprintf(f,
	    ".It Ft void Fn json_%s_array\n"
	    ".TS\n"
	    "l l.\n"
	    "r\tstruct kjsonreq *\n"
	    "q\tconst struct %s_q *\n"
	    ".TE\n", s->name, s->name) < 0)
		return 0;
	if ((s->flags & STRCT_HAS_ITERATOR) && fprintf(f,
	    ".It Ft void Fn json_%s_iterate\n"
	    ".TS\n"
	    "l l.\n"
	    "p\tconst struct %s *\n"
	    "arg\tvoid * (cast to struct kjsonreq *)\n"
	    ".TE\n", s->name, s->name) < 0)
		return 0;
	return 1;
}

/*
 * Return FALSE on failure, TRUE on success.
 */
static int
gen_json_outputs(FILE *f, const struct config *cfg, int syn)
{
	const struct strct	*s;

	if (TAILQ_EMPTY(&cfg->sq))
		return 1;

	if (!syn && fputs(
	    ".Ss JSON output\n"
	    "Write out structure data in JSON to request\n"
	    ".Fa r ,\n"
	    "omitting passwords, fields marked \"noexport\", and\n"
	    "those disallowed by the current role.\n"
	    ".Bl -tag -width Ds\n", f) == EOF)
		return 0;

	TAILQ_FOREACH(s, &cfg->sq, entries)
		if (!gen_json_output(f, s, syn))
			return 0;

	if (!syn && fputs(".El\n", f) == EOF)
		return 0;
	return 1;
}

/*
 * Return FALSE on failure, TRUE on success.
 */
static int
gen_json_valids(FILE *f, const struct config *cfg, int syn)
{

	if (TAILQ_EMPTY(&cfg->sq))
		return 1;

	if (syn && fputs(
	    ".Vt enum valid_keys;\n"
	    ".Vt const struct kvalid valid_keys[];\n", f) == EOF)
		return 0;
	if (!syn && fputs(
	    ".Ss Validation\n"
	    "Each non-struct field in the configuration has a "
	    "validation function.\n"
	    "These may be passed to the HTTP parsing functions in\n"
	    ".Xr kcgi 3 ,\n"
	    "specifically\n"
	    ".Xr khttp_parse 3 .\n"
	    ".Bl -tag -width Ds\n"
	    ".It Va enum valid_keys\n"
	    "A list of keys, each one corresponding to a field.\n"
	    "The keys are named\n"
	    ".Dv VALID_struct_field .\n"
	    ".It Va const struct kvalid valid_keys[]\n"
	    "Validation functions associated with each field.\n"
	    ".El\n", f) == EOF)
		return 0;
	return 1;
}

/*
 * Return FALSE on failure, TRUE on success.
 */
static int
gen_json_inputs(FILE *f, const struct config *cfg, int syn)
{
	const struct strct	*s;

	if (TAILQ_EMPTY(&cfg->sq))
		return 1;

	if (!syn && fputs(
	    ".Ss JSON input\n"
	    "Allow for JSON objects and arrays, such as\n"
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
	    ".Bl -tag -width Ds\n", f) == EOF)
		return 0;

	if (syn) {
		if (fputs(
		    ".Ft void\n"
		    ".Fo jsmn_init\n"
		    ".Fa \"jsmn_parser *p\"\n"
		    ".Fc\n"
		    ".Ft int\n"
		    ".Fo jsmn_parse\n"
		    ".Fa \"jsmn_parser *p\"\n"
		    ".Fa \"const char *buf\"\n"
		    ".Fa \"size_t sz\"\n"
		    ".Fa \"jsmntok_t *toks\"\n"
		    ".Fa \"junsigned int toksz\"\n"
		    ".Fc\n"
		    ".Ft int\n"
		    ".Fo jsmn_eq\n"
		    ".Fa \"const char *json\"\n"
		    ".Fa \"const jsmntok_t *tok\"\n"
		    ".Fa \"const char *s\"\n"
		    ".Fc\n", f) == EOF)
			return 0;
		TAILQ_FOREACH(s, &cfg->sq, entries)
			if (!gen_json_input(f, s, 1))
				return 0;
		return 1;
	}

	if (fputs(
	    ".It Ft void Fn jsmn_init\n"
	    ".TS\n"
	    "l l.\n"
	    "p\tjsmn_parser *\n"
	    ".TE\n"
	    ".Pp\n"
	    "Initialise a parser\n"
	    ".Fa p\n"
	    "for use in\n"
	    ".Fn jsmn_parse .\n", f) == EOF)
		return 0;
	if (fputs(
	    ".It Ft int Fn jsmn_parse\n"
	    ".TS\n"
	    "l l.\n"
	    "p\tjsmn_parser *\n"
	    "buf\tconst char *\n"
	    "sz\tsize_t\n"
	    "toks\tjsmntok_t *\n"
	    "toksz\tunsigned int\n"
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
	if (fputs(
	    ".It Ft int Fn jsmn_eq\n"
	    ".TS\n"
	    "l l.\n"
	    "json\tconst char *\n"
	    "tok\tconst jsmntok_t *\n"
	    "s\tconst char *\n"
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
		if (!gen_json_input(f, s, 0))
			return 0;

	return fputs(".El\n", f) != EOF;
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
	    ".\\\" DO NOT EDIT!\n", ORT_VERSION) < 0)
		return 0;

	if (fprintf(f,
	    ".Dd $" "Mdocdate" "$\n"
	    ".Dt ORT 3\n"
	    ".Os\n"
	    ".Sh NAME\n"
	    ".Nm ort\n"
	    ".Nd C API for your openradtool data model\n"
	    ".Sh SYNOPSIS\n") < 0)
		return 0;
	if (!gen_general(f, cfg, 1))
		return 0;
	if (!gen_queries(f, cfg, 1))
		return 0;
	if (!gen_updates(f, cfg, 1))
		return 0;
	if (!gen_deletes(f, cfg, 1))
		return 0;
	if (!gen_inserts(f, cfg, 1))
		return 0;
	if ((args->flags & ORT_LANG_C_JSON_JSMN) &&
	    !gen_json_inputs(f, cfg, 1))
		return 0;
	if ((args->flags & ORT_LANG_C_JSON_KCGI) &&
	    !gen_json_outputs(f, cfg, 1))
		return 0;
	if ((args->flags & ORT_LANG_C_VALID_KCGI) &&
	    !gen_json_valids(f, cfg, 1))
		return 0;

	if (fprintf(f,
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

	if (!gen_general(f, cfg, 0))
		return 0;
	if (!gen_queries(f, cfg, 0))
		return 0;
	if (!gen_updates(f, cfg, 0))
		return 0;
	if (!gen_deletes(f, cfg, 0))
		return 0;
	if (!gen_inserts(f, cfg, 0))
		return 0;
	if ((args->flags & ORT_LANG_C_JSON_JSMN) &&
	    !gen_json_inputs(f, cfg, 0))
		return 0;
	if ((args->flags & ORT_LANG_C_JSON_KCGI) &&
	    !gen_json_outputs(f, cfg, 0))
		return 0;
	if ((args->flags & ORT_LANG_C_VALID_KCGI) &&
	    !gen_json_valids(f, cfg, 0))
		return 0;

	if ((args->flags & ORT_LANG_C_VALID_KCGI) ||
	    (args->flags & ORT_LANG_C_JSON_KCGI)) {
		if (fputs(".Sh SEE ALSO\n", f) == EOF)
			return 0;
		if ((args->flags & ORT_LANG_C_VALID_KCGI) &&
		    fputs(".Xr kcgi 3", f) == EOF)
			return 0;
		if ((args->flags & ORT_LANG_C_VALID_KCGI) &&
		    (args->flags & ORT_LANG_C_JSON_KCGI)) {
			if (fputs(" ,\n", f) == EOF)
				return 0;
		} else if (args->flags & ORT_LANG_C_VALID_KCGI) {
			if (fputc('\n', f) == EOF)
				return 0;
		}
		if ((args->flags & ORT_LANG_C_JSON_KCGI) &&
		    fputs(".Xr kcgijson 3\n", f) == EOF)
			return 0;
	}

	return 1;
}

