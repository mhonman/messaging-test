/*
File: x_application_diagnostics.h

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

#ifndef _X_APPLICATION_DIAGNOSTICS_H_
#define _X_APPLICATION_DIAGNOSTICS_H_

#include "x_application_internals.h"

/* Host-side functions which dump out information from x-lib application 
   internal data structures. They are mainly useful for debugging x-lib 
   itself.
*/

/* Provide summary information from the header of the x-lib Application
   data structure. 
   
   At level of detail > 0, the task descriptor table and connection list
   printing routines are called, with a level of detail 1 less than 
   specified by the caller. 
*/

void 
x_dump_application_internals (unsigned int level_of_detail);

/* Dump out the contents of the Task Descriptor Table: unused descriptors 
   are only shown at levels of detail > 0 
*/

void 
x_dump_application_task_descriptor_table (unsigned int level_of_detail);

/* the connection list dump output is unaffected by level of detail 
*/

void 
x_dump_application_connection_list (unsigned int level_of_detail);

#endif /* X_APPLICATION_DIAGNOSTICS_H_ */
   