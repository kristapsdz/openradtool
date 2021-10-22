
const db: ortdb = ort(dbfile);
const ctx: ortctx = db.connect();

const id1: bigint = ctx.db_foo_insert('testing', 'foobarbaz', ortns.enm.a, BigInt(0x01));
if (id1 < 0)
	return false;

const obj1: ortns.foo|null = ctx.db_foo_get_id(id1);
if (obj1 === null)
	return false;

if (JSON.stringify(obj1.export()) !== '{"a":"testing","c":"200","d":"1","id":"1"}')
	return false;

return true;
