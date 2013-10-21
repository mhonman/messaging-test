/*
File: x_application_display.h

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

#ifndef _X_APPLICATION_DISPLAY_H_
#define _X_APPLICATION_DISPLAY_H_

#include "x_application.h"

/* Functions to present application information from the X-Lib data 
   structures to the user - the routines provide a task-level overview
   of the application. 
   Note that the X_LIVE_DISPLAY flag results in a periodic screen
   refresh, resulting in a "top" like activity display.             
*/

typedef uint16_t x_display_style_t;
#define X_DISPLAY_TYPE_MASK   (0xF)
#define X_NO_DISPLAY          (0x0)
#define X_SUMMARY_DISPLAY     (0x1)
#define X_TASK_LIST_DISPLAY   (0x2)
#define X_MAP_DISPLAY         (0x4)
#define X_MESSAGING_DISPLAY   (0x8)
#define X_LIVE_DISPLAY       (0x10)
#define X_DISPLAY_RUN_TIME   (0x20)
#define X_NO_DISPLAY_HEADERS (0x40)
#define X_DEFAULT_DISPLAY     (X_STATUS_DISPLAY)
#define X_STATUS_DISPLAY      (X_SUMMARY_DISPLAY|X_TASK_LIST_DISPLAY)
#define X_DETAIL_DISPLAY      (X_SUMMARY_DISPLAY|X_TASK_LIST_DISPLAY| \
                               X_MAP_DISPLAY|X_MESSAGING_DISPLAY)

x_application_state_t x_monitor_application (x_display_style_t display_style,
                                             uint16_t interval);

void x_display_application (x_display_style_t display_style);

/* The following routines are display building blocks. They output
   their pre-programmed display info, and only the following elements
   of the display style are honoured:
   * X_NO_DISPLAY
   * X_NO_DISPLAY_HEADERS
*/

void x_display_application_summary (x_display_style_t display_style);

void x_display_application_task_list (x_display_style_t display_style);

void x_display_application_map (x_display_style_t display_style);

void x_display_application_messaging (x_display_style_t display_style);

#endif /* _X_APPLICATION_DISPLAY_H_ */
