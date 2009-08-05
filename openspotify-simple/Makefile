CC = gcc
CFLAGS = -Wall -ggdb -I../include -DDEBUG

ifndef libspotify
LDFLAGS = -L../libopenspotify -lopenspotify
else
LDFLAGS = -lspotify
endif

CORE_OBJS = simple.o
OUTPUT = simple

all: $(CORE_OBJS)
	(cd ../libopenspotify && make)
	gcc -o $(OUTPUT) $(LDFLAGS) $(CORE_OBJS)

clean:
	(cd ../libopenspotify && make clean)
	rm -f $(OUTPUT) $(CORE_OBJS)
