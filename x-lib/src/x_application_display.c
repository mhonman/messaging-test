/*
File: x_application_display.c

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

/* Functions to display the contents of X-lib data structures.
   While they are compilable on both host and Epiphany, they aren't
   much use on the latter!
*/

#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <sys/time.h>
#include "x_application.h"
#include "x_application_internals.h"
#include "x_application_display.h"

/* x_monitor_application

   Monitor the state of the application while it is running. 
   Periodic status display is performed if indicated by the caller in
   the display_style parameter. 

   Additionally the 'LIVE DISPLAY' flag can be included in DISPLAY_STYLE
   to indicate that the terminal should be cleared each time the application
   information is displayed. 
   
   And if the X_DISPLAY_RUN_TIME flag is included, the time from the call
   to application completion is computed and output before returning to the
   caller. 

   The return value is the terminal state of the application.
   
   Notes:
   * the application status is checked every millisecond regardless of the
     display interval. 
*/
x_application_state_t x_monitor_application (x_display_style_t display_style,
                                             uint16_t interval)
{
        struct timeval        start_time, end_time, elapsed_time;
        x_application_state_t last_state;
        int                   output_frequency = interval * 1000;
        int                   check_count = output_frequency;
        char                 *outcome;
        
        if (x_application == NULL) {
          printf ("Monitor Application: No application exists\n");
          return X_NO_APPLICATION_DATA;
        }
        else {
          gettimeofday(&start_time, NULL);
        
          while (x_get_application_state(NULL) > X_VIRGIN_APPLICATION) {
            if (check_count == output_frequency) {
              x_display_application (display_style);
              check_count = 0;
            }	  
            usleep(1000);
            check_count++;
          }
        
          gettimeofday(&end_time, NULL);
          x_display_application (display_style);
        
          last_state = x_get_application_state(NULL);
          if (display_style & X_DISPLAY_RUN_TIME) {
            timersub (&end_time, &start_time, &elapsed_time);
            if (last_state <= X_FAILED_APPLICATION) {
              outcome = "Failed";
            }
            else {
              outcome = "Successful";
            }
            printf ("Application %s. Seconds elapsed: %f\n", outcome,
                    elapsed_time.tv_sec + (elapsed_time.tv_usec / 1000000.0));
          }
          return last_state;
        }
}

/* x_display_application

   Provide a once-off dump of human-readable application info, following
   the requested display style. Effectively this dispatches the request
   to one or more of the x_print_application_xyz routines. 

   Notes:
   * The X_DISPLAY_RUN_TIME option is ignored.
   * The ANSI/VT100 escape sequence for home and clear is used in live
     display mode - other terminal types are not supported. 
*/

void x_display_application (x_display_style_t display_style)
{
        if (x_application == NULL) {
          printf ("Monitor Application: No application exists\n");
        }
        else {
          if (display_style & X_LIVE_DISPLAY) {
            puts("\033[H\033[J");
          }
          if (display_style & X_SUMMARY_DISPLAY) {
            x_display_application_summary (display_style); 
          }
          if (display_style & X_TASK_LIST_DISPLAY) {
            x_display_application_task_list (display_style);
          }
          if (display_style & X_MAP_DISPLAY) {
            x_display_application_map (display_style);
          }
          if (display_style & X_MESSAGING_DISPLAY) {
            x_display_application_messaging (display_style);
          }
        }  
}

/*-------------------- Building-block functions -----------------------*/
        
/* x_display_application_summary

   Writes a summary of the state of the application to standard output. 
*/

