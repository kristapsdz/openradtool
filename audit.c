/*	$Id$ */
/*
 * Copyright (c) 2017--2018 Kristaps Dzonsons <kristaps@bsd.lv>
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
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "extern.h"

#define	SPACE	"\t" /* Space for output indentation. */

enum	op {
	OP_AUDIT,
	OP_AUDIT_GV,
	OP_AUDIT_JSON
};

/*
 * Search single access.
 * This keeps track of an origin function and the structure stack
 * required to get to the structure.
 */
struct	srsaccess {
	const struct search *orig; /* original query */
	const struct field **fs; /* array of fields til here */
	size_t		 fsz; /* fields in "fs" */
	int		 exported; /* whether exportable (inherit) */
};

/*
 * Use this for keeping track of how we access any given structure.
 * For each structure "p" we can access, we fill in the origin (how we
 * got there).
 */
struct	sraccess {
	const struct strct *p; /* structure that we can access */
	struct srsaccess *origs; /* query origin (read event) */
	size_t origsz; /* number of query origins */
};

struct	fieldstack {
	const struct field **f;
	size_t		cur;
	size_t		max;
};

/*
 * Recursive check to see whether a given role "role" can access a
 * resource marked with "r".
 * To do so, we walk to the root of the role tree from "root", checking
 * to see whether "r" is along the way.
 */
static int
check_role(const struct role *r, const struct role *role)
{
	if (NULL == role)
		return(0);
	if (r == role)
		return(1);
	return(check_role(r, role->parent));
}

/*
 * Check to see whether the rolemap, which contains all roles allowed to
 * perform the operation, contains our role.
 * In other words, "r" may contain "bar" and "foo"; and "role" may
 * consist of "baz" which extends from "foo".
 * This would be allowed, because "foo" covers the role "baz".
 * In the extreme case, "all" would catch everything, because every role
 * is a subset of "all" except for "none" and "default".
 * Returns zero if it does not contain our role; non-zero otherwise.
 */
static int
check_rolemap(const struct rolemap *r, const struct role *role)
{
	const struct roleset *rs;

	TAILQ_FOREACH(rs, &r->setq, entries)
		if (check_role(rs->role, role))
			return(1);

	return(0);
}

/*
 * Return zero if the given field isn't exported for the given role
 * where the structure is marked "exportable".
 * Otherwise, it is.
 */
static int
check_field_exported(const struct field *f, 
	const struct role *role, int exportable)
{
	int	 export;

	/* 
	 * By default, we're exporting.
	 * Inhibit that if we're marked as no-export on the
	 * field itself.
	 */

	export = FTYPE_PASSWORD != f->type &&
		! (FIELD_NOEXPORT & f->flags);

	/*
	 * If the structure isn't reachable (i.e., no paths to this
	 * structure that are in the exporting role), then override its
	 * exportability.
	 */

	if ( ! exportable)
		export = 0;

	/*
	 * If the local structure inhibits our exportability, then mark
	 * it as such.
	 */

	if (NULL != f->rolemap && check_rolemap(f->rolemap, role))
		export = 0;

	return(export);
}

/*
 * See if any role at or descending from "r" matches the named "role".
 * Returns the pointer to the matching role or NULL otherwise.
 */
static const struct role *
check_role_exists_r(const struct role *r, const char *role)
{
	const struct role *rq, *ret;

	if (0 == strcasecmp(r->name, role))
		return(r);

	TAILQ_FOREACH(rq, &r->subrq, entries)
		if (NULL != (ret = check_role_exists_r(rq, role)))
			return(ret);

	return(NULL);
}

/*
 * See if the given role "role" exists in "rq".
 * Return NULL if it does not, the pointer otherwise.
 */
static const struct role *
check_role_exists(const struct roleq *rq, const char *role)
{
	const struct role *r, *rr;

	/*
	 * Begin by checking to see if the role exists at all.
	 * For this, we need to walk through all roles as they're
	 * arranged in the role tree.
	 */

	TAILQ_FOREACH(rr, rq, entries)
		if (NULL != (r = check_role_exists_r(rr, role)))
			return(r);

	return(NULL);
}


