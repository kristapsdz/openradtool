#include <stdlib.h>

#include "db.h"

enum	stmt {
	STMT_FOO_GET,
	STMT__MAX
};

#define SCHEMA_FOO "foo.bar,foo.baz"

static	const char *const stmts[STMT__MAX] = {
	"SELECT " FOO " FROM foo WHERE id=?",
};

static void
db_foo_fill(struct foo *p, struct ksqlstmt *stmt, size_t *pos)
{
	size_t i = 0;
	if (NULL == pos)
		pos = &i;
	memset(p, 0, sizeof(*p));
	p->bar = ksql_stmt_int(stmt, (*pos)++);
	p->baz = kstrdup(ksql_stmt_str(stmt, (*pos)++));
}

static void
db_foo_unfill(struct foo *p)
{
	if (NULL == p)
		return;
	free(p->baz);
}

void
db_foo_free(struct foo *p)
{
	db_foo_unfill(p);
	free(p);
}

