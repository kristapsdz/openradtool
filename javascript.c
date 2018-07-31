/*	$Id$ */
/*
 * Copyright (c) 2017, 2018 Kristaps Dzonsons <kristaps@bsd.lv>
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
#if HAVE_ERR
# include <err.h>
#endif
#include <inttypes.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "extern.h"

static	const char *types[FTYPE__MAX] = {
	"number", /* FTYPE_BIT */
	"number", /* FTYPE_DATE */
	"number", /* FTYPE_EPOCH */
	"number", /* FTYPE_INT */
	"double", /* FTYPE_REAL */
	NULL, /* FTYPE_BLOB */
	"string", /* FTYPE_TEXT */
	"string", /* FTYPE_PASSWORD */
	"string", /* FTYPE_EMAIL */
	NULL, /* FTYPE_STRUCT */
	"number", /* FTYPE_ENUM */
	"number", /* FTYPE_BITFIELD */
};

/*
 * Escape JavaScript string literal.
 * This escapes backslashes and single apostrophes.
 */
static void
gen_label_text(const char *cp)
{
	unsigned char	 c;

	while ('\0' != (c = *cp++))
		switch (c) {
		case '\\':
			printf("\\\\");
			break;
		case '\'':
			printf("\\\'");
			break;
		default:
			putchar(c);
			break;
		}
}

/*
 * Print out a characteristic array of language labels.
 * This looks like {_default: 'xxx', en: 'yyy'} and so on.
 * Each language is represented; if not found, an empty string is used.
 * The default language is called "_default".
 */
static void
gen_labels(const struct config *cfg, const struct labelq *q)
{
	const struct label *l;
	size_t		 i;
	const char	*def = NULL;

	TAILQ_FOREACH(l, q, entries)
		if (l->lang == 0) {
			def = l->label;
			break;
		}

	putchar('{');
	for (i = 0; i < cfg->langsz; i++) {
		TAILQ_FOREACH(l, q, entries) 
			if (l->lang == i)
				break;
		if (NULL != l) {
			printf("%s: \'", i == 0 ? 
				"_default" : cfg->langs[i]);
			gen_label_text(l->label);
			putchar('\'');
		} else if (i > 0 && NULL != def) {
			printf("%s: \'", i == 0 ? 
				"_default" : cfg->langs[i]);
			gen_label_text(def);
			putchar('\'');
		} else
			printf("%s: \'\'", i == 0 ?
				"_default" : cfg->langs[i]);

		if (i < cfg->langsz - 1)
			printf(", ");
	}
	putchar('}');
}

static void
warn_label(const struct config *cfg, const struct labelq *q, 
	const struct pos *p, const char *name, const char *sub, 
	const char *type)
{
	size_t	 	 i;
	int		 hasdef;
	const struct label *l;

	TAILQ_FOREACH(l, q, entries)
		if (l->lang == 0)
			break;

	if ( ! (hasdef = NULL != l)) 
		fprintf(stderr, "%s:%zu: %s%s%s: "
			"%s jslabel not defined\n",
			p->fname, p->line, name,
			NULL != sub ? "." : "", 
			NULL != sub ? sub : "", type);

	for (i = 1; i < cfg->langsz; i++) {
		TAILQ_FOREACH(l, q, entries)
			if (l->lang == i)
				break;
		if (NULL != l)
			continue;
		fprintf(stderr, "%s:%zu: %s%s%s: %s "
			"jslabel.%s not defined: %s\n",
			p->fname, p->line, name,
			NULL != sub ? "." : "", 
			NULL != sub ? sub : "",
			type, cfg->langs[i],
			hasdef ? "using default" :
			"using empty string");
	}
}

static void
gen_jsdoc_field(const char *ns, const struct field *f)
{

	if (FIELD_NOEXPORT & f->flags || FTYPE_BLOB == f->type)
		return;

	if (FIELD_NULL & f->flags) {
		print_commentv(2, COMMENT_JS_FRAG,
			"<li>%s-has-%s: <code>hide</code> class "
			"removed if <i>%s</i> not null, otherwise "
			"<code>hide</code> class is added</li>",
			f->parent->name, f->name, f->name);
		print_commentv(2, COMMENT_JS_FRAG,
			"<li>%s-no-%s: <code>hide</code> class "
			"added if <i>%s</i> not null, otherwise "
			"<code>hide</code> class is removed</li>",
			f->parent->name, f->name, f->name);
	} 

	if (FTYPE_STRUCT == f->type) {
		print_commentv(2, COMMENT_JS_FRAG,
			"<li>%s-%s-obj: invoke {@link "
			"%s.%s#fillInner} with %s data%s</li>",
			f->parent->name, f->name, 
			ns, f->ref->tstrct, f->name,
			FIELD_NULL & f->flags ? 
			" (if non-null)" : "");
	} else {
		print_commentv(2, COMMENT_JS_FRAG,
			"<li>%s-%s-enum-select: sets the "
			"<code>select</code> attribute for "
			"<code>&lt;option&gt;</code> values "
			"matching <i>%s</i> under the element%s</li>",
			f->parent->name, f->name, f->name,
			FIELD_NULL & f->flags ? 
			" (if non-null)" : "");
		print_commentv(2, COMMENT_JS_FRAG,
			"<li>%s-%s-text: replace contents "
			"with <i>%s</i> data%s</li>",
			f->parent->name, f->name, f->name,
			FIELD_NULL & f->flags ? 
			" (if non-null)" : "");
		print_commentv(2, COMMENT_JS_FRAG,
			"<li>%s-%s-value: replace <code>value</code> "
			"attribute with <i>%s</i> data%s</li>",
			f->parent->name, f->name, f->name,
			FIELD_NULL & f->flags ? 
			" (if non-null)" : "");
	}
}

