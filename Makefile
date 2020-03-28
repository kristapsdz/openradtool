.SUFFIXES: .dot .svg .1 .1.html .5 .5.html
.PHONY: regress

include Makefile.configure

VERSION_MAJOR	 = 0
VERSION_MINOR	 = 8
VERSION_BUILD	 = 8
VERSION		:= $(VERSION_MAJOR).$(VERSION_MINOR).$(VERSION_BUILD)
LIBOBJS		 = comments.o \
		   compats.o \
		   config.o \
		   linker.o \
		   parser.o \
		   parser_bitfield.o \
		   parser_enum.o \
		   parser_field.o \
		   parser_roles.o \
		   parser_struct.o \
		   printer.o \
		   protos.o \
		   writer.o
OBJS		 = audit.o \
		   main.o \
		   header.o \
		   javascript.o \
		   source.o \
		   sql.o \
		   xliff.o
HTMLS		 = archive.html \
		   index.html \
		   ort.1.html \
		   ort-audit.1.html \
		   ort-audit-gv.1.html \
		   ort-audit-json.1.html \
		   ort-c-header.1.html \
		   ort-c-source.1.html \
		   ort-javascript.1.html \
		   ort-sql.1.html \
		   ort-sqldiff.1.html \
		   ort-xliff.1.html \
		   ort.5.html
WWWDIR		 = /var/www/vhosts/kristaps.bsd.lv/htdocs/openradtool
MAN1S		 = ort.1 \
		   ort-audit.1 \
		   ort-audit-gv.1 \
		   ort-audit-json.1 \
		   ort-c-header.1 \
		   ort-c-source.1 \
		   ort-javascript.1 \
		   ort-sql.1 \
		   ort-sqldiff.1 \
		   ort-xliff.1
DOTAREXEC	 = configure
DOTAR		 = audit.c \
		   audit.css \
		   audit.html \
		   audit.js \
		   b64_ntop.c \
		   comments.c \
		   compats.c \
		   config.c \
		   extern.h \
		   gensalt.c \
		   header.c \
		   javascript.c \
		   jsmn.c \
		   $(MAN1S) \
		   ort.5 \
		   ort.h \
		   linker.c \
		   Makefile \
		   main.c \
		   parser.c \
		   parser.h \
		   parser_bitfield.c \
		   parser_enum.c \
		   parser_field.c \
		   parser_roles.c \
		   parser_struct.c \
		   printer.c \
		   protos.c \
		   source.c \
		   sql.c \
		   tests.c \
		   test.c \
		   writer.c \
		   xliff.c
XMLS		 = test.xml.xml \
		   versions.xml
IHTMLS		 = audit-example.txt.html \
		   audit-out.js \
		   db.ort.html \
		   db.fr.xml.html \
		   db.h.html \
		   db.c.html \
		   db.sql.html \
		   db.old.ort.html \
		   db.update.sql.html \
		   db.js.html \
		   db.ts.html \
		   db.trans.txt.html \
		   test.c.html
BINS		 = ort \
		   ort-audit \
		   ort-audit-gv \
		   ort-audit-json \
		   ort-c-header \
		   ort-c-source \
		   ort-javascript \
		   ort-sql \
		   ort-sqldiff \
		   ort-xliff
IMAGES		 = index.svg \
		   index2.svg \
		   index-fig0.svg \
		   index-fig1.svg \
		   index-fig2.svg \
		   index-fig3.svg \
		   index-fig4.svg \
		   index-fig5.svg \
		   index-fig6.svg \
		   index-fig7.svg
DIFFARGS	 = -I '^[/ ]\*.*' -I '^\# define KWBP_.*' -w

# Only needed for test, not built by default.
LIBS_SQLBOX	!= pkg-config --libs sqlbox 2>/dev/null || echo "-lsqlbox -lsqlite3"
CFLAGS_SQLBOX	!= pkg-config --cflags sqlbox 2>/dev/null || echo ""

LIBS_PKG	!= pkg-config --libs expat 2>/dev/null || echo "-lexpat"
CFLAGS_PKG	!= pkg-config --cflags expat 2>/dev/null || echo ""

# FreeBSD's make doesn't support CPPFLAGS.
# CFLAGS += $(CPPFLAGS)

all: $(BINS)

afl::
	$(MAKE) clean
	$(MAKE) all CC=afl-gcc
	cp $(BINS) afl

ort: main.o libort.a
	$(CC) -o $@ main.o libort.a

