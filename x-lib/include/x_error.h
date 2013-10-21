/*
File: x_error.h

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

#ifndef _X_ERROR_H_
#define _X_ERROR_H_

#include <x_types.h>

#define X_E_ERROR_CODES_START                  (-30000)

#define X_E_APPLICATION_NOT_INITIALISED        (-30000)
#define X_E_ENDPOINT_MODE_MISMATCH             (-30001)
#define X_E_ENDPOINT_NOT_INITIALISED           (-30002)
#define X_E_EPIPHANY_CANNOT_MANAGE_APPLICATION (-30003)
#define X_E_NULL_TASK_DESCRIPTOR               (-30004)
#define X_E_TASK_DESCRIPTOR_IN_USE             (-30005)
#define X_E_TASK_COORDINATES_OUTSIDE_WORKGROUP (-30006)
#define X_E_HOST_TASK_SLOTS_FULL               (-30007)
#define X_E_TASK_RESULT_OUT_OF_RANGE           (-30008)
#define X_E_INVALID_TASK_ID                    (-30009)

#define X_E_XTASK_INTERNAL_ERROR               (-30010)

#define X_E_GET_ENDPOINT_KEY_NOT_FOUND         (-30011)
#define X_E_SYNC_TRANSFER_MISMATCH             (-30012)
#define X_E_SEND_TOO_BIG_FOR_RECEIVE_BUFFER    (-30013)
#define X_E_INVALID_TRANSFER_SIZE              (-30014)

/* Report an error - the code should be one of the above X-lib error
   codes, or a user-selected negative value between -1 and -29999 
   inclusive.
   The return value is always X_ERROR. 
*/

x_return_stat_t x_error (int code, int int_info, void * address_info);

/* Supplies information on the last error reported via a call to
   x_error within the current task. 
   Returns 0 if no error has been reported since the task started.
*/

int x_last_error (int * int_info_p, void ** address_info_p); 

#endif /* _X_ERROR_H_ */
