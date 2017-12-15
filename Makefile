.SUFFIXES: .dot .svg .1 .1.html .5 .5.html

include Makefile.configure

VERSION		 = 0.3.9
CFLAGS		+= -DVERSION=\"$(VERSION)\"
OBJS		 = audit.o \
		   comments.o \
		   compats.o \
		   header.o \
		   linker.o \
		   javascript.o \
		   main.o \
		   parser.o \
		   printer.o \
		   protos.o \
		   source.o \
		   sql.o
HTMLS		 = archive.html \
		   index.html \
		   kwebapp.1.html \
		   kwebapp-audit.1.html \
		   kwebapp-c-header.1.html \
		   kwebapp-c-source.1.html \
		   kwebapp-javascript.1.html \
		   kwebapp-sql.1.html \
		   kwebapp-sqldiff.1.html \
		   kwebapp.5.html
WWWDIR		 = /var/www/vhosts/kristaps.bsd.lv/htdocs/kwebapp
MAN1S		 = kwebapp.1 \
		   kwebapp-audit.1 \
		   kwebapp-c-header.1 \
		   kwebapp-c-source.1 \
		   kwebapp-javascript.1 \
		   kwebapp-sql.1 \
		   kwebapp-sqldiff.1
DOTAR		 = comments.c \
		   compats.c \
		   configure \
		   extern.h \
		   header.c \
		   javascript.c \
		   $(MAN1S) \
		   kwebapp.5 \
		   linker.c \
		   Makefile \
		   main.c \
		   parser.c \
		   printer.c \
		   protos.c \
		   source.c \
		   sql.c \
		   tests.c \
		   test.c
XMLS		 = db.txt.xml \
		   test.xml.xml \
		   TODO.xml \
		   versions.xml
IHTMLS		 = db.txt.html \
		   db.h.html \
		   db.c.html \
		   db.sql.html \
		   db.old.txt.html \
		   db.update.sql.html \
		   db.js.html \
		   test.c.html
LINKS		 = kwebapp-audit \
		   kwebapp-c-header \
		   kwebapp-c-source \
		   kwebapp-javascript \
		   kwebapp-sql \
		   kwebapp-sqldiff

all: kwebapp $(LINKS)

kwebapp: $(OBJS)
	$(CC) -o $@ $(OBJS)

www: index.svg $(HTMLS) kwebapp.tar.gz kwebapp.tar.gz.sha512

installwww: www
	mkdir -p $(WWWDIR)
	mkdir -p $(WWWDIR)/snapshots
	install -m 0444 *.html *.css index.svg $(WWWDIR)
	install -m 0444 kwebapp.tar.gz kwebapp.tar.gz.sha512 $(WWWDIR)/snapshots
	install -m 0444 kwebapp.tar.gz $(WWWDIR)/snapshots/kwebapp-$(VERSION).tar.gz
	install -m 0444 kwebapp.tar.gz.sha512 $(WWWDIR)/snapshots/kwebapp-$(VERSION).tar.gz.sha512

install: kwebapp
	mkdir -p $(DESTDIR)$(BINDIR)
	mkdir -p $(DESTDIR)$(MANDIR)/man1
	mkdir -p $(DESTDIR)$(MANDIR)/man5
	$(INSTALL_PROGRAM) kwebapp $(DESTDIR)$(BINDIR)
	$(INSTALL_MAN) $(MAN1S) $(DESTDIR)$(MANDIR)/man1
	$(INSTALL_MAN) kwebapp.5 $(DESTDIR)$(MANDIR)/man5
	ln -f $(DESTDIR)$(BINDIR)/kwebapp $(DESTDIR)$(BINDIR)/kwebapp-audit
	ln -f $(DESTDIR)$(BINDIR)/kwebapp $(DESTDIR)$(BINDIR)/kwebapp-c-source
	ln -f $(DESTDIR)$(BINDIR)/kwebapp $(DESTDIR)$(BINDIR)/kwebapp-c-header
	ln -f $(DESTDIR)$(BINDIR)/kwebapp $(DESTDIR)$(BINDIR)/kwebapp-javascript
	ln -f $(DESTDIR)$(BINDIR)/kwebapp $(DESTDIR)$(BINDIR)/kwebapp-sql
	ln -f $(DESTDIR)$(BINDIR)/kwebapp $(DESTDIR)$(BINDIR)/kwebapp-sqldiff

