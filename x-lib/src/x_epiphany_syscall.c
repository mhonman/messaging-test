/*
File: x_epiphany_syscall.c

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

/* Routines to support the fulfilment of Epiphany system calls in the
 * host O/S environment - matching the Epiphany-side functionality of
 * libgloss/epiphany/epiphany-syscalls.c
 *
 * NB! presumes that the host is little-endian like the e-cores.
 *
 * These routines are written with the following typical system-call 
 * handling flow in mind:
 *    Detect a HALT state and extract the trap number
 *    Gather the trap parameters into a local data area
 *      If a system-call trap is going to be handled in-process,
 *      it is possible to read or write blocks of data directly to
 *      the Epiphany core memory or external mapped RAM - otherwise
 *      copy the contents of data buffers associated with the call
 *      from Epiphany address space into local buffers. 
 *    Call a handler routine appropriate to the type of trap, that
 *      performs the associated function in the host environment,
 *      converting between Epiphany and host data formats as needed.
 *    Return the results of the operation into Epiphany address space.
 *    Adjust the e-core program counter and resume execution of the
 *      core program.
 *
 * The interface to routines in this module tends to use packaged
 * data because it is envisaged that the host-side implementation of
 * functions called via the trap mechanism could be done by an
 * separate process that does not have administrative privileges.
 *
 * Provision is made for handling of legacy IO traps by mapping them
 * to more generic system call equivalents. 
 * 
 * Many of the parameters of the get and put functions provide 
 * information needed for conversion between Epiphany and host 
 * addresses. It would be good to encapsulate all of this information
 * in a structure of some sort... expect the function signatures
 * to change in future. 
 *
 *--------------------------------------------------------------------
 * Notes on the full range of Epiphany system calls as implemented in
 * the Epiphany newlib - as at November 2013.
 * 
 * Ref: epiphany-newlib/libgloss/epiphany
 *
 *    environ is a single NULL pointer
 *    __exit
 *        -> Trap 3
 *    _exit
 *        -> calls asm_exit
 *    access(filename, flags) 
 *        -> calls stat
 *    _close(fd) 
 *        -> calls asm_close(fd)
 *    _execve (filename, argv[], envp[]) 
 *        -> returns -1, errno = ENOSYS  
 *    _fork () 
 *        -> returns -1, errno = ENOSYS
 *    _fstat (fd, struct stat * st) 
 *        -> asm_syscall (fd, st, NULL, SYS_fstat)
 *    _getpid () 
 *        -> always returns 1. Suggestion: make it return coreid. 
 *    _gettimeofday (struct timeval *tp, void tzp) 
 *        -> calls asm_syscall (tp, tzp, NULL, SYS_gettimeofday)
 *    _isatty (fd) 
 *        -> returns TRUE for fd 0,1,2. 
 *           Could create a trap that triggers a host-side call.  
 *    _kill (pid, sig) 
 *        -> returns -1 and ENOSYS. 
 *           Could implement as sending a signal to another core. 
 *    _link (char * old, char * new) 
 *        -> calls asm_syscall (old, new, NULL, SYS_link)
 *    _lseek (fd, offset, whence) 
 *        -> calls asm_syscall (fd, offset, whence, SYS_lseek)
 *    _open (filename, flags, mode) 
 *        -> calls asm_syscall (file, flags, mode, SYS_open)
 *    _read (fd, ptr, len) 
 *        -> calls asm_syscall (fd, ptr, len, SYS_read)
 *    _stat (filename, struct stat * st) 
 *        -> calls asm_syscall(filename, st, NULL, SYS_stat)
 *    _times (struct tms *buf) 
 *        -> returns -1 and ENOSYS. 
 *    _unlink (name) 
 *        -> calls asm_syscall (name, NULL, NULL, SYS_unlink)
 *    _wait (int * status)  
 *        -> returns -1 and ENOSYS.
 *    _write (fd, ptr, len) 
 *        -> calls asm_syscall (fd, ptr, len, SYS_write)
 *
 * epiphany_syscalls.c contains the routines that implement the system call
 * TRAP interface - results are returned in R0, errnos in R3
 *   asm_write (CHAN, ADDR, LEN) - Trap 0. 
 *   asm_read (CHAN, ADDR, LEN) - Trap 1
 *   asm_open (FILEname -> R0, FLAGS -> R1, MODE (ignored)) - Trap 2 
 *   asm_exit (status -> R0) - Trap 3
 *   asm_close (CHAN) - Trap 6
 *   asm_syscall (P1 -> R0, P2 -> R1, P3 -> R2, SUBFUN -> R3) - Trap 7
 *
 *--------------------------------------------------------------------
 * Other notes and caveats:
 *
 * On Linux hosts, struct stat has been hacked, replacing the st_atime
 *   st_mtime, and st_ctime values with struct timevals named st_atim,
 *   st_mtim, and c_tim. st_atime etc. are then #defined to st_atim.tv_sec
 *   etc.
 *   Thus st_atime cannot be used as a member name in the x_epiphany_stat_t
 *   structure.
 *
 * Requests to close fds 0, 1, or 2 are silently ignored.  
 *
 * The system call numbers used in conjunction with the trap interface 
 * are defined in libgloss - its syscall.h must be used, NOT the 
 * syscall.h of the host operating system.  
 */

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/time.h>
#include "libgloss/syscall.h"
#include "x_types.h"
#include "x_epiphany_control.h"
#include "x_epiphany_internals.h"
#include "x_epiphany_exception.h"
#include "x_epiphany_syscall.h"

