/* Wraparound. */

const num1: ortJson.Long|null =
	ortJson.Long.fromString('18446744073709551616', true);
if (num1 === null)
	return false;
if (!num1.eq(ortJson.Long.ZERO))
	return false;

/* Wraparound + 1. */

const num2: ortJson.Long|null =
	ortJson.Long.fromString('18446744073709551617', true);
if (num2 === null)
	return false;
if (!num2.eq(ortJson.Long.ONE))
	return false;

return true;
