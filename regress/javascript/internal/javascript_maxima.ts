const num1: ortJson.Long|null =
	ortJson.Long.fromString(Number.MAX_SAFE_INTEGER.toString(), true);
if (num1 === null)
	return false;
if (num1.toNumber() !== Number.MAX_SAFE_INTEGER)
	return false;

const num2: ortJson.Long|null =
	ortJson.Long.fromString(Number.MAX_SAFE_INTEGER.toString(), false);
if (num2 === null)
	return false;
if (num2.toNumber() !== Number.MAX_SAFE_INTEGER)
	return false;

const num3: ortJson.Long|null =
	ortJson.Long.fromString(Number.MIN_SAFE_INTEGER.toString());
if (num3 === null)
	return false;
if (num3.toNumber() !== Number.MIN_SAFE_INTEGER)
	return false;

const num4: ortJson.Long =
	ortJson.Long.fromNumber(Number.MAX_SAFE_INTEGER, true);
if (num4.toNumber() !== Number.MAX_SAFE_INTEGER)
	return false;

const num5: ortJson.Long =
	ortJson.Long.fromNumber(Number.MAX_SAFE_INTEGER, false);
if (num5.toNumber() !== Number.MAX_SAFE_INTEGER)
	return false;

const num6: ortJson.Long =
	ortJson.Long.fromNumber(Number.MIN_SAFE_INTEGER);
if (num6.toNumber() !== Number.MIN_SAFE_INTEGER)
	return false;

return true;