/*
 * Invoked for all structures that are reachable by the role.
 */
static void
gen_audit_exportable(const struct strct *p, 
	const struct sraccess *ac, int json, 
	const struct role *role)
{
	const struct field *f;
	size_t	 i, j;
	int	 export, exportable = 0;

	/*
	 * If this structure is exportable, that means that at least one
	 * route to the structure (i.e., search path) has allowed for
	 * export functionality.
	 * This is the default---the "noexport" overrides it.
	 */

	for (i = 0; 0 == exportable && i < ac->origsz; i++) 
		if (ac->origs[i].exported)
			exportable = 1;

	if (json) 
		printf("\t\t\t\"exportable\": %s,\n"
		       "\t\t\t\"data\": [\n", 
		       exportable ? "true" : "false");
	else 
		printf("%sdata:\n", SPACE);

	TAILQ_FOREACH(f, &p->fq, entries) {
		export = check_field_exported(f, role, exportable);

		if (json)
			printf("\t\t\t\t\"%s\"%s\n", f->name, 
			       NULL != TAILQ_NEXT(f, entries) ?
			       "," : "");
		else 
			printf("%s%s%s%s%s\n", SPACE, SPACE, f->name,
				export ? "" : ": NOT EXPORTED",
				exportable ? "" : " (BY INHERITENCE)");
	}

	if (json)
		puts("\t\t\t],\n"
		     "\t\t\t\"accessfrom\": [");
	else 
		printf("%saccessed from:\n", SPACE);

	for (i = 0; i < ac->origsz; i++) {
		if (json) 
			printf("\t\t\t\t{ \"function\": \"");
		else 
			printf("%s%s", SPACE, SPACE);

		print_name_db_search(ac->origs[i].orig);

		if (json) 
			printf("\",\n"
			       "\t\t\t\t  \"exporting\": %s,\n"
			       "\t\t\t\t  \"path\": [",
			       ac->origs[i].exported ? "true" : "false");
		else 
			printf(": ");

		for (j = 0; j < ac->origs[i].fsz; j++) {
			if (j > 0 && json)
				printf(", ");
			else if (j > 0)
				putchar('.');
			if (json)
				putchar('"');
			printf("%s", ac->origs[i].fs[j]->parent->name);
			putchar('.');
			printf("%s", ac->origs[i].fs[j]->name);
			if (json)
				putchar('"');
		}

		if ( ! json) {
			if (j > 0)
				printf(": ");
			if (ac->origs[i].exported)
				printf("exporting, ");
			if (0 == j)
				puts("self-reference");
			else
				puts("foreign-reference");
		} else 
			printf("] }%s\n", 
				i < ac->origsz - 1 ? "," : "");
	}

	if (json) 
		puts("\t\t\t],");
}

static void
print_doc(const char *cp)
{
	char	 c;

	if (NULL == cp) {
		printf("null");
		return;
	}

	putchar('"');

	while ('\0' != (c = *cp++))
		switch (c) {
		case ('"'):
		case ('\\'):
		case ('/'):
			putchar('\\');
			putchar(c);
			break;
		case ('\b'):
			fputs("\\b", stdout);
			break;
		case ('\f'):
			fputs("\\f", stdout);
			break;
		case ('\n'):
			fputs("\\n", stdout);
			break;
		case ('\r'):
			fputs("\\r", stdout);
			break;
		case ('\t'):
			fputs("\\t", stdout);
			break;
		default:
			putchar(c);
			break;
		}

	putchar('"');
}

static void
gen_audit_inserts(const struct strct *p, 
	int json, const struct role *role)
{

	if (json)
		printf("\t\t\t\"insert\": ");
	else
		printf("%sinsert:\n", SPACE);
	if (NULL != p->ins &&
	    NULL != p->ins->rolemap && 
	    check_rolemap(p->ins->rolemap, role)) {
		if (json)
			putchar('"');
		else
			printf("%s%s", SPACE, SPACE);
		print_name_db_insert(p);
		if (json)
			puts("\",");
		else
			puts("");
	} else if (json)
		puts("null,");
}

