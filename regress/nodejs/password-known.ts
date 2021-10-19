
const db: ortdb = ort(dbfile, { bcrypt_cost: 4 });
const ctx: ortctx = db.connect();

const id: bigint = ctx.db_foo_insert('password');
if (id < 0)
	return false;

const obj1: ortns.foo|null = ctx.db_foo_get_hash('password');
if (obj1 === null)
	return false;

if (!ctx.db_foo_update_hash('$2b$08$YCwD6QeXPZTW8NOEuh5H5er5ALQq9.r58nqUxy8K/IVamm.h7Fl2q', id))
	return false;

const obj2: ortns.foo|null = ctx.db_foo_get_hash('foobarbaz');
if (obj2 === null)
	return false;

return true;
