/*
File: x_application.c

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

#include <stdlib.h>
#include <stddef.h>
#include <string.h>

#include "x_error.h"
#include "x_epiphany_control.h"
#include "x_application.h"
#include "x_application_internals.h"
#include "x_application_display.h"

/*=================== APPLICATION DATA STRUCTURES =====================*/

/*  Each task has its own local control structure, and a pointer back to the
 *  application data structure in shared DRAM. In the case of Epiphany code 
 *  the location of that structure is defined at compile-time (right here!)
 *  but in the host program the setup must be done at run-time. 
 *
 *  The data structures in shared memory are accessed via a structure
 *  mapped onto shared RAM on the Epiphany side, and via a pointer 
 *  returned by an e_alloc call on the host side. On the host side the
 *  information needed to manage the Epiphany is contained in an
 *  x_epiphany_control structure. 
 * 
 *  CAVEAT:
 *	  The ARM host compiler normally aligns pointers on 64-bit boundaries
 *	  even when they are 32-bit quantities. So when there are arrays of 
 *	  pointers each element starts 8 bytes after the previous element. 
 *	  This is only an issue when dynamically allocating memory for these
 *	  arrays, because 8 bytes per pointer must be allocated. 
 *	  The alignment of pointers in structures is controlled by the 
 *	  pack(n) pragma (and that is used in x_application_internals.h to
 *	  ensure that host and epiphany have consistent packing rules). 
 */

x_task_control_t task_control;

#ifdef __epiphany__

#include <e_lib.h>
x_application_t *x_application = (x_application_t*)
                   (X_EPIPHANY_SHARED_DRAM_BASE + X_LIB_SECTION_OFFSET);

#else

#include <e_hal.h>
x_application_t * x_application = NULL;

x_epiphany_control_t * x_epiphany_control = NULL;

#endif

/*========================= COMMON FUNCTIONS ===========================*/

/*--------------------------- INTERNAL USE -----------------------------*/

/*  Conversion between task IDs and descriptors. 
 *
 *  The Task IDs are 0-based indexes into the task descriptor table in the
 *  application data structure.
 *
 *  The functions return a null task or pointer if no such task exists in
 *  the application data structure. 
 *
 *  Validation in x_task_descriptor_id:
 *	1. the descriptor address is within the address range of the task 
 *	   descriptor table. 
 *  2. the descriptor address points to the start of a descriptor in the
 *	   table
 *  3. the descriptor is in use (executable name is non-empty)
 *
 *  Validation in x_task_id_descriptor:
 *	1. The supplied ID is a valid index into the task descriptor table. 
 * 	2. The descriptor is in use (executable name is non-empty)
 */

static 
x_task_id_t x_task_descriptor_id (x_task_descriptor_t * descriptor)
{
    x_task_id_t result = X_NULL_TASK;
    x_task_descriptor_t * task_descriptor_table,
                        * last_descriptor_in_table;
        
    task_descriptor_table = (x_task_descriptor_t*)
        ((char*)x_application + x_application->task_descriptor_table_offset);
    last_descriptor_in_table = task_descriptor_table +
        (x_application->workgroup_rows * x_application->workgroup_columns) +
        x_application->host_task_slots - 1;
        
    if ((descriptor >= task_descriptor_table) && 
        (descriptor <= last_descriptor_in_table) && 
        (((char*)descriptor - (char*)task_descriptor_table) %
          sizeof(x_task_descriptor_t) == 0) &&
        (descriptor->executable_file_name != 0)) {
        result = descriptor - task_descriptor_table;
    }		
    return result;
}

static 
x_task_descriptor_t * x_task_id_descriptor (x_task_id_t id)
{
    x_task_descriptor_t * result = NULL;

    if ((id >= 0) && 
        (id < (x_application->workgroup_rows * x_application->workgroup_columns) +
              x_application->host_task_slots)) {
        x_task_descriptor_t * task_descriptor_table = (x_task_descriptor_t*)
            ((char*)x_application + x_application->task_descriptor_table_offset);
        if (task_descriptor_table[id].executable_file_name != 0) {
            result = task_descriptor_table + id; 		  
        }	  
    }
}

/*------------------ EXTERNALLY VISIBLE FUNCTIONS --------------------*/

/*  x_workgroup_member_task
 *
 *  Returns the task ID of the task assigned to the specified workgroup 
 *  member, the null task ID if there is none OR if there is an error 
 *  (e.g. coordinates outside the workgroup dimensions). 
 *  In the latter case the x-lib error info is set for future retrieval.
 *  If there is no task configured for the specified position, this is
 *  not considered to be an error. 
 *
 *  In future this routine could take directional modifiers. 
 */

x_task_id_t 
x_workgroup_member_task (int row, int column)
{
    x_task_id_t result = X_NULL_TASK;
    if (x_application == NULL) {
        x_error (X_E_APPLICATION_NOT_INITIALISED, 0, NULL);
    }
    else if (column < 0 || column >= x_application->workgroup_columns) {
        x_error (X_E_TASK_COORDINATES_OUTSIDE_WORKGROUP, column, NULL);
    }
    else if (row < 0 || row >= x_application->workgroup_columns) {
        x_error (X_E_TASK_COORDINATES_OUTSIDE_WORKGROUP, row, NULL);
    }
    else {
        int index = (row * x_application->workgroup_columns) + column;
        x_task_descriptor_t * task_descriptor_table = (x_task_descriptor_t*)
            ((char*)x_application + x_application->task_descriptor_table_offset);
        if (task_descriptor_table[index].executable_file_name != 0) {
            result = (x_task_id_t)index;
        }
    }
    return result;
}

/*  x_get_task_coordinates
 *
 *  Given a task ID, returns its coordinates in the workgroup if it is
 *  a workgroup task. The coordinates are returned in int variables passed by
 *  reference, and the return value of the function is normally X_SUCCESS. 
 *
 *  X_ERROR is returned if the task ID is not associated with a previously
 *  initialised workgroup task. More information is recorded by x_error
 *  for later retrieval. 
 */

