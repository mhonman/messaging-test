/*
  File: e_messaging_test.c

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

#include <e_lib.h>
#include <x_task.h>
#include <x_sleep.h>
#include <x_application.h>
#include <x_endpoint.h>
#include <x_sync.h>
#include <x_connection_internals.h>

void sync_test (int connection_key)
{
        x_endpoint_handle_t endpoint = x_get_endpoint(connection_key);
        if (connection_key == X_TO_LEFT) {
          x_sleep(6);
          x_set_task_status ("First sync to left %x",endpoint);
          x_sync(endpoint);
          x_set_task_status ("First sync done");
          x_sleep(3);
          x_set_task_status ("Second sync to left");
          x_sync(endpoint);
          x_set_task_status ("Second sync done");
        }
        else if (connection_key == X_FROM_RIGHT) {
          x_sleep(3);
          x_set_task_status ("First sync from right %x",endpoint);
          x_sync(endpoint);
          x_set_task_status ("First sync done");
          x_sleep(6);
          x_set_task_status ("Second from right");
          x_sync(endpoint);
          x_set_task_status ("Second sync done");
        }
}

/* Sync speed tests

   x_sync as at 19/10/2013

   @600MHz, -O2 : 2035 outer loops per second, i.e. 20 million syncs per second.
   29 cycles (49nS) per sync - 5.5 times faster.
   absolute tightest time - without sanity checking - is 25 cycles. 
   12 of the cycles are needed for the function call and return.

   First working x_sync

   @600Mhz, no optimisation, an e-core does 244 outer loops per second, 
   i.e. 244 x 10000 sync/sec
   roughly 400nS per sync, 245 cycles.
   with -O2, this becomes 386 outer loops per second, 260nS, 155 cycles. 
*/

void sync_speed_test(int connection_key)
{
        x_endpoint_handle_t endpoint = x_get_endpoint(connection_key);
        int i, j;
        for (i = 0; i < 10000; i++) {
          for (j = 0; j < 1000; j++) {
            x_sync(endpoint);
            x_sync(endpoint);
            x_sync(endpoint);
            x_sync(endpoint);
            x_sync(endpoint);
            x_sync(endpoint);
            x_sync(endpoint);
            x_sync(endpoint);
            x_sync(endpoint);
            x_sync(endpoint);
          }
          x_task_heartbeat();
        }
}

void sync_send_test (int connection_key)
{
        x_endpoint_handle_t endpoint = x_get_endpoint(connection_key);
        int send_result, i;        
        char buf[512];
        for (i = 0; i < sizeof(buf); i++) {
          buf[i] = i & 0xFF;
        }
        x_sleep(6);
        x_set_task_status ("First send on %x",endpoint);
        send_result = x_sync_send(endpoint, buf, sizeof(buf));
        x_set_task_status ("First send result %d", send_result);
        x_sleep(3);
        x_set_task_status ("Second send");
        send_result = x_sync_send(endpoint, buf, sizeof(buf));
        x_set_task_status ("Second send result %d", send_result);
}

void sync_receive_test (int connection_key)
{
        x_endpoint_handle_t endpoint = x_get_endpoint(connection_key);
        int receive_result, i, failpos;        
        char buf[512];
        
        x_sleep(3);
        x_set_task_status ("First receive on %x",endpoint);
        for (i = 0; i < sizeof(buf); i++) {
          buf[i] = 0;          
        }
        receive_result = x_sync_receive(endpoint, buf, sizeof(buf));
        failpos = -1;
        for (i = 0; i < sizeof(buf); i++) {
          if (buf[i] != (i & 0xFF)) {
            failpos = i;
          } 
          buf[i] = 0;          
        }
        if (failpos >= 0) {
          x_set_task_status ("First transfer failed: mismatch at position %d",failpos);
          x_sleep(10);
        }                
        x_set_task_status ("First receive result %d", receive_result);
        x_sleep(6);
        x_set_task_status ("Second receive");
        failpos = -1;
        receive_result = x_sync_receive(endpoint, buf, sizeof(buf));
        for (i = 0; i < sizeof(buf); i++) {
          if (buf[i] != (i & 0xFF)) {
            failpos = i;
          } 
          buf[i] = 0;          
        }
        if (failpos >= 0) {
          x_set_task_status ("Second transfer failed: mismatch at position %d",failpos);
          x_sleep(10);
        }                
        x_set_task_status ("Second receive result %d", receive_result);
}

