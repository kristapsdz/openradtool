include Makefile.configure

COMPAT_OBJS	= compat_err.o \
		  compat_progname.o \
		  compat_reallocarray.o \
		  compat_strlcat.o \
		  compat_strlcpy.o \
		  compat_strtonum.o
OBJS		= header.o \
		  linker.o \
		  main.o \
		  parser.o \
		  source.o

kwebapp: $(COMPAT_OBJS) $(OBJS)
	$(CC) -o $@ $(COMPAT_OBJS) $(OBJS)

$(COMPAT_OBJS) $(OBJS): config.h

$(OBJS): extern.h

clean:
	rm -f kwebapp $(COMPAT_OBJS) $(OBJS)

distclean: clean
	rm -f config.h config.log Makefile.configure
