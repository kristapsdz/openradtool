const num1: ortJson.Long|null =
	ortJson.Long.fromString('0');
if (num1 === null)
	return false;
const num2: ortJson.Long|null =
	ortJson.Long.fromString('123');
if (num2 === null)
	return false;
if (!num1.and(num2).eq(ortJson.Long.ZERO))
	return false;
const num3: ortJson.Long|null =
	ortJson.Long.fromString('-123');
if (num3 === null)
	return false;
if (!num1.and(num3).eq(ortJson.Long.ZERO))
	return false;

const num4: ortJson.Long|null =
	ortJson.Long.fromValue(ortJson.Long.MAX_UNSIGNED_VALUE);
if (num4 === null)
	return false;
const num5: ortJson.Long|null =
	ortJson.Long.fromValue(ortJson.Long.MAX_UNSIGNED_VALUE);
if (num5 === null)
	return false;
if (!num4.and(num5).eq(ortJson.Long.MAX_UNSIGNED_VALUE))
	return false;

if (!ortJson.Long.MAX_VALUE.and
    (ortJson.Long.MAX_UNSIGNED_VALUE).eq(ortJson.Long.MAX_VALUE))
	return false;

const num6: ortJson.Long|null =
	ortJson.Long.fromValue(ortJson.Long.MAX_UNSIGNED_VALUE);
if (num6 === null)
	return false;
const num5: ortJson.Long|null =
	ortJson.Long.fromValue(ortJson.Long.MAX_UNSIGNED_VALUE);

return true
