- Validation for FTYPE\_EPOCH needs 32-bit check for those architectures.
- Warning is emitted about singular results for iteration when child
  structures are unique, which is wrong: only the root structure should
  have this check.
- Don't emit fill functions if they're never used.
- Don't emit validation functions if they're the plain-old versions.
- Allow for noexport statements for queries.
- Restrict grouprow, maxrow, and minrow not to be passwords or null values.
- Improve maxrow and minrow docs regarding lexicographic max/min.
- Improve docs for distinct.
- Make sure that "nullify" action can only occur with null-ok refs.
- Allow "search limit 1".
