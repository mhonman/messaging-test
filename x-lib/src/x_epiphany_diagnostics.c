/*
File: x_epiphany_diagnostics.c

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

/*  These host-side functions provide a way to get diagnotic information 
 *  from an epiphany chip and the SDK data structures. The Epiphany processor
 *  must have been mapped into host address space and the mapping info saved
 *  in the e_epiphany_t structure supplied to these routines. 
 */

#ifndef __epiphany__

#include <stdio.h>
#include <e-hal.h>
#include "x_lib_configuration.h"
#include "x_epiphany_internals.h"
#include "x_epiphany_diagnostics.h"

/*  x_dump_epiphany_workgroup
 *
 *  Assuming the supplied e_epiphany_t structure has been populated with data 
 *  by e_open, dumps the contents in 
 *  human-readable form. 
 */

void 
x_dump_epiphany_workgroup (e_epiphany_t * workgroup, 
                           unsigned int level_of_detail)
{
    int       row, col;
    e_core_t *curr_core;
    printf ("Workgroup of %d rows, %d columns at (col, row) (%d, %d) on an ",
            workgroup->rows, workgroup->cols, workgroup->col, workgroup->row);
    if (workgroup->type == 0) {
        printf ("E16G301\n");
    }
    else if (workgroup->type == 1) {
        printf ("E64G401\n");
    }
    else {
        printf ("unknown Epiphany chip type %d\n", workgroup->type);
    }
    if (level_of_detail > 0) {
        printf ("  Base coreid %d (0x%x)\n", 
                workgroup->base_coreid,
                workgroup->base_coreid);		
        for (row = 0; row < workgroup->rows; row++) {
            for (col = 0; col < workgroup->cols; col++) {
                curr_core = &(workgroup->core[row][col]);
                printf ("  Core ID %d (%d,%d)\tMem:  phys 0x%x host 0x%x\n\t\t\tRegs: phys 0x%x host 0x%x\n",
                        curr_core->id, curr_core->col, curr_core->row,  
                        (uint32_t)curr_core->mems.phy_base, (uint32_t)curr_core->mems.base,
                        (uint32_t)curr_core->regs.phy_base, (uint32_t)curr_core->regs.base);
            }
        }
    }	  
}

/*  x_dump_ecore_registers
 *
 *  Displays the contents of a core's register in human-readable form (sort of:
 *  the values are in hexadecimal).
 *
 *  Level of detail indicates the number of 16-register blocks to display.
 *  DMA register values are only dumped at level of detail >= 1.
 *
 *  Reading general purpose registers while the core is running seems to send
 *  it off into the woods (at least on Epiphany III). 
 */