/* Synchronous send-receive speed tests. 

   19/10/2013 - Epiphany-III @ 600MHz
   Baseline overhead - call, transfer of parameters, sanity checking & 
   2 synchronisations - is 93nS (55 cycles) for buffer addresses that
   are global, 111ns (67 cycles) for local buffer addresses (this includes 
   15 cycle call-and-return overhead). 

   With fast.ldf and using memcpy to move the data, 40-byte transfers are
   glacially slow - 22500 cycles. 
   Better with e_dma_copy. The e_dma_copy call costs (for doubleword aligned data): 
      360 cycles for 4 bytes, 440 cycles for 512 bytes (base cost 141 loops).
   For sub-optimal alignment things start to look ugly
      1807 cycles for 511 bytes, 647 cycles for 508 bytes.
   The penalty for using less-than-doubleword sizes seems to be non-linear.
   Lesson: If receiver buffer is big enough and buffer positions are word
   or doubleword-aligned, transfer code should round up!
 
   Comparison of C-coded logic (loops/sec:cycles) and e_dma_copy.
   Each loop is 10000 repetitions of the transfer.
   Times recorded before amending the code to choose the transfer method. 

          |<------- c-coded -------->|   |<------- e_dma_copy ------>|
   bytes  byte al  word al  dw al        byte al  word al  dw al
     3               475 
     4               561
     8               530
     9               500
    16               530   525   
    32               450   477  
    64         153   355   431               115    141   141
    96                     377
   128          86   250   345                84    141   141
   129          86   238   333                84     84    84
   152          75                            74
   256          46   156   251                54    115   141
   512          24    90   161                32     84   115
   704                68                             66      
   768          16    63   118                23     66    97
  1024          12    49    94                18     54    84
  1280                      78                             74
  1408                      72                             66
  2048                      51.6                           54.5
  4096                      27                             32
  8192                      13.5                           17.5
  Lesson: for throughput (and approximately), e_dma_copy is best for byte 
  transfers > 152 bytes, words > 704 bytes, doublewords > 1408. 
    
  Transputer comparison
    T800@20MHz: 20Mbps links, FP add+mult 7+13 cycles. 
      Thus 20 bits transferred per FP add+mult result (best case!)
      In theory this could happen on all 4 links. 
    Thus with doubleword writes, theoretical performance of an Epiphany
      core is 64 bits in 2 cycles, 32 bits per cycle. However e_dma_copy
      is asymptotic at half that value, probably because the test for end
      of transfer is consuming half the RAM bandwidth. 
*/

void sync_send_speed_test(int connection_key)
{
        x_endpoint_handle_t endpoint = x_get_endpoint(connection_key);
        int send_result;
        char buf[8192];
        int i, j;
        int send_size = 1280;
        char* transfer_from = buf;
        
        for (i = 0; i < 10000; i++) {
          for (j = 0; j < 1000; j++) {
            send_result = x_sync_send(endpoint, transfer_from, send_size);
            send_result = x_sync_send(endpoint, transfer_from, send_size);
            send_result = x_sync_send(endpoint, transfer_from, send_size);
            send_result = x_sync_send(endpoint, transfer_from, send_size);
            send_result = x_sync_send(endpoint, transfer_from, send_size);
            send_result = x_sync_send(endpoint, transfer_from, send_size);
            send_result = x_sync_send(endpoint, transfer_from, send_size);
            send_result = x_sync_send(endpoint, transfer_from, send_size);
            send_result = x_sync_send(endpoint, transfer_from, send_size);
            send_result = x_sync_send(endpoint, transfer_from, send_size);
          }
          x_task_heartbeat();
        }
}

void sync_receive_speed_test(int connection_key)
{
        x_endpoint_handle_t endpoint = x_get_endpoint(connection_key);
        int receive_result;
        char buf[8192];
        int i, j;
        
        for (i = 0; i < 10000; i++) {
          for (j = 0; j < 1000; j++) {
            receive_result = x_sync_receive(endpoint, buf, sizeof(buf));
            receive_result = x_sync_receive(endpoint, buf, sizeof(buf));
            receive_result = x_sync_receive(endpoint, buf, sizeof(buf));
            receive_result = x_sync_receive(endpoint, buf, sizeof(buf));
            receive_result = x_sync_receive(endpoint, buf, sizeof(buf));
            receive_result = x_sync_receive(endpoint, buf, sizeof(buf));
            receive_result = x_sync_receive(endpoint, buf, sizeof(buf));
            receive_result = x_sync_receive(endpoint, buf, sizeof(buf));
            receive_result = x_sync_receive(endpoint, buf, sizeof(buf));
            receive_result = x_sync_receive(endpoint, buf, sizeof(buf));
          }
          x_task_heartbeat();
        }
}

#include <stdio.h>
int task_main(int argc, const char *argv[]) 
{
        e_coreid_t coreid;
        unsigned row, col;
        int wg_rows, wg_cols, my_row, my_col;
        coreid = e_get_coreid();
        e_coords_from_coreid (coreid, &row, &col);
        x_get_task_environment (&wg_rows, &wg_cols, &my_row, &my_col);
        x_set_task_status ("Hello from core 0x%03x (%d,%d) taskpos %d %d in %d %d", 
                           coreid, col, row, my_col, my_row, wg_cols, wg_rows);
        if (row == 1 && col == 1) {
          sync_send_test(X_TO_LEFT);
        }
        else if (row == 1 && col == 0) {
          sync_receive_test(X_FROM_RIGHT);      
        }
        else {
          x_sleep(30);
          x_set_task_status ("Goodbye from core 0x%03x (%d,%d) pid %d", 
                             coreid, col, row, my_col, my_row);
        }
        return X_SUCCESSFUL_TASK;
}
