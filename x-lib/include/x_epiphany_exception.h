/*
File: x_epiphany_exception.h

Copyright 2013 Mark Honman

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU Lesser General Public License (LGPL)
as published by the Free Software Foundation, either version 3 of the
License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
and the GNU Lesser General Public License along with this program,
see the files COPYING and COPYING.LESSER. If not, see
<http://www.gnu.org/licenses/>.
*/

/* General Epiphany exception-handling definitions and prototypes for
 * both trap-handling and exception-handling. 
 */

#ifndef _X_EPIPHANY_EXCEPTION_H_
#define _X_EPIPHANY_EXCEPTION_H_

/*  Epiphany exceptions can be either a trap or a software 
 *  exception.
 */
 
typedef int32_t x_epiphany_exception_t;

#define X_EPIPHANY_EXCEPTION_TYPE_MASK \
                                (0x70000000)
#define X_EPIPHANY_EXCEPTION_VALUE_MASK \
                                (~(X_EPIPHANY_EXCEPTION_TYPE_MASK))
#define X_TRAP_EXCEPTION        (0x10000000)
#define X_SOFTWARE_EXCEPTION    (0x20000000)

#define X_NO_EXCEPTION          (0x00000000)

/*  Software exception numbers. 
 *
 *  This is a 4-bit field, bits 16 to 19 of the STATUS register, 
 *  but Epiphany-III and -IV have different definitions for it.
 *
 *  These constants are based on the Epiphany-IV definitions, and the
 *  exception codes must be mapped to them on Epiphany-III.  
 */
 
 #define X_UNIMPLEMENTED_SW_EXCEPTION      (0xF)
 #define X_SOFTWARE_INTERRUPT_SW_EXCEPTION (0xE)
 #define X_UNALIGNED_SW_EXCEPTION          (0xD)
 #define X_ILLEGAL_ACCESS_SW_EXCEPTION     (0xC)
 #define X_FPU_SW_EXCEPTION                (0x7)
 
/*  Epiphany trap numbers. The trap number range is 5 or 6 bits
 *  (documentation is unclear, 6 bits looks most likely), i.e. 0-63. 
 *  Negative numbers can thus be used for special "non trap" result 
 *  codes. 
 *  Most trap codes are presently unused, so symbolic names are only
 *  provided for the traps that have defined functionality. 
 *  See the Epiphany architecture reference for details. 
 */

#define X_WRITE_TRAP	(0)
#define X_READ_TRAP		(1)
#define X_OPEN_TRAP		(2)
#define X_EXIT_TRAP		(3)
#define X_CLOSE_TRAP	(6)
#define X_SYSCALL_TRAP	(7)

/* Trap instruction opcode - the trap number is in the top 6 bits. */

#define X_TRAP_OPCODE_BITS (0x03E2)
#define X_TRAP_OPCODE_MASK (0x3FF)
#define X_TRAP_NUMBER_SHIFT (10)

/* Macro to generate specific trap instruction opcodes */

#define X_SYSCALL_TRAP_OPCODE(_TRAP_NUMBER) \
           (((_TRAP_NUMBER)<<X_TRAP_NUMBER_SHIFT)|X_TRAP_OPCODE_BITS)

/* Return a string that describes the supplied exception */

char * x_epiphany_exception_desc (x_epiphany_exception_t exception);

/*  Check whether the specified core is in a halted state, returning the
 *  an exception number based on the reason for the halt (whether trap
 *  or software exception), X_NO_EXCEPTION if the core is not halted, 
 *  or a negative error code. 
 *  
 *  Only implemented for the host (even though it is possible to 
 *  implement on the Epiphany itself, the logic is totally different 
 *  and the host-only x_epiphany_control structure is not available). 
 */

#ifndef __epiphany__

#include <e-hal.h>
#include "x_epiphany_control.h"

x_epiphany_exception_t
x_catch_epiphany_exception (x_epiphany_control_t * epiphany_control, 
                            int row, int col);

#endif /* __epiphany__ */


#endif /* _X_EPIPHANY_EXCEPTION_H_ */