/*
File: x_exchange.h

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

#ifndef _X_EXCHANGE_H_
#define _X_EXCHANGE_H_

/* An exchange is a collection of message passing operations, both
   sending and receiving of information. 
   Really useful, but NOT YET IMPLEMENTED! (October 2013). 
*/

#include <unistd.h>
#include <x_types.h>
#include <x_messaging_types.h>

typedef struct {
	x_endpoint_handle_t endpoint;
	void               *buffer;
	size_t              size;
} x_exchange_element_t;

typedef x_exchange_element_t *x_exchange_list_t;

/* Management of the exchange lists */

x_exchange_list_t x_new_exchange_list (unsigned max_entries);

void x_init_exchange_list (x_exchange_list_t exchange_list, unsigned max_entries);

x_return_stat_t x_add_exchange_element (x_exchange_list_t exchange_list, x_endpoint_handle_t endpoint, void * buffer, size_t size);

/* Synchronous and asynchronous data exchanges */

int x_sync_exchange (x_exchange_list_t exchange_list);

int x_background_exchange (x_exchange_list_t exchange_list);

x_bool_t x_exchange_done (x_exchange_list_t exchange_list);

#endif /* _X_EXCHANGE_H_ */
