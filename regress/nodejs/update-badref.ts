const db: ortdb = ort(dbfile);
const ctx: ortctx = db.connect();

const id1: bigint = ctx.db_bar_insert();
if (id1 < 0)
	return false;

const id2: bigint = ctx.db_foo_insert(id1);
if (id2 < 0)
	return false;

if (ctx.db_foo_update_barid(1242))
	return false;

return true;

