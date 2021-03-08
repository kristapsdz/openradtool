/*	$Id$ */
/*
 * Copyright (c) 2020 Kristaps Dzonsons <kristaps@bsd.lv>
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
#include <expat.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ort.h"
#include "ort-lang-xliff.h"

/*
 * A single source->target pair.
 */
struct	xliffunit {
	char			*name; /* id */
	char			*source; /* source */
	char			*target; /* target */
};

/*
 * All source->target pairs for a given translation.
 */
struct	xliffset {
	struct xliffunit	*u; /* translatable pairs */
	size_t			 usz; /* size of "u" */
	char			 *trglang; /* target language */
	char			 *srclang; /* source language */
	char			 *original; /* original file */
};

/*
 * Parse tracker for an XLIFF file that we're parsing.
 * This will be passed in (assuming success) to the "struct hparse" as
 * the xliffs pointer.
 */
struct	xparse {
	struct config		*cfg; /* ort(3) config (for messages) */
	XML_Parser		 p; /* parser */
	const char		*fname; /* xliff filename */
	struct xliffset		*set; /* resulting translation units */
	struct xliffunit	*curunit; /* current translating unit */
	int			 type; /* >0 source, <0 target, 0 none */
	int			 er; /* if non-zero exit on sys fail */
};

/* Forward declarations. */

static	void xend(void *, const XML_Char *);
static	void xstart(void *, const XML_Char *, const XML_Char **);
static	void xparse_err(struct xparse *, const char *, ...)
	__attribute__((format(printf, 2, 3)));

/*
 * Return zero on failure, non-zero on success.
 */
static int
xliff_extract_unit(struct config *cfg, FILE *f, 
	const struct labelq *lq, const char *type,
	const struct pos *pos, const char ***s, size_t *ssz)
{
	const struct label	*l;
	size_t			 i;
	void			*pp;

	TAILQ_FOREACH(l, lq, entries)
		if (l->lang == 0)
			break;

	if (l == NULL && type == NULL) {
		ort_msg(&cfg->mq, MSGTYPE_WARN, 0, pos, 
			"missing jslabel for translation");
		return 1;
	} else if (l == NULL) {
		ort_msg(&cfg->mq, MSGTYPE_WARN, 0, pos, "missing "
			"\"%s\" jslabel for translation", type);
		return 1;
	}

	for (i = 0; i < *ssz; i++)
		if (strcmp((*s)[i], l->label) == 0)
			return 1;

	pp = reallocarray(*s, *ssz + 1, sizeof(char **));
	if (NULL == pp)
		return 0;
	*s = pp;
	(*s)[*ssz] = l->label;
	(*ssz)++;
	return 1;
}

static int
xliff_sort(const void *p1, const void *p2)
{

	return(strcmp(*(const char **)p1, *(const char **)p2));
}

static void
xparse_err(struct xparse *xp, const char *fmt, ...)
{
	va_list		 ap;
	struct pos	 pos;

	pos.fname = xp->fname;
	pos.line = XML_GetCurrentLineNumber(xp->p);
	pos.column = XML_GetCurrentColumnNumber(xp->p);

	va_start(ap, fmt);
	ort_msgv(&xp->cfg->mq, MSGTYPE_WARN, 0, &pos, fmt, ap);
	va_end(ap);
}

static void
xparse_xliff_free(struct xliffset *p)
{
	size_t	 i;

	if (p == NULL)
		return;

	for (i = 0; i < p->usz; i++) {
		free(p->u[i].source);
		free(p->u[i].target);
		free(p->u[i].name);
	}
	free(p->original);
	free(p->srclang);
	free(p->trglang);
	free(p->u);
	free(p);
}

static void
xparse_free(struct xparse *p)
{

	assert(p != NULL);
	xparse_xliff_free(p->set);
	free(p);
}

