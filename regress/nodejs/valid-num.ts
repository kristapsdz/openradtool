let val: BigInt|null;

if ((val = ortvalid.ortValids['foo-bt']('0')) === null)
	return false;
else if (val !== BigInt(0))
	return false;

if ((val = ortvalid.ortValids['foo-bt']('4294967296')) === null)
	return false;
else if (val !== (2n ** 32n))
	return false;

if ((val = ortvalid.ortValids['foo-bt'](2n ** 63n - 1n)) === null)
	return false;
else if (val !== (2n ** 63n - 1n))
	return false;

if ((val = ortvalid.ortValids['foo-bt'](-(2n ** 63n))) === null)
	return false;
else if (val !== BigInt('-9223372036854775808'))
	return false;

/* Out of range. */

if (ortvalid.ortValids['foo-bt']('18446744073709551615') !== null)
	return false;
if (ortvalid.ortValids['foo-bt']('9223372036854775808') !== null)
	return false;
if (ortvalid.ortValids['foo-bt']('-9223372036854775809') !== null)
	return false;

return true;
