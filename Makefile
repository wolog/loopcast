DEBUG?=

CC:=gcc
CFLAGS=-Wall -O3 $(CFLAGS_DBG)
ifeq ($(DEBUG), 1)
CFLAGS_DBG=-DDEBUG
endif

TARGET=loopsend looprecv
OBJS=$(patsubst %.c,%.o,$(wildcard *.c))

all: $(TARGET)

# Always recompile loopcast.o as
# looprecv & loopsend shares it
# One will be built in gcc, the other in klcc
loopcast.o::
	$(CC) $(CFLAGS) -c loopcast.c

loopsend: loopsend.o loopcast.o

looprecv: looprecv.o loopcast.o

install-loopsend:
	mkdir -p $(DESTDIR)/usr/bin
	install -m 755 loopsend $(DESTDIR)/usr/bin

install-looprecv:
	mkdir -p $(DESTDIR)/usr/bin
	install -m 755 looprecv $(DESTDIR)/usr/bin

install: install-loopsend install-looprecv

clean:
	$(RM) $(TARGET) $(OBJS)

distclean: clean test-clean

test: $(TARGET) test.rand.in
	./tests/00-runall.sh

test.rand.in:
	dd if=/dev/urandom of=$@ bs=1M count=100

test-clean:
	$(RM) test.rand.in test.rand.out test.time test.md5

.PHONY: all clean test test-clean
