/*
File: x_epiphany_control.c

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

/* Host-only bundling of information available from the host-side
 * e-hal interface, and functions which use that information to assist 
 * host code in interfacing with the Epiphany chip.
 */

#ifndef __epiphany__

#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>

#include <e-hal.h>

#include "x_epiphany_control.h"
#include "x_epiphany_internals.h"

/* Notes:
 *   e_get_platform_info does not return information on the external memory
 *     segments, which is needed to build the list of external memory mappings. 
 *     So this has to be done the dirty way, by direct access to the eSDK
 *     global e_platform_t object.
 */
 
extern e_platform_t e_platform;

/*  x_alloc_epiphany_control
 *
 *  Returns an an empty (all-zeros) epiphany_control_t structure that
 *  has been sized to accommodate an external memory mapping for each
 *  segment known to the eSDK. 
 *
 *  Returns NULL on error. 
 */
 
x_epiphany_control_t * 
x_alloc_epiphany_control() 
{
    return calloc (1, sizeof(x_epiphany_control_t) + 
                      e_platform.num_emems*sizeof(e_mem_t));
}

/*  x_map_epiphany_resources
 *
 *  Opens a workgroup of the specified size and position on the Epiphany
 *  platform, also mapping all known external shared memory segments into 
 *  the caller's address space.
 *
 *  This information is saved into the supplied x_epiphany_control_t 
 *  structure which is then marked as initialised.  If a NULL pointer is
 *  received, an epiphany control structure is allocated and returned.
 *
 *  Returns the address of the x_epiphany_control_t structure if successful,
 *  or NULL on error. If the epiphany control structure is already marked
 *  as initialised, that is considered to be an error.
 *
 *  The supplied workgroup dimensions are trimmed to fit the
 *  Epiphany device. If the row and column count are zero, the workgroup
 *  spans the entire device. 
 *
 *  Notes:
 *    When the workgroup is opened, e_open maps the Epiphany into host 
 *      address space - and also map the first external shared RAM segment 
 *      (e_alloc does not provide a way to map any additional segments). 
 *    Under certain error conditions a dynamically allocated Epiphany control
 *      structure may not be freed:
 *        if e_get_platform_info(), e_open(), or e_alloc() fails
 */
 
x_epiphany_control_t *
x_map_epiphany_resources (x_epiphany_control_t * epiphany_control, 
                          int first_row, int first_col,
                          int rows,      int cols)
{
    x_epiphany_control_t * result = epiphany_control;
    int                    errors = 0;
    int                    workgroup_rows, workgroup_cols, mem_seg;

    if ((first_row < 0) || (first_col < 0)) {
        fprintf (stderr, "%s: origin of workgroup is less than (0,0)\n",
                 "x_map_epiphany_resources");    
        errors++;
    }
    else if (result == NULL) {
        if (NULL == 
            (result = x_alloc_epiphany_control ()) ) {
            fprintf (stderr, 
                     "%s: failed to allocate memory for the Epiphany control structure\n",
                     "x_map_epiphany_resources");
            errors++;
        }        
    }
    else if (result->initialized) {
        fprintf (stderr, 
                 "%s: the Epiphany control structure is already initialised\n",
                 "x_map_epiphany_resources");
        errors++;
    }    
    if (result != NULL) {                
        if (E_OK != e_get_platform_info(&(result->platform))) {
            fprintf (stderr,
                     "%s: e_get_platform_info() failed\n",
                     "x_map_epiphany_resources");
            errors++;
        }
        else {
            workgroup_rows = rows;
            workgroup_cols = cols;
            if ((first_row + workgroup_rows > result->platform.rows) ||
                (workgroup_rows == 0)) {
                workgroup_rows = result->platform.rows - first_row;
            }
            if ((first_col + workgroup_cols > result->platform.cols) ||
                (workgroup_cols == 0)) {
                workgroup_cols = result->platform.cols - first_col;
            }
            if (E_OK != e_open(&(result->workgroup), first_row, first_col,
                                 workgroup_rows, workgroup_cols)) {
                fprintf (stderr, "%s: e_open failed opening workgroup\n",
                         "x_map_epiphany_resources");
                errors++;
            }
            else {
                for (mem_seg = 0; 
                     (errors == 0) && (mem_seg < e_platform.num_emems); 
                     mem_seg++) {
                    if (E_OK != 
                        e_alloc (result->external_memory_mappings + mem_seg,
                                 0, e_platform.emem[mem_seg].size)) {
                        fprintf (stderr, 
                                 "%s: e_alloc failed mapping external memory segment %d\n",
                                 "x_map_epiphany_resources", mem_seg);
                        errors++;
                    }	
                    else {
                        result->num_external_memory_mappings++;
                    }
                }
                if (errors == 0) {
                    result->initialized = 1;
                }
                else {
                    // best-effort cleanup on error
                    for (mem_seg = 0; 
                         mem_seg < result->num_external_memory_mappings; 
                         mem_seg++) {
                        e_free (result->external_memory_mappings + mem_seg);
                    }                             
                    e_close(&(result->workgroup));
                }
            }
        }
    }
    return (errors == 0 ? result : NULL);
}    

