/*
File: x_application_diagnostics.c

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

/* These host-side functions provide a way to get diagnotic information 
   from a the x-lib internal data structures. They are mainly useful in
   diagnosing problems in the x-lib application management functions. 
*/

#include <stdio.h>
#include "x_application_internals.h"
#include "x_application_diagnostics.h"

/* x_dump_application_internals

   Dumps the contents of the application data area to standard output in
   vaguely human-readable form.
   
   If level of detail is > 0, the task descriptor table and connection list
   are included in the dump - they receive a level of detail one less than
   that requested by the caller. 
*/

void 
x_dump_application_internals (unsigned int level_of_detail)
{
    if (!x_application) {
        printf ("The x-lib application has not been initialised, x_application is NULL\n");
    }
    else {
        printf ("X-lib internal application data structures at 0x%lx\n",
                (unsigned long)x_application);
        printf ("  Workgroup %d rows by %d columns\n", 
                x_application->workgroup_rows, x_application->workgroup_columns);
        printf ("  Host task slots: %d\n",x_application->host_task_slots);
        printf ("  Master connection list entries: %d\n",
                x_application->connection_list_length);
        printf ("  Task descriptor table at offset 0x%lx\n",
                (unsigned long)x_application->task_descriptor_table_offset);
        printf ("  Master Connection list at offset 0x%lx\n",
                (unsigned long)x_application->connection_list_offset);
        printf ("  Available working memory from offset 0x%lx up to 0x%lx\n",
                (unsigned long)x_application->available_working_memory_start, 
                (unsigned long)x_application->available_working_memory_end);
        if (level_of_detail > 0) {
            x_dump_application_task_descriptor_table (level_of_detail - 1);
            x_dump_application_connection_list (level_of_detail - 1);
        }                                            
    }
}

/* x_dump_application_task_descriptor_table

   Unused descriptor are only shown at levels of detail > 0
*/

void 
x_dump_application_task_descriptor_table (unsigned int level_of_detail)
{
    x_task_descriptor_t *task_descriptor_table, *last_descriptor, 
                        *first_host_task_descriptor, *descriptor;
    x_task_id_t          task_id;
    int                  unused_task_descriptors = 0;
    uint32_t            *connection_index;
    int                  row, column, i;
    char                *arg;
        
    if ((!x_application) ||
        (x_application->connection_list_offset == 0)) {
        printf ("The x-lib application's task descriptor table has not been created yet\n");
    }
    else {
        task_descriptor_table = (x_task_descriptor_t*)
            ((char*)x_application + x_application->task_descriptor_table_offset);        
        first_host_task_descriptor = task_descriptor_table +
            x_application->workgroup_rows*x_application->workgroup_columns;
        last_descriptor = first_host_task_descriptor + x_application->host_task_slots - 1;
        for (descriptor = task_descriptor_table; 
             descriptor <= last_descriptor; 
             descriptor++) {
            if (descriptor == task_descriptor_table) {
                printf ("Workgroup tasks\n");
            }
            else if (descriptor == first_host_task_descriptor) {
                printf ("Host tasks\n");
            }                    
            if (descriptor->executable_file_name == 0) {
                unused_task_descriptors++;
                if (level_of_detail > 0) {                    
                    printf ("  Unused descriptor 0x%lx\n",(unsigned long)descriptor);
                }
            }
            else {
                task_id = x_task_descriptor_id (descriptor);
                if (descriptor < first_host_task_descriptor) {
                    x_get_task_coordinates (task_id, &column, &row);
                    printf ("  Descriptor for workgroup task %d (col, row) (%d,%d) coreid 0x%lx image %s\n",
                            task_id, column, row, (unsigned long)descriptor->coreid_or_pid,
                            (char*)x_application + descriptor->executable_file_name); 
                }
                else {              
                    printf ("  Descriptor for task %d host PID %d image %s\n",
                            task_id, descriptor->coreid_or_pid,
                            (char*)x_application + descriptor->executable_file_name);
                }
                if (descriptor->num_arguments == 0) {
                    printf ("    No arguments\n");
                }
                else {              
                    printf ("    Arguments:\n");
                    arg = (char*)x_application + descriptor->arguments_offset;
                    for (i = 0; i < descriptor->num_arguments; i++) {
                        printf("      %s\n", arg);
                        arg = (char*)rawmemchr(arg,'\0') + 1;
                    }
                }        
                if (descriptor->num_connections == 0) {
                    printf ("    No connections\n");
                }
                else {
                    printf ("    Connection numbers in master list\n      ");
                    connection_index = (uint32_t*)
                        ((char*)x_application + descriptor->connection_index);
                    for (i = 0; i < descriptor->num_connections; i++) {
                        printf ("%d ",connection_index[i]);
                    }                        
                    printf ("\n");    
                }
                printf ("      State %d Heartbeat %d\n", 
                        descriptor->state, descriptor->heartbeat);
                printf ("      Status string %s\n", descriptor->status);
            }              
        }  
        if (level_of_detail == 0) {
            printf ("%d task descriptors unused\n", unused_task_descriptors); 
        }					
    }                
}

/* x_dump_application_connection_list
*/

void x_dump_application_connection_list (unsigned int level_of_detail)
{
    x_connection_t *connection_list, *last_connection, *conn;
        
    if ((!x_application) ||
        (x_application->connection_list_offset == 0)) {
        printf ("The x-lib application's master connection list has not been created yet\n");
    }
    else {
        connection_list = (x_connection_t*)
                ((char*)x_application + x_application->connection_list_offset);
        printf ("Master Connection List starting at 0x%lx\n",
                (unsigned long)connection_list);
        if (x_application->connection_list_length == 0) {
            printf ("  The Connection list is empty\n");
        }
        else {
            printf ("  Source      Sink\n");
            printf ("  Task  Key        Endpoint     Task  Key        Endpoint\n");
            last_connection = connection_list + 
                              (x_application->connection_list_length-1);
            for (conn = connection_list; conn <= last_connection; conn++) {
                printf ("  %.5d 0x%.8lx 0x%.8lx  %.5d 0x%.8lx 0x%.8lx\n",
                        conn->source_task, 
                        (unsigned long)conn->source_key, 
                        (unsigned long)conn->source_endpoint,
                        conn->sink_task,   
                        (unsigned long)conn->sink_key,
                        (unsigned long)conn->sink_endpoint);
            }                         
        }
    }                
}

