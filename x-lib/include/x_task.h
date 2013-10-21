/*
File: x_task.h

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

#ifndef _X_TASK_H_
#define _X_TASK_H_

/* Routines and function prototypes needed by the implementors of
   x-lib tasks.
*/

#include <stdlib.h>
#include <x_task_types.h>

/* Prototype for the task_main function which must be provided by
   the user.
*/

int task_main (int argc, const char *argv[]);

/* These functions are for use within tasks.
   See x_task_management.h for task management functionality.
*/

/* Returns the task ID withing the application (zero based). */

x_task_id_t x_get_task_id ();

/* For Epiphany tasks, returns the dimensions of the workgroup and the
   calling task's corrdinates within it. 
   For host tasks, -1 is returned for both row and column coordinates. */
   
void x_get_task_environment (int *workgroup_rows, int *workgroup_columns,
                             int *my_row, int *my_column);

/* Formats the arguments using the supplied format string, writing the result to
   a host-accessible status buffer. */
   
void x_set_task_status (const char *format, ...);

/* Increments a simple "heartbeat" counter that is accessible to the host,
   as a crude sign of life on the core. The heartbeat is also updated by
   some x-lib functions. */
   
void x_task_heartbeat ();

#endif /* _X_TASK_H_ */