void 
x_dump_ecore_registers (e_epiphany_t * workgroup, int row, int col, 
                        unsigned int level_of_detail)
{
    if ((row < 0) || (row >= workgroup->rows) ||
        (col < 0) || (col >= workgroup->cols)) {
        printf ("x_dump_ecore_registers: coordinates (row, col) = (%d, %d) are outside the workgroup\n",
                row, col);
    }
    else {            
        x_epiphany_core_registers_t *regs = (x_epiphany_core_registers_t*)
                                    (workgroup->core[row][col].regs.base);
        printf ("Core %d (%d,%d) registers from 0x%.8x mapped to 0x%.8x\n",
                workgroup->core[row][col].id, col, row,
                (uint32_t)(workgroup->core[row][col].regs.phy_base),
                (uint32_t)(workgroup->core[row][col].regs.base));
        if ((regs->debugstatus & 0x1) == 0) {
            printf ("Not in HALT state: General purpose registers cannot be safely read\n");
        }
        printf ("  CONFIG 0x%.8x  STATUS  0x%.8x  PC      0x%.8x  DEBUG   0x%.8x\n",
                   regs->config,  regs->status,   regs->pc,       regs->debugstatus);
        printf ("  LC     0x%.8x  LS      0x%.8x  LE      0x%.8x\n",
                   regs->lc,      regs->ls,       regs->le);
        printf ("  IRET   0x%.8x  IMASK   0x%.8x  ILAT    0x%.8x  IPEND   0x%.8x\n",
                   regs->iret,    regs->imask,    regs->ilat,     regs->ipend);
        printf ("  CTIMER0 0x%.8x  CTIMER1 0x%.8x\n",
                   regs->ctimer0,  regs->ctimer1);
        if ((regs->debugstatus & 0x1) != 0) {
            printf ("* R0(A1) 0x%.8x  R1(A2)  0x%.8x  R2(A3)  0x%.8x  R3(A4)  0x%.8x\n",
                       regs->r0,      regs->r1,       regs->r2,       regs->r3);
            printf ("  R4(V1) 0x%.8x  R5(V2)  0x%.8x  R6(V3)  0x%.8x  R7(V4)  0x%.8x\n",
                       regs->r4,      regs->r5,       regs->r6,       regs->r7);
            printf ("  R8(V5) 0x%.8x  R9(SB)  0x%.8x  R10(SL) 0x%.8x  R11(FP) 0x%.8x\n",
                       regs->r8,      regs->r9,       regs->r10,      regs->r11);
            printf ("  R12    0x%.8x  SP      0x%.8x  LR      0x%.8x  R15     0x%.8x\n",
                       regs->r12,     regs->sp,       regs->lr,       regs->r15);
            if (level_of_detail > 0) {
                printf ("* R16    0x%.8x  R17     0x%.8x  R18     0x%.8x  R19     0x%.8x\n",
                           regs->r16,     regs->r17,      regs->r18,      regs->r19);
                printf ("  R20    0x%.8x  R21     0x%.8x  R22     0x%.8x  R23     0x%.8x\n",
                           regs->r20,     regs->r21,      regs->r22,      regs->r23);
                printf ("  R24    0x%.8x  R25     0x%.8x  R26     0x%.8x  R27     0x%.8x\n",
                           regs->r24,     regs->r25,      regs->r26,      regs->r27);
                printf ("  R28    0x%.8x  R29     0x%.8x  R30     0x%.8x  R31     0x%.8x\n",
                           regs->r28,     regs->r29,      regs->r30,      regs->r31);
            }
            if (level_of_detail > 1) {
                printf ("* R32    0x%.8x  R33     0x%.8x  R34     0x%.8x  R35     0x%.8x\n",
                           regs->r32,     regs->r33,      regs->r34,      regs->r35);
                printf ("  R36    0x%.8x  R37     0x%.8x  R38     0x%.8x  R39     0x%.8x\n",
                           regs->r36,     regs->r37,      regs->r38,      regs->r39);
                printf ("  R40    0x%.8x  R41     0x%.8x  R42     0x%.8x  R43     0x%.8x\n",
                           regs->r40,     regs->r41,      regs->r42,      regs->r43);
                printf ("  R44    0x%.8x  R45     0x%.8x  R46     0x%.8x  R47     0x%.8x\n",
                           regs->r44,     regs->r45,      regs->r46,      regs->r47);
            }
            if (level_of_detail > 2) {
                printf ("* R48    0x%.8x  R49     0x%.8x  R50     0x%.8x  R51     0x%.8x\n",
                           regs->r48,     regs->r49,      regs->r50,      regs->r51);
                printf ("  R52    0x%.8x  R53     0x%.8x  R54     0x%.8x  R55     0x%.8x\n",
                           regs->r52,     regs->r53,      regs->r54,      regs->r55);
                printf ("  R56    0x%.8x  R57     0x%.8x  R58     0x%.8x  R59     0x%.8x\n",
                           regs->r56,     regs->r57,      regs->r58,      regs->r59);
                printf ("  R60    0x%.8x  R61     0x%.8x  R62     0x%.8x  R63     0x%.8x\n",
                           regs->r60,     regs->r61,      regs->r62,      regs->r63);
            }
        }
        if (level_of_detail > 0) {
            printf ("DMA0 CFG 0x%.8x  STRIDE  0x%.8x  COUNT   0x%.8x  SRCADR  0x%.8x\n",
                    regs->dma0config,regs->dma0stride,regs->dma0count,regs->dma0srcaddr);
            printf ("  DSTADR 0x%.8x  AUTO0   0x%.8x  AUTO1   0x%.8x  STATUS  0x%.8x\n",
                    regs->dma0dstaddr,regs->dma0autodma0,regs->dma0autodma1,regs->dma0status);
            printf ("DMA1 CFG 0x%.8x  STRIDE  0x%.8x  COUNT   0x%.8x  SRCADR  0x%.8x\n",
                    regs->dma1config,regs->dma1stride,regs->dma1count,regs->dma1srcaddr);
            printf ("  DSTADR 0x%.8x  AUTO0   0x%.8x  AUTO1   0x%.8x  STATUS  0x%.8x\n",
                    regs->dma1dstaddr,regs->dma1autodma0,regs->dma1autodma1,regs->dma1status);
        }        
    }        
}

