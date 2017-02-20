#
# Compilers and flags to use on Sun's
#

#SUN4_CC = cc -pipe
#SUN4_CFLAGS = -g -w
# Sun's unbundled C compiler
#SUN4_CC = cc -pipe
#SUN4_CFLAGS = -O4 -w
# Sun's ANSI C compiler
#SUN4_CC = acc -pipe
#SUN4_CFLAGS = -Xa -O2 -w
# GNU C compiler
#SUN4_CC = gcc -pipe
#SUN4_CFLAGS = -g -w
# CenterLine (MetaWare)
#SUN4_CC = clcc
#SUN4_CFLAGS = -Xa -g -w

SUN4_CC = gcc -pipe
SUN4_CFLAGS = -g -O2

NEXT_CC = cc
NEXT_CFLAGS = -w -g -O2

NSFIP_CC = cc
NSFIP_CFLAGS = -w -g -O2 -Dmemcmp=Memcmp -DNSFIP

HP_CC = gcc -pipe
HP_CFLAGS = -O2

ici lib clean install tests:
	@if sun; then \
	  $(MAKE) A=sun CC='$(SUN4_CC)' CFLAGS='$(SUN4_CFLAGS)' DEFS='$(DEFS)' real_$@;\
	elif nsfip; then \
	  $(MAKE) A=nsfip CC='$(NSFIP_CC)' CFLAGS='$(NSFIP_CFLAGS)' DEFS='$(DEFS)' real_$@;\
	elif next; then \
	  $(MAKE) A=next CC='$(NEXT_CC)' CFLAGS='$(NEXT_CFLAGS)' DEFS='$(DEFS)' real_$@;\
	elif hp-pa; then \
	  $(MAKE) A=hp-pa CC='$(HP_CC)' CFLAGS='$(HP_CFLAGS)' DEFS='$(DEFS)' RANLIB=':' real_$@;\
	else \
	  echo "Unknown host architecture";\
	fi


LIBS	= -ltermcap -lm
LIB	= $A/ici.a
RANLIB	= ranlib
INSTALL	= install

BINDIR	= /usr/local/bin

.c.a:;

.c.o:;

real_ici : $A/ici

real_lib : $(LIB)

$A/ici	: $A/conf.o $(LIB)
	$(CC) $(LDFLAGS) $(CFLAGS) $(DEFS) -o $A/ici $A/conf.o $(LIB) $(LIBS)

$A/conf.o : conf.c
	$(CC) $(CFLAGS) $(DEFS) -o $@ -c conf.c

OBJS	= \
	$(LIB)(alloc.o) $(LIB)(arith.o) $(LIB)(array.o) $(LIB)(object.o) \
	$(LIB)(mem.o) $(LIB)(set.o) $(LIB)(call.o) $(LIB)(main.o) \
	$(LIB)(control.o) $(LIB)(struct.o) $(LIB)(exec.o) $(LIB)(float.o) \
	$(LIB)(func.o) $(LIB)(int.o) $(LIB)(icimain.o) $(LIB)(op.o) \
	$(LIB)(ptr.o) $(LIB)(string.o) $(LIB)(mark.o) $(LIB)(init.o) \
	$(LIB)(unary.o) $(LIB)(file.o) $(LIB)(catch.o) $(LIB)(smash.o) \
	$(LIB)(parse.o) $(LIB)(lex.o) $(LIB)(cfunc.o) \
	$(LIB)(null.o) $(LIB)(regexp.o) $(LIB)(nptrs.o) \
	$(LIB)(pc.o) $(LIB)(src.o) $(LIB)(exerror.o) \
	$(LIB)(clib.o) $(LIB)(syscall.o) $(LIB)(syserr.o) \
	$(LIB)(mkvar.o) $(LIB)(wrap.o) $(LIB)(mkstruct.o) $(LIB)(sfile.o) \
	$(LIB)(compile.o) $(LIB)(forall.o) \
	$(LIB)(skt.o) $(LIB)(trace.o) \
	$(LIB)(win.o) $(LIB)(ti.o) \
	$(LIB)(clib2.o)

#OBJS	= conf.o \
#	alloc.o arith.o array.o object.o \
#	mem.o set.o call.o main.o \
#	control.o struct.o exec.o float.o \
#	func.o int.o icimain.o op.o \
#	ptr.o string.o mark.o init.o \
#	unary.o file.o catch.o smash.o \
#	parse.o lex.o cfunc.o \
#	null.o regexp.o nptrs.o \
#	pc.o src.o exerror.o \
#	clib.o syscall.o syserr.o \
#	mkvar.o wrap.o mkstruct.o sfile.o \
#	compile.o forall.o clib2.o

.PRECIOUS: $(LIB)

$(LIB)	: $(OBJS)
	$(CC) -c $(CFLAGS) $(DEFS) -DLIBDIR='"/usr/local/lib/ici"'  $(?:.o=.c)
	$(AR) r $@ $?
	$(RANLIB) $@
	$(RM) $?

real_clean:
	rm -f *.o $A/*

real_install: $A/ici FORCE
	$(INSTALL) $A/ici $(BINDIR)

real_tests: $A/ici FORCE
	-cd test; ../$A/ici -f tst-all.ici

FORCE:

