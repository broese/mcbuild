CC=clang
CFLAGS=-g
LIBS_LIBHELPER=-L../libhelper -lhelper
LIBS=-lm -lpng -lz -lpcap -lcurl $(LIBS_LIBHELPER)
DEFS=-DDEBUG_MEMORY=0 -D_FILE_OFFSET_BITS=64 -D_LARGEFILE_SOURCE
INC=-I../libhelper

UNAME := $(shell uname -s)
ifeq ($(UNAME),SunOS)
	DEFS += -DBUG_UNNAMED_INITIALIZER=1
        INC  += -I/users/atm/broese/include
        LIBS += -lsocket -lnsl -lmd5 -L/users/atm/broese/lib -lz -lssl -lcrypto
endif
ifeq ($(UNAME),Linux)
        LIBS += -lcrypto -lz -lssl
endif


all: mcproxy mcpdump nbttest



mcproxy: mcproxy.o mcp_gamestate.o mcp_packet.o mcp_game.o mcp_ids.o nbt.o mcp_build.o
	$(CC) -o $@ $^ $(LIBS)

mcpdump: mcpdump.o mcp_gamestate.o mcp_packet.o mcp_ids.o nbt.o
	$(CC) -o $@ $^ $(LIBS)

nbttest: nbt.c
	$(CC) $(CFLAGS) $(INC) $(DEFS) -DTEST=1 -o $@ nbt.c $(LIBS_LIBHELPER)

varint: varint.c
	$(CC) $(CFLAGS) $(INC) -o $@ $^ $(LIBS)



.c.o:
	$(CC) $(CFLAGS) $(INC) $(DEFS) -o $@ -c $<



mcproxy.o mcpdump.o: mcp_gamestate.h mcp_game.h mcp_ids.h nbt.h

mcp_gamestate.o: mcp_gamestate.h mcp_ids.h

mcp_game.o: mcp_gamestate.h mcp_game.h mcp_ids.h mcp_packet.h mcp_build.h

mcp_ids.o: mcp_ids.h

mcp_packet.o: nbt.h




clean:
	rm -f *.o *~ nbttest mcproxy mcpdump
