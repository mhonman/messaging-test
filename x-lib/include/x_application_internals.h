/*
File: x_application_internals.h

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

/* Common definitions for implementation of the task management 
   functionality. Defines internal-use data structures.
   Not needed by functions that merely use the task management
   functionality.
   
   NB NB! these structures need to be exactly the same size in
   host and Epiphany programs!
   
   Default alignment for the ARM is 8 bytes, default alignment for
   Epiphany cores is 4 bytes. 
   This results in misinterpretation of data in memory shared between
   the two. 
   In order to prevent this, application, task, and connection data
   structures are explicitly packed on 4-byte boundaries. 
   This may have a performance impact on the ARM core.
   
   Note: arrays and structures within a structure will by default
   be aligned on 8-byte boundaries on ARM, even when the elements 
   within them are 4 bytes wide. 
*/

#ifndef _X_APPLICATION_INTERNALS_H_
#define _X_APPLICATION_INTERNALS_H_

#include "x_lib_configuration.h"
#include "x_types.h"
#include "x_task_types.h"
#include "x_connection_internals.h"

#pragma pack(4)

/* There are two elements to the task descriptor:
      the main descriptor in shared memory, and 
      an on-core data structure
   The basic principle is that only performance-sensitive data
   that is not needed by the host or other cores is stored on-core. 
   As far as possible the main descriptor does not contain pointers
   into the shared memory area - where pointers are present these
   are in the epiphany address space and fully qualified with core ID.  
   
   Potential issues:
       What happens when multiple Parallella boards are connected -
         is there still a global shared memory? If necessary sacrifice
	 a core to act as agent of the host?
	 
   The status buffer size is chosen so that the total structure size is
   a multiple of 8 bytes. 
*/

typedef struct {
	int                         coreid_or_pid;   
	x_memory_offset_t           executable_file_name;
	int                         num_arguments;
	x_memory_offset_t           arguments_offset;  
	int                         num_connections;
	x_memory_offset_t           connection_index;
	volatile x_task_state_t     state;     // any -ve value indicates failure.
	volatile x_task_heartbeat_t heartbeat; // copy of heartbeat from core mem
	char                        status[100];
} x_task_descriptor_t;

typedef struct {
	x_task_id_t           task_id;
	x_task_descriptor_t  *descriptor;
	x_task_heartbeat_t    heartbeat;
	int                   num_endpoints;
	x_endpoint_t         *endpoints;
} x_task_control_t;


/* Both #task descriptors and #connections are not known at compile time.
   With one task per core, an upper bound on the number of task 
   descriptors can be determined once the workgroup is created. 

   Yet space for these structures must be statically allocated in the
	core memory.

   The descriptor table is zero-indexed, starting with a descriptor per
   Ephiphany core, and ending with a descritor slot per host core. 

   The per-task connection list is only compiled at time of launching the
   task (otherwise there is going to be endless re-allocation of these
   lists and fixing-up of indexes into them).

   Note the offsets are offsets from the base of the x_application_t
   structure, not from the beginning of working memory. 
*/

typedef struct {
	unsigned int      workgroup_rows;
	unsigned int      workgroup_columns;
	unsigned int      host_task_slots;
	unsigned int      connection_list_length;
	x_memory_offset_t task_descriptor_table_offset;
	x_memory_offset_t connection_list_offset;
	x_memory_offset_t available_working_memory_start;
	x_memory_offset_t available_working_memory_end;
	char              working_memory[X_APPLICATION_WORKING_MEMORY_SIZE];
} x_application_t;	

/* Future:

   Need to provide for buffers that host tasks can use for communication. 
   Alternatively the semantics
   for host communication can be different - i.e. host always reads or writes
   instead of "sender writes" - then epiphany-accessible host buffers are
   not needed. 
   What then of host-host communication? Use Unix primitives?
*/


extern x_application_t *x_application;


#endif /* _X_APPLICATION_INTERNALS_H_ */
