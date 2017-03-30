include Makefile.configure

VERSION		 = 0.0.2
CFLAGS		+= -DVERSION=\"$(VERSION)\"
COMPAT_OBJS	 = compat_err.o \
		   compat_progname.o \
		   compat_reallocarray.o \
		   compat_strlcat.o \
		   compat_strlcpy.o \
		   compat_strtonum.o
OBJS		 = header.o \
		   linker.o \
		   main.o \
		   parser.o \
		   source.o \
		   sql.o

kwebapp: $(COMPAT_OBJS) $(OBJS)
	$(CC) -o $@ $(COMPAT_OBJS) $(OBJS)

test: db.o db.db

db.o: db.c db.h
	$(CC) $(CFLAGS) -I/usr/local/include -o $@ -c db.c

db.c: kwebapp db.txt
	./kwebapp -c db.txt >$@

db.h: kwebapp db.txt
	./kwebapp -h db.txt >$@

db.sql: kwebapp db.txt
	./kwebapp -s db.txt >$@

db.db: db.sql
	rm -f $@
	sqlite3 $@ < db.sql

$(COMPAT_OBJS) $(OBJS): config.h

$(OBJS): extern.h

clean:
	rm -f kwebapp $(COMPAT_OBJS) $(OBJS) db.c db.h db.o db.sql db.db

distclean: clean
	rm -f config.h config.log Makefile.configure
