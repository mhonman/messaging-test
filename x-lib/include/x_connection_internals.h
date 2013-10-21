/*
File: x_connection_internals.h

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

/* Internal-use data structures for connection managment.
   Not needed by functions that merely use the connection management
   functionality.
*/

#ifndef _X_CONNECTION_INTERNALS_H_
#define _X_CONNECTION_INTERNALS_H_

#include "x_task_types.h"

typedef enum {
        X_UNINITIALISED_ENDPOINT = 0,
        X_SENDING_ENDPOINT       = 1,
        X_RECEIVING_ENDPOINT     = 2,
} x_endpoint_mode_t;

/* Note on "ready" status

   An endpoint is "ready" when the sequence number posted to the peer's
   endpoint (sequence_from_peer) is greater (modulo max unit32_t) than 
   the sequence number of the previous transaction between those endpoints.

   A simple synchronisation does not have a data transfer step, so
   the control value transferred is a special sync flag
   rather than the expected transfer size.
*/

typedef uint32_t x_transfer_address_t;
typedef uint32_t x_transfer_control_t;
typedef uint32_t x_transfer_sequence_t;

/* a special value of 0 indicates a sync operation - thus 0 is not a 
   valid length for data transfers. 
*/

#define X_ENDPOINT_SYNC_CONTROL   ((x_transfer_control_t)0)

/* Note that the optimal form of this structure uses 32-bit values
   for the sequence, control, and address information.
   The use of 16-bit values (packed or unpacked) significantly slows
   the sync and transfer code. 
*/

typedef struct x_endpoint_struct {
	volatile x_transfer_sequence_t sequence_from_peer;
	volatile x_transfer_control_t  control_from_peer;
	volatile x_transfer_address_t  address_from_peer;
        x_transfer_sequence_t          sequence;
        x_endpoint_mode_t              mode;
	uint16_t                       connection_id;
        struct x_endpoint_struct      *remote_endpoint;    
} x_endpoint_t;

typedef struct {
	x_task_id_t     source_task;
	int             source_key;
	x_task_id_t     sink_task;
	int             sink_key;
	x_endpoint_t   *source_endpoint;
	x_endpoint_t   *sink_endpoint;
} x_connection_t;

/* x_global_address_local_coreid_bits

   Aargh! what a name! But a rather useful value, the local coreid already
   shifted into the top 12 bits of a 32-bit word. 

   This should be initialised at task startup and then left unchanged!

   The mask X_GLOBAL_ADDRESS_COREID_MASK can be used to select just that 
   part of an address and test whether there is anything present. 

   However it is slightly faster to shift the address right by 20 bits.

   The implementation of this can be improved, as the value is presently
   (Oct 2013) initialised by the task initialisation code. 
*/

#define X_GLOBAL_ADDRESS_COREID_MASK (0xFFF00000)

extern x_transfer_address_t x_global_address_local_coreid_bits;

#endif /* _X_CONNECTION_INTERNALS_H_ */
