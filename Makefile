.SUFFIXES: .dot .svg .1 .1.html .5 .5.html .in.pc .pc .3 .3.html
.PHONY: regress

include Makefile.configure

VERSION_MAJOR	 = 0
VERSION_MINOR	 = 11
VERSION_BUILD	 = 17
VERSION		:= $(VERSION_MAJOR).$(VERSION_MINOR).$(VERSION_BUILD)
LIBOBJS		 = compats.o \
		   config.o \
		   diff.o \
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
		   writer.o \
		   writer-diff.o
LIBS		 = libort.a \
		   libort-lang-c.a \
		   libort-lang-nodejs.a \
		   libort-lang-javascript.a \
		   libort-lang-json.a \
		   libort-lang-sql.a \
		   libort-lang-xliff.a
PKGCONFIGS	 = ort.pc \
		   ort-lang-nodejs.pc \
		   ort-lang-javascript.pc \
		   ort-lang-json.pc \
		   ort-lang-sql.pc
OBJS		 = audit.o \
		   cheader.o \
		   csource.o \
		   lang.o \
		   lang-c-header.o \
		   lang-c-source.o \
		   lang-c.o \
		   lang-javascript.o \
		   lang-json.o \
		   lang-nodejs.o \
		   lang-sql.o \
		   lang-xliff.o \
		   main.o \
		   maindiff.o \
		   nodejs.o \
		   javascript.o \
		   json.o \
		   sql.o \
		   sqldiff.o \
		   xliff.o
HTMLS		 = archive.html \
		   index.html \
		   man/ort.1.html \
		   man/ort-audit.1.html \
		   man/ort-audit-gv.1.html \
		   man/ort-audit-json.1.html \
		   man/ort-c-header.1.html \
		   man/ort-c-source.1.html \
		   man/ort-diff.1.html \
		   man/ort-javascript.1.html \
		   man/ort-json.1.html \
		   man/ort-nodejs.1.html \
		   man/ort-sql.1.html \
		   man/ort-sqldiff.1.html \
		   man/ort-xliff.1.html \
		   man/ort.3.html \
		   man/ort_config_alloc.3.html \
		   man/ort_config_free.3.html \
		   man/ort_diff.3.html \
		   man/ort_diff_free.3.html \
		   man/ort_lang_c_header.3.html \
		   man/ort_lang_c_source.3.html \
		   man/ort_lang_javascript.3.html \
		   man/ort_lang_json.3.html \
		   man/ort_lang_nodejs.3.html \
		   man/ort_lang_sql.3.html \
		   man/ort_lang_xliff_extract.3.html \
		   man/ort_lang_xliff_join.3.html \
		   man/ort_lang_xliff_update.3.html \
		   man/ort_msg.3.html \
		   man/ort_msgq_free.3.html \
		   man/ort_parse_close.3.html \
		   man/ort_parse_file.3.html \
		   man/ort_write_diff_file.3.html \
		   man/ort_write_file.3.html \
		   man/ort_write_msg_file.3.html \
		   man/ort.5.html
WWWDIR		 = /var/www/vhosts/kristaps.bsd.lv/htdocs/openradtool
MAN3S		 = man/ort.3 \
		   man/ort_config_alloc.3 \
		   man/ort_config_free.3 \
		   man/ort_diff.3  \
		   man/ort_diff_free.3  \
		   man/ort_lang_c_header.3 \
		   man/ort_lang_c_source.3 \
		   man/ort_lang_javascript.3 \
		   man/ort_lang_json.3 \
		   man/ort_lang_nodejs.3 \
		   man/ort_lang_sql.3 \
		   man/ort_lang_xliff_extract.3 \
		   man/ort_lang_xliff_join.3 \
		   man/ort_lang_xliff_update.3 \
		   man/ort_msg.3 \
		   man/ort_msgq_free.3 \
		   man/ort_parse_close.3 \
		   man/ort_parse_file.3 \
		   man/ort_write_file.3 \
		   man/ort_write_diff_file.3 \
		   man/ort_write_msg_file.3
MAN5S		 = man/ort.5
MAN1S		 = man/ort.1 \
		   man/ort-audit.1 \
		   man/ort-audit-gv.1 \
		   man/ort-audit-json.1 \
		   man/ort-c-header.1 \
		   man/ort-c-source.1 \
		   man/ort-diff.1 \
		   man/ort-javascript.1 \
		   man/ort-json.1 \
		   man/ort-nodejs.1 \
		   man/ort-sql.1 \
		   man/ort-sqldiff.1 \
		   man/ort-xliff.1
