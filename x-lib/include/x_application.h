/*
File: x_application.h

Copyright 2012 Mark Honman

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

#ifndef _X_APPLICATION_H_
#define _X_APPLICATION_H_

/* These functions provide information on the parallel application and
   permit it to be managed. Only a few of the informational functions
   are available to Epiphany code, and even there it is better to use
   the "task" calls which are less dependent on external memory access. */
   
#include "x_types.h"
#include "x_task_types.h"

typedef uint16_t x_application_state_t;
/* Application state values. 
   Values > X_VIRGIN_APPLICATION are at least partially active. */
#define X_NO_APPLICATION_DATA (-32767)
#define X_FAILED_APPLICATION      (-1)
#define X_SUCCESSFUL_APPLICATION   (0)
#define X_VIRGIN_APPLICATION       (1)
#define X_INITIALIZING_APPLICATION (2)
#define X_RUNNING_APPLICATION      (3)

/* Application statistics */

typedef struct {
        int virgin_tasks;
	int initializing_tasks;
        int active_tasks;
        int successful_tasks;
        int failed_tasks;
        int unused_cores;  // of both host and Epiphany chip
} x_application_statistics_t;

/*--------------------- Information functions -----------------------*/

x_return_stat_t x_get_task_coordinates (x_task_id_t task_id, 
                                        int * row, int * column);

int x_is_workgroup_task (x_task_id_t task_id);

int x_is_host_task (x_task_id_t task_id);

/* The caller can supply a NULL pointer - the address of an
   application_statistics_t structure is only needed if the caller
   wishes to make use of that information.   */
   
x_application_state_t x_get_application_state (x_application_statistics_t *statistics);

x_task_id_t x_workgroup_member_task (int row, int column);

/* Application management functions - only implemented for the host */

/*-------------------------- Initialisation --------------------------------*/

x_return_stat_t x_initialize_application (const char * caller_executable_file_name,
                                          int *workgroup_rows, int *workgroup_columns,
                                          int *host_task_slots);


/*----------------------- Task loading and assignment ----------------------*/

/* Special core ID modifiers for task creation - can be masked into core ID in
   Create Task operation
*/
#define X_CORE_MODIFIER_MASK     (0xF0000000)
#define X_SPECIFIED_CORE_ONLY    (0)
#define X_CORE_LEFT_OF           (0x10000000)
#define X_CORE_RIGHT_OF          (0x20000000)
#define X_CORE_ABOVE             (0x30000000)
#define X_CORE_BELOW             (0x40000000)
#define X_CORE_NEAREST           (0x80000000)
#define X_ANY_CORE               (0xF0000000)

x_task_id_t x_create_task (const char *executable_file_name, int core_id);

/* Many applications are SPMD meshes - x_mesh_application sets up the application for 
   this mode of operation, creating tasks (if not already present) for all workgroup
   members, and creating mesh connections with optional wraparound.

   The keys used to identify connections in the mesh are X_FROM_LEFT, X_TO_LEFT,
   X_FROM_ABOVE, X_TO_ABOVE etc. 
*/
#define X_WRAPAROUND_MESH   (0x00000001)

// Connection keys used in x_mesh_application 
#define X_FROM_LEFT         (0x10000000)
#define X_TO_LEFT           (0x20000000)
#define X_FROM_RIGHT        (0x30000000)
#define X_TO_RIGHT          (0x40000000)
#define X_FROM_ABOVE        (0x50000000)
#define X_TO_ABOVE          (0x60000000)
#define X_FROM_BELOW        (0x70000000)
#define X_TO_BELOW          (0x80000000)

x_return_stat_t x_prepare_mesh_application (
                                 const char *workgroup_executable_file_name,
                                 int mesh_options);

/*-------------------- Setup of inter-task communication -------------------*/

/* The keys used in making connections can be any integer value, but
   a core cannot have connections with duplicate keys. 
   Connections can only be made prior to tasks being started. 
*/

x_return_stat_t x_connect_tasks (x_task_id_t sender,   int sender_key, 
                                 x_task_id_t receiver, int receiver_key);

/*----------------------------- Task execution -----------------------------*/

x_return_stat_t x_launch_task (x_task_id_t task_id, ...);

/* This launches any tasks that have not yet been started, passing the same string arguments
   to all of these tasks. 
*/ 
x_return_stat_t x_launch_application (int argc, char *argv[]);

/*-------------------------- Shutdown and cleanup --------------------------*/

x_application_state_t x_finalize_application ();

#endif /* _X_APPLICATION_H_ */
