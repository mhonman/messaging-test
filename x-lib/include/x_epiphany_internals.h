/*
File: x_epiphany_internals.h

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

#ifndef _X_EPIPHANY_INTERNALS_H_
#define _X_EPIPHANY_INTERNALS_H_

#include <stdint.h>

/* The top 12 bits of an Epiphany-side address are the coreid, 0
 * is a core-local address. 
 */
 
#define X_EPIPHANY_ADDRESS_COREID_MASK	(0xFFF00000)
#define X_EPIPHANY_ADDRESS_COREID_SHIFT	(20)

#define X_EPIPHANY_ADDRESS_COREID(_ADDRESS) \
            ((_ADDRESS)>>X_EPIPHANY_ADDRESS_COREID_SHIFT)

/*  Structure-equivalent to the e-core register file.
 *
 *  Filler elements in this structure are named xx1, xx2, xx3 etc. 
 *   
 *  Note that reading general-purpose registers of a running e-core
 *  from the host has side-effects and will probably cause the e-core
 *  to crash. It is safe to read the config through debug registers
 *  while the core is running.
 */

#pragma pack(4)
typedef struct {
        uint32_t r0,  r1,  r2,  r3,  r4,  r5,  r6,  r7,
                 r8,  r9,  r10, r11, r12, sp,  lr,  r15, 
                 r16, r17, r18, r19, r20, r21, r22, r23,
                 r24, r25, r26, r27, r28, r29, r30, r31,
                 r32, r33, r34, r35, r36, r37, r38, r39,
                 r40, r41, r42, r43, r44, r45, r46, r47,
                 r48, r49, r50, r51, r52, r53, r54, r55, 
                 r56, r57, r58, r59, r60, r61, r62, r63,
                 xx1[0xC0],
                 config,  status, pc,      debugstatus, 
                 xx2,     lc,     ls,      le,
                 iret,    imask,  ilat,    ilatst, 
                 ilatcl,  ipend,  ctimer0, ctimer1,
                 fstatus, xx3,    debugcmd, 
                 xx4[0x2D],
                 dma0config,  dma0stride,   dma0count,    dma0srcaddr,
                 dma0dstaddr, dma0autodma0, dma0autodma1, dma0status,
                 dma1config,  dma1stride,   dma1count,    dma1srcaddr,
                 dma1dstaddr, dma1autodma0, dma1autodma1, dma1status;
} x_epiphany_core_registers_t;
#pragma pack()

/* Status register bit-masks and shifts */

#define X_STATUS_EXCAUSE_MASK           (0x000F0000)
#define X_STATUS_EXCAUSE_SHIFT          (16)

/* Debug Status register bit-masks */

#define X_DEBUG_HALT_STATUS				(0x01)
#define X_DEBUG_EXTERNAL_LOAD_PENDING	(0x02)
#define X_DEBUG_MULTICORE_BREAKPOINT	(0x04)

/* Debug Command register bit-masks */

#define X_DEBUG_CMD_MASK				(0x03)
#define X_DEBUG_RESUME_CMD				(0x00)
#define X_DEBUG_HALT_CMD				(0x01)

#endif /* _X_EPIPHANY_INTERNALS_H_ */