static void
gen_js_field(const struct field *f)
{
	char	*buf = NULL;
	int	 rc;

	if (FIELD_NOEXPORT & f->flags || FTYPE_BLOB == f->type)
		return;
	if (FTYPE_STRUCT == f->type) {
		rc = asprintf(&buf, "new %s(o.%s)", 
			f->ref->tstrct, f->name);
		if (rc < 0)
			err(EXIT_FAILURE, NULL);
	}

	printf("\t\t\t_fillfield(e, '%s', '%s', custom, o.%s, "
		"inc, %s, %s, %s, %s);\n",
		f->parent->name, f->name, f->name,
		FIELD_NULL & f->flags ? "true" : "false",
		FTYPE_BLOB == f->type ? "true" : "false",
		NULL == buf ? "null" : buf,
		FTYPE_ENUM == f->type ? "true" : "false");
	free(buf);
}

/*
 * Generate variable declarations using JavaScript or TypeScript,
 * depending upon "tsc".
 * Accepts a variable number of name-type pairs terminating in a single
 * NULL.
 */
static void
gen_vars(int tsc, size_t tabs, ...)
{
	va_list	 	 ap;
	size_t		 i;
	const char	*name, *type;

	va_start(ap, tabs);
	while (NULL != (name = va_arg(ap, char *))) {
		type = va_arg(ap, char *);
		assert(NULL != type);
		for (i = 0; i < tabs; i++)
			putchar('\t');
		if (tsc) 
			printf("let %s: %s;\n", name, type);
		else
			printf("var %s;\n", name);
	}
	va_end(ap);
}

/*
 * Generate a class-level function prototype for class "cls".
 * If "priv", it is marked as private.
 * Like gen_proto(), otherwise.
 */
static void
gen_class_proto(int tsc, int priv, const char *cls, 
	const char *ret, const char *func, ...)
{
	va_list	 	 ap;
	int		 first = 1;
	const char	*name, *type;
	size_t		 sz;

	if (tsc)
		printf("\t\t%s%s(", priv ? "private " : "", func);
	else
		printf("\t\t%s.prototype.%s = function(", cls, func);
	va_start(ap, func);
	while (NULL != (name = va_arg(ap, char *))) {
		sz = strlen(name);
		if ( ! tsc && sz && '?' == name[sz - 1])
			sz--;
		if (0 == first)
			printf(", ");
		type = va_arg(ap, char *);
		assert(NULL != type);
		printf("%.*s", (int)sz, name);
		if (tsc)
			printf(": %s", type);
		if (first)
			first = 0;
	}
	va_end(ap);
	putchar(')');
	if (tsc)
		printf(": %s", ret);
	puts("\n"
	     "\t\t{");
}

static void
gen_func_static(int tsc, const char *cls, const char *name)
{

	if (tsc)
		printf("\t\tstatic %s(e: HTMLElement, "
			"name: string, "
			"val: string|null): void\n"
		      "\t\t{\n", name);
	else
		printf("\t\t%s.%s = function(e, name, val)\n"
		       "\t\t{\n", cls, name);
}

static void
gen_class_static(int tsc, const char *cls)
{

	if (tsc) 
		printf("\texport class %s {\n", cls);
	else
		printf("\tvar %s = (function()\n"
		       "\t{\n"
		       "\t\tfunction %s() { }\n",
		       cls, cls);
}

static void
gen_namespace(int tsc, const char *ns)
{

	if (tsc) 
		printf("namespace %s {\n", ns);
	else
		printf("var %s;\n"
		       "(function(%s) {\n"
		       "\t'use strict';\n"
		       "\n", ns, ns);
}

/*
 * Generate a function prototype using JavaScript or TypeScript,
 * depending upon "tsc".
 * Uses return type "ret", function name "func", and a variable number
 * of name-type pairs terminating in a single NULL.
 */
static void
gen_proto(int tsc, const char *ret, const char *func, ...)
{
	va_list	 	 ap;
	int		 first = 1;
	const char	*name, *type;

	printf("\tfunction %s(", func);
	va_start(ap, func);
	while (NULL != (name = va_arg(ap, char *))) {
		if (0 == first)
			printf(", ");
		type = va_arg(ap, char *);
		assert(NULL != type);
		printf("%s", name);
		if (tsc)
			printf(": %s", type);
		if (first)
			first = 0;
	}
	va_end(ap);
	putchar(')');
	if (tsc)
		printf(": %s", ret);
	puts("\n"
	     "\t{");
}

