This directory contains the test harness for ort-json(1) and
ort-json.ts.  It is intended to be run from `make regress`, so long as
there is a valid `node_modules/.bin/ts-node` stemming from the root
directory of the source.

```
npm install
make regress
```

The regression tests converts all the examples in *regress* to JSON
using ort-json(1), then converts them back using the functions in
ort-json.ts, then checks that they're the same with ort-diff(1).

To manually run the framework:

```
cat ort-json.ts regress/json/regress-runner.ts > regress.ts
node_modules/.bin/ts-node --skip-project regress.ts
rm -f regress.ts
```

Of course, this uses a temporary *regress.ts* file to store the full
code to run.
