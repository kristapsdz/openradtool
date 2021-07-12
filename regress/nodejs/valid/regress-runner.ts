let str: string|null;

if (ortvalid.ortValids['foo-mail']('hello') !== null)
	process.exit(1);
if (ortvalid.ortValids['foo-mail']('hello@') !== null)
	process.exit(1);
if ((str = ortvalid.ortValids['foo-mail']('hello@hello.com')) === null)
	process.exit(1);
else if (str !== 'hello@hello.com')
	process.exit(1);
if ((str = ortvalid.ortValids['foo-mail']('  hello@hello.com  ')) === null)
	process.exit(1);
else if (str !== 'hello@hello.com')
	process.exit(1);

if ((str = ortvalid.ortValids['foo-txt']('hello')) === null)
	process.exit(1);
else if (str !== 'hello')
	process.exit(1);
if ((str = ortvalid.ortValids['foo-txt'](1234)) === null)
	process.exit(1);
else if (str !== '1234')
	process.exit(1);
if ((str = ortvalid.ortValids['foo-txt'](123.456)) === null)
	process.exit(1);
else if (str !== '123.456')
	process.exit(1);
if ((str = ortvalid.ortValids['foo-txt'](' hello ')) === null)
	process.exit(1);
else if (str !== ' hello ')
	process.exit(1);
if (ortvalid.ortValids['foo-txt']('0123456789') !== null)
	process.exit(1);
if (ortvalid.ortValids['foo-txt']('') !== null)
	process.exit(1);
if (ortvalid.ortValids['foo-txt'](null) !== null)
	process.exit(1);
if (ortvalid.ortValids['foo-txt'](undefined) !== null)
	process.exit(1);

if ((str = ortvalid.ortValids['foo-txteq']('12')) === null)
	process.exit(1);
else if (str !== '12')
	process.exit(1);
if ((str = ortvalid.ortValids['foo-txteq'](12)) === null)
	process.exit(1);
else if (str !== '12')
	process.exit(1);
if ((str = ortvalid.ortValids['foo-txteq']('  ')) === null)
	process.exit(1);
else if (str !== '  ')
	process.exit(1);
if (ortvalid.ortValids['foo-txteq']('1') !== null)
	process.exit(1);
if (ortvalid.ortValids['foo-txteq']('123') !== null)
	process.exit(1);

if ((str = ortvalid.ortValids['foo-txtineq']('12')) === null)
	process.exit(1);
else if (str !== '12')
	process.exit(1);
if ((str = ortvalid.ortValids['foo-txtineq']('123')) === null)
	process.exit(1);
else if (str !== '123')
	process.exit(1);
if (ortvalid.ortValids['foo-txteq']('1') !== null)
	process.exit(1);
if (ortvalid.ortValids['foo-txteq']('1234') !== null)
	process.exit(1);

let rl: number|null;

if ((rl = ortvalid.ortValids['foo-rl']('-1.0')) === null)
	process.exit(1);
else if (rl !== -1)
	process.exit(1);
if ((rl = ortvalid.ortValids['foo-rl']('1.0')) === null)
	process.exit(1);
else if (rl !== 1)
	process.exit(1);
if ((rl = ortvalid.ortValids['foo-rl']('1')) === null)
	process.exit(1);
else if (rl !== 1)
	process.exit(1);
if ((rl = ortvalid.ortValids['foo-rl'](' 1 ')) === null)
	process.exit(1);
else if (rl !== 1)
	process.exit(1);
if ((rl = ortvalid.ortValids['foo-rl'](-0.5)) === null)
	process.exit(1);
else if (rl !== -0.5)
	process.exit(1);

if (ortvalid.ortValids['foo-rl']('-1.1') !== null)
	process.exit(1);
if (ortvalid.ortValids['foo-rl'](1.1) !== null)
	process.exit(1);
if (ortvalid.ortValids['foo-rl']('a') !== null)
	process.exit(1);
if (ortvalid.ortValids['foo-rl']('') !== null)
	process.exit(1);
if (ortvalid.ortValids['foo-rl'](' ') !== null)
	process.exit(1);

if ((rl = ortvalid.ortValids['foo-rleq']('-1.001')) === null)
	process.exit(1);
