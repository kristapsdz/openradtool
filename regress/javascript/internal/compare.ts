const num1: ortJson.Long =
	ortJson.Long.fromNumber(2);
const num2: ortJson.Long|null =
	ortJson.Long.fromNumber(3);
if (num1.compare(num2) >= 0)
	return false;
if (num2.compare(num1) <= 0)
	return false;

const num3: ortJson.Long =
	ortJson.Long.fromNumber(-2);
const num4: ortJson.Long|null =
	ortJson.Long.fromNumber(3);
if (num3.compare(num4) >= 0)
	return false;
if (num4.compare(num3) <= 0)
	return false;

const num5: ortJson.Long =
	ortJson.Long.fromNumber(-2);
const num6: ortJson.Long|null =
	ortJson.Long.fromNumber(-1);
if (num5.compare(num6) >= 0)
	return false;
if (num6.compare(num5) <= 0)
	return false;

const num7: ortJson.Long =
	ortJson.Long.fromNumber(-2);
if (num7.compare(num7) !== 0)
	return false;

const num8: ortJson.Long =
	ortJson.Long.fromNumber(2);
if (num8.compare(num8) !== 0)
	return false;

if (ortJson.Long.ZERO.compare(ortJson.Long.ZERO) !== 0)
	return false;
if (ortJson.Long.UZERO.compare(ortJson.Long.ZERO) !== 0)
	return false;
if (ortJson.Long.ZERO.compare(ortJson.Long.UZERO) !== 0)
	return false;

if (ortJson.Long.MAX_UNSIGNED_VALUE.compare(ortJson.Long.MAX_UNSIGNED_VALUE) !== 0)
	return false;

if (ortJson.Long.MAX_UNSIGNED_VALUE.compare(ortJson.Long.ZERO) <= 0)
	return false;
if (ortJson.Long.MAX_UNSIGNED_VALUE.compare(ortJson.Long.MIN_VALUE) <= 0)
	return false;
if (ortJson.Long.MAX_UNSIGNED_VALUE.compare(ortJson.Long.MAX_VALUE) <= 0)
	return false;

return true;