libort.a: $(LIBOBJS)
	$(AR) rs $@ $(LIBOBJS)

ort-c-source: source.o libort.a
	$(CC) -o $@ source.o libort.a $(LDFLAGS) $(LDADD)

ort-c-header: header.o libort.a
	$(CC) -o $@ header.o libort.a $(LDFLAGS) $(LDADD)

ort-javascript: javascript.o libort.a
	$(CC) -o $@ javascript.o libort.a $(LDFLAGS) $(LDADD)

ort-sql: sql.o libort.a
	$(CC) -o $@ sql.o libort.a $(LDFLAGS) $(LDADD)

ort-sqldiff: sql.o libort.a
	$(CC) -o $@ sql.o libort.a $(LDFLAGS) $(LDADD)

ort-audit: audit.o libort.a
	$(CC) -o $@ audit.o libort.a $(LDFLAGS) $(LDADD)

ort-audit-gv: audit.o libort.a
	$(CC) -o $@ audit.o libort.a $(LDFLAGS) $(LDADD)

ort-audit-json: audit.o libort.a
	$(CC) -o $@ audit.o libort.a $(LDFLAGS) $(LDADD)

ort-xliff: xliff.o libort.a
	$(CC) -o $@ xliff.o libort.a $(LDFLAGS) $(LIBS_PKG) $(LDADD)

xliff.o: xliff.c
	$(CC) $(CFLAGS) $(CPPFLAGS) $(CFLAGS_PKG) -c xliff.c

www: $(IMAGES) $(HTMLS) openradtool.tar.gz openradtool.tar.gz.sha512 atom.xml

installwww: www
	mkdir -p $(WWWDIR)
	mkdir -p $(WWWDIR)/snapshots
	$(INSTALL_DATA) *.html *.css *.js $(IMAGES) atom.xml $(WWWDIR)
	$(INSTALL_DATA) openradtool.tar.gz openradtool.tar.gz.sha512 $(WWWDIR)/snapshots
	$(INSTALL_DATA) openradtool.tar.gz $(WWWDIR)/snapshots/openradtool-$(VERSION).tar.gz
	$(INSTALL_DATA) openradtool.tar.gz.sha512 $(WWWDIR)/snapshots/openradtool-$(VERSION).tar.gz.sha512

# Something to run before doing a release: just makes sure that we have
# the given version documented and that make and make install works
# when run on the distributed tarball.
# Also checks that our manpages are nice.

distcheck: openradtool.tar.gz.sha512 openradtool.tar.gz
	mandoc -Tlint -Werror $(MAN1S)
	newest=`grep "<h3>" versions.xml | head -1 | sed 's![ 	]*!!g'` ; \
	       [ "$$newest" = "<h3>$(VERSION)</h3>" ] || \
		{ echo "Version $(VERSION) not newest in versions.xml" 1>&2 ; exit 1 ; }
	rm -rf .distcheck
	[ "`openssl dgst -sha512 -hex openradtool.tar.gz`" = "`cat openradtool.tar.gz.sha512`" ] || \
 		{ echo "Checksum does not match." 1>&2 ; exit 1 ; }
	mkdir -p .distcheck
	( cd .distcheck && tar -zvxpf ../openradtool.tar.gz )
	( cd .distcheck/openradtool-$(VERSION) && ./configure PREFIX=prefix )
	( cd .distcheck/openradtool-$(VERSION) && $(MAKE) )
	( cd .distcheck/openradtool-$(VERSION) && $(MAKE) regress )
	( cd .distcheck/openradtool-$(VERSION) && $(MAKE) install )
	rm -rf .distcheck

version.h: Makefile
	( echo "#define VERSION \"$(VERSION)\"" ; \
	  echo "#define VSTAMP `echo $$((($(VERSION_BUILD)+1)+($(VERSION_MINOR)+1)*100+($(VERSION_MAJOR)+1)*10000))`" ; ) >$@

header.o source.o: version.h
	
parser.o parser_bitfield.o parser_enum.o parser_roles.o parser_field.o: parser.h

paths.h: Makefile
	( echo "#define SHAREDIR \"$(SHAREDIR)/openradtool\"" ; \
	  echo "#define FILE_GENSALT \"gensalt.c\"" ; \
	  echo "#define FILE_B64_NTOP \"b64_ntop.c\"" ; \
	  echo "#define FILE_JSMN \"jsmn.c\"" ; ) >$@

source.o: paths.h