static void
xtext(void *dat, const XML_Char *s, int len)
{
	struct xparse	 *p = dat;
	size_t		 sz;
	void		*pp;

	assert(p->curunit != NULL);
	assert(p->type);

	if (p->type > 0) {
		if (p->curunit->source != NULL) {
			sz = strlen(p->curunit->source);
			pp = realloc(p->curunit->source,
				 sz + len + 1);
			if (pp == NULL) {
				p->er = 1;
				XML_StopParser(p->p, 0);
				return;
			}
			p->curunit->source = pp;
			memcpy(p->curunit->source + sz, s, len);
			p->curunit->source[sz + len] = '\0';
		} else {
			p->curunit->source = strndup(s, len);
			if (p->curunit->source == NULL) {
				p->er = 1;
				XML_StopParser(p->p, 0);
				return;
			}
		}
	} else {
		if (p->curunit->target != NULL) {
			sz = strlen(p->curunit->target);
			pp = realloc(p->curunit->target,
				 sz + len + 1);
			if (pp == NULL) {
				p->er = 1;
				XML_StopParser(p->p, 0);
				return;
			}
			p->curunit->target = pp;
			memcpy(p->curunit->target + sz, s, len);
			p->curunit->target[sz + len] = '\0';
		} else {
			p->curunit->target = strndup(s, len);
			if (p->curunit->target == NULL) {
				p->er = 1;
				XML_StopParser(p->p, 0);
				return;
			}
		}
	}
}

static void
xstart(void *dat, const XML_Char *s, const XML_Char **atts)
{
	struct xparse	 *p = dat;
	const XML_Char	**attp;
	const char	 *ver, *id;
	void		 *pp;

	if (strcmp(s, "xliff") == 0) {
		if (p->set != NULL) {
			xparse_err(p, "nested <xliff>");
			XML_StopParser(p->p, 0);
			return;
		}
		p->set = calloc(1, sizeof(struct xliffset));
		if (p->set == NULL) {
			p->er = 1;
			XML_StopParser(p->p, 0);
			return;
		}
		ver = NULL;
		for (attp = atts; *attp != NULL; attp += 2) 
			if (strcmp(attp[0], "version") == 0)
				ver = attp[1];
		if (ver == NULL) {
			xparse_err(p, "<xliff> without version");
			XML_StopParser(p->p, 0);
			return;
		} else if (strcmp(ver, "1.2")) {
			xparse_err(p, "<xliff> version must be 1.2");
			XML_StopParser(p->p, 0);
			return;
		}
	} else if (strcmp(s, "file") == 0) {
		if (p->set == NULL) {
			xparse_err(p, "<file> not in <xliff>");
			XML_StopParser(p->p, 0);
			return;
		}
		if (p->set->trglang != NULL) {
			xparse_err(p, "nested <file>");
			XML_StopParser(p->p, 0);
			return;
		}
		for (attp = atts; *attp != NULL; attp += 2)
			if (strcmp(attp[0], "target-language") == 0) {
				free(p->set->trglang);
				p->set->trglang = strdup(attp[1]);
				if (p->set->trglang == NULL) {
					p->er = 1;
					XML_StopParser(p->p, 0);
					return;
				}
			} else if (strcmp(attp[0], "source-language") == 0) {
				free(p->set->srclang);
				p->set->srclang = strdup(attp[1]);
				if (p->set->srclang == NULL) {
					p->er = 1;
					XML_StopParser(p->p, 0);
					return;
				}
			} else if (strcmp(attp[0], "original") == 0) {
				free(p->set->original);
				p->set->original = strdup(attp[1]);
				if (p->set->original == NULL) {
					p->er = 1;
					XML_StopParser(p->p, 0);
					return;
				}
			}
		if (p->set->trglang == NULL) {
			xparse_err(p, "missing <file> target-language");
			XML_StopParser(p->p, 0);
			return;
		}
		if (p->set->srclang == NULL) {
			xparse_err(p, "missing <file> source-language");
			XML_StopParser(p->p, 0);
			return;
		}
		if (p->set->original == NULL) {
			xparse_err(p, "missing <file> original");
			XML_StopParser(p->p, 0);
			return;
		}
	} else if (strcmp(s, "trans-unit") == 0) {
		if (p->set->trglang == NULL) {
			xparse_err(p, "<trans-unit> not in <file>");
			XML_StopParser(p->p, 0);
			return;
		} else if (p->curunit != NULL) {
			xparse_err(p, "nested <trans-unit>");
			XML_StopParser(p->p, 0);
			return;
		}

		id = NULL;
		for (attp = atts; *attp != NULL; attp += 2)
			if (strcmp(attp[0], "id") == 0) {
				id = attp[1];
				break;
			}
		if (id == NULL) {
			xparse_err(p, "<trans-unit> without id");
			XML_StopParser(p->p, 0);
			return;
		}

		pp = reallocarray
			(p->set->u, p->set->usz + 1,
			 sizeof(struct xliffunit));
		if (pp == NULL) {
			p->er = 1;
			XML_StopParser(p->p, 0);
			return;
		}
		p->set->u = pp;
		p->curunit = &p->set->u[p->set->usz++];
		memset(p->curunit, 0, sizeof(struct xliffunit));
		p->curunit->name = strdup(id);
		if (p->curunit->name == NULL) {
			p->er = 1;
			XML_StopParser(p->p, 0);
			return;
		}
	} else if (strcmp(s, "source") == 0) {
		if (p->curunit == NULL) {
			xparse_err(p, "<soure> not in <trans-unit>");
			XML_StopParser(p->p, 0);
			return;
		} else if (p->type) {
			xparse_err(p, "nested <source>");
			XML_StopParser(p->p, 0);
			return;
		}
		p->type = 1;
		XML_SetDefaultHandlerExpand(p->p, xtext);
	} else if (strcmp(s, "target") == 0) {
		if (p->curunit == NULL) {
			xparse_err(p, "<target> not in <trans-unit>");
			XML_StopParser(p->p, 0);
			return;
		} else if (p->type) {
			xparse_err(p, "nested <target>");
			XML_StopParser(p->p, 0);
			return;
		}
		p->type = -1;
		XML_SetDefaultHandlerExpand(p->p, xtext);
	} else {
		if (p->type) {
			xparse_err(p, "element in translation");
			XML_StopParser(p->p, 0);
			return;
		}
	}
}

