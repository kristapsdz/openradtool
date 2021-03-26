const num: ortJson.Long|null =
	ortJson.Long.fromString('123');
if (num === null)
	return false;

return num.toNumber() === 123;
