/*
 * Copyright (c) 2012 Jeremy Pepper
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 *  * Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 *  * Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *  * Neither the name of message_queue nor the names of its contributors may
 *    be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include "message_queue.h"
#include <inttypes.h>
#include <stdlib.h>
#include <unistd.h>

union padding {
	char chardata;
	short shortdata;
	int intdata;
	long longdata;
	float floatdata;
	double doubledata;
	void *pointerdata;
};

static inline int pad_size(int size) {
	return size % sizeof(union padding) ?
	       (size + (sizeof(union padding) - (size % sizeof(union padding)))) :
		   size;
}

static inline uint32_t round_to_pow2(uint32_t x) {
	x--;
	x |= x >> 1;
	x |= x >> 2;
	x |= x >> 4;
	x |= x >> 8;
	x |= x >> 16;
	x++;
	return x;
}

static inline int max(int x, int y) {
	return x > y ? x : y;
}

int message_queue_init(struct message_queue *queue, int message_size, int max_depth) {
	queue->message_size = pad_size(message_size);
	queue->max_depth = max_depth;
	queue->memory = malloc(queue->message_size * max_depth);
	if(!queue->memory)
		goto error;
	queue->allocator.freelist_size = round_to_pow2(max_depth);
	queue->allocator.freelist = malloc(sizeof(void *) * queue->allocator.freelist_size);
	if(!queue->allocator.freelist)
		goto error_after_memory;
	for(int i=0;i<max_depth;++i) {
		queue->allocator.freelist[i] = queue->memory + (sizeof(void *) * i);
	}
	for(int i=max_depth;i<queue->allocator.freelist_size;++i) {
		queue->allocator.freelist[i] = NULL;
	}
	queue->allocator.free_blocks = max_depth;
	queue->allocator.allocpos = 0;
	queue->allocator.freepos = queue->max_depth;
	return 0;

error_after_memory:
	free(queue->memory);
error:
	return -1;
}

void *message_queue_message_alloc(struct message_queue *queue) {
	if(__sync_fetch_and_add(&queue->allocator.free_blocks, -1)) {
		unsigned int pos = __sync_fetch_and_add(&queue->allocator.allocpos, 1) % queue->allocator.freelist_size;
		void *rv = queue->allocator.freelist[pos];
		while(!rv) {
			usleep(10); __sync_synchronize();
			rv = queue->allocator.freelist[pos];
		}
		queue->allocator.freelist[pos] = NULL;
		return rv;
	}
	__sync_fetch_and_add(&queue->allocator.free_blocks, 1);
	return NULL;
}

void message_queue_message_free(struct message_queue *queue, void *message) {
	unsigned int pos = __sync_fetch_and_add(&queue->allocator.freepos, 1) % queue->allocator.freelist_size;
	void *cur = queue->allocator.freelist[pos];
	while(cur) {
		usleep(10); __sync_synchronize();
		cur = queue->allocator.freelist[pos];
	}
	queue->allocator.freelist[pos] = message;
	__sync_fetch_and_add(&queue->allocator.free_blocks, 1);
}

void message_queue_destroy(struct message_queue *queue) {
	free(queue->allocator.freelist);
	free(queue->memory);
}
