/*
File: x_lib_configuration.h

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

#ifndef _X_LIB_CONFIGURATION_H_
#define _X_LIB_CONFIGURATION_H_

/* Dimensions of unavoidably fixed-length x-lib data structures and
   other nasty constants.
   All collected in one place so that they can be tweaked together.
   Some of this is laziness because the values could be read from the
   Epiphany HDF file. 
   
   If constants related to the shared memory layout are changed, it
   is usually necessary to also change the LDF files in x-lib/bsps.   
*/

// Frequency of the Epiphany chip
#define X_EPIPHANY_FREQUENCY (600)

// Task management data structures
#define X_ESTIMATED_CORES (64)
#define X_APPLICATION_WORKING_MEMORY_SIZE (X_ESTIMATED_CORES*1024)

// Minimum message items for DMA transfers. Due to the effect of aligment
// on both DMA and non-DMA transfer speeds, it is the number of items
// rather than the transfer size in bytes that determines the cutoff point.
#define X_MESSAGING_MIN_DMA_ITEMS (152)

// NB! The following must match the HDF and LDF in use. 
// In fact the information can probably be obtained from the LDF
// The DRAM has a different base address in host physical, host process,
// and Epiphany core address space. 
// The XLIB_SHARED_DRAM section has the same offset from the appropriate 
// base address.. BUT the LDF must not place other information in that
// area. 
// Furthermore if the Epiphany program contains a data area mapped onto
// shared DRAM, this will be initialised when a core is loaded with that
// program (this initialisation is from the srec file) - and is NOT 
// desirable when the host has already been building shared data structures
// in that area.
// The XLIB area defined here is the second 8MB chunk of shared DRAM, i.e.
// the latter half of DRAM area 0. 
#define X_HOST_PROCESS_SHARED_DRAM_BASE  (0x00000000)
#define X_EPIPHANY_SHARED_DRAM_BASE      (0x8e000000)
#define X_LIB_SECTION_OFFSET             (0x00800000)

#endif /* _X_LIB_CONFIGURATION_H_ */
