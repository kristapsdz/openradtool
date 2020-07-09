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
#include <sys/param.h>

#include <assert.h>
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

#include "ort.h"
#include "extern.h"
#include "comments.h"
#include "paths.h"

static	const char *types[FTYPE__MAX] = {
	"number", /* FTYPE_BIT */
	"number", /* FTYPE_DATE */
	"number", /* FTYPE_EPOCH */
	"number", /* FTYPE_INT */
	"number", /* FTYPE_REAL */
	NULL, /* FTYPE_BLOB */
	"string", /* FTYPE_TEXT */
	"string", /* FTYPE_PASSWORD */
	"string", /* FTYPE_EMAIL */
	NULL, /* FTYPE_STRUCT */
	"number", /* FTYPE_ENUM */
	"number", /* FTYPE_BITFIELD */
};

static	const char *tstypes[FTYPE__MAX] = {
	"number", /* FTYPE_BIT */
	"number", /* FTYPE_DATE */
	"number", /* FTYPE_EPOCH */
	"number", /* FTYPE_INT */
	"number", /* FTYPE_REAL */
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

	while ((c = *cp++) != '\0')
		switch (c) {
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
		if (l != NULL) {
			printf("%s: \'", i == 0 ? 
				"_default" : cfg->langs[i]);
			gen_label_text(l->label);
			putchar('\'');
		} else if (i > 0 && def != NULL) {
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

	if (!(hasdef = (l != NULL)))
		fprintf(stderr, "%s:%zu: %s%s%s: "
			"%s jslabel not defined\n",
			p->fname, p->line, name,
			sub != NULL ? "." : "", 
			sub != NULL ? sub : "", type);

	for (i = 1; i < cfg->langsz; i++) {
		TAILQ_FOREACH(l, q, entries)
			if (l->lang == i)
				break;
		if (l != NULL)
			continue;
		fprintf(stderr, "%s:%zu: %s%s%s: %s "
			"jslabel.%s not defined: %s\n",
			p->fname, p->line, name,
			sub != NULL ? "." : "", 
			sub != NULL ? sub : "",
			type, cfg->langs[i],
			hasdef ? "using default" :
			"using empty string");
	}
}

static void
gen_jsdoc_field(const struct field *f)
{

	if ((f->flags & FIELD_NOEXPORT) || f->type == FTYPE_BLOB)
		return;

	if (f->flags & FIELD_NULL) {
		print_commentv(2, COMMENT_JS_FRAG,
			"- `%s-has-%s`: *hide* class removed if "
			"value is not null, otherwise it is added",
			f->parent->name, f->name);
		print_commentv(2, COMMENT_JS_FRAG,
			"- `%s-no-%s`: *hide* class added if "
			"value is not null, otherwise it is removed",
			f->parent->name, f->name);
	} 

	if (f->type == FTYPE_STRUCT) {
		print_commentv(2, COMMENT_JS_FRAG,
			"- `%s-%s-obj`: invoke {@link "
			"%s#fillInner} with **%s** data%s",
			f->parent->name, f->name, 
			f->ref->target->parent->name, f->name,
			(f->flags & FIELD_NULL) ? 
			" (if non-null)" : "");
	} else {
		print_commentv(2, COMMENT_JS_FRAG,
			"- `%s-%s-enum-select`: sets or unsets the "
			"`selected` attribute for non-inclusive "
			"descendent `<option>` elements depending on "
			"whether the value matches%s",
			f->parent->name, f->name, 
			(f->flags & FIELD_NULL) ? 
			" (if non-null)" : "");
		print_commentv(2, COMMENT_JS_FRAG,
			"- `%s-%s-value-checked`: sets or unsets "
			"the `checked` attribute depending on whether "
			"the value matches%s",
			f->parent->name, f->name, 
			(f->flags & FIELD_NULL) ? 
			" (if non-null)" : "");
		print_commentv(2, COMMENT_JS_FRAG,
			"- `%s-%s-text`: replace contents "
			"with **%s** data%s",
			f->parent->name, f->name, f->name,
			(f->flags & FIELD_NULL) ? 
			" (if non-null)" : "");
		print_commentv(2, COMMENT_JS_FRAG,
			"- `%s-%s-value`: replace `value` "
			"attribute with **%s** data%s",
			f->parent->name, f->name, f->name,
			(f->flags & FIELD_NULL) ? 
			" (if non-null)" : "");
	}

	if (f->type == FTYPE_DATE ||
	    f->type == FTYPE_EPOCH) {
		print_commentv(2, COMMENT_JS_FRAG,
			"- `%s-%s-date-value`: set the element's "
			"`value` to the ISO-8601 date format of the "
			"data%s", f->parent->name, f->name, 
			(f->flags & FIELD_NULL) ? 
			" (if non-null)" : "");
		print_commentv(2, COMMENT_JS_FRAG,
			"- `%s-%s-date-text`: like "
			"`%s-%s-date-value`, but replacing textual "
			"content", f->parent->name, f->name, 
			f->parent->name, f->name);
	}

	if (f->type == FTYPE_BIT ||
	    f->type == FTYPE_BITFIELD)
		print_commentv(2, COMMENT_JS_FRAG,
			"- `%s-%s-bits-checked`: set the `checked` "
			"attribute when the bit index of the "
			"element's `value` is set in the data as "
			"a bit-field%s",
			f->parent->name, f->name, 
			(f->flags & FIELD_NULL) ? 
			" (if non-null)" : "");
}

static void
gen_js_field(const struct field *f)
{
	char	*buf = NULL;
	int	 rc;

	if ((f->flags & FIELD_NOEXPORT) || f->type == FTYPE_BLOB)
		return;
	if (f->type == FTYPE_STRUCT) {
		rc = asprintf(&buf, "new %s(o.%s)", 
			f->ref->target->parent->name, f->name);
		if (rc == -1)
			err(EXIT_FAILURE, "asprintf");
	}

	printf("\t\t\t_fillField(e, '%s', '%s', custom, o.%s, "
		"inc, %s, %s, %s);\n",
		f->parent->name, f->name, f->name,
		(f->flags & FIELD_NULL) ? "true" : "false",
		f->type == FTYPE_BLOB ? "true" : "false",
		buf == NULL ? "null" : buf);
	free(buf);

	if (f->type == FTYPE_BIT || f->type == FTYPE_BITFIELD)
		printf("\t\t\t_fillBitsChecked"
			"(e, '%s-%s', o.%s, inc);\n",
			f->parent->name, f->name, f->name);
	if (f->type == FTYPE_DATE || f->type == FTYPE_EPOCH)
		printf("\t\t\t_fillDateValue"
			"(e, '%s-%s', o.%s, inc);\n",
			f->parent->name, f->name, f->name);
}

/*
 * Generate variable declarations.
 * Accepts a variable number of name-type pairs terminating in a single
 * NULL.
 */
static void
gen_vars(size_t tabs, ...)
{
	va_list	 	 ap;
	size_t		 i;
	const char	*name, *type;

	va_start(ap, tabs);
	while ((name = va_arg(ap, char *)) != NULL) {
		type = va_arg(ap, char *);
		assert(type != NULL);
		for (i = 0; i < tabs; i++)
			putchar('\t');
		printf("let %s: %s;\n", name, type);
	}
	va_end(ap);
}

/*
 * Generate a class-level method prototype.
 * If "priv", it is marked as private.
 * Like gen_proto(), otherwise.
 */
static void
gen_class_proto(int priv, const char *ret, const char *func, ...)
{
	va_list	 	 ap;
	int		 first = 1, rc;
	const char	*name, *type;
	size_t		 sz, col = 0;

	rc = printf("\t\t%s%s(", priv ? "private " : "", func);
	col += rc > 0 ? (size_t)rc : 0;
	va_start(ap, func);
	while ((name = va_arg(ap, char *)) != NULL) {
		if (first == 0) {
			rc = printf(", ");
			col += rc > 0 ? (size_t)rc : 0;
		}
		sz = strlen(name);
		type = va_arg(ap, char *);
		assert(type != NULL);
		if (sz + 2 + strlen(type) + col >= 72) {
			fputs("\n\t\t\t", stdout);
			col = 24;
		}
		rc = printf("%.*s", (int)sz, name);
		col += rc > 0 ? (size_t)rc : 0;
		rc =printf(": %s", type);
		col += rc > 0 ? (size_t)rc : 0;
		first = 0;
	}
	va_end(ap);
	rc = printf("): ");
	col += rc > 0 ? (size_t)rc : 0;
	if (col + strlen(ret) >= 72)
		fputs("\n\t\t\t", stdout);
	printf("%s\n", ret);
}

/*
 * Main driver function.
 * This emits the top-level structure.
 */
static void
gen_javascript(const struct config *cfg, const char *priv, int privfd)
{
	const struct strct	*s;
	const struct field	*f;
	const struct bitf	*bf;
	const struct bitidx	*bi;
	const struct enm	*e;
	const struct eitem	*ei;
	char			*obj, *objarray, *type, *typearray;
	char			 buf[BUFSIZ];
	int64_t			 maxvalue;
	ssize_t			 ssz;

	puts("namespace ort {");

	while ((ssz = read(privfd, buf, sizeof(buf))) > 0)
		fwrite(buf, 1, (size_t)ssz, stdout);
	if (ssz == -1)
		err(EXIT_FAILURE, "%s", priv);

	puts("\n"
	     "\texport type DCbstring = (e: HTMLElement,\n"
	     "\t\tname: string, val: string) => void;\n"
	     "\texport type DCbstringNull = (e: HTMLElement,\n"
	     "\t\tname: string, val: string|null) => void;\n"
	     "\texport type DCbnumber = (e: HTMLElement,\n"
	     "\t\tname: string, val: number) => void;\n"
	     "\texport type DCbnumberNull = (e: HTMLElement,\n"
	     "\t\tname: string, val: number|null) => void;");

	TAILQ_FOREACH(s, &cfg->sq, entries)
		printf("\texport type DCbStruct%s = (e: HTMLElement,\n"
		       "\t\tname: string, val: ort.%sData|null) "
			"=> void;\n", s->name, s->name);

	puts("");
	print_commentt(1, COMMENT_JS,
		"All possible custom callbacks for this "
		"ort configuration.");
	puts("\texport interface DataCallbacks\n"
	     "\t{\n"
	     "\t\t[key: string]: any;");
	TAILQ_FOREACH(s, &cfg->sq, entries) {
		printf("\t\t'%s'?: DCbStruct%s|DCbStruct%s[];\n",
			s->name, s->name, s->name);
		TAILQ_FOREACH(f, &s->fq, entries) {
			if (FTYPE_STRUCT == f->type) {
				printf("\t\t\'%s-%s\'?: "
					"DCbStruct%s|DCbStruct%s[];\n",
					s->name, f->name, 
					f->ref->target->parent->name, 
					f->ref->target->parent->name);
				continue;
			} else if (tstypes[f->type] == NULL)
				continue;

			printf("\t\t'%s-%s'?: DCb%s%s|"
				"DCb%s%s[];\n", s->name, 
				f->name, tstypes[f->type], 
				(f->flags & FIELD_NULL) ? 
				"Null" : "", 
				tstypes[f->type],
				(f->flags & FIELD_NULL) ? 
				"Null" : "");
		}
	}
	puts("\t}\n");

	/*
	 * If we have a TypeScript file, then define each of the JSON
	 * objects as interfaces.
	 */

	TAILQ_FOREACH(s, &cfg->sq, entries) {
		if (s->doc != NULL)
			print_commentt(1, COMMENT_JS, s->doc);
		printf("\texport interface %sData\n"
		       "\t{\n", s->name);
		TAILQ_FOREACH(f, &s->fq, entries) {
			if (f->doc != NULL &&
			    (f->type == FTYPE_STRUCT ||
			     types[f->type] != NULL))
				print_commentt(2, COMMENT_JS, f->doc);
			if (f->type == FTYPE_STRUCT)
				printf("\t\t%s: %sData;\n",
					f->name, 
					f->ref->target->parent->name);
			else if (types[f->type] != NULL)
				printf("\t\t%s: %s;\n",
					f->name,
					types[f->type]);
		}
		puts("\t}\n");
	}

	/* Generate classes for each structure. */

	TAILQ_FOREACH(s, &cfg->sq, entries) {
		if (asprintf(&obj, 
		    "%sData|%sData[]|null", s->name, s->name) < 0)
			err(EXIT_FAILURE, NULL);
		if (asprintf(&objarray, "%sData[]", s->name) < 0)
			err(EXIT_FAILURE, NULL);
		if (asprintf(&type, "<DCbStruct%s>", s->name) < 0)
			err(EXIT_FAILURE, NULL);
		if (asprintf(&typearray, "<DCbStruct%s[]>", s->name) < 0)
			err(EXIT_FAILURE, NULL);

		print_commentv(1, COMMENT_JS,
			"Writes {@link %sData} into a DOM tree.",
			s->name);
		printf("\texport class %s {\n"
		       "\t\tobj: %sData|%sData[];\n",
		       s->name, s->name, s->name);

		/* Constructor. */

		print_commentt(2, COMMENT_JS,
			"@param obj The object(s) to write.");
		printf("\t\tconstructor(o: %sData|%sData[]) {\n"
		       "\t\t\tthis.obj = o;\n"
		       "\t\t}\n\n", s->name, s->name);

		/* fill() method. */

		print_commentv(2, COMMENT_JS_FRAG_OPEN,
			"Writes {@link %sData} into the given "
			"element. If constructed with an array, the "
			"first element is used.  Elements within (and "
			"including) the element having the following "
			"classes are manipulated as follows:", s->name);
		print_commentt(2, COMMENT_JS_FRAG, "");
		TAILQ_FOREACH(f, &s->fq, entries)
			gen_jsdoc_field(f);
		print_commentt(2, COMMENT_JS_FRAG, "");
		print_commentt(2, COMMENT_JS_FRAG_CLOSE,
			"@param e The DOM element.\n"
			"@param custom The dictionary of functions "
			"keyed by structure and field name (e.g., "
			"*foo** structure, **bar** field would be "
			"`foo-bar`). The value is a function for "
			"custom handling that accepts the \'e\' value, "
			"the name of the structure-field, and the "
			"value of the structure and field. "
			"You may also specify an array of functions "
			"instead of a singleton. "
			"These callbacks are invoked *after* "
			"the generic classes are filled.");
		gen_class_proto(0, "void", "fill",
			"e", "HTMLElement|null", 
			"custom?", "DataCallbacks|null", NULL);
		puts("\t\t{\n"
		     "\t\t\tthis._fill(e, this.obj, true, custom);\n"
		     "\t\t}\n");

		/* fillInner() method. */

		print_commentt(2, COMMENT_JS,
			"Like {@link fill} but not including the "
			"passed-in element.\n"
			"@param e The DOM element.\n"
			"@param custom Custom handler dictionary (see "
			"{@link fill} for details).");
		gen_class_proto(0, "void", "fillInner",
			"e", "HTMLElement|null", 
			"custom?", "DataCallbacks|null", NULL);
		puts("\t\t{\n"
		     "\t\t\tthis._fill(e, this.obj, false, custom);\n"
		     "\t\t}\n");

		/* fillByClass() method. */

		print_commentt(2, COMMENT_JS,
			"Like {@link fill} but instead of accepting a "
			"single element to fill, filling into all "
			"elements (inclusive) matching the given "
			"class name beneath (inclusive) the element.\n"
			"@param e The DOM element.\n"
			"@param name Name of the class to fill.\n"
			"@param custom Custom handler dictionary (see "
			"{@link fill} for details).");
		gen_class_proto(0, "void", "fillByClass",
			"e", "HTMLElement|null", 
			"name", "string",
			"custom?", "DataCallbacks|null", NULL);
		puts("\t\t{\n"
		     "\t\t\tthis._fillByClass(e, name, true, custom);\n"
		     "\t\t}\n");

		/* fillInnerByClass() method. */

		print_commentt(2, COMMENT_JS,
			"Like {@link fillByClass} but not inclusive "
			"the root element and class matches.\n"
			"@param e The DOM element.\n"
			"@param name Name of the class to fill.\n"
			"@param custom Custom handler dictionary (see "
			"{@link fill} for details).");
		gen_class_proto(0, "void", "fillInnerByClass",
			"e", "HTMLElement|null", 
			"name", "string",
			"custom?", "DataCallbacks|null", NULL);
		puts("\t\t{\n"
		     "\t\t\tthis._fillByClass(e, name, false, custom);\n"
		     "\t\t}\n");

		/* _fill() private method. */

		gen_class_proto(1, "void", "_fill",
			"e", "HTMLElement|null", 
			"o", obj,
			"inc", "boolean",
			"custom?", "DataCallbacks|null", NULL);
		puts("\t\t{\n"
		     "\t\t\tlet i: number;\n"
		     "\t\t\tif (null === o || null === e)\n"
		     "\t\t\t\treturn;\n"
		     "\t\t\tif (o instanceof Array) {\n"
		     "\t\t\t\tif (0 === o.length)\n"
		     "\t\t\t\t\treturn;\n"
		     "\t\t\t\to = o[0];\n"
		     "\t\t\t}\n"
		     "\t\t\tif (typeof custom === 'undefined')\n"
		     "\t\t\t\tcustom = null;");
		TAILQ_FOREACH(f, &s->fq, entries)
			gen_js_field(f);
		printf("\t\t\tif (null !== custom && '%s' in custom) {\n"
		       "\t\t\t\tif (custom['%s'] instanceof Array) {\n"
		       "\t\t\t\t\tfor (i = 0; "
				      "i < custom['%s']!.length; i++)\n"
		       "\t\t\t\t\t\t(%scustom['%s'])[i](e, '%s', o);\n"
		       "\t\t\t\t} else {\n"
		       "\t\t\t\t\t(%scustom['%s'])(e, '%s', o);\n"
		       "\t\t\t\t}\n"
		       "\t\t\t}\n"
		       "\t\t}\n"
		       "\n", s->name, s->name, s->name, typearray, 
		       s->name, s->name, type, s->name, s->name);

		/* _fillByClass() private method. */

		gen_class_proto(1, "void", "_fillByClass",
			"e", "HTMLElement|null", 
			"name", "string",
			"inc", "boolean",
			"custom?", "DataCallbacks|null", NULL);
		puts("\t\t{\n"
		     "\t\t\tlet i: number;\n"
		     "\t\t\tconst list: HTMLElement[] = \n"
		     "\t\t\t\t_elemList(e, name, inc);\n"
	     	     "\t\t\tfor (i = 0; i < list.length; i++)\n"
		     "\t\t\t\tthis._fill(list[i], this.obj, "
		     	"inc, custom);\n"
		     "\t\t}\n");

		/* fillArrayOrHide() method. */

		print_commentt(2, COMMENT_JS,
			"Like {@link fillArray}, but hiding an "
			"element if the array is empty or null.\n"
			"@param e The DOM element.\n"
			"@param tohide DOM element to hide.\n"
			"@param o The array (or object) to fill.\n"
			"@param custom Custom handler dictionary (see "
			"{@link fill}).");
		gen_class_proto(0, "void", "fillArrayOrHide", 
			"e", "HTMLElement|null",
			"tohide", "HTMLElement|null",
			"custom?", "DataCallbacks", NULL);
		puts("\t\t{\n"
		     "\t\t\tlet len: number;\n"
		     "\t\t\tif (null === this.obj)\n"
		     "\t\t\t\tlen = 0;\n"
		     "\t\t\telse if (this.obj instanceof Array)\n"
		     "\t\t\t\tlen = this.obj.length;\n"
		     "\t\t\telse\n"
		     "\t\t\t\tlen = 1;\n"
		     "\t\t\tif (null !== e)\n"
		     "\t\t\t\t_hide(e);\n"
		     "\t\t\tif (null !== tohide)\n"
		     "\t\t\t\t_show(tohide);\n"
		     "\t\t\tthis.fillArray(e, custom);\n"
		     "\t\t\tif (null !== tohide && 0 === len)\n"
		     "\t\t\t\t_hide(tohide);\n"
		     "\t\t}\n");

		/* fillArrayOrShow() method. */

		print_commentt(2, COMMENT_JS,
			"Like {@link fillArray}, but showing an "
			"element if the array is empty or null.\n"
			"@param e The DOM element.\n"
			"@param toshow The DOM element to show.\n"
			"@param o The array or object to fill.\n"
			"@param custom Custom handler dictionary (see "
			"{@link fill}).");
		gen_class_proto(0, "void", "fillArrayOrShow", 
			"e", "HTMLElement|null",
			"toshow", "HTMLElement|null",
			"custom?", "DataCallbacks", NULL);
		puts("\t\t{\n"
		     "\t\t\tlet len: number;\n"
		     "\t\t\tif (null === this.obj)\n"
		     "\t\t\t\tlen = 0;\n"
		     "\t\t\telse if (this.obj instanceof Array)\n"
		     "\t\t\t\tlen = this.obj.length;\n"
		     "\t\t\telse\n"
		     "\t\t\t\tlen = 1;\n"
		     "\t\t\tif (null !== e)\n"
		     "\t\t\t\t_hide(e);\n"
		     "\t\t\tif (null !== toshow)\n"
		     "\t\t\t\t_hide(toshow);\n"
		     "\t\t\tthis.fillArray(e, custom);\n"
		     "\t\t\tif (null !== toshow && 0 === len)\n"
		     "\t\t\t\t_show(toshow);\n"
		     "\t\t}\n");

		/* fillArray() method. */

		print_commentt(2, COMMENT_JS,
			"Like {@link fill} but for an array. If the "
			"data is not an array, it is remapped as an "
			"array of one. This will save the first "
			"element within \'e\', remove all children of "
			"\'e\', then repeatedly clone the saved "
			"element and re-append it, filling in the "
			"cloned subtree with the array (inclusive "
			"of the subtree root). If the input array is "
			"empty or null, \'e\' is hidden by using the "
			"*hide* class. Otherwise, the *hide* class is "
			"removed.\n"
			"@param e The DOM element.\n"
			"@param custom Custom handler dictionary (see "
			"{@link fill}).");
		gen_class_proto(0, "void", "fillArray", 
			"e", "HTMLElement|null",
			"custom?", "DataCallbacks", NULL);
		puts("\t\t{\n"
		     "\t\t\tlet j: number;\n"
		     "\t\t\tlet cln: HTMLElement;\n"
		     "\t\t\tlet row: HTMLElement;");
		printf("\t\t\tlet o: %s;\n"
	    	       "\t\t\tlet ar: %s;\n", obj, objarray);
		puts("\t\t\to = this.obj;\n"
		     "\t\t\tif (null !== e)\n"
		     "\t\t\t\t_hide(e);\n"
		     "\t\t\tif (null === o || null === e)\n"
		     "\t\t\t\treturn;\n"
		     "\t\t\tif ( ! (o instanceof Array)) {\n"
		     "\t\t\t\tar = [];\n"
		     "\t\t\t\tar.push(o);\n"
		     "\t\t\t\to = ar;\n"
		     "\t\t\t}\n"
		     "\t\t\tif (0 === o.length)\n"
		     "\t\t\t\treturn;\n"
		     "\t\t\t_show(e);\n"
		     "\t\t\trow = <HTMLElement>e.children[0];\n"
		     "\t\t\tif (null === row)\n"
		     "\t\t\t\treturn;\n"
		     "\t\t\te.removeChild(row);\n"
		     "\t\t\twhile (null !== e.firstChild)\n"
		     "\t\t\t\te.removeChild(e.firstChild)\n"
		     "\t\t\tfor (j = 0; j < o.length; j++) {\n"
		     "\t\t\t\tcln = <HTMLElement>row.cloneNode(true);\n"
		     "\t\t\t\te.appendChild(cln);\n"
		     "\t\t\t\tthis._fill(cln, o[j], true, custom);\n"
		     "\t\t\t}\n"
		     "\t\t}\n");

		/* fillArrayByClass() method. */

		print_commentt(2, COMMENT_JS,
			"Like {@link fillArray} but instead of "
			"accepting a single element to fill, filling "
			"all elements by class name beneath the "
			"given root (non-inclusive).\n"
			"@param e The DOM element.\n"
			"@param name Name of the class to fill.\n"
			"@param custom Custom handler dictionary (see "
			"{@link fill} for details).");
		gen_class_proto(0, "void", "fillArrayByClass", 
			"e", "HTMLElement|null",
			"name", "string",
			"custom?", "DataCallbacks", NULL);
		puts("\t\t{\n"
		     "\t\t\tlet i: number;\n"
		     "\t\t\tconst list: HTMLElement[] =\n"
		     "\t\t\t\t_elemList(e, name, false);\n"
	     	     "\t\t\tfor (i = 0; i < list.length; i++)\n"
		     "\t\t\t\tthis.fillArray(list[i], custom);\n"
		     "\t\t}\n\n"
		     "\t}\n");

		free(obj);
		free(objarray);
		free(type);
		free(typearray);
	}

	TAILQ_FOREACH(bf, &cfg->bq, entries) {
		print_commentv(1, COMMENT_JS,
			"%s%s"
			"This defines the bit indices for the %s "
			"bit-field.\n"
			"The `BITI` fields are the bit indices "
			"(0&#8211;63) and the `BITF` fields are the "
			"masked integer values.\n"
			"All of these values are static: **do "
			"not use the constructor**.",
			NULL == bf->doc ? "" : bf->doc,
			NULL == bf->doc ? "" : "<br/>\n",
			bf->name);
		printf("\texport class %s {\n", bf->name);
		maxvalue = -INT64_MAX;
		TAILQ_FOREACH(bi, &bf->bq, entries) {
			if (bi->doc != NULL)
				print_commentt(2, COMMENT_JS, bi->doc);
			printf("\t\tstatic readonly "
				 "BITF_%s: number = %u;\n"
				"\t\tstatic readonly "
			      	 "BITI_%s: number = %" PRId64 ";\n",
				bi->name, 1U << bi->value,
				bi->name, bi->value);
			if (bi->value > maxvalue)
				maxvalue = bi->value;
		}

		/* Now the maximum enumeration value. */

		print_commentt(2, COMMENT_JS,
			"One larger than the largest enumeration index.");
		printf("\t\tstatic readonly "
			"BITI__MAX: number = %" PRId64 ";\n", 
			maxvalue + 1);

		warn_label(cfg, &bf->labels_unset, &bf->pos,
			bf->name, NULL, "bits isunset");
		warn_label(cfg, &bf->labels_null, &bf->pos,
			bf->name, NULL, "bits isnull");

		print_commentv(2, COMMENT_JS,
			"Uses a bit field's **jslabel** "
			"to format a custom label as invoked on an "
			"object's `fill` functions. "
			"This will act on *xxx-yyy-label* "
			"classes, where *xxx* is the "
			"structure name and *yyy* is the "
			"field name. "
			"Multiple entries are comma-separated.\n"
			"For example, `xxx.fill(e, { 'xxx-yyy': "
			"ort.%s.format });`, where "
			"*yyy* is a field of type "
			"**enum %s**.\n"
			"@param e The DOM element.\n"
			"@param name If non-null, data is written to "
			"elements under the root with the given class "
			"name. Otherwise, data is written directly "
			"into the DOM element.\n"
			"@param v The bitfield.",
			bf->name, bf->name);
		puts("\t\tstatic format(e: HTMLElement, name: string|null, "
			"v: number|null): void\n"
		      "\t\t{");
		gen_vars(3, 
			"i", "number",
			"s", "string", NULL);
		printf("\t\t\ts = '';\n"
		       "\t\t\ti = 0;\n"
		       "\t\t\tif (name !== null)\n"
		       "\t\t\t\tname += '-label';\n"
		       "\t\t\tif (v === null && name !== null) {\n"
		       "\t\t\t\t_classaddcl(e, name, "
		       	"\'ort-null\', false);\n"
		       "\t\t\t\t_replcllang(e, name, ");
		gen_labels(cfg, &bf->labels_null);
		printf(");\n"
		       "\t\t\t\treturn;\n"
		       "\t\t\t} else if (v === null) {\n"
		       "\t\t\t\t_classadd(e, \'ort-null\');\n"
		       "\t\t\t\t_repllang(e, ");
		gen_labels(cfg, &bf->labels_null);
		printf(");\n"
		       "\t\t\t\treturn;\n"
		       "\t\t\t} else if (v === 0 && name !== null) {\n"
		       "\t\t\t\t_classaddcl(e, name, "
		     	"\'ort-unset\', false);\n"
		       "\t\t\t\t_replcllang(e, name, ");
		gen_labels(cfg, &bf->labels_unset);
		printf(");\n"
		       "\t\t\t\treturn;\n"
		       "\t\t\t} else if (v === 0) {\n"
		       "\t\t\t\t_classadd(e, \'ort-unset\');\n"
		       "\t\t\t\t_repllang(e, ");
		gen_labels(cfg, &bf->labels_unset);
		puts(");\n"
		     "\t\t\t\treturn;\n"
		     "\t\t\t}");
		TAILQ_FOREACH(bi, &bf->bq, entries) {
			warn_label(cfg, &bi->labels, &bi->pos,
				bf->name, bi->name, "item");
			printf("\t\t\tif ((v & %s.BITF_%s))\n"
		       	       "\t\t\t\ts += (i++ > 0 ? ', ' : '') +\n"
			       "\t\t\t\t  _strlang(", 
			       bf->name, bi->name);
			gen_labels(cfg, &bi->labels);
			puts(");");
		}
		puts("\t\t\tif (s.length === 0 && name !== null) {\n"
		     "\t\t\t\t_replcl(e, name, \'unknown\', false);\n"
		     "\t\t\t\treturn;\n"
		     "\t\t\t} else if (s.length === 0) { \n"
		     "\t\t\t\t_repl(e, \'unknown\');\n"
		     "\t\t\t\treturn;\n"
		     "\t\t\t}\n"
		     "\t\t\tif (name !== null)\n"
		     "\t\t\t\t_replcl(e, name, s, false);\n"
		     "\t\t\telse\n"
		     "\t\t\t\t_repl(e, s);\n"
		     "\t\t}\n"
		     "\t}\n");
	}

	TAILQ_FOREACH(e, &cfg->eq, entries) {
		if (e->doc != NULL)
			print_commentt(1, COMMENT_JS, e->doc);
		printf("\texport class %s {\n", e->name);
		TAILQ_FOREACH(ei, &e->eq, entries) {
			if (ei->doc != NULL)
				print_commentt(2, COMMENT_JS, ei->doc);
			printf("\t\tstatic readonly %s: number = %" 
				PRId64 ";\n", ei->name, 
				ei->value);
		}

		print_commentt(2, COMMENT_JS,
			"Uses the enumeration item's **jslabel** " 
			"(or an empty string if no **jslabel** is " 
			"defined or there is no matching item "
			"for the value) to format a custom label as "
			"invoked on an object's `fill` method. "
			"This will act on *xxx-yyy-label* classes, "
			"where *xxx* is the structure name and "
			"*yyy* is the field name.\n"
			"@param e The DOM element.\n"
			"@param name If non-null, data is written "
			"to elements under the root with the given "
			"class name. If null, data is written "
			"directly into the DOM element.\n"
			"@param v The enumeration value.");
		puts("\t\tstatic format(e: HTMLElement, name: string|null, "
			"v: number|null): void\n"
		      "\t\t{");
		gen_vars(3, "s", "string", NULL);
		printf("\t\t\tif (name !== null)\n"
		       "\t\t\t\tname += '-label';\n"
		       "\t\t\tif (v === null && name !== null) {\n"
		       "\t\t\t\t_replcl(e, name, \'not given\', false);\n"
		       "\t\t\t\t_classaddcl(e, name, \'noanswer\', false);\n"
		       "\t\t\t\treturn;\n"
		       "\t\t\t} else if (v === null) {\n"
		       "\t\t\t\t_repl(e, \'not given\');\n"
		       "\t\t\t\t_classadd(e, \'noanswer\');\n"
		       "\t\t\t\treturn;\n"
		       "\t\t\t}\n"
		       "\t\t\tswitch(v) {\n");
		TAILQ_FOREACH(ei, &e->eq, entries) {
			warn_label(cfg, &ei->labels, &ei->pos,
				e->name, ei->name, "item");
			printf("\t\t\tcase %s.%s:\n"
			       "\t\t\t\ts = _strlang(",
			       e->name, ei->name);
			gen_labels(cfg, &ei->labels);
			puts(");\n"
			     "\t\t\t\tbreak;");
		}
		printf("\t\t\tdefault:\n"
		       "\t\t\t\tconsole.log(\'%s.format: "
		         "unknown value: ' + v);\n"
		       "\t\t\t\ts = '';\n"
		       "\t\t\t\tbreak;\n"
		       "\t\t\t}\n"
		       "\t\t\tif (name !== null)\n"
		       "\t\t\t\t_replcl(e, name, s, false);\n"
		       "\t\t\telse\n"
		       "\t\t\t\t_repl(e, s);\n"
		       "\t\t}\n"
		       "\t}\n"
		       "\n", e->name);
	}

	puts("}");
}

int
main(int argc, char *argv[])
{
	const char	 *sharedir = SHAREDIR;
	char		  buf[MAXPATHLEN];
	struct config	 *cfg = NULL;
	int		  c, rc = 0, priv;
	FILE		**confs = NULL;
	size_t		  i, confsz;
	ssize_t		  sz;

#if HAVE_PLEDGE
	if (pledge("stdio rpath", NULL) == -1)
		err(EXIT_FAILURE, "pledge");
#endif

	while ((c = getopt(argc, argv, "S:t")) != -1)
		switch (c) {
		case 'S':
			sharedir = optarg;
			break;
		case ('t'):
			/* Ignored. */
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
			err(EXIT_FAILURE, "calloc");
		for (i = 0; i < confsz; i++) {
			confs[i] = fopen(argv[i], "r");
			if (confs[i] == NULL)
				err(EXIT_FAILURE, "%s: open", argv[i]);
		}
	}

	/* Read our private namespace. */

	sz = snprintf(buf, sizeof(buf), 
		"%s/ortPrivate.ts", sharedir);
	if (sz == -1 || (size_t)sz > sizeof(buf))
		errx(EXIT_FAILURE, "%s: too long", sharedir);
	if ((priv = open(buf, O_RDONLY, 0)) == -1)
		err(EXIT_FAILURE, "%s", buf);

#if HAVE_PLEDGE
	if (pledge("stdio", NULL) == -1)
		err(EXIT_FAILURE, "pledge");
#endif

	if ((cfg = ort_config_alloc()) == NULL)
		goto out;

	for (i = 0; i < confsz; i++)
		if (!ort_parse_file(cfg, confs[i], argv[i]))
			goto out;

	if (confsz == 0 && !ort_parse_file(cfg, stdin, "<stdin>"))
		goto out;

	if ((rc = ort_parse_close(cfg)))
		gen_javascript(cfg, buf, priv);

out:
	for (i = 0; i < confsz; i++)
		if (fclose(confs[i]) == EOF)
			warn("%s", argv[i]);

	close(priv);
	free(confs);
	ort_config_free(cfg);
	return rc ? EXIT_SUCCESS : EXIT_FAILURE;
usage:
	fprintf(stderr, "usage: %s [-S sharedir] [config...]\n", 
		getprogname());
	return EXIT_FAILURE;
}
