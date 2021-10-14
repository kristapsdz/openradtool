const db: ortdb = ort(dbfile);
const ctx: ortctx = db.connect();

const id1: bigint = ctx.db_foo_insert(null, null);
if (id1 < 0)
	return false;

const obj1: ortns.foo|null = ctx.db_foo_get_id(id1);
if (obj1 === null)
	return false;
if (obj1.obj.bar1 !== null)
	return false;
if (obj1.obj.bar2 !== null)
	return false;

const id2: bigint = ctx.db_bar_insert();
if (id2 < 0)
	return false;
const id3: bigint = ctx.db_foo_insert(id2, null);
if (id3 < 0)
	return false;

const obj2: ortns.foo|null = ctx.db_foo_get_id(id3);
if (obj2 === null)
	return false;
if (obj2.obj.bar1 === null)
	return false;
if (obj2.obj.bar2 !== null)
	return false;

const id4: bigint = ctx.db_foo_insert(id2, id2);
if (id4 < 0)
	return false;

const obj3: ortns.foo|null = ctx.db_foo_get_id(id4);
if (obj3 === null)
	return false;
if (obj3.obj.bar1 === null)
	return false;
if (obj3.obj.bar2 === null)
	return false;

return true;
