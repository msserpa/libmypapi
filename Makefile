CC=gcc
CFLAGS=-O2 -fPIC -Wall
LDFLAGS=-shared -Wl,--no-as-needed -ldl -lpthread

libnamedyn=libmypapi.so

all: $(libnamedyn)
	@echo "Compiled! Yes!"

mypapi.o: mypapi.c
	$(CC) -c mypapi.c -o mypapi.o $(CFLAGS)

$(libnamedyn): mypapi.o
	$(CC) mypapi.o -o $(libnamedyn) $(CFLAGS) $(LDFLAGS)

clean:
	- rm -f *.o
	- rm -f $(libnamedyn)