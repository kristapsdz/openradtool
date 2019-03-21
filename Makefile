.SUFFIXES: .dot .svg .1 .1.html .5 .5.html

include Makefile.configure

VERSION_MAJOR	 = 0
VERSION_MINOR	 = 6
VERSION_BUILD	 = 14
VERSION		:= $(VERSION_MAJOR).$(VERSION_MINOR).$(VERSION_BUILD)
LIBOBJS		 = comments.o \
		   compats.o \
		   config.o \
		   linker.o \
		   parser.o \
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
WWWDIR		 = /var/www/vhosts/kristaps.bsd.lv/htdocs/kwebapp
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
		   db.txt.html \
		   db.fr.xml.html \
		   db.h.html \
		   db.c.html \
		   db.sql.html \
		   db.old.txt.html \
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
		   index-fig1.svg \
		   index-fig2.svg \
		   index-fig3.svg \
		   index-fig5.svg \
		   index-fig6.svg

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
	$(CC) -o $@ source.o libort.a

ort-c-header: header.o libort.a
	$(CC) -o $@ header.o libort.a

ort-javascript: javascript.o libort.a
	$(CC) -o $@ javascript.o libort.a

ort-sql: sql.o libort.a
	$(CC) -o $@ sql.o libort.a

ort-sqldiff: sql.o libort.a
	$(CC) -o $@ sql.o libort.a

ort-audit: audit.o libort.a
	$(CC) -o $@ audit.o libort.a

ort-audit-gv: audit.o libort.a
	$(CC) -o $@ audit.o libort.a

ort-audit-json: audit.o libort.a
	$(CC) -o $@ audit.o libort.a

ort-xliff: xliff.o libort.a
	$(CC) -o $@ xliff.o libort.a $(LDFLAGS) -lexpat

www: $(IMAGES) $(HTMLS) ort.tar.gz ort.tar.gz.sha512 atom.xml

installwww: www
	mkdir -p $(WWWDIR)
	mkdir -p $(WWWDIR)/snapshots
	$(INSTALL_DATA) *.html *.css *.js $(IMAGES) atom.xml $(WWWDIR)
	$(INSTALL_DATA) ort.tar.gz ort.tar.gz.sha512 $(WWWDIR)/snapshots
	$(INSTALL_DATA) ort.tar.gz $(WWWDIR)/snapshots/ort-$(VERSION).tar.gz
	$(INSTALL_DATA) ort.tar.gz.sha512 $(WWWDIR)/snapshots/ort-$(VERSION).tar.gz.sha512

version.h: Makefile
	( echo "#define VERSION \"$(VERSION)\"" ; \
	  echo "#define VSTAMP `echo $$((($(VERSION_BUILD)+1)+($(VERSION_MINOR)+1)*100+($(VERSION_MAJOR)+1)*10000))`" ; ) >$@

header.o source.o: version.h

paths.h: Makefile
	( echo "#define PATH_GENSALT \"$(SHAREDIR)/kwebapp/gensalt.c\"" ; \
	  echo "#define PATH_B64_NTOP \"$(SHAREDIR)/kwebapp/b64_ntop.c\"" ; \
	  echo "#define PATH_JSMN \"$(SHAREDIR)/kwebapp/jsmn.c\"" ; ) >$@

source.o: paths.h

install: all
	mkdir -p $(DESTDIR)$(BINDIR)
	mkdir -p $(DESTDIR)$(MANDIR)/man1
	mkdir -p $(DESTDIR)$(MANDIR)/man5
	mkdir -p $(DESTDIR)$(SHAREDIR)/kwebapp
	$(INSTALL_MAN) $(MAN1S) $(DESTDIR)$(MANDIR)/man1
	$(INSTALL_MAN) ort.5 $(DESTDIR)$(MANDIR)/man5
	$(INSTALL_DATA) audit.html audit.css audit.js $(DESTDIR)$(SHAREDIR)/kwebapp
	$(INSTALL_DATA) b64_ntop.c jsmn.c gensalt.c $(DESTDIR)$(SHAREDIR)/kwebapp
	$(INSTALL_PROGRAM) $(BINS) $(DESTDIR)$(BINDIR)