static void
gen_audit_deletes(const struct strct *p, 
	int json, const struct role *role)
{
	const struct update *u;
	int	 first = 1;

	if (json)
		printf("\t\t\t\"deletes\": ");
	else
		printf("%sdeletes:\n", SPACE);

	TAILQ_FOREACH(u, &p->dq, entries) {
		if (NULL == u->rolemap ||
		    ! check_rolemap(u->rolemap, role))
			continue;
		if (json && ! first)
			printf(",\n\t\t\t\t\"");
		else if ( ! json)
			printf("%s%s", SPACE, SPACE);
		else
			printf("[\n\t\t\t\t\"");
		print_name_db_update(u);
		if (json)
			putchar('"');
		else
			putchar('\n');
		first = 0;
	}

	if (json)
		printf("%s],\n", first ? "[" : " ");
}

static void
gen_audit_updates(const struct strct *p, 
	int json, const struct role *role)
{
	const struct update *u;
	int	 first = 1;

	if (json)
		printf("\t\t\t\"updates\": ");
	else
		printf("%supdates:\n", SPACE);

	TAILQ_FOREACH(u, &p->uq, entries) {
		if (NULL == u->rolemap ||
	 	    ! check_rolemap(u->rolemap, role)) 
			continue;
		if (json && ! first)
			printf(",\n\t\t\t\t\"");
		else if ( ! json)
			printf("%s%s", SPACE, SPACE);
		else 
			printf("[\n\t\t\t\t\"");
		print_name_db_update(u);
		if (json)
			putchar('"');
		else
			putchar('\n');
		first = 0;
	}

	if (json)
		printf("%s],\n", first ? "[" : " ");
}

static void
gen_protos_insert(const struct strct *s,
	int *first, const struct role *role)
{

	if (NULL == s->ins || NULL == s->ins->rolemap)
		return;
	if ( ! check_rolemap(s->ins->rolemap, role))
		return;
	printf("%s\n\t\t\"", *first ? "" : ",");
	print_name_db_insert(s);
	fputs("\": {\n"
 	      "\t\t\t\"doc\": null", stdout);
	fputs(",\n\t\t\t\"type\": \"insert\" }", stdout);
	*first = 0;
}

static void
gen_protos_updates(const struct updateq *uq,
	int *first, const struct role *role)
{
	const struct update *u;

	TAILQ_FOREACH(u, uq, entries) {
		if (NULL == u->rolemap ||
		    ! check_rolemap(u->rolemap, role))
			continue;
		printf("%s\n\t\t\"", *first ? "" : ",");
		print_name_db_update(u);
		fputs("\": {\n"
	 	      "\t\t\t\"doc\": ", stdout);
		print_doc(u->doc);
		printf(",\n\t\t\t\"type\": \"%s\" }",
			UP_MODIFY == u->type ? "update" : 
			"delete");
		*first = 0;
	}
}

static void
gen_protos_queries(const struct searchq *sq,
	int *first, const struct role *role)
{
	const struct search *s;

	TAILQ_FOREACH(s, sq, entries) {
		if (NULL == s->rolemap ||
		    ! check_rolemap(s->rolemap, role))
			continue;
		printf("%s\n\t\t\"", *first ? "" : ",");
		print_name_db_search(s);
		fputs("\": {\n"
	 	      "\t\t\t\"doc\": ", stdout);
		print_doc(s->doc);
		printf(",\n\t\t\t\"type\": \"%s\" }",
			STYPE_SEARCH == s->type ? "search" : 
			STYPE_ITERATE == s->type ? "iterate" :
			"list");
		*first = 0;
	}
}

static void
gen_protos_fields(const struct strct *s,
	int *first, const struct role *role)
{
	const struct field *f;
	int	 export;

	TAILQ_FOREACH(f, &s->fq, entries) {
		export = FTYPE_PASSWORD != f->type &&
			! (FIELD_NOEXPORT & f->flags);
		if (NULL != f->rolemap &&
		    check_rolemap(f->rolemap, role))
			export = 0;
		printf("%s\n\t\t\"%s.%s\": {\n"
		       "\t\t\t\"export\": %s,\n"
		       "\t\t\t\"doc\": ",
			*first ? "" : ",",
			f->parent->name,
			f->name,
			export ? "true" : "false");
		print_doc(f->doc);
		printf(" }");
		*first = 0;
	}
}

