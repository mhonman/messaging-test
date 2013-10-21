/*
File: x_task.c

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

/* These functions are for use within tasks.
   See x_task_management.h for task management functionality.
   
   But more than that, a main() is provided that will initialise
   x_lib, pick up parameters passed from the host, and then call
   the user-supplied task_main().
*/

#include <stdio.h>
#include <stdarg.h>

#ifdef __epiphany__
#include <e_lib.h>
#include <e_coreid.h>
#endif

#include <x_error.h>
#include <x_address.h>

#include <x_task_types.h>
#include <x_task.h>
#include <x_application_internals.h>
#include <x_endpoint.h>

/* x_global_address_local_coreid_bits

   If seems a little clunky having this here... there must be a more elegant way!
   The purpose of this global value (that should be initialised correctly and
   never changed!) is to assist with the many address transformations that
   are needed for inter-core communication.
*/

x_transfer_address_t x_global_address_local_coreid_bits = 0;

#define DO_TASK_HEARTBEAT { x_task_control.descriptor->heartbeat = ++x_task_control.heartbeat; }

static x_task_control_t x_task_control;

void x_task_heartbeat ()
{
  DO_TASK_HEARTBEAT;
}

/* x_get_task_id

   Gets the task's own ID. 
*/

x_task_id_t x_get_task_id ()
{
        return x_task_control.task_id;
}

/* x_get_task_environment

  The size of the workgroup must be obtained from the global x_application_data
  structure because the e_lib does not provide a way to retrieve this information.
  
  In the case of a host task, the row and column are not relevant and -1 is
  returned for these values. 
*/

void x_get_task_environment (int *workgroup_rows, int *workgroup_columns,
                             int *my_row, int *my_column)
{
        unsigned int row, col;
#ifdef __epiphany__	
        e_coreid_t coreid;
        coreid = e_get_coreid();
        e_coords_from_coreid (coreid, &row, &col);
#else
        row = -1;
        col = -1;
#endif
        *workgroup_rows    = x_application->workgroup_rows;
        *workgroup_columns = x_application->workgroup_columns;
        *my_row            = row;
        *my_column         = col;
}	

void x_set_task_status (const char *format, ...)
{
        va_list args;
        va_start (args, format);
        vsnprintf (x_task_control.descriptor->status, 
                   sizeof(x_task_control.descriptor->status),
                   format, args); 
        DO_TASK_HEARTBEAT;
}

/* x_get_endpoint

  Since keys are unique per task, simply look through the endpoint list
  to find the endpoint that matches the supplied key. 

  x_error is called and NULL returned if there is no local endpoint
  associated with the key - though this is not likely to be an error that 
  the caller can deal with gracefully.

  The search for matching key is necessarily in external RAM. If this turns
  out to be a source of performance issues, the key can be copied into the
  local endpoint structures. 
*/

x_endpoint_handle_t x_get_endpoint (int key)
{
  x_endpoint_handle_t result = NULL;
  x_endpoint_t *endpoint,
               *last_endpoint = x_task_control.endpoints + x_task_control.num_endpoints - 1;

  x_connection_t *connection_list = (x_connection_t*) (((char*)x_application) +
                                                        x_application->connection_list_offset);

  if (connection_list && x_task_control.endpoints) {

    for (endpoint = x_task_control.endpoints; endpoint <= last_endpoint; endpoint++) {
      if (endpoint->mode == X_SENDING_ENDPOINT &&
          connection_list[endpoint->connection_id].source_key == key) {
        return (x_endpoint_handle_t)endpoint;
      }
      else if (endpoint->mode == X_RECEIVING_ENDPOINT &&
               connection_list[endpoint->connection_id].sink_key == key) {
        return (x_endpoint_handle_t)endpoint;
      }
    }
  }   
  x_error (X_E_GET_ENDPOINT_KEY_NOT_FOUND, key, NULL);
  return result;
}   

/* x_initialise_task_control

   Initialise the local x_task_control structure, linking it back to the
   task descriptor in the xlib application data.

   The internal logic of this routine is different for Epiphany and
   host tasks. 
*/

static void xt_initialise_task_control ()
{
        x_task_descriptor_t *task_descriptor_table;
        int                  pid, workgroup_task_slots, i;

        task_descriptor_table = (x_task_descriptor_t*)
                (((char*)x_application) + x_application->task_descriptor_table_offset);

#ifdef __epiphany__	
        e_coreid_t   coreid;
        unsigned int row, col;
        coreid = e_get_coreid();
        e_coords_from_coreid (coreid, &row, &col);
        x_task_control.task_id = (x_task_id_t)
                (row * x_application->workgroup_columns) + col;        
#else  // i.e. NOT __epiphany__
        pid = getpid();
        workgroup_task_slots = x_application->workgroup_rows *
                               x_application->workgroup_columns;
        for (i = workgroup_task_slots; 
             i < workgroup_task_slots + x_application->host_task_slots; 
             i++) {
          if (task_descriptor_table[i].coreid_or_pid == pid) {
            x_task_control.task_id = (x_task_id_t)i;
            break;
          }
        }
#endif // __epiphany__
        x_task_control.descriptor = task_descriptor_table + 
                                    ((int)x_task_control.task_id);
        x_task_control.heartbeat     = 0;
        x_task_control.num_endpoints = 0;
        x_task_control.endpoints     = NULL;
}

/* xt_initialise_endpoints

   Builds the list of endpoints basic on the task-specific indexes into
   the master list in shared memory, updating shared memory with the
   addresses of the endpoints, and also waiting for the communication
   peers to supply the addresses of their endpoints. 

   On exit the endpoint list is full initialised. 

   The endpoint list is received as parameter because in the Epiphany
   environment there is no on-core memory manager - the simplest way to
   allocate memory areas of dynamic size is on the stack. 

*/

