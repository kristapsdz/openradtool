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
implementation) and an SQL schema.  The API consists of "getters" and
"setters", and is implemented in straight-forward C code you link
directly into your application.

Why is *kwebapp* handy?  It removes a lot of "boilerplate" code querying
the database and allocating objects.

# License

All sources use the ISC (like OpenBSD) license.
See the [LICENSE.md](LICENSE.md) file for details.
