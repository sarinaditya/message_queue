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

#ifndef MESSAGE_QUEUE_H
#define MESSAGE_QUEUE_H
#include <stdint.h>
#include <semaphore.h>

#ifndef CACHE_LINE_SIZE
#define CACHE_LINE_SIZE 64
#endif

struct message_queue {
	struct {
		struct queue_ent *freelist;
		int_fast8_t lock;
		int message_size;
	} allocator;
	struct {
		struct queue_ent *head;
		struct queue_ent **tail;
		int_fast8_t lock;
		int blocked_readers;
		sem_t *sem;
	} queue __attribute__((aligned(CACHE_LINE_SIZE)));
};

#ifdef __cplusplus
extern "C" {
#endif

int message_queue_init(struct message_queue *queue, int message_size);
void *message_queue_message_alloc(struct message_queue *queue);
void message_queue_message_free(struct message_queue *queue, void *message);
void message_queue_write(struct message_queue *queue, void *message);
void *message_queue_tryread(struct message_queue *queue);
void *message_queue_read(struct message_queue *queue);
void message_queue_destroy(struct message_queue *queue);

#ifdef __cplusplus
}
#endif

#endif
