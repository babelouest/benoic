#
# Angharad server
#
# Environment used to control home devices (switches, sensors, heaters, etc)
# Using different protocols and controllers:
# - Arduino UNO
# - ZWave
#
# Copyright 2014-2015 Nicolas Mora <mail@babelouest.org>
# Gnu Public License V3 <http://fsf.org/>
#
# Makefile used to build the software
#
#

# Environment variables
# Modify them to fit your system
CC=gcc
CFLAGS=-c -Wall -D_REENTRANT
LIBS=-lc -lmicrohttpd -lconfig -lsqlite3 -lm -lpthread -lopenzwave -ludev
DEBUGFLAGS=-DDEBUG -g -O0
FLAGS=-O3
KILLALLFLAG=-q

oz_include=/usr/local/include/openzwave/

OZINCLUDES:= -I $(oz_include) -I $(oz_include)command_classes -I $(oz_include)platform -I $(oz_include)value_classes
LDFLAGS += -L/usr/local/lib -Wl,-R/usr/local/lib '-Wl,-R$$ORIGIN'
CPP=g++

all: angharad

static: angharad.o arduino-serial-lib.o nondevice-commands.o scheduler.o control-meta.o control-arduino.o control-zwave.o
	$(CPP) $(LDFLAGSSTATIC) -o angharad angharad.o nondevice-commands.o control-meta.o control-arduino.o control-zwave.o scheduler.o arduino-serial-lib.o

angharad: angharad.o arduino-serial-lib.o nondevice-commands.o scheduler.o control-meta.o control-arduino.o control-zwave.o
	$(CPP) $(LIBS) $(LDFLAGS) -o angharad angharad.o nondevice-commands.o control-meta.o control-arduino.o control-zwave.o scheduler.o arduino-serial-lib.o

angharad.o: angharad.c angharad.h
	$(CC) $(CFLAGS) $(FLAGS) angharad.c

nondevice-commands.o: nondevice-commands.c angharad.h
	$(CC) $(CFLAGS) $(FLAGS) nondevice-commands.c

control-meta.o: control-meta.c angharad.h
	$(CC) $(CFLAGS) $(FLAGS) control-meta.c

control-arduino.o: control-arduino.c angharad.h
	$(CC) $(CFLAGS) $(FLAGS) control-arduino.c

control-zwave.o: control-zwave.c angharad.h
	$(CPP) $(CFLAGS) $(FLAGS) $(OZINCLUDES) control-zwave.c

scheduler.o: scheduler.c angharad.h
	$(CC) $(CFLAGS) $(FLAGS) scheduler.c

arduino-serial-lib.o: arduino-serial-lib.c arduino-serial-lib.h
	$(CC) $(CFLAGS) $(FLAGS) arduino-serial-lib.c

clean:
	rm -f *.o angharad

stop:
	-sudo service angharad stop

start:
	sudo service angharad start

install: angharad
	sudo cp -f angharad /usr/bin
	
run: angharad stop install start

debug: angharad.c angharad.h nondevice-commands.c control-meta.c control-arduino.c control-zwave.c scheduler.c arduino-serial-lib.c arduino-serial-lib.h
	$(CC) $(CFLAGS) $(DEBUGFLAGS) arduino-serial-lib.c
	$(CC) $(CFLAGS) $(DEBUGFLAGS) nondevice-commands.c
	$(CC) $(CFLAGS) $(DEBUGFLAGS) control-meta.c
	$(CC) $(CFLAGS) $(DEBUGFLAGS) control-arduino.c
	$(CPP) $(CFLAGS) $(DEBUGFLAGS) $(OZINCLUDES) control-zwave.c
	$(CC) $(CFLAGS) $(DEBUGFLAGS) scheduler.c
	$(CC) $(CFLAGS) $(DEBUGFLAGS) angharad.c
	$(CPP) $(LIBS) $(LDFLAGS) $(DEBUGFLAGS) -o angharad angharad.o nondevice-commands.o control-meta.o control-arduino.o control-zwave.o scheduler.o arduino-serial-lib.o
	
test: debug
	./angharad ./angharad.conf
