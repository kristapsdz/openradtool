What follows is a list of features needing imminent work.

- *No immediate needs.*

What follows is a list of features needing soon-ish work.  The system is
usable without them, but they'll soon be required.

- Make "db" prefix not be hardcoded.
- Make "DB" preprocessor prefix not be hardcoded.
- Allow for all structures to have a prefix.
- Duplicate "unnamed" functions.  Right now there's no check for
  duplicate function names.  For example, we might have two update
  functions for the same thing.  This is pretty easy to check.
- Allow for unary operators and binary equality (eq, neq) on password
  fields.  Right now, I only allow equality.
- The integers exported are in 64-bit values, which JavaScript does not
  support. 
- Provide a facility to export bitfields as an array of bit mask
  components, e.g., 0 -> [0], 3 -> [1, 2].

More longer-term:

- Allow for more complex queries that would pass multiple structures
  into a search function.  This would occur outside of the "struct" and
  in the main area.
- Allow for prepopulating databases with some initial fields.
- Allow for generic SQL (?) being attached to queries.

Portability notes:

- Validation for FTYPE\_EPOCH will need 32-bit check.
