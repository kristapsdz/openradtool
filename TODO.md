What follows is a list of features needing imminent work.  The system is
usable without them, but they'll soon be required.

- Make "db" prefix not be hardcoded.
- Make "DB" preprocessor prefix not be hardcoded.
- Allow for all structures to have a prefix.
- Duplicate "unnamed" functions.  Right now there's no check for
  duplicate function names.  For example, we might have two update
  functions for the same thing.  This is pretty easy to check.
- Allow for unary operators and binary equality (eq, neq) on password
  fields.  Right now, I only allow equality.

More longer-term:

- Allow for more complex queries that would pass multiple structures
  into a search function.  This would occur outside of the "struct" and
  in the main area.
- Allow for prepopulating databases with some initial fields.
- When deletions occur, override the "ON DELETE" clause.
- Allow for generic SQL (?) being attached to queries.
- Allow for JavaScript options to allow custom callbacks.
- Allow for JavaScript list/iterator support.
- Have the configure script also build in certain functions; e.g., the
  encryption function.
