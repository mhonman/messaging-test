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
/*======================== x_naive_matmul.c ===========================*/

/* Very simple test program to measure floating-point performance of the
   e-core in the absence of the need to interact with external memory 
   and/or other cores. 
   This program should be compiled with -O3 to get the best performance. 
   
   It uses the task heartbeat to allow progress to be visualised when
   the host program is monitoring the ecore task statuses.    
*/

#include <x_task.h> 
#include <x_sleep.h> 
#include <x_endpoint.h> 
#include <x_application.h>

#define DIM (32)
void
naive_matmul (float a[DIM][DIM], float b[DIM][DIM], float c[DIM][DIM])
{
    int i, j, k;
    for (i = 0; i < DIM; i++) {
        for (j = 0; j < DIM; j++) {
            c[i][j] = 0;
            for (k = 0; k < DIM; k++) {
                c[i][j] += a[i][k] * b[k][j];
            }
        }
    }
}

void
matmul_32_32 (float a[DIM][DIM], float b[DIM][DIM], float c[DIM][DIM])
{
    int i, j, k;
    for (i = 0; i < DIM; i++) {
        for (j = 0; j < DIM; j++) {
            c[i][j] = 	(a[i][0]  * b[0][j]) +
                        (a[i][1]  * b[1][j]) + 
                        (a[i][2]  * b[2][j]) + 
                        (a[i][3]  * b[3][j]) + 
                        (a[i][4]  * b[4][j]) + 
                        (a[i][5]  * b[5][j]) + 
                        (a[i][6]  * b[6][j]) + 
                        (a[i][7]  * b[7][j]) + 
                        (a[i][8]  * b[8][j]) + 
                        (a[i][9]  * b[9][j]) + 
                        (a[i][10] * b[10][j]) + 
                        (a[i][11] * b[11][j]) + 
                        (a[i][12] * b[12][j]) + 
                        (a[i][13] * b[13][j]) + 
                        (a[i][14] * b[14][j]) + 
                        (a[i][15] * b[15][j]) + 
                        (a[i][16] * b[16][j]) + 
                        (a[i][17] * b[17][j]) + 
                        (a[i][18] * b[18][j]) + 
                        (a[i][19] * b[19][j]) + 
                        (a[i][20] * b[20][j]) + 
                        (a[i][21] * b[21][j]) + 
                        (a[i][22] * b[22][j]) + 
                        (a[i][23] * b[23][j]) + 
                        (a[i][24] * b[24][j]) + 
                        (a[i][25] * b[25][j]) + 
                        (a[i][26] * b[26][j]) + 
                        (a[i][27] * b[27][j]) + 
                        (a[i][28] * b[28][j]) + 
                        (a[i][29] * b[29][j]) + 
                        (a[i][30] * b[30][j]) + 
                        (a[i][31] * b[31][j]);
        }
    }
}

int 
task_main(int argc, const char *argv[]) 
{ 
    int                 wg_rows, wg_cols, my_row, my_col; 
    float a[DIM][DIM], b[DIM][DIM], c[DIM][DIM];
    int   i, j, k, l;

    x_get_task_environment (&wg_rows, &wg_cols, &my_row, &my_col); 
    for (i = 0; i < DIM; i++) {
        for (j = 0; j < DIM; j++) {
            a[i][j] = (float)(i + j);
            b[i][j] = (float)(i - j);
        }
    }
    x_set_task_status ("Naive local matrix mult (%dx%d) - 10 per HB",DIM,DIM);
    for (k = 0; k < 10000; k++) {
        for (l = 0; l < 10; l++) {
            naive_matmul (a, b, c);
            a[0][0]=c[0][0]/3.0;
        }
        x_task_heartbeat();
    }
    x_set_task_status("c[0][0] %f", c[0][0]);
    return X_SUCCESSFUL_TASK; 
} 
