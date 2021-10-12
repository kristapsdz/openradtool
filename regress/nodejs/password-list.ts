const db: ortdb = ort(dbfile);
const ctx: ortctx = db.connect();

const rc: bigint = ctx.db_foo_insert('password');
if (rc < 0)
	return false;

const obj1: ortns.foo[] = ctx.db_foo_get_id(rc);
if (obj1.length === 0)
	return false;

const obj2: ortns.foo[] = ctx.db_foo_list_hash('password');
if (obj2.length === 0)
	return false;

const obj3: ortns.foo[] = ctx.db_foo_list_hash('shmassword');
if (obj3.length !== 0)
	return false;

const obj4: ortns.foo[] = ctx.db_foo_list_nhash('password');
if (obj4.length !== 0)
	return false;

const obj5: ortns.foo[] = ctx.db_foo_list_nhash('shmassword');
if (obj5.length === 0)
	return false;

return true;
