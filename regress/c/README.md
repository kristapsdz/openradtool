These are tests for full C applications.  Driven by `make regress` in
the source root.

In order to be picked up by the regression framework, the operating
system must have [sqlbox](https://kristaps.bsd.lv/sqlbox),
[kcgi](https://kristaps.bsd.lv/kcgi), and
[libcurl](https://curl.se/libcurl/) installed as found by the Makefile.

Each regression tests consists of an ort and C source file (.ort, .c)
pair.

The regression suite converts each ort file into C source, header, and SQL file
using `ort-c-source`, `ort-c-header`, and `ort-sql`.  The suite generates a
temporary database from the SQL for each test.

The generated C source is then compiled along with the given C source of
the test pair into a binary.

Each regression C source should include [regress.h](regress.h), which has the
testing functions themselves.  These are defined and documented in
[regress.c](regress.c).  It should also include a header file by the same name
(e.g., *foo.c* should include *foo.ort.h*), which includes the validations,
database routines, and JSON routines embodied by the ort configuration.

See [simple.c](simple.c) and [simple.ort](simple.ort) for an example.
