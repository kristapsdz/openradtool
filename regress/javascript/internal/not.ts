const num1: ortJson.Long|null =
	ortJson.Long.fromValue(ortJson.Long.UZERO);
if (num1 === null)
	return false;
if (!num1.not().eq(ortJson.Long.MAX_UNSIGNED_VALUE))
	return false;

/* Unsigned vs signed. */

const num2: ortJson.Long|null =
	ortJson.Long.fromValue(ortJson.Long.ZERO);
if (num2 === null)
	return false;
if (num2.not().eq(ortJson.Long.MAX_UNSIGNED_VALUE))
	return false;

return true;

