const num1: ortJson.Long =
	ortJson.Long.fromString('0', true);
if (!num1.eq(ortJson.Long.ZERO))
	return false;

const num2: ortJson.Long =
	ortJson.Long.fromString('0', false);
if (!num2.eq(ortJson.Long.ZERO))
	return false;

const num3: ortJson.Long =
	ortJson.Long.fromString('0', true);
if (!num3.eq(ortJson.Long.ZERO))
	return false;

const num4: ortJson.Long =
	ortJson.Long.fromString('0', false);
if (!num4.eq(ortJson.Long.ZERO))
	return false;

return true;