static void
gen_audit_queries(const struct strct *p, int json, 
	enum stype t, const char *tp, const struct role *role)
{
	const struct search *s;
	int	 first = 1;

	if (json)
		printf("\t\t\t\"%s\": ", tp);
	else
		printf("%s%s:\n", SPACE, tp);

	TAILQ_FOREACH(s, &p->sq, entries) {
		if (t != s->type ||
		    NULL == s->rolemap ||
		    ! check_rolemap(s->rolemap, role))
			continue;
		if (json && ! first)
			printf(",\n\t\t\t\t\"");
		else if ( ! json)
			printf("%s%s", SPACE, SPACE);
		else 
			printf("[\n\t\t\t\t\"");
		print_name_db_search(s);
		if (json) {
			putchar('"');
		} else
			putchar('\n');
		first = 0;
	}

	/* Last item: don't have a trailing comma. */

	if (json && STYPE_SEARCH != t)
		printf("%s],\n", first ? "[" : " ");
	else if (json)
		printf("%s]\n", first ? "[" : " ");
}

/*
 * Take "p", which is allowed by to queried (by a search, list, or
 * iterator) by role "role".
 * Mark it (by appending to "sp" and "spsz") to mean that we'll access
 * this structure.
 * Then walk its fields and follow foreign keys if and only if the
 * references are not marked as noexport.
 */
static void
mark_structs(const struct search *orig, 
	const struct strct *p, 
	struct sraccess **sp, size_t *spsz,
	const struct role *role, struct fieldstack *fs,
	int export)
{
	size_t	 i;
	const struct field *f;
	struct srsaccess *ac;
	int	 exp;

	/* Look up our structure in the list of knowns. */

	for (i = 0; i < *spsz; i++)
		if ((*sp)[i].p == p)
			break;

	/* If not found, allocate a search-access record. */

	if (i == *spsz) {
		*sp = reallocarray(*sp, *spsz + 1,
			sizeof(struct sraccess));
		if (NULL == *sp)
			err(EXIT_FAILURE, NULL);
		(*spsz)++;
		memset(&(*sp)[i], 0, sizeof(struct sraccess));
		(*sp)[i].p = p;
	} 

	/* Re-allocate the array of search-access origins. */

	(*sp)[i].origs = reallocarray((*sp)[i].origs,
		(*sp)[i].origsz + 1,
		sizeof(struct srsaccess));
	if (NULL == (*sp)[i].origs)
		err(EXIT_FAILURE, NULL);
	ac = &(*sp)[i].origs[(*sp)[i].origsz++];
	memset(ac, 0, sizeof(struct srsaccess));

	/* Append our current search origin and path. */

	ac->exported = export;
	ac->orig = orig;
	ac->fs = calloc(fs->cur, sizeof(struct field *));
	if (NULL == ac->fs)
		err(EXIT_FAILURE, NULL);
	for (i = 0; i < fs->cur; i++) 
		ac->fs[i] = fs->f[i];
	ac->fsz = fs->cur;

	/* Now recurse into the nested structures. */

	TAILQ_FOREACH(f, &p->fq, entries) {
		if (FTYPE_STRUCT != f->type)
			continue;
		exp = export;
		if (NULL != f->rolemap &&
		    check_rolemap(f->rolemap, role))
			exp = 0;

		if (fs->cur + 1 > fs->max) {
			fs->f = reallocarray(fs->f,
				fs->cur + 10,
				sizeof(struct field *));
			if (NULL == fs->f)
				err(EXIT_FAILURE, NULL);
			fs->max = fs->cur + 10;
		}
		fs->f[fs->cur++] = f;
		mark_structs(orig, 
			f->ref->target->parent, 
			sp, spsz, role, fs, exp);
		fs->cur--;
	}
}

/*
 * Mark all of the structures that can be "seen" by the role by first
 * computing whether they have searches used by our role, then climbing
 * the dependency tree.
 * We deliberately do this if we've already seen the structure (we could
 * otherwise "break" after marking the structure) to record access
 * paths.
 */
