
const num1: ortJson.Long = ortJson.Long.fromNumber(0);
if (!num1.eq(ortJson.Long.ZERO))
	return false;
const num2: ortJson.Long = ortJson.Long.fromNumber(-0);
if (!num2.eq(ortJson.Long.ZERO))
	return false;
const num3: ortJson.Long = ortJson.Long.fromNumber(1);
if (!num3.eq(ortJson.Long.ONE))
	return false;
const num4: ortJson.Long = ortJson.Long.fromNumber(-1);
if (!num4.eq(ortJson.Long.ONE.neg()))
	return false;
const num5: ortJson.Long = ortJson.Long.fromNumber(100000000);
if (!num5.eq(ortJson.Long.TEN_TO_EIGHT))
	return false;
const num6: ortJson.Long = ortJson.Long.fromNumber(-100000000);
if (!num6.eq(ortJson.Long.TEN_TO_EIGHT.neg()))
	return false;
const num7: ortJson.Long = ortJson.Long.fromNumber(9007199254740991);
if (!num7.eq(Number.MAX_SAFE_INTEGER))
	return false;
const num8: ortJson.Long = ortJson.Long.fromNumber(Number.NaN);
if (!num8.eq(ortJson.Long.ZERO))
	return false;
const num9: ortJson.Long = ortJson.Long.fromNumber(Number.POSITIVE_INFINITY);
if (!num9.eq(ortJson.Long.MAX_VALUE))
	return false;
const num9: ortJson.Long = ortJson.Long.fromNumber(Number.NEGATIVE_INFINITY);
if (!num9.eq(ortJson.Long.MIN_VALUE))
	return false;

return true;

