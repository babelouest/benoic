#
# Benoic device modules
#
# Makefile used to build the software
#
# Copyright 2016 Nicolas Mora <mail@babelouest.org>
#
# This program is free software; you can redistribute it and/or
# modify it under the terms of the GNU Lesser General Public License
# as published by the Free Software Foundation;
# version 2.1 of the License.
#
# This library is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	See the
# GNU GENERAL PUBLIC LICENSE for more details.
#
# You should have received a copy of the GNU General Public
# License along with this library.	If not, see <http://www.gnu.org/licenses/>.
#

VERSION=0.4.0
PREFIX=/usr/local
MODULES_TARGET=$(PREFIX)/lib/angharad/benoic
LIBZWPATH=/usr/local/lib
CC=gcc
CPP=g++
CFLAGS=-c -fPIC -Wall -D_REENTRANT -shared -nostartfiles -I$(PREFIX)/include -Wno-pragmas $(ADDITIONALFLAGS)
LIBS=-L$(PREFIX)/lib -lc -ljansson -lyder -lorcania -lhoel -ldl
LDFLAGS += -L$(LIBZWPATH) -Wl,-R$(LIBZWPATH) '-Wl,-R$$ORIGIN' -lopenzwave

oz_include=/usr/local/include/openzwave/

OZINCLUDES:= -I $(oz_include) -I $(oz_include)command_classes -I $(oz_include)platform -I $(oz_include)value_classes

all: release

libdevmock.so: device-mock.o
	$(CC) -shared -Wl,-soname,libdevmock.so -o libdevmock.so device-mock.o $(LIBS)

device-mock.o: ../benoic.h device-mock.c
	$(CC) $(CFLAGS) $(FLAGS_YDER) device-mock.c

device-zwave.o: device-zwave.cpp
	$(CPP) $(CFLAGS) -Wno-write-strings $(FLAGS_YDER) $(OZINCLUDES) device-zwave.cpp

libdevzwave.so: device-zwave.o
	$(CC) -shared -Wl,-soname,libdevzwave.so -o libdevzwave.so device-zwave.o $(LIBS) $(LDFLAGS)

clean:
	rm -f *.o *.so

debug: ADDITIONALFLAGS=-DDEBUG -g -O0

debug: libdevmock.so libdevzwave.so

release: ADDITIONALFLAGS=-O3

release: libdevmock.so libdevzwave.so

install: libdevmock.so libdevzwave.so
	mkdir -p $(MODULES_TARGET)
	cp -f libdevmock.so libdevzwave.so $(MODULES_TARGET)
