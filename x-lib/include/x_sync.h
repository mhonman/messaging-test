/*
File: x_sync.h

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

#ifndef _X_SYNC_H_
#define _X_SYNC_H_

/* Synchronisation and synchronous message passing functions */

#include <x_types.h>
#include <x_endpoint.h>

/* Synchronise with the task having the other end of the connection.
   Returns X_SUCCESS unless there was a problem, in which case X_ERROR
   is returned. 
*/

x_return_stat_t x_sync (x_endpoint_handle_t endpoint);

/* Synchronise with an x_sync_receive call made by the peer, transferring
   data from the given buffer into the memory area supplied by the peer. 
   This call only returns on completion of the transfer.
   
   For optimum transfer speed ensure that buffers are doubleword-aligned
   (usually OK thanks to the e-gcc compiler) and in the case of transfers
   that could use DMA round transfer size up to the nearest doubleword. 
   
   The return value is the number of bytes transferred, or -1 on error. 
   The endpoint must be a sending endpoint!
*/

int x_sync_send (x_endpoint_handle_t endpoint, const void * buf, x_transfer_size_t size);

/* Receive data from the peer connected to the given receiving endpoint,
   waiting as long as is neccessary. 
   Transfer speed can be optimised by providing a doubleword aligned and
   sized buffer. 

   The return value is the number of bytes received, or -1 on error. 
*/

int x_sync_receive (x_endpoint_handle_t endpoint, void * buf, x_transfer_size_t size);

#endif /* _X_SYNC_H_ */
