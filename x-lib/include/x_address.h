/*
File: x_address.h

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

#ifndef _X_ADDRESS_H_
#define _X_ADDRESS_H_

/* NB as at 21/10/2013 these are just prototypes - the code has not been written yet */

/* These routines provide for conversion between host and epiphany global
   addresses. 
*/

void * x_host_to_epiphany_shared_memory_address (void * host_shared_memory_address);
void * x_epiphany_to_host_shared_memory_address (void * epiphany_shared_memory_address);

/* These routines are host-side only, because the Epiphany cores do not
   know where in host address space the core memory has been mapped.
*/

#ifndef __epiphany__
void * x_epiphany_core_memory_to_host_mapped_address (void * epiphany_address);
void * x_host_mapped_to_epiphany_core_memory_address (void * host_mapped_core_address);
#endif

#endif /* _X_ADDRESS_H_ */
