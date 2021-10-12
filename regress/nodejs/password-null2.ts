const db: ortdb = ort(dbfile);
const ctx: ortctx = db.connect();

const rc: bigint = ctx.db_foo_insert(null);
if (rc < 0)
	return false;

const obj1: ortns.foo|null = ctx.db_foo_get_id(rc);
if (obj1 === null)
	return false;

const obj2: ortns.foo|null = ctx.db_foo_get_hash('password');
if (obj2 !== null)
	return false;

const obj3: ortns.foo|null = ctx.db_foo_get_nhash('password');
if (obj3 === null)
	return false;

const obj4: ortns.foo|null = ctx.db_foo_get_hash(null);
if (obj4 === null)
	return false;

const obj5: ortns.foo|null = ctx.db_foo_get_nhash(null);
if (obj5 !== null)
	return false;

return true;