x_return_stat_t 
x_get_task_coordinates (x_task_id_t task_id, int * row, int * column)
{
    x_return_stat_t      result;
    int                  calculated_row, calculated_column;
    x_task_descriptor_t *descriptor;
        
    if (x_application == NULL) {
        result = x_error (X_E_APPLICATION_NOT_INITIALISED, 0, NULL);
    }
    else if ((int)task_id < 0) {
        result = x_error (X_E_TASK_COORDINATES_OUTSIDE_WORKGROUP, task_id, NULL);
    }
    else {        
        calculated_row    = (int)task_id / x_application->workgroup_columns;
        calculated_column = (int)task_id % x_application->workgroup_columns;
        if (calculated_row > x_application->workgroup_rows) {  
            result = x_error (X_E_TASK_COORDINATES_OUTSIDE_WORKGROUP, task_id, NULL);
        }
        else {
            descriptor = x_task_id_descriptor (task_id);
            if (descriptor->executable_file_name == 0) {
                result = x_error (X_E_INVALID_TASK_ID, task_id, NULL);
            }
            else {
                *row    = calculated_row;
                *column = calculated_column;
                result = X_SUCCESS;
            }        
        }                  
    }
    return result;
}

/*  x_is_workgroup_task
 *  x_is_host_task
 *
 *  x_is_workgroup_task returns 1 if the given task ID refers to a member 
 *  of the workgroup, or 0 if it is a host task or the task ID is not
 *  associated with a previously initialised task.
 *
 *  The return values of x_is_host_task are analogous.
 *
 *  If the application is not yet initialised an error is recorded via
 *  x_error and the return value is 0. 
 */

int 
x_is_workgroup_task (x_task_id_t task_id)
{
    int                  result = 0;
    x_task_descriptor_t *descriptor;
        
    if (x_application == NULL) {
        x_error (X_E_APPLICATION_NOT_INITIALISED, 0, NULL);
    }
    else if (((int)task_id >= 0) && 
             ((int)task_id < x_application->workgroup_rows*
                             x_application->workgroup_columns)) {
        descriptor = x_task_id_descriptor (task_id);
        if (descriptor->executable_file_name == 0) {
            x_error (X_E_INVALID_TASK_ID, task_id, NULL);
        }
        else {
            result = 1;
        }                  
    }
}

int 
x_is_host_task (x_task_id_t task_id)
{
    int                  result = 0;
    int                  workgroup_slots;
    x_task_descriptor_t *descriptor;
        
    if (x_application == NULL) {
        x_error (X_E_APPLICATION_NOT_INITIALISED, 0, NULL);
    }
    else {
        workgroup_slots = x_application->workgroup_rows*
                          x_application->workgroup_columns;
        if (((int)task_id >= workgroup_slots) &&
            ((int)task_id < workgroup_slots+x_application->host_task_slots)) {
            descriptor = x_task_id_descriptor (task_id);
            if (descriptor->executable_file_name == 0) {
                x_error (X_E_INVALID_TASK_ID, task_id, NULL);
            }
            else {
                result = 1;
            }        
        }                  
    }
}

/*======================= HOST-ONLY FUNCTIONS =========================*/

#ifndef __epiphany__

#include <unistd.h>
#include <stdio.h>
#include <string.h>

/*------------------------ INTERNAL FUNCTIONS --------------------------*/

/* Not the fastest way of doing this but it will do for now! */

static 
int x_get_host_core_count ()
{
    static host_core_count = 0;
    if ( host_core_count == 0 ) {
        host_core_count = sysconf ( _SC_NPROCESSORS_ONLN );
        }
    return host_core_count;
}

/*  Working memory management within the application data structure
 *
 *  These functions return offsets from the start of the application data
 *  area - not absolute addresses (offsets are equally meaningful to epiphany
 *  and host cores). 
 *
 *  The return value is zero if insufficient space is available or the application
 *  data area has not been initialised. 
 *
 *  Storage is normally allocated from the end of the available working 
 *  memory, and is aligned on longword boundaries. 
 *
 *  All of these routines directly access and modify the application data area. 
 *
 *
 *  xawm_allocz       - allocate and clear space from the high end of working
 *	 				    memory, returning an offset to that space
 *  xawm_alloc_string - allocate space from the high end of working memory 
 *					    for the supplied string, copy it into that space, and 
 *					    return an offset to it. 
 */

static 
x_memory_offset_t xawm_allocz (size_t size)
{
    x_memory_offset_t result = 0;
    char             *start_address;
        
    if (x_application != NULL) {
        start_address = (void*) 
            (((char*)x_application) + 
             x_application->available_working_memory_end - size);
        start_address = start_address - ((uint64_t)start_address & 0x7);
        if (start_address >= 
            ((char*)x_application) + 
            x_application->available_working_memory_start) {
            memset (start_address, 0, size);
            result = start_address - (char*)x_application;
            x_application->available_working_memory_end = result;                   
        }
        return x_application->available_working_memory_end;
    }
    return result;
}

static 
x_memory_offset_t xawm_alloc_string (const char *string_to_copy)
{
    x_memory_offset_t result = 0;
    size_t            size = strlen(string_to_copy)+1;
    char             *start_address;
        
    if (x_application != NULL) {
        start_address = (void*) 
            (((char*)x_application) + 
             x_application->available_working_memory_end - size);
        start_address = start_address - ((uint64_t)start_address & 0x7);
        if (start_address >= 
            ((char*)x_application) + 
            x_application->available_working_memory_start) {
            strncpy (start_address, string_to_copy, size);
            result = start_address - (char*)x_application;
            x_application->available_working_memory_end = result;                   
        }
        return x_application->available_working_memory_end;
    }
    return result;
}

/*  Task Descriptor Table management functions
 *
 *  xtdt_next_available_host_task_descriptor - returns the address of the
 *					   the next unused task descriptor not associated with
 *					   an Epiphany workgroup core, NULL if there are no
 *					   non-workgroup descriptors available.
 *  xtdt_init_task_descriptor - initialises the given task descriptor. 
 *					   The descriptor must be unused OR a virgin task. 
 */

static 
x_task_descriptor_t * xtdt_next_available_host_task_descriptor ()
{
    x_task_descriptor_t * task_descriptor_table,
                        * search_start,
                        * search_end,
                        * result = NULL;
    
    task_descriptor_table = (x_task_descriptor_t*)
        ((char*)x_application + x_application->task_descriptor_table_offset);
    search_start = task_descriptor_table + 
        (x_application->workgroup_rows * x_application->workgroup_columns);
    search_end = search_start + x_application->host_task_slots - 1;
    result = search_start;
    while ((result <= search_end) && 
           (result->executable_file_name != 0)) {
      result++;
    }
    if (result > search_end) {
        result = NULL;
    }
    return result;
}

static int 
xtdt_init_task_descriptor (x_task_descriptor_t *descriptor, 
                           x_memory_offset_t executable_file_name)
{
    int result = 0;
    if (!descriptor) { 
        result = X_E_NULL_TASK_DESCRIPTOR;
    }
    else if (descriptor->executable_file_name != 0) {
        result = X_E_TASK_DESCRIPTOR_IN_USE;
    }
    else {
        memset (descriptor, 0, sizeof(*descriptor));
        descriptor->executable_file_name = executable_file_name;
        descriptor->state = X_VIRGIN_TASK;
    }  
    return result;
}	

/*--------------- Internal Connection Management functions -------------*/

/*  During the process of establishing connections these are tracked using
 *  host-private data structures. On completion they are transferred into
 *  in the application shared memory.
 *
 *  The reason for this is that it is easy to dynamically grow these 
 *  structures while they are in the heap. 
 *
 *  This logic assumes that the size of the workgroup and number of 
 *  host task slots does not change. 
 */

/* Temporary data structures */

#define XC_CHUNK_SIZE (20)
#define XC_SOURCE      (0)
#define XC_SINK        (1)

typedef struct {
    int      elements_allocated;
    int      elements_used;
    uint32_t connection_index[];
} xc_task_connection_lookup_t;

int xc_master_elements_allocated = 0;
int xc_master_elements_used      = 0;
x_connection_t * xc_master_connection_list = NULL;

xc_task_connection_lookup_t **xc_task_connection_index = NULL;

/*  xc_connect_by_task_id
 *  xc_connect_by_workgroup_coords
 *
 *  Internal routines to record connections in the application data structure.
 *  The caller is expected to have performed basic sanity checks on the 
 *  application.
 *
 *  xc_connect_by_workgroup_coords will take row and column values outside
 *  the workgroup boundaries - these are mapped onto a workgroup member by 
 *  reducing them modulo the workgroup dimensions. The purpose of this is
 *  to facilitate the configuration of wraparound connections. 
 *
 *  Validation:
 *	 1. The sender and receiver tasks must be configured
 *	 2. The keys must be unique per task
 *
 *  The internal routine xc_validate_task_key does one half of the work,
 *  for sender or receiver - validating input and checking for a 
 *  pre-existing connection using the given key.
 *  
 *  Returns:
 *	 -1 on duplicate key
 *	 -2 on error
 *	 0 for success
 */

static int 
xc_validate_task_key (x_task_id_t task_id, int key, 
                      int role, char *role_text,
                      xc_task_connection_lookup_t ***connection_lookup_p)
{
    int                           result = -2;
    x_task_descriptor_t          *descriptor;
    x_connection_t               *connection_to_test;
    int                           i;
    xc_task_connection_lookup_t **task_slot_p;

    descriptor   = x_task_id_descriptor(task_id);

    if (!descriptor || descriptor->executable_file_name == 0) {
        printf ("Connect Tasks: specified %s task %d is not configured\n",
                role_text, task_id);
    }
    else if (!(xc_master_connection_list) ||
             !(*connection_lookup_p)) {
        result = 0;  // OK: no data yet -> no duplication        
    }
    else {
        task_slot_p = *connection_lookup_p + (int)task_id;
        if (!(*task_slot_p)) {
            result = 0;
        }
        else {
            i = 0;
            if (role == XC_SOURCE) {                  
                while (i < (*task_slot_p)->elements_used && result == -2) {
                    connection_to_test = xc_master_connection_list +
                                         (*task_slot_p)->connection_index[i];
                    if (connection_to_test->source_task == task_id &&
                        connection_to_test->source_key  == key) {
                        result = -1;
                    }
                    i++;
                }
            }
            else if (role == XC_SINK) { 
                connection_to_test = xc_master_connection_list +
                                     (*task_slot_p)->connection_index[i];
                while (i < (*task_slot_p)->elements_used && result == -2) {
                    if (connection_to_test->sink_task == task_id &&
                        connection_to_test->sink_key  == key) {
                        result = -1;
                    }
                    i++;
                }
            }
            if (result == -1) {
                printf ("Connect Tasks: Duplicate key 0x%lx for %s task %d\n",
                        (unsigned long) key, role_text, task_id); 
            }
            else {
                result = 0;
            }                    
        }                   
    }          
}

/*  xc_add_task_endpoint
 *
 *  Assuming that the tasks concerned exist and the yets are not duplicated
 *  (checks performed by xc_validate_task_key), adds a connection to one of
 *  the tasks. 
 *
 *  Allocates or extends temporary connection related data structures as
 *  needed. 
 *
 *  Provides for 8 bytes per pointer in the connection_lookup array because this
 *  is the unit of pointer arithmetic used by the ARM compiler when dealing
 *  with arrays of pointers. See module heading comments for more details.
 *
 *  Returns -1 on error, 0 if successful. 
 */

static int 
xc_add_task_endpoint (x_task_id_t task_id,
                      xc_task_connection_lookup_t ***connection_lookup_p,
                      int new_connection_index)
{
    int                           result = -1;
    xc_task_connection_lookup_t **task_slot_p;
    int                           num_task_slots;
        
    num_task_slots = x_application->host_task_slots +
        (x_application->workgroup_rows * x_application->workgroup_columns);        

    if (!(*connection_lookup_p)) {
        *connection_lookup_p = (xc_task_connection_lookup_t**)calloc(num_task_slots,8);
    }        
    if (!(*connection_lookup_p)) {
        printf ("Connect Tasks: failed to allocate memory\n");
    }
    else {
        task_slot_p = *connection_lookup_p + (int)task_id;
        if (!(*task_slot_p)) {
            (*task_slot_p) = malloc(sizeof(xc_task_connection_lookup_t) + 
                                    XC_CHUNK_SIZE*sizeof(uint32_t));
            if (!(*task_slot_p)) {
                printf ("Connect Tasks: failed to allocate memory\n");
            }
            else {
                (*task_slot_p)->elements_allocated = XC_CHUNK_SIZE;
                (*task_slot_p)->elements_used = 0;
            }
        }
        if ((*task_slot_p)->elements_used == 
            (*task_slot_p)->elements_allocated) {
                
            (*task_slot_p)->elements_allocated += XC_CHUNK_SIZE;
            (*task_slot_p) = 
                realloc(*task_slot_p,
                        sizeof(xc_task_connection_lookup_t) + 
                        ((*task_slot_p)->elements_allocated) * sizeof(uint32_t));
            if (!(*task_slot_p)) {
                printf ("Connect Tasks: fatal error - failed to grow a dynamic structure\n");
            }
        }
        if (*task_slot_p) {
            (*task_slot_p)->connection_index[(*task_slot_p)->elements_used] =
                  new_connection_index;
            (*task_slot_p)->elements_used++;
            result = 0;
        }                   
    }          
}

/*  xc_connect_by_task_id
 *
 *  xc_add_task_endpoint is expected to do all the work of ensuring that 
 *  there is enough space to add another key to the connection lists of
 *  the tasks. 
 */

static x_return_stat_t 
xc_connect_by_task_id (x_task_id_t sender,   int sender_key,
                       x_task_id_t receiver, int receiver_key)
{
    x_return_stat_t result = X_ERROR;
    int             index;
                
    if ((0 == xc_validate_task_key (sender, sender_key, XC_SOURCE, "sending",
                                    &xc_task_connection_index)) &&
        (0 == xc_validate_task_key (receiver, receiver_key, XC_SINK, "receiving",
                                    &xc_task_connection_index))) {
                                                
        if (!xc_master_connection_list) {
            xc_master_connection_list = (x_connection_t*) 
                                         malloc(XC_CHUNK_SIZE*sizeof(x_connection_t));
            if (!xc_master_connection_list) {
                printf ("Connect tasks: failed to allocate memory\n");
            }
            else {
                xc_master_elements_allocated = XC_CHUNK_SIZE;
            }                    
        }
        else if (xc_master_elements_used == xc_master_elements_allocated) {
            xc_master_elements_allocated += XC_CHUNK_SIZE;
            xc_master_connection_list = (x_connection_t*)
                realloc(xc_master_connection_list,
                        xc_master_elements_allocated*sizeof(x_connection_t));
            if (!xc_master_connection_list) {                
                printf ("Connect Tasks: fatal error - failed to grow a dynamic structure\n");
            }
        }
        if (xc_master_connection_list) {
            index = xc_master_elements_used;
            xc_master_elements_used++;                    
            xc_master_connection_list[index].source_task     = sender;
            xc_master_connection_list[index].source_key      = sender_key;
            xc_master_connection_list[index].sink_task       = receiver;
            xc_master_connection_list[index].sink_key        = receiver_key;
            xc_master_connection_list[index].source_endpoint = NULL;
            xc_master_connection_list[index].sink_endpoint   = NULL;
            if ((0 != xc_add_task_endpoint (sender, &xc_task_connection_index,
                                            index)) ||
                (0 != xc_add_task_endpoint (receiver, &xc_task_connection_index,
                                            index))) {
              printf ("Connect Tasks: fatal error adding endpoint\n");
            }
            else {
                result = X_SUCCESS;
            }        
        }        
    }
    return result;
}

static 
x_return_stat_t xc_connect_by_workgroup_coords 
                (int sender_row,   int sender_column,   int sender_key,
                 int receiver_row, int receiver_column, int receiver_key)
{
    x_return_stat_t result = X_SUCCESS;
    x_task_id_t     sender_task_id;
    x_task_id_t     receiver_task_id;
    int             wrapped_sender_row, wrapped_sender_column,
                    wrapped_receiver_row, wrapped_receiver_column;
        
    wrapped_sender_row      = (sender_row + x_application->workgroup_rows) %
                               x_application->workgroup_rows;
    wrapped_sender_column   = (sender_column + x_application->workgroup_columns) %
                               x_application->workgroup_columns;
    wrapped_receiver_row    = (receiver_row + x_application->workgroup_rows) %
                               x_application->workgroup_rows;
    wrapped_receiver_column = (receiver_column + x_application->workgroup_columns) %
                               x_application->workgroup_columns;
        
    sender_task_id = x_workgroup_member_task  (wrapped_sender_row,
                                               wrapped_sender_column);
    receiver_task_id = x_workgroup_member_task(wrapped_receiver_row,
                                               wrapped_receiver_column);

    result = xc_connect_by_task_id (sender_task_id,   sender_key, 
                                    receiver_task_id, receiver_key);
                                        
    return result;
}

