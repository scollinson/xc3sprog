# Spartan3 JTAG programmer and other utilities

# Copyright (C) 2004 Andrew Rogers

# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2 of the License, or
# (at your option) any later version.

# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.

# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA 
ACS_ROOT = /spare/bon/svn/acs

PREFIX?=/usr/local
DEVLIST=devlist.txt
DEVLIST_INSTALL=share/XC3Sprog
#FTDI_LIB=-DUSE_FTD2XX
DEFS=-DDEVICEDB=\"${PREFIX}/${DEVLIST_INSTALL}/${DEVLIST}\" ${FTDI_LIB}
PROGS=detectchain debug bitparse jedecparse xc3sprog
GCC=g++ -g -Wall -I/usr/local/include/ -I${ACS_ROOT}/devices/llbbc08/trunk/xguff/host/lib/include `libftdi-config --cflags`
#GCC=i386-mingw32msvc-g++ -g -Wall
ifeq ("$(FTDI_LIB)","")
LIBS=-lstdc++ `libftdi-config --libs` 
else
LIBS=-lstdc++ -lftd2xx
endif

all: ${PROGS}

install: all
	install ${PROGS} ${PREFIX}/bin
	install -d ${PREFIX}/${DEVLIST_INSTALL}
	install ${DEVLIST} ${PREFIX}/${DEVLIST_INSTALL}

debug: debug.o iobase.o ioftdi.o ioparport.o iodebug.o
	${GCC} ${LIBS} $^ -o $@

bitparse: bitparse.o bitfile.o
	${GCC} ${LIBS} $^ -o $@

jedecparse: jedecparse.o jedecfile.o
	${GCC} ${LIBS} $^ -o $@

detectchain: detectchain.o jtag.o iobase.o iofx2.o ioftdi.o ioparport.o iodebug.o devicedb.o
	${GCC} ${LIBS} $^ -o $@

xc3sprog: xc3sprog.o jtag.o iobase.o iofx2.o ioftdi.o ioparport.o iodebug.o bitfile.o devicedb.o progalgxcf.o progalgxc3s.o jedecfile.o progalgxc95x.o
	${GCC} ${LIBS} $^ -o $@

debug.o: debug.cpp iobase.h ioftdi.h iodebug.h
	${GCC} ${DEFS} -c $< -o $@

bitparse.o: bitparse.cpp bitfile.h
	${GCC} ${DEFS} -c $< -o $@

jedecparse.o: jedecparse.cpp jedecfile.h
	${GCC} ${DEFS} -c $< -o $@

detectchain.o: detectchain.cpp iobase.h iofx2.h ioftdi.h jtag.h iodebug.h devicedb.h
	${GCC} ${DEFS} -c $< -o $@

xc3sprog.o: xc3sprog.cpp iobase.h ioftdi.h jtag.h iodebug.h bitfile.h devicedb.h progalgxcf.h progalgxc3s.h
	${GCC} ${DEFS} -c $< -o $@

iobase.o: iobase.cpp iobase.h
	${GCC} ${DEFS} -c $< -o $@

iodebug.o: iodebug.cpp iodebug.h iobase.h
	${GCC} ${DEFS} -c $< -o $@

ioftdi.o: ioftdi.cpp ioftdi.h iobase.h
	${GCC} ${DEFS} -c $< -o $@ 

iofx2.o: iofx2.cpp iofx2.h iobase.h
	${GCC} ${DEFS} -c $< -o $@ 

ioparport.o: ioparport.cpp ioparport.h iobase.h
	${GCC} ${DEFS} -c $< -o $@ 

bitfile.o: bitfile.cpp bitfile.h
	${GCC} ${DEFS} -c $< -o $@

jedecfile.o: jedecfile.cpp jedecfile.h
	${GCC} ${DEFS} -c $< -o $@

jtag.o: jtag.cpp jtag.h
	${GCC} ${DEFS} -c $< -o $@

devlist: devlist.o 
	${GCC} ${DEFS}  $^ -o $@

devlist.h: devlist devicedb.h devlist.txt
	./devlist

devicedb.o: devicedb.cpp devicedb.h devlist.h
	${GCC} ${DEFS} -c $< -o $@

progalgxcf.o: progalgxcf.cpp progalgxcf.h iobase.h jtag.h bitfile.h
	${GCC} ${DEFS} -c $< -o $@

progalgxc3s.o: progalgxc3s.cpp progalgxc3s.h iobase.h jtag.h bitfile.h
	${GCC} ${DEFS} -c $< -o $@

progalgxc95x.o: progalgxc95x.cpp progalgxc95x.h iobase.h jtag.h bitfile.h
	${GCC} ${DEFS} -c $< -o $@

clean:
	rm -f *.o devlist devlist.h
	rm -f ${PROGS}
