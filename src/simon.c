/*
File: simon.c

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

/* ISSUES
 *  1. If an Epiphany program is launched while simon is polling, the 
 *     zedboard crashes. Is this because simon gets bad addresses,
 *     or some contention for Epiphany resources? 
 *     Most likely Epiphany becomes unavailable during reset (duration
 *     up to 0.2s and for E16G301 includes link speed reprogramming)?
 *     See also note in the eSDK documentation for e_reset_core()
 */
 
/* This is the Simple Io Monitor for Epiphany. 
 *
 * This program polls the Epiphany chip to detect cores that have
 * halted on a TRAP instruction, and if it is a system-call trap
 * the call parameters are retrieved from the e-core address space
 * and translated into a local system call. 
 *
 * When run without any parameters, the program polls every core
 * each 10 microseconds. 
 *
 * Alternatively a row and column can be specified on the command
 * line, in which case just that core is polled. 
 *
 * After a trap has been handled, the next 10 polls are on a 
 * 1-microsecond interval on the assumption that I/O operations
 * are gregarious and that the operation is part of a series.  
 *
 * Since the program interfaces directly with the Epiphany chip
 * it must run with administrative rights, and file operations
 * are performed with those priveliges.
 *
 * Although this program uses some x-lib modules to interface to
 * the Epiphany chip and handle the traps, it can be used with any
 * Epiphany program.
 */
 
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <endian.h>

#include <e-hal.h>

#include "x_types.h"
#include "x_epiphany_control.h"
#include "x_epiphany_internals.h"
#include "x_epiphany_exception.h"
#include "x_epiphany_syscall.h"
#include "x_epiphany_diagnostics.h"

#define POLL_MICROSECONDS  (10)
#define REPOLL_MICROSECONDS (2)
#define REPOLL_COUNT        (5)

static char * program_name = "simon";

/* process_syscall_trap
 *
 * Internal routine to retrieve the system call parameters from Epiphany
 * memory (internal and/or external), get the call handled, return 
 * the results to the trapped core, and then resume execution of that
 * core program. 
 *
 * This uses functions from the x_epiphany_syscall module and also calls
 * e_resume. 
 *
 * Returns 0 if successful, -1 if any error occurs. If an error occurs the
 * core will be left in a trapped state. 
 */
 
int
process_syscall_trap (x_epiphany_control_t * epiphany_control, 
                      int                    row, 
                      int                    col)
{
    int result = -1;
    x_epiphany_syscall_parameters_t syscall_parameters;
    
    if (X_SUCCESS != 
        x_get_epiphany_syscall_parameters (epiphany_control, 
                                           row, col, 
                                           &syscall_parameters)) {
        fprintf (stderr, 
                 "%s: error getting system call parameters from core (%d,%d)\n",
                 program_name, row, col);                              
    }
    else if (X_SUCCESS != 
             x_handle_epiphany_syscall (&syscall_parameters)) {
        fprintf (stderr, 
                 "%s: error handling system call %d on behalf of core (%d,%d)\n",
                 program_name, syscall_parameters.syscall_number, row, col);
    }
    else if (X_SUCCESS !=
             x_put_epiphany_syscall_results (epiphany_control, 
                                             row, col, &syscall_parameters)) {
        fprintf (stderr,
                 "%s: error placing system call %d results in memory of core (%d,%d)\n",
                 program_name, syscall_parameters.syscall_number, row, col);
    }                                                 
    else if (E_OK != 
             e_resume (&(epiphany_control->workgroup), row, col)) {
        fprintf (stderr,
                 "%s: e_resume failed on core (%d,%d) after system call %d\n",
                 program_name, row, col, syscall_parameters.syscall_number);
    }
    else {
        result = 0;
    }
    return result;
}

/* process_legacy_io_trap
 *
 * Internal routine to handle the "legacy" I/O traps write, read, close, and open
 * The basic flow mirrors that of process_syscall_trap except that the parameters
 * are mapped to the "proper" syscall types and the normal syscall handler is
 * invoked. 
 */