/*  xc_setup_application_connections
 *
 *  This function uses the information in the temporary connection map to
 *  create the master connection list and task-specific indexes into it - 
 *  all within the application data structure that is accessible to both the
 *  host and the Epiphany cores.
 *
 *  Since the temporary data structures have the same shape as the 
 *  structures in shared memory, the task is mainly one of allocating
 *  blocks of shared memory and copying entire arrays into them. 
 *
 *  Algorithm:
 *	  Check that no master connection list has yet been created. 
 *	  Allocate the shared master connection list and populate it from the
 *	    temporary data structure. 
 *	  Create a connection index list for each task, sized to 
 *	    contain the master-list index of every endpoint needed by that task. 
 */

#define MYDESC "Setup global connection lists"

static 
x_return_stat_t xc_setup_application_connections ()
{
    x_return_stat_t      result = X_ERROR;
    int                  num_task_slots, task_slot;
    x_task_descriptor_t *global_task_descriptors;
        
    num_task_slots = x_application->host_task_slots +
        (x_application->workgroup_rows * x_application->workgroup_columns);        

    if (x_application->connection_list_offset != 0) {
        printf ("%s: lists have already been created\n",MYDESC);
    }
    else if (xc_master_elements_used == 0) {
        result = X_SUCCESS;   // hooray, nothing to do!
    }
    else if (0 == (x_application->connection_list_offset = 
                   xawm_allocz(xc_master_elements_used*sizeof(x_connection_t)))) {
        printf ("%s: failed to allocate shared memory\n",MYDESC);
    }
    else {
        memcpy ((char*)x_application + x_application->connection_list_offset,
                xc_master_connection_list, xc_master_elements_used*sizeof(x_connection_t));
        x_application->connection_list_length = xc_master_elements_used;
                
        global_task_descriptors = (x_task_descriptor_t*)
            ((char*)x_application + x_application->task_descriptor_table_offset);                

        for (task_slot = 0; task_slot < num_task_slots; task_slot++) {
            if (xc_task_connection_index[task_slot] &&
                (xc_task_connection_index[task_slot]->elements_used > 0)) {
                if (0 == 
                    (global_task_descriptors[task_slot].connection_index =
                        xawm_allocz(xc_task_connection_index[task_slot]->elements_used*sizeof(int)))) {
                    printf ("%s: failed to allocate shared memory\n",MYDESC);
                    return X_ERROR;
                }
                else {
                    memcpy ((char*)x_application +
                            global_task_descriptors[task_slot].connection_index,
                            xc_task_connection_index[task_slot]->connection_index,
                            xc_task_connection_index[task_slot]->elements_used*sizeof(uint32_t));
                    global_task_descriptors[task_slot].num_connections =
                        xc_task_connection_index[task_slot]->elements_used;
                }        
            }                      
        }
        result = X_SUCCESS;          
    }        
    return result;
}

/*---------------------- EXTERNALLY VISIBLE FUNCTIONS --------------------*/

/*  x_get_application_state
 *
 *  Determine the application state by inspecting the state of its tasks.
 *  If the caller supplies the address of an x_task_statistics_t structure
 *  the number of tasks in each state will be recorded in that structure.
 *
 *  This function would work in an Epiphany core program but would be rather
 *  inefficient due to the shared DRAM references. 
 *
 *  Basic rules:
 *  - do not count descriptors that do not have an executable file name. 
 *  - if any task has a "failed" status (negative), the whole application has
 *	  failed.
 *  - otherwise if all tasks are successful, the application as a whole has 
 *	  completed successfully
 *  - otherwise if all tasks are in a virgin state, the whole application is
 *	  still virgin. 
 *  - if none of the above are true, the application is running normally
 */

x_application_state_t x_get_application_state (
                        x_application_statistics_t *statistics)
{
    x_application_state_t result = X_NO_APPLICATION_DATA;
    x_task_descriptor_t * task_descriptor_table,
                        * last_descriptor_in_table,
                        * descriptor;
    x_task_state_t        cur_task_state;
    int                   num_descriptors_in_use  = 0,
                          num_failed_tasks        = 0,
                          num_successful_tasks    = 0,
                          num_virgin_tasks        = 0,
                          num_initializing_tasks  = 0,
                          num_active_tasks        = 0;
        
    if (x_application == NULL) {
        x_error (X_E_APPLICATION_NOT_INITIALISED, 0, NULL);
    }
    else {
        task_descriptor_table = (x_task_descriptor_t*)
            ((char*)x_application + x_application->task_descriptor_table_offset);
        last_descriptor_in_table = task_descriptor_table +
            (x_application->workgroup_rows * x_application->workgroup_columns) +
            x_application->host_task_slots - 1;
                
        for (descriptor = task_descriptor_table;
             descriptor <= last_descriptor_in_table; descriptor++) {
            if (descriptor->executable_file_name != 0) {
                num_descriptors_in_use++;
                cur_task_state = descriptor->state;
                if (cur_task_state >= X_ACTIVE_TASK) {
                    num_active_tasks++;
                }
                else if (cur_task_state == X_INITIALIZING_TASK) {
                    num_initializing_tasks++;
                }        
                else if (cur_task_state == X_VIRGIN_TASK) {
                    num_virgin_tasks++;
                }
                else if (cur_task_state == X_SUCCESSFUL_TASK) {
                    num_successful_tasks++;
                }
                else {
                    num_failed_tasks++;
                }
            }
        }	       
        if (num_failed_tasks > 0) {
            result = X_FAILED_APPLICATION;
        }
        else if (num_virgin_tasks == num_descriptors_in_use) {
            result = X_VIRGIN_APPLICATION;
        }
        else if ((num_successful_tasks + num_virgin_tasks) == num_descriptors_in_use) {
            result = X_SUCCESSFUL_APPLICATION;
        }
        else if (num_initializing_tasks > 0) {
            result = X_INITIALIZING_APPLICATION;
        }        
        else {
            result = X_RUNNING_APPLICATION;
        }	  
    }
    if (statistics != NULL) {
        statistics->virgin_tasks       = num_virgin_tasks;
        statistics->initializing_tasks = num_initializing_tasks;
        statistics->active_tasks       = num_active_tasks;
        statistics->successful_tasks   = num_successful_tasks;
        statistics->failed_tasks       = num_failed_tasks;
        statistics->unused_cores       = 
            (x_application->workgroup_rows * x_application->workgroup_columns) +
            x_application->host_task_slots - num_descriptors_in_use;
    }
    return result;
}

