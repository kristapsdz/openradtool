.SUFFIXES: .dot .svg .1 .1.html .5 .5.html .in.pc .pc
.PHONY: regress

include Makefile.configure

VERSION_MAJOR	 = 0
VERSION_MINOR	 = 10
VERSION_BUILD	 = 3
VERSION		:= $(VERSION_MAJOR).$(VERSION_MINOR).$(VERSION_BUILD)
LIBOBJS		 = compats.o \
		   config.o \
		   linker.o \
		   linker_aliases.o \
		   linker_resolve.o \
		   log.o \
		   parser.o \
		   parser_bitfield.o \
		   parser_enum.o \
		   parser_field.o \
		   parser_roles.o \
		   parser_struct.o \
		   writer.o
OBJS		 = audit.o \
		   cheader.o \
		   csource.o \
		   lang.o \
		   lang-c-header.o \
		   lang-c-source.o \
		   lang-c.o \
		   lang-javascript.o \
		   lang-nodejs.o \
		   main.o \
		   nodejs.o \
		   javascript.o \
		   sql.o \
		   xliff.o
HTMLS		 = archive.html \
		   index.html \
		   man/ort.1.html \
		   man/ort-audit.1.html \
		   man/ort-audit-gv.1.html \
		   man/ort-audit-json.1.html \
		   man/ort-c-header.1.html \
		   man/ort-c-source.1.html \
		   man/ort-javascript.1.html \
		   man/ort-nodejs.1.html \
		   man/ort-sql.1.html \
		   man/ort-sqldiff.1.html \
		   man/ort-xliff.1.html \
		   man/ort.5.html
WWWDIR		 = /var/www/vhosts/kristaps.bsd.lv/htdocs/openradtool
MAN3S		 = man/ort.3 \
		   man/ort_config_alloc.3 \
		   man/ort_config_free.3 \
		   man/ort_parse_close.3 \
		   man/ort_parse_file.3 \
		   man/ort_write_file.3
MAN5S		 = man/ort.5
MAN1S		 = man/ort.1 \
		   man/ort-audit.1 \
		   man/ort-audit-gv.1 \
		   man/ort-audit-json.1 \
		   man/ort-c-header.1 \
		   man/ort-c-source.1 \
		   man/ort-javascript.1 \
		   man/ort-nodejs.1 \
		   man/ort-sql.1 \
		   man/ort-sqldiff.1 \
		   man/ort-xliff.1
GENHEADERS	 = paths.h \
		   version.h
HEADERS 	 = extern.h \
		   lang-c.h \
		   lang.h \
		   linker.h \
		   ort-lang-c.h \
		   ort-lang-javascript.h \
		   ort-lang-nodejs.h \
		   ort-nodejs.h \
		   ort.h \
		   parser.h
DOTAREXEC	 = configure
DOTAR		 = $(HEADERS) \
		   Makefile \
		   audit.c \
		   audit.css \
		   audit.html \
		   audit.js \
		   b64_ntop.c \
		   cheader.c \
		   compats.c \
		   config.c \
		   csource.c \
		   gensalt.c \
		   javascript.c \
		   jsmn.c \
		   lang-c-header.c \
		   lang-c-source.c \
		   lang-c.c \
		   lang-javascript.c \
		   lang-nodejs.c \
		   lang.c \
		   linker.c \
		   linker_aliases.c \
		   linker_resolve.c \
		   log.c \
		   main.c \
		   nodejs.c \
		   ortPrivate.ts \
		   parser.c \
		   parser_bitfield.c \
		   parser_enum.c \
		   parser_field.c \
		   parser_roles.c \
		   parser_struct.c \
		   sql.c \
		   test.c \
		   tests.c \
		   writer.c \
		   xliff.c
XMLS		 = test.xml.xml \
		   versions.xml
IHTMLS		 = audit-example.ort.html \
		   audit-out.js \
		   db.ort.html \
		   db.fr.xml.html \
		   db.h.html \
		   db.c.html \
		   db.node.ts.html \
		   db.sql.html \
		   db.old.ort.html \
		   db.update.sql.html \
		   db.ts.html \
		   db.trans.ort.html \
		   test.c.html
