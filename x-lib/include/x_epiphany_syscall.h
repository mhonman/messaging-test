/*
File: x_epiphany_syscall.h

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

/* Functionality to support the handling of Epiphany syscall traps.
 * Some of this is presently host-only but could be generalised to
 * the Epiphany environment if the need presents itself.
 */

#ifndef _X_EPIPHANY_SYSCALL_H_
#define _X_EPIPHANY_SYSCALL_H_

#include <stdint.h>
#include "x_epiphany_control.h"
#include "x_epiphany_exception.h"

/* System call structures defined using Epiphany packing rules */

/* The x_syscall_demo program can be used to confirm the
 * Epiphany-side sizes and offsets of these items. 
 */
   
typedef int16_t  e_dev_t;
typedef uint16_t e_ino_t;   
typedef uint32_t e_mode_t;
typedef uint16_t e_nlink_t;
typedef uint16_t e_uid_t;
typedef uint16_t e_gid_t;
typedef int32_t  e_off_t;
typedef int32_t  e_blksize_t;
typedef int32_t  e_blkcnt_t;
typedef int32_t  e_time_t;
typedef int32_t  e_suseconds_t;   

#pragma pack(4)
typedef struct e_timeval_t {
    e_time_t      tv_sec;   /* seconds */
    e_suseconds_t tv_usec;  /* microseconds */
} x_epiphany_timeval_t;

typedef struct e_timezone_t {
    int32_t tz_minuteswest; /* minutes west of Greenwich */
    int32_t tz_dsttime;     /* type of DST correction */
} x_epiphany_timezone_t;

/* Note that st_atime, st_mtime, and st_ctime cannot be used as
 * member names because in Linux they are #defined to st_atim.tv_sec etc.
 */

typedef struct e_stat_t {
    e_dev_t     e_st_dev;     /* ID of device containing file */
    e_ino_t     e_st_ino;     /* inode number */
    e_mode_t    e_st_mode;    /* protection */
    e_nlink_t   e_st_nlink;   /* number of hard links */
    e_uid_t     e_st_uid;     /* user ID of owner */
    e_gid_t     e_st_gid;     /* group ID of owner */
    e_dev_t     e_st_rdev;    /* device ID (if special file) */
    e_off_t     e_st_size;    /* total size, in bytes */
    e_time_t    e_st_atime;   /* time of last access */
    uint32_t    e_st_spare1;
    e_time_t    e_st_mtime;   /* time of last modification */
    uint32_t    e_st_spare2;
    e_time_t    e_st_ctime;   /* time of last status change */
    uint32_t    e_st_spare3;
    e_blksize_t e_st_blksize; /* blocksize for file system I/O */
    e_blkcnt_t  e_st_blocks;  /* number of 512B blocks allocated */
} x_epiphany_stat_t;
#pragma pack()

/* Syscall parameters structure - used in syscall handling routines.
 * It is an in-out parameter, the syscall handler modifies it in place. 
 * This structure is designed to be easily flattened for transmission 
 * to an unpriveliged handler process.
 */

#pragma pack(4)
typedef struct x_epiphany_syscall_parameters_t {
    uint32_t syscall_number;
    int32_t  r0;
    int32_t  r1;
    int32_t  r2;
    int32_t  r3;
    int32_t  data_size_1_in;
    char    *data_1_in;
    int32_t  data_size_2_in;
    char    *data_2_in;
    int32_t  result;
    int32_t  errno_out;
    uint32_t data_size_1_out;
    char    *data_1_out;
    uint32_t data_size_2_out;
    char    *data_2_out;
} x_epiphany_syscall_parameters_t;
#pragma pack()

/* -------------------------- Host-only -------------------------------*/
#ifndef __epiphany__
#include <e-hal.h>

/* Extract the parameters from Epiphany address space into an
 * Epiphany syscall parameters structure - addresses amongst the
 * parameters are converted into host-side addresses data_1_in etc.
 */

x_return_stat_t
x_get_epiphany_syscall_parameters (x_epiphany_control_t * epiphany_control, 
                                   int row, int col, 
                                   x_epiphany_syscall_parameters_t * parameters);

/* Return the results of a system call to Epiphany address space. 
 * If the output data blocks are not in the areas originally specified in
 * the system call trap, they are copied into the target areas.
 */

x_return_stat_t
x_put_epiphany_syscall_results (x_epiphany_control_t * epiphany_control, 
                                int row, int col, 
                                x_epiphany_syscall_parameters_t * parameters);

/* Extraction and return calls for parameters associated with the legacy
 * IO traps Write, Read, Open, and Close. These parameters are mapped to
 * the more generic system call parameters - an appropriate system call
 * number is included in the parameters.
 */

x_return_stat_t
x_get_epiphany_legacy_io_trap_parameters 
                               (x_epiphany_control_t * epiphany_control, 
                                int row, int col, uint32_t trap_number,
                                x_epiphany_syscall_parameters_t * parameters);

x_return_stat_t
x_put_epiphany_legacy_io_trap_results 
                               (x_epiphany_control_t * epiphany_control, 
                                int row, int col, uint32_t trap_number,
                                x_epiphany_syscall_parameters_t * parameters);

#endif /* __epiphany__ */

/*-------------------------- Host and e-core ----------------------------*/

/* Given a structure containing the parameters of an Epiphany system
 * call, perform the equivalent call on the host and update the structure
 * with the results. 
 */

x_return_stat_t
x_handle_epiphany_syscall (x_epiphany_syscall_parameters_t * parameters);

#endif /* _X_EPIPHANY_SYSCALL_H_ */


