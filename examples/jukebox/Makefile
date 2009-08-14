# Copyright (c) 2009 Spotify Ltd
#
# This file has intentionally been kept small. It is not en par with
# current make best-practices.

dirname = $(notdir $(PWD))
targets = $(dirname)

alsa_CFLAGS = $(shell PKG_CONFIG_PATH=$(PKG_CONFIG_PATH) pkg-config --cflags alsa)
alsa_LDFLAGS = $(shell PKG_CONFIG_PATH=$(PKG_CONFIG_PATH) pkg-config --libs-only-L alsa)
alsa_LDLIBS = $(shell PKG_CONFIG_PATH=$(PKG_CONFIG_PATH) pkg-config --libs-only-l --libs-only-other alsa)

CFLAGS = -ggdb -I../../include
LDFLAGS = -L../../libopenspotify
LDLIBS = -lopenspotify
ifdef libspotify
CFLAGS = $(shell PKG_CONFIG_PATH=$(PKG_CONFIG_PATH) pkg-config --cflags libspotify) $(alsa_CFLAGS) -D__USE_XOPEN=1 -DI_UNDERSTAND_REMOVE=1
LDFLAGS = $(shell PKG_CONFIG_PATH=$(PKG_CONFIG_PATH) pkg-config --libs-only-L libspotify) $(alsa_LDFLAGS)
LDLIBS = $(shell PKG_CONFIG_PATH=$(PKG_CONFIG_PATH) pkg-config --libs-only-l --libs-only-other libspotify) $(alsa_LDLIBS)
endif

CFLAGS += -DDEBUG

JUKEBOX_OBJS = jukebox.o appkey.o
ifeq ($(shell uname -s),Linux)
	JUKEBOX_OBJS += alsa-audio.o
else
	JUKEBOX_OBJS += dummy-audio.o
endif


.PHONY: all check-libspotify clean distclean
all: check-libspotify $(targets)

check-libspotify:
#	@pkg-config --exists libspotify || (echo "Failed to find libspotify using pkg-config(1)" >&2 ; exit 1)
#	@pkg-config --exists alsa || (echo "Failed to find libalsa using pkg-config(1)" >&2 ; exit 1)

clean distclean:
	rm -fr *.o $(targets)

jukebox: $(JUKEBOX_OBJS)

alsa-audio.o: alsa-audio.c audio.h
jukebox.o: jukebox.c audio.h