/*-------------------------- Initialisation --------------------------------*/

/* x_initialize_application
 *
 * NB: the rows, columns, and host_task_slots parameters are IN-OUT.
 *
 * Prepare internal data structures for an application using the specified
 * workgroup size and number of host processes. 
 * If a value of zero is specified for workgroup rows or columns, the 
 * maximum possible workgroup size is used for that dimension. 
 * If the number of host tasks is specified to be zero, the number of actual
 * host CPU cores is returned. 
 * The workgroup size is constrained by the number of Epiphany cores available,
 * but the number of host-side tasks is unlimited. 
 * Returns the actual workgroup size and number of host task slots. 
 * If the host process executable file name supplied is non-null, 
 * a task entry is created for the current host task.
 */

x_return_stat_t 
x_initialize_application (const char * caller_executable_file_name,
                          int * workgroup_rows, int * workgroup_columns,
                          int * host_task_slots)
{
    int             task_descriptor_table_length;
    int             i;
    x_return_stat_t result = X_ERROR;
    
    // Sanity checking
    if ((x_epiphany_control != NULL) && (x_epiphany_control->initialized)) {
        fprintf (stderr,"The application has already been initialized\n");
        return X_ERROR;
    }
    // initialize system, read platform params from default HDF. 
    // Then, reset the platform and get the actual system parameters.
    // NB will clobber anything else running on the Epiphany!
    if (E_OK != e_init(NULL)) {
        fprintf (stderr,"x_initialize_application: e_init failed, aborting\n");
    }
    else if (E_OK != e_reset_system()) {
        fprintf (stderr,"x_initialize_application: e_reset_system failed, aborting\n");
    }
    else if (NULL ==
             (x_epiphany_control = 
                    x_map_epiphany_resources (NULL, 0, 0,
                                             *workgroup_rows, 
                                             *workgroup_columns)) ) {
            fprintf (stderr," %s: could not map the needed Epiphany resources\n",
                     "x_initialize_application");
    }                                        
    else {
        *workgroup_rows    = x_epiphany_control->workgroup.rows;
        *workgroup_columns = x_epiphany_control->workgroup.cols;
        if (*host_task_slots == 0) {
            *host_task_slots = x_get_host_core_count ();
        }
        // Initialise the application's shared memory data structures.
        x_application = (x_application_t*) 
            x_epiphany_to_host_address (x_epiphany_control, 0, 0, 
                                        X_EPIPHANY_SHARED_DRAM_BASE + 
                                            X_LIB_SECTION_OFFSET);
        if (x_application == NULL) {
            fprintf (stderr, 
                     "%s: Internal error while getting address of application data error!\n",
                     "x_initialize_application");
        }
        else {
            x_application->workgroup_rows    = *workgroup_rows;
            x_application->workgroup_columns = *workgroup_columns;
            x_application->host_task_slots   = *host_task_slots;
            x_application->connection_list_length       = 0;
            x_application->task_descriptor_table_offset = 0;
            x_application->connection_list_offset       = 0;
            x_application->available_working_memory_start =
                (void*)(x_application->working_memory) - 
                (void*)(x_application);
            x_application->available_working_memory_end =
                x_application->available_working_memory_start +
                sizeof(x_application->working_memory);
            // Allocate dynamically sized areas from working memory
            task_descriptor_table_length = 
                (*workgroup_rows)*(*workgroup_columns) + (*host_task_slots);	
            x_application->task_descriptor_table_offset =
                xawm_allocz (task_descriptor_table_length * 
                             sizeof(x_task_descriptor_t));
            // If required, create a task descriptor for the host process
            if (caller_executable_file_name != NULL) {
                xtdt_init_task_descriptor (
                            xtdt_next_available_host_task_descriptor(),  
                            xawm_alloc_string(caller_executable_file_name));
            }
            result = X_SUCCESS;
        }
    }
    return result;
}

/*----------------------- Task loading and assignment ----------------------*/

/*  x_create_task
 *
 */


x_task_id_t 
x_create_task (const char *executable_file_name, int core_id)
{
    if ( -1 == access (executable_file_name, R_OK) ) {
        printf ("Executable file %s cannot be accessed\n", executable_file_name);
        return X_ERROR;
    }
    else if (x_application == NULL) {
        printf ("Create Task: No application exists\n");
        return X_NO_APPLICATION_DATA;
    }
    else {
          
    }        

}


/*  x_prepare_mesh_application
 *
 *  Once the basic application data structures have been initialised via a call
 *  to x_initialize_application, x_prepare_mesh_application can be called to
 *  assign the specified executable to all Epiphany cores that do not already
 *  have an assigned task. The Epiphany cores are connected in a mesh, with or
 *  without wraparound connections depending on the mesh options. 
 *
 *  This communication network corresponds to the topology of the workgroup
 *  and includes tasks that were previously individually assigned via 
 *  x_create_task. 
 */

