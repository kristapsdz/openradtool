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
#include <sys/queue.h>
#include <sys/types.h>

#include <err.h>
#include <float.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include <kcgi.h>
#include <kcgijson.h>
#include <kcgiregress.h>

#include "regress.h"
#include "complex-case.ort.h"

static int
server(const char *fname)
{
	struct kreq	 r;
	struct user_q	*uq;
	struct user	*u;
	struct ort	*ort;
	struct kjsonreq	 req;
	int64_t		 cid, uid;

	if ((ort = db_open(fname)) == NULL)
		return 0;
	if ((cid = db_company_insert(ort, "test name", NULL)) == -1)
		return 0;
	if ((uid = db_user_insert(ort, cid, SEX_male, "abcd",
	    "kristaps@bsd.lv", 0, NULL, "kristaps", time(NULL))) == -1)
		return 0;

	if (khttp_parse(&r, NULL, 0, NULL, 0, 0) != KCGI_OK)
		return 0;

	khttp_head(&r, kresps[KRESP_STATUS], 
		"%s", khttps[KHTTP_200]);
	khttp_head(&r, kresps[KRESP_CONTENT_TYPE], 
		"%s", kmimetypes[KMIME_APP_JSON]);
	khttp_body(&r);

	uq = db_user_list_foo(ort);

	kjson_open(&req, &r);
	kjson_array_open(&req);
	TAILQ_FOREACH(u, uq, _entries) {
		kjson_obj_open(&req);
		json_user_data(&req, u);
		kjson_obj_close(&req);
	}
	kjson_close(&req);
	khttp_free(&r);
	db_user_freeq(uq);
	db_close(ort);
	return 1;
}

static int
client(long http, const char *buf, size_t sz)
{
	struct user	*u = NULL;
	size_t		 usz = 0;
	int		 rc = 0, tsz, ntsz;
	jsmn_parser	 jp;
	jsmntok_t	*t = NULL;

	if (http != 200)
		goto out;

	warnx("%.*s", (int)sz, buf);

	/* Parse JSON results. */

	jsmn_init(&jp);
	if ((tsz = jsmn_parse(&jp, buf, sz, NULL, 0)) <= 0)
		goto out;
	if ((t = calloc(tsz, sizeof(jsmntok_t))) == NULL)
		goto out;
	jsmn_init(&jp);
	if ((ntsz = jsmn_parse(&jp, buf, sz, t, tsz)) != tsz)
		goto out;
	
	/* Analyse. */

	if ((ntsz = jsmn_user_array(&u, &usz, buf, t, tsz)) <= 0)
		goto out;
	tsz += ntsz;

	if (usz != 1)
		goto out;
	if (strcmp(u[0].email, "kristaps@bsd.lv") != 0)
		goto out;
	if (strcmp(u[0].company.name, "test name") != 0)
		goto out;

	rc = 1;
out:
	jsmn_user_free_array(u, usz);
	free(t);
	return rc;
}

int
main(int argc, char *argv[])
{

	return regress(client, server, argc, argv);
}