#define INTERNAL_READ_BUFFER_SIZE (16384)

/*====================== INTERNAL ROUTINES =========================*/

/* x_set_stat_result, x_set_timeofday_result
 *
 * Internal routines to set results of stat-family and gettimeofday calls
 * in a syscall parameters structure. 
 *
 * The structures are packed according to Epiphany rather than host rules,
 * hence the need for these helper functions. If space for the Epiphany-
 * equivalent is not provided by the caller, it is supplied by these
 * routines in the form of internal static variables which will be 
 * overwritten on the next call. 
 *
 * The structure definitions are based on Linux manpages.
 *
 * The packing of the structures in Epiphany-land is empirically determined
 * with the help of code that outputs the size and offset of each element in
 * the structure, and informed by epiphany-elf/sys-includes/machine/type.h 
 * and sys_includes/sys/_types.h
 */

static void
x_set_stat_result (x_epiphany_syscall_parameters_t * parameters, 
                   const struct stat * local_stat)
{
    static x_epiphany_stat_t static_epiphany_stat;
    static x_epiphany_stat_t * epiphany_stat;
    parameters->data_size_1_out = sizeof(static_epiphany_stat);
    if (parameters->data_1_out == NULL) {
        parameters->data_1_out = (char*)(&static_epiphany_stat);
    }
    epiphany_stat = (x_epiphany_stat_t*)(parameters->data_1_out);
    epiphany_stat->e_st_dev     = local_stat->st_dev;
    epiphany_stat->e_st_ino     = local_stat->st_ino;    
    epiphany_stat->e_st_mode    = local_stat->st_mode;   
    epiphany_stat->e_st_nlink   = local_stat->st_nlink;   
    epiphany_stat->e_st_uid     = local_stat->st_uid;     
    epiphany_stat->e_st_gid     = local_stat->st_gid;     
    epiphany_stat->e_st_rdev    = local_stat->st_rdev;    
    epiphany_stat->e_st_size    = local_stat->st_size;    
    epiphany_stat->e_st_blksize = local_stat->st_blksize; 
    epiphany_stat->e_st_blocks  = local_stat->st_blksize;  
    epiphany_stat->e_st_atime   = local_stat->st_atime;   
    epiphany_stat->e_st_mtime   = local_stat->st_mtime;   
    epiphany_stat->e_st_ctime   = local_stat->st_ctime; 
}

