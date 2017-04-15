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
#include <inttypes.h>
#include <stdlib.h>

#include <ksql.h>

#include "db.h"

int
main(void)
{
	struct ksql	*sql;
	int64_t		 cid, uid, nuid;
	struct user	*u, *u2, *u3;

	if (NULL == (sql = db_open("db.db")))
		errx(EXIT_FAILURE, "db.db");

	if ((cid = db_company_insert(sql, "foo bar")) < 0)
		errx(EXIT_FAILURE, "db_company_insert");

	uid = db_user_insert(sql, cid, 
		"password", "foo@foo.com", "foo bar");
	if (uid < 0)
		errx(EXIT_FAILURE, "db_user_insert");

	nuid = db_user_insert(sql, cid, 
		"password", "foo@foo.com", "foo bar");

	if (nuid >= 0)
		errx(EXIT_FAILURE, "db_user_insert (duplicate)");

	if (NULL == (u = db_user_by_rowid(sql, uid)))
		errx(EXIT_FAILURE, "db_user_by_rowd");

	warnx("company name: %s", u->company.name);
	warnx("company id: %" PRId64, u->company.id);
	warnx("cid: %" PRId64, u->cid);
	warnx("hash: %s", u->hash);
	warnx("email: %s", u->email);
	warnx("name: %s", u->name);
	warnx("uid: %" PRId64, u->uid);

	u2 = db_user_by_creds(sql, "foo@foo.com", "password");
	if (NULL == u2)
		errx(EXIT_FAILURE, "db_user_by_creds");

	u3 = db_user_by_creds(sql, "foo@foo.com", "password2");
	if (NULL != u3)
		errx(EXIT_FAILURE, "db_user_by_creds");

	db_user_free(u);
	db_user_free(u2);
	db_user_free(u3);

	if ( ! db_user_update_hash_by_uid(sql, "password2", uid))
		errx(EXIT_FAILURE, "db_user_update_hash");

	u2 = db_user_by_creds(sql, "foo@foo.com", "password");
	if (NULL != u2)
		errx(EXIT_FAILURE, "db_user_by_creds");

	u3 = db_user_by_creds(sql, "foo@foo.com", "password2");
	if (NULL == u3)
		errx(EXIT_FAILURE, "db_user_by_creds");

	db_user_free(u2);
	db_user_free(u3);

	db_close(sql);
	return(EXIT_FAILURE);
}