install: all
	mkdir -p $(DESTDIR)$(BINDIR)
	mkdir -p $(DESTDIR)$(MANDIR)/man1
	mkdir -p $(DESTDIR)$(MANDIR)/man5
	mkdir -p $(DESTDIR)$(SHAREDIR)/openradtool
	$(INSTALL_MAN) $(MAN1S) $(DESTDIR)$(MANDIR)/man1
	$(INSTALL_MAN) ort.5 $(DESTDIR)$(MANDIR)/man5
	$(INSTALL_DATA) audit.html audit.css audit.js $(DESTDIR)$(SHAREDIR)/openradtool
	$(INSTALL_DATA) b64_ntop.c jsmn.c gensalt.c $(DESTDIR)$(SHAREDIR)/openradtool
	$(INSTALL_PROGRAM) $(BINS) $(DESTDIR)$(BINDIR)

uninstall:
	@for f in $(MAN1S); do \
		echo rm -f $(DESTDIR)$(MANDIR)/man1/$$f ; \
		rm -f $(DESTDIR)$(MANDIR)/man1/$$f ; \
	done
	rm -f $(DESTDIR)$(MANDIR)/man5/ort.5
	rm -f $(DESTDIR)$(SHAREDIR)/openradtool/audit.{html,css,js}
	rm -f $(DESTDIR)$(SHAREDIR)/openradtool/{b64_ntop,jsmn,gensalt}.c
	rmdir $(DESTDIR)$(SHAREDIR)/openradtool
	@for f in $(BINS); do \
		echo rm -f $(DESTDIR)$(BINDIR)/$$f ; \
		rm -f $(DESTDIR)$(BINDIR)/$$f ; \
	done

openradtool.tar.gz.sha512: openradtool.tar.gz
	openssl dgst -sha512 -hex openradtool.tar.gz >$@

