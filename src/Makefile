# ex: set ts=8:

CFLAGS = -W -Wall -std=gnu99 -pedantic
DEBUGCFLAGS = -g -O0 -Wall -DDEBUG
DUJOURCFLAGS = -g -O0 -Wall -DDUJOUR
LFLAGS = -lm -lc
CC = gcc
OBJS = shac.o llist.o
PROGRAM = shac

all: shac

shac: $(OBJS)
	$(CC) $(LFLAGS) $(OBJS) -o $(PROGRAM)

shac.o: shac.c shac.h

llist.o: llist.c llist.h

install: all
	cp -f shac /usr/local/bin/shac

debug: clean
	$(MAKE) "CFLAGS = $(DEBUGCFLAGS)" all

dujour: clean
	$(MAKE) "CFLAGS = $(DUJOURCFLAGS)" all

clean:
	$(RM) $(PROGRAM) $(OBJS) 2>/dev/null

