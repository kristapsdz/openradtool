# Introduction

**openradtool** ("**ort**") translates a data model (data layout and
operations) into C, SQL ([SQLite3](https://sqlite.org)), and
TypeScript/JavaScript code.  It also has a raft of related features,
such as role-based access control, auditing, full documentation of
generated sources, etc.

This repository consists of bleeding-edge code between versions: to keep
up to date with the current stable release of **openradtool**, visit the
[website](https://kristaps.bsd.lv/openradtool).
The website also contains canonical installation, deployment, examples,
and usage documentation.

What follows describes using the bleeding-edge version of the system.

# Installation

First, make sure the depending libraries
[kcgi](https://kristaps.bsd.lv/kcgi)
and
[sqlbox](https://kristaps.bsd.lv/sqlbox)
are up to date.

Then clone or update your sources.  Configure with `./configure`,
compile with `make` (BSD make, so it may be `bmake` on your system),
then `make install` (or use `sudo` or `doas`, if applicable).

To install in an alternative directory to `/usr/local`, set the `PREFIX`
variable when you run `configure`.

```sh
./configure PREFIX=$HOME/.local
make
make install
```

Good luck!

# License

All sources use the ISC (like OpenBSD) license.  See the
[LICENSE.md](LICENSE.md) file for details.