openradtool.tar.gz: $(DOTAR) $(DOTAREXEC)
	mkdir -p .dist/openradtool-$(VERSION)/
	mkdir -p .dist/openradtool-$(VERSION)/regress
	install -m 0444 $(DOTAR) .dist/openradtool-$(VERSION)
	install -m 0444 regress/*.[ch] regress/*.or[ft] .dist/openradtool-$(VERSION)/regress
	install -m 0555 $(DOTAREXEC) .dist/openradtool-$(VERSION)
	( cd .dist/ && tar zcf ../$@ ./ )
	rm -rf .dist/

test: test.o db.o db.db
	$(CC) -o $@ test.o db.o $(LIBS_SQLBOX) $(LDADD_CRYPT)

audit-out.js: ort-audit-json audit-example.txt
	./ort-audit-json user audit-example.txt >$@

db.o: db.c db.h
	$(CC) $(CPPFLAGS) $(CFLAGS) $(CFLAGS_SQLBOX) -o $@ -c db.c

test.o: test.c db.h
	$(CC) $(CPPFLAGS) $(CFLAGS) $(CFLAGS_SQLBOX) -o $@ -c test.c

db.c: ort-c-source db.ort
	./ort-c-source -S. db.ort >$@

db.h: ort-c-header db.ort
	./ort-c-header db.ort >$@

db.sql: ort-sql db.ort
	./ort-sql db.ort >$@

db.js: ort-javascript db.ort
	./ort-javascript db.ort >$@

db.ts: ort-javascript db.ort
	./ort-javascript -t db.ort >$@

db.update.sql: ort-sqldiff db.old.ort db.ort
	./ort-sqldiff db.old.ort db.ort >$@

db.trans.txt: ort-xliff db.ort db.fr.xml
	./ort-xliff -j db.ort db.fr.xml >$@

db.db: db.sql
	rm -f $@
	sqlite3 $@ < db.sql

$(LIBOBJS) $(OBJS): config.h extern.h ort.h

.5.5.html:
	mandoc -Ostyle=https://bsd.lv/css/mandoc.css -Thtml $< >$@

.1.1.html:
	mandoc -Ostyle=https://bsd.lv/css/mandoc.css -Thtml $< >$@

.dot.svg:
	dot -Tsvg $< > tmp-$@
	xsltproc --novalid notugly.xsl tmp-$@ >$@
	rm tmp-$@

db.ort.xml: db.ort
	( echo "<article data-sblg-article=\"1\">" ; \
	  highlight -l --enclose-pre --src-lang=conf -f db.ort ; \
	  echo "</article>" ; ) >$@

db.fr.xml.html: db.fr.xml
	highlight -s whitengrey -I -l --src-lang=xml db.fr.xml | sed 's!ISO-8859-1!UTF-8!g' >$@

db.h.xml: db.h
	( echo "<article data-sblg-article=\"1\">" ; \
	  highlight -l --enclose-pre --src-lang=c -f db.h ; \
	  echo "</article>" ; ) >$@

db.sql.xml: db.sql
	( echo "<article data-sblg-article=\"1\">" ; \
	  highlight -l --enclose-pre --src-lang=sql -f db.sql ; \
	  echo "</article>" ; ) >$@

db.update.sql.xml: db.update.sql
	( echo "<article data-sblg-article=\"1\">" ; \
	  highlight -l --enclose-pre --src-lang=sql -f db.update.sql ; \
	  echo "</article>" ; ) >$@

test.c.html: test.c
	highlight -s whitengrey -I -l --src-lang=c test.c >$@

test.xml.xml: test.xml
	( echo "<article data-sblg-article=\"1\">" ; \
	  highlight -l --enclose-pre --src-lang=xml -f test.xml ; \
	  echo "</article>" ; ) >$@

db.c.html: db.c
	highlight -s whitengrey -I -l --src-lang=c db.c >$@

db.ort.html: db.ort
	highlight -s whitengrey -I -l --src-lang=f db.ort >$@

audit-example.txt.html: audit-example.txt
	highlight -s whitengrey -I -l --src-lang=c audit-example.txt >$@

db.old.ort.html: db.old.ort
	highlight -s whitengrey -I -l --src-lang=conf db.old.ort >$@

db.h.html: db.h
	highlight -s whitengrey -I -l --src-lang=c db.h >$@

db.sql.html: db.sql
	highlight -s whitengrey -I -l --src-lang=sql db.sql >$@

db.js.html: db.js
	highlight -s whitengrey -I -l --src-lang=js db.js >$@

db.ts.html: db.ts
	highlight -s whitengrey -I -l --src-lang=js db.ts >$@

db.update.sql.html: db.update.sql
	highlight -s whitengrey -I -l --src-lang=sql db.update.sql >$@

db.trans.txt.html: db.trans.txt
	highlight -s whitengrey -I -l --src-lang=c db.trans.txt | sed 's!ISO-8859-1!UTF-8!g' >$@

highlight.css:
	highlight --print-style -s whitengrey -O xhtml --stdout >$@

index.html: index.xml $(XMLS) $(IHTMLS) highlight.css
	sblg -s cmdline -t index.xml -o- $(XMLS) >$@

archive.html: archive.xml versions.xml
	sblg -s date -t archive.xml -o- versions.xml >$@

TODO.xml: TODO.md
	( echo "<article data-sblg-article=\"1\">" ; \
	  lowdown -Thtml TODO.md ; \
	  echo "</article>" ; ) >$@

atom.xml: versions.xml atom-template.xml
	sblg -s date -a versions.xml >$@

# Remove what is built by "all" and "www".

clean:
	rm -f $(BINS) version.h paths.h $(LIBOBJS) libort.a test test.o
	rm -f db.c db.h db.o db.sql db.js db.ts db.ts db.update.sql db.db db.trans.txt
	rm -f openradtool.tar.gz openradtool.tar.gz.sha512
	rm -f $(IMAGES) highlight.css $(HTMLS) atom.xml
	rm -f db.ort.xml db.h.xml db.sql.xml db.update.sql.xml test.xml.xml $(IHTMLS) TODO.xml
	rm -f source.o header.o javascript.o sql.o audit.o main.o xliff.o
	rm -rf regress/share

# Remove both what can be built and what's built by ./configure.

distclean: clean
	rm -f config.h config.log Makefile.configure

# The tests go over each type of file in "regress".
# Each test has a configuration (.ort) and a type to cross-check: a C
# file (.c), header file (.h), or configuration (.orf).
# First, make sure the .ort generates a similar C, header, or config.
# Second, create a configuration from the configuration and try again,
# making sure that it's the same.

regress:
	# Do nothing.

xxregress: all
	@tmp=`mktemp` ; \
	tmp2=`mktemp` ; \
	mkdir -p regress/share ; \
	touch regress/share/gensalt.c ; \
	touch regress/share/b64_ntop.c ; \
	touch regress/share/jsmn.c ; \
	for f in regress/*.c ; do \
		bf=`basename $$f .c`.ort ; \
		bff=`basename $$f .c`.orf ; \
		set +e ; \
		echo "ort-c-source -Sregress/share regress/$$bf... " ; \
		./ort-c-source -Sregress/share regress/$$bf > $$tmp ; \
		if [ $$? -ne 0 ] ; then \
			rm $$tmp $$tmp2 ; \
			exit 1 ; \
		fi ; \
		diff $(DIFFARGS) $$tmp $$f >/dev/null 2>&1 ; \
		if [ $$? -ne 0 ] ; then \
			diff $(DIFFARGS) -u $$tmp $$f ; \
			rm $$tmp $$tmp2 ; \
			exit 1 ; \
		fi ; \
		echo "ort-c-source (cross-check...)" ; \
		./ort regress/$$bf > $$tmp2 ; \
		if [ $$? -ne 0 ] ; then \
			rm $$tmp $$tmp2 ; \
			exit 1 ; \
		fi ; \
		./ort-c-source -Sregress/share $$tmp2 > $$tmp ; \
		if [ $$? -ne 0 ] ; then \
			rm $$tmp $$tmp2 ; \
			exit 1 ; \
		fi ; \
		diff $(DIFFARGS) $$tmp $$f >/dev/null 2>&1 ; \
		if [ $$? -ne 0 ] ; then \
			diff $(DIFFARGS) -u $$tmp $$f ; \
			rm $$tmp $$tmp2 ; \
			exit 1 ; \
		fi ; \
		set -e ; \
	done ; \
	for f in regress/*.h ; do \
		bf=`basename $$f .h`.ort ; \
		echo "ort-c-header regress/$$bf... " ; \
		set +e ; \
		./ort-c-header regress/$$bf > $$tmp ; \
		if [ $$? -ne 0 ] ; then \
			rm $$tmp $$tmp2 ; \
			exit 1 ; \
		fi ; \
		diff $(DIFFARGS) $$tmp $$f >/dev/null 2>&1 ; \
		if [ $$? -ne 0 ] ; then \
			diff $(DIFFARGS) -u $$tmp $$f ; \
			rm $$tmp $$tmp2 ; \
			exit 1 ; \
		fi ; \
		echo "ort-c-header (cross-check...)" ; \
		./ort regress/$$bf > $$tmp2 ; \
		if [ $$? -ne 0 ] ; then \
			rm $$tmp $$tmp2 ; \
			exit 1 ; \
		fi ; \
		./ort-c-header $$tmp2 > $$tmp ; \
		if [ $$? -ne 0 ] ; then \
			rm $$tmp $$tmp2 ; \
			exit 1 ; \
		fi ; \
		diff $(DIFFARGS) $$tmp $$f >/dev/null 2>&1 ; \
		if [ $$? -ne 0 ] ; then \
			diff $(DIFFARGS) -u $$tmp $$f ; \
			rm $$tmp $$tmp2 ; \
			exit 1 ; \
		fi ; \
		set -e ; \
	done ; \
	for f in regress/*.orf ; do \
		bf=`basename $$f .orf`.ort ; \
		echo "ort regress/$$bf... " ; \
		set +e ; \
		./ort regress/$$bf > $$tmp ; \
		if [ $$? -ne 0 ] ; then \
			rm $$tmp $$tmp2 ; \
			exit 1 ; \
		fi ; \
		diff -I '^$$' -w $$tmp $$f >/dev/null 2>&1 ; \
		if [ $$? -ne 0 ] ; then \
			diff -i '^$$' -w -u $$tmp $$f ; \
			rm $$tmp $$tmp2 ; \
			exit 1 ; \
		fi ; \
		echo "ort (cross-check...)" ; \
		./ort regress/$$bf > $$tmp2 ; \
		if [ $$? -ne 0 ] ; then \
			rm $$tmp $$tmp2 ; \
			exit 1 ; \
		fi ; \
		./ort $$tmp2 > $$tmp ; \
		if [ $$? -ne 0 ] ; then \
			rm $$tmp $$tmp2 ; \
			exit 1 ; \
		fi ; \
		diff -I '^$$' -w $$tmp $$f >/dev/null 2>&1 ; \
		if [ $$? -ne 0 ] ; then \
			diff -i '^$$' -w -u $$tmp $$f ; \
			rm $$tmp $$tmp2 ; \
			exit 1 ; \
		fi ; \
		set -e ; \
	done ; \
	rm -rf regress/share ; \
	rm $$tmp $$tmp2

