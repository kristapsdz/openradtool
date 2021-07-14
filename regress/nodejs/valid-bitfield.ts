let val: bigint|null;

if ((val = ortvalid.ortValids['foo-bt']('0')) === null)
	return false;
else if (val !== BigInt(0))
	return false;

if ((val = ortvalid.ortValids['foo-bt']('4294967296')) === null)
	return false;
else if (!(val & (2n ** 32n)))
	return false;

if ((val = ortvalid.ortValids['foo-bt']('4294967296')) === null)
	return false;
else if (!(val & (2n ** 32n)))
	return false;

/* All bits. */

if ((val = ortvalid.ortValids['foo-bt']('18446744073709551615')) === null)
	return false;
else if (!(val & (2n ** 64n - 1n)))
	return false;

/* Out of range. */

if ((val = ortvalid.ortValids['foo-bt']('18446744073709551616')) !== null)
	return false;

return true;
