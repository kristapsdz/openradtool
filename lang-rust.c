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
#include <limits.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ort.h"
#include "ort-lang-rust.h"
#include "ort-version.h"
#include "lang.h"

static	const char *const ftypes[FTYPE__MAX] = {
	"i64", /* FTYPE_BIT */
	"i64", /* FTYPE_DATE */
	"i64", /* FTYPE_EPOCH */
	"i64", /* FTYPE_INT */
	"f64", /* FTYPE_REAL */
	"ByteBuffer", /* FTYPE_BLOB */
	"String", /* FTYPE_TEXT */
	"String", /* FTYPE_PASSWORD */
	"String", /* FTYPE_EMAIL */
	NULL, /* FTYPE_STRUCT */
	NULL, /* FTYPE_ENUM */
	"i64", /* FTYPE_BITFIELD */
};

static char *
strdup_title(const char *s)
{
	char	*cp;

	if ((cp = strdup(s)) != NULL)
		cp[0] = (char)toupper((unsigned char)cp[0]);

	return cp;
}

static int
gen_data_strct(const struct strct *s, FILE *f)
{
	const struct field	*fd;
	char			*cp = NULL, *name = NULL;
	int			 rc = 0;

	if ((name = strdup_title(s->name)) == NULL)
		goto out;
	if (fprintf(f, "%8spub struct %s {\n", "", name) < 0)
		goto out;

	TAILQ_FOREACH(fd, &s->fq, entries) {
		if (fprintf(f, "%12spub %s: ", "", fd->name) < 0)
			goto out;

		if ((fd->flags & FIELD_NULL) &&
		    fputs("Option<", f) == EOF)
			goto out;

		if (fd->type == FTYPE_STRUCT) {
			free(cp);
			cp = strdup_title(fd->ref->target->parent->name);
			if (cp == NULL)
				goto out;
			if (fputs(cp, f) == EOF)
				goto out;
		} else if (fd->type == FTYPE_ENUM) {
			free(cp);
			cp = strdup_title(fd->enm->name);
			if (cp == NULL)
				goto out;
			if (fprintf(f, "super::types::%s", cp) < 0)
				goto out;
		} else {
			if (fputs(ftypes[fd->type], f) == EOF)
				goto out;
		}

		if ((fd->flags & FIELD_NULL) && fputc('>', f) == EOF)
			goto out;
		if (fputs(",\n", f) == EOF)
			goto out;
	}

	if (fprintf(f, "%8s}\n", "") < 0)
		goto out;

	if (fprintf(f, 
	    "%8simpl %s {\n"
	    "%12sfn to_json(&self) -> String {\n"
	    "%16slet ret = String::new();\n",
	    "", name, "", "") < 0)
		goto out;

	TAILQ_FOREACH(fd, &s->fq, entries) {
		/* TODO */
	}

	rc = fprintf(f, "%16sret\n%12s}\n%8s}\n", "", "", "") >= 0;
out:
	free(name);
	free(cp);
	return rc;
}

static int
gen_types_bitf(const struct bitf *b, FILE *f)
{
	const struct bitidx	*bi;
	char			*name = NULL, *cp = NULL;
	int			 rc = 0;

	if ((name = strdup_title(b->name)) == NULL)
		goto out;
	if (fprintf(f, "%8spub enum %s {\n", "", name) < 0)
		goto out;

	TAILQ_FOREACH(bi, &b->bq, entries) {
		free(cp);
		if ((cp = strdup_title(bi->name)) == NULL)
			goto out;
		if (fprintf(f,
		    "%12s%s = %" PRId64 ",\n", "", cp, bi->value) < 0)
			goto out;
	}

	rc = fprintf(f, "%8s}\n", "") >= 0;
out:
	free(name);
	free(cp);
	return rc;
}

static int
gen_types_enum(const struct enm *e, FILE *f)
{
	const struct eitem	*ei;
	char			*name = NULL, *cp = NULL;
	int			 rc = 0;

	if ((name = strdup_title(e->name)) == NULL)
		goto out;
	if (fprintf(f, "%8spub enum %s {\n", "", name) < 0)
		goto out;

	TAILQ_FOREACH(ei, &e->eq, entries) {
		free(cp);
		if ((cp = strdup_title(ei->name)) == NULL)
			goto out;
		if (fprintf(f,
		    "%12s%s = %" PRId64 ",\n", "", cp, ei->value) < 0)
			goto out;
	}

	rc = fprintf(f, "%8s}\n", "") >= 0;
out:
	free(name);
	free(cp);
	return rc;
}