kwebapp.tar.gz.sha512: kwebapp.tar.gz
	sha512 kwebapp.tar.gz >$@

kwebapp.tar.gz: $(DOTAR)
	mkdir -p .dist/kwebapp-$(VERSION)/
	install -m 0444 $(DOTAR) .dist/kwebapp-$(VERSION)
	( cd .dist/ && tar zcf ../$@ ./ )
	rm -rf .dist/

$(LINKS): kwebapp
	ln -f kwebapp $@

OBJS: extern.h

test: test.o db.o db.db
	$(CC) -Wextra -L/usr/local/lib -o $@ test.o db.o -lksql -lsqlite3 -lkcgijson -lkcgi -lz

db.o: db.c db.h
	$(CC) $(CFLAGS) -Wextra -I/usr/local/include -o $@ -c db.c

test.o: test.c db.h
	$(CC) $(CFLAGS) -Wextra -I/usr/local/include -o $@ -c test.c

db.c: kwebapp-c-source db.txt
	./kwebapp-c-source -vsj db.txt >$@

db.h: kwebapp-c-header db.txt
	./kwebapp-c-header -vsj db.txt >$@

db.sql: kwebapp-sql db.txt
	./kwebapp-sql db.txt >$@

db.js: kwebapp-javascript db.txt
	./kwebapp-javascript db.txt >$@

db.update.sql: kwebapp-sqldiff db.old.txt db.txt
	./kwebapp-sqldiff db.old.txt db.txt >$@

db.db: db.sql
	rm -f $@
	sqlite3 $@ < db.sql

$(OBJS): config.h extern.h

.5.5.html:
	mandoc -Thtml -Ostyle=mandoc.css $< >$@

.1.1.html:
	mandoc -Thtml -Ostyle=mandoc.css $< >$@

.dot.svg:
	dot -Tsvg $< | xsltproc --novalid notugly.xsl - >$@

db.txt.xml: db.txt
	( echo "<article data-sblg-article=\"1\">" ; \
	  highlight -l --enclose-pre --src-lang=conf -f db.txt ; \
	  echo "</article>" ; ) >$@

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

db.old.txt.html: db.old.txt
	highlight -s whitengrey -I -l --src-lang=c db.old.txt >$@

db.h.html: db.h
	highlight -s whitengrey -I -l --src-lang=c db.h >$@

db.sql.html: db.sql
	highlight -s whitengrey -I -l --src-lang=sql db.sql >$@

db.js.html: db.js
	highlight -s whitengrey -I -l --src-lang=js db.js >$@

db.update.sql.html: db.update.sql
	highlight -s whitengrey -I -l --src-lang=sql db.update.sql >$@

highlight.css:
	highlight --print-style -s whitengrey

index.html: index.xml $(XMLS) $(IHTMLS) highlight.css
	sblg -s cmdline -t index.xml -o- $(XMLS) | \
	       sed "s!@VERSION@!$(VERSION)!g" > $@

archive.html: archive.xml versions.xml
	sblg -s date -t archive.xml -o- versions.xml | \
	       sed "s!@VERSION@!$(VERSION)!g" > $@

TODO.xml: TODO.md
	( echo "<article data-sblg-article=\"1\">" ; \
	  lowdown -Thtml TODO.md ; \
	  echo "</article>" ; ) >$@

clean:
	rm -f kwebapp $(LINKS)
	rm -f $(OBJS) db.c db.h db.o db.sql db.js db.update.sql db.db test test.o
	rm -f kwebapp.tar.gz kwebapp.tar.gz.sha512
	rm -f index.svg highlight.css $(HTMLS)
	rm -f db.txt.xml db.h.xml db.sql.xml db.update.sql.xml test.xml.xml $(IHTMLS) TODO.xml

distclean: clean
	rm -f config.h config.log Makefile.configure