BINS		 = ort \
		   ort-audit \
		   ort-audit-gv \
		   ort-audit-json \
		   ort-c-header \
		   ort-c-source \
		   ort-javascript \
		   ort-nodejs \
		   ort-sql \
		   ort-sqldiff \
		   ort-xliff
IMAGES		 = index.svg \
		   index-fig0.svg \
		   index-fig1.svg \
		   index-fig2.svg \
		   index-fig3.svg \
		   index-fig4.svg \
		   index-fig5.svg \
		   index-fig6.svg \
		   index-fig7.svg \
		   index-fig8.svg
NODE_TYPES	 = $(LIBDIR)/node_modules/@types

# Only needed for test, not built by default.
LIBS_SQLBOX	!= pkg-config --libs sqlbox 2>/dev/null || echo "-lsqlbox -lsqlite3"
CFLAGS_SQLBOX	!= pkg-config --cflags sqlbox 2>/dev/null || echo ""

LIBS_PKG	!= pkg-config --libs expat 2>/dev/null || echo "-lexpat"
CFLAGS_PKG	!= pkg-config --cflags expat 2>/dev/null || echo ""

PKG_REGRESS	 = sqlbox kcgi-regress kcgi-json libcurl
LIBS_REGRESS	!= pkg-config --libs $(PKG_REGRESS) 2>/dev/null || echo ""
CFLAGS_REGRESS	!= pkg-config --cflags $(PKG_REGRESS) 2>/dev/null || echo ""

all: $(BINS) ort.pc

afl::
	$(MAKE) clean
	$(MAKE) all CC=afl-gcc
	cp $(BINS) afl

ort: main.o libort.a
	$(CC) -o $@ main.o libort.a

libort.a: $(LIBOBJS)
	$(AR) rs $@ $(LIBOBJS)

ort-nodejs: nodejs.o lang-nodejs.o lang.o libort.a
	$(CC) -o $@ nodejs.o lang-nodejs.o lang.o libort.a $(LDFLAGS) $(LDADD)

ort-c-source: csource.o lang-c.o lang-c-source.o lang.o libort.a
	$(CC) -o $@ csource.o lang-c.o lang-c-source.o lang.o libort.a $(LDFLAGS) $(LDADD)

ort-c-header: cheader.o lang-c.o lang-c-header.o lang.o libort.a
	$(CC) -o $@ cheader.o lang-c.o lang-c-header.o lang.o libort.a $(LDFLAGS) $(LDADD)

ort-javascript: javascript.o lang-javascript.o lang.o libort.a
	$(CC) -o $@ javascript.o lang-javascript.o lang.o libort.a $(LDFLAGS) $(LDADD)

ort-sql: sql.o lang.o libort.a
	$(CC) -o $@ sql.o lang.o libort.a $(LDFLAGS) $(LDADD)

