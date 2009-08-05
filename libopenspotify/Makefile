CC = gcc
CFLAGS = -Wall -ggdb -I../include -DDEBUG
LDFLAGS = -lresolv -lcrypto

CORE_OBJS = buf.o dns.o hmac.o login.o network.o request.o sha1.o shn.o util.o 
LIB_OBJS = sp_error.o sp_session.o sp_user.o

# Linux
LIB_NAME = libopenspotify.so
SHLIB_FLAGS = -shared -lpthread

# Mac OS X
ifeq ($(shell uname -s),Darwin)
	LIB_NAME = libopenspotify.dylib
	SHLIB_FLAGS = -dynamiclib -dylib
endif

all: $(CORE_OBJS) $(LIB_OBJS)
	gcc -o $(LIB_NAME) $(SHLIB_FLAGS) $(LDFLAGS) $(CORE_OBJS) $(LIB_OBJS)

clean:
	rm -f $(CORE_OBJS) $(LIB_OBJS) $(LIB_NAME)
