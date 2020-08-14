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
#include <float.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include <curl/curl.h>
#include <kcgi.h>
#include <kcgijson.h>
#include <kcgiregress.h>

#include "simple.ort.h"

#define	URL "http://localhost:17123/index.json"

static int
server(void *arg)
{
	struct kreq	 r;
	struct ort	*ort;
	const char	*db = arg;
	struct foo	*foo;
	struct kjsonreq	 req;
	int64_t		 id;

	if ((ort = db_open(db)) == NULL)
		return 0;
	id = db_foo_insert(ort, "test", 
		1.0, ENM_a, BITF_BITS_a);
	if (id == -1)
		return 0;
	if ((foo = db_foo_get_id(ort, id)) == NULL)
		return 0;

	if (khttp_parse(&r, NULL, 0, NULL, 0, 0) != KCGI_OK)
		return 0;
	khttp_head(&r, kresps[KRESP_STATUS], 
		"%s", khttps[KHTTP_200]);
	khttp_head(&r, kresps[KRESP_CONTENT_TYPE], 
		"%s", kmimetypes[KMIME_APP_JSON]);
	khttp_body(&r);

	kjson_open(&req, &r);
	kjson_obj_open(&req);
	json_foo_data(&req, foo);
	kjson_close(&req);
	khttp_free(&r);
	db_close(ort);
	db_foo_free(foo);
	return 1;
}

static size_t
client_parse(void *dat, size_t sz, size_t nm, void *arg)
{

	return kcgi_buf_write(dat, nm * sz, arg) != KCGI_OK ?
		0 : nm * sz;
}

static int
client(void *arg)
{
	CURL		*curl;
	long		 http;
	struct kcgi_buf	 buf;
	struct foo	 foo;
	struct foo	*foop = NULL;
	int		 rc = 0, tsz, ntsz;
	jsmn_parser	 jp;
	jsmntok_t	*t = NULL;

	if ((curl = curl_easy_init()) == NULL)
		return 0;
	memset(&buf, 0, sizeof(struct kcgi_buf));
	curl_easy_setopt(curl, CURLOPT_URL, URL);
	curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, client_parse);
	curl_easy_setopt(curl, CURLOPT_WRITEDATA, &buf);

	/* Make HTTP query. */

	if (curl_easy_perform(curl) != CURLE_OK)
		goto out;
	curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http);
	if (http != 200)
		goto out;

	/* Parse JSON results. */

	jsmn_init(&jp);
	if ((tsz = jsmn_parse(&jp, buf.buf, buf.sz, NULL, 0)) <= 0)
		goto out;
	if ((t = calloc(tsz, sizeof(jsmntok_t))) == NULL)
		goto out;
	jsmn_init(&jp);
	if ((ntsz = jsmn_parse(&jp, buf.buf, buf.sz, t, tsz)) != tsz)
		goto out;
	
	/* Analyse. */

	if ((ntsz = jsmn_foo(&foo, buf.buf, t, tsz)) <= 0)
		goto out;
	tsz += ntsz;
	foop = &foo;
	if (strcmp(foo.a, "test"))
		goto out;
	if (foo.b > 1.0 + DBL_EPSILON || foo.b < 1.0 - DBL_EPSILON)
		goto out;
	if (foo.c != ENM_a)
		goto out;
	if (foo.d != BITF_BITS_a)
		goto out;
	if (foo.id != 1)
		goto out;

	rc = 1;
out:
	curl_easy_cleanup(curl);
	curl_global_cleanup();
	jsmn_foo_clear(foop);
	free(buf.buf);
	free(t);
	return rc;
}

int
main(int argc, char *argv[])
{

	if (argc != 2)
		return 1;
	return kcgi_regress_cgi
		(client, NULL, server, argv[1]) ? 0 : 1;
}

