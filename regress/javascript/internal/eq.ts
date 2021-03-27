/* Same bit pattern, different sign. */

const num1: ortJson.Long|null =
	ortJson.Long.fromValue('-32');
if (num1 === null)
	return false;
const num2: ortJson.Long|null =
	ortJson.Long.fromString('18446744073709551584', true);
if (num2 === null)
	return false;
if (num1.eq(num2))
	return false;

/* ...except zero. */

if (!ortJson.Long.ZERO.eq(ortJson.Long.UZERO))
	return false;

/* Signed vs unsigned, same value. */

if (ortJson.Long.ONE.eq(ortJson.Long.ONE.neg()))
	return false;

/* Extrema. */

if (!ortJson.Long.MAX_UNSIGNED_VALUE.eq(ortJson.Long.MAX_UNSIGNED_VALUE))
	return false;

return true;