static void
x_set_timeofday_result (x_epiphany_syscall_parameters_t * parameters,
                        const struct timeval   * local_tv,
                        const struct timezone  * local_tz)
{
    static x_epiphany_timeval_t  static_epiphany_tv, *epiphany_tv;
    static x_epiphany_timezone_t static_epiphany_tz, *epiphany_tz;
    parameters->data_size_1_out = 0;
    parameters->data_size_2_out = 0;
    if (local_tv != NULL) {
        parameters->data_size_1_out = sizeof(static_epiphany_tv);
        if (parameters->data_1_out == NULL) {
            parameters->data_1_out = (char*)(&static_epiphany_tv);
        }
        epiphany_tv = (x_epiphany_timeval_t*)(parameters->data_1_out);
        epiphany_tv->tv_sec  = local_tv->tv_sec;
        epiphany_tv->tv_usec = local_tv->tv_usec;
        if (local_tz != NULL) {
            parameters->data_size_2_out = sizeof(static_epiphany_tz);
            if (parameters->data_2_out == NULL) {
                parameters->data_2_out = (char*)(&static_epiphany_tz);
            }
            epiphany_tz = (x_epiphany_timezone_t*)(parameters->data_2_out);
            epiphany_tz->tz_minuteswest = local_tz->tz_minuteswest;
            epiphany_tz->tz_dsttime     = local_tz->tz_dsttime;
        }
    }
}

/*========================= PUBLIC FUNCTIONS =========================*/

/*--------------------------- Host-only ------------------------------*/

/*  x_get_epiphany_syscall_parameters
 *
 *  Assuming that the core at (row,col) in the workgroup has been detected
 *  as having halted for a system-call trap, gathers the parameters from
 *  its registers and address space, and populates the system call
 *  parameters structure with them. 
 *  
 *  NB where there is a data buffer supplied in the call (e.g. write()), 
 *  its address is converted into host address space - the host can either 
 *  copy the data to a working memory area, or directly access the 
 *  Epiphany-side buffer.
 *
 *  Since the nature and location of these buffers is dependent on the
 *  system call that was made, this routine needs to have a data-gathering
 *  branch for each type of call.
 *
 *  It also needs a full list of shared memory segment mappings because in
 *  many cases the buffers will be located in shared memory.
 *
 *  Returns X_SUCCESS on successful completion, or X_ERROR if an error
 *  was encountered. 
 *
 *  Notes: 
 *    1. the output fields of the syscall parameters structure are
 *       initialised to "empty" values.
 *    2. All parameters are received in r0 through r3, the syscall number
 *       itself is found in r3. 
 */

#ifndef __epiphany__

