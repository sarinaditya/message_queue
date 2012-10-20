#include "message_queue.h"
#include <stdlib.h>
#include <stddef.h>
#include <stdbool.h>
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>

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
	char sem_name[128];
	queue->freelist = NULL;
	queue->freelist_lock = 0;
	queue->message_size = message_size;
	queue->queue_head = NULL;
	queue->queue_tail = &queue->queue_head;
	queue->queue_lock = 0;
	queue->blocked_readers = 0;
	snprintf(sem_name, 128, "%d_%p", getpid(), queue);
	sem_name[127] = '\0';
	queue->sem = sem_open(sem_name, O_CREAT | O_EXCL, 0600);
	sem_unlink(sem_name);
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

void message_queue_write(struct message_queue *queue, void *message) {
	struct queue_ent *x = message - offsetof(struct queue_ent, user_data);
	spinlock_lock(&queue->queue_lock);
	x->next = NULL;
	*queue->queue_tail = x;
	queue->queue_tail = &x->next;
	if(queue->blocked_readers) {
		--queue->blocked_readers;
		spinlock_unlock(&queue->queue_lock);
		sem_post(queue->sem);
	} else {
		spinlock_unlock(&queue->queue_lock);
	}
}

void *message_queue_tryread(struct message_queue *queue) {
	struct queue_ent *rv = queue->queue_head;
	while(rv) {
		spinlock_lock(&queue->queue_lock);
		rv = queue->queue_head;
		if(rv) {
			queue->queue_head = rv->next;
			if(!rv->next)
				queue->queue_tail = &queue->queue_head;
			spinlock_unlock(&queue->queue_lock);
			return &rv->user_data;
		}
		spinlock_unlock(&queue->queue_lock);
	}
	return NULL;
}

void *message_queue_read(struct message_queue *queue) {
	while(true) {
		for(int i=9;i>=0;--i) {
			struct queue_ent *rv = queue->queue_head;
			if(!rv) {
				if(!i) {
					spinlock_lock(&queue->queue_lock);
					++queue->blocked_readers;
					spinlock_unlock(&queue->queue_lock);
					break;
				}
				__sync_synchronize();
				continue;
			}
			spinlock_lock(&queue->queue_lock);
			rv = queue->queue_head;
			if(rv) {
				queue->queue_head = rv->next;
				if(!rv->next)
					queue->queue_tail = &queue->queue_head;
				spinlock_unlock(&queue->queue_lock);
				return &rv->user_data;
			}
			if(!i) {
				++queue->blocked_readers;
			}
			spinlock_unlock(&queue->queue_lock);
		}
		sem_wait(queue->sem);
	}
}

int message_queue_destroy(struct message_queue *queue) {
	struct queue_ent *head = queue->freelist;
	while(head) {
		struct queue_ent *next = head->next;
		free(head);
		head = next;
	}
	sem_close(queue->sem);
}
