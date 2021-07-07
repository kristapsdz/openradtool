/* Same bit pattern, different sign. */

const num1: ortJson.Long = ortJson.Long.ZERO;
if (!num1.shl(0).eq(ortJson.Long.ZERO))
	return false;
if (!num1.shl(32).eq(ortJson.Long.ZERO))
	return false;
if (!num1.shl(64).eq(ortJson.Long.ZERO))
	return false;

/* Test zero, default, going into max value, and overflow. */

const num2: ortJson.Long = ortJson.Long.ONE;
if (!num2.shl(0).eq(ortJson.Long.ONE))
	return false;
if (!num2.shl(1).eq(ortJson.Long.fromNumber(2)))
	return false;
if (!num2.shl(63).eq(ortJson.Long.fromStringZero('9223372036854775808')))
	return false;
if (!num2.shl(64).eq(ortJson.Long.ONE))
	return false;
if (!num2.shl(65).eq(ortJson.Long.fromNumber(2)))
	return false;
if (!num2.shl(128).eq(ortJson.Long.ONE))
	return false;
if (!num2.shl(129).eq(ortJson.Long.fromNumber(2)))
	return false;

return true;