GENHEADERS	 = paths.h \
		   version.h
PUBHEADERS	 = ort.h \
		   ort-lang-javascript.h \
		   ort-lang-json.h \
		   ort-lang-nodejs.h \
		   ort-lang-sql.h
HEADERS 	 = $(PUBHEADERS) \
		   extern.h \
		   lang-c.h \
		   lang.h \
		   linker.h \
		   ort-lang-c.h \
		   ort-lang-xliff.h \
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
		   diff.c \
		   gensalt.c \
		   javascript.c \
		   json.c \
		   jsmn.c \
		   lang-c-header.c \
		   lang-c-source.c \
		   lang-c.c \
		   lang-javascript.c \
		   lang-json.c \
		   lang-nodejs.c \
		   lang-sql.c \
		   lang-xliff.c \
		   lang.c \
		   linker.c \
		   linker_aliases.c \
		   linker_resolve.c \
		   log.c \
		   main.c \
		   maindiff.c \
		   nodejs.c \
		   ortPrivate.ts \
		   ort.in.pc \
		   ort-lang-nodejs.in.pc \
		   ort-lang-javascript.in.pc \
		   ort-lang-json.in.pc \
		   ort-lang-sql.in.pc \
		   ort-json.ts \
		   parser.c \
		   parser_bitfield.c \
		   parser_enum.c \
		   parser_field.c \
		   parser_roles.c \
		   parser_struct.c \
		   sql.c \
		   sqldiff.c \
		   test.c \
		   tests.c \
		   writer.c \
		   writer-diff.c \
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
		   ort-diff \
		   ort-javascript \
		   ort-json \
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

all: $(BINS) $(LIBS) $(PKGCONFIGS)

afl::
	$(MAKE) clean
	$(MAKE) all CC=afl-gcc
	cp $(BINS) afl

ort: main.o libort.a
	$(CC) -o $@ main.o libort.a

ort-diff: maindiff.o libort.a
	$(CC) -o $@ maindiff.o libort.a

libort.a: $(LIBOBJS)
	$(AR) rs $@ $(LIBOBJS)

libort-lang-c.a: lang-c.o lang-c-source.o lang-c-header.o lang.o
	$(AR) rs $@ lang-c.o lang-c-source.o lang-c-header.o lang.o

libort-lang-javascript.a: lang-javascript.o lang.o
	$(AR) rs $@ lang-javascript.o lang.o

libort-lang-json.a: lang-json.o
	$(AR) rs $@ lang-json.o

libort-lang-nodejs.a: lang-nodejs.o lang.o
	$(AR) rs $@ lang-nodejs.o lang.o

libort-lang-sql.a: lang-sql.o lang.o
	$(AR) rs $@ lang-sql.o lang.o

libort-lang-xliff.a: lang-xliff.o
	$(AR) rs $@ lang-xliff.o

ort-nodejs: nodejs.o libort-lang-nodejs.a libort.a
	$(CC) -o $@ nodejs.o libort-lang-nodejs.a libort.a $(LDFLAGS) $(LDADD)

ort-c-source: csource.o libort-lang-c.a libort.a
	$(CC) -o $@ csource.o libort-lang-c.a libort.a $(LDFLAGS) $(LDADD)

ort-c-header: cheader.o libort-lang-c.a libort.a
	$(CC) -o $@ cheader.o libort-lang-c.a libort.a $(LDFLAGS) $(LDADD)

ort-javascript: javascript.o libort-lang-javascript.a libort.a
	$(CC) -o $@ javascript.o libort-lang-javascript.a libort.a $(LDFLAGS) $(LDADD)

ort-json: json.o libort-lang-json.a libort.a
	$(CC) -o $@ json.o libort-lang-json.a libort.a $(LDFLAGS) $(LDADD)

ort-sql: sql.o libort-lang-sql.a libort.a
	$(CC) -o $@ sql.o libort-lang-sql.a libort.a $(LDFLAGS) $(LDADD)

ort-sqldiff: sqldiff.o libort-lang-sql.a libort.a
	$(CC) -o $@ sqldiff.o libort-lang-sql.a libort.a $(LDFLAGS) $(LDADD)

