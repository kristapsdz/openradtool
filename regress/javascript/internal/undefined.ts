const num: ortJson.Long|null =
	ortJson.Long.fromValue(undefined);
if (num !== null)
	return false;

const num: ortJson.Long|null|undefined =
	ortJson.Long.fromValueUndef(undefined);
if (num !== undefined)
	return false;

return true;
