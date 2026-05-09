/* -----------------------------------------------------------------------
 * GGPO.net (http://ggpo.net)  -  Copyright 2009 GroundStorm Studios, LLC.
 *
 * Use of this software is governed by the MIT license that can be found
 * in the LICENSE file.
 */

#ifndef _RING_BUFFER_H
#define _RING_BUFFER_H

#include "types.h"

struct RingBuffer
{
   int      _head;
   int      _tail;
   int      _size;
   int _N;
};
typedef struct RingBuffer RingBuffer;

inline void ring_ctor(RingBuffer* ring, int N)
{
    ring->_head = 0;
    ring->_tail = 0;
    ring->_size = 0;
    ring->_N = N;
}

inline int ring_front(RingBuffer* ring) {
	ASSERT(ring->_size != ring->_N);
	return ring->_tail;
}

inline int  ring_item(RingBuffer* ring, int i) {
	ASSERT(i < ring->_size);
	return (ring->_tail + i) % ring->_N;
}

inline void  ring_pop(RingBuffer* ring) {
	ASSERT(ring->_size != ring->_N);
	ring->_tail = (ring->_tail + 1) % ring->_N;
	ring->_size--;
}

inline int  ring_push(RingBuffer* ring) {
	ASSERT(ring->_size != (ring->_N - 1));
	int result = ring->_head;
	ring->_head = (ring->_head + 1) % ring->_N;
	ring->_size++;
    return result;
}

inline int  ring_size(RingBuffer* ring) {
	return ring->_size;
}

inline bool  ring_empty(RingBuffer* ring) {
	return ring->_size == 0;
}

#endif
