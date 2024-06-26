## Introduction

**openradtool** ("**ort**") translates a data model (data layout and
operations) into C, SQL ([SQLite3](https://sqlite.org)), and TypeScript
code.  It also has a raft of related features, such as role-based access
control, auditing, full documentation of generated sources, etc.

To keep up to date with the current stable release of **openradtool**, visit
https://kristaps.bsd.lv/openradtool.  The website also contains canonical
installation, deployment, examples, and usage documentation.

## Installation

Clone or update your sources.  Configure with `./configure`, compile
with `make` (BSD make, so it may be `bmake` on your system), then `make
install` (or use `sudo` or `doas`, if applicable).

To install in an alternative directory to `/usr/local`, set the `PREFIX`
variable when you run `configure`.

```sh
./configure PREFIX=$HOME/.local
make
make install
```

If you plan on using `pkg-config` with the above invocation, make sure
that *~/.local/lib/pkgconfig* is recognised as a path to package
specifications.  You'll also want to make sure that `man` can access the
installed location of *~/.local/man*, in this case.

Good luck!

## Tests

It's useful to run the installed regression tests on the bleeding edge
sources.  (Again, this uses BSD make, so it may be `bmake` on your
system.)  The full regression suite is extensive and may take several
minutes to run to completion.

None of the tests require an external network, though some will use an
internal network connection to test client/server mode.

```sh
make regress
```

The TypeScript and Node.js regression tests will use Node.js tools if
available, though it will not configure these for you.  You'll need
[npm](https://www.npmjs.com) for this to work.

```sh
npm install
make regress
```

This will use the *package.json* and *package-lock.json* files to
configure the *node_modules* subdirectory required for the regression
tests.  You can safely remove the *node_modules* directory at any time,
though you will need to re-run `npm install` to recreate it.  Once
installed, the regression suite will automatically pick these up.

To enable picking up the Node.js tests, set the `TS_NODE` variable
to the *ts-node* script, usually *node\_modules/.bin/ts-node*, in the
*Makefile*.  Alternatively, drop it into a *Makefile.local* that will be
included by the *Makefile*.

To test JSON output, you'll need both the Node.js installation (as described
above) and the [py-jsonschema](http://github.com/Julian/jsonschema) (sometimes
listed as py3-jsonschema) utilities installed for validating JSON against a
schema.

To enable picking up the JSON tests, set the `TS_JSONSCHEMA` variable to
the *typescript-json-shema* script, usually
*node\_modules/.bin/typescript-json-schema*, in the *Makefile*.
Or alternatively, again, in a *Makefile.local*.

The C interface tests need [kcgi](https://kristaps.bsd.lv/kcgi),
[libcurl](https://curl.se/libcur),
[sqlbox](https://kristaps.bsd.lv/sqlbox), and
[sqlite3](https://sqlite.org) installed.  The regression suite will
automatically pick these up.

The XLIFF tests need [xmllint](http://xmlsoft.org/) to validate the
XLIFF output.  The XSD files are included in the regression suite, so no
network connection is made to fetch them.

To enable picking up the XLIFF tests, set the `XMLLINT` variable to
the *xmllint* program in the *Makefile* or a *Makefile.local*.

The rust tests need [cargo](https://crates.io) to operate.  Prior to
running the tests, dependencies must be downloaded.  Regression builds
are all performed in offline mode.  (The addition of the *main.rs* file
is just to silence the target test.  The file will be overwritten when
regression tests are run.)

```sh
cd rust
mkdir -p rust/src
echo "fn main(){}" > rust/src/main.rs
cargo fetch
cd ..
make regress
```

As usual, you'll need a `CARGO` variable in the *Makefile* or
*Makefile.local* set to the *cargo* binary.

## License

All sources use the ISC (like OpenBSD) license.  See the
[LICENSE.md](LICENSE.md) file for details.
