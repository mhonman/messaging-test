/*
  File: x_naive_matmul.c

  Copyright 2013 Mark Honman

  This program is free software: you can redistribute it and/or modify
  it under the terms of the GNU Lesser General Public License (LGPL) as published by
  the Free Software Foundation, either version 3 of the License, or
  (at your option) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  and the GNU Lesser General Public License along with this program,
  see the files COPYING and COPYING.LESSER. If not, see
  <http://www.gnu.org/licenses/>.
*/
/*========================= x_naive_matmul.c =============================*/

/* A very simple matrix multiplication that operates on in-core matrices
   and serves only to obtain a baseline floating-point performance figure.
*/

#include <stdio.h>
#include <string.h>
#include <x_task.h> 
#include <x_sleep.h> 
#include <x_endpoint.h> 
#include <x_application.h>
#include <x_sync.h> 

int task_main(int argc, const char *argv[]) 
{ 
    int                 wg_rows, wg_cols, my_row, my_col; 
    x_endpoint_handle_t ep;
    char                message[80];
    int                 received;

    x_get_task_environment (&wg_rows, &wg_cols, &my_row, &my_col); 
    x_set_task_status ("Howzit from task %d at (%d, %d) in workgroup of %d by %d", 
                       x_get_task_id(), my_col, my_row, wg_cols, wg_rows); 
    x_sleep(10); 
    if (my_col == 0) {
      x_sleep (my_row*2);
      ep = x_get_endpoint (X_TO_RIGHT);
      snprintf (message, sizeof(message), "Hello from (%d,%d)", my_col, my_row);
      x_set_task_status ("Sending...");
      x_sync_send(ep, message, strlen(message)+1);
    }
    else if (my_col == 1) {
      ep = x_get_endpoint (X_FROM_LEFT);
      x_set_task_status ("Receiving...");
      x_sleep((wg_rows - (my_row+1))*2);
      received = x_sync_receive(ep, message, sizeof(message));
      x_set_task_status ("Received [%s] from left", message);
      x_sleep(3);
    }  
    x_set_task_status ("Goodnight and goodbye from (%d, %d)", my_col, my_row); 
    return X_SUCCESSFUL_TASK; 
} 
