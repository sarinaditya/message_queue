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
#include <stdlib.h>
#include <stddef.h>
#include <stdbool.h>
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <sched.h>

struct queue_ent {
	struct queue_ent *next;
	union {
		char chardata;
		short shortdata;
		int intdata;
		long longdata;
		float floatdata;
		double doubledata;
		void *pointerdata;
	} user_data;
};

static inline void spinlock_lock(int_fast8_t *lock) {
	while(__sync_lock_test_and_set(lock, 1)) {
		do { sched_yield(); __sync_synchronize(); } while(*lock);
	}
}

static inline void spinlock_unlock(int_fast8_t *lock) {
	__sync_lock_release(lock);
}

int message_queue_init(struct message_queue *queue, int message_size) {
	char sem_name[128];
	queue->allocator.freelist = NULL;
	queue->allocator.lock = 0;
	queue->allocator.message_size = message_size;
	queue->queue.head = NULL;
	queue->queue.tail = &queue->queue.head;
	queue->queue.lock = 0;
	queue->queue.blocked_readers = 0;
	snprintf(sem_name, 128, "%d_%p", getpid(), queue);
	sem_name[127] = '\0';
	queue->queue.sem = sem_open(sem_name, O_CREAT | O_EXCL, 0600);
	sem_unlink(sem_name);
}

void *message_queue_message_alloc(struct message_queue *queue) {
	struct queue_ent *rv;
	spinlock_lock(&queue->allocator.lock);
	rv = queue->allocator.freelist;
	if(rv) {
		queue->allocator.freelist = rv->next;
		spinlock_unlock(&queue->allocator.lock);
		return &rv->user_data;
	}
	spinlock_unlock(&queue->allocator.lock);
	rv = malloc(queue->allocator.message_size + offsetof(struct queue_ent, user_data));
	return &rv->user_data;
}

void message_queue_message_free(struct message_queue *queue, void *message) {
	struct queue_ent *x = message - offsetof(struct queue_ent, user_data);
	spinlock_lock(&queue->allocator.lock);
	x->next = queue->allocator.freelist;
	queue->allocator.freelist = x;
	spinlock_unlock(&queue->allocator.lock);
}

void message_queue_write(struct message_queue *queue, void *message) {
	struct queue_ent *x = message - offsetof(struct queue_ent, user_data);
	spinlock_lock(&queue->queue.lock);
	x->next = NULL;
	*queue->queue.tail = x;
	queue->queue.tail = &x->next;
	if(queue->queue.blocked_readers) {
		--queue->queue.blocked_readers;
		spinlock_unlock(&queue->queue.lock);
		sem_post(queue->queue.sem);
	} else {
		spinlock_unlock(&queue->queue.lock);
	}
}

void *message_queue_tryread(struct message_queue *queue) {
	struct queue_ent *rv = queue->queue.head;
	while(rv) {
		spinlock_lock(&queue->queue.lock);
		rv = queue->queue.head;
		if(rv) {
			queue->queue.head = rv->next;
			if(!rv->next)
				queue->queue.tail = &queue->queue.head;
			spinlock_unlock(&queue->queue.lock);
			return &rv->user_data;
		}
		spinlock_unlock(&queue->queue.lock);
	}
	return NULL;
}

void *message_queue_read(struct message_queue *queue) {
	while(true) {
		for(int i=9;i>=0;--i) {
			struct queue_ent *rv = queue->queue.head;
			if(!rv) {
				if(!i) {
					spinlock_lock(&queue->queue.lock);
					++queue->queue.blocked_readers;
					spinlock_unlock(&queue->queue.lock);
					break;
				}
				__sync_synchronize();
				continue;
			}
			spinlock_lock(&queue->queue.lock);
			rv = queue->queue.head;
			if(rv) {
				queue->queue.head = rv->next;
				if(!rv->next)
					queue->queue.tail = &queue->queue.head;
				spinlock_unlock(&queue->queue.lock);
				return &rv->user_data;
			}
			if(!i) {
				++queue->queue.blocked_readers;
			}
			spinlock_unlock(&queue->queue.lock);
		}
		sem_wait(queue->queue.sem);
	}
}

int message_queue_destroy(struct message_queue *queue) {
	struct queue_ent *head = queue->allocator.freelist;
	while(head) {
		struct queue_ent *next = head->next;
		free(head);
		head = next;
	}
	sem_close(queue->queue.sem);
}
