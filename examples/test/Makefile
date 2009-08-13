# Copyright (c) 2009 Spotify Ltd
#
# This file has intentionally been kept small. It is not en par with
# current make best-practices.

dirname = $(notdir $(PWD))
targets = $(dirname)

CFLAGS = -I../../include -ggdb -DDEBUG
LDFLAGS = -L../../libopenspotify
LDLIBS = -lopenspotify
ifdef libspotify
LDLIBS = -lspotify ../../libopenspotify/shn.o
endif

.PHONY: all check-libspotify clean distclean
all: check-libspotify $(targets)

check-libspotify:
#	@pkg-config --exists libspotify || (echo "Failed to find libspotify using pkg-config(1)" >&2 ; exit 1)

clean distclean:
	rm -fr *.o $(targets)

test: test.o browse.o appkey.o session.o
