#
# Angharad server
#
# Environment used to control home devices (switches, sensors, heaters, etc)
# Using different protocols and controllers:
# - Arduino UNO
# - ZWave
#
# Makefile used to build the software
#
# Copyright 2014-2015 Nicolas Mora <mail@babelouest.org>
#
# This program is free software; you can redistribute it and/or
# modify it under the terms of the GNU GENERAL PUBLIC LICENSE
# License as published by the Free Software Foundation;
# version 3 of the License.
#
# This library is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU GENERAL PUBLIC LICENSE for more details.
#
# You should have received a copy of the GNU General Public
# License along with this library.  If not, see <http://www.gnu.org/licenses/>.
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

static: angharad.o arduino-serial-lib.o scheduler.o control-meta.o control-arduino.o control-zwave.o webserver.o set-data.o actions.o scripts.o tags.o tools.o misc.o api_rest.o
	$(CPP) $(LDFLAGSSTATIC) -o angharad angharad.o arduino-serial-lib.o scheduler.o control-meta.o control-arduino.o control-zwave.o webserver.o set-data.o actions.o scripts.o tags.o tools.o misc.o api_rest.o

angharad: angharad.o arduino-serial-lib.o scheduler.o control-meta.o control-arduino.o control-zwave.o webserver.o set-data.o actions.o scripts.o tags.o tools.o misc.o api_rest.o
	$(CPP) $(LIBS) $(LDFLAGS) -o angharad angharad.o arduino-serial-lib.o scheduler.o control-meta.o control-arduino.o control-zwave.o webserver.o set-data.o actions.o scripts.o misc.o tags.o tools.o api_rest.o

angharad.o: angharad.c angharad.h
	$(CC) $(CFLAGS) $(FLAGS) angharad.c

webserver.o: angharad.h webserver.c
	$(CC) $(CFLAGS) $(FLAGS) webserver.c

set-data.o: angharad.h set-data.c
	$(CC) $(CFLAGS) $(FLAGS) set-data.c

actions.o: angharad.h actions.c
	$(CC) $(CFLAGS) $(FLAGS) actions.c

scripts.o: angharad.h scripts.c
	$(CC) $(CFLAGS) $(FLAGS) scripts.c

tags.o: angharad.h tags.c
	$(CC) $(CFLAGS) $(FLAGS) tags.c

tools.o: angharad.h tools.c
	$(CC) $(CFLAGS) $(FLAGS) tools.c

misc.o: angharad.h misc.c
	$(CC) $(CFLAGS) $(FLAGS) misc.c

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

api_rest.o: api_rest.json
#	objcopy --input binary --output elf64-little --binary-architecture i386 api_rest.json api_rest.o
	objcopy --input binary --output elf32-littlearm --binary-architecture arm api_rest.json api_rest.o

clean:
	rm -f *.o angharad

stop:
	-sudo service angharad stop

start:
	sudo service angharad start

install: angharad
	sudo cp -f angharad /usr/bin
	
run: angharad stop install start

debug: clean api_rest.o
	$(CC) $(CFLAGS) $(DEBUGFLAGS) angharad.c
	$(CC) $(CFLAGS) $(DEBUGFLAGS) webserver.c
	$(CC) $(CFLAGS) $(DEBUGFLAGS) set-data.c
	$(CC) $(CFLAGS) $(DEBUGFLAGS) actions.c
	$(CC) $(CFLAGS) $(DEBUGFLAGS) scripts.c
	$(CC) $(CFLAGS) $(DEBUGFLAGS) tags.c
	$(CC) $(CFLAGS) $(DEBUGFLAGS) tools.c
	$(CC) $(CFLAGS) $(DEBUGFLAGS) misc.c
	$(CC) $(CFLAGS) $(DEBUGFLAGS) scheduler.c
	$(CC) $(CFLAGS) $(DEBUGFLAGS) arduino-serial-lib.c
	$(CC) $(CFLAGS) $(DEBUGFLAGS) control-meta.c
	$(CC) $(CFLAGS) $(DEBUGFLAGS) control-arduino.c
	$(CPP) $(CFLAGS) $(DEBUGFLAGS) $(OZINCLUDES) control-zwave.c
	$(CPP) $(LIBS) $(LDFLAGS) $(DEBUGFLAGS) -o angharad angharad.o arduino-serial-lib.o scheduler.o control-meta.o control-arduino.o control-zwave.o webserver.o set-data.o actions.o scripts.o misc.o tags.o tools.o api_rest.o
	
test: debug
	./angharad --config-file=./angharad.conf