static void
sraccess_alloc(const struct strctq *sq, 
	const struct role *r, struct sraccess **sp, size_t *spsz)
{
	const struct strct *s;
	const struct search *sr;
	struct fieldstack fs;

	memset(&fs, 0, sizeof(struct fieldstack));

	TAILQ_FOREACH(s, sq, entries)
		TAILQ_FOREACH(sr, &s->sq, entries)
			if (NULL != sr->rolemap &&
			    check_rolemap(sr->rolemap, r))
				mark_structs(sr, s, sp, spsz, r, &fs, 1);

	free(fs.f);
}

static void
sraccess_free(struct sraccess *sp, size_t spsz)
{
	size_t	 i, j;

	for (i = 0; i < spsz; i++) {
		for (j = 0; j < sp[i].origsz; j++)
			free(sp[i].origs[j].fs);
		free(sp[i].origs);
	}
	
	free(sp);
}

static int
gen_audit_json(const struct config *cfg, const char *role)
{
	const struct strct *s;
	const struct role *r;
	struct sraccess *sp = NULL;
	size_t	 i, spsz = 0;
	int	 first;

	if (NULL == (r = check_role_exists(&cfg->rq, role))) {
		warnx("%s: role not found", role);
		return(0);
	}

	sraccess_alloc(&cfg->sq, r, &sp, &spsz);

	printf("(function(root) {\n"
	       "\t'use strict';\n"
	       "\tvar audit = {\n"
	       "\t    \"role\": \"%s\",\n"
	       "\t    \"doc\": ", r->name);
	print_doc(r->doc);
	puts(",\n\t    \"access\": [");

	TAILQ_FOREACH(s, &cfg->sq, entries) {
		printf("\t\t{ \"name\": \"%s\",\n"
		       "\t\t  \"access\": {\n", s->name);

		for (i = 0; i < spsz; i++) {
			if (s != sp[i].p) 
				continue;
			gen_audit_exportable(s, &sp[i], 1, r);
			break;
		}

		gen_audit_inserts(s, 1, r);
		gen_audit_updates(s, 1, r);
		gen_audit_deletes(s, 1, r);
		gen_audit_queries(s, 1, STYPE_ITERATE, "iterates", r);
		gen_audit_queries(s, 1, STYPE_LIST, "lists", r);
		gen_audit_queries(s, 1, STYPE_SEARCH, "searches", r);

		printf("\t\t}}%s\n",
			NULL != TAILQ_NEXT(s, entries) ? "," : "");
	}

	fputs("\t],\n"
	      "\t\"functions\": {", stdout);

	first = 1;
	TAILQ_FOREACH(s, &cfg->sq, entries) {
		gen_protos_queries(&s->sq, &first, r);
		gen_protos_updates(&s->uq, &first, r);
		gen_protos_updates(&s->dq, &first, r);
		gen_protos_insert(s, &first, r);
	}

	puts("\n\t},\n"
	     "\t\"fields\": {");

	for (first = 1, i = 0; i < spsz; i++)
		gen_protos_fields(sp[i].p, &first, r);

	puts("\n\t}};\n"
	     "\n"
	     "\troot.audit = audit;\n"
	     "})(this);");

	sraccess_free(sp, spsz);
	return(1);
}

static int
gen_audit(const struct config *cfg, const char *role)
{
	const struct strct *s;
	const struct role *r;
	struct sraccess *sp = NULL;
	size_t	 i, spsz = 0;

	if (NULL == (r = check_role_exists(&cfg->rq, role))) {
		warnx("%s: role not found", role);
		return(0);
	}

	sraccess_alloc(&cfg->sq, r, &sp, &spsz);

	TAILQ_FOREACH(s, &cfg->sq, entries) {
		printf("%s\n", s->name);

		for (i = 0; i < spsz; i++) {
			if (s != sp[i].p) 
				continue;
			gen_audit_exportable(s, &sp[i], 0, r);
			break;
		}

		gen_audit_inserts(s, 0, r);
		gen_audit_updates(s, 0, r);
		gen_audit_deletes(s, 0, r);
		gen_audit_queries(s, 0, STYPE_ITERATE, "iterates", r);
		gen_audit_queries(s, 0, STYPE_LIST, "lists", r);
		gen_audit_queries(s, 0, STYPE_SEARCH, "searches", r);
	}

	sraccess_free(sp, spsz);
	return(1);
}