int
process_legacy_io_trap (x_epiphany_control_t * epiphany_control, 
                        int                    row, 
                        int                    col,
                        uint32_t               trap_number)
{
    int result = -1;
    x_epiphany_syscall_parameters_t syscall_parameters;
    
    if (X_SUCCESS != 
        x_get_epiphany_legacy_io_trap_parameters (epiphany_control, 
                                                  row, col, trap_number,
                                                  &syscall_parameters)) {
        fprintf (stderr, 
                 "%s: error getting legacy IO trap parameters from core (%d,%d)\n",
                 program_name, row, col);                              
    }
    else if (X_SUCCESS != 
             x_handle_epiphany_syscall (&syscall_parameters)) {
        fprintf (stderr, 
                 "%s: error handling legacy IO trap %d on behalf of core (%d,%d)\n",
                 program_name, trap_number, row, col);
    }
    else if (X_SUCCESS !=
             x_put_epiphany_legacy_io_trap_results (epiphany_control, 
                                                    row, col, trap_number,
                                                    &syscall_parameters)) {
        fprintf (stderr,
                 "%s: error placing legacy IO trap %d results in memory of core (%d,%d)\n",
                 program_name, trap_number, row, col);
    }                                                 
    else if (E_OK != 
             e_resume (&(epiphany_control->workgroup), row, col)) {
        fprintf (stderr,
                 "%s: e_resume failed on core (%d,%d) after legacy IO trap %d\n",
                 program_name, row, col, trap_number);
    }
    else {
        result = 0;
    }
    return result;
}

/* service_epiphany_traps
 *
 * The main polling routine. There may be reasons for a halted core other
 * that I/O traps - as far as possible this routine attempts to output
 * meaningful diagnostic information, but in general those core programs
 * must be treated as having terminated disgracefully. 
 *
 * Parameters:
 *   epiphany_control: an x_epiphany_control_t structure containing info
 *              on memory ranges shared between Epiphany and host (some 
 *              Epiphany internal RAM, some host RAM). 
 *   first_row, last_row,
 *   first_col, last_col: define a rectangular group of cores to be polled
 *              for I/O traps. 
 *
 * Return values:
 *    0: success - all cores have halted with an exit trap, or a trap or 
 *                 exception that cannot be handled. 
 *   -1: error
 *
 * Uses the global program_name in diagnostic messages written to stderr.
 *
 * Notes:
 *   Each core is normally polled every POLL_MICROSECONDS. If any I/O 
 *     operation has been performed the interval drops to REPOLL_MICROSECONDS
 *     for the next REPOLL_COUNT polls.
 *   The exit status of the cores is monitored and when all have halted on
 *     an exit trap the function returns to its caller. 
 *   If a trap occurs that cannot be handled - or any kind of software 
 *     exception - this is dealt with much like an exit trap. The "exit
 *     trap detected" flag is set for that core until there is a sign of 
 *     life in the form of a trap that *can* be handled.  
 *   If a hitherto unknown type of trap is detected the core's register 
 *     contents are dumped to aid debugging. 
 *   If an error occurs while handling a trap the routine returns to the
 *     caller with an error status.
 *   If I/O is performed on behalf of a core that core is polled again
 *     before trying another core - while this means that one core can
 *     monopolise the I/O monitor, it minimises interleaving of output
 *     when several cores are writing a character at a time. 
 */
 
