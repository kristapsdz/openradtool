These are tests for full C applications.  Driven by `make regress` in the
source root.

In order to be picked up by the regression framework, the operating system must
have sqlbox, kcgi, and libcurl installed as found by the Makefile.

Each regression tests consists of an ort and C source file (.ort, .c)
pair. The ort file converted into C source, header, and SQL file.  The
SQL file is used to create a database.

The generated C source is then compiled along with the given C source of
the test pair into a binary, which is then executed.
