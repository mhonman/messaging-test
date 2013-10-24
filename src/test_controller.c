/* 
  File: test_controller.c

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

/* This is a generic test driver that loads an x-lib Epiphany task onto every core
   of the device, configures wraparound connections between the tasks, then launches
   them and monitors progress of the application until its completion. 
   The name of the Epiphany executable is received as first argument, and the
   remaining arguments are passed to the newly created tasks. 
*/

#include <stdlib.h>
#include <stdio.h>
#include <x_application.h>
#include <x_application_display.h>

int main(int argc, char *argv[])
{
    int workgroup_rows    = 0,
        workgroup_columns = 0,
        host_task_slots   = 0;

    if (argc <= 1) {
        printf("test_controller: please specify an Epiphany executable on the command line\n");
    }	
    else if (x_initialize_application (NULL, &workgroup_rows, &workgroup_columns,
                                       &host_task_slots) == X_ERROR) {
        printf("test_controller: failed to initialise the application\n");
    }
    else {
        if (x_prepare_mesh_application (argv[1], X_WRAPAROUND_MESH) == X_ERROR) {
            printf("test_controller: failed to configure a mesh application\n");
        }
        else if (x_launch_application (argc-2, argv+2) == X_ERROR) {
            printf("test_controller: the application launch failed\n");
        }
        else {
          x_monitor_application (X_STATUS_DISPLAY | X_LIVE_DISPLAY, 1);
        }
        return (x_finalize_application ());
    }
}
