/*
File: x_epiphany_control.h

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

#ifndef _X_EPIPHANY_CONTROL_H_
#define _X_EPIPHANY_CONTROL_H_

/* Host-side common interface to eSDK and Epiphany functionality.
 *
 *  The x_epiphany_control structure provides a single location
 *  where the esdk information needed by x-lib programs can be collected.
 *
 *   platform:  basic platform information as provided by e_get_platform_info
 *              - does *not* include the list of external memory segments. 
 *   workgroup: an e_dev_t structure populated by e_open (from this the 
 *              mapped addresses of Epiphany resources can be determined)
 *   external_memory_mappings: array of descriptors for shared memory 
 *              segments that have been mapped into process address space 
 *   num_external_memory_mappings: number of elements in the array
   
*/

#ifndef __epiphany__

#include <stdint.h>
#include <e-hal.h>

#include <x_types.h>

typedef struct {
	int              initialized;
	e_platform_t     platform;
	e_epiphany_t     workgroup;
	int              num_external_memory_mappings;
	e_mem_t          external_memory_mappings[];
} x_epiphany_control_t;

/*  Dynamically allocate an x_epiphany_control_t structure having enough space to
 *  accommodate mappings for all known external memory segments. 
 */

x_epiphany_control_t * 
x_alloc_epiphany_control();

/*  Map a workgroup on the attached Epiphany platform as well as mapping all known
 *  external memory segments. 
 *  In the case of x_map_epiphany_resources a workgroup of the desired size is opened
 *  and the platform information structure is populated with the results of 
 *  e_get_platform_info. 
 *  If all of the workgroup size specifications are zero, a maximally sized workgroup
 *  is opened. Otherwise the number of rows and columns is trimmed to fit the device. 
 *  If a NULL pointer is passed to x_map_epiphany_resources it will allocate a new
 *  epiphany control structure. 
 *
 *  x_unmap_epiphany_resources closes the workgroup and frees the external memory
 *  segments. 
*/

x_epiphany_control_t *
x_map_epiphany_resources (x_epiphany_control_t * epiphany_control, 
                          int first_row, int first_col,
						  int rows,      int cols);

x_return_stat_t 
x_unmap_epiphany_resources (x_epiphany_control_t * epiphany_control); 

/*  Use the mappings described in the Epiphany control structure to determine the
 *  host-side address corresponding to the specified Epiphany-side address as seen
 *  by the given core in the workgroup. 
 *
 *  Returns NULL if the Epiphany address is not mapped into host address space.
 *  Note that Epiphany-side NULL pointers also result in a return value of NULL. 
 */

void *
x_epiphany_to_host_address (x_epiphany_control_t * epiphany_control, 
                            int row, int col, uint32_t epiphany_address);
#endif

#endif /* _X_EPIPHANY_CONTROL_H_ */