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

PREFIX=/usr/local
MODULES_TARGET=$(PREFIX)/lib/angharad/benoic
CC=gcc
CFLAGS=-c -fPIC -Wall -D_REENTRANT -shared -nostartfiles -I$(PREFIX)/include $(ADDITIONALFLAGS)
LIBS=-L$(PREFIX)/lib -lc -ljansson -lyder -lorcania -lhoel
VERSION=0.1.0

all: release

libdevmock.so: device-mock.o
	$(CC) -shared -Wl,-soname,libdevmock.so -o libdevmock.so device-mock.o $(LIBS)

device-mock.o: ../benoic.h device-mock.c
	$(CC) $(CFLAGS) $(FLAGS_YDER) device-mock.c

clean:
	rm -f *.o *.so

debug: ADDITIONALFLAGS=-DDEBUG -g -O0

debug: libdevmock.so

release: ADDITIONALFLAGS=-O3

release: libdevmock.so

install: libdevmock.so
	mkdir -p $(MODULES_TARGET)
	cp -f libdevmock.so $(MODULES_TARGET)
