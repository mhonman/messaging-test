/*
File: x_sync.c

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

#include <unistd.h>
#include <string.h>
#include "x_lib_configuration.h"
#include "x_types.h"
#include "x_error.h"
#include "x_endpoint.h"
#include "x_sync.h"
#include "x_connection_internals.h"
#include "x_task.h"

/* x_sync

  Quick synchronisation based on sequence numbers. See earlier versions of this
  module for the double-handshake x_sync code. This algorithm turned out to be
  a factor of 5 more efficient due to reduced need for sanity-checking.

  Algorithm:
    calculate next sequence number in the cycle (makes use of rollover in 
      unsigned arithmetic)
    write a control value to the peer indicating that this is a sync operation
    write new sequence number to the peer endpoint
    wait for the peer to write a matching sequence number into the local 
      endpoint
    check the control value written by the peer, flagging an error if the peer
      is not itself attempting a sync
      
  Note: 
    While using sequence and control values that 16 bits wide would permit
      assembly-language longword writes - saving one write-mesh transaction,
      this slows down the C code. 
    There is a possible race condition if the flow of control is interrupted
      between writing the new sequence value to the peer, and acting on data
      received from the peer. In that time window it is possible that the peer
      could already have begun another sync or transfer and updated the
      sequence number and control word. The work-around would be to disable
      interrupts for the duration of this call.       
      
  Things that slow this code down if implemented. 
    1. a control value of 0x8000 rather than 0 increases time by 5%
    2. If the error branch passes local_endpoint->control_from_peer as the
       int value, this slows normal execution by 5% even though the branch is
       not taken. 
    3. Simply returning X_ERROR in the error branch slows normal (non-error)
       execution time by a bit more than 5%. 
    4. Using a 16-bit sequence number is only 75% the speed of the optimal
       solution. 
    5. Masking two values together in a merged sequence+control 32-bit word
       is half the speed of a solution employing separate 32-bit words for
       these data. 
    6. Writing an additional 32-bit value to an adjacent peer slows 
       execution by 5%.     
*/

x_return_stat_t x_sync (x_endpoint_handle_t endpoint)
{
    x_endpoint_t                  *local_endpoint  = (x_endpoint_t*) endpoint;
    x_endpoint_t                  *remote_endpoint = local_endpoint->remote_endpoint;
    register x_transfer_sequence_t new_sequence = (local_endpoint->sequence + 1);
    x_transfer_control_t           control_from_peer;
    
    remote_endpoint->control_from_peer = X_ENDPOINT_SYNC_CONTROL;
    remote_endpoint->sequence_from_peer = new_sequence;
    while (local_endpoint->sequence_from_peer != new_sequence) { } ;
    control_from_peer = local_endpoint->control_from_peer;
    local_endpoint->sequence = new_sequence;
    if (control_from_peer != X_ENDPOINT_SYNC_CONTROL) {
        return x_error (X_E_SYNC_TRANSFER_MISMATCH, control_from_peer, endpoint);
    }
    else {
        return X_SUCCESS;
    }
}

/* x_sync_send

  Synchronous communication, sender side. 

  Will wait indefinitely for the peer to become ready. 

  Returns the amount of data actually transferred, or -1 on error. 

  If the supplied size is the same as a special flag an error is returned
  after synchronising with the peer.  

  Error conditions:
    The specified endpoint is not a Sending endpoint. 

  Global references: 
    The coreid bits that are needed to transform local into global addresses
      are picked up from x_global_address_local_coreid_bits.


  Algorithm:
    Synchronise with the receiver; transmitting transfer size and local
      buffer address (the latter for info only) and receiving destination
      address and buffer size. 
    If the destination buffer is too small, or the control value is a
      sync flag, exit with an appropriate error. 
    Otherwise if there is data to be transferred
      Transfer the data to the peer's destination buffer. 
      Synchronise again with the receiver to indicate completion of the
        transfer.
        
  Notes:
    * Conversion of local to global addresses is required (not strictly
      requred in the sender, but 
    * The receiver is expected to perform the same checks on the control
      word, thus the peers make the same decision as to whether the
      second synchronisation is performed. 
    * The size could be sanity-checked to ensure that is positive. 
    * The second sync with the receiver is needed so that the receiver can
      be informed of transfer completion while avoiding race conditions. 
      An alternative would be to introduce yet another coordination flag
      in the endpoint that would hold the sequence number of the last 
      completed transfer. TEST PERFORMANCE of this alternative!
    * The potential race condition described in the x_sync comments should
      not arise because of the second sync - provided that the control and
      address values written to the peer are left unaltered. 
    * it is hard to believe, but masking in the global_address_local_coreid
      bits costs 12 cycles over and above the cost of writing the address
      to the other core. On the other hand the assembly approach used in
      e_get_coreid has an additional overhead of 27 cycles.
    * It is slightly faster to test whether an address is global by shifting 
      it right 20 bits, than using a bitmask.     

  Data transfer (copying) logic - the reasoning
    * Epiphany multibyte load/store instructions require that data be aligned
      in memory according to data size. 
    * For sub-optimal start and end offsets, it is possible to ramp up to
      doubleword transfers, and ramp down at the end. 
    * When the source and destination addresses have different alignments,
      this limits the biggest load/store operation size.
      
  Data copying algorithm:
    Since there are a lot of cases to check, the tests are structured to
    favour the most common case, word-aligned data (which may turn out to be
    doubleword-aligned, but due to the absence of double precision floating
    point support, naturally doubleword-aligned data are rare). 
    Doing too many tests increases the overhead, limiting the benefits of
    special-case coding. 
    
    If send and receive buffers are word-aligned
       If size is >= 32 and both buffers have the same even or odd word 
       alignment, a doubleword transfer is worthwhile
         If both buffer addresses are not doubleword-aligned
           Copy the first word
         while size remaining >= 32 
           Copy double words in a 4x unrolled loop
       while size remaining >= 16
           Copy words in a 4x unrolled loop
       while size remaining >= 4
           Copy a word
       for size remaining 
           Copy a byte
    Else (buffers not word-aligned) 
      While size remaining >= 8
         Copy bytes in an 8x unrolled loop
      for size remaining
         copy a byte
*/

