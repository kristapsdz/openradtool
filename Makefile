.SUFFIXES: .dot .svg

include Makefile.configure

VERSION		 = 0.0.4
CFLAGS		+= -DVERSION=\"$(VERSION)\"
COMPAT_OBJS	 = compat_err.o \
		   compat_progname.o \
		   compat_reallocarray.o \
		   compat_strlcat.o \
		   compat_strlcpy.o \
		   compat_strtonum.o
OBJS		 = comment.o \
		   header.o \
		   linker.o \
		   main.o \
		   parser.o \
		   source.o \
		   sql.o

kwebapp: $(COMPAT_OBJS) $(OBJS)
	$(CC) -o $@ $(COMPAT_OBJS) $(OBJS)

www: index.svg index.html kwebapp.5.html kwebapp.1.html

OBJS: extern.h

test: test.o db.o db.db
	$(CC) -L/usr/local/lib -o $@ test.o db.o -lksql -lsqlite3

db.o: db.c db.h
	$(CC) $(CFLAGS) -I/usr/local/include -o $@ -c db.c

test.o: test.c db.h
	$(CC) $(CFLAGS) -I/usr/local/include -o $@ -c test.c

db.c: kwebapp db.txt
	./kwebapp -c db.h db.txt >$@

db.h: kwebapp db.txt
	./kwebapp -h db.txt >$@

db.sql: kwebapp db.txt
	./kwebapp -s db.txt >$@

db.db: db.sql
	rm -f $@
	sqlite3 $@ < db.sql

$(COMPAT_OBJS) $(OBJS): config.h

$(OBJS): extern.h

kwebapp.5.html: kwebapp.5
	mandoc -Thtml -Ostyle=mandoc.css kwebapp.5 >$@

kwebapp.1.html: kwebapp.1
	mandoc -Thtml -Ostyle=mandoc.css kwebapp.1 >$@

.dot.svg:
	dot -Tsvg $< | xsltproc --novalid notugly.xsl - >$@

db.txt.xml: db.txt
	( echo "<article data-sblg-article=\"1\">" ; \
	  highlight -l --enclose-pre --src-lang=c -f db.txt ; \
	  echo "</article>" ; ) >$@

db.h.xml: db.h
	( echo "<article data-sblg-article=\"1\">" ; \
	  highlight -l --enclose-pre --src-lang=c -f db.h ; \
	  echo "</article>" ; ) >$@

db.sql.xml: db.sql
	( echo "<article data-sblg-article=\"1\">" ; \
	  highlight -l --enclose-pre --src-lang=sql -f db.sql ; \
	  echo "</article>" ; ) >$@

db.c.html: db.c
	highlight -s whitengrey -I -l --src-lang=c db.c >$@

db.txt.html: db.txt
	highlight -s whitengrey -I -l --src-lang=c db.txt >$@

db.h.html: db.h
	highlight -s whitengrey -I -l --src-lang=c db.h >$@

highlight.css:
	highlight --print-style -s whitengrey

index.html: index.xml db.txt.xml db.txt.html db.h.xml db.h.html db.c.html db.sql.xml highlight.css
	sblg -s cmdline -t index.xml -o- db.txt.xml db.h.xml db.sql.xml > $@

clean:
	rm -f kwebapp $(COMPAT_OBJS) $(OBJS) db.c db.h db.o db.sql db.db test test.o
	rm -f index.svg index.html highlight.css kwebapp.5.html kwebapp.1.html
	rm -f db.txt.xml db.h.xml db.c.html db.h.html db.txt.html db.sql.xml

distclean: clean
	rm -f config.h config.log Makefile.configure
