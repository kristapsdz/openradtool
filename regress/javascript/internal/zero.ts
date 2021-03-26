const num1: ortJson.Long|null =
	ortJson.Long.fromString('0', true);
if (num1 === null)
	return false;
if (!num1.eq(ortJson.Long.ZERO))
	return false;
if (num1.toNumber() !== 0)
	return false;

const num2: ortJson.Long|null =
	ortJson.Long.fromString('0', false);
if (num2 === null)
	return false;
if (!num2.eq(ortJson.Long.ZERO))
	return false;
if (num2.toNumber() !== 0)
	return false;

const num3: ortJson.Long|null =
	ortJson.Long.fromString('0', true);
if (num3 === null)
	return false;
if (!num3.eq(ortJson.Long.ZERO))
	return false;
if (num3.toNumber() !== 0)
	return false;

const num4: ortJson.Long|null =
	ortJson.Long.fromString('0', false);
if (num4 === null)
	return false;
if (!num4.eq(ortJson.Long.ZERO))
	return false;
if (num4.toNumber() !== 0)
	return false;

const num5: ortJson.Long =
	ortJson.Long.fromNumber(0, false);
if (!num5.eq(ortJson.Long.ZERO))
	return false;
if (num5.toNumber() !== 0)
	return false;

const num6: ortJson.Long =
	ortJson.Long.fromNumber(0, true);
if (!num6.eq(ortJson.Long.ZERO))
	return false;
if (num6.toNumber() !== 0)
	return false;

return true;