static void
gen_javascript(const struct config *cfg, int tsc)
{
	const struct strct  *s;
	const struct field  *f;
	const struct bitf   *bf;
	const struct bitidx *bi;
	const struct enm    *e;
	const struct eitem  *ei;
	const char	    *ns = "kwebapp";
	char		    *obj, *objarray;

	/*
	 * Begin with the methods we'll use throughout the file.
	 * All of these are scoped locally to the namespace, but not
	 * exposed outside of it.
	 * They're not documented in the file.
	 */

	print_commentv(0, COMMENT_JS,
		"Top-level namespace of these objects.\n"
		"@namespace");
	gen_namespace(tsc, ns);

	if (tsc)
		puts("\tinterface langmap { [lang: string]: string };\n"
		     "");

	gen_proto(tsc, "string", "_strlang",
		"vals", "langmap", NULL);
	gen_vars(tsc, 2, "lang", "string|null", NULL);
	puts("\t\tlang = document.documentElement.lang;\n"
	     "\t\tif (null === lang || '' === lang)\n"
	     "\t\t\treturn vals['_default'];\n"
	     "\t\telse if (lang in vals)\n"
	     "\t\t\treturn vals[lang];\n"
	     "\t\telse\n"
	     "\t\t\treturn '';\n"
	     "\t}\n"
	     "");

	gen_proto(tsc, "void", "_replcllang",
		"e", "HTMLElement|null",
		"name", "string",
		"vals", "langmap", NULL);
	puts("\t\t_replcl(e, name, _strlang(vals), false);\n"
	     "\t}\n"
	     "");

	gen_proto(tsc, "void", "_attr", 
		"e", "HTMLElement|null",
		"attr", "string",
		"text", "string", NULL);
	puts("\t\tif (null !== e)\n"
	     "\t\t\te.setAttribute(attr, text);\n"
	     "\t}\n"
	     "");

	gen_proto(tsc, "void", "_rattr", 
		"e", "HTMLElement|null",
		"attr", "string", NULL);
	puts("\t\tif (null !== e)\n"
	     "\t\t\te.removeAttribute(attr);\n"
	     "\t}\n"
	     "");

	gen_proto(tsc, "void", "_fillEnumSelect",
		"e", "HTMLElement|null",
		"val", "number|string", NULL);
	gen_vars(tsc, 2,
	     "list", "NodeListOf<Element>", 
	     "i", "number", 
	     "v", "string|number", NULL);
	printf("\t\tif (null === e)\n"
	       "\t\t\treturn;\n"
	       "\t\tlist = e.getElementsByTagName('option');\n"
	       "\t\tfor (i = 0; i < list.length; i++) {\n"
	       "\t\t\tv = 'number' === typeof val ? \n"
	       "\t\t\t     parseInt((%slist[i]).value) :\n"
	       "\t\t\t     (%slist[i]).value;\n"
	       "\t\t\tif (val === v)\n"
	       "\t\t\t\t_attr(%slist[i], 'selected', 'true');\n"
	       "\t\t\telse\n"
	       "\t\t\t\t_rattr(%slist[i], 'selected');\n"
	       "\t\t}\n"
	       "\t}\n"
	       "\n",
	       tsc ? "<HTMLOptionElement>" : "",
	       tsc ? "<HTMLOptionElement>" : "",
	       tsc ? "<HTMLOptionElement>" : "",
	       tsc ? "<HTMLOptionElement>" : "");
	
	gen_proto(tsc, "void", "_attrcl",
		"e", "HTMLElement|null",
		"attr", "string",
		"name", "string",
		"text", "string",
		"inc", "boolean", NULL);
	gen_vars(tsc, 2, "list", "HTMLElement[]", "i", "number", NULL);
	puts("\t\tif (null === e)\n"
	     "\t\t\treturn;\n"
	     "\t\tlist = _elemList(e, name, inc);\n"
	     "\t\tfor (i = 0; i < list.length; i++)\n"
	     "\t\t\t_attr(list[i], attr, text);\n"
	     "\t}\n"
	     "");

	gen_proto(tsc, "HTMLElement[]", "_elemList", 
		"e", "HTMLElement|null",
		"cls", "string",
		"inc", "boolean", NULL);
	gen_vars(tsc, 2, "a", "HTMLElement[]",
		"list", "NodeListOf<Element>",
		"i", "number", NULL);
	printf("\t\ta = [];\n"
	       "\t\tif (null === e)\n"
	       "\t\t\treturn a;\n"
	       "\t\tlist = e.getElementsByClassName(cls);\n"
	       "\t\tfor (i = 0; i < list.length; i++)\n"
	       "\t\t\ta.push(%slist[i]);\n"
	       "\t\tif (inc && e.classList.contains(cls))\n"
	       "\t\t\ta.push(e);\n"
	       "\t\treturn a;\n"
	       "\t}\n"
	       "\n", tsc ? "<HTMLElement>" : "");

	gen_proto(tsc, "void", "_repl",
		"e", "HTMLElement|null",
		"text", "string", NULL);
	puts("\t\tif (null === e)\n"
	     "\t\t\treturn;\n"
	     "\t\twhile (e.firstChild)\n"
	     "\t\t\te.removeChild(e.firstChild);\n"
	     "\t\te.appendChild(document.createTextNode(text));\n"
	     "\t}\n"
	     "");

	gen_proto(tsc, "void", "_fillfield",
		"e", "HTMLElement|null",
		"strct", "string",
		"name", "string",
		"funcs", "any",
		"obj", "any",
		"inc", "boolean",
		"cannull", "boolean",
		"isblob", "boolean",
		"sub", "any",
		"isenum", "boolean", NULL);
	gen_vars(tsc, 2, "i", "number", "fname", "string", 
		"list", "HTMLElement[]", NULL);
	puts("\t\tfname = strct + '-' + name;\n"
	     "\t\t/* First handle the custom callback. */\n"
	     "\t\tif (typeof funcs !== 'undefined' && \n"
	     "\t\t    null !== funcs && fname in funcs) {\n"
	     "\t\t\tif (funcs[fname] instanceof Array) {\n"
	     "\t\t\t\tfor (i = 0; i < funcs[fname].length; i++)\n"
	     "\t\t\t\t\tfuncs[fname][i](e, fname, obj);\n"
	     "\t\t\t} else {\n"
	     "\t\t\t\tfuncs[fname](e, fname, obj);\n"
	     "\t\t\t}\n"
	     "\t\t}\n"
	     "\t\t/* Now handle our has/no null situation. */\n"
	     "\t\tif (cannull) {\n"
	     "\t\t\tif (null === obj) {\n"
	     "\t\t\t\t_hidecl(e, strct + '-has-' + name, inc);\n"
	     "\t\t\t\t_showcl(e, strct + '-no-' + name, inc);\n"
	     "\t\t\t} else {\n"
	     "\t\t\t\t_showcl(e, strct + '-has-' + name, inc);\n"
	     "\t\t\t\t_hidecl(e, strct + '-no-' + name, inc);\n"
	     "\t\t\t}\n"
	     "\t\t}\n"
	     "\t\t/* Don't account for blobs any more. */\n"
	     "\t\tif (isblob)\n"
	     "\t\t\treturn;\n"
	     "\t\t/* Don't process null values that can be null. */\n"
	     "\t\tif (cannull && null === obj)\n"
	     "\t\t\treturn;\n"
	     "\t\t/* Non-null non-structs. */\n"
	     "\t\tif (null !== sub) {\n"
	     "\t\t\tlist = _elemList(e, fname + '-obj', inc);\n"
	     "\t\t\tfor (i = 0; i < list.length; i++) {\n"
	     "\t\t\t\tsub.fillInner(list[i], funcs);\n"
	     "\t\t\t}\n"
	     "\t\t} else {\n"
	     "\t\t\t_replcl(e, fname + '-text', obj, inc);\n"
	     "\t\t\tlist = _elemList"
	     	"(e, fname + '-enum-select', inc);\n"
	     "\t\t\tfor (i = 0; i < list.length; i++) {\n"
	     "\t\t\t\t_fillEnumSelect(list[i], obj);\n"
	     "\t\t\t}\n"
	     "\t\t\t_attrcl(e, 'value', fname + '-value', obj, inc);\n"
	     "\t\t}\n"
	     "\t}\n"
	     "");

	gen_proto(tsc, "void", "_replcl",
		"e", "HTMLElement|null",
		"name", "string",
		"text", "string",
		"inc", "boolean", NULL);
	gen_vars(tsc, 2, "list", "HTMLElement[]", "i", "number", NULL);
	puts("\t\tif (null === e)\n"
	     "\t\t\treturn;\n"
	     "\t\tlist = _elemList(e, name, inc);\n"
	     "\t\tfor (i = 0; i < list.length; i++)\n"
	     "\t\t\t_repl(list[i], text);\n"
	     "\t}\n"
	     "");

	gen_proto(tsc, "HTMLElement|null", "_classadd",
		"e", "HTMLElement|null",
		"name", "string", NULL);
	puts("\t\tif (null === e)\n"
	     "\t\t\treturn(null);\n"
	     "\t\tif ( ! e.classList.contains(name))\n"
	     "\t\t\te.classList.add(name);\n"
	     "\t\treturn(e);\n"
	     "\t}\n"
	     "");

	gen_proto(tsc, "void", "_classaddcl",
		"e", "HTMLElement|null",
		"name", "string",
		"cls", "string",
		"inc", "boolean", NULL);
	gen_vars(tsc, 2, "list", "HTMLElement[]", "i", "number", NULL);
	puts("\t\tif (null === e)\n"
	     "\t\t\treturn;\n"
	     "\t\tlist = _elemList(e, name, inc);\n"
	     "\t\tfor (i = 0; i < list.length; i++)\n"
	     "\t\t\t_classadd(list[i], cls);\n"
	     "\t}\n"
	     "");

	gen_proto(tsc, "HTMLElement|null", "_hide",
		"e", "HTMLElement|null", NULL);
	puts("\t\tif (null === e)\n"
	     "\t\t\treturn null;\n"
	     "\t\tif ( ! e.classList.contains('hide'))\n"
	     "\t\t\te.classList.add('hide');\n"
	     "\t\treturn e;\n"
	     "\t}\n"
	     "");

	gen_proto(tsc, "void", "_hidecl",
		"e", "HTMLElement|null",
		"name", "string",
		"inc", "boolean", NULL);
	gen_vars(tsc, 2, "list", "HTMLElement[]", "i", "number", NULL);
	puts("\t\tif (null === e)\n"
	     "\t\t\treturn;\n"
	     "\t\tlist = _elemList(e, name, inc);\n"
	     "\t\tfor (i = 0; i < list.length; i++)\n"
	     "\t\t\t_hide(list[i]);\n"
	     "\t}\n"
	     "");

	gen_proto(tsc, "HTMLElement|null", "_show",
		"e", "HTMLElement|null", NULL);
	puts("\t\tif (null === e)\n"
	     "\t\t\treturn null;\n"
	     "\t\tif (e.classList.contains('hide'))\n"
	     "\t\t\te.classList.remove('hide');\n"
	     "\t\treturn e;\n"
	     "\t}\n"
	     "");

	gen_proto(tsc, "void", "_showcl",
		"e", "HTMLElement|null",
		"name", "string",
		"inc", "boolean", NULL);
	gen_vars(tsc, 2, "list", "HTMLElement[]", "i", "number", NULL);
	puts("\t\tif (null === e)\n"
	     "\t\t\treturn;\n"
	     "\t\tlist = _elemList(e, name, inc);\n"
	     "\t\tfor (i = 0; i < list.length; i++)\n"
	     "\t\t\t_show(list[i]);\n"
	     "\t}\n"
	     "");

	/*
	 * If we have a TypeScript file, then define each of the JSON
	 * objects as interfaces.
	 */

	TAILQ_FOREACH(s, &cfg->sq, entries) {
		print_commentv(1, COMMENT_JS,
			"%s%s%s\n"
			"@interface %s.%sData",
			NULL == s->doc ? "" : "\n",
			NULL == s->doc ? "" : s->doc,
			NULL == s->doc ? "" : "<br />\n",
			ns, s->name);
		if ( ! tsc)
			continue;
		printf("\texport interface %sData\n"
		       "\t{\n", s->name);
		TAILQ_FOREACH(f, &s->fq, entries)
			if (FTYPE_STRUCT == f->type)
				printf("\t\t%s: %sData;\n",
					f->name, 
					f->ref->tstrct);
			else if (NULL != types[f->type])
				printf("\t\t%s: %s;\n",
					f->name,
					types[f->type]);
		puts("\t}\n");
	}

	/*
	 * Each structure is a clas initialised by either an object from
	 * the server (interface) or an array of objects.
	 * Each object has the "fill" and "fillArray" methods.
	 * These use the internal _fill method, which accepts both the
	 * object (or array) and the element to be filled.
	 */

	TAILQ_FOREACH(s, &cfg->sq, entries) {
		if (asprintf(&obj, 
		    "%s.%sData|%s.%sData[]|null", 
		    ns, s->name, ns, s->name) < 0)
			err(EXIT_FAILURE, NULL);
		if (asprintf(&objarray, "%s.%sData[]", ns, s->name) < 0)
			err(EXIT_FAILURE, NULL);
		print_commentv(1, COMMENT_JS,
			"Accepts {@link %s.%sData} for writing into "
			"a DOM tree.\n"
			"@param {(%s.%sData|%s.%sData[])} obj - The "
			"object(s) to write.\n"
			"@memberof %s\n"
			"@constructor\n"
			"@class",
			ns, s->name, ns, s->name, ns, s->name, ns);
		if (tsc)
			printf("\texport class %s {\n"
			       "\t\tobj: %sData|%sData[];\n"
			       "\t\tconstructor(o: %sData|%sData[]) {\n"
			       "\t\t\tthis.obj = o;\n"
			       "\t\t}\n"
			       "\n", s->name, s->name, 
			       s->name, s->name, s->name);
		else
			printf("\tvar %s = (function()\n"
			       "\t{\n"
			       "\t\tfunction %s(o)\n"
			       "\t\t{\n"
			       "\t\t\tthis.obj = o;\n"
			       "\t\t}\n",
			       s->name, s->name);

		print_commentv(2, COMMENT_JS_FRAG_OPEN,
			"Write the {@link %s.%sData} into the given "
			"HTMLElement in the DOM tree.\n"
			"If constructed with an array, the first "
			"element is used.\n"
			"Elements within (and including) \"e\" having "
			"the following classes are manipulated as "
			"follows:", ns, s->name);
		print_commentt(2, COMMENT_JS_FRAG, "<ul>");
		TAILQ_FOREACH(f, &s->fq, entries)
			gen_jsdoc_field(ns, f);
		print_commentt(2, COMMENT_JS_FRAG, "</ul>");
		print_commentv(2, COMMENT_JS_FRAG_CLOSE,
			"@param {HTMLElement} e - The DOM element.\n"
			"@param {Object} custom - An optional dictionary "
			"of functions keyed by structure and field "
			"name (e.g., <i>foo</i> structure, <i>bar</i> "
			"field would be <code>foo-bar</code>). "
			"The value is a function for custom "
			"handling that accepts the \"e\" value, "
			"the name of the structure-field, and the "
			"value of the structure and field.\n"
			"You may also specify an array of functions "
			"instead of a singleton.\n"
			"@function fill\n"
			"@memberof %s.%s#",
			ns, s->name);
		gen_class_proto(tsc, 0, s->name, "void", "fill",
			"e", "HTMLElement|null",
			"custom?", "any", NULL);
		printf("\t\t\tthis._fill(e, this.obj, true, custom);\n"
		       "\t\t}%s\n"
		       "\n", tsc ? "" : ";");

		print_commentv(2, COMMENT_JS,
			"Like {@link %s.%s#fill} but not "
			"including the root element \"e\".\n"
			"@param {HTMLElement} e - The DOM element.\n"
			"@param {Object} custom - The optional custom "
			"handler dictionary (see {@link "
			"%s.%s#fill} for details).\n"
			"@function fillInner\n"
			"@memberof %s.%s#",
			ns, s->name, ns, s->name, ns, s->name);
		gen_class_proto(tsc, 0, s->name, "void", "fillInner",
			"e", "HTMLElement|null",
			"custom?", "any", NULL);
		printf("\t\t\tthis._fill(e, this.obj, false, custom);\n"
		       "\t\t}%s\n"
		       "\n", tsc ? "" : ";");

		print_commentv(2, COMMENT_JS,
			"Implements all {@link %s.%s#fill} "
			"functions.\n"
			"@param {HTMLElement} e - The DOM element.\n"
			"@param {%s} o - The object "
			"(or array) to fill.\n"
			"@param {Number} inc - Whether to include "
			"the root or not when processing.\n"
			"@param {Object} custom - The optional custom "
			"handler dictionary (see {@link "
			"%s.%s#fill}).\n"
			"@private\n"
			"@function _fill\n"
			"@memberof %s.%s#",
			ns, s->name, obj, ns, s->name, ns, s->name);
		gen_class_proto(tsc, 1, s->name, "void", "_fill",
			"e", "HTMLElement|null",
			"o", obj,
			"inc", "boolean",
			"custom?", "any", NULL);
		gen_vars(tsc, 3, "i", "number", NULL);
		printf("\t\t\tif (null === o || null === e)\n"
		       "\t\t\t\treturn;\n"
		       "\t\t\tif (o instanceof Array) {\n"
		       "\t\t\t\tif (0 === o.length)\n"
		       "\t\t\t\t\treturn;\n"
		       "\t\t\t\to = o[0];\n"
		       "\t\t\t}\n"
		       "\t\t\tif (typeof custom !== 'undefined' && \n"
		       "\t\t\t    null !== custom && '%s' in custom) {\n"
		       "\t\t\t\tif (custom['%s'] instanceof Array) {\n"
		       "\t\t\t\t\tfor (i = 0; "
				      "i < custom['%s'].length; i++)\n"
		       "\t\t\t\t\t\tcustom['%s'][i](e, \"%s\", o);\n"
		       "\t\t\t\t} else {\n"
		       "\t\t\t\t\tcustom['%s'](e, \"%s\", o);\n"
		       "\t\t\t\t}\n"
		       "\t\t\t}\n",
		       s->name, s->name, s->name, s->name, 
		       s->name, s->name, s->name);
		TAILQ_FOREACH(f, &s->fq, entries)
			gen_js_field(f);
		printf("\t\t}%s\n"
		       "\n", tsc ? "" : ";");

		print_commentv(2, COMMENT_JS,
			"Like {@link %s.%s#fill} but for an "
			"array of {@link %s.%sData}.\n"
			"This will save the first element within "
			"\"e\", remove all children of \"e\", "
			"then repeatedly clone the saved element "
			"and re-append it, filling in the cloned "
			"subtree with the array (inclusive of the "
			"subtree root).\n"
			"If \"e\" is not an array, it is construed "
			"as an array of one.\n"
			"If the input array is empty, \"e\" is hidden "
			"by using the <code>hide</code> class.\n"
			"Otherwise, the <code>hide</code> class is "
			"removed.\n"
			"@param {HTMLElement} e - The DOM element.\n"
			"@param {Object} custom - The optional custom "
			"handler dictionary (see {@link "
			"%s.%s#fill}).\n"
			"@memberof %s.%s#\n"
			"@function fillArray",
			ns, s->name, ns, s->name, ns, s->name, ns, s->name);
		gen_class_proto(tsc, 0, s->name, "void", "fillArray",
			"e", "HTMLElement|null",
			"custom?", "any", NULL);
		gen_vars(tsc, 3, "j", "number", 
			"o", obj,
			"cln", "any",
			"ar", objarray,
			"row", "HTMLElement", NULL);
		printf("\t\t\to = this.obj;\n"
		       "\t\t\tif (null === o || null === e)\n"
		       "\t\t\t\treturn;\n"
		       "\t\t\tif ( ! (o instanceof Array)) {\n"
		       "\t\t\t\tar = [];\n"
		       "\t\t\t\tar.push(o);\n"
		       "\t\t\t\to = ar;\n"
		       "\t\t\t}\n"
		       "\t\t\tif (0 === o.length) {\n"
		       "\t\t\t\t_hide(e);\n"
		       "\t\t\t\treturn;\n"
		       "\t\t\t}\n"
		       "\t\t\t_show(e);\n"
		       "\t\t\trow = %se.children[0];\n"
		       "\t\t\tif (null === row)\n"
		       "\t\t\t\treturn;\n"
		       "\t\t\te.removeChild(row);\n"
		       "\t\t\twhile (null !== e.firstChild)\n"
		       "\t\t\t\te.removeChild(e.firstChild)\n"
		       "\t\t\tfor (j = 0; j < o.length; j++) {\n"
		       "\t\t\t\tcln = %srow.cloneNode(true);\n"
		       "\t\t\t\te.appendChild(cln);\n"
		       "\t\t\t\tthis._fill(cln, o[j], true, custom);\n"
		       "\t\t\t}\n"
		       "\t\t}%s\n", 
			tsc ? "<HTMLElement>" : "",
			tsc ? "<HTMLElement>" : "",
			tsc ? "" : ";");

		if ( ! tsc)
			printf("\t\treturn %s;\n", s->name);
		printf("\t}%s\n", tsc ? "" : "());");
		if ( ! tsc)
			printf("\t%s.%s = %s;\n",
				ns, s->name, s->name);
		puts("");
		free(obj);
		free(objarray);
	}

	TAILQ_FOREACH(bf, &cfg->bq, entries) {
		print_commentv(1, COMMENT_JS,
			"%s%s"
			"This defines the bit indices for the %s "
			"bit-field.\n"
			"The <code>BITI</code> fields are the bit indices "
			"(0&#8211;63) and the <code>BITF</code> fields are the "
			"masked integer values.\n"
			"All of these values are static: <strong>do "
			"not use the constructor</strong>.\n"
			"@class\n"
			"@memberof %s", 
			NULL == bf->doc ? "" : bf->doc,
			NULL == bf->doc ? "" : "<br />\n",
			bf->name, ns);
		gen_class_static(tsc, bf->name);
		TAILQ_FOREACH(bi, &bf->bq, entries) {
			print_commentv(2, COMMENT_JS,
				"%s%s"
				"This is the bit index from zero.\n"
				"@memberof %s.%s#\n"
				"@readonly\n"
				"@const {number} BITI_%s",
				NULL == bi->doc ? "" : bi->doc,
				NULL == bi->doc ? "" : "<br />\n",
				ns, bf->name, bi->name);
			print_commentv(2, COMMENT_JS,
				"%s%s"
				"This is the bit mask.\n"
				"@memberof %s.%s#\n"
				"@readonly\n"
				"@const {number} BITF_%s",
				NULL == bi->doc ? "" : bi->doc,
				NULL == bi->doc ? "" : "<br />\n",
				ns, bf->name, bi->name);
			if (tsc)
				printf("\t\tstatic readonly "
					 "BITF_%s: number;\n"
					"\t\tstatic readonly "
				      	 "BITI_%s: number;\n",
					bi->name, bi->name);
			else
				printf("\t\t%s.BITI_%s = %" PRId64 ";\n"
				       "\t\t%s.BITF_%s = %u;\n",
					bf->name, bi->name, bi->value,
					bf->name, bi->name, 1U << bi->value);
		}

		warn_label(cfg, &bf->labels_unset, &bf->pos,
			bf->name, NULL, "bits isunset");
		warn_label(cfg, &bf->labels_null, &bf->pos,
			bf->name, NULL, "bits isnull");

		print_commentv(2, COMMENT_JS,
			"Uses a bit field's <i>jslabel</i> "
			"to format a custom label as invoked on an "
			"object's <code>fill</code> functions. "
			"This will act on <code>xxx-yyy-label</code> "
			"classes, where <code>xxx</code> is the "
			"structure name and <code>yyy</code> is the "
			"field name. "
			"Multiple entries are comma-separated.\n"
			"For example, <code>xxx.fill(e, { 'xxx-yyy': "
			"%s.%s.format });</code>, where "
			"<code>yyy</code> is a field of type "
			"<i>enum %s</i>.\n"
			"@static\n"
			"@function format\n"
			"@memberof %s.%s#",
			ns, bf->name, bf->name, ns, bf->name);
		gen_func_static(tsc, bf->name, "format");
		gen_vars(tsc, 3, 
			"v", "number",
			"i", "number",
			"str", "string", NULL);
		printf("\t\t\tstr = '';\n"
		       "\t\t\ti = 0;\n"
		       "\t\t\tname += '-label';\n"
		       "\t\t\tif (null === val) {\n"
		       "\t\t\t\t_classaddcl(e, name, "
		         "\'kwbp-null\', false);\n"
		       "\t\t\t\t_replcllang(e, name, ");
		gen_labels(cfg, &bf->labels_null);
		printf(");\n"
		       "\t\t\t\treturn;\n"
		       "\t\t\t}\n"
		       "\t\t\tv = parseInt(val);\n"
		       "\t\t\tif (0 === v) {\n"
		       "\t\t\t\t_classaddcl(e, name, "
		     	"\'kwbp-unset\', false);\n"
		       "\t\t\t\t_replcllang(e, name, ");
		gen_labels(cfg, &bf->labels_unset);
		puts(");\n"
		     "\t\t\t\treturn;\n"
		     "\t\t\t}");
		TAILQ_FOREACH(bi, &bf->bq, entries) {
			warn_label(cfg, &bi->labels, &bi->pos,
				bf->name, bi->name, "item");
			printf("\t\t\tif (%s.BITF_%s & v)\n"
		       	       "\t\t\t\tstr += "
			        "(i++ > 0 ? ', ' : '') + _strlang(",
			       bf->name, bi->name);
			gen_labels(cfg, &bi->labels);
			puts(");");
		}
		printf("\t\t\tif (0 === str.length) {\n"
		       "\t\t\t\t_replcl(e, name, \'unknown\', false);\n"
		       "\t\t\t\treturn;\n"
		       "\t\t\t}\n"
		       "\t\t\t_replcl(e, name, str, false);\n"
		       "\t\t}%s\n",
		       tsc ? "" : ";");
		if ( ! tsc) 
			printf("\t\treturn %s;\n", bf->name);
		printf("\t}%s\n"
		       "\n",
		       tsc ? "" : "());");
	}

	TAILQ_FOREACH(e, &cfg->eq, entries) {
		print_commentv(1, COMMENT_JS,
			"%s%s"
			"This object consists of all values for "
			"the <i>%s</i> enumeration.\n"
			"It also contains a formatting function "
			"designed to work as a custom callback for "
			"<code>fill</code> functions.\n"
			"All of these values are static: <strong>do "
			"not use the constructor</strong>.\n"
			"@memberof %s\n"
			"@class", 
			NULL == e->doc ? "" : e->doc,
			NULL == e->doc ? "" : "<br />\n",
			e->name, ns);
		gen_class_static(tsc, e->name);

		TAILQ_FOREACH(ei, &e->eq, entries) {
			print_commentv(2, COMMENT_JS,
				"%s%s"
				"@memberof %s.%s#\n"
				"@readonly\n"
				"@const {number} %s", 
				NULL == ei->doc ? "" : ei->doc,
				NULL == ei->doc ? "" : "<br />\n",
				ns, e->name, ei->name);
			if (tsc) 
				printf("\t\tstatic readonly %s: number = %" 
					PRId64 ";\n", ei->name, 
					ei->value);
			else
				printf("\t\t%s.%s = %" PRId64 ";\n",
					e->name, ei->name, ei->value);
		}

		print_commentv(2, COMMENT_JS,
			"Uses the enumeration item's <i>jslabel</i> "
			"(or just the name, if no <i>jslabel</i> is "
			"defined) to format a custom label as "
			"invoked on an object's <code>fill</code> "
			"function. "
			"This will act on <code>xxx-yyy-label</code> "
			"classes, where <code>xxx</code> is the "
			"structure name and <code>yyy</code> is the "
			"field name. "
			"For example, <code>xxx.fill(e, { 'xxx-yyy': "
			"%s.%s.format });</code>, where "
			"<code>yyy</code> is a field of type "
			"<i>enum %s</i>.\n"
			"@static\n"
			"@function format\n"
			"@memberof %s.%s#",
			ns, e->name, e->name, ns, e->name);
		gen_func_static(tsc, e->name, "format");
		printf("\t\t\tname += '-label';\n"
		       "\t\t\tif (null === val) {\n"
		       "\t\t\t\t_replcl(e, name, \'not given\', false);\n"
		       "\t\t\t\t_classaddcl(e, name, \'noanswer\', false);\n"
		       "\t\t\t\treturn;\n"
		       "\t\t\t}\n"
		       "\t\t\tswitch(parseInt(val)) {\n");
		TAILQ_FOREACH(ei, &e->eq, entries) {
			warn_label(cfg, &ei->labels, &ei->pos,
				e->name, ei->name, "item");
			printf("\t\t\tcase %s.%s:\n"
			       "\t\t\t\t_replcllang(e, name, ",
			       e->name, ei->name);
			gen_labels(cfg, &ei->labels);
			puts(");\n"
			     "\t\t\t\tbreak;");
		}
		printf("\t\t\tdefault:\n"
		       "\t\t\t\tconsole.log(\'%s.format: "
		         "unknown value: ' + val);\n"
		       "\t\t\t\t_replcl(e, name, \'\', false);\n"
		       "\t\t\t\tbreak;\n"
		       "\t\t\t}\n"
		       "\t\t}%s\n",
		       e->name, tsc ? "" : ";");
		if ( ! tsc) 
			printf("\t\treturn %s;\n", e->name);
		printf("\t}%s\n"
		       "\n",
		       tsc ? "" : "());");
	}

	if ( ! tsc) {
		TAILQ_FOREACH(s, &cfg->sq, entries)
			printf("\t%s.%s = %s;\n", ns, s->name, s->name);
		TAILQ_FOREACH(bf, &cfg->bq, entries) 
			printf("\t%s.%s = %s;\n", ns, bf->name, bf->name);
		TAILQ_FOREACH(e, &cfg->eq, entries)
			printf("\t%s.%s = %s;\n", ns, e->name, e->name);
		printf("})(%s || (%s = {}));\n", ns, ns);
	} else
		puts("}");
}

