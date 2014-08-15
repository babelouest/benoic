CC=gcc
CFLAGS=-c -Wall -D_REENTRANT -O3
LDFLAGS=-lc -lmicrohttpd -lconfig -lsqlite3 -lm
DEBUGFLAGS=-DDEBUG -g
KILLALLFLAG=-q

all: angharad

static: angharad.o arduino-serial-lib.o nondevice-commands.o device-commands.o scheduler.o
	$(CC) $(LDFLAGSSTATIC) -o angharad angharad.o nondevice-commands.o device-commands.o scheduler.o arduino-serial-lib.o

angharad: angharad.o arduino-serial-lib.o nondevice-commands.o device-commands.o scheduler.o
	$(CC) $(LDFLAGS) -o angharad angharad.o nondevice-commands.o device-commands.o scheduler.o arduino-serial-lib.o

angharad.o: angharad.c angharad.h
	$(CC) $(CFLAGS) angharad.c

nondevice-commands.o: nondevice-commands.c angharad.h
	$(CC) $(CFLAGS) nondevice-commands.c

device-commands.o: device-commands.c angharad.h
	$(CC) $(CFLAGS) device-commands.c

scheduler.o: scheduler.c angharad.h
	$(CC) $(CFLAGS) scheduler.c

arduino-serial-lib.o: arduino-serial-lib.c arduino-serial-lib.h
	$(CC) $(CFLAGS) arduino-serial-lib.c

clean:
	rm -f *.o angharad test_eval test_mhd

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
	$(CC) $(LDFLAGS) -o angharad angharad.o nondevice-commands.o device-commands.o scheduler.o arduino-serial-lib.o
	
test: debug
	./angharad angharad.conf
