const num1: ortJson.Long|null =
	ortJson.Long.fromValue(ortJson.Long.ZERO);
if (num1 === null)
	return false;
if (!num1.or(ortJson.Long.ZERO).eq(ortJson.Long.ZERO))
	return false;
const num2: ortJson.Long|null =
	ortJson.Long.fromValue(ortJson.Long.MAX_UNSIGNED_VALUE);
if (num2 === null)
	return false;
if (!num2.or(num1).eq(ortJson.Long.MAX_UNSIGNED_VALUE))
	return false;

const num3: ortJson.Long|null =
	ortJson.Long.fromString('1');
if (num3 === null)
	return false;
const num4: ortJson.Long|null =
	ortJson.Long.fromString('2');
if (num4 === null)
	return false;
const num5: ortJson.Long|null =
	ortJson.Long.fromString('3');
if (num5 === null)
	return false;
if (!num3.or(num4).eq(num5))
	return false;

return true;

