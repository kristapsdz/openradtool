const num1: ortJson.Long|null =
	ortJson.Long.fromString('a');
if (num1 !== null)
	return false;

const num2: ortJson.Long|null =
	ortJson.Long.fromString('1a');
if (num2 !== null)
	return false;

const num3: ortJson.Long|null =
	ortJson.Long.fromString('a1');
if (num3 !== null)
	return false;

const num4: ortJson.Long|null =
	ortJson.Long.fromString('-4-');
if (num4 !== null)
	return false;

const num5: ortJson.Long|null =
	ortJson.Long.fromString('-');
if (num5 !== null)
	return false;

const num6: ortJson.Long|null =
	ortJson.Long.fromString('');
if (num6 !== null)
	return false;

return true;
