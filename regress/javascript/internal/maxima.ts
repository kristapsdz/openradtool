const num1: ortJson.Long|null =
	ortJson.Long.fromString('18446744073709551615', true);
if (num1 === null)
	return false;
if (!num1.eq(ortJson.Long.MAX_UNSIGNED_VALUE))
	return false;

const num2: ortJson.Long|null =
	ortJson.Long.fromString('9223372036854775807', true);
if (num2 === null)
	return false;
if (!num2.eq(ortJson.Long.MAX_VALUE))
	return false;

const num3: ortJson.Long|null =
	ortJson.Long.fromString('9223372036854775807', false);
if (num3 === null)
	return false;
if (!num3.eq(ortJson.Long.MAX_VALUE))
	return false;

const num4: ortJson.Long|null =
	ortJson.Long.fromString('-9223372036854775808', false);
if (num4 === null)
	return false;
if (!num4.eq(ortJson.Long.MIN_VALUE))
	return false;

return true;
