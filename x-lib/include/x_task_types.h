/*
File: x_task_types.h

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

#ifndef _X_TASK_TYPES_H_
#define _X_TASK_TYPES_H_

#include <stdint.h>


/* Task IDs start at zero (for consistency with e-lib and MPI).
   The special "null task" return value indicates that no task was found
   or an error occured in routines that return a task ID. 
*/

typedef int32_t	x_task_id_t;

#define X_NULL_TASK               (-1)

/* Task states are short int values - 
        any negative value represents a failure
	zero represents success
	positive values represent not-yet-finished conditions (informational)
	Error codes defined in x_error.h may also be returned - user programs
	should not use these codes for other purposes. 
*/

typedef int16_t		x_task_state_t;

#define X_FAILED_TASK            (-1)
#define X_SUCCESSFUL_TASK         (0)
#define X_VIRGIN_TASK             (1)
#define X_INITIALIZING_TASK       (2)
#define X_ACTIVE_TASK             (3)
#define X_SYNC_WAITING_TASK       (4)
#define X_MUTEX_WAITING_TASK      (5)
#define X_BARRIER_WAITING_TASK    (6)

/* Task heartbeat increments each time a status update is performed.
   The task implementation code can also frob the heartbeat via a call 
   to x_task_heartbeat.   
*/
typedef uint16_t	x_task_heartbeat_t;

#endif /* _X_TASK_TYPES_H_ */
