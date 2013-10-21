/*
File: x_sleep.c

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

#include <errno.h>
#include <unistd.h>
#include "x_sleep.h"
#include "x_lib_configuration.h"

#ifdef __epiphany__
#include <e_lib.h>
#endif

int x_usleep (unsigned int microseconds)
{
        if (microseconds > 1000000) {
          return -1;  // should also set EINVAL
        }
        else {
#ifdef __epiphany__
          // Future enhancement: find a timer that is not in use
          e_wait (E_CTIMER_0, microseconds * X_EPIPHANY_FREQUENCY);
          // Future: check for early end due to interrupt and return -1,
          //  setting error EINTR
          return 0;
#else
          return usleep (microseconds);
#endif
        }
}

/* x_sleep
*/

unsigned int x_sleep (int seconds)
{
        int second;
        for (second = 0; second < seconds; second++) {
          if ( EINTR == x_usleep(1000000) ) return (seconds - second);
        }
        return 0;
}