/*  x_unmap_epiphany_resources
 *
 *  Undoes the good work of the analagous map routine (though it does not
 *  free the structure itself). 
 *
 *  Returns X_SUCCESS if successful, X_WARNING if the structure is
 *  uninitialised, otherwise X_ERROR. 
 */
x_return_stat_t 
x_unmap_epiphany_resources (x_epiphany_control_t * epiphany_control)
{
    x_return_stat_t result = X_SUCCESS;
    int             mem_seg;
    
    if ((epiphany_control == NULL) || (!epiphany_control->initialized)) { 
        fprintf (stderr, 
                 "%s: the supplied Epiphany control structure is uninitialised\n",
                 "x_unmap_epiphany_resources");
        result = X_WARNING;
    }
    else {
        if (E_OK != e_close (&(epiphany_control->workgroup))) {
            fprintf (stderr, "%s: e_close() failed\n",
                             "x_unmap_epiphany_resources");
            result = X_ERROR;
        }
        else {    
            for (mem_seg = 0; 
                 mem_seg < epiphany_control->num_external_memory_mappings;
                 mem_seg++) {
                if (epiphany_control->
                      external_memory_mappings[mem_seg].mapped_base != 0) {
                    if (E_OK != 
                        e_free (&(epiphany_control->
                                        external_memory_mappings[mem_seg]))) {
                        fprintf (stderr, "%s: e_free() failed\n",
                                 "x_unmap_epiphany_resources");
                        result = X_ERROR;
                    }                        
                }
            }	
        }
        epiphany_control -> initialized = 0;
    }
    return result;
}

/*  x_epiphany_to_host_address
 *
 *  Use the mappings described in the Epiphany control structure to determine the
 *  host-side address corresponding to the specified Epiphany-side address as seen
 *  by the given core in the workgroup. 
 *
 *  The epiphany_address is received as an unsigned 32-bit integer because
 *  (a) it was probably obtained from the register file of an Epiphany core and
 *  (b) this makes it independent of the host-side pointer model. 
 *
 *  Returns NULL if the Epiphany address is not mapped into host address space.
 *  Note that Epiphany-side NULL pointers also result in a return value of NULL. 
 *
 *  Tests are done in order of likely location and performance-sensitivity:
 *  first internal RAM of the specified core, then external RAM, and finally
 *  the unlkely case of internal RAM of another core.
 */

void *
x_epiphany_to_host_address (x_epiphany_control_t * epiphany_control, 
                            int row, int col, 
                            uint32_t epiphany_address)
{
    void * result = NULL;
        uint32_t   epiphany_address_coreid;
        int        mem_seg, epiphany_address_row, epiphany_address_col;
        e_mem_t   *mem_seg_p;

    if (epiphany_address != 0) {
        if ((epiphany_address & X_EPIPHANY_ADDRESS_COREID_MASK) == 0) {
            result = epiphany_control->workgroup.core[row][col].mems.base + 
                     epiphany_address;
        }
        else {
            for (mem_seg = 0; 
                 mem_seg < epiphany_control->num_external_memory_mappings; 
                 mem_seg++) {
                mem_seg_p = 
                    epiphany_control->external_memory_mappings + mem_seg;
                if ((epiphany_address >= mem_seg_p->ephy_base) && 
                    (epiphany_address < mem_seg_p->ephy_base +
                                        mem_seg_p->map_size)) {
                    result = mem_seg_p->base + 
                             (epiphany_address - mem_seg_p->ephy_base);
                }        
            }
            if (result == NULL) {
                // mem segment not matched, perhaps it is another core
                epiphany_address_coreid = 
                    X_EPIPHANY_ADDRESS_COREID(epiphany_address);
                epiphany_address_row = 
                            ((epiphany_address_coreid >> 6) & 0x3F) - 
                            epiphany_control->workgroup.row;
                epiphany_address_col = 
                            ((epiphany_address_coreid >> 0) & 0x3F) - 
                            epiphany_control->workgroup.col;
                if (epiphany_address_row < 0 || 
                    epiphany_address_row >= epiphany_control->workgroup.rows ||
                    epiphany_address_col < 0 || 
                    epiphany_address_col >= epiphany_control->workgroup.cols) {
                    fprintf(stderr,
                            "%s: core (%d,%d) address 0x%x is not in workgroup or external memory\n",
                            "x_epiphany_to_host_address", row, col,
                            epiphany_address);
                printf("0x%x 0x%x 0x%x %d %d %d %d\n",
                        epiphany_address & X_EPIPHANY_ADDRESS_COREID_MASK,
                        epiphany_address, epiphany_address_coreid,
                        epiphany_address_col, epiphany_address_row,
                        epiphany_control->workgroup.row, epiphany_control->workgroup.col);
                }
                else {
                    result = 
                        epiphany_control->
                            workgroup.core[epiphany_address_row]
                                          [epiphany_address_col].mems.base + 
                        (epiphany_address & ~X_EPIPHANY_ADDRESS_COREID_MASK);
                }	
            }
        }
    }
    return result;
}

#endif
