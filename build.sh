#!/bin/bash

#set -e

ESDK=${EPIPHANY_HOME}
XLIB=x-lib
HLIBS=${ESDK}/tools/host/lib
HINCS=${ESDK}/tools/host/include
#ELIBS=${ESDK}/tools/e-gnu/epiphany-elf/lib
#EINCS=${ESDK}/tools/e-gnu/epiphany-elf/sys-include
ELDF=${XLIB}/bsps/current/fast.ldf
XHLIBS=${XLIB}/host/lib
XELIBS=${XLIB}/epiphany/lib
XINCS=${XLIB}/include

# Build x-lib for HOST
echo Building host-side x-lib
(cd x-lib/host/object;
gcc -c ../../src/*.c -I ../../include -I ${HINCS})
rm x-lib/host/lib/libx-lib.a
ar crs x-lib/host/lib/libx-lib.a x-lib/host/object/*.o

# Build HOST side application
echo Building host-side executable
gcc src/messaging_test.c -o Debug/messaging_test.elf -I ${XINCS} -I ${HINCS} -L ${XHLIBS} -L ${HLIBS} -lx-lib -le-hal 

# Build x-lib for DEVICE
echo Building device-side x-lib
(cd x-lib/epiphany/object;
e-gcc -O2 -c ../../src/*.c -I ../../include)
rm x-lib/epiphany/lib/libx-lib.a
ar crs x-lib/epiphany/lib/libx-lib.a x-lib/epiphany/object/*.o

# Build DEVICE side program
echo Building device-side executables
e-gcc -T ${ELDF} src/e_messaging_test.c -o Debug/e_messaging_test.elf -I ${XINCS} -L ${XELIBS} -lx-lib -le-lib

# Convert ebinary to SREC file
echo Converting epiphany executables to SREC
e-objcopy --srec-forceS3 --output-target srec Debug/e_messaging_test.elf Debug/e_messaging_test.srec

echo All done
