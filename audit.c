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

#if HAVE_ERR
# include <err.h>
#endif
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "extern.h"

#define	SPACE	"\t" /* Space for output indentation. */

/*
 * Search single access.
 * This keeps track of an origin function and the structure stack
 * required to get to the structure.
 */
struct	srsaccess {
	const struct search *orig;
	const struct field **fs;
	size_t		 fsz;
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
 * See if any role at or descending from "r" matches the named "role".
 * Returns the pointer to the matching role or NULL otherwise.
 */
static const struct role *
get_roleassign(const struct role *r, const char *role)
{
	const struct role *rq, *ret;

	if (0 == strcasecmp(r->name, role))
		return(r);

	TAILQ_FOREACH(rq, &r->subrq, entries)
		if (NULL != (ret = get_roleassign(rq, role)))
			return(ret);

	return(NULL);
}

static void
gen_audit_exportable(const struct strct *p, 
	const struct sraccess *ac, int json,
	const struct role *role)
{
	const struct field *f;
	size_t	 i, j;
	int	 export;
	
	if (json) 
		puts("\t\t\t\"data\": [");
	else
		printf("%sdata:\n", SPACE);

	TAILQ_FOREACH(f, &p->fq, entries) {
		export = 1;
		if (NULL != f->rolemap &&
		    check_rolemap(f->rolemap, role))
			export = 0;
		if (json)
			printf("\t\t\t\t{ \"field\": \"%s\", "
			       "\"export\": %s }%s\n", f->name, 
			       export ? "true" : "false",
			       NULL != TAILQ_NEXT(f, entries) ?
			       "," : "");
		else
			printf("%s%s%s%s\n", SPACE, SPACE, f->name,
				export ? "" : ": NOT EXPORTED");
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
			printf("\",\n\t\t\t\t  \"path\": [");
		else
			printf(": ");
		for (j = 0; j < ac->origs[i].fsz; j++) {
			if (j > 0 && json)
				printf(", ");
			else if (j > 0)
				putchar('.');
			if (json)
				putchar('"');
			printf("%s", ac->origs[i].fs[j]->name);
			if (json)
				putchar('"');
		}
		if (json)
			printf("] }%s\n", 
				i < ac->origsz - 1 ? "," : "");
		else if (0 == j)
			puts("self-reference");
		else
			puts("");
	}

	if (json) 
		puts("\t\t\t],");
}

static void
gen_audit_inserts(const struct strct *p, 
	int json, const struct role *role)
{

	if (NULL == p->irolemap) 
		return;
	if (json)
		printf("\t\t\t\"insertion\": ");
	else
		printf("%sinsertion:\n", SPACE);
	if (check_rolemap(p->irolemap, role)) {
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
		printf("\t\t\t\"deletes\": [");
	else
		printf("%sdeletes:\n", SPACE);

	TAILQ_FOREACH(u, &p->dq, entries) {
		if (check_rolemap(u->rolemap, role)) {
			if (json && ! first)
				printf(", ");
			else
				printf("%s%s", SPACE, SPACE);
			print_name_db_update(u);
			putchar('\n');
			first = 0;
		}
	}

	if (json)
		puts("],");
}

static void
gen_audit_updates(const struct strct *p, const struct role *role)
{
	const struct update *u;

	printf("%supdates:\n", SPACE);
	TAILQ_FOREACH(u, &p->uq, entries) {
		if (check_rolemap(u->rolemap, role)) {
			printf("%s%s", SPACE, SPACE);
			print_name_db_update(u);
			putchar('\n');
		}
	}
}

static void
gen_audit_iterates(const struct strct *p, const struct role *role)
{
	const struct search *s;

	printf("%siterates:\n", SPACE);
	TAILQ_FOREACH(s, &p->sq, entries)
		if (STYPE_ITERATE == s->type)
			if (check_rolemap(s->rolemap, role)) {
				printf("%s%s", SPACE, SPACE);
				print_name_db_search(s);
				putchar('\n');
			}
}

static void
gen_audit_lists(const struct strct *p, const struct role *role)
{
	const struct search *s;

	printf("%slists:\n", SPACE);
	TAILQ_FOREACH(s, &p->sq, entries)
		if (STYPE_LIST == s->type)
			if (check_rolemap(s->rolemap, role)) {
				printf("%s%s", SPACE, SPACE);
				print_name_db_search(s);
				putchar('\n');
			}
}

static void
gen_audit_searches(const struct strct *p, const struct role *role)
{
	const struct search *s;

	printf("%ssearches:\n", SPACE);
	TAILQ_FOREACH(s, &p->sq, entries)
		if (STYPE_SEARCH == s->type)
			if (check_rolemap(s->rolemap, role)) {
				printf("%s%s", SPACE, SPACE);
				print_name_db_search(s);
				putchar('\n');
			}
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
	const struct role *role, struct fieldstack *fs)
{
	size_t	 i;
	const struct field *f;
	struct srsaccess *ac;

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
		if (NULL != f->rolemap &&
		    check_rolemap(f->rolemap, role))
			continue;

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
			sp, spsz, role, fs);
		fs->cur--;
	}
}

