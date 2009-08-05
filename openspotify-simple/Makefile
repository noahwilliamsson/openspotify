CC = gcc
CFLAGS = -Wall -ggdb -I../include -DDEBUG
LDFLAGS = -L../libopenspotify -lopenspotify

CORE_OBJS = simple.o
OUTPUT = simple

all: $(CORE_OBJS)
	(cd ../libopenspotify && make)
	gcc -o $(OUTPUT) $(LDFLAGS) $(CORE_OBJS)

clean:
	(cd ../libopenspotify && make clean)
	rm -f $(OUTPUT) $(CORE_OBJS)