static int
gen_objs_strct(const struct strct *s, FILE *f)
{
	const struct field	*fd;
	char			*name = NULL;
	int			 rc = 0;

	if ((name = strdup_title(s->name)) == NULL)
		goto out;
	if (fprintf(f,
	    "%8spub struct %s {\n"
	    "%12spub data: super::data::%s,\n",
	    "", name, "", name) < 0)
		goto out;

	if (!TAILQ_EMPTY(&s->cfg->arq) &&
	    fprintf(f, "%12srole: super::Ortrole,\n", "") < 0)
		goto out;

	if (fprintf(f,
	    "%8s}\n"
	    "%8simpl %s {\n"
	    "%12spub fn export(&self) -> String {\n"
	    "%16slet ret = String::new();\n",
	    "", "", name, "", "") < 0)
		goto out;

	TAILQ_FOREACH(fd, &s->fq, entries) {
		/* TODO */
	}

	rc = fprintf(f, "%16sret\n%12s}\n%8s}\n", "", "", "") >= 0;
out:
	free(name);
	return rc;
}

static int
gen_data(const struct config *cfg, FILE *f)
{
	const struct strct	*s;

	if (fprintf(f, "\n%4spub mod data {\n", "") < 0)
		return 0;

	TAILQ_FOREACH(s, &cfg->sq, entries)
		if (!gen_data_strct(s, f))
			return 0;

	return fprintf(f, "%4s}\n", "") != EOF;
}

static int
gen_types(const struct config *cfg, FILE *f)
{
	const struct enm	*e;
	const struct bitf	*b;

	if (fprintf(f, "\n%4spub mod types {\n", "") < 0)
		return 0;

	TAILQ_FOREACH(e, &cfg->eq, entries)
		if (!gen_types_enum(e, f))
			return 0;
	TAILQ_FOREACH(b, &cfg->bq, entries)
		if (!gen_types_bitf(b, f))
			return 0;

	return fprintf(f, "%4s}\n", "") != EOF;
}

static int
gen_objs(const struct config *cfg, FILE *f)
{
	const struct strct	*s;

	if (fprintf(f, "\n%4spub mod objs {\n", "") < 0)
		return 0;

	TAILQ_FOREACH(s, &cfg->sq, entries)
		if (!gen_objs_strct(s, f))
			return 0;

	return fprintf(f, "%4s}\n", "") != EOF;
}

static int
gen_roles(const struct config *cfg, FILE *f)
{
	const struct role	*r;
	char			*cp;
	int			 rc;

	if (TAILQ_EMPTY(&cfg->arq))
		return 1;

	if (fprintf(f, "\n"
	    "%4s#[derive(PartialEq)]\n"
	    "%4s#[derive(Debug)]\n"
	    "%4spub enum Ortrole {\n", "", "", "") < 0)
		return 0;

	TAILQ_FOREACH(r, &cfg->arq, entries) {
		if ((cp = strdup_title(r->name)) == NULL)
			return 0;
		rc = fprintf(f, "%8s%s,\n", "", cp);
		free(cp);
		if (rc < 0)
			return 0;
	}

	return fprintf(f, "%4s}\n", "") >= 0;
}

int
ort_lang_rust(const struct ort_lang_rust *args,
	const struct config *cfg, FILE *f)
{
	struct ort_lang_rust	 tmp;

	if (args == NULL) {
		memset(&tmp, 0, sizeof(struct ort_lang_rust));
		args = &tmp;
	}

	if (!gen_commentv(f, 0, COMMENT_C, 
	    "WARNING: automatically generated by ort %s.\n"
	    "DO NOT EDIT!", ORT_VERSION))
		return 0;

	if (fprintf(f,
	    "pub mod ort {\n"
	    "%4suse rusqlite::{Connection,Result};\n"
	    "\n"
	    "%4spub const Version: &str = \"%s\";\n"
	    "%4spub const Vstamp: i64 = %lld;\n",
	    "", "", ORT_VERSION, "", (long long)ORT_VSTAMP) < 0)
		return 0;

	if (!gen_roles(cfg, f))
		return 0;
	if (!gen_data(cfg, f))
		return 0;
	if (!gen_objs(cfg, f))
		return 0;
	if (!gen_types(cfg, f))
		return 0;

	if (fprintf(f, "\n"
	    "%4spub struct Ortctx {\n"
	    "%8sconn: Connection,\n", "", "") < 0)
		return 0;
	if (!TAILQ_EMPTY(&cfg->arq) &&
	    fprintf(f, "%8srole: Ortrole,\n", "") < 0)
		return 0;
	if (fprintf(f, "%4s}\n", "") < 0)
		return 0;

	if (fprintf(f, "\n"
	    "%4simpl Ortctx {\n"
	    "%8spub fn connect(dbname: &str) -> Result<Ortctx, rusqlite::Error> {\n"
	    "%12slet conn = Connection::open(dbname)?;\n"
	    "%12sOk(Ortctx {\n"
	    "%16sconn,\n", "", "", "", "", "") < 0)
		return 0;
	if (!TAILQ_EMPTY(&cfg->arq) &&
	    fprintf(f, "%16srole: Ortrole::Default,\n", "") < 0)
		return 0;
	if (fprintf(f, "%12s})\n%8s}\n%4s}\n", "", "", "") < 0)
		return 0;

	return fputs("}\n", f) != EOF;
}
