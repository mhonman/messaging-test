/*
File: x_epiphany_diagnostics.h

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

#ifndef _X_EPIPHANY_DIAGNOSTICS_H_
#define _X_EPIPHANY_DIAGNOSTICS_H_

/* Diagnostic routines to extract information from the Epiphany
   processor and esdk data structures and write it to standard output 
   in human-readable form.
*/

/* Provide basic information on the structure of the present Epiphany
   workgroup as recorded by the esdk routines. 
   At level_of_detail > 0 the host-side physical and mapped memory base 
   addresses are included in the data dump.  
*/

void 
x_dump_epiphany_workgroup (e_epiphany_t * workgroup,
                           unsigned int level_of_detail);

/* Output the contents of the registers of an ecore (note that registers
   that cannot be safely accessed while a core is running will be skipped
   if that is the case). 
   
   Level of detail indicates the number of 16-register blocks to display.
   DMA register values are only dumped at level of detail >= 1.
*/

void 
x_dump_ecore_registers (e_epiphany_t * workgroup,
                        int row, int column, unsigned int level_of_detail);

/* Write out the contents of an area of e-core address space (which can
   be either internal or external RAM). 

   There is potential for several dump modes, however the only ones
   presently supported are:
     PC relative
     
   No use is presently made of level-of-detail
*/

/* Memory dump modes */
#define X_PC_RELATIVE_DUMP_MODE (0)

void
x_dump_ecore_memory (e_epiphany_t * workgroup,
                     int row, int column, int mode, 
                     int start_offset, int end_offset,
                     unsigned int level_of_detail);

#endif /* _X_EPIPHANY_DIAGNOSTICS_H_ */
