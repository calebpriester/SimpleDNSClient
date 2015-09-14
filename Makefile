
CC=clang
CFLAGS=-Wall -g

BINS=dnsq


all: $(BINS)

clean:
		rm -f ${BINS}