x_return_stat_t xt_initialise_endpoints (x_endpoint_t * endpoint_array, 
                                         size_t sizeof_endpoint_array)
{
        x_return_stat_t result = X_ERROR;
        x_task_id_t     this_task = x_get_task_id();
        int             num_endpoints, num_unresolved_endpoints;
        uint32_t       *connection_index;
        x_connection_t *master_connection_list;
        x_connection_t *connection;
        x_endpoint_t   *endpoint, *endpoint_global_address;
        int             i;

        num_endpoints          = x_task_control.descriptor->num_connections;
        connection_index       = (uint32_t*)
                                 ((char*)x_application + 
                                  x_task_control.descriptor->connection_index);
        master_connection_list = (x_connection_t*)
                                 ((char*)x_application +
                                  x_application->connection_list_offset);
        
        if (sizeof_endpoint_array < num_endpoints*sizeof(x_endpoint_t)) {
          result = x_error (X_E_XTASK_INTERNAL_ERROR, 0, xt_initialise_endpoints);
        }
        else {
          x_task_control.num_endpoints = num_endpoints;
          x_task_control.endpoints     = endpoint_array;
                
          endpoint = &(x_task_control.endpoints[0]);
#ifdef __epiphany__                
          e_coreid_t   coreid;
          unsigned int row, col;
          coreid = e_get_coreid();
          e_coords_from_coreid (coreid, &row, &col);
          endpoint_global_address = (x_endpoint_t*)
                                    e_get_global_address (row, col, endpoint);
#else
          endpoint_global_address = 
            (x_endpoint_t*)x_host_to_epiphany_shared_memory_address (endpoint);
#endif                
          for (i = 0; i < num_endpoints; i++) {
            connection = master_connection_list + connection_index[i];
            endpoint->sequence_from_peer = 0;
            endpoint->control_from_peer  = 0;
	    endpoint->address_from_peer  = 0;
	    endpoint->sequence = 0;
            endpoint->connection_id   = connection_index[i];
            endpoint->remote_endpoint = NULL;                  
            if (connection->source_task == this_task) {
              endpoint->mode              = X_SENDING_ENDPOINT;
              connection->source_endpoint = endpoint_global_address;
            }
            else if (connection->sink_task == this_task) {
              endpoint->mode              = X_RECEIVING_ENDPOINT;
              connection->sink_endpoint   = endpoint_global_address;
            }
            else { 
              endpoint->mode = X_UNINITIALISED_ENDPOINT;
            }        
            endpoint++;
            endpoint_global_address++;
          }
          /* Now the funky bit - pick up peer endpoint addresses from global
             memory. Hope that there are no slip-ups and this is not an 
             eternal loop. */
          do {
            x_usleep (10);
            endpoint = &(x_task_control.endpoints[0]);
            num_unresolved_endpoints = 0;
            for (i = 0; i < num_endpoints; i++) {
              if ((endpoint->mode != X_UNINITIALISED_ENDPOINT) &&
                  (endpoint->remote_endpoint == NULL)) {
                connection = master_connection_list + connection_index[i];
                if ((endpoint->mode == X_SENDING_ENDPOINT) &&
                    (connection->sink_endpoint != NULL)) {
                  endpoint->remote_endpoint = connection->sink_endpoint;
#ifndef __epiphany__
                  endpoint->remote_endpoint = 
                    (x_endpoint_t*) x_epiphany_core_memory_to_host_mapped_address
                            (endpoint->remote_endpoint);
#endif                            
                }
                else if ((endpoint->mode == X_RECEIVING_ENDPOINT) &&
                         (connection->source_endpoint != NULL)) {
                  endpoint->remote_endpoint = connection->source_endpoint;
#ifndef __epiphany__
                  endpoint->remote_endpoint = 
                    (x_endpoint_t*) x_epiphany_core_memory_to_host_mapped_address
                            (endpoint->remote_endpoint);
#endif                            
                }
                else {
                  num_unresolved_endpoints++;
                }
              } 
              endpoint++;              
            }  
            DO_TASK_HEARTBEAT;
          } 
          while (num_unresolved_endpoints > 0);
          
          result = X_SUCCESS;
        }
        return result;	
}

/* main() for Epiphany tasks

   The result must be a terminal task state - zero or negative. 
   Non-terminal result values and values outside the uint16_t range are
   mapped to the value X_E_TASK_RESULT_OUT_OF_RANGE;
*/

int main ()
{
        int      task_result;
        uint16_t result_to_report;

#ifdef __epiphany__
	x_global_address_local_coreid_bits = 
	    ((x_transfer_address_t)e_get_coreid()) << 20;
#else
	x_global_address_local_coreid_bits = 0;
#endif
	
        xt_initialise_task_control();
        
        { // Allocate storage for endpoints on stack before proceeding
          x_endpoint_t endpoints[x_task_control.descriptor->num_connections];

          x_task_control.descriptor->state = X_INITIALIZING_TASK;
          if (X_SUCCESS != xt_initialise_endpoints (endpoints, sizeof(endpoints))) {
            result_to_report = x_last_error(NULL,NULL);	  
          }
          else {		
            x_task_control.descriptor->state = X_ACTIVE_TASK;
            task_result = task_main (0, NULL);
            if ( task_result > 0 || task_result <= X_E_ERROR_CODES_START ) { 
              result_to_report = X_E_TASK_RESULT_OUT_OF_RANGE;
            }
            else {
              result_to_report = task_result;
            }
          }
          x_task_control.descriptor->state = result_to_report;
        }    
        return result_to_report; // actually goes nowhere for Epiphany tasks
}


