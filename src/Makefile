# ex: set ts=8:

CFLAGS = -W -Wall -Wno-unused -std=gnu99 -pedantic
DEBUGCFLAGS = -g -O0 -Wall -DDEBUG
DUJOURCFLAGS = -g -O0 -Wall -DDUJOUR
LFLAGS = -lm -lc
CC = gcc
OBJS = shac.o llist.o util.o mnt.o perm.o user.o path.o
PROGRAM = shac

all: shac

shac: $(OBJS)
	$(CC) $(LFLAGS) $(OBJS) -o $(PROGRAM)

shac.o: shac.c shac.h
util.o: shac.h util.c util.h
mnt.o: shac.h util.h mnt.c mnt.h

llist.o: llist.c llist.h

install: all
	cp -f shac /usr/local/bin/shac

debug: clean
	$(MAKE) "CFLAGS = $(DEBUGCFLAGS)" all

dujour: clean
	$(MAKE) "CFLAGS = $(DUJOURCFLAGS)" all

clean:
	$(RM) $(PROGRAM) $(OBJS) 2>/dev/null

