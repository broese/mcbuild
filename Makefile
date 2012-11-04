CFLAGS=-g -pg -I../libhelper
LIBS=-lm -lpng -lz -L../libhelper -lhelper -lpcap -lssl
DEFS=

all: minemap netmine mcproxy nbttest

minemap: main.o anvil.o nbt.o draw.o ../libhelper/libhelper.a
	$(CC) -o $@ $^ $(LIBS)

netmine: netmine.o anvil.o nbt.o
	$(CC) -o $@ $^ $(LIBS)

mcsanvil: mcsanvil.o anvil.o nbt.o
	$(CC) -o $@ $^ $(LIBS)

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

