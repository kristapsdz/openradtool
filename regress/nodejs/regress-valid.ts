if (ortvalid.ortValids['foo-name']('hello') === null)
	process.exit(1);
if (ortvalid.ortValids['foo-name']('') !== null)
	process.exit(1);
if (ortvalid.ortValids['foo-name'](null) !== null)
	process.exit(1);
if (ortvalid.ortValids['foo-name'](undefined) !== null)
	process.exit(1);