int x_sync_send (x_endpoint_handle_t endpoint, const void * buf, 
                 x_transfer_size_t size)
{
    int                            result = -1;
    x_endpoint_t                  *local_endpoint  = (x_endpoint_t*) endpoint;
    x_endpoint_t                  *remote_endpoint = local_endpoint->remote_endpoint;
    register x_transfer_sequence_t new_sequence = (local_endpoint->sequence + 1);
    x_transfer_control_t           size_from_peer;

    if (local_endpoint->mode != X_SENDING_ENDPOINT) {
        x_error (X_E_ENDPOINT_MODE_MISMATCH, 0, endpoint);
    }
    else {
        if ((((x_transfer_address_t)buf) >> 20) == 0) {
            remote_endpoint->address_from_peer = 
                ((x_transfer_address_t)buf) | x_global_address_local_coreid_bits; 
        }
        else {
            remote_endpoint->address_from_peer = (x_transfer_address_t)buf;
        }
        remote_endpoint->control_from_peer  = size;
        remote_endpoint->sequence_from_peer = new_sequence;

        while (local_endpoint->sequence_from_peer != new_sequence) { } ;
        size_from_peer = local_endpoint->control_from_peer;
        local_endpoint->sequence = new_sequence;
        if (size == X_ENDPOINT_SYNC_CONTROL) {
            x_error (X_E_INVALID_TRANSFER_SIZE, size, endpoint);
        }
        else if (size_from_peer == X_ENDPOINT_SYNC_CONTROL) {
            x_error (X_E_SYNC_TRANSFER_MISMATCH, size_from_peer, endpoint);
        }        
        else if (size_from_peer < size) {
            x_error (X_E_SEND_TOO_BIG_FOR_RECEIVE_BUFFER, size_from_peer, endpoint);
        }
        else {
//            if ( size < X_MESSAGING_MIN_DMA_SIZE ) {
//               e_dma_copy((void*)local_endpoint->address_from_peer, buf, size);
            register char * src_ptr  = (char*)buf;
            register char * dest_ptr = (char*)local_endpoint->address_from_peer;
            register char * after_end_ptr  = dest_ptr + size;
            if ((((uint32_t)buf | (uint32_t)local_endpoint->address_from_peer) & 0x3) == 0) {
                // Word-aligned happy zone, maybe doubleword transfers can be done
                if ((size >= 32) && 
                    ((((uint32_t)src_ptr ^ (uint32_t)dest_ptr) & 0x4) == 0)) {
                    if ((uint32_t)dest_ptr & 0x4) {
                        *((uint32_t*)dest_ptr) = *((uint32_t*)src_ptr);
                        src_ptr  += 4;
                        dest_ptr += 4;
                    }
                    while (after_end_ptr - dest_ptr >= 32) {
                        *((uint64_t*)dest_ptr)   = *((uint64_t*)src_ptr);
                        *((uint64_t*)dest_ptr+1) = *((uint64_t*)src_ptr+1);
                        *((uint64_t*)dest_ptr+2) = *((uint64_t*)src_ptr+2);
                        *((uint64_t*)dest_ptr+3) = *((uint64_t*)src_ptr+3);
                        src_ptr  += 32;
                        dest_ptr += 32;
                    }
                }
                while (after_end_ptr - dest_ptr >= 16) {
                    *((uint32_t*)dest_ptr)   = *((uint32_t*)src_ptr);
                    *((uint32_t*)dest_ptr+1) = *((uint32_t*)src_ptr+1);
                    *((uint32_t*)dest_ptr+2) = *((uint32_t*)src_ptr+2);
                    *((uint32_t*)dest_ptr+3) = *((uint32_t*)src_ptr+3);
                    src_ptr  += 16;
                    dest_ptr += 16;
                }
                while (after_end_ptr - dest_ptr >= 4) {
                    *((uint32_t*)dest_ptr) = *((uint32_t*)src_ptr);
                    src_ptr  += 4;
                    dest_ptr += 4;
                }
                while (after_end_ptr - dest_ptr >= 1) {
                    *dest_ptr++ = *src_ptr++;
                }
            }
            else {
                // not word-aligned, do the best possible with bytewise copy
                while (after_end_ptr - dest_ptr >= 8) {
                    *dest_ptr++ = *src_ptr++;
                    *dest_ptr++ = *src_ptr++;
                    *dest_ptr++ = *src_ptr++;
                    *dest_ptr++ = *src_ptr++;
                    *dest_ptr++ = *src_ptr++;
                    *dest_ptr++ = *src_ptr++;
                    *dest_ptr++ = *src_ptr++;
                    *dest_ptr++ = *src_ptr++;
                }
                while (after_end_ptr - dest_ptr >= 1) {
                    *dest_ptr++ = *src_ptr++;
                }
            }  
            new_sequence++;
            remote_endpoint->sequence_from_peer = new_sequence;
            while (local_endpoint->sequence_from_peer != new_sequence) { } ;
            local_endpoint->sequence = new_sequence;
            result = size;
        }    
    }
    return result;
}

