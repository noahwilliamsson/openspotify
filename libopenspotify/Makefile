CC = gcc
CFLAGS = -Wall -ggdb -I../include -fPIC -DDEBUG
LDFLAGS = -lcrypto -lresolv -lz

CORE_OBJS = adler32.o browse.o buf.o channel.o commands.o dns.o ezxml.o handlers.o hashtable.o hmac.o login.o network.o packet.o playlist.o request.o sha1.o shn.o util.o
LIB_OBJS = sp_album.o sp_artist.o sp_error.o sp_link.o sp_playlist.o sp_session.o sp_track.o sp_user.o

# Linux
LIB_NAME = libopenspotify.so
SHLIB_FLAGS = -shared -lpthread

# Mac OS X
ifeq ($(shell uname -s),Darwin)
	LIB_NAME = libopenspotify.dylib
	SHLIB_FLAGS = -dynamiclib -dylib
endif
ifeq ($(shell uname -s),Linux)
	LDFLAGS += -lrt
endif

all: $(CORE_OBJS) $(LIB_OBJS)
	gcc -o $(LIB_NAME) $(SHLIB_FLAGS) $(LDFLAGS) $(CORE_OBJS) $(LIB_OBJS)

clean:
	rm -f $(CORE_OBJS) $(LIB_OBJS) $(LIB_NAME)
