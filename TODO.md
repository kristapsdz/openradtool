- Validation for FTYPE\_EPOCH needs 32-bit check for those architectures.
- Warning is emitted about singular results for iteration when child
  structures are unique, which is wrong: only the root structure should
  have this check.
- Allow for noexport statements for queries.
- Make sure that "nullify" action can only occur with null-ok refs.
- Allow some sort of "search" without qualifiers.
- Allow (eventually) "count" query to have password equality checks.
- Make sure that "count" queries don't have nullrefs.
- Lift restriction that enum and bitfield values should be unique.  (Why!?)
- Allow bitfield field default to be set from a ref like enums.
- Updates to a unique id should have a warning if referrers don't have cascade actup.