/*
 * XML-escape the given string.
 * This only considers the five predefined XML escapes.
 * Return NULL on memory failure.
 */
static char *
escape(const char *s)
{
	size_t	 i, sz, count, bufsz;
	char	*buf;

	sz = strlen(s);
	for (i = count = 0; i < sz; i++)
		if (s[i] == '<' || 
		    s[i] == '&' ||
		    s[i] == '>' || 
		    s[i] == '"' || 
		    s[i] == '\'')
			count++;

	if (count == 0)
		return strdup(s);

	bufsz = sz + count * 5 + 1;
	if ((buf = malloc(bufsz)) == NULL)
		return 0;

	for (i = count = 0; i < sz; i++) {
		buf[count] = '\0';
		if ('<' == s[i])
			count = strlcat(buf, "&lt;", bufsz);
		else if ('&' == s[i])
			count = strlcat(buf, "&amp;", bufsz);
		else if ('>' == s[i])
			count = strlcat(buf, "&gt;", bufsz);
		else if ('"' == s[i])
			count = strlcat(buf, "&quot;", bufsz);
		else if ('\'' == s[i])
			count = strlcat(buf, "&apos;", bufsz);
		else 
			buf[count++] = s[i];
	}

	buf[count] = '\0';
	return buf;
}

/*
 * In-place unescape from XML into native characters.
 * This considers only '&amp;' -> '&' and '&lt;' -> '<'.
 */
static void
unescape(char *s)
{
	char	*ns = s;

	for ( ; *s != '\0'; ns++)
		if (*s != '&') {
			*ns = *s;
			s++;
		} else if (strncmp(s, "&lt;", 4) == 0) {
			*ns = '<';
			s += 4;
		} else if (strncmp(s, "&amp;", 5) == 0) {
			*ns = '&';
			s += 5;
		} else if (strncmp(s, "&gt;", 4) == 0) {
			*ns = '>';
			s += 4;
		} else if (strncmp(s, "&quot;", 6) == 0) {
			*ns = '"';
			s += 6;
		} else if (strncmp(s, "&apos;", 6) == 0) {
			*ns = '\'';
			s += 6;
		} else {
			*ns = *s;
			s++;
		}
	*ns = '\0';
}

static void
xend(void *dat, const XML_Char *s)
{
	struct xparse	*p = dat;

	XML_SetDefaultHandlerExpand(p->p, NULL);

	if (strcmp(s, "trans-unit") == 0) {
		assert(p->curunit != NULL);
		if (p->curunit->source == NULL || 
		    p->curunit->target == NULL) {
			xparse_err(p, "missing <source> or "
				"<target> in <trans-unit>");
			XML_StopParser(p->p, 0);
			return;
		}
		p->curunit = NULL;
	} else if (strcmp(s, "target") == 0) {
		assert(p->curunit != NULL);
		unescape(p->curunit->target);
		p->type = 0;
	} else if (strcmp(s, "source") == 0) {
		assert(p->curunit != NULL);
		unescape(p->curunit->source);
		p->type = 0;
	}
}