uninstall:
	@for f in $(MAN1S); do \
		echo rm -f $(DESTDIR)$(MANDIR)/man1/$$f ; \
		rm -f $(DESTDIR)$(MANDIR)/man1/$$f ; \
	done
	rm -f $(DESTDIR)$(MANDIR)/man5/ort.5
	rm -f $(DESTDIR)$(SHAREDIR)/kwebapp/audit.{html,css,js}
	rm -f $(DESTDIR)$(SHAREDIR)/kwebapp/{b64_ntop,jsmn,gensalt}.c
	rmdir $(DESTDIR)$(SHAREDIR)/kwebapp
	@for f in $(BINS); do \
		echo rm -f $(DESTDIR)$(BINDIR)/$$f ; \
		rm -f $(DESTDIR)$(BINDIR)/$$f ; \
	done

ort.tar.gz.sha512: ort.tar.gz
	sha512 ort.tar.gz >$@

ort.tar.gz: $(DOTAR) $(DOTAREXEC)
	mkdir -p .dist/ort-$(VERSION)/
	install -m 0444 $(DOTAR) .dist/ort-$(VERSION)
	install -m 0555 $(DOTAREXEC) .dist/ort-$(VERSION)
	( cd .dist/ && tar zcf ../$@ ./ )
	rm -rf .dist/

test: test.o db.o db.db
	$(CC) -o $@ test.o db.o $(LDFLAGS) -lksql -lsqlite3 -lpthread -lkcgijson -lkcgi -lz $(LDADD)

audit-out.js: ort-audit-json audit-example.txt
	./ort-audit-json user audit-example.txt >$@

db.o: db.c db.h
	$(CC) $(CPPFLAGS) $(CFLAGS) -o $@ -c db.c

test.o: test.c db.h
	$(CC) $(CPPFLAGS) $(CFLAGS) -o $@ -c test.c

db.c: ort-c-source db.txt
	./ort-c-source -vsjJ db.txt >$@

db.h: ort-c-header db.txt
	./ort-c-header -vsjJ db.txt >$@

db.sql: ort-sql db.txt
	./ort-sql db.txt >$@

db.js: ort-javascript db.txt
	./ort-javascript db.txt >$@

db.ts: ort-javascript db.txt
	./ort-javascript -t db.txt >$@

db.update.sql: ort-sqldiff db.old.txt db.txt
	./ort-sqldiff db.old.txt db.txt >$@

db.trans.txt: ort-xliff db.txt db.fr.xml
	./ort-xliff -j db.txt db.fr.xml >$@

db.db: db.sql
	rm -f $@
	sqlite3 $@ < db.sql

$(LIBOBJS) $(OBJS): config.h extern.h ort.h

.5.5.html:
	mandoc -Thtml -Ostyle=mandoc.css $< >$@

.1.1.html:
	mandoc -Thtml -Ostyle=mandoc.css $< >$@

.dot.svg:
	dot -Tsvg $< > tmp-$@
	xsltproc --novalid notugly.xsl tmp-$@ >$@
	rm tmp-$@

db.txt.xml: db.txt
	( echo "<article data-sblg-article=\"1\">" ; \
	  highlight -l --enclose-pre --src-lang=conf -f db.txt ; \
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

db.txt.html: db.txt
	highlight -s whitengrey -I -l --src-lang=c db.txt >$@

audit-example.txt.html: audit-example.txt
	highlight -s whitengrey -I -l --src-lang=c audit-example.txt >$@

db.old.txt.html: db.old.txt
	highlight -s whitengrey -I -l --src-lang=c db.old.txt >$@

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
	highlight --print-style -s whitengrey

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

clean:
	rm -f $(BINS) version.h paths.h $(LIBOBJS) libort.a test test.o
	rm -f db.c db.h db.o db.sql db.js db.ts db.ts db.update.sql db.db db.trans.txt
	rm -f ort.tar.gz ort.tar.gz.sha512
	rm -f $(IMAGES) highlight.css $(HTMLS) atom.xml
	rm -f db.txt.xml db.h.xml db.sql.xml db.update.sql.xml test.xml.xml $(IHTMLS) TODO.xml
	rm -f source.o header.o javascript.o sql.o audit.o main.o xliff.o

distclean: clean
	rm -f config.h config.log Makefile.configure
