
if (!ortJson.Long.ZERO.neg().eq(ortJson.Long.UZERO))
	return false;
if (!ortJson.Long.UZERO.neg().eq(ortJson.Long.ZERO))
	return false;

const num1: ortJson.Long = ortJson.Long.fromNumber(1);
if (!num1.neg().eq(ortJson.Long.fromNumber(-1)))
	return false;

const num2: ortJson.Long = ortJson.Long.fromNumber(1000);
if (!num2.neg().eq(ortJson.Long.fromNumber(-1000)))
	return false;

const num3: ortJson.Long = ortJson.Long.fromNumber(4294967295);
if (!num3.neg().eq(ortJson.Long.fromNumber(-4294967295)))
	return false;

const num3: ortJson.Long = ortJson.Long.fromStringZero('9223372036854775808');
if (!num3.neg().eq(ortJson.Long.fromStringZero('-9223372036854775808')))
	return false;

const num6: ortJson.Long = ortJson.Long.fromNumber(-1);
if (!num6.neg().eq(ortJson.Long.fromNumber(1)))
	return false;

const num8: ortJson.Long = ortJson.Long.fromNumber(-1000);
if (!num8.neg().eq(ortJson.Long.fromNumber(1000)))
	return false;

const num7: ortJson.Long = ortJson.Long.fromNumber(-4294967295);
if (!num7.neg().eq(ortJson.Long.fromNumber(4294967295)))
	return false;

const num9: ortJson.Long = ortJson.Long.fromStringZero('-9223372036854775808');
if (!num9.neg().eq(ortJson.Long.fromStringZero('9223372036854775808')))
	return false;

return true;
