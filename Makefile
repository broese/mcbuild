CFLAGS=-g -pg -I../libhelper
LIBS=-lm -lpng -lz -L../libhelper -lhelper -lpcap -lssl -lcrypto -lcurl
DEFS=-DDEBUG_MEMORY=0

all: mcproxy
#all: minemap netmine mcproxy mcsanvil chunkradar invedit

minemap: main.o anvil.o nbt.o draw.o
	$(CC) -o $@ $^ $(LIBS)

netmine: netmine.o anvil.o nbt.o
	$(CC) -o $@ $^ $(LIBS)

invedit: invedit.o nbt.o
	$(CC) -o $@ $^ $(LIBS)

chunkradar: chunkradar.o 
	$(CC) -o $@ $^ $(LIBS) -lSDL

mcsanvil: mcsanvil.o anvil.o nbt.o
	$(CC) -o $@ $^ $(LIBS)

mcproxy-rebuild: mcproxy.o ../libhelper/libhelper.a mcproxy

mcproxy: mcproxy.o
	$(CC) -o $@ $^ $(LIBS)

.c.o:
	$(CC) $(CFLAGS) $(DEFS) -o $@ -c $<

main.o: anvil.h nbt.h draw.h

anvil.o: anvil.h nbt.h

nbt.o: nbt.h

draw.o: anvil.h nbt.h draw.h

clean:
	rm -f *.o *~

mtrace: mcsanvil
	rm -rf region/*
	MALLOC_TRACE=mtrace ./mcsanvil 20120726_from-3rd-base-to-1nd-base.mcs
#	mtrace mtrace | awk '{ if ( $$1 ~ /^0x/ ) { addr2line -e filemap $$4 | getline $$fileline ; print $$fileline $$0 $$1; } }'
	mtrace mtrace | perl -e 'while (<>) { if (/^(0x\S+)\s+(0x\S+)\s+at\s+(0x\S+)/) { print "$$1 $$2 at ".`addr2line -e mcsanvil $$3`; } }'

