#include "message_queue.h"
#include <stdlib.h>
#include <stddef.h>
#include <stdbool.h>

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
		do { __sync_synchronize(); } while(*lock);
	}
}

static inline void spinlock_unlock(int_fast8_t *lock) {
	__sync_lock_release(lock);
}

int message_queue_init(struct message_queue *queue, int message_size) {
	queue->allocator.freelist = NULL;
	queue->allocator.lock = 0;
	queue->allocator.message_size = message_size;
	queue->queue.head = NULL;
	queue->queue.tail = &queue->queue.head;
	queue->queue.lock = 0;
}

void *message_queue_message_alloc(struct message_queue *queue) {
	struct queue_ent *rv = queue->allocator.freelist;
	while(rv) {
		spinlock_lock(&queue->allocator.lock);
		rv = queue->allocator.freelist;
		if(rv) {
			queue->allocator.freelist = rv->next;
			spinlock_unlock(&queue->allocator.lock);
			return &rv->user_data;
		}
		spinlock_unlock(&queue->allocator.lock);
	}
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
	spinlock_unlock(&queue->queue.lock);
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

int message_queue_destroy(struct message_queue *queue) {
	struct queue_ent *head = queue->allocator.freelist;
	while(head) {
		struct queue_ent *next = head->next;
		free(head);
		head = next;
	}
}