/*
 * Parse an XLIFF file.
 * Sets result in "ret" iff returning >0, otherwise res is set NULL.
 * Returns <0 on failure, 0 on syntax error, >0 on success.
 */
static int
xliff_read(struct config *cfg, FILE *f, 
	const char *fn, XML_Parser p, struct xliffset **ret)
{
	struct xparse	*xp;
	size_t		 sz;
	char		 buf[BUFSIZ];

	*ret = NULL;

	if ((xp = calloc(1, sizeof(struct xparse))) == NULL)
		return -1;

	xp->cfg = cfg;
	xp->fname = fn;
	xp->p = p;

	XML_ParserReset(p, NULL);
	XML_SetDefaultHandlerExpand(p, NULL);
	XML_SetElementHandler(p, xstart, xend);
	XML_SetUserData(p, xp);

	do {
		sz = fread(buf, 1, sizeof(buf), f);
		if (ferror(f)) {
			xparse_free(xp);
			return -1;
		}
		if (XML_Parse(p, buf, sz, feof(f)) == XML_STATUS_OK)
			continue;

		/* If we have a hard error, stop at once. */

		if (xp->er) {
			xparse_free(xp);
			return -1;
		}
		xparse_err(xp, "%s", 
			XML_ErrorString(XML_GetErrorCode(p)));
		xparse_free(xp);
		return 0;
	} while (!feof(f));

	*ret = xp->set;
	xp->set = NULL;
	xparse_free(xp);
	return 1;
}

static int
xliffunit_sort(const void *a1, const void *a2)
{
	struct xliffunit *p1 = (struct xliffunit *)a1,
		 	 *p2 = (struct xliffunit *)a2;

	return strcmp(p1->source, p2->source);
}

/*
 * Return <0 on error, 0 on syntax error, >0 on success.
 */
static int
xliff_join_unit(struct config *cfg, struct labelq *q, int copy, 
	const char *type, size_t lang, const struct xliffset *x, 
	const struct pos *pos)
{
	struct label	*l;
	size_t		 i;
	const char	*targ = NULL;

	TAILQ_FOREACH(l, q, entries)
		if (l->lang == 0)
			break;

	/* 
	 * See if we have a default translation (lang == 0). 
	 * This is going to be the material we want to translate.
	 */

	if (l == NULL && type == NULL) {
		ort_msg(&cfg->mq, MSGTYPE_ERROR, 0, pos, 
			"missing jslabel for translation");
		return 0;
	} else if (l == NULL) {
		ort_msg(&cfg->mq, MSGTYPE_ERROR, 0, pos, "missing "
			"\"%s\" jslabel for translation", type);
		return 0;
	}

	/* Look up what we want to translate in the database. */

	for (i = 0; i < x->usz; i++)
		if (strcmp(x->u[i].source, l->label) == 0)
			break;

	if (i == x->usz && type == NULL) {
		if (copy) {
			ort_msg(&cfg->mq, MSGTYPE_WARN, 0, pos, 
				"using source for translation");
			targ = l->label;
		} else {
			ort_msg(&cfg->mq, MSGTYPE_ERROR, 0, pos, 
				"missing jslabel for translation");
			return 0;
		}
	} else if (i == x->usz) {
		if (copy) {
			ort_msg(&cfg->mq, MSGTYPE_WARN, 0, pos, "using "
				"source for \"%s\" translation", type);
			targ = l->label;
		} else {
			ort_msg(&cfg->mq, MSGTYPE_ERROR, 0, pos, "missing "
				"\"%s\" jslabel for translation", type);
			return 0;
		}
	} else
		targ = x->u[i].target;

	assert(targ != NULL);

	/* 
	 * We have what we want to translate, now make sure that we're
	 * not overriding an existing translation.
	 */

	TAILQ_FOREACH(l, q, entries)
		if (l->lang == lang)
			break;

	if (l != NULL && type == NULL) {
		ort_msg(&cfg->mq, MSGTYPE_WARN, 0, pos, 
			"not overriding existing translation");
		return 1;
	} else if (l != NULL) {
		ort_msg(&cfg->mq, MSGTYPE_WARN, 0, pos, 
			"not overriding existing \"%s\" translation",
			type);
		return 1;
	}

	/* Add the translation. */

	if ((l = calloc(1, sizeof(struct label))) == NULL)
		return -1;
	l->lang = lang;
	if ((l->label = strdup(targ)) == NULL)
		return -1;
	TAILQ_INSERT_TAIL(q, l, entries);
	return 1;
}

