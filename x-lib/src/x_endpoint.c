/*
File: x_endpoint.c

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

#include <unistd.h>
#include "x_error.h"
#include "x_endpoint.h"
#include "x_application_internals.h"
#include "x_connection_internals.h"

/* x_endpoint_ready

  Returns TRUE if the peer has indicated its readiness to communicate.

  Rollover case needs to be tested!
*/

x_bool_t x_endpoint_ready (x_endpoint_handle_t endpoint)
{
  x_endpoint_t *local_endpoint = (x_endpoint_t*)endpoint;
  return (local_endpoint->sequence_from_peer == (local_endpoint->sequence + 1));
}

