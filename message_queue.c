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
	queue->freelist = NULL;
	queue->freelist_lock = 0;
	queue->message_size = message_size;
}

void *message_queue_message_alloc(struct message_queue *queue) {
	struct queue_ent *rv;
	spinlock_lock(&queue->freelist_lock);
	rv = queue->freelist;
	if(rv) {
		queue->freelist = rv->next;
		spinlock_unlock(&queue->freelist_lock);
		return &rv->user_data;
	}
	spinlock_unlock(&queue->freelist_lock);
	rv = malloc(queue->message_size + offsetof(struct queue_ent, user_data));
	return &rv->user_data;
}

void message_queue_message_free(struct message_queue *queue, void *message) {
	struct queue_ent *x = message - offsetof(struct queue_ent, user_data);
	spinlock_lock(&queue->freelist_lock);
	x->next = queue->freelist;
	queue->freelist = x;
	spinlock_unlock(&queue->freelist_lock);
}

int message_queue_destroy(struct message_queue *queue) {
	struct queue_ent *head = queue->freelist;
	while(head) {
		struct queue_ent *next = head->next;
		free(head);
		head = next;
	}
}
