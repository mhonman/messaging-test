/*
File: x_task_diagnostics.c

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

/* This is optional task diagnostic functionality. All task functionality
   that requires string libraries is contained in this module.
   
   Core task/application functionality must be kept free of 
   dependencies (direct or indirect) on string libraries.
   
   The prototypes for these functions can be found in x_task.h.
*/

#include <stdio.h>
#include <stdarg.h>

#ifdef __epiphany__
#include <e_lib.h>
#include <e_coreid.h>
#endif

#include <x_error.h>
#include <x_address.h>

#include <x_task_types.h>
#include <x_task.h>
#include <x_application_internals.h>
#include <x_endpoint.h>


#define DO_TASK_HEARTBEAT { x_task_control.descriptor->heartbeat = ++x_task_control.heartbeat; }

/*
static x_task_control_t x_task_control;

void x_set_task_status (const char *format, ...)
{
        va_list args;
        va_start (args, format);
        vsnprintf (x_task_control.descriptor->status, 
                   sizeof(x_task_control.descriptor->status),
                   format, args); 
        DO_TASK_HEARTBEAT;
}

*/