ort-sqldiff: sql.o lang.o libort.a
	$(CC) -o $@ sql.o lang.o libort.a $(LDFLAGS) $(LDADD)

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
	$(INSTALL_DATA) *.html *.css *.js man/*.html $(IMAGES) atom.xml $(WWWDIR)
	$(INSTALL_DATA) openradtool.tar.gz openradtool.tar.gz.sha512 $(WWWDIR)/snapshots
	$(INSTALL_DATA) openradtool.tar.gz $(WWWDIR)/snapshots/openradtool-$(VERSION).tar.gz
	$(INSTALL_DATA) openradtool.tar.gz.sha512 $(WWWDIR)/snapshots/openradtool-$(VERSION).tar.gz.sha512

# Something to run before doing a release: just makes sure that we have
# the given version documented and that make and make install works
# when run on the distributed tarball.
# Also checks that our manpages are nice.

distcheck: openradtool.tar.gz.sha512 openradtool.tar.gz
	mandoc -Tlint -Werror $(MAN1S) $(MAN5S) $(MAN3S)
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

paths.h: Makefile
	( echo "#define SHAREDIR \"$(SHAREDIR)/openradtool\"" ; \
	  echo "#define FILE_GENSALT \"gensalt.c\"" ; \
	  echo "#define FILE_B64_NTOP \"b64_ntop.c\"" ; \
	  echo "#define FILE_JSMN \"jsmn.c\"" ; ) >$@

install: all
	mkdir -p $(DESTDIR)$(BINDIR)
	mkdir -p $(DESTDIR)$(INCLUDEDIR)
	mkdir -p $(DESTDIR)$(LIBDIR)
	mkdir -p $(DESTDIR)$(LIBDIR)/pkgconfig
	mkdir -p $(DESTDIR)$(MANDIR)/man1
	mkdir -p $(DESTDIR)$(MANDIR)/man3
	mkdir -p $(DESTDIR)$(MANDIR)/man5
	mkdir -p $(DESTDIR)$(SHAREDIR)/openradtool
	$(INSTALL_MAN) $(MAN1S) $(DESTDIR)$(MANDIR)/man1
	$(INSTALL_MAN) $(MAN3S) $(DESTDIR)$(MANDIR)/man3
	$(INSTALL_MAN) $(MAN5S) $(DESTDIR)$(MANDIR)/man5
	$(INSTALL_DATA) audit.html audit.css audit.js $(DESTDIR)$(SHAREDIR)/openradtool
	$(INSTALL_DATA) b64_ntop.c jsmn.c gensalt.c $(DESTDIR)$(SHAREDIR)/openradtool
	$(INSTALL_DATA) ortPrivate.ts $(DESTDIR)$(SHAREDIR)/openradtool
	$(INSTALL_DATA) ort.h $(DESTDIR)$(INCLUDEDIR)
	$(INSTALL_LIB) libort.a $(DESTDIR)$(LIBDIR)
	$(INSTALL_DATA) ort.pc $(DESTDIR)$(LIBDIR)/pkgconfig
	$(INSTALL_PROGRAM) $(BINS) $(DESTDIR)$(BINDIR)

openradtool.tar.gz.sha512: openradtool.tar.gz
	openssl dgst -sha512 -hex openradtool.tar.gz >$@

openradtool.tar.gz: $(DOTAR) $(DOTAREXEC)
	mkdir -p .dist/openradtool-$(VERSION)/
	mkdir -p .dist/openradtool-$(VERSION)/man
	mkdir -p .dist/openradtool-$(VERSION)/regress
	mkdir -p .dist/openradtool-$(VERSION)/regress/c
	mkdir -p .dist/openradtool-$(VERSION)/regress/javascript
	mkdir -p .dist/openradtool-$(VERSION)/regress/sqldiff
	mkdir -p .dist/openradtool-$(VERSION)/regress/sql
	install -m 0444 $(DOTAR) ort.pc .dist/openradtool-$(VERSION)
	install -m 0444 man/*.[0-9] .dist/openradtool-$(VERSION)/man
	install -m 0444 regress/*.ort .dist/openradtool-$(VERSION)/regress
	install -m 0444 regress/*.result .dist/openradtool-$(VERSION)/regress
	install -m 0444 regress/*.nresult .dist/openradtool-$(VERSION)/regress
	install -m 0444 regress/sqldiff/*.ort .dist/openradtool-$(VERSION)/regress/sqldiff
	install -m 0444 regress/sqldiff/*.result .dist/openradtool-$(VERSION)/regress/sqldiff
	install -m 0444 regress/sqldiff/*.nresult .dist/openradtool-$(VERSION)/regress/sqldiff
	install -m 0444 regress/sql/*.ort .dist/openradtool-$(VERSION)/regress/sql
	install -m 0444 regress/sql/*.result .dist/openradtool-$(VERSION)/regress/sql
	install -m 0444 regress/javascript/*.ort .dist/openradtool-$(VERSION)/regress/javascript
	install -m 0444 regress/javascript/*.result .dist/openradtool-$(VERSION)/regress/javascript
	install -m 0444 regress/javascript/*.ts .dist/openradtool-$(VERSION)/regress/javascript
	install -m 0444 regress/javascript/*.xml .dist/openradtool-$(VERSION)/regress/javascript
	install -m 0444 regress/c/*.ort .dist/openradtool-$(VERSION)/regress/c
	install -m 0444 regress/c/*.c .dist/openradtool-$(VERSION)/regress/c
	install -m 0444 regress/c/regress.h .dist/openradtool-$(VERSION)/regress/c
	install -m 0555 $(DOTAREXEC) .dist/openradtool-$(VERSION)
	( cd .dist/ && tar zcf ../$@ ./ )
	rm -rf .dist/

test: test.o db.o db.db
	$(CC) -o $@ test.o db.o $(LIBS_SQLBOX) $(LDADD_CRYPT)

audit-out.js: ort-audit-json audit-example.ort
	./ort-audit-json user audit-example.ort >$@

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

db.ts: ort-javascript db.ort ortPrivate.ts
	./ort-javascript -S . db.ort >$@

db.node.ts: ort-nodejs db.ort
	./ort-nodejs db.ort >$@

db.update.sql: ort-sqldiff db.old.ort db.ort
	./ort-sqldiff db.old.ort db.ort >$@

db.trans.ort: ort-xliff db.ort db.fr.xml
	./ort-xliff -j db.ort db.fr.xml >$@

db.db: db.sql
	rm -f $@
	sqlite3 $@ < db.sql

# These can be optimised but there's not much point.

$(LIBOBJS) $(OBJS): config.h $(GENHEADERS) $(HEADERS)

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
	highlight -s whitengrey -I -l --src-lang=conf db.ort >$@

audit-example.ort.html: audit-example.ort
	highlight -s whitengrey -I -l --src-lang=conf audit-example.ort >$@

db.old.ort.html: db.old.ort
	highlight -s whitengrey -I -l --src-lang=conf db.old.ort >$@

db.h.html: db.h
	highlight -s whitengrey -I -l --src-lang=c db.h >$@

db.sql.html: db.sql
	highlight -s whitengrey -I -l --src-lang=sql db.sql >$@

db.ts.html: db.ts
	highlight -s whitengrey -I -l --src-lang=js db.ts >$@

db.node.ts.html: db.node.ts
	highlight -s whitengrey -I -l --src-lang=js db.node.ts >$@

db.update.sql.html: db.update.sql
	highlight -s whitengrey -I -l --src-lang=sql db.update.sql >$@

db.trans.ort.html: db.trans.ort
	highlight -s whitengrey -I -l --src-lang=conf db.trans.ort | sed 's!ISO-8859-1!UTF-8!g' >$@

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
	rm -f $(BINS) $(GENHEADERS) $(LIBOBJS) $(OBJS) libort.a test test.o
	rm -f db.c db.h db.o db.sql db.ts db.node.ts db.update.sql db.db db.trans.ort
	rm -f openradtool.tar.gz openradtool.tar.gz.sha512
	rm -f $(IMAGES) highlight.css $(HTMLS) atom.xml ort.pc
	rm -f db.ort.xml db.h.xml db.sql.xml db.update.sql.xml test.xml.xml $(IHTMLS) TODO.xml

# Remove both what can be built and what's built by ./configure.

distclean: clean
	rm -f config.h config.log Makefile.configure

regress: all
	@tmp=`mktemp` ; \
	if [ "$(LIBS_REGRESS)" == "" ]; then \
		echo "regress/c: ignoring (no sqlbox library)" 1>&2 ; \
	else \
		for f in regress/c/*.ort ; do \
			rr=regress/c/regress.c ; \
			bf=regress/c/`basename $$f .ort` ; \
			cf=regress/c/`basename $$f .ort`.c ; \
			hf=`basename $$f`.h ; \
			./ort-c-header -Jj $$f > $$f.h ; \
			./ort-c-source -S. -h $$hf -Jj $$f > $$f.c ; \
			./ort-sql $$f | sqlite3 $$tmp ; \
			set +e ; \
			$(CC) $(CFLAGS_REGRESS) $(CFLAGS) -o $$bf \
				$$f.c $$cf $$rr $(LIBS_REGRESS) \
				$(LDADD_CRYPT) 2>/dev/null ; \
			if [ $$? -ne 0 ] ; then \
				set -e ; \
				echo "$$f: fail (did not compile)" ; \
				rm -f $$f.h $$f.c $$bf $$tmp ; \
				exit 1 ; \
			fi ; \
			rm $$f.h $$f.c ; \
			./$$bf 2>/dev/null 1>/dev/null $$tmp ; \
			if [ $$? -ne 0 ] ; then \
				set -e ; \
				echo "$$f: fail" ; \
				rm $$bf $$tmp ; \
				exit 1 ; \
			fi ; \
			rm $$bf $$tmp ; \
			set -e ; \
			echo "$$f: pass" ; \
		done ; \
	fi ; \
	for f in regress/*.result ; do \
		bf=`basename $$f .result`.ort ; \
		set +e ; \
		printf "$$bf... " ; \
		./ort regress/$$bf >$$tmp 2>/dev/null ; \
		if [ $$? -ne 0 ] ; then \
			echo "fail (did not execute)" ; \
			rm $$tmp ; \
			exit 1 ; \
		fi ; \
		diff -w $$tmp $$f >/dev/null 2>&1 ; \
		if [ $$? -ne 0 ] ; then \
			echo "fail (output)" ; \
			diff -wu $$tmp $$f ; \
			rm $$tmp ; \
			exit 1 ; \
		fi ; \
		echo "pass" ; \
		set -e ; \
	done ; \
	for f in regress/*.nresult ; do \
		set +e ; \
		printf "`basename $$f`... " ; \
		./ort $$f >/dev/null 2>&1 ; \
		if [ $$? -eq 0 ] ; then \
			echo "fail (did not error out)" ; \
			exit 1 ; \
		fi ; \
		echo "pass" ; \
		set -e ; \
	done ; \
	for f in regress/sqldiff/*.result ; do \
		new=regress/sqldiff/`basename $$f .result`.new.ort ; \
		old=regress/sqldiff/`basename $$f .result`.old.ort ; \
		set +e ; \
		printf "$$old -> $$new... " ; \
		./ort-sqldiff $$old $$new >$$tmp 2>/dev/null ; \
		if [ $$? -ne 0 ] ; then \
			echo "fail (did not execute)" ; \
			rm $$tmp ; \
			exit 1 ; \
		fi ; \
		diff -w $$tmp $$f >/dev/null 2>&1 ; \
		if [ $$? -ne 0 ] ; then \
			echo "fail (output)" ; \
			diff -wu $$tmp $$f ; \
			rm $$tmp ; \
			exit 1 ; \
		fi ; \
		echo "pass" ; \
		set -e ; \
	done ; \
	for old in regress/sqldiff/*.old.nresult ; do \
		new=regress/sqldiff/`basename $$old .old.nresult`.new.nresult ; \
		set +e ; \
		printf "$$old -> $$new... " ; \
		./ort-sqldiff $$old $$new >/dev/null 2>&1 ; \
		if [ $$? -eq 0 ] ; then \
			echo "fail (did not error out)" ; \
			exit 1 ; \
		fi ; \
		echo "pass" ; \
		set -e ; \
	done ; \
	for f in regress/sql/*.result ; do \
		bf=regress/sql/`basename $$f .result`.ort ; \
		set +e ; \
		printf "$$bf... " ; \
		./ort-sql $$bf >$$tmp 2>/dev/null ; \
		if [ $$? -ne 0 ] ; then \
			echo "fail (did not execute)" ; \
			rm $$tmp ; \
			exit 1 ; \
		fi ; \
		diff -w $$tmp $$f >/dev/null 2>&1 ; \
		if [ $$? -ne 0 ] ; then \
			echo "fail (output)" ; \
			diff -wu $$tmp $$f ; \
			rm $$tmp ; \
			exit 1 ; \
		fi ; \
		echo "pass" ; \
		set -e ; \
	done ; \
	rm $$tmp ; \
	for f in regress/javascript/*.ort ; do \
		set +e ; \
		printf "$$f... " ; \
		./ort-javascript -S. $$f >/dev/null 2>/dev/null ; \
		if [ $$? -ne 0 ] ; then \
			echo "fail (did not execute)" ; \
			./ort-javascript -S. $$f >/dev/null ; \
			exit 1 ; \
		fi ; \
		echo "pass" ; \
		set -e ; \
	done ; \
	set +e ; \
	command -v ts-node >/dev/null 2>/dev/null; \
	if [ $$? -ne 0 ]; then \
		set -e ; \
		echo "regress/javascript: ignoring (no ts-node)" 1>&2 ; \
	else \
		set -e ; \
		ts-node --skip-project regress/javascript/regress-runner.ts ; \
	fi

.in.pc.pc:
	sed -e "s!@PREFIX@!$(PREFIX)!g" \
	    -e "s!@LIBDIR@!$(LIBDIR)!g" \
	    -e "s!@INCLUDEDIR@!$(INCLUDEDIR)!g" \
	    -e "s!@VERSION@!$(VERSION)!g" $< >$@
