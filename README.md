# Introduction

*kwebapp* translates business logic of a database application---objects
and queries---into C and SQL code.  The business logic is given as a
configuration file, for example:

```
struct user {
  field name text comment "full name";
  field id int rowid comment "unique identifier";
  comment "a system user";
}
```

This configuration is then translated into a C API (header file and
implementation) and an SQL schema or update sequence.  The API consists
of "getters" and "setters", and is implemented in straight-forward C
code you link directly into your application.

Why is *kwebapp* handy?  It removes a lot of "boilerplate" code querying
the database and allocating objects.  Some more features:

- automagic foreign key "inner joins" on returned structures
- functions generated for updates, inserts, and queries
- iterator (function callback), list (queue), and unique queries
- querying on nested fields (foreign key references)
- support for row identifiers, unique fields, and null fields
- well-formed, readable C output
- documentation in generated C and SQL output
- difference engine between configuration helps database updates be
  reasonable
- password support with automatic hashing

This repository mirrors the main repository on
[BSD.lv](https://www.bsd.lv).  It is still very much under development!

# License

All sources use the ISC (like OpenBSD) license.
See the [LICENSE.md](LICENSE.md) file for details.