static void
gen_audit_exportable_gv(const struct strct *p, 
	const struct sraccess *ac, const struct role *role)
{
	const struct field *f;
	const struct search *s;
	size_t	  i, j, k, cols = 2 /* FIXME: settable */, col;
	int	  rc, export, closed, exportable = 0;
	char	**edges = NULL;
	char	 *buf;
	size_t	  edgesz = 0;

	for (i = 0; 0 == exportable && i < ac->origsz; i++) 
		if (ac->origs[i].exported)
			exportable = 1;

	/* Start with our HTML record. */

	printf("\tstruct_%s[shape=none, fillcolor=\"%s\", "
	        "style=\"filled\", label=<\n"
	       "\t\t<table cellspacing=\"0\" "
	        "border=\"0\" cellborder=\"1\">\n"
	       "\t\t\t<tr>\n"
	       "\t\t\t\t<td colspan=\"%zu\" port=\"_top\">\n"
	       "\t\t\t\t\t<b>%s</b>\n"
	       "\t\t\t\t</td>\n"
	       "\t\t\t</tr>\n",
	       p->name, exportable ? "#ffffff" : "#cccccc", 
	       cols, p->name);

	/* Columnar fields. */

	col = 0;
	closed = 0;
	TAILQ_FOREACH(f, &p->fq, entries) {
		export = check_field_exported(f, role, exportable);
		if (col && 0 == (col % cols))
			puts("\t\t\t</tr>");
		if (0 == (col % cols))
			puts("\t\t\t<tr>");
		printf("\t\t\t\t<td bgcolor=\"%s\" "
		       "port=\"%s\">%s</td>\n", 
		       export ? "#ffffff" : "#aaaaaa",
		       f->name, f->name);
		col++;
		closed = 0;
	}

	/* Finish fields remaining in row. */

	if (0 != (col % cols)) {
		while (0 != (col % cols)) {
			puts("\t\t\t\t<td></td>");
			col++;
		}
		printf("\t\t\t</tr>\n");
		closed = 1;
	}

	if (col && 0 == closed)
		puts("\t\t\t</tr>");

	/* Show query routines, each on their own row. */

	TAILQ_FOREACH(s, &p->sq, entries) {
		if (NULL == s->rolemap ||
		    ! check_rolemap(s->rolemap, role))
			continue;
		printf("\t\t\t<tr><td bgcolor=\"#eeeeee\" "
		       "colspan=\"%zu\">", cols);
		print_name_db_search(s);
		puts("</td></tr>");
	}

	printf("\t\t</table>>];\n");

	for (i = 0; i < ac->origsz; i++) {
		if (0 == ac->origs[i].fsz) 
			continue;

		for (j = 0; j < ac->origs[i].fsz - 1; j++) {
			rc = asprintf(&buf, "struct_%s:%s->struct_%s:%s",
				ac->origs[i].fs[j]->parent->name,
				ac->origs[i].fs[j]->name,
				ac->origs[i].fs[j + 1]->parent->name,
				ac->origs[i].fs[j + 1]->name);
			if (rc < 0)
				err(EXIT_FAILURE, NULL);
			for (k = 0; k < edgesz; k++)
				if (0 == strcmp(edges[k], buf))
					break;
			if (k < edgesz) {
				free(buf);
				continue;
			}
			edges = reallocarray(edges,
				edgesz + 1, sizeof(char *));
			if (NULL == edges)
				err(EXIT_FAILURE, NULL);
			edges[edgesz++] = buf;
		}

		rc = asprintf(&buf, "struct_%s:%s->struct_%s:_top",
			ac->origs[i].fs[j]->parent->name,
			ac->origs[i].fs[j]->name, p->name);
		if (rc < 0)
			err(EXIT_FAILURE, NULL);
		for (k = 0; k < edgesz; k++)
			if (0 == strcmp(edges[k], buf))
				break;
		if (k < edgesz) {
			free(buf);
			continue;
		}
		edges = reallocarray(edges,
			edgesz + 1, sizeof(char *));
		if (NULL == edges)
			err(EXIT_FAILURE, NULL);
		edges[edgesz++] = buf;
	}

	for (i = 0; i < edgesz; i++) {
		if (NULL != strstr(edges[i], ":_top"))
			printf("\t%s;\n", edges[i]);
		else
			printf("\t%s[style=\"dotted\"];\n", edges[i]);
		free(edges[i]);
	}
	free(edges);
}