ort-audit: audit.o libort.a
	$(CC) -o $@ audit.o libort.a $(LDFLAGS) $(LDADD)

ort-audit-gv: audit.o libort.a
	$(CC) -o $@ audit.o libort.a $(LDFLAGS) $(LDADD)

ort-audit-json: audit.o libort.a
	$(CC) -o $@ audit.o libort.a $(LDFLAGS) $(LDADD)

ort-xliff: xliff.o libort-lang-xliff.a libort.a
	$(CC) -o $@ xliff.o libort-lang-xliff.a libort.a $(LDFLAGS) $(LIBS_PKG) $(LDADD)

lang-xliff.o: lang-xliff.c
	$(CC) $(CFLAGS) $(CPPFLAGS) $(CFLAGS_PKG) -c lang-xliff.c

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
	echo "#define SHAREDIR \"$(SHAREDIR)/openradtool\"" >$@

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
	$(INSTALL_DATA) ortPrivate.ts ort-json.ts $(DESTDIR)$(SHAREDIR)/openradtool
	$(INSTALL_DATA) $(PUBHEADERS) $(DESTDIR)$(INCLUDEDIR)
	$(INSTALL_LIB) $(LIBS) $(DESTDIR)$(LIBDIR)
	$(INSTALL_DATA) $(PKGCONFIGS) $(DESTDIR)$(LIBDIR)/pkgconfig
	$(INSTALL_PROGRAM) $(BINS) $(DESTDIR)$(BINDIR)

openradtool.tar.gz.sha512: openradtool.tar.gz
	openssl dgst -sha512 -hex openradtool.tar.gz >$@