void x_display_application_summary (x_display_style_t display_style)
{
        char  message_buf[80];
        char *state_message;
        x_application_state_t      state;
        x_application_statistics_t statistics;
        
        if ((display_style & X_DISPLAY_TYPE_MASK) != X_NO_DISPLAY) {
          if (x_application == NULL) {
            printf ("Application Summary: No application exists\n");
          }
          else {
            state = x_get_application_state (&statistics);
            if (state == X_RUNNING_APPLICATION) {
              state_message = "Running";
            }
            else if (state <= X_FAILED_APPLICATION) {
              state_message = "Failed";
            }
            else if (state == X_SUCCESSFUL_APPLICATION) {
              state_message = "Successful!";
            }
            else if (state == X_INITIALIZING_APPLICATION) {
              state_message = "Initializing";
            }        
            else if (state == X_VIRGIN_APPLICATION) {
              state_message = "In Preparation";
            }
            else {
              snprintf(message_buf, sizeof(message_buf), "Unknown state %d", state);
              state_message = message_buf;
            }
            printf ("%s  WG %dx%d  Virgin:%d  Init:%d  Act:%d  Success:%d  Fail:%d\n",
                    state_message, 
                    x_application->workgroup_columns,
                    x_application->workgroup_rows,
                    statistics.virgin_tasks,
                    statistics.initializing_tasks,
                    statistics.active_tasks,
                    statistics.successful_tasks,
                    statistics.failed_tasks
                   );
          }
        }
}

/* x_display_application_task_list

   Writes the information in the application's task list to standard output
*/

void x_display_application_task_list (x_display_style_t display_style)
{
        char *executable_path, *executable_name;
        int   index, workgroup_column, workgroup_row;
        
        if ((display_style & X_DISPLAY_TYPE_MASK) != X_NO_DISPLAY) {
          if (x_application == NULL) {
            printf ("Application Task List: No application exists\n");
          }
          else {
            x_task_descriptor_t *task_descriptor_table,
                                *last_descriptor_in_table,
                                *descriptor;
            task_descriptor_table = (x_task_descriptor_t*)
              ((char*)x_application + x_application->task_descriptor_table_offset);
            last_descriptor_in_table = task_descriptor_table +
              (x_application->workgroup_rows * x_application->workgroup_columns) +
              x_application->host_task_slots - 1;
 
            if ( (display_style & X_NO_DISPLAY_HEADERS) == 0 ) {
              printf ("Core/PID  Co Ro ST   HB CN Cmd                Status\n");
            }                    
            for (descriptor = task_descriptor_table;
                 descriptor <= last_descriptor_in_table; descriptor++) {
              if (descriptor->executable_file_name != 0) {
                index = descriptor - task_descriptor_table;
                if (index >= x_application->workgroup_columns *
                             x_application->workgroup_rows) {
                  // This is a Host task descriptor
                  workgroup_column = -1;
                  workgroup_row    = -1;
                }
                else {
                  workgroup_column = index % x_application->workgroup_columns;
                  workgroup_row    = index / x_application->workgroup_columns;
                }
                executable_path = ((char*)x_application) + 
                                  descriptor->executable_file_name;
                executable_name = strrchr(executable_path,'/');
                if (executable_name != NULL) {
                  executable_name++;
                }
                else {
                  executable_name = executable_path;
                }
                printf("%7d   %2d %2d %2d %4d %2d %.18s %s\n",
                       descriptor->coreid_or_pid, workgroup_column, workgroup_row,
                       descriptor->state, descriptor->heartbeat % 10000,
                       descriptor->num_connections,
                       executable_name, descriptor->status);                      
              }
            }              
          }
        }
}

/* x_display_application_map

   Writes a visual representation of the cores and their activity to standard output
*/

void x_display_application_map (x_display_style_t display_style)
{
        if ((display_style & X_DISPLAY_TYPE_MASK) != X_NO_DISPLAY) {
          if (x_application == NULL) {
            printf ("Application Map: No application exists\n");
          }
          else {
            printf ("x_display_application_map is not yet implemented\n");
          }	  
        }
}
/* x_display_application_messaging

  Writes as much information as possible on the state of messaging to 
  standard output. 
*/

void x_display_application_messaging (x_display_style_t display_style)
{
        if ((display_style & X_DISPLAY_TYPE_MASK) != X_NO_DISPLAY) {
          if (x_application == NULL) {
            printf ("Application Messaging: No application exists\n");
          }
          else {
            printf ("x_display_application_messaging is not yet implemented\n");
          }
        }  
}