x_return_stat_t
x_get_epiphany_syscall_parameters (x_epiphany_control_t * epiphany_control, 
                                   int row, int col, 
                                   x_epiphany_syscall_parameters_t * parameters)
{
    x_return_stat_t result = X_SUCCESS;
    
    if ((row < 0) || (row > epiphany_control->workgroup.rows) ||
        (col < 0) || (col > epiphany_control->workgroup.cols)) {
        printf ("%s: coordinates (row,col) = (%d,%d) are outside the workgroup\n",
                "x_get_epiphany_syscall_parameters", row, col);
        result = X_ERROR;    
    }
    else {        
        x_epiphany_core_registers_t *regs = 
            (x_epiphany_core_registers_t*)
                (epiphany_control->workgroup.core[row][col].regs.base);
        
        parameters->r0              = regs->r0;
        parameters->r1              = regs->r1;
        parameters->r2              = regs->r2;
        parameters->r3              = regs->r3;
        parameters->syscall_number  = parameters->r3;
        parameters->data_size_1_in  = 0;
        parameters->data_1_in       = NULL;
        parameters->data_size_2_in  = 0;
        parameters->data_2_in       = NULL;
        parameters->result          = 0;
        parameters->errno_out       = 0;
        parameters->data_size_1_out = 0;
        parameters->data_1_out      = NULL;
        parameters->data_size_2_out = 0;
        parameters->data_2_out      = NULL;
        
        // Data buffer linkage
        switch (parameters->syscall_number) {
            
            // read() has the address of the buffer in R1, length in R2
            case SYS_read:
                parameters->data_1_out = x_epiphany_to_host_address 
                               (epiphany_control, row, col, parameters->r1);
                parameters->data_size_1_out = parameters->r2;
                break;
            
            // write() has the address of the buffer in R1, length in R2
            case SYS_write:
                parameters->data_1_in = x_epiphany_to_host_address 
                                (epiphany_control, row, col, parameters->r1);
                parameters->data_size_1_in = parameters->r2;
                break;
            
            // stat() is like fstat() except that it takes a filename in R0
            case SYS_stat:  
                parameters->data_1_in = x_epiphany_to_host_address
                                (epiphany_control, row, col, parameters->r0);
                parameters->data_size_1_in = strlen ((char*)(parameters->r0));
            case SYS_fstat:
                parameters->data_1_out = x_epiphany_to_host_address 
                                (epiphany_control, row, col, parameters->r1);
                parameters->data_size_1_out = sizeof(x_epiphany_stat_t);
                break;
            
            // open(), link(), and unlink() take a filename as first parameter,
            // link() has an additional second filename. 
            case SYS_link:
                parameters->data_2_in = x_epiphany_to_host_address 
                                (epiphany_control, row, col, parameters->r1);
                parameters->data_size_2_in = strlen ((char*)(parameters->r1));
            case SYS_unlink:
            case SYS_open:
                parameters->data_1_in = x_epiphany_to_host_address 
                                (epiphany_control, row, col, parameters->r0);
                parameters->data_size_1_in = strlen ((char*)(parameters->r0));
                break;
            
            // gettimeofday() has two output parameters, timeval and timezone
            // timezone may be NULL if the caller does not require that info.
            case SYS_gettimeofday:
                parameters->data_1_out = x_epiphany_to_host_address 
                                (epiphany_control, row, col, parameters->r0);
                parameters->data_size_1_out = sizeof(x_epiphany_timeval_t);
                if (parameters->r1 == 0) {
                    parameters->data_2_out = NULL;
                    parameters->data_size_2_out = 0;
                }
                else {
                    parameters->data_2_out = x_epiphany_to_host_address 
                                                (epiphany_control, row, col, 
                                                 parameters->r1);
                    parameters->data_size_2_out = sizeof(x_epiphany_timezone_t);
                }    
                break;

            // Other system call numbers cannot be handled
            default:
                fprintf (stderr, 
                         "%s: cannot handle system call %d from core (%d,%d)\n",
                         "x_get_epiphany_syscall_parameters",
                         parameters->syscall_number,
                         row, col);
                result = X_ERROR;
        } 
    }
    return result;
}
#endif /* not __epiphany__ */

/* x_put_epiphany_syscall_results
 *
 * Given a system call parameters structure that contains the results
 * of a system call performed on behalf of an Epiphany program, returns
 * those results to Epiphany address space.
 *
 * Normally returns X_SUCCESS, will return X_ERROR if problems are detected.
 *
 * Data buffer linkage: If the output buffer addresses in the 
 * parameters structure match the original addresses supplied by
 * the Epiphany core program, the syscall handler will have already
 * placed the results in those areas - if not, the results need to
 * be copied into Epiphany address space (assuming the call was successful!).
 *
 * stat() and fstat() return a struct stat - address originally in R1.
 * Similarly the address of the read() buffer is in R1. 
 *
 * gettimeofday() has two output parameters, timeval and timezone -
 *    addresses in R0 and R1. 
 *    timezone may be NULL if the caller does not require that info.
 *
 * Note that using memcpy to memory on the Epiphany chip is unpleasantly
 * inefficient - but due to the limited size of that memory it is 
 * probably a safe assumption that buffers are in external RAM. 
 *
 * See program heading comments for notes on register usage. 
 */

#ifndef __epiphany__

