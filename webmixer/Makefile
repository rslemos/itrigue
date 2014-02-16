# BEGIN COPYRIGHT NOTICE
# 
# This file is part of program "I-Trigue 2.1 3300 Digital Control"
# Copyright 2013-2014  R. Lemos
# 
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
# 
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
# 
# You should have received a copy of the GNU General Public License
# along with this program.  If not, see <http://www.gnu.org/licenses/>.
# 
# END COPYRIGHT NOTICE

CFLAGS += $(shell pkg-config --cflags alsa jansson libmicrohttpd) -g -Wall -O0
LDFLAGS += $(shell pkg-config --libs alsa jansson libmicrohttpd) -ldl -lm -rdynamic

.PHONY: all

all: webmixer decodejson

webmixer: webmixer.o httpd.o alsa.o

decodejson: decodejson.o

clean:
	rm -f *.o webmixer decodejson

