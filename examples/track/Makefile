# Copyright (c) 2009 Spotify Ltd
#
# This file has intentionally been kept small. It is not en par with
# current make best-practices.

dirname = $(notdir $(PWD))
targets = $(dirname)

#CFLAGS = $(shell PKG_CONFIG_PATH=$(PKG_CONFIG_PATH) pkg-config --cflags libspotify)
#LDFLAGS = $(shell PKG_CONFIG_PATH=$(PKG_CONFIG_PATH) pkg-config --libs-only-L libspotify)
#LDLIBS = $(shell PKG_CONFIG_PATH=$(PKG_CONFIG_PATH) pkg-config --libs-only-l --libs-only-other libspotify)
CFLAGS = -ggdb -I../../include
LDFLAGS = -L../../libopenspotify
LDLIBS = -lopenspotify

.PHONY: all check-libspotify clean distclean
all: check-libspotify $(targets)

check-libspotify:
#	@pkg-config --exists libspotify || (echo "Failed to find libspotify using pkg-config(1)" >&2 ; exit 1)

clean distclean:
	rm -fr *.o $(targets)

track: track.o appkey.o session.o