x_return_stat_t 
x_prepare_mesh_application (const char *workgroup_executable_file_name,
                            int         mesh_options)
{
    x_return_stat_t      result = X_SUCCESS;
    x_task_descriptor_t *task_descriptor_table,
                        *descriptor;
    x_memory_offset_t    executable_file_name_offset;
    int                  row, col;
    x_return_stat_t      left_connection_result, top_connection_result,
                         right_connection_result, bottom_connection_result;

    if (-1 == access (workgroup_executable_file_name, R_OK)) {
        printf ("Executable file %s cannot be accessed\n", 
                workgroup_executable_file_name);
        result = X_ERROR;
    }
    else if (x_application == NULL) {
        printf ("Prepare Mesh Application: No application exists\n");
        result = X_ERROR;
    }
    else {
        task_descriptor_table = (x_task_descriptor_t*)
            ((char*)x_application + x_application->task_descriptor_table_offset);
        executable_file_name_offset = 
            xawm_alloc_string(workgroup_executable_file_name);
                
        for (row = 0; row < x_application->workgroup_rows; row++) {
            for (col = 0; col < x_application->workgroup_columns; col++) {
                descriptor = task_descriptor_table + 
                             (row * x_application->workgroup_columns) + col;
                if (descriptor->executable_file_name == 0) {
                    // Core has nothing assigned: assign the default executable
                    xtdt_init_task_descriptor (descriptor, 
                                               executable_file_name_offset);  
                }        
            }
        }
        // Create connections
        for (row = 0; row < x_application->workgroup_rows; row++) {
            for (col = 0; col < x_application->workgroup_columns; col++) {
                if ((col > 0) || (mesh_options & X_WRAPAROUND_MESH)) {
                    left_connection_result =                     
                        xc_connect_by_workgroup_coords(row,col,  X_TO_LEFT,
                                                       row,col-1,X_FROM_RIGHT); 
                }
                if ((row > 0) || (mesh_options & X_WRAPAROUND_MESH)) {      
                    top_connection_result =
                        xc_connect_by_workgroup_coords(row,  col,X_TO_ABOVE,
                                                       row-1,col,X_FROM_BELOW); 
                }
                if ((col+1 < x_application->workgroup_columns) || (mesh_options & X_WRAPAROUND_MESH)) {
                    right_connection_result =       
                        xc_connect_by_workgroup_coords(row,col,  X_TO_RIGHT,
                                                       row,col+1,X_FROM_LEFT); 
                }        
                if ((row+1 < x_application->workgroup_rows) || (mesh_options & X_WRAPAROUND_MESH)) {
                    bottom_connection_result =      
                        xc_connect_by_workgroup_coords(row,  col,X_TO_BELOW,
                                                       row+1,col,X_FROM_ABOVE); 
                }
                if (left_connection_result < result) {
                    result = left_connection_result;
                }
                if (top_connection_result < result) {
                    result = top_connection_result;
                }
                if (right_connection_result < result) {
                    result = right_connection_result;
                }
                if (bottom_connection_result < result) {
                    result = bottom_connection_result;
                }
            }
        }
    }
    return result;
}

/*-------------------- Setup of inter-task communication -------------------*/

/*  The keys used in making connections can be any integer value, but
 *  a core cannot have connections with duplicate keys. 
 *  Connections can only be made prior to tasks being started. 
 */

x_return_stat_t 
x_connect_tasks (x_task_id_t sender,   int sender_key,
                 x_task_id_t receiver, int receiver_key)
{
    x_return_stat_t result = X_SUCCESS;
        
    if (x_application == NULL) {
        printf ("Connect Tasks: No application exists\n");
        result = X_ERROR;
    }
    result = xc_connect_by_task_id (sender, sender_key, receiver, receiver_key);
        
    return result;
}

/*----------------------------- Task execution -----------------------------*/

/*  x_launch_task
 *
 *  Loads and starts the specified task. If already running, the task is
 *  not affected and and warning is issued. 
 *  The optional parameters will be supplied to the task as its arguments. 
 */

x_return_stat_t x_launch_task (x_task_id_t task_id, ...)
{
    x_return_stat_t      result = X_ERROR;
    x_task_descriptor_t *descriptor;
    pid_t                new_pid;
    int                  e_result, row, column;

    if (x_application == NULL) {
        printf ("Launch Task: No application exists\n");
    }
    else { 
        descriptor = x_task_id_descriptor (task_id);
        if (!descriptor) {
            printf("Launch Task: There is no task with ID %d\n", task_id);
        }
        if (descriptor->executable_file_name == 0) {
            printf ("Launch Task: no executable file set for task ID %d\n", task_id);
        }
        else if (descriptor->state <= X_SUCCESSFUL_TASK) {
            printf ("Launch Task: task %d has already completed\n", task_id);
        }
        else if (descriptor->state != X_VIRGIN_TASK) {
            printf ("Launch Task: task %d is already running\n", task_id);
            result = X_WARNING;
        }
        else {
            if (x_is_workgroup_task(task_id)) {  
                x_get_task_coordinates (task_id, &row, &column);
                // start task on Epiphany
                e_result = e_start (&(x_epiphany_control->workgroup),
                                    row, column);
                if (e_result == E_ERR) {
                    printf ("Launch Task: e_start task %d on (%d,%d) failed\n",
                            task_id, column, row);
                }
                else {
                    result = X_SUCCESS;                      
                }
            }
            else if (x_is_host_task(task_id)) {
                // start task on host OS
                new_pid = fork();
                if (new_pid == -1) {
                    printf ("Launch Task: fork() failed\n");
                }
                else if (new_pid != 0) {
                    // parent process: store PID in task descriptor
                    descriptor->coreid_or_pid = new_pid;
                }
                else {
                    // new process: load the Unix executable
                }
            }                    
        }
    }        
}

/*  This launches any tasks that have not yet been started, passing the same string arguments
 *  to all of these tasks. 
 *
 *  If all workgroup members are to be loaded with the same executable, uses
 *  e_load_group. This is a simplistic test as the contents of the executable
 *  name strings are not compared, and e_load_group is capable of loading a
 *  subset of the cores in the workgroup. 
 *
 *  Separate load and start operations are used so that the whole workgroup
 *  can be started at the same time. 
 */
