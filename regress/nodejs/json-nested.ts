const db: ortdb = ort(dbfile);
const ctx: ortctx = db.connect();

const id1: bigint = ctx.db_bar_insert('foobar');
if (id1 < 0)
	return false;

const id2: bigint = ctx.db_foo_insert(id1, 'testing', 1.2, ortns.enm.a, BigInt(0x01));
if (id2 < 0)
	return false;

const obj1: ortns.foo|null = ctx.db_foo_get_id(id2);
if (obj1 === null)
	return false;

if (JSON.stringify(obj1.export()) !== '{"bar":{"a":"foobar","id":"1"},"barid":"1","a":"testing","b":"1.2","c":"200","d":"1","id":"1"}')
	return false;

return true;
