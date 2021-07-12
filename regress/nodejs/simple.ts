const db: ortdb = ort(dbfile);
const ctx: ortctx = db.connect();

const rc: BigInt = ctx.db_foo_insert('a', 2.0, ortns.enm.a, BigInt(ortns.bits.BITF_a));
if (rc < 0)
	return false;

const badobj: ortns.foo|null = ctx.db_foo_get_id(BigInt(3));
if (badobj !== null)
	return false;

const obj: ortns.foo|null = ctx.db_foo_get_id(rc);
if (obj === null)
	return false;
if (obj.obj.id !== rc)
	return false;
if (obj.obj.a !== 'a')
	return false;
if (obj.obj.c !== ortns.enm.a)
	return false;
if (!(obj.obj.d & BigInt(ortns.bits.BITF_a)))
	return false;

return true;
