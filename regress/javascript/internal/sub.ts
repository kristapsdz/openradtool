const num1: ortJson.Long =
	ortJson.Long.fromNumber(2);
const num2: ortJson.Long|null =
	ortJson.Long.fromNumber(3);
if (!num1.sub(num2).eq(ortJson.Long.fromNumber(-1)))
	return false;

const num3: ortJson.Long =
	ortJson.Long.fromNumber(2);
const num4: ortJson.Long|null =
	ortJson.Long.fromNumber(-3);
if (!num3.sub(num4).eq(ortJson.Long.fromNumber(5)))
	return false;

const num5: ortJson.Long =
	ortJson.Long.fromNumber(0);
const num6: ortJson.Long|null =
	ortJson.Long.fromNumber(0);
if (!num5.sub(num6).eq(ortJson.Long.ZERO))
	return false;

if (!ortJson.Long.MAX_UNSIGNED_VALUE.sub(ortJson.Long.ZERO).eq(ortJson.Long.MAX_UNSIGNED_VALUE))
	return false;
if (!ortJson.Long.MIN_VALUE.sub(ortJson.Long.UZERO).eq(ortJson.Long.MIN_VALUE))
	return false;

return true;