/*
 * Return <0 on failure, 0 on syntax error, >0 on success.
 */
static int
xliff_update_unit(struct config *cfg, struct labelq *q, 
	const char *type, struct xliffset *x, const struct pos *pos)
{
	struct label		*l;
	size_t			 i;
	struct xliffunit	*u;
	char			 nbuf[32];
	void			*pp;

	TAILQ_FOREACH(l, q, entries)
		if (l->lang == 0)
			break;

	/* 
	 * See if we have a default translation (lang == 0). 
	 * This is going to be the material we want to translate.
	 */

	if (l == NULL && type == NULL) {
		ort_msg(&cfg->mq, MSGTYPE_ERROR, 0, pos, 
			"missing jslabel for translation");
		return 0;
	} else if (l == NULL) {
		ort_msg(&cfg->mq, MSGTYPE_ERROR, 0, pos, "missing "
			"\"%s\" jslabel for translation", type);
		return 0;
	}

	/* Look up what we want to translate in the database. */

	for (i = 0; i < x->usz; i++)
		if (strcmp(x->u[i].source, l->label) == 0)
			break;

	if (i == x->usz) {
		pp = reallocarray(x->u, 
			x->usz + 1, sizeof(struct xliffunit));
		if (pp == NULL)
			return -1;
		x->u = pp;
		u = &x->u[x->usz++];
		snprintf(nbuf, sizeof(nbuf), "%zu", x->usz);
		memset(u, 0, sizeof(struct xliffunit));
		if ((u->name = strdup(nbuf)) == NULL)
			return -1;
		if ((u->source = strdup(l->label)) == NULL)
			return -1;
	}

	return 1;
}

/*
 * Return <0 on error, 0 on syntax error, >0 on success.
 */
static int
xliff_join_xliff(struct config *cfg, int copy,
	size_t lang, const struct xliffset *x)
{
	struct enm 	*e;
	struct eitem 	*ei;
	struct bitf	*b;
	struct bitidx	*bi;
	int		 rc;

	TAILQ_FOREACH(e, &cfg->eq, entries)
		TAILQ_FOREACH(ei, &e->eq, entries) {
			rc = xliff_join_unit
			    (cfg, &ei->labels, copy, NULL, 
			     lang, x, &ei->pos);
			if (rc <= 0)
				return rc;
		}

	TAILQ_FOREACH(b, &cfg->bq, entries) {
		TAILQ_FOREACH(bi, &b->bq, entries) {
			rc = xliff_join_unit
			    (cfg, &bi->labels, copy, NULL, 
			     lang, x, &bi->pos);
			if (rc <= 0)
				return rc;
		}
		rc = xliff_join_unit
		    (cfg, &b->labels_unset, copy, 
		     "isunset", lang, x, &b->pos);
		if (rc <= 0)
			return rc;
		rc = xliff_join_unit
		    (cfg, &b->labels_null, copy, 
		     "isnull", lang, x, &b->pos);
		if (rc <= 0)
			return rc;
	}

	return 1;
}

/*
 * Parse an XLIFF file with open descriptor "xml" and merge the
 * translations with labels in "cfg".
 * Returns <0 on error, 0 on syntax error, >0 on success.
 */
static int
xliff_join_single(struct config *cfg, int copy, 
	FILE *xml, const char *fn, XML_Parser p)
{
	struct xliffset	*x;
	size_t		 i;
	void		*pp;
	int		 rc;
	struct pos	 pos;

	if ((rc = xliff_read(cfg, xml, fn, p, &x)) <= 0)
		return rc;

	assert(x->trglang != NULL);

	for (i = 0; i < cfg->langsz; i++)
		if (strcmp(cfg->langs[i], x->trglang) == 0)
			break;

	if (i == cfg->langsz) {
		pp = reallocarray(cfg->langs,
			 cfg->langsz + 1, sizeof(char *));
		if (pp == NULL)
			return -1;
		cfg->langs = pp;
		cfg->langs[cfg->langsz] = strdup(x->trglang);
		if (cfg->langs[cfg->langsz] == NULL)
			return -1;
		cfg->langsz++;
	} else {
		pos.fname = fn;
		pos.line = pos.column = 0;
		ort_msg(&cfg->mq, MSGTYPE_WARN, 0, &pos, 
			"language \"%s\" already noted", x->trglang);
	}

	rc = xliff_join_xliff(cfg, copy, i, x);
	xparse_xliff_free(x);
	return rc;
}

