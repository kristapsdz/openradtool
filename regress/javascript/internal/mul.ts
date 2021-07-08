/* Always be zero. */

if (!ortJson.Long.ZERO.mul(ortJson.Long.ZERO).isZero())
	return false;
if (!ortJson.Long.ONE.mul(ortJson.Long.ZERO).isZero())
	return false;
if (!ortJson.Long.MAX_VALUE.mul(ortJson.Long.ZERO).isZero())
	return false;
if (!ortJson.Long.MAX_UNSIGNED_VALUE.mul(ortJson.Long.ZERO).isZero())
	return false;
if (!ortJson.Long.ZERO.mul(ortJson.Long.MUL).isZero())
	return false;
if (!ortJson.Long.ZERO.mul(ortJson.Long.MAX_VALUE).isZero())
	return false;
if (!ortJson.Long.ZERO.mul(ortJson.Long.MAX_UNSIGNED_VALUE).isZero())
	return false;

/* Identity. */

if (!ortJson.Long.ONE.mul(ortJson.Long.ONE).eq(ortJson.Long.ONE))
	return false;
if (!ortJson.Long.ONE.mul(ortJson.Long.MAX_VALUE).eq(ortJson.Long.MAX_VALUE))
	return false;
if (!ortJson.Long.ONE.mul(ortJson.Long.MIN_VALUE).eq(ortJson.Long.MIN_VALUE))
	return false;

/* Negation. */

const num1: ortJson.Long = ortJson.Long.fromNumber(-1);

if (!num1.mul(ortJson.Long.ONE).eq(num1))
	return false;
if (!num1.mul(ortJson.Long.fromNumber(100)).eq(ortJson.Long.fromNumber(-100)))
	return false;
if (!num1.mul(ortJson.Long.fromNumber(1000)).eq(ortJson.Long.fromNumber(-1000)))
	return false;
if (!num1.mul(ortJson.Long.fromNumber(2147483648)).eq(ortJson.Long.fromNumber(-2147483648)))
	return false;

/* Double negation. */

if (!num1.mul(ortJson.Long.fromNumber(-100)).eq(ortJson.Long.fromNumber(100)))
	return false;
if (!num1.mul(ortJson.Long.fromNumber(-1000)).eq(ortJson.Long.fromNumber(1000)))
	return false;
if (!num1.mul(ortJson.Long.fromNumber(-2147483648)).eq(ortJson.Long.fromNumber(2147483648)))
	return false;

return true;
