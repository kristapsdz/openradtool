# Introduction

*kwebapp* translates business logic of a database application---objects
and queries---into C, SQL ([SQLite](https://sqlite.org)), and JavaScript
code.  The business logic is given as a configuration file, for example:

```
struct user {
  field name text null comment "full name (or null)";
  field email text unique comment "unique e-mail";
  field hash password comment "hashed password";
  field id int rowid comment "unique identifier";
  search id: comment "search by user identifier";
  search email,password: name creds comment "lookup by credentials";
  comment "a system user";
};
struct session {
  field user struct uid;
  field uid:user.id int comment "user bound to session";
  field token int comment "unique session token";
  field id int rowid;
  comment "web session";
};
```

This configuration is then translated into a C API (header file and
implementation) and an SQL schema or update sequence.  The API consists
of "getters", "setters", updaters, and deleters; and is implemented in
straight-forward C code you link directly into your application.

The generated files are currently
[OpenBSD](https://www.openbsd.org)-only and use
[ksql(3)](https://kristaps.bsd.lv/ksql) to wrap around SQLite and
(optionally)
[kcgi(3)](https://kristaps.bsd.lv/kcgi) for JSON or validation output.

**You will need the newest library version to use *kwebapp*.**  All of these tools are built in tandem.

Why is *kwebapp* handy?  It removes a lot of "boilerplate" code querying
the database and allocating objects.  Some more features:

- Functions generated for modifying (updates), allocating (inserts), and
  accessing (queries).
- Automagic foreign key "INNER JOIN" when accessing structures.  You
  specify nested structures in your configuration file, and *kwebapp*
  creates the "join" functionality when querying for those objects.
- Several types of accessors: iterator-based (function callback),
  list-based (queue), and unique (selecting on unique fields).
- Well-formed, readable C and SQL output describing functions (if
  applicable), variables, structures, and so on.
- "Difference calculator" between configuration files helps database
  updates be reasonable by providing the necessary "ALTER TABLE" and so
  on to help with versioning.  (Also protects against accidental
  incompatible changes.)
- Beyond the usual native type support (int, text, real, blob), also
  supports "password" type that has automatic hashing mechanism built-in
  during selection from and insertion into the database.
- Several different types of SQL query (and update and delete)
  operators.
- Optional JSON output functions.
- Optional field validation ([kcgi(3)](https://kristaps.bsd.lv/kcgi))
  functions.

See the [TODO](TODO.md) for what still needs to be done.

This repository mirrors the main repository on
[BSD.lv](https://www.bsd.lv).  It is still very much under development!

# License

All sources use the ISC (like OpenBSD) license.
See the [LICENSE.md](LICENSE.md) file for details.
