CFLAGS=-g -pg -I../libhelper
LIBS=-lm -lpng -lz -L../libhelper -lhelper -lpcap
DEFS=

all: minemap slimemap netmine

minemap: main.o anvil.o nbt.o
	$(CC) -o $@ $^ $(LIBS)

netmine: netmine.o anvil.o nbt.o
	$(CC) -o $@ $^ $(LIBS)

slimemap: slimemap.o
	$(CC) -o $@ $^ $(LIBS)

.c.o:
	$(CC) $(CFLAGS) $(DEFS) -o $@ -c $<

main.o : anvil.h

clean:
	rm -f *.o *~

