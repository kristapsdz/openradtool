These are tests for full C applications.

In order to be picked up by running `make regress`, the operating system
must sqlbox, kcgi, and libcurl installed.

Each regression tests consists of an ort and C source file (.ort, .c)
pair. The ort file converted into C source, header, and SQL file.  The
SQL file is used to create a database.

The generated C source is then compiled along with the given C source of
the test pair into a binary, which is then executed.
