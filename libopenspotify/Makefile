# Destination prefix, defaults to /usr
# Use 'make prefix=/usr/local' if you prefer to
# have files intalled under /usr/local
prefix ?= /usr

CC = gcc
CFLAGS = -Wall -ggdb -I../include -fPIC -O2
LDFLAGS = -lcrypto -lresolv -lz -lvorbisfile
SHLIB_FLAGS = -Wl,-x
LIB_NAME = libopenspotify.so.3
LIB_ALTNAMES = 


# 'make nodebug=1 all' will turn off -DDEBUG 
ifneq ($(nodebug), 1)
	CFLAGS += -DDEBUG
endif


ifeq ($(shell uname -s),Linux)
	# Linux specific flags
	LIB_ALTNAMES = libopenspotify.so
	LDFLAGS += -lpthread -lrt
	SHLIB_FLAGS += -shared -Wl,-soname=$(LIB_NAME)
endif
ifeq ($(shell uname -s),Darwin)
	# Mac OS X specific flags
	LIB_NAME = libopenspotify.3.dylib
	LIB_ALTNAMES = libopenspotify.dylib
	SHLIB_FLAGS += -dynamiclib -Wl,-install_name,$(prefix)/lib/$(LIB_NAME) -current_version $(shell date +%y.%m.%d) -compatibility_version $(shell date +%y.%m)
endif


CORE_OBJS = aes.o browse.o buf.o cache.o channel.o commands.o dns.o ezxml.o handlers.o hashtable.o hmac.o link.o login.o iothread.o packet.o player.o playlist.o rbuf.o request.o search.o sha1.o shn.o toplistbrowse.o user.o util.o
LIB_OBJS = sp_album.o sp_artist.o sp_albumbrowse.o sp_artistbrowse.o sp_error.o sp_image.o sp_link.o sp_playlist.o sp_search.o sp_session.o sp_toplistbrowse.o sp_track.o sp_user.o


# Expose symbols in sp_*.c
sp_%.o: sp_%.c
	gcc $(CFLAGS) -c -o $@ $<

# Hide symbols in all other files
%.o: %.c
	gcc -fvisibility=hidden $(CFLAGS) -c -o $@ $<


all: $(CORE_OBJS) $(LIB_OBJS)
	gcc -o $(LIB_NAME) $(SHLIB_FLAGS) $(LDFLAGS) $(CORE_OBJS) $(LIB_OBJS)

install: all
	install -m 0755 -d $(DESTDIR)$(prefix)/share/doc/libopenspotify
	install -m 0644 TODO.txt ../README.txt ../LICENSE.txt $(DESTDIR)$(prefix)/share/doc/libopenspotify
	install -m 0755 -d $(DESTDIR)$(prefix)/lib
	install -m 0755 $(LIB_NAME) $(DESTDIR)$(prefix)/lib
	for altname in $(LIB_ALTNAMES); do ln -sf $(LIB_NAME) $(DESTDIR)$(prefix)/lib/$${altname}; done
	install -m 0755 -d $(DESTDIR)$(prefix)/include/spotify
	install -m 0644 ../include/spotify/api.h $(DESTDIR)$(prefix)/include/spotify/api.h
	install -m 0755 -d $(DESTDIR)$(prefix)/lib/pkgconfig
	install -m 0644 openspotify.pc.in $(DESTDIR)$(prefix)/lib/pkgconfig/openspotify.pc
	sed -e "s:^prefix=.*:prefix=$(prefix):" -e "s:@@VER@@:$(shell date +%Y%m%d):" < openspotify.pc.in > $(DESTDIR)$(prefix)/lib/pkgconfig/openspotify.pc

clean:
	rm -f $(CORE_OBJS) $(LIB_OBJS) $(LIB_NAME)
