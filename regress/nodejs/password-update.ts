const db: ortdb = ort(dbfile, { bcrypt_cost: 4 });
const ctx: ortctx = db.connect();

const id: bigint = ctx.db_foo_insert('password');
if (id < 0)
	return false;

if (!ctx.db_foo_update_pass('shmassword', id))
	return false;

const obj: ortns.foo|null = ctx.db_foo_get_hash('shmassword');
if (obj === null)
	return false;
if (obj.obj.id !== id)
	return false;

if (ctx.db_foo_get_hash('password') !== null)
	return false;

return true;
