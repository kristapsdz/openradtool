# Introduction

**openradtool** ("**ort**") translates a data model (data layout and
operations) into C, SQL ([SQLite3](https://sqlite.org)), and TypeScript
code.  It also has a raft of related features, such as role-based access
control, auditing, full documentation of generated sources, etc.

This repository consists of bleeding-edge code between versions: to keep
up to date with the current stable release of **openradtool**, visit the
[website](https://kristaps.bsd.lv/openradtool).  The website also
contains canonical installation, deployment, examples, and usage
documentation.

What follows describes using the bleeding-edge version of the system.

# Installation

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

To test JSON output, you'll need both the Node.js installation (as described
above) and the [py-jsonschema](http://github.com/Julian/jsonschema) utilities
installed for validating JSON against a schema.

The C interface tests need [kcgi](https://kristaps.bsd.lv/kcgi) and
[libcurl](https://curl.se/libcur) installed.  The regression suite will
automatically pick these up.

# License

All sources use the ISC (like OpenBSD) license.  See the
[LICENSE.md](LICENSE.md) file for details.
