CFLAGS=-g
LIBS_LIBHELPER=-L../libhelper -lhelper
LIBS=-lm -lpng -lz -lpcap -lcurl $(LIBS_LIBHELPER)
DEFS=-D_FILE_OFFSET_BITS=64 -D_LARGEFILE_SOURCE
INC=-I../libhelper

UNAME := $(shell uname -s)
ifeq ($(UNAME),SunOS)
	DEFS += -DBUG_UNNAMED_INITIALIZER=1
        INC  += -I/users/atm/broese/include
        LIBS += -lsocket -lnsl -lmd5 -L/users/atm/broese/lib -lz -lssl -lcrypto
	CC=gcc
endif
ifeq ($(UNAME),Linux)
        LIBS += -lcrypto -lz -lssl
endif


all: mcproxy mcpdump nbttest argtest spiral bptest



mcproxy: mcproxy.o mcp_gamestate.o mcp_packet.o mcp_game.o mcp_ids.o nbt.o mcp_build.o mcp_arg.o mcp_bplan.o
	$(CC) -o $@ $^ $(LIBS)

mcpdump: mcpdump.o mcp_gamestate.o mcp_packet.o mcp_ids.o nbt.o
	$(CC) -o $@ $^ $(LIBS)

spiral: spiral.o
	$(CC) -o $@ $^ $(LIBS)

nbttest: nbt.c
	$(CC) $(CFLAGS) $(INC) $(DEFS) -DTEST=1 -o $@ $^ $(LIBS_LIBHELPER)

argtest: mcp_arg.c mcp_ids.o mcp_gamestate.o mcp_packet.o nbt.o
	$(CC) $(CFLAGS) $(INC) $(DEFS) -DTEST=1 -o $@ $^ $(LIBS)

bptest: mcp_bplan.c mcp_ids.o mcp_packet.o nbt.o
	$(CC) $(CFLAGS) $(INC) $(DEFS) -DTEST=1 -o $@ $^ $(LIBS)

varint: varint.c
	$(CC) $(CFLAGS) $(INC) $(DEFS) -o $@ $^ $(LIBS)

.c.o:
	$(CC) $(CFLAGS) $(INC) $(DEFS) -o $@ -c $<



mcproxy.o mcpdump.o: mcp_gamestate.h mcp_game.h mcp_ids.h nbt.h

mcp_gamestate.o: mcp_gamestate.h mcp_ids.h

mcp_game.o: mcp_gamestate.h mcp_game.h mcp_ids.h mcp_packet.h mcp_build.h

mcp_ids.o: mcp_ids.h

mcp_packet.o: nbt.h

mcp_build.o: mcp_ids.h mcp_build.h mcp_bplan.h mcp_gamestate.h mcp_game.h mcp_arg.h

mcp_bplan.o: mcp_bplan.h mcp_packet.h



clean:
	rm -f *.o *~ nbttest mcproxy mcpdump argtest