/*
 * Parse XLIFF files with existing translations and update them (using a
 * single file) with new labels in "cfg"
 * Returns zero on failure, non-zero on success.
 */
int
ort_lang_xliff_update(const struct ort_lang_xliff *args, 
	struct config *cfg, FILE *f, struct msgq *mq)
{
	struct xliffset	*x;
	int		 rc;
	size_t		 i;
	XML_Parser	 p;
	struct enm 	*e;
	struct eitem 	*ei;
	struct bitf	*b;
	struct bitidx	*bi;
	char		*sbuf = NULL, *tbuf = NULL;
	struct msgq	 tmpq = TAILQ_HEAD_INITIALIZER(tmpq);

	if ((p = XML_ParserCreate(NULL)) == NULL)
		return -1;

	if (mq == NULL)
		mq = &tmpq;

	assert(args->insz == 1);
	rc = xliff_read(cfg, args->in[0], args->fnames[0], p, &x);
	if (rc <= 0)
		goto out;

	assert(x != NULL);

	TAILQ_FOREACH(e, &cfg->eq, entries)
		TAILQ_FOREACH(ei, &e->eq, entries) {
			rc = xliff_update_unit
			    (cfg, &ei->labels, NULL, x, &ei->pos);
			if (rc <= 0)
				goto out;
		}

	TAILQ_FOREACH(b, &cfg->bq, entries) {
		TAILQ_FOREACH(bi, &b->bq, entries) {
			rc = xliff_update_unit
			    (cfg, &bi->labels, NULL, x, &bi->pos);
			if (rc <= 0)
				goto out;
		}
		rc = xliff_update_unit
		    (cfg, &b->labels_unset, "isunset", x, &b->pos);
		if (rc <= 0)
			goto out;
		rc = xliff_update_unit
		    (cfg, &b->labels_null, "isnull", x, &b->pos);
		if (rc <= 0)
			goto out;
	}

	rc = -1;
	qsort(x->u, x->usz, sizeof(struct xliffunit), xliffunit_sort);

	if (fprintf(f, 
	    "<xliff version=\"1.2\" "
	    "xmlns=\"urn:oasis:names:tc:xliff:document:1.2\">\n"
	    "\t<file target-language=\"%s\" source-language=\"%s\" "
	    "original=\"%s\" datatype=\"plaintext\">\n"
            "\t\t<body>\n", x->trglang, x->srclang, x->original) < 0)
		goto out;

	for (i = 0; i < x->usz; i++) {
		if ((sbuf = escape(x->u[i].source)) == NULL)
			goto out;
		if (x->u[i].target != NULL &&
		    (tbuf = escape(x->u[i].target)) == NULL)
			goto out;

		if (tbuf == NULL && 
		    (args->flags & ORT_LANG_XLIFF_COPY)) {
			if (fprintf(f,
			    "\t\t\t<trans-unit id=\"%s\">\n"
			    "\t\t\t\t<source>%s</source>\n"
			    "\t\t\t\t<target>%s</target>\n"
			    "\t\t\t</trans-unit>\n",
			    x->u[i].name, sbuf, sbuf) < 0)
				goto out;
		} else if (tbuf == NULL) {
			if (fprintf(f, 
			    "\t\t\t<trans-unit id=\"%s\">\n"
			    "\t\t\t\t<source>%s</source>\n"
			    "\t\t\t</trans-unit>\n",
			    x->u[i].name, sbuf) < 0)
				goto out;
		} else {
			if (fprintf(f, 
			    "\t\t\t<trans-unit id=\"%s\">\n"
			    "\t\t\t\t<source>%s</source>\n"
			    "\t\t\t\t<target>%s</target>\n"
			    "\t\t\t</trans-unit>\n",
			    x->u[i].name, sbuf, tbuf) < 0)
				goto out;
		}
		free(sbuf);
		free(tbuf);
		sbuf = tbuf = NULL;
	}

