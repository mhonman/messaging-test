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
XHOBJS=${XLIB}/host/object
XEOBJS=${XLIB}/epiphany/object

# Make target directories if needed
for d in Debug $XHLIBS $XELIBS $XINCS $XHOBJS $XEOBJS ; do
  if [ ! -d $d ] ; then
    mkdir -p $d
  fi
done

# Build x-lib for HOST
echo Building host-side x-lib
(cd $XHOBJS;
gcc -c ../../src/*.c -I ../../include -I ${HINCS})
rm ${XHLIBS}/libx-lib.a
ar crs ${XHLIBS}/libx-lib.a ${XHOBJS}/*.o

# Build HOST side applications
echo Building host-side executables
gcc src/messaging_test.c -o Debug/messaging_test.elf -I ${XINCS} -I ${HINCS} -L ${XHLIBS} -L ${HLIBS} -lx-lib -le-hal 
gcc src/test_controller.c -o Debug/test_controller.elf -I ${XINCS} -I ${HINCS} -L ${XHLIBS} -L ${HLIBS} -lx-lib -le-hal 
gcc src/simon.c -o Debug/simon.elf -I ${XINCS} -I ${HINCS} -L ${XHLIBS} -L ${HLIBS} -lx-lib -le-hal 

# Build x-lib for DEVICE
echo Building device-side x-lib
(cd $XEOBJS;
e-gcc -O2 -c ../../src/*.c -I ../../include)
rm ${XELIBS}/libx-lib.a
ar crs ${XELIBS}/libx-lib.a ${XEOBJS}/*.o

# Build DEVICE side programs
echo Building device-side executables
for TARGET in e_messaging_test x_hello x_syscall_demo ; do
  echo ">>> $TARGET"
  e-gcc -T ${ELDF} src/${TARGET}.c -o Debug/${TARGET}.elf -I ${XINCS} -L ${XELIBS} -lx-lib -le-lib
done
for TARGET in x_naive_matmul ; do
  echo ">>> $TARGET"
  e-gcc -O3 -T ${ELDF} src/${TARGET}.c -o Debug/${TARGET}.elf -I ${XINCS} -L ${XELIBS} -lx-lib -le-lib
done

# Convert ebinary to SREC file
echo Converting epiphany executables to SREC
for TARGET in e_messaging_test x_hello x_naive_matmul x_syscall_demo ; do
  echo ">>> $TARGET"
  e-objcopy --srec-forceS3 --output-target srec Debug/${TARGET}.elf Debug/${TARGET}.srec
done

echo All done