x_return_stat_t
x_put_epiphany_syscall_results (x_epiphany_control_t * epiphany_control, 
                                int row, int col, 
                                x_epiphany_syscall_parameters_t * parameters)
{
    x_return_stat_t   result = X_SUCCESS;
    char            * output_address;
    
    if ((row < 0) || (row > epiphany_control->workgroup.rows) ||
        (col < 0) || (col > epiphany_control->workgroup.cols)) {
        printf ("%s: coordinates (row,col) = (%d,%d) are outside the workgroup\n",
                "x_put_epiphany_syscall_results", row, col);
        result = X_ERROR;    
    }
    else {        
        x_epiphany_core_registers_t *regs = 
            (x_epiphany_core_registers_t*)
                (epiphany_control->workgroup.core[row][col].regs.base);
        
        if (parameters->result >= 0) {
            if ((parameters->syscall_number == SYS_read) ||
                (parameters->syscall_number == SYS_stat) ||
                (parameters->syscall_number == SYS_fstat)) {
                output_address = 
                    x_epiphany_to_host_address (epiphany_control, row, col, 
                                                regs->r1);
                if (parameters->data_1_out != output_address) {
                    memcpy(output_address, parameters->data_1_out,
                           parameters->data_size_1_out);
                }
            }
            else if (parameters->syscall_number == SYS_gettimeofday) {
                output_address = 
                    x_epiphany_to_host_address (epiphany_control, row, col, 
                                                regs->r0);
                if (parameters->data_1_out != output_address) {
                    memcpy(output_address, parameters->data_1_out,
                           parameters->data_size_1_out);
                }    
                output_address = 
                    x_epiphany_to_host_address (epiphany_control, row, col, 
                                                regs->r1);
                if ((output_address != NULL) &&
                    (parameters->data_2_out != output_address)) {
                    memcpy(output_address, parameters->data_2_out,
                           parameters->data_size_2_out);
                }
            }
        }
        
        regs->r0 = parameters->result;
        regs->r3 = parameters->errno_out;
        
    }    
    return result;    
}
#endif /* not __epiphany__ */

#ifndef __epiphany__

/* x_get_epiphany_legacy_io_trap_parameters
 *
 * Almost all of the legacy IO traps' parameters map directly to 
 * those of equivalent system calls. The main difference is that
 * the Open trap does not include a file creation mode (i.e. permissions), 
 * and this routine thus supplies the normal rwx modes for User, Group, and
 * Other.  
 */

x_return_stat_t
x_get_epiphany_legacy_io_trap_parameters 
                               (x_epiphany_control_t * epiphany_control, 
                                int row, int col, uint32_t trap_number,
                                x_epiphany_syscall_parameters_t * parameters)
{
    x_return_stat_t result = X_SUCCESS;
    
    if ((row < 0) || (row > epiphany_control->workgroup.rows) ||
        (col < 0) || (col > epiphany_control->workgroup.cols)) {
        printf ("%s: coordinates (row,col) = (%d,%d) are outside the workgroup\n",
                "x_get_epiphany_legacy_io_trap_parameters", row, col);
        result = X_ERROR;    
    }
    else {        
        x_epiphany_core_registers_t *regs = 
            (x_epiphany_core_registers_t*)
                (epiphany_control->workgroup.core[row][col].regs.base);
        
        parameters->r0              = regs->r0;
        parameters->r1              = regs->r1;
        parameters->r2              = regs->r2;
        parameters->r3              = regs->r3;
        parameters->data_size_1_in  = 0;
        parameters->data_1_in       = NULL;
        parameters->data_size_2_in  = 0;
        parameters->data_2_in       = NULL;
        parameters->result          = 0;
        parameters->errno_out       = 0;
        parameters->data_size_1_out = 0;
        parameters->data_1_out      = NULL;
        parameters->data_size_2_out = 0;
        parameters->data_2_out      = NULL;

        if (trap_number == X_WRITE_TRAP) {
            // Write has the address of the buffer in R1, length in R2
            // just like SYS_write
            parameters->syscall_number = SYS_write;
            parameters->data_1_in = x_epiphany_to_host_address 
                                (epiphany_control, row, col, parameters->r1);
            parameters->data_size_1_in = parameters->r2;
        }
        else if (trap_number == X_READ_TRAP) {        
            // Read has address of buffer in R1, length in R2 - like SYS_read
            parameters->syscall_number = SYS_read;
            parameters->data_1_out = x_epiphany_to_host_address 
                                (epiphany_control, row, col, parameters->r1);
            parameters->data_size_1_out = parameters->r2;
        }
        else if (trap_number == X_OPEN_TRAP) { 
            // Open and SYS_open takes a filename as first parameter.
            // It is necessary to fake the permissions for newly created files
            // (this parameter is ignored by open(2) unless O_CREAT is specified
            // in the flags). 
            parameters->syscall_number = SYS_open;
            parameters->r2 = S_IRWXU | S_IRWXG | S_IRWXO;
            parameters->data_1_in = x_epiphany_to_host_address 
                                (epiphany_control, row, col, parameters->r0);
            parameters->data_size_1_in = strlen ((char*)(parameters->r0));
        }
        else if (trap_number == X_CLOSE_TRAP) {
            // Close is just as simple either way
            parameters->syscall_number = SYS_close;
        }
        else {
            // Other trap numbers cannot be handled
            fprintf (stderr, 
                     "%s: cannot handle legacy IO trap %d from core (%d,%d)\n",
                     "x_get_epiphany_legacy_io_trap_parameters",
                     trap_number, row, col);
            result = X_ERROR;
        }
    }
    return result;
}
#endif /* not __epiphany__ */

