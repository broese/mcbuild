CFLAGS=-g
DEFS=-D_FILE_OFFSET_BITS=64 -D_LARGEFILE_SOURCE
INC=-I../libhelper
LIBS_LIBHELPER=-L../libhelper -lhelper
LIBS=$(LIBS_LIBHELPER) -lm -lpng -lz -lcurl -lcrypto

SRC_BASE=$(addsuffix .c, mcp_packet mcp_ids mcp_types nbt)
SRC_MCPROXY=$(addsuffix .c, mcproxy mcp_gamestate mcp_game mcp_build mcp_arg mcp_bplan) $(SRC_BASE)
SRC_MCPDUMP=$(addsuffix .c, mcpdump mcp_gamestate) $(SRC_BASE)
SRC_ALL=$(SRC_MCPROXY) mcpdump.c varint.c nbt.c spiral.c

ALLBIN=mcproxy mcpdump
TSTBIN=nbttest argtest bptest varint

HDR_ALL=$(addsuffix .h, mcp_packet mcp_ids mcp_types nbt mcp_game mcp_gamestate mcp_build mcp_arg mcp_bplan)

DEPFILE=make.depend

ifeq ($(shell uname -s),SunOS)
	INC  += -I$(HOME)/include
	LIBS += -lsocket -lnsl -lmd5 -L$(HOME)/lib
	CC   = gcc
else
ifeq ($(shell uname -o),Cygwin)
	CFLAGS=-std=gnu99
endif
endif


all: $(ALLBIN)

test: $(TSTBIN)
	@echo > /dev/null

mcproxy: $(SRC_MCPROXY:.c=.o)
	$(CC) -o $@ $^ $(LIBS)

mcpdump: $(SRC_MCPDUMP:.c=.o)
	$(CC) -o $@ $^ $(LIBS)



argtest: $(SRC_BASE:.c=.o) mcp_arg.c
	$(CC) $(CFLAGS) $(INC) $(DEFS) -DTEST=1 -o $@ $^ $(LIBS)

bptest: $(SRC_BASE:.c=.o) mcp_bplan.c
	$(CC) $(CFLAGS) $(INC) $(DEFS) -DTEST=1 -o $@ $^ $(LIBS)

nbttest: nbt.c
	$(CC) $(CFLAGS) $(INC) $(DEFS) -DTEST=1 -o $@ $^ $(LIBS)

varint: varint.c
	$(CC) $(CFLAGS) $(INC) $(DEFS) -DTEST=1 -o $@ $^ $(LIBS)



.c.o: $(DEPFILE)
	$(CC) $(CFLAGS) $(DEFS) $(INC) $(CONFIG) -o $@ -c $<

$(DEPFILE): $(SRC_ALL) $(HDR_ALL)
	@rm -rf $(DEPFILE) $(DEPFILE).bak
	@touch $(DEPFILE)
	makedepend -Y -f $(DEPFILE) $(SRC_ALL) 2> /dev/null

install: $(ALLBIN)
	test -d $(HOME)/bin || mkdir $(HOME)/bin
	cp -a scripts/* $(HOME)/bin
	chmod +x $(HOME)/bin/mcb*
	echo 'export PATH="$$HOME/bin:$$PATH"' >> ~/.bashrc

clean:
	rm -f *.o *~ $(ALLBIN) $(TSTBIN) $(DEPFILE)

FORCE:

sinclude $(DEPFILE)