int 
service_epiphany_traps (x_epiphany_control_t * epiphany_control,
                        int                    first_row, 
                        int                    last_row,
                        int                    first_col, 
                        int                    last_col)
{
    int result  = 0;
    int polling = 1;
    int post_io_countdown = 0;
    int num_cores_monitored = (last_row - first_row + 1)
                            * (last_col - first_col + 1);
    int num_cores_exited = 0;
    int exit_trapped[last_row+1][last_col+1];
    int exit_status [last_row+1][last_col+1];
    int row, col;
    int trap_action;

    x_epiphany_exception_t exception;
    int32_t                trap_number;

    for (row = first_row; row <= last_row; row++) {
        for (col = first_col; col <= last_col; col++) {
            exit_trapped[row][col] = 0;
            exit_status[row][col]  = 0;
        }
    }
    while (polling) {
        for (row = first_row; row <= last_row && polling; row++) {
            col = first_col;
            while (col <= last_col && polling) {
                trap_action = 1;                
                exception = 
                    x_catch_epiphany_exception (epiphany_control, row, col);
                trap_number = exception & X_EPIPHANY_EXCEPTION_VALUE_MASK;
                if (exception == X_NO_EXCEPTION) {
                    // peace and quiet
                }
                else if (exception < 0) {
                    fprintf (stderr, "%s: stopping polling due to error %d\n",
                             program_name, exception);
                    polling = 0;
                    result  = -1;
                }
                else if (((exception & X_EPIPHANY_EXCEPTION_TYPE_MASK) ==
                          X_TRAP_EXCEPTION) &&
                         (trap_number == X_SYSCALL_TRAP ||
                          trap_number == X_WRITE_TRAP   ||
                          trap_number == X_READ_TRAP    ||
                          trap_number == X_OPEN_TRAP    ||
                          trap_number == X_CLOSE_TRAP)) {
                    if (trap_number == X_SYSCALL_TRAP) {
                        trap_action = process_syscall_trap (epiphany_control, 
                                                            row, col);
                    }
                    else {
                        trap_action = process_legacy_io_trap (epiphany_control, 
                                                              row, col,
                                                              trap_number);
                    }
                    if (trap_action == -1) {
                        fprintf (stderr, 
                                 "%s: aborting polling due to previous error\n",
                                 program_name);
                        polling =  0;
                        result  = -1;
                    }
                    else if (exit_trapped[row][col]) {
                        exit_trapped[row][col] = 0;
                        num_cores_exited--;
                    }	
                    post_io_countdown = REPOLL_COUNT;
                }
                else if (!(exit_trapped[row][col])) {
                    num_cores_exited++;
                    if (num_cores_exited == num_cores_monitored) {
                        polling = 0;
                        result  = 0;
                    }
                    exit_trapped[row][col] = 1;
                    exit_status[row][col]  = trap_number;
                    if ((exception & X_EPIPHANY_EXCEPTION_TYPE_MASK) !=
                        X_TRAP_EXCEPTION) {
                        fprintf (stderr, "%s: Core (%d,%d) halted by %s\n",
                                 program_name, row, col,  
                                 x_epiphany_exception_desc (exception));
                    }             
                    else if (trap_number == X_EXIT_TRAP) {
                        if (E_ERR == 
                            (e_read (&(epiphany_control->workgroup), 
                                     row, col, E_REG_R0, 
                                     &(exit_status[row][col]), 4))) {
                            fprintf (stderr, "%s: e_read error on (%d,%d) r0\n",
                                     program_name, row, col);
                            exit_status[row][col] = -1;
                        }                        
                        fprintf (stderr, 
                                 "%s: Core (%d,%d) has exited with result %d\n",
                                 program_name, row, col, exit_status[row][col]);
                    }
                    else if (0) {
                        fprintf (stderr,
                                 "%s: Trap handler for trap %d is not implemented\n",
                                 program_name, trap_number);
                    }
                    else {
                        fprintf (stderr,
                                 "%s: Unknown trap %d on core (%d,%d)\n",
                                 program_name, trap_number, row, col);
                        x_dump_ecore_registers (&(epiphany_control->workgroup),
                                                row, col, 0);
                    }
                }
                if (trap_action == 0) {
                    // repoll the same core
                    usleep (REPOLL_MICROSECONDS);
                }
                else {
                    // no I/O done, move to next core
                    col++;
                }                    
            }
        }
        if (post_io_countdown <= 0) {
            usleep (POLL_MICROSECONDS);
        }
        else {
            usleep (REPOLL_MICROSECONDS);
            post_io_countdown--;
        }
    }
    return result;
}
        
