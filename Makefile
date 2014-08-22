CC=gcc
CFLAGS=-c -Wall -D_REENTRANT
LDFLAGS=-lc -lmicrohttpd -lconfig -lsqlite3 -lm -lpthread
DEBUGFLAGS=-DDEBUG -g -O0
RELEASEFLAGS=-O3
KILLALLFLAG=-q

all: angharad

static: angharad.o arduino-serial-lib.o nondevice-commands.o device-commands.o scheduler.o
	$(CC) $(LDFLAGSSTATIC) -o angharad angharad.o nondevice-commands.o device-commands.o scheduler.o arduino-serial-lib.o

angharad: angharad.o arduino-serial-lib.o nondevice-commands.o device-commands.o scheduler.o
	$(CC) $(LDFLAGS) -o angharad angharad.o nondevice-commands.o device-commands.o scheduler.o arduino-serial-lib.o

angharad.o: angharad.c angharad.h
	$(CC) $(CFLAGS) $(RELEASEFLAGS) angharad.c

nondevice-commands.o: nondevice-commands.c angharad.h
	$(CC) $(CFLAGS) $(RELEASEFLAGS) nondevice-commands.c

device-commands.o: device-commands.c angharad.h
	$(CC) $(CFLAGS) $(RELEASEFLAGS) device-commands.c

scheduler.o: scheduler.c angharad.h
	$(CC) $(CFLAGS) $(RELEASEFLAGS) scheduler.c

arduino-serial-lib.o: arduino-serial-lib.c arduino-serial-lib.h
	$(CC) $(CFLAGS) $(RELEASEFLAGS) arduino-serial-lib.c

clean:
	rm -f *.o angharad

stop:
	-sudo /etc/init.d/angharad stop

start:
	sudo /etc/init.d/angharad start

install: angharad
	sudo cp -f angharad /usr/local/bin
	
run: angharad stop install start

debug: angharad.c angharad.h nondevice-commands.c device-commands.c scheduler.c arduino-serial-lib.c arduino-serial-lib.h
	$(CC) $(CFLAGS) $(DEBUGFLAGS) arduino-serial-lib.c
	$(CC) $(CFLAGS) $(DEBUGFLAGS) nondevice-commands.c
	$(CC) $(CFLAGS) $(DEBUGFLAGS) device-commands.c
	$(CC) $(CFLAGS) $(DEBUGFLAGS) scheduler.c
	$(CC) $(CFLAGS) $(DEBUGFLAGS) angharad.c
	$(CC) $(LDFLAGS) $(DEBUGFLAGS) -o angharad angharad.o nondevice-commands.o device-commands.o scheduler.o arduino-serial-lib.o
	
test: debug
	./angharad angharad.conf
