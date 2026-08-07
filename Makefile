CFLAGS?=-O2 -g -Wall -W $(shell pkg-config --cflags librtlsdr)
LDLIBS+=$(shell pkg-config --libs librtlsdr) -lpthread -lm
CC?=gcc
PROGNAME=dump1090

# All headers (dump1090.h, anet.h) live at the project root, so every
# module can just do #include "dump1090.h" regardless of which
# subdirectory it lives in.
CFLAGS+=-I.

OBJS=state.o util.o anet.o \
     core/sdr_capture.o core/demod.o core/decode.o core/aircraft_state.o \
     net/server.o net/protocols.o net/http_handler.o \
     ui/interactive_display.o \
     main.o

all: $(PROGNAME)

%.o: %.c dump1090.h anet.h
	$(CC) $(CFLAGS) -c $< -o $@

$(PROGNAME): $(OBJS)
	$(CC) -g -o $(PROGNAME) $(OBJS) $(LDFLAGS) $(LDLIBS)

clean:
	rm -f $(OBJS) $(PROGNAME)

.PHONY: all clean