/* x_put_epiphany_legacy_io_trap_results
 *
 * This is easy for once, as the return values of the legacy Write, Read,
 * Open, and Close trap are returned to the same locations as their syscall
 * equivalents, so if sanity checks are passed, the real work can be
 * done by a call to x_put_epiphany_syscall_results. 
 * The trap number parameter is optional - if greater than zero it is used
 * to check that the system call type matches the legacy trap type. 
 */

#ifndef __epiphany__

x_return_stat_t
x_put_epiphany_legacy_io_trap_results 
                               (x_epiphany_control_t * epiphany_control, 
                                int row, int col, uint32_t trap_number,
                                x_epiphany_syscall_parameters_t * parameters)
{
    x_return_stat_t result = X_ERROR;
    if (trap_number > 0) {
        if (((trap_number == X_WRITE_TRAP) && 
             (parameters->syscall_number != SYS_write)) ||
            ((trap_number == X_READ_TRAP) && 
             (parameters->syscall_number != SYS_read)) ||
            ((trap_number == X_OPEN_TRAP) && 
             (parameters->syscall_number != SYS_open)) ||
            ((trap_number == X_CLOSE_TRAP) && 
             (parameters->syscall_number != SYS_close))) {
            fprintf (stderr, 
                     "%s: cannot use results of system call %d for trap %d\n",
                     "x_put_epiphany_legacy_io_trap_results",
                     parameters->syscall_number, trap_number);
        }
        else if ((trap_number != X_WRITE_TRAP) &&
                 (trap_number != X_READ_TRAP)  &&
                 (trap_number != X_OPEN_TRAP)  &&
                 (trap_number != X_CLOSE_TRAP)) {
            fprintf (stderr, "%s: no suport for trap %d\n",
                     "x_put_epiphany_legacy_io_trap_results", trap_number);
        }
    }
    if ((parameters->syscall_number == SYS_write) ||    
        (parameters->syscall_number == SYS_read)  ||
        (parameters->syscall_number == SYS_open)  ||
        (parameters->syscall_number == SYS_close)) {
        result = x_put_epiphany_syscall_results (epiphany_control, 
                                                 row, col, parameters);
    }
    else {
        fprintf (stderr, "%s: system call %d has no legacy IO equivalent\n",
                 "x_put_epiphany_legacy_io_trap_results", 
                 parameters->syscall_number);
    }
    return result;
}
#endif /* not __epiphany__ */


/*------------------------- Host and Epiphany -------------------------*/

