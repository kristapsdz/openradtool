const db: ortdb = ort(dbfile);
const ctx: ortctx = db.connect();

const id1: bigint = ctx.db_foo_insert(1241);
if (id1 < 0)
	return false;

const id2: bigint = ctx.db_foo_insert(1241);
if (id2 >= 0)
	return false;

return true;

