/* 
  File: messaging_test.c

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

#include <stdio.h>
#include <x_application.h>
#include <x_application_display.h>

int main(int argc, char *argv[])
{
    int workgroup_rows    = 0,
        workgroup_columns = 0,
        host_task_slots   = 1;
    
    x_initialize_application (argv[0], &workgroup_rows, &workgroup_columns,
                              &host_task_slots); 
    x_prepare_mesh_application ("e_messaging_test.srec", X_WRAPAROUND_MESH);
    x_launch_application (argc-1, argv+1);
    x_monitor_application (X_STATUS_DISPLAY | X_DISPLAY_RUN_TIME | X_LIVE_DISPLAY, 1);
    return (x_finalize_application ());
}

