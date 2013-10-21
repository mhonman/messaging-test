/*
File: x_endpoint.h

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

#ifndef _X_ENDPOINT_H_
#define _X_ENDPOINT_H_

#include <x_types.h>

typedef void * x_endpoint_handle_t;

/* Get a handle to the local endpoint for the supplied connection key,
   for subsequent communication with the peer that has the other endpoint
   of that connection. 
*/

x_endpoint_handle_t x_get_endpoint (int key);

/* Returns TRUE if the peer has indicated its readiness to communicate. 
*/

x_bool_t x_endpoint_ready (x_endpoint_handle_t endpoint);

#endif /* _X_ENDPOINT_H_ */