/* x_dump_ecore_memory
 *
 *  Writes out a region of e_core memory in human-readable form. 
 *
 *  The only mode supported right now is PC relative, as that is what is
 *  needed to find out what is going on in the context of a trap. 
 *
 *  THIS ROUTINE IS ONLY PARTIALLY IMPLEMENTED. 
 *
 *  At least the following needs to be done:
 *  - implement modes other than PC-relative. 
 *  - check that the workgroup has been initialised. 
 */

void 
x_dump_ecore_memory (e_epiphany_t * workgroup,
                     int row, int col, int mode, 
                     int start_offset, int end_offset,
                     unsigned int level_of_detail)
{
    if ((row < 0) || (row >= workgroup->rows) ||
        (col < 0) || (col >= workgroup->cols)) {
        printf ("x_dump_ecore_memory: coordinates (row, col) = (%d, %d) are outside the workgroup\n",
                row, col);
        }
    else if (mode != X_PC_RELATIVE_DUMP_MODE) {
        printf ("x_dump_ecore_memory: mode %d is not defined\n", mode);
    }
    else {        
        x_epiphany_core_registers_t *regs = (x_epiphany_core_registers_t*)
                                    (workgroup->core[row][col].regs.base);
        uint32_t   pc, start_addr;
        int        size, i;
        e_mem_t    external_ram_region;
        char      *locally_mapped_start_addr;
        int        e_result;

        pc = regs->pc;
        start_addr = (uint32_t)((pc + start_offset - 3) & ~0x3);
        size = (end_offset+7-start_offset) & ~0x3;
        if (size < 0) {
            size = 4;
        }                  
        if ((pc & X_EPIPHANY_ADDRESS_COREID_MASK) == X_EPIPHANY_SHARED_DRAM_BASE) {
            printf("Data dump from external RAM\n");
            e_result = e_alloc (&external_ram_region,
                                (X_HOST_PROCESS_SHARED_DRAM_BASE 
                                 + (start_addr - X_EPIPHANY_SHARED_DRAM_BASE)),
                                size);
            if (e_result == E_ERR) {
                printf ("x_dump_ecore_memory: e_alloc failed on external RAM\n");
            }
            else {
                locally_mapped_start_addr = external_ram_region.base;
                for (i = 0; i < size; i += 4) {
                    printf ("(%d,%d) 0x%.8x => 0x%.8x\n", 
                            col, row, start_addr+i,
                            *((uint32_t*)(locally_mapped_start_addr+i)));     
                }                    
                e_free (&external_ram_region);            
            }
        }
        else if ((pc & X_EPIPHANY_ADDRESS_COREID_MASK) == 0) {
            printf("Data dump from ecore internal RAM\n");
            locally_mapped_start_addr = 
                            workgroup->core[row][col].mems.base + start_addr;
            for (i = 0; i < size; i += 4) {
                printf ("(%d,%d) 0x%.8x => 0x%.8x\n", col, row, start_addr+i,
                        *((uint32_t*)(locally_mapped_start_addr+i)));                    
            }                                
        }
        else {
            printf ("x_dump_ecore_memory: can only dump internal and external RAM at present\n");
        }        
    }
}        

#endif
