/*
File: x_error.c

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

#include "x_types.h"
#include "x_error.h"

typedef struct {
        int   code;
        int   int_info;
        void *address_info;
} x_error_control_t;

static x_error_control_t x_error_control = 
                                    { code: 0, int_info: 0, address_info: NULL };

/* x_error

  Store the supplied error code and integer and address values for
  future reference, and return X_ERROR. 
  
  Fatal errors should probably cause a process halt.

  The values accompanying this call can be whatever the caller deems
  to be appropriate. 
*/

x_return_stat_t x_error (int code, int int_info, void *address_info)
{
        x_error_control.code         = code;
        x_error_control.int_info     = int_info;
        x_error_control.address_info = address_info;
        return X_ERROR;
}

/* x_last_error

   Retrieve the info started in the last call to x_error
*/

int x_last_error (int * int_info_p, void ** address_info_p)
{
        if (int_info_p) {
          *int_info_p = x_error_control.int_info;
        }
        if (address_info_p) {
          *address_info_p = x_error_control.address_info;
        }
        return x_error_control.code;
}

