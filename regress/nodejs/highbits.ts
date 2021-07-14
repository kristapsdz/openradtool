const db: ortdb = ort(dbfile);
const ctx: ortctx = db.connect();

const bits: bigint = 
	BigInt(ortns.bits.BITF_lo) | BigInt(ortns.bits.BITF_hi);

const rc: bigint = ctx.db_foo_insert(bits);
if (rc < 0)
	return false;

const obj1: ortns.foo|null = 
	ctx.db_foo_get_bybits(BigInt(ortns.bits.BITF_lo));
if (obj1 === null)
	return false;

const obj2: ortns.foo|null = 
	ctx.db_foo_get_bybits(BigInt(ortns.bits.BITF_hi));
if (obj2 === null)
	return false;

const obj6: ortns.foo|null = 
	ctx.db_foo_get_bybits(
		BigInt(ortns.bits.BITF_lo) |
		BigInt(ortns.bits.BITF_hi));
if (obj6 === null)
	return false;

const obj3: ortns.foo|null = 
	ctx.db_foo_get_bybits(BigInt(0));
if (obj3 !== null)
	return false;

if (!ctx.db_foo_update_bybits
    (BigInt(ortns.bits.BITF_hi), 
     BigInt(ortns.bits.BITF_lo) | BigInt(ortns.bits.BITF_hi)))
	return false;

const obj4: ortns.foo|null = 
	ctx.db_foo_get_bybits(BigInt(ortns.bits.BITF_hi));
if (obj4 === null)
	return false;

const obj5: ortns.foo|null = 
	ctx.db_foo_get_bybits(BigInt(ortns.bits.BITF_lo));
if (obj5 !== null)
	return false;

return true;