x_return_stat_t 
x_launch_application (int argc, char * argv[])
{
    x_return_stat_t      result = X_SUCCESS;
    x_task_descriptor_t *task_descriptor_table,
                        *descriptor;
    int   e_result;
    int   errors = 0;
    int   row, col;
    int   workgroup_size;
    int   first_executable_file_name_offset = -1;
    int   executable_file_name_matches = 0;
    int   workgroup_members_to_start   = 0;
    char *executable_file_name;
        
    if (x_application == NULL) {
        printf ("Launch Application: No application exists\n");
        result = X_ERROR;
    }
    else {

        xc_setup_application_connections ();

        workgroup_size = x_application->workgroup_rows * 
                         x_application->workgroup_columns;     
                
        task_descriptor_table = (x_task_descriptor_t*)
            ((char*)x_application + x_application->task_descriptor_table_offset);
        
        // Test whether a group load is possible
        for (row = 0; row < x_application->workgroup_rows; row++) {
            for (col = 0; col < x_application->workgroup_columns; col++) {
                descriptor = task_descriptor_table + 
                             (row * x_application->workgroup_columns) + col;
                if ((descriptor->executable_file_name != 0) && 
                    (descriptor->state == X_VIRGIN_TASK)) {
                    workgroup_members_to_start++;          
                    if (first_executable_file_name_offset == -1) {
                        first_executable_file_name_offset = 
                            descriptor->executable_file_name;
                    }        
                    if (descriptor->executable_file_name ==
                        first_executable_file_name_offset) {
                        executable_file_name_matches++;
                    }
                }
            }
        }

        if ((executable_file_name_matches == workgroup_size) &&
            (workgroup_members_to_start == workgroup_size) &&
            (workgroup_size > 0)) {
             
            executable_file_name = ((char*)x_application) + 
                                   task_descriptor_table[0].executable_file_name;
            e_result = e_load_group (executable_file_name,
                                     &(x_epiphany_control->workgroup), 0, 0,
                                     x_application->workgroup_rows,
                                     x_application->workgroup_columns, E_FALSE);
            if (e_result == E_ERR) {
                printf ("Launch Application: e_load_group %s failed\n",executable_file_name); 
                errors++;
            }        
        }
        else if (workgroup_size > 0) {
            for (row = 0; row < x_application->workgroup_rows; row++) {
                for (col = 0; col < x_application->workgroup_columns; col++) {
                    descriptor = task_descriptor_table + 
                                 (row * x_application->workgroup_columns) + 
                                 col;
                    if ((descriptor->executable_file_name != 0) && 
                        (descriptor->state == X_VIRGIN_TASK)) {
                            
                        executable_file_name = ((char*)x_application) + 
                                               descriptor->executable_file_name;
                        e_result = e_load (executable_file_name,
                                           &(x_epiphany_control->workgroup),
                                           row, col, E_FALSE);
                        if (e_result == E_ERR) {
                            printf ("Launch Application: e_load %s failed\n",
                                    executable_file_name); 
                            errors++;
                        }        
                    }
                }  
            }            
        }

        if (errors > 0) {
            printf ("Launch Application: not starting cores due to errors in loading phase\n");
            result = X_ERROR;
        }        
        else if (workgroup_members_to_start > 0) {          
            // If it would be useful to record start time, this is the place to do so
            if (workgroup_members_to_start == workgroup_size) {
                e_result = e_start_group (&(x_epiphany_control->workgroup));
                if (e_result == E_ERR) {
                    printf ("Launch Application: e_start_group failed\n");
                    errors++;
                }        
            }
            else {
                for (row = 0; row < x_application->workgroup_rows; row++) {
                    for (col = 0; col < x_application->workgroup_columns; col++) {
                        descriptor = task_descriptor_table + 
                            (row * x_application->workgroup_columns) + col;
                        if ((descriptor->executable_file_name != 0) && 
                            (descriptor->state == X_VIRGIN_TASK)) {
                            e_result = 
                                e_start (&(x_epiphany_control->workgroup),
                                         row, col);
                            if (e_result == E_ERR) {
                                printf ("Launch Application: e_start %d %d failed\n",
                                        row,col);
                                errors++;
                            }        
                        }            
                    }
                }
            }
        }            
        result = (errors == 0 ? X_SUCCESS : X_ERROR);
    }        
    return result;
}

/*-------------------------- Shutdown and cleanup --------------------------*/

/*  x_finalize_application
 *
 *  Regardless if the application state, if an application object exists, 
 *  terminate all running processes, stop the Epiphany coprocessor, and 
 *  free all related storage.
 *
 *  The return value is the last known state of the application,
 *  -ve values for failures, 0 for success, and +ve values for various
 *  unfinished states. 
 *  If there is no current application object, -32767 is returned.
 */

x_application_state_t x_finalize_application ()
{
    int                   e_result, row, col;
    x_application_state_t result;

    if (x_application == NULL) {
        result = X_NO_APPLICATION_DATA;
    }
    else {
        result = x_get_application_state(NULL);
        for (row = 0; row < x_application->workgroup_rows; row++) {
            for (col = 0; col < x_application->workgroup_columns; col++) {
                e_result = e_halt(&(x_epiphany_control->workgroup), row, col);
            }
        }
        if (X_ERROR == x_unmap_epiphany_resources (x_epiphany_control)) {
            fprintf (stderr, "%s: failed to unmap the Epiphany device and external RAM\n",
                             "x_finalize_application");
        }
        if (E_OK != e_finalize()) {
            fprintf (stderr, "%s: e_finalize() failed\n",
                             "x_finalize_application");
        }  
        // And pull out the shiny Unix gun and kill any spawned tasks
          
        // And discard temporary data structures. 
    }
    return result;
}


#endif /* NOT __epiphany__ */

