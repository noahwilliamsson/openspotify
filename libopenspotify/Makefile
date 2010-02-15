# Destination prefix, defaults to /usr
# Use 'make prefix=/usr/local' if you prefer to
# have files intalled under /usr/local
prefix ?= /usr

CC = gcc
CFLAGS = -Wall -ggdb -I../include -fPIC -O2
LDFLAGS = -lcrypto -lresolv -lz -lvorbisfile
SHLIB_FLAGS =
LIB_NAME = libopenspotify.so


# 'make nodebug=1 all' will turn off -DDEBUG 
ifneq ($(nodebug), 1)
	CFLAGS += -DDEBUG
endif


ifeq ($(shell uname -s),Linux)
	# Linux specific flags
	LDFLAGS += -lpthread -lrt
	SHLIB_FLAGS = -shared
endif
ifeq ($(shell uname -s),Darwin)
	# Mac OS X specific flags
	LIB_NAME = libopenspotify.dylib
	SHLIB_FLAGS = -dynamiclib -dylib
endif


CORE_OBJS = aes.o browse.o buf.o cache.o channel.o commands.o dns.o ezxml.o handlers.o hashtable.o hmac.o link.o login.o iothread.o packet.o player.o playlist.o rbuf.o request.o search.o sha1.o shn.o toplistbrowse.o user.o util.o
LIB_OBJS = sp_album.o sp_artist.o sp_albumbrowse.o sp_artistbrowse.o sp_error.o sp_image.o sp_link.o sp_playlist.o sp_search.o sp_session.o sp_toplistbrowse.o sp_track.o sp_user.o


all: $(CORE_OBJS) $(LIB_OBJS)
	gcc -o $(LIB_NAME) $(SHLIB_FLAGS) $(LDFLAGS) $(CORE_OBJS) $(LIB_OBJS)

install: all
	install -m 0755 -d $(DESTDIR)$(prefix)/share/doc/libopenspotify
	install -m 0644 TODO.txt ../README.txt ../LICENSE.txt $(DESTDIR)$(prefix)/share/doc/libopenspotify
	install -m 0755 -d $(DESTDIR)$(prefix)/lib
	install -m 0755 $(LIB_NAME) $(DESTDIR)$(prefix)/lib
	install -m 0755 -d $(DESTDIR)$(prefix)/include/spotify
	install -m 0644 ../include/spotify/api.h $(DESTDIR)$(prefix)/include/spotify/api.h
	install -m 0755 -d $(DESTDIR)$(prefix)/lib/pkgconfig
	install -m 0644 openspotify.pc.in $(DESTDIR)$(prefix)/lib/pkgconfig/openspotify.pc
	sed -e "s:^prefix=.*:prefix=$(prefix):" -e "s:@@VER@@:$(shell date +%Y%m%d):" < openspotify.pc.in > $(DESTDIR)$(prefix)/lib/pkgconfig/openspotify.pc

clean:
	rm -f $(CORE_OBJS) $(LIB_OBJS) $(LIB_NAME)
