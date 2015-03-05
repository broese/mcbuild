CFLAGS=-g -pg
LIBS=-lm -lpng -lz -L../libhelper -lhelper -lpcap -lcurl
DEFS=-DDEBUG_MEMORY=0 -D_FILE_OFFSET_BITS=64 -D_LARGEFILE_SOURCE
INC=-I../libhelper

UNAME := $(shell uname -s)
ifeq ($(UNAME),SunOS)
        INC  += -I/users/atm/broese/include
        LIBS += -lsocket -lnsl -lmd5 -L/users/atm/broese/lib -lz -lssl -lcrypto
endif
ifeq ($(UNAME),Linux)
        LIBS += -lcrypto -lz -lssl
endif


all: mcproxy mcpdump

mcproxy: mcproxy.o mcp_gamestate.o mcp_packet.o mcp_game.o mcp_ids.o
	$(CC) -o $@ $^ $(LIBS)

mcpdump: mcpdump.o mcp_gamestate.o mcp_packet.o mcp_ids.o
	$(CC) -o $@ $^ $(LIBS)

varint: varint.c
	$(CC) $(CFLAGS) $(INC) -o $@ $^ $(LIBS)


#all: minemap netmine mcproxy mcpdump mcsanvil chunkradar invedit

#minemap: main.o anvil.o nbt.o draw.o
#	$(CC) -o $@ $^ $(LIBS)

#netmine: netmine.o anvil.o nbt.o
#	$(CC) -o $@ $^ $(LIBS)

#invedit: invedit.o nbt.o
#	$(CC) -o $@ $^ $(LIBS)

#chunkradar: chunkradar.o 
#	$(CC) -o $@ $^ $(LIBS) -lSDL

#mcsanvil: mcsanvil.o anvil.o nbt.o
#	$(CC) -o $@ $^ $(LIBS)

.c.o:
	$(CC) $(CFLAGS) $(INC) $(DEFS) -o $@ -c $<

mcproxy.o mcpdump.o: mcp_gamestate.h mcp_game.h mcp_ids.h

mcp_gamestate.o: mcp_gamestate.h mcp_ids.h

mcp_game.o: mcp_gamestate.h mcp_game.h mcp_ids.h mcp_packet.h

mcp_ids.o: mcp_ids.h


#main.o: anvil.h nbt.h draw.h

#anvil.o: anvil.h nbt.h

#nbt.o: nbt.h

#draw.o: anvil.h nbt.h draw.h

clean:
	rm -f *.o *~

mtrace: mcproxy
	MALLOC_TRACE=mtrace ./mcproxy 10.0.0.1
#	mtrace mtrace | awk '{ if ( $$1 ~ /^0x/ ) { addr2line -e mcproxy $$4 | getline $$fileline ; print $$fileline $$0 $$1; } }'
	mtrace mtrace | perl -e 'while (<>) { if (/^(0x\S+)\s+(0x\S+)\s+at\s+(0x\S+)/) { print "$1 $2 at ".`addr2line -e mcproxy $3`; } }'

