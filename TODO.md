- Validation for FTYPE\_EPOCH needs 32-bit check for those architectures.
- Warning is emitted about singular results for iteration when child
  structures are unique, which is wrong: only the root structure should
  have this check.
- Don't emit fill functions if they're never used.
- Don't emit validation functions if they're the plain-old versions.
- Add default for enumerations.
- Allow for noexport statements for queries.
- Allow unary isnull/notnull operations for password queries.
