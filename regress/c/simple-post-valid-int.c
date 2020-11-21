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
#include <sys/types.h>

#include <float.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include <kcgi.h>
#include <kcgijson.h>
#include <kcgiregress.h>

#include "regress.h"
#include "simple-post-valid-int.ort.h"

static int
server(const char *fname)
{
	struct kreq	 r;
	struct foo	*foo;
	struct ort	*ort;
	struct kjsonreq	 req;
	int64_t		 id;

	if ((ort = db_open(fname)) == NULL)
		return 0;

	if (khttp_parse(&r, valid_keys,
	    VALID__MAX, NULL, 0, 0) != KCGI_OK)
		return 0;

	if (r.fieldmap[VALID_FOO_INTGTZERO] == NULL ||
	    r.fieldmap[VALID_FOO_INTGTZERO]->next != NULL ||
	    r.fieldmap[VALID_FOO_INTLTZERO] == NULL ||
	    r.fieldmap[VALID_FOO_INTLTZERO]->next != NULL ||
	    r.fieldmap[VALID_FOO_INTEQZERO] == NULL ||
	    r.fieldmap[VALID_FOO_INTEQZERO]->next != NULL ||
	    r.fieldmap[VALID_FOO_INTONE] == NULL ||
	    r.fieldmap[VALID_FOO_INTONE]->next != NULL)
		return 0;

	id = db_foo_insert(ort, 
		r.fieldmap[VALID_FOO_INTGTZERO]->parsed.i,
		r.fieldmap[VALID_FOO_INTLTZERO]->parsed.i,
		r.fieldmap[VALID_FOO_INTEQZERO]->parsed.i,
		r.fieldmap[VALID_FOO_INTONE]->parsed.i);
	if (id == -1)
		return 0;

	if ((foo = db_foo_get_id(ort, id)) == NULL)
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
	db_foo_free(foo);
	db_close(ort);
	return 1;
}

static int
client(long http, const char *buf, size_t sz)
{
	struct foo	 foo;
	struct foo	*foop = NULL;
	int		 rc = 0, tsz, ntsz;
	jsmn_parser	 jp;
	jsmntok_t	*t = NULL;

	if (http != 200)
		goto out;

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

	if ((ntsz = jsmn_foo(&foo, buf, t, tsz)) <= 0)
		goto out;
	tsz += ntsz;
	foop = &foo;

	if (foo.intgtzero == 123456 &&
	    foo.intltzero == -123456 &&
	    foo.inteqzero == 0 &&
	    foo.intone == 1)
		rc = 1;
out:
	jsmn_foo_clear(foop);
	free(t);
	return rc;
}

int
main(int argc, char *argv[])
{

	return regress_fields(client, server, argc, argv, 
		"foo-intgtzero=123456&"
		"foo-intgtzero=-123456&"
		"foo-intgtzero=0&"
		"foo-intltzero=123456&"
		"foo-intltzero=-123456&"
		"foo-intltzero=0&"
		"foo-inteqzero=0&"
		"foo-inteqzero=1&"
		"foo-intone=-1&"
		"foo-intone=0&"
		"foo-intone=1&"
		"foo-intone=2");
}
