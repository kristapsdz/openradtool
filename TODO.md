What follows is a list of features needing imminent work.  The system is
usable without them, but they'll soon be required.

- Make "db" prefix not be hardcoded.
- Make "DB" preprocessor prefix not be hardcoded.
- Allow for all structures to have a prefix.
- Naming of "unnamed" functions.  For example, consider
  db\_delete\_by\_xx().  This can be confusing if we have two "xx"
  fields, one being a "notnull" constraint that does not take a
  parameter.
  Any solution will make for long function names, but since we allow
  overriding the name, I'm not too concerned.
- Duplicate "unnamed" functions.  Right now there's no check for
  duplicate function names.  For example, we might have two update
  functions for the same thing.  This is pretty easy to check.

More longer-term:

- Allow for more complex queries that would pass multiple structures
  into a search function.  This would occur outside of the "struct" and
  in the main area.
- Allow for prepopulating databases with some initial fields.
- When deletions occur, override the "ON DELETE" clause.

More output types (not necessarily "long-term" work, but also not really
imminent):

- Create kcgi validators for all fields.  This can be accomplished
  fairly easily for 90% of use cases.  (E.g., have a "min" and "max"
  that checks numeric ranges or string length.)
