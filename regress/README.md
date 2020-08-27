Top-level regression test directory.  Language-specific regression tests
are in subdirectories.  This directory can run on any operating system
that can compile the openradtool binaries: it has no dependencies.

The files in this directory are tested only for being parsed properly
(or for failing to parse as expected).

There are two types of test:

1. Test files ending in .nresult should fail compilation.
2. Text files ending in .result have their corresponding .ort files
   translated and the output compared.  They must match.