int
main(int argc, char *argv[])
{
	FILE		*conf = NULL;
	const char	*confile = NULL;
	struct config	*cfg;
	int		 c, typescript = 0;

#if HAVE_PLEDGE
	if (-1 == pledge("stdio rpath", NULL))
		err(EXIT_FAILURE, "pledge");
#endif

	while (-1 != (c = getopt(argc, argv, "t")))
		switch (c) {
		case ('t'):
			typescript = 1;
			break;
		default:
			goto usage;
		}
	argc -= optind;
	argv += optind;

	if (0 == argc) {
		confile = "<stdin>";
		conf = stdin;
	} else
		confile = argv[0];

	if (argc > 1)
		goto usage;

	if (NULL == conf &&
	    NULL == (conf = fopen(confile, "r")))
		err(EXIT_FAILURE, "%s", confile);

#if HAVE_PLEDGE
	if (-1 == pledge("stdio", NULL))
		err(EXIT_FAILURE, "pledge");
#endif

	cfg = parse_config(conf, confile);
	fclose(conf);

	if (NULL == cfg || ! parse_link(cfg)) {
		parse_free(cfg);
		return EXIT_FAILURE;
	}

	gen_javascript(cfg, typescript);
	parse_free(cfg);
	return EXIT_SUCCESS;
usage:
	fprintf(stderr, 
		"usage: %s [-t] [config]\n",
		getprogname());
	return EXIT_FAILURE;
}