/* x_sync_receive

  Synchronous communication, receiving side. 

  Will wait indefinitely for the peer to become ready. 

  Returns the amount of data actually transferred, zero in the case of a
  sync from the sender, or -1 on error. 

  Error conditions:
    The specified endpoint is not a Receiving endpoint. 

  Global references: 
    The coreid bits that are needed to transform local into global addresses
      are picked up from x_global_address_local_coreid_bits.

  Algorithm:
    check endpoint mode.
    write size and global address of destination buffer to receiver status 
      in peer's endpoint
    synchronise with the sender - after this syncronisation the control word
      and address sent by the peer are valid and the transfer size in the 
      control word can be validated against the size of the destination area.
    If the destination buffer is too small, or the control value is a
      sync flag, exit with an appropriate error. 
    Otherwise if there is data to be transferred
      synchronise again with the sender - this will only occur once the
        sender has completed the transfer. 

  Notes: the sender is expected to perform the transfer, as writing from one
    core to another is faster than reading. 

*/

int x_sync_receive (x_endpoint_handle_t endpoint, void * buf, 
                    x_transfer_size_t size)
{
    int                            result = -1;
    x_endpoint_t                  *local_endpoint  = (x_endpoint_t*) endpoint;
    x_endpoint_t                  *remote_endpoint = local_endpoint->remote_endpoint;
    register x_transfer_sequence_t new_sequence = (local_endpoint->sequence + 1);
    x_transfer_control_t           size_from_peer;

    if (local_endpoint->mode != X_RECEIVING_ENDPOINT) {
        x_error (X_E_ENDPOINT_MODE_MISMATCH, 0, endpoint);
    }
    else {
        if ((((x_transfer_address_t)buf) >> 20) == 0) {
            remote_endpoint->address_from_peer = 
                ((x_transfer_address_t)buf) | x_global_address_local_coreid_bits; 
        }
        else {
            remote_endpoint->address_from_peer = (x_transfer_address_t)buf;
        }
        remote_endpoint->control_from_peer  = size;
        remote_endpoint->sequence_from_peer = new_sequence;

        while (local_endpoint->sequence_from_peer != new_sequence) { } ;
        size_from_peer = local_endpoint->control_from_peer;
        local_endpoint->sequence = new_sequence;
        if (size == X_ENDPOINT_SYNC_CONTROL) {
            x_error (X_E_INVALID_TRANSFER_SIZE, size, endpoint);
        }
        else if (size_from_peer == X_ENDPOINT_SYNC_CONTROL) {
            x_error (X_E_SYNC_TRANSFER_MISMATCH, size_from_peer, endpoint);
        }        
        else if (size_from_peer > size) {
            x_error (X_E_SEND_TOO_BIG_FOR_RECEIVE_BUFFER, size_from_peer, endpoint);
        }
        else {
            new_sequence++;
            remote_endpoint->sequence_from_peer = new_sequence;
            while (local_endpoint->sequence_from_peer != new_sequence) { } ;
            local_endpoint->sequence = new_sequence;
            result = size_from_peer;
        }    
    }
    return result;
}