else if (rl !== -1.001)
	process.exit(1);
if (ortvalid.ortValids['foo-rleq']('-1.002') !== null)
	process.exit(1);
if (ortvalid.ortValids['foo-rleq'](1.001) !== null)
	process.exit(1);

let num: BigInt|null;

if ((num = ortvalid.ortValids['foo-num'](-1)) === null)
	process.exit(1);
else if (num !== BigInt(-1))
	process.exit(1);
if ((num = ortvalid.ortValids['foo-num'](9)) === null)
	process.exit(1);
else if (num !== BigInt(9))
	process.exit(1);
if ((num = ortvalid.ortValids['foo-num']('99')) === null)
	process.exit(1);
else if (num !== BigInt(99))
	process.exit(1);
if ((num = ortvalid.ortValids['foo-num'](' 99 ')) === null)
	process.exit(1);
else if (num !== BigInt(99))
	process.exit(1);
if ((num = ortvalid.ortValids['foo-num']('-99')) === null)
	process.exit(1);
else if (num !== BigInt(-99))
	process.exit(1);
if ((num = ortvalid.ortValids['foo-num']('0000')) === null)
	process.exit(1);
else if (num !== BigInt(0))
	process.exit(1);
if (ortvalid.ortValids['foo-num']('abc') !== null)
	process.exit(1);
if (ortvalid.ortValids['foo-num']('1.1') !== null)
	process.exit(1);
if (ortvalid.ortValids['foo-num']('1/1') !== null)
	process.exit(1);
if (ortvalid.ortValids['foo-num']('') !== null)
	process.exit(1);
if (ortvalid.ortValids['foo-num'](' ') !== null)
	process.exit(1);

if ((num = ortvalid.ortValids['foo-numeq']('-9223372036854775807')) === null)
	process.exit(1);
else if (num !== BigInt('-9223372036854775807'))
	process.exit(1);

if ((num = ortvalid.ortValids['foo-dt']('2020-04-04')) === null)
	process.exit(1);
else if (num !== BigInt('1585958400'))
	process.exit(1);
if ((num = ortvalid.ortValids['foo-dt'](' 2020-04-04 ')) === null)
	process.exit(1);
else if (num !== BigInt('1585958400'))
	process.exit(1);
if ((num = ortvalid.ortValids['foo-dt']('1970-01-01')) === null)
	process.exit(1);
else if (num !== BigInt('0'))
	process.exit(1);
if ((num = ortvalid.ortValids['foo-dt']('1969-01-01')) === null)
	process.exit(1);
else if (num !== BigInt('-31536000'))
	process.exit(1);
if (ortvalid.ortValids['foo-dt']('20200404') !== null)
	process.exit(1);
if (ortvalid.ortValids['foo-dt']('2020/04/04') !== null)
	process.exit(1);

if ((num = ortvalid.ortValids['foo-bt']('1')) === null)
	process.exit(1);
else if (num !== BigInt('1'))
	process.exit(1);
if ((num = ortvalid.ortValids['foo-bt']('64')) === null)
	process.exit(1);
else if (num !== BigInt('64'))
	process.exit(1);
if (ortvalid.ortValids['foo-bt'](65) !== null)
	process.exit(1);
if (ortvalid.ortValids['foo-bt'](-1) !== null)
	process.exit(1);

if ((num = ortvalid.ortValids['foo-btineq'](' 3 ')) === null)
	process.exit(1);
else if (num !== BigInt('3'))
	process.exit(1);
if (ortvalid.ortValids['foo-btineq'](5) !== null)
	process.exit(1);

let bz: ortns.baz|null;

if (ortvalid.ortValids['foo-bz']('a') !== null)
	process.exit(1);
if ((bz = ortvalid.ortValids['foo-bz']('1')) === null)
	process.exit(1);
else if (bz !== ortns.baz.a)
	process.exit(1);
if ((bz = ortvalid.ortValids['foo-bz'](' 1 ')) === null)
	process.exit(1);
else if (bz !== ortns.baz.a)
	process.exit(1);

if ((bz = ortvalid.ortValids['foo-bz'](2)) === null)
	process.exit(1);
else if (bz !== ortns.baz.b)
	process.exit(1);