	if (fputs("\t\t</body>\n\t</file>\n</xliff>\n", f) == EOF)
		goto out;

	rc = 1;
out:
	free(sbuf);
	free(tbuf);
	xparse_xliff_free(x);
	XML_ParserFree(p);
	if (mq == &tmpq)
		ort_msgq_free(mq);
	return rc;
}

int
ort_lang_xliff_join(const struct ort_lang_xliff *args,
	struct config *cfg, FILE *f, struct msgq *mq)
{
	int	 	 rc = 1;
	size_t		 i;
	XML_Parser	 p;
	struct msgq	 tmpq = TAILQ_HEAD_INITIALIZER(tmpq);

	if ((p = XML_ParserCreate(NULL)) == NULL)
		return -1;

	if (mq == NULL)
		mq = &tmpq;

	for (i = 0; i < args->insz; i++)  {
		rc = xliff_join_single(cfg, 
			args->flags & ORT_LANG_XLIFF_COPY, 
			args->in[i], args->fnames[i], p);
		if (rc <= 0)
			break;
	}

	XML_ParserFree(p);

	if (rc > 0 && !ort_write_file(f, cfg))
		rc = -1;
	if (mq == &tmpq)
		ort_msgq_free(mq);

	return rc;
}

int
ort_lang_xliff_extract(const struct ort_lang_xliff *args,
	struct config *cfg, FILE *f, struct msgq *mq)
{
	const struct enm	 *e;
	const struct eitem	 *ei;
	const struct bitf	 *b;
	const struct bitidx	 *bi;
	size_t			  i, ssz = 0;
	const char		**s = NULL;
	char			 *buf;
	int			  rc = 0;
	struct msgq		  tmpq = TAILQ_HEAD_INITIALIZER(tmpq);

	if (mq == NULL)
		mq = &tmpq;

	/* Extract all unique labels strings. */

	TAILQ_FOREACH(e, &cfg->eq, entries)
		TAILQ_FOREACH(ei, &e->eq, entries)
			if (!xliff_extract_unit(cfg, f, &ei->labels, 
			    NULL, &ei->pos, &s, &ssz))
				goto out;

	TAILQ_FOREACH(b, &cfg->bq, entries) {
		TAILQ_FOREACH(bi, &b->bq, entries)
			if (!xliff_extract_unit(cfg, f, &bi->labels, 
			    NULL, &bi->pos, &s, &ssz))
				goto out;
		if (!xliff_extract_unit(cfg, f, &b->labels_unset, 
		    "unset", &b->pos, &s, &ssz))
			goto out;
		if (!xliff_extract_unit(cfg, f, &b->labels_null,
		    "isnull", &b->pos, &s, &ssz))
			goto out;
	}

	qsort(s, ssz, sizeof(char *), xliff_sort);

	/* 
	 * Emit them all, sorted, in XLIFF format.
	 * Escape all '<' and '&' as '&lt;' and '&amp;'.
	 */

	if (fprintf(f, "<xliff version=\"1.2\" "
	    "xmlns=\"urn:oasis:names:tc:xliff:document:1.2\">\n"
	    "\t<file source-language=\"TODO\" original=\"%s\" "
	    "target-language=\"TODO\" datatype=\"plaintext\">\n"
            "\t\t<body>\n", cfg->fnamesz ? cfg->fnames[0] : "") < 0)
		goto out;

	for (i = 0; i < ssz; i++) {
		rc = 0;
		if ((buf = escape(s[i])) == NULL)
			goto out;
		rc = (args->flags & ORT_LANG_XLIFF_COPY) ?
			fprintf(f, "\t\t\t<trans-unit id=\"%zu\">\n"
			    "\t\t\t\t<source>%s</source>\n"
			    "\t\t\t\t<target>%s</target>\n"
			    "\t\t\t</trans-unit>\n",
			    i + 1, buf, buf) :
			fprintf(f, "\t\t\t<trans-unit id=\"%zu\">\n"
			    "\t\t\t\t<source>%s</source>\n"
			    "\t\t\t</trans-unit>\n",
			    i + 1, buf);
		free(buf);
		if (rc < 0) {
			rc = 0;
			goto out;
		}
	}

	rc = fputs("\t\t</body>\n\t</file>\n</xliff>\n", f) != EOF;
out:
	if (mq == &tmpq)
		ort_msgq_free(mq);
	return rc;

}

