const db: ortdb = ort(dbfile);
const ctx: ortctx = db.connect();

const rc: bigint = ctx.db_foo_insert('password');
if (rc < 0)
	return false;

const obj1: ortns.foo|null = ctx.db_foo_get_id(rc);
if (obj1 === null)
	return false;

let foo: boolean;

foo = false;
ctx.db_foo_iterate_hash('password', function(res: ortns.foo) {
	foo = true;
});
if (!foo)
	return false;

foo = true;
ctx.db_foo_iterate_hash('shmassword', function(res: ortns.foo) {
	foo = false;
});
if (!foo)
	return false;

foo = true;
ctx.db_foo_iterate_nhash('password', function(res: ortns.foo) {
	foo = false;
});
if (!foo)
	return false;

foo = false;
ctx.db_foo_iterate_nhash('shmassword', function(res: ortns.foo) {
	foo = true;
});
if (!foo)
	return false;

return true;