openradtool.tar.gz: $(DOTAR) $(DOTAREXEC)
	mkdir -p .dist/openradtool-$(VERSION)/
	mkdir -p .dist/openradtool-$(VERSION)/man
	mkdir -p .dist/openradtool-$(VERSION)/regress
	mkdir -p .dist/openradtool-$(VERSION)/regress/c
	mkdir -p .dist/openradtool-$(VERSION)/regress/diff
	mkdir -p .dist/openradtool-$(VERSION)/regress/javascript
	mkdir -p .dist/openradtool-$(VERSION)/regress/json
	mkdir -p .dist/openradtool-$(VERSION)/regress/nodejs
	mkdir -p .dist/openradtool-$(VERSION)/regress/sqldiff
	mkdir -p .dist/openradtool-$(VERSION)/regress/sql
	mkdir -p .dist/openradtool-$(VERSION)/regress/xliff
	install -m 0444 $(DOTAR) .dist/openradtool-$(VERSION)
	install -m 0444 man/*.[0-9] .dist/openradtool-$(VERSION)/man
	install -m 0444 regress/*.ort .dist/openradtool-$(VERSION)/regress
	install -m 0444 regress/*.result .dist/openradtool-$(VERSION)/regress
	install -m 0444 regress/*.nresult .dist/openradtool-$(VERSION)/regress
	install -m 0444 regress/*.md .dist/openradtool-$(VERSION)/regress
	install -m 0444 regress/c/*.ort .dist/openradtool-$(VERSION)/regress/c
	install -m 0444 regress/c/*.c .dist/openradtool-$(VERSION)/regress/c
	install -m 0444 regress/c/*.md .dist/openradtool-$(VERSION)/regress/c
	install -m 0444 regress/c/regress.h .dist/openradtool-$(VERSION)/regress/c
	install -m 0444 regress/diff/*.ort .dist/openradtool-$(VERSION)/regress/diff
	install -m 0444 regress/diff/*.result .dist/openradtool-$(VERSION)/regress/diff
	install -m 0444 regress/diff/*.md .dist/openradtool-$(VERSION)/regress/diff
	install -m 0444 regress/javascript/*.ort .dist/openradtool-$(VERSION)/regress/javascript
	install -m 0444 regress/javascript/*.result .dist/openradtool-$(VERSION)/regress/javascript
	install -m 0444 regress/javascript/*.ts .dist/openradtool-$(VERSION)/regress/javascript
	install -m 0444 regress/javascript/*.xml .dist/openradtool-$(VERSION)/regress/javascript
	install -m 0444 regress/json/*.md .dist/openradtool-$(VERSION)/regress/json
	install -m 0444 regress/json/*.ts .dist/openradtool-$(VERSION)/regress/json
	install -m 0444 regress/nodejs/*.md .dist/openradtool-$(VERSION)/regress/nodejs
	install -m 0444 regress/nodejs/*.ts .dist/openradtool-$(VERSION)/regress/nodejs
	install -m 0444 regress/sql/*.ort .dist/openradtool-$(VERSION)/regress/sql
	install -m 0444 regress/sql/*.result .dist/openradtool-$(VERSION)/regress/sql
	install -m 0444 regress/sqldiff/*.ort .dist/openradtool-$(VERSION)/regress/sqldiff
	install -m 0444 regress/sqldiff/*.result .dist/openradtool-$(VERSION)/regress/sqldiff
	install -m 0444 regress/sqldiff/*.nresult .dist/openradtool-$(VERSION)/regress/sqldiff
	install -m 0444 regress/xliff/*.xsd .dist/openradtool-$(VERSION)/regress/xliff
	install -m 0444 regress/xliff/*.ort .dist/openradtool-$(VERSION)/regress/xliff
	install -m 0444 regress/xliff/*.result .dist/openradtool-$(VERSION)/regress/xliff
	install -m 0444 regress/xliff/*.xlf .dist/openradtool-$(VERSION)/regress/xliff
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

.5.5.html .3.3.html .1.1.html:
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
	rm -f $(BINS) $(GENHEADERS) $(LIBOBJS) $(OBJS) $(LIBS) test test.o
	rm -f db.c db.h db.o db.sql db.ts db.node.ts db.update.sql db.db db.trans.ort
	rm -f openradtool.tar.gz openradtool.tar.gz.sha512
	rm -f $(IMAGES) highlight.css $(HTMLS) atom.xml $(PKGCONFIGS)
	rm -f db.ort.xml db.h.xml db.sql.xml db.update.sql.xml test.xml.xml $(IHTMLS) TODO.xml
	rm -f ort-json.schema

# Remove both what can be built and what's built by ./configure.

distclean: clean
	rm -f config.h config.log Makefile.configure

regress: all
	@tmp=`mktemp` ; \
	MALLOC_OPTIONS=S ; \
	set +e ; \
	echo "=== ort output tests === " ; \
	for f in regress/*.result ; do \
		bf=`basename $$f .result`.ort ; \
		printf "ort: regress/$$bf... " ; \
		./ort regress/$$bf >$$tmp 2>/dev/null ; \
		if [ $$? -ne 0 ] ; then \
			echo "fail (did not execute)" ; \
			rm -f $$tmp ; \
			exit 1 ; \
		fi ; \
		diff -w $$tmp $$f >/dev/null 2>&1 ; \
		if [ $$? -ne 0 ] ; then \
			echo "fail (output)" ; \
			diff -wu $$tmp $$f ; \
			rm -f $$tmp ; \
			exit 1 ; \
		fi ; \
		echo "pass" ; \
	done ; \
	echo "=== ort bad syntax tests === " ; \
	for f in regress/*.nresult ; do \
		printf "ort: `basename $$f`... " ; \
		./ort $$f >/dev/null 2>&1 ; \
		if [ $$? -eq 0 ] ; then \
			echo "fail (did not error out)" ; \
			rm -f $$tmp ; \
			exit 1 ; \
		fi ; \
		echo "pass" ; \
	done ; \
	which xmllint >/dev/null 2>&1 ; \
	if [ $$? -eq 0 ]; then \
		echo "=== ort-xliff syntax tests === " ; \
		sc=regress/xliff/xliff-core-1.2-strict.xsd ; \
		for f in regress/*.ort ; do \
			printf "xmllint: $$f... " ; \
			./ort-xliff $$f >$$tmp 2>/dev/null ; \
			if [ $$? -ne 0 ] ; then \
				echo "fail (did not execute)" ; \
				rm -f $$tmp ; \
				exit 1 ; \
			fi ; \
			xmllint --schema $$sc --nonet --noout \
				--path "regress/xliff" $$tmp 2>/dev/null ; \
			if [ $$? -ne 0 ] ; then \
				echo "fail" ; \
				xmllint --schema $$sc --nonet \
					--path "regress/xliff" $$tmp ; \
				rm -f $$tmp ; \
				exit 1 ; \
			fi ; \
			echo "pass" ; \
		done ; \
	else \
		echo "!!! skipping ort-xliff syntax tests !!! " ; \
	fi ; \
	echo "=== ort-sql syntax tests === " ; \
	for f in regress/*.ort ; do \
		printf "sqlite3: $$f... " ; \
		./ort-sql $$f >$$tmp 2>/dev/null ; \
		if [ $$? -ne 0 ] ; then \
			echo "fail (did not execute)" ; \
			rm -f $$tmp ; \
			exit 1 ; \
		fi ; \
		sqlite3 ":memory:" < $$tmp 2>/dev/null ; \
		if [ $$? -ne 0 ] ; then \
			echo "fail" ; \
			sqlite3 ":memory:" < $$tmp ; \
			rm -f $$tmp ; \
			exit 1 ; \
		fi ; \
		echo "pass" ; \
	done ; \
	echo "=== ort-sql output tests === " ; \
	for f in regress/sql/*.result ; do \
		bf=regress/sql/`basename $$f .result`.ort ; \
		printf "ort-sql: $$bf... " ; \
		./ort-sql $$bf >$$tmp 2>/dev/null ; \
		if [ $$? -ne 0 ] ; then \
			echo "fail (did not execute)" ; \
			rm -f $$tmp ; \
			exit 1 ; \
		fi ; \
		diff -w $$tmp $$f >/dev/null 2>&1 ; \
		if [ $$? -ne 0 ] ; then \
			echo "fail (output)" ; \
			diff -wu $$tmp $$f ; \
			rm -f $$tmp ; \
			exit 1 ; \
		fi ; \
		echo "pass" ; \
	done ; \
	echo "=== ort-diff output tests === " ; \
	for f in regress/diff/*.result ; do \
		fn=`basename $$f .result`.{old,new}.ort ; \
		new=regress/diff/`basename $$f .result`.new.ort ; \
		old=regress/diff/`basename $$f .result`.old.ort ; \
		printf "ort-diff: regress/diff/$$fn... " ; \
		./ort-diff $$old $$new >$$tmp 2>/dev/null ; \
		if [ $$? -gt 1 ] ; then \
			echo "fail (did not execute)" ; \
			rm -f $$tmp ; \
			exit 1 ; \
		fi ; \
		diff -w $$tmp $$f >/dev/null 2>&1 ; \
		if [ $$? -ne 0 ] ; then \
			echo "fail (output check)" ; \
			diff -wu $$tmp $$f ; \
			rm -f $$tmp ; \
			exit 1 ; \
		fi ; \
		echo "pass" ; \
	done ; \
	echo "=== ort-diff identity tests === " ; \
	for f in regress/diff/*.ort ; do \
		printf "ort-diff: $$f... " ; \
		./ort-diff $$f $$f >$$tmp 2>/dev/null ; \
		rc=$$? ; \
		if [ $$rc -gt 0 ] ; then \
			echo "fail (did not execute)" ; \
			rm -f $$tmp ; \
			exit 1 ; \
		fi ; \
		val=`cat $$tmp | wc -l | sed 's! !!g'` ; \
		if [ $$val -ne 2 -o $$rc -ne 0 ] ; then \
			echo "fail (output check)" ; \
			cat $$tmp ; \
			rm -f $$tmp ; \
			exit 1 ; \
		fi ; \
		echo "pass" ; \
	done ; \
	echo "=== ort-sqldiff output tests === " ; \
	for f in regress/sqldiff/*.result ; do \
		fn=`basename $$f .result`.{old,new}.ort ; \
		new=regress/sqldiff/`basename $$f .result`.new.ort ; \
		old=regress/sqldiff/`basename $$f .result`.old.ort ; \
		printf "ort-sqldiff: regress/sqldiff/$$fn... " ; \
		./ort-sqldiff $$old $$new >$$tmp 2>/dev/null ; \
		if [ $$? -ne 0 ] ; then \
			echo "fail (did not execute)" ; \
			rm -f $$tmp ; \
			exit 1 ; \
		fi ; \
		diff -w $$tmp $$f >/dev/null 2>&1 ; \
		if [ $$? -ne 0 ] ; then \
			echo "fail (output check)" ; \
			diff -wu $$tmp $$f ; \
			rm -f $$tmp ; \
			exit 1 ; \
		fi ; \
		echo "pass" ; \
	done ; \
	echo "=== ort-sqldiff failing tests === " ; \
	for old in regress/sqldiff/*.old.nresult ; do \
		fn=`basename $$old .old.nresult`.{old,new}.nresult ; \
		new=regress/sqldiff/`basename $$old .old.nresult`.new.nresult ; \
		printf "ort-sqldiff: regress/sqldiff/$$fn... " ; \
		./ort-sqldiff $$old $$new >/dev/null 2>&1 ; \
		if [ $$? -eq 0 ] ; then \
			echo "fail (did not error out)" ; \
			rm -f $$tmp ; \
			exit 1 ; \
		fi ; \
		echo "pass" ; \
	done ; \
	echo "=== ort-xliff join output tests === " ; \
	for f in regress/xliff/*.join.result ; do \
		fn=regress/xliff/`basename $$f .join.result`.ort ; \
		jn=regress/xliff/`basename $$f .join.result`.xlf ; \
		printf "ort-xliff: $$fn... " ; \
		./ort-xliff -j $$fn $$jn >$$tmp 2>/dev/null ; \
		if [ $$? -ne 0 ] ; then \
			echo "fail (did not execute)" ; \
			rm -f $$tmp ; \
			exit 1 ; \
		fi ; \
		./ort-diff $$tmp $$f 2>/dev/null 1>&2 ; \
		if [ $$? -ne 0 ] ; then \
			echo "fail (output check)" ; \
			./ort-diff $$tmp $$f ; \
			rm -f $$tmp ; \
			exit 1 ; \
		fi ; \
		echo "pass" ; \
	done ; \
	echo "=== ort-xliff update output tests === " ; \
	for f in regress/xliff/*.update.result ; do \
		fn=regress/xliff/`basename $$f .update.result`.ort ; \
		jn=regress/xliff/`basename $$f .update.result`.xlf ; \
		printf "ort-xliff: $$fn... " ; \
		./ort-xliff -u $$fn $$jn >$$tmp 2>/dev/null ; \
		if [ $$? -ne 0 ] ; then \
			echo "fail (did not execute)" ; \
			rm -f $$tmp ; \
			exit 1 ; \
		fi ; \
		diff -w $$tmp $$f 2>/dev/null 1>&2 ; \
		if [ $$? -ne 0 ] ; then \
			echo "fail (output check)" ; \
			diff -uw $$tmp $$f ; \
			rm -f $$tmp ; \
			exit 1 ; \
		fi ; \
		echo "pass" ; \
	done ; \
	if [ "$(LIBS_REGRESS)" == "" ]; then \
		echo "!!! skipping ort-c-{header,source} compile tests !!! " ; \
		echo "!!! skipping ort-c-{header,source,sql} run tests !!! " ; \
	else \
		echo "=== ort-c-{header,source} compile tests === " ; \
		for f in regress/*.ort ; do \
			hf=`basename $$f`.h ; \
			set -e ; \
			./ort-c-header -vJj $$f > $$f.h 2>/dev/null ; \
			./ort-c-source -S. -h $$hf -vJj $$f > $$f.c 2>/dev/null ; \
			set +e ; \
			printf "$(CC): $$f... " ; \
			$(CC) $(CFLAGS) $(CFLAGS_SQLBOX) -o /dev/null -c $$f.c 2>/dev/null ; \
			if [ $$? -ne 0 ] ; then \
				echo "fail (compile check)" ; \
				$(CC) $(CFLAGS) $(CFLAGS_SQLBOX) -o /dev/null -c $$f.c ; \
				rm -f $$f.h $$f.c $$tmp ; \
				exit 1 ; \
			fi ; \
			rm -f $$f.h $$f.c ; \
			echo "pass" ; \
		done ; \
		echo "=== ort-c-{header,source,sql} run tests === " ; \
		for f in regress/c/*.ort ; do \
			rr=regress/c/regress.c ; \
			bf=regress/c/`basename $$f .ort` ; \
			cf=regress/c/`basename $$f .ort`.c ; \
			hf=`basename $$f`.h ; \
			rm -f $$tmp ; \
			set -e ; \
			./ort-c-header -vJj $$f > $$f.h 2>/dev/null ; \
			./ort-c-source -S. -h $$hf -vJj $$f > $$f.c 2>/dev/null ; \
			./ort-sql $$f | sqlite3 $$tmp 2>/dev/null ; \
			set +e ; \
			printf "$(CC): $$f... " ; \
			$(CC) $(CFLAGS_REGRESS) $(CFLAGS) -o $$bf \
				$$f.c $$cf $$rr $(LIBS_REGRESS) \
				$(LDADD_CRYPT) 2>/dev/null ; \
			if [ $$? -ne 0 ] ; then \
				echo "fail (did not compile)" ; \
				$(CC) $(CFLAGS_REGRESS) $(CFLAGS) -o $$bf \
					$$f.c $$cf $$rr $(LIBS_REGRESS) \
					$(LDADD_CRYPT) ; \
				rm -f $$f.h $$f.c $$bf $$tmp ; \
				exit 1 ; \
			fi ; \
			rm -f $$f.h $$f.c ; \
			./$$bf 2>/dev/null 1>/dev/null $$tmp ; \
			if [ $$? -ne 0 ] ; then \
				echo "fail" ; \
				rm -f $$bf $$tmp ; \
				exit 1 ; \
			fi ; \
			rm -f $$bf ; \
			echo "pass" ; \
		done ; \
	fi ; \
	if [ -f "node_modules/.bin/ts-node" ]; then \
		echo "=== ort-json reformat tests === " ; \
		cat ort-json.ts regress/json/regress-runner.ts > $$tmp.ts ; \
		set -e ; \
		node_modules/.bin/ts-node --skip-project $$tmp.ts ; \
		set +e ; \
		rm -f $$tmp.ts ; \
	else \
		echo "!!! skipping ort-json reformat run tests !!! " ; \
	fi ; \
	if [ -f "node_modules/.bin/ts-node" ]; then \
		echo "=== ort-nodejs compile tests === " ; \
		set -e ; \
		node_modules/.bin/ts-node --skip-project \
			regress/nodejs/regress-runner.ts ; \
		set +e ; \
	else \
		echo "!!! skipping ort-nodejs compile tests !!! " ; \
	fi ; \
	echo "=== ort-javascript output tests === " ; \
	for f in regress/javascript/*.ort ; do \
		printf "ort-javascript: $$f... " ; \
		./ort-javascript -S. $$f >/dev/null 2>/dev/null ; \
		if [ $$? -ne 0 ] ; then \
			echo "fail (did not execute)" ; \
			./ort-javascript -S. $$f >/dev/null ; \
			rm -f $$tmp ; \
			exit 1 ; \
		fi ; \
		echo "pass" ; \
	done ; \
	if [ -f "node_modules/.bin/ts-node" ]; then \
		echo "=== ort-javascript run tests === " ; \
		set -e ; \
		node_modules/.bin/ts-node --skip-project \
			regress/javascript/regress-runner.ts ; \
		set +e ; \
	else \
		echo "!!! skipping ort-javascript run tests !!! " ; \
	fi ; \
	if [ -f "node_modules/.bin/typescript-json-schema" ]; then \
		node_modules/.bin/typescript-json-schema \
			--strictNullChecks ort-json.ts \
			ortJson.ortJsonConfig > ort-json.schema ; \
		which jsonschema-3 >/dev/null 2>&1 ; \
		if [ $$? -eq 0 ]; then \
			echo "=== ort-json output tests === " ; \
			for f in regress/*.ort ; do \
				printf "ort-json: $$f... " ; \
				./ort-json $$f > $$tmp 2>/dev/null ; \
				if [ $$? -ne 0 ]; then \
					echo "fail (did not execute)" ; \
					rm -f $$tmp ; \
					exit 1 ; \
				fi ; \
				jsonschema-3 -i $$tmp \
					ort-json.schema >/dev/null 2>&1; \
				if [ $$? -ne 0 ]; then \
					echo "fail" ; \
					jsonschema-3 -i $$tmp ort-json.schema ; \
					rm -f $$tmp ; \
					exit 1 ; \
				fi ; \
				echo "pass" ; \
			done ; \
		else \
			echo "!!! skipping ort-json output tests !!! " ; \
		fi ; \
	else \
		echo "!!! skipping ort-json output tests !!! " ; \
	fi ; \
	rm -f $$tmp

.in.pc.pc:
	sed -e "s!@PREFIX@!$(PREFIX)!g" \
	    -e "s!@LIBDIR@!$(LIBDIR)!g" \
	    -e "s!@INCLUDEDIR@!$(INCLUDEDIR)!g" \
	    -e "s!@VERSION@!$(VERSION)!g" $< >$@