int
gen_audit(const struct config *cfg, int json, const char *role)
{
	const struct strct *s;
	const struct search *sr;
	const struct role *r = NULL, *rr;
	struct sraccess *sp = NULL;
	size_t	i, j, spsz = 0;
	struct fieldstack fs;

	memset(&fs, 0, sizeof(struct fieldstack));

	/*
	 * Begin by checking to see if the role exists at all.
	 * For this, we need to walk through all roles as they're
	 * arranged in the role tree.
	 */

	TAILQ_FOREACH(rr, &cfg->rq, entries)
		if (NULL != (r = get_roleassign(rr, role)))
			break;

	if (NULL == r) {
		warnx("%s: role not found", role);
		return(0);
	}

	/*
	 * Now mark all of the structures that can be "seen" by the role
	 * by first computing whether they have searches used by our
	 * role, then climbing the dependency tree.
	 * We deliberately do this if we've already seen the structure
	 * (we could otherwise "break" after marking the structure) to
	 * record access paths.
	 */

	TAILQ_FOREACH(s, &cfg->sq, entries)
		TAILQ_FOREACH(sr, &s->sq, entries)
			if (NULL != sr->rolemap &&
			    check_rolemap(sr->rolemap, r))
				mark_structs(sr, s, &sp, &spsz, r, &fs);

	/*
	 * Now walk through all structures and print the ways we have of
	 * accessing the structure, if any.
	 * Only print exports if the structure is reachable.
	 */

	if (json)
		puts("(function(root) {\n"
		     "\t'use strict';\n"
		     "\tvar audit = [");

	TAILQ_FOREACH(s, &cfg->sq, entries) {
		if (json)
			printf("\t\t{ \"%s\": {\n", s->name);
		else
			printf("%s\n", s->name);
		for (i = 0; i < spsz; i++)
			if (s == sp[i].p) {
				gen_audit_exportable
					(s, &sp[i], json, r);
				break;
			}
		gen_audit_inserts(s, json, r);
		gen_audit_updates(s, r);
		gen_audit_deletes(s, json, r);
		gen_audit_searches(s, r);
		gen_audit_lists(s, r);
		gen_audit_iterates(s, r);
		if (json)
			printf("\t\t}%s\n",
				NULL != TAILQ_NEXT(s, entries) ?
				"," : "");
	}

	if (json)
		puts("\t];\n"
		     "\n"
		     "\troot.audit = audit;\n"
		     "})(this);");

	for (i = 0; i < spsz; i++) {
		for (j = 0; j < sp[i].origsz; j++)
			free(sp[i].origs[j].fs);
		free(sp[i].origs);
	}
	free(fs.f);
	free(sp);
	return(1);
}