/*
 * Audit for a GraphViz digraph.
 */
static int
gen_audit_gv(const struct config *cfg, const char *role)
{
	const struct strct 	*s;
	const struct role 	*r;
	struct sraccess 	*sp = NULL;
	size_t	 		 i, spsz = 0;

	if (NULL == (r = check_role_exists(&cfg->rq, role))) {
		warnx("%s: role not found", role);
		return(0);
	}

	printf("digraph %s {\n", r->name);
	sraccess_alloc(&cfg->sq, r, &sp, &spsz);

	TAILQ_FOREACH(s, &cfg->sq, entries)
		for (i = 0; i < spsz; i++)
			if (s == sp[i].p) {
				gen_audit_exportable_gv(s, &sp[i], r);
				break;
			}

	puts("}");
	sraccess_free(sp, spsz);
	return(1);
}

int
main(int argc, char *argv[])
{
	const char	 *role = NULL;
	struct config	 *cfg;
	int		  rc = 0;
	enum op		  op = OP_AUDIT;
	size_t		  i, confsz;
	FILE		**confs = NULL;

#if HAVE_PLEDGE
	if (-1 == pledge("stdio rpath", NULL))
		err(EXIT_FAILURE, "pledge");
#endif

	if (0 == strcmp(getprogname(), "kwebapp-audit-gv"))
		op = OP_AUDIT_GV;
	else if (0 == strcmp(getprogname(), "kwebapp-audit-json"))
		op = OP_AUDIT_JSON;

	if (-1 != getopt(argc, argv, ""))
		goto usage;

	argc -= optind;
	argv += optind;
	if (0 == argc)
		goto usage;
	role = argv[0];
	argc--;
	argv++;

	confsz = (size_t)argc;
	
	/* Read in all of our files now so we can repledge. */

	if (confsz > 0) {
		confs = calloc(confsz, sizeof(FILE *));
		if (NULL == confs)
			err(EXIT_FAILURE, NULL);
		for (i = 0; i < confsz; i++) {
			confs[i] = fopen(argv[i], "r");
			if (NULL == confs[i]) {
				warn("%s", argv[i]);
				goto out;
			}
		}
	}

#if HAVE_PLEDGE
	if (-1 == pledge("stdio", NULL))
		err(EXIT_FAILURE, "pledge");
#endif

	cfg = config_alloc();

	for (i = 0; i < confsz; i++)
		if ( ! parse_config_r(cfg, confs[i], argv[i]))
			goto out;

	if (0 == confsz && ! parse_config_r(cfg, stdin, "<stdin>"))
		goto out;
	if ( ! parse_link(cfg))
		goto out;

	if (OP_AUDIT == op)
		rc = gen_audit(cfg, role);
	else if (OP_AUDIT_JSON == op)
		rc = gen_audit_json(cfg, role);
	else 
		rc = gen_audit_gv(cfg, role);

out:
	for (i = 0; i < confsz; i++)
		if (EOF == fclose(confs[i]))
			warn("%s", argv[i]);
	free(confs);
	config_free(cfg);

	return rc ? EXIT_SUCCESS : EXIT_FAILURE;
usage:
	if (OP_AUDIT_GV == op)
		fprintf(stderr, 
			"usage: %s "
			"role [config...]\n",
			getprogname());
	else if (OP_AUDIT_JSON == op)
		fprintf(stderr, 
			"usage: %s "
			"role [config...]\n",
			getprogname());
	else 
		fprintf(stderr, 
			"usage: %s "
			"role [config...]\n",
			getprogname());
	return EXIT_FAILURE;
}