/* x_handle_epiphany_syscall
 *
 * Perform the operation requested in an Epiphany system call.
 * By its nature this routine is utterly coupled to the newlib syscall 
 * interface implemented for the Epiphany esdk. 
 *
 * The parameters are received in an x_epiphany_syscall_parameters_t  
 * structure - the result, errno, data_size_out, and data_out members are 
 * updated with the results of the call. 
 * 
 * The return value is X_SUCCESS if the system call could not be handled,
 * in which case X_ERROR is returned. If the system call fails this is not
 * considered to be an error condition - the error resulting from the 
 * system call is reported in the system call parameters structure. 
 *
 * If the output data pointers are non-NULL, results are copied into those
 * areas (assumed to be big enough for the structure(s) or data block 
 * returned by the system call in question). 
 *
 * If output data areas are not pre-created, this routine (or internal 
 * subroutines in this module) place output data in static data areas that 
 * will be overwritten on the next call.
 *
 * Note that pointers in the syscall parameters are not interpreted in any  
 * way. It would have been neat to make this more general, but that will be  
 * left for anyone who might want to re-use the code on a different 
 * architecture!
 *
 * Implementation notes:
 *   Epiphany doc claims that open just takes the filename in R0, but the 
 *     open(2) manpage indicates additional flags and/or mode parameters. 
 *     The libgloss implementation puts mode and flags in R1 and R2, so 
 *     this code follows its lead. 
 *   If an internal read buffer must be used, the read size is limited to
 *     the size of that buffer. 
 *   close operations on fds 0,1, and 2 are faked. 
 */

x_return_stat_t
x_handle_epiphany_syscall (x_epiphany_syscall_parameters_t * parameters)
{
    x_return_stat_t result = X_SUCCESS;
    int             call_result;
    struct stat     file_stat;
    struct timeval  current_time;
    struct timezone current_timezone;
    int             read_size;
    char            static_read_buffer[INTERNAL_READ_BUFFER_SIZE];    
    
    parameters->errno_out       = 0;
    parameters->data_size_1_out = 0;
    parameters->data_size_2_out = 0;
    
    switch (parameters->syscall_number) {
        case SYS_open:
            call_result = open (parameters->data_1_in, parameters->r1, 
                                parameters->r2);
            break;
        case SYS_close:
            if (parameters->r0 > 2) {
                call_result = close (parameters->r0);
            }
            else { /* fake close */
                call_result = 0;
            }    
            break;
        case SYS_read:
            read_size = parameters->r2;
            if (parameters->data_1_out == NULL) {
                parameters->data_1_out = static_read_buffer;
                if (read_size > sizeof(static_read_buffer)) {
                    read_size = sizeof(static_read_buffer);
                }
            } 
            call_result = read (parameters->r0, parameters->data_1_out, 
                                read_size);
            if (result >= 0) {
                parameters->data_size_1_out = result;
            }
            break;
        case SYS_write:
            call_result = write (parameters->r0, parameters->data_1_in, 
                                 parameters->r2);
            break;
        case SYS_lseek:
            call_result = lseek (parameters->r0, parameters->r1, 
                                 parameters->r2);
        case SYS_unlink:
            call_result = unlink (parameters->data_1_in);
        case SYS_fstat:
            call_result = fstat (parameters->r0, &file_stat);
            if (call_result >= 0) {
                x_set_stat_result (parameters, &file_stat);
            }    
            break;
        case SYS_stat:
            call_result = stat (parameters->data_1_in, &file_stat);
            if (result >= 0) {
                x_set_stat_result (parameters, &file_stat);
            }    
            break;
        case SYS_link:
            call_result = link (parameters->data_1_in, parameters->data_2_in);
            break;
        case SYS_gettimeofday:
            call_result = gettimeofday (&current_time, &current_timezone);
            if (result >= 0) {
                x_set_timeofday_result (parameters, &current_time, 
                                        &current_timezone);
            }
            break;
        default:
            fprintf (stderr, "Cannot handle Epiphany syscall %d!\n",
                     parameters->syscall_number);
            result = X_ERROR;
    }
    if (call_result < 0) {
        parameters->errno_out = errno;
    }
    parameters->result = call_result;
    return result;
}
