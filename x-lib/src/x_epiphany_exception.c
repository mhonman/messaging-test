/*
File: x_epiphany_exception.c

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

/*  Functionality to facilitate the creation of code that interacts
 *  with the Epiphany trap and software exception mechanisms. 
 *  No interpretation is placed on the purpose of the exceptions 
 *  themselves. 
 */

#include <stdio.h>

#include "x_types.h"
#include "x_epiphany_control.h"
#include "x_epiphany_internals.h"
#include "x_epiphany_exception.h"

/*  x_epiphany_exception_desc
 *
 *  Given an exception code (trap or software exception), returns a string
 *  describing that exception.
 */

char * x_epiphany_exception_desc (x_epiphany_exception_t exception)
{
    uint32_t subtype = exception & X_EPIPHANY_EXCEPTION_VALUE_MASK;
    
    if (exception == X_NO_EXCEPTION) {
        return "No Exception";
    }
    else if ((exception & X_EPIPHANY_EXCEPTION_TYPE_MASK) == 
             X_SOFTWARE_EXCEPTION) {
        if (subtype == X_UNIMPLEMENTED_SW_EXCEPTION) {
            return "Software exception: Unimplemented";
        }
        else if (subtype == X_SOFTWARE_INTERRUPT_SW_EXCEPTION) {
            return "Software exception: Software Interrupt";
        }
        else if (subtype == X_UNALIGNED_SW_EXCEPTION) {
            return "Software exception: Unaligned Access";
        }
        else if (subtype == X_ILLEGAL_ACCESS_SW_EXCEPTION) {
            return "Software exception: Illegal Access";
        }
        else if (subtype == X_FPU_SW_EXCEPTION) {
            return "Software exception: Floating Point Unit";
        }
        else {
            return "Software exception of unknown type";
        }
    }    
    else if ((exception & X_EPIPHANY_EXCEPTION_TYPE_MASK) == 
             X_TRAP_EXCEPTION) {
        if (subtype == X_WRITE_TRAP) {
            return "Trap: Write";
        }
        else if (subtype == X_READ_TRAP) {
            return "Trap: Read";
        }
        else if (subtype == X_OPEN_TRAP) {
            return "Trap: Open";
        }
        else if (subtype == X_EXIT_TRAP) {
            return "Trap: Exit";
        }
        else if (subtype == X_CLOSE_TRAP) {
            return "Trap: Close";
        }
        else if (subtype == X_SYSCALL_TRAP) {
            return "Trap: System Call";
        }
        else {
            return "Trap of unknown type";
        }
    }    
}

/*======================= HOST-ONLY FUNCTIONS ========================*/

#ifndef __epiphany__

#include <e-hal.h>

#include "x_epiphany_diagnostics.h"

/*  x_catch_epiphany_exception
 *
 *  Check whether the specified core is in a halted state, returning the
 *  an exception number based on the reason for the halt (whether trap
 *  or software exception), X_NO_EXCEPTION if the core is not halted, 
 *  or a negative error code. 
 *  
 *  In the case of traps, access to the external memory is needed in order
 *  to extract the trap number from the instruction that caused the core to 
 *  halt.
 *
 *  The registers are dumped if the core is halted after an instruction 
 *  that is not a trap and the EXCAUSE field of the status register is empty.
 *
 *  Notes
 *    Not all halt conditions result from traps - software exceptions may
 *      also result in a halt (indicated by the EXCAUSE bits 19:16 of the  
 *      status register. 
 *    Traps are tested first because the other halt conditions are usually
 *      terminal.  
 *    All trap instructions are 16 bits wide.
 *    Unknown platform types are assumed to be Epiphany-IV architecture. 
 *    The platform type is only available with eSDK >= 5.13.09.10 & is thus
 *      assumed to be Epiphany-III. 
 *    
 */

x_epiphany_exception_t
x_catch_epiphany_exception (x_epiphany_control_t * epiphany_control,
                            int row, int col)
{
    x_epiphany_exception_t result = -1;
    
    if ((row < 0) || (row > epiphany_control->workgroup.rows) ||
        (col < 0) || (col > epiphany_control->workgroup.cols)) {
        printf ("%s: coordinates (row,col) = (%d,%d) are outside the workgroup\n",
                "x_catch_epiphany_exception", row, col);
    }
    else {        
        uint16_t   halt_instruction;
        uint16_t * locally_mapped_pc;
        int32_t    exception_cause;
        
        x_epiphany_core_registers_t * regs;
        regs = (x_epiphany_core_registers_t*)
                    (epiphany_control->workgroup.core[row][col].regs.base);

        if (!(regs->debugstatus & X_DEBUG_HALT_STATUS)) {
            result = X_NO_EXCEPTION;
        }
        else if (NULL == 
                 (locally_mapped_pc = 
                    x_epiphany_to_host_address (epiphany_control, 
                                                row, col, regs->pc))) {
            fprintf (stderr, 
                     "%s: failed to map core (%d,%d) pc 0x%x to a host address\n",
                     "x_catch_epiphany_exception", row, col, regs->pc);
        }
        else {
            halt_instruction = *(locally_mapped_pc-1);
            if ((halt_instruction & X_TRAP_OPCODE_MASK) == 
                X_TRAP_OPCODE_BITS) {
                result = X_TRAP_EXCEPTION | 
                         (halt_instruction >> X_TRAP_NUMBER_SHIFT);
            }
            else if ((regs->status & X_STATUS_EXCAUSE_MASK) != 0) {
                exception_cause = (regs->status & X_STATUS_EXCAUSE_MASK) >> 
                                        X_STATUS_EXCAUSE_SHIFT;
                
                if (1 /*(epiphany_control->platform.type == E_EMEK301) ||
                    (epiphany_control->platform.type == E_ZEDBOARD1601) ||
                    (epiphany_control->platform.type == E_PARALLELLA1601)*/) {
                    // convert Epiphany III causes to IV codes
                    if (exception_cause == 0x04 ) {
                        exception_cause = X_UNIMPLEMENTED_SW_EXCEPTION;
                    }
                    else if (exception_cause == 0x01) {
                        exception_cause = X_SOFTWARE_INTERRUPT_SW_EXCEPTION;
                    }
                    else if (exception_cause == 0x02) {
                        exception_cause = X_UNALIGNED_SW_EXCEPTION;
                    }
                    else if (exception_cause == 0x05) {
                        exception_cause = X_ILLEGAL_ACCESS_SW_EXCEPTION;
                    }
                    else if (exception_cause == 0x03) {
                        exception_cause = X_FPU_SW_EXCEPTION;
                    }
                }
                result = X_SOFTWARE_EXCEPTION | exception_cause;
            }
            else {
                fprintf(stderr,
                        "%s: core (%d,%d) pc 0x%x instruction 0x%x is not a trap\n",
                        "x_catch_epiphany_exception", row, col, regs->pc, halt_instruction);
                x_dump_ecore_registers (&(epiphany_control->workgroup), row, col, 0);
            }
        }		
    }
    return result;
}
    
#endif /* __epiphany__ */