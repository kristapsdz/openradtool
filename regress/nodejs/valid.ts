let str: string|null;

if (ortvalid.ortValids['foo-mail']('hello') !== null)
	return false;
if (ortvalid.ortValids['foo-mail']('hello@') !== null)
	return false;
if ((str = ortvalid.ortValids['foo-mail']('hello@hello.com')) === null)
	return false;
else if (str !== 'hello@hello.com')
	return false;
if ((str = ortvalid.ortValids['foo-mail']('  hello@hello.com  ')) === null)
	return false;
else if (str !== 'hello@hello.com')
	return false;

if ((str = ortvalid.ortValids['foo-txt']('hello')) === null)
	return false;
else if (str !== 'hello')
	return false;
if ((str = ortvalid.ortValids['foo-txt'](1234)) === null)
	return false;
else if (str !== '1234')
	return false;
if ((str = ortvalid.ortValids['foo-txt'](123.456)) === null)
	return false;
else if (str !== '123.456')
	return false;
if ((str = ortvalid.ortValids['foo-txt'](' hello ')) === null)
	return false;
else if (str !== ' hello ')
	return false;
if (ortvalid.ortValids['foo-txt']('0123456789') !== null)
	return false;
if (ortvalid.ortValids['foo-txt']('') !== null)
	return false;
if (ortvalid.ortValids['foo-txt'](null) !== null)
	return false;
if (ortvalid.ortValids['foo-txt'](undefined) !== null)
	return false;

if ((str = ortvalid.ortValids['foo-txteq']('12')) === null)
	return false;
else if (str !== '12')
	return false;
if ((str = ortvalid.ortValids['foo-txteq'](12)) === null)
	return false;
else if (str !== '12')
	return false;
if ((str = ortvalid.ortValids['foo-txteq']('  ')) === null)
	return false;
else if (str !== '  ')
	return false;
if (ortvalid.ortValids['foo-txteq']('1') !== null)
	return false;
if (ortvalid.ortValids['foo-txteq']('123') !== null)
	return false;

if ((str = ortvalid.ortValids['foo-txtineq']('12')) === null)
	return false;
else if (str !== '12')
	return false;
if ((str = ortvalid.ortValids['foo-txtineq']('123')) === null)
	return false;
else if (str !== '123')
	return false;
if (ortvalid.ortValids['foo-txteq']('1') !== null)
	return false;
if (ortvalid.ortValids['foo-txteq']('1234') !== null)
	return false;

let rl: number|null;

if ((rl = ortvalid.ortValids['foo-rl']('-1.0')) === null)
	return false;
else if (rl !== -1)
	return false;
if ((rl = ortvalid.ortValids['foo-rl']('1.0')) === null)
	return false;
else if (rl !== 1)
	return false;
if ((rl = ortvalid.ortValids['foo-rl']('1')) === null)
	return false;
else if (rl !== 1)
	return false;
if ((rl = ortvalid.ortValids['foo-rl'](' 1 ')) === null)
	return false;
else if (rl !== 1)
	return false;
if ((rl = ortvalid.ortValids['foo-rl'](-0.5)) === null)
	return false;
else if (rl !== -0.5)
	return false;

if (ortvalid.ortValids['foo-rl']('-1.1') !== null)
	return false;
if (ortvalid.ortValids['foo-rl'](1.1) !== null)
	return false;
if (ortvalid.ortValids['foo-rl']('a') !== null)
	return false;
if (ortvalid.ortValids['foo-rl']('') !== null)
	return false;
if (ortvalid.ortValids['foo-rl'](' ') !== null)
	return false;

if ((rl = ortvalid.ortValids['foo-rleq']('-1.001')) === null)
	return false;
else if (rl !== -1.001)
	return false;
if (ortvalid.ortValids['foo-rleq']('-1.002') !== null)
	return false;
if (ortvalid.ortValids['foo-rleq'](1.001) !== null)
	return false;

let num: BigInt|null;

if ((num = ortvalid.ortValids['foo-num'](-1)) === null)
	return false;
else if (num !== BigInt(-1))
	return false;
if ((num = ortvalid.ortValids['foo-num'](9)) === null)
	return false;
else if (num !== BigInt(9))
	return false;
if ((num = ortvalid.ortValids['foo-num']('99')) === null)
	return false;
else if (num !== BigInt(99))
	return false;
if ((num = ortvalid.ortValids['foo-num'](' 99 ')) === null)
	return false;
else if (num !== BigInt(99))
	return false;
if ((num = ortvalid.ortValids['foo-num']('-99')) === null)
	return false;
else if (num !== BigInt(-99))
	return false;
if ((num = ortvalid.ortValids['foo-num']('0000')) === null)
	return false;
else if (num !== BigInt(0))
	return false;
if (ortvalid.ortValids['foo-num']('abc') !== null)
	return false;
if (ortvalid.ortValids['foo-num']('1.1') !== null)
	return false;
if (ortvalid.ortValids['foo-num']('1/1') !== null)
	return false;
if (ortvalid.ortValids['foo-num']('') !== null)
	return false;
if (ortvalid.ortValids['foo-num'](' ') !== null)
	return false;

if ((num = ortvalid.ortValids['foo-numeq']('-9223372036854775807')) === null)
	return false;
else if (num !== BigInt('-9223372036854775807'))
	return false;

if ((num = ortvalid.ortValids['foo-dt']('2020-04-04')) === null)
	return false;
else if (num !== BigInt('1585958400'))
	return false;
if ((num = ortvalid.ortValids['foo-dt'](' 2020-04-04 ')) === null)
	return false;
else if (num !== BigInt('1585958400'))
	return false;
if ((num = ortvalid.ortValids['foo-dt']('1970-01-01')) === null)
	return false;
else if (num !== BigInt('0'))
	return false;
if ((num = ortvalid.ortValids['foo-dt']('1969-01-01')) === null)
	return false;
else if (num !== BigInt('-31536000'))
	return false;
if (ortvalid.ortValids['foo-dt']('20200404') !== null)
	return false;
if (ortvalid.ortValids['foo-dt']('2020/04/04') !== null)
	return false;

if ((num = ortvalid.ortValids['foo-bt']('1')) === null)
	return false;
else if (num !== BigInt('1'))
	return false;
if ((num = ortvalid.ortValids['foo-bt']('64')) === null)
	return false;
else if (num !== BigInt('64'))
	return false;
if (ortvalid.ortValids['foo-bt'](65) !== null)
	return false;
if (ortvalid.ortValids['foo-bt'](-1) !== null)
	return false;

if ((num = ortvalid.ortValids['foo-btineq'](' 3 ')) === null)
	return false;
else if (num !== BigInt('3'))
	return false;
if (ortvalid.ortValids['foo-btineq'](5) !== null)
	return false;

let bz: ortns.baz|null;

if (ortvalid.ortValids['foo-bz']('a') !== null)
	return false;
if ((bz = ortvalid.ortValids['foo-bz']('1')) === null)
	return false;
else if (bz !== ortns.baz.a)
	return false;
if ((bz = ortvalid.ortValids['foo-bz'](' 1 ')) === null)
	return false;
else if (bz !== ortns.baz.a)
	return false;

if ((bz = ortvalid.ortValids['foo-bz'](2)) === null)
	return false;
else if (bz !== ortns.baz.b)
	return false;

return true;
