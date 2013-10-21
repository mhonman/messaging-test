/*
File: x_types.h

Copyright 2013 Mark Honman
Copyright 2012 Adapteva Inc (for code from e_types.h)

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

#ifndef _X_TYPES_H_
#define _X_TYPES_H_

#include <stdint.h>
#include <stddef.h>

/* the following is an almost exact cut-and-paste from e_types.h, needed 
   because the Adapteva host library does not define these types.  
   The only exception is that E_OK is replaced by X_SUCCESS because X_OK
   is defined in fcntl.h, and for consistency E_ERR is replaced by X_ERROR
   and E_WARN by X_WARNING.
*/

#ifdef __cplusplus
extern "C"
{
typedef enum {
        X_FALSE = false,
        X_TRUE  = true,
} x_bool_t;
#else
typedef enum {
        X_FALSE = 0,
        X_TRUE  = 1,
} x_bool_t;
#endif

typedef enum {
        X_SUCCESS =  0,
        X_ERROR   = -1,
        X_WARNING = -2,
} x_return_stat_t;

/* Memory offsets are used instead of pointers in shared DRAM data structures
   to minimise confusion between pointers in host and Epiphany address spaces.
*/

typedef ptrdiff_t x_memory_offset_t;

typedef uint16_t  x_transfer_size_t;

#endif /* _X_TYPES_H_ */
