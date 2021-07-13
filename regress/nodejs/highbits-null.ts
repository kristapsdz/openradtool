const db: ortdb = ort(dbfile);
const ctx: ortctx = db.connect();

const rc: BigInt = ctx.db_foo_insert(null);
if (rc < 0)
	return false;

if (ctx.db_foo_get_bybits(BigInt(ortns.bits.BITF_lo)) !== null)
	return false;
if (ctx.db_foo_get_bybits(BigInt(ortns.bits.BITF_hi)) !== null)
	return false;
if (ctx.db_foo_get_bybits(BigInt(0)) !== null)
	return false;

/* AND of null always fails. */

if (ctx.db_foo_get_bybits(null) !== null)
	return false;
if (ctx.db_foo_get_bybitsnull() === null)
	return false;

/* AND of null always fails. */

if (!ctx.db_foo_update_bybits(BigInt(ortns.bits.BITF_hi), null))
	return false;
if (ctx.db_foo_get_bybits(BigInt(ortns.bits.BITF_hi)) !== null)
	return false;

if (!ctx.db_foo_update_bybitsnull(BigInt(ortns.bits.BITF_hi)))
	return false;
if (ctx.db_foo_get_bybits(BigInt(ortns.bits.BITF_hi)) === null)
	return false;

if (!ctx.db_foo_update_bybits(null, ortns.bits.BITF_hi))
	return false;
if (ctx.db_foo_get_bybits(BigInt(ortns.bits.BITF_hi)) !== null)
	return false;
if (ctx.db_foo_get_bybitsnull() === null)
	return false;

return true;