/* extract_args
 *
 * Check and extract the command-line argments (row and column, both optional).
 * They are checked against the supplied workgroup dimensions - if not 
 * supplied, they default to the whole workgroup. 
 *
 * Outputs usage and error messages using the global program_name,
 * returning 0 if successful, -1 on error.
*/

void 
usage()
{
    fprintf (stderr, "Usage: no argments    - poll the entire Epiphany for I/O traps\n");
    fprintf (stderr, "       row and column - poll that specific core for I/O traps\n");
}

int
extract_args (int argc, char * argv[], 
              int workgroup_rows, int workgroup_cols,
              int * first_row_p,  int * last_row_p,
              int * first_col_p,  int * last_col_p)
{
    int result = 0;
    if (argc != 1 && argc != 3) {
        usage();
        result = -1;
    }
    if (argc != 3) {
        *first_row_p = 0;
        *last_row_p  = workgroup_rows - 1;
        *first_col_p = 0;
        *last_col_p  = workgroup_cols - 1; 
    }	
    else {
        if (1 != sscanf (argv[1], "%d", first_row_p)) {
            fprintf (stderr, "%s: the first argument is not a valid row number\n",
                     program_name);
            usage();
            result = -1;
        }
        else if (1 != sscanf (argv[2], "%d", first_col_p)) {
            fprintf (stderr, "%s: the second argument is not a valid column number\n",
                     program_name);
            usage();
            result = -1;
        }
        else if ((*first_row_p < 0) || (*first_row_p >= workgroup_rows)) {
            fprintf (stderr, "%s: the Epiphany chip has no row %d\n",
                     program_name, *first_row_p);
            result = -1;
        }
        else if ((*first_col_p < 0) || (*first_col_p >= workgroup_cols)) {
            fprintf (stderr, "%s: the Epiphany chip has no column %d\n",
                     program_name, *first_col_p);
            result = -1;
        }
        else {
            *last_row_p = *first_row_p;
            *last_col_p = *first_col_p;
        }	
    }
    return result;
}

/* main program for simon
 */

int 
main (int argc, char * argv[])
{
    int                    result = 1; 
    e_platform_t           platform;
    x_epiphany_control_t * epiphany_control;
    
    int                    row, first_row, last_row;
    int                    col, first_col, last_col;
    
    if (BYTE_ORDER != LITTLE_ENDIAN) {
        fprintf (stderr, 
                 "%s: this program can only run on a little-endian architecture\n", 
                 program_name);
    }
    else if (E_OK != e_init(NULL)) {
        fprintf (stderr, "%s: e_init failed, cannot connect to Epiphany\n",
                 program_name);
    }
    else if (E_OK != e_get_platform_info(&platform)) {
        fprintf (stderr, "%s: e_get_platform_info failed, aborting\n",
                 program_name);
    }
    else {
        if (0 == extract_args (argc, argv, platform.rows, platform.cols,
                               &first_row, &last_row, 
                               &first_col, &last_col)) {
                                        
            epiphany_control = 
                x_map_epiphany_resources (NULL, first_row, first_col,
                                          last_row - first_row + 1, 
                                          last_col - first_col + 1);
            if (epiphany_control == NULL) {
                fprintf (stderr, 
                         "%s: failed to map workgroup and external RAM\n",
                         program_name);                        
            }
            else {
                result = service_epiphany_traps (epiphany_control, 
                                                 first_row, last_row,
                                                 first_col, last_col);
                if (X_ERROR ==
                    x_unmap_epiphany_resources (epiphany_control)) {                    
                    fprintf (stderr, 
                             "%s: failed to unmap workgroup and external RAM\n",
                             program_name);
                }        
            }	
        }	
        if (E_OK != e_finalize()) {
            fprintf (stderr, "%s: e_finalize failed when shutting down\n",
                     program_name);
        }	
    }
    return result;
}
