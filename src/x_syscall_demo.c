/*
  File: x_syscall_demo.c

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

/* A little program to identify packing of epiphany system call structures
   (stat, timeval, and timezone) and test Epiphany system calls. 
*/

#include <sys/types.h>
#include <sys/time.h>
#include <sys/stat.h>
#include <stdio.h>
#include <x_task.h> 
#include <x_sleep.h> 


int 
task_main(int argc, const char *argv[]) 
{ 
    int wg_rows, wg_cols, my_row, my_col; 
    
    x_get_task_environment (&wg_rows, &wg_cols, &my_row, &my_col);
    show_stat_layout (my_row, my_col);
    printf("Hello from (R,C) (%d,%d)\n",my_row,my_col);
    x_sleep(5);	
    
    return X_SUCCESSFUL_TASK; 
} 


int
show_stat_layout (int my_row, int my_col)
{
    struct stat stat_result;
    struct timeval  time;
    struct timezone zone;
    if (my_row == 0 && my_col == 0) {
        x_set_task_status("st_dev %d size %d",
                          (char*)(&(stat_result.st_dev))-(char*)(&stat_result),
                          sizeof(stat_result.st_dev));
    }
    if (my_row == 0 && my_col == 1) {
        x_set_task_status("st_ino %d size %d",
                          (char*)(&(stat_result.st_ino))-(char*)(&stat_result),
                          sizeof(stat_result.st_ino));
    }
    if (my_row == 0 && my_col == 2) {
        x_set_task_status("st_mode %d size %d",
                          (char*)(&(stat_result.st_mode))-(char*)(&stat_result),
                          sizeof(stat_result.st_mode));
    }
    if (my_row == 0 && my_col == 3) {
        x_set_task_status("st_nlink %d size %d",
                          (char*)(&(stat_result.st_nlink))-(char*)(&stat_result),
                          sizeof(stat_result.st_nlink));
    }
    if (my_row == 1 && my_col == 0) {
        x_set_task_status("st_uid %d size %d",
                          (char*)(&(stat_result.st_uid))-(char*)(&stat_result),
                          sizeof(stat_result.st_uid));
    }
    if (my_row == 1 && my_col == 1) {
        x_set_task_status("st_gid %d size %d",
                          (char*)(&(stat_result.st_gid))-(char*)(&stat_result),
                          sizeof(stat_result.st_gid));
    }
    if (my_row == 1 && my_col == 2) {
        x_set_task_status("st_rdev %d size %d",
                          (char*)(&(stat_result.st_rdev))-(char*)(&stat_result),
                          sizeof(stat_result.st_rdev));
    }
    if (my_row == 1 && my_col == 3) {
        x_set_task_status("st_size %d size %d",
                          (char*)(&(stat_result.st_size))-(char*)(&stat_result),
                          sizeof(stat_result.st_size));
    }
    if (my_row == 2 && my_col == 0) {
        x_set_task_status("st_blksize %d size %d",
                          (char*)(&(stat_result.st_blksize))-(char*)(&stat_result),
                          sizeof(stat_result.st_blksize));
    }
    if (my_row == 2 && my_col == 1) {
        x_set_task_status("st_blocks %d size %d",
                          (char*)(&(stat_result.st_blocks))-(char*)(&stat_result),
                          sizeof(stat_result.st_blocks));
    }
    if (my_row == 2 && my_col == 2) {
        x_set_task_status("st_atime %d size %d",
                          (char*)(&(stat_result.st_atime))-(char*)(&stat_result),
                          sizeof(stat_result.st_atime));
    }
    if (my_row == 2 && my_col == 3) {
        x_set_task_status("st_mtime %d size %d",
                          (char*)(&(stat_result.st_mtime))-(char*)(&stat_result),
                          sizeof(stat_result.st_mtime));
    }
    if (my_row == 3 && my_col == 0) {
        x_set_task_status("st_ctime %d size %d",
                          (char*)(&(stat_result.st_ctime))-(char*)(&stat_result),
                          sizeof(stat_result.st_ctime));
    }
    if (my_row == 3 && my_col == 1) {
        x_set_task_status("tv_sec %d size %d",
                          (char*)(&(time.tv_sec))-(char*)(&time),
                          sizeof(time.tv_sec));
    }
    if (my_row == 3 && my_col == 2) {
        x_set_task_status("tv_usec %d size %d",
                          (char*)(&(time.tv_usec))-(char*)(&time),
                          sizeof(time.tv_usec));
    }
    if (my_row == 3 && my_col == 3) {
        x_set_task_status("tz_dsttime %d size %d",
                          (char*)(&(zone.tz_dsttime))-(char*)(&zone),
                          sizeof(zone.tz_dsttime));
    }
}

