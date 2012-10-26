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
	queue->max_depth = round_to_pow2(max_depth);
	queue->memory = malloc(queue->message_size * max_depth);
	if(!queue->memory)
		goto error;
	queue->allocator.freelist = malloc(sizeof(void *) * queue->max_depth);
	if(!queue->allocator.freelist)
		goto error_after_memory;
	for(int i=0;i<queue->max_depth;++i) {
		queue->allocator.freelist[i] = queue->memory + (sizeof(void *) * i);
	}
	queue->allocator.free_blocks = queue->max_depth;
	queue->allocator.allocpos = 0;
	queue->allocator.freepos = 0;
	queue->queue.queue = malloc(sizeof(void *) * queue->max_depth);
	if(!queue->queue.queue)
		goto error_after_freelist;
	for(int i=0;i<queue->max_depth;++i) {
		queue->queue.queue[i] = NULL;
	}
	queue->queue.entries = 0;
	queue->queue.readpos = 0;
	queue->queue.writepos = 0;
	return 0;

error_after_freelist:
	free(queue->allocator.freelist);
error_after_memory:
	free(queue->memory);
error:
	return -1;
}

void *message_queue_message_alloc(struct message_queue *queue) {
	if(__sync_fetch_and_add(&queue->allocator.free_blocks, -1)) {
		unsigned int pos = __sync_fetch_and_add(&queue->allocator.allocpos, 1) % queue->max_depth;
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
	unsigned int pos = __sync_fetch_and_add(&queue->allocator.freepos, 1) % queue->max_depth;
	void *cur = queue->allocator.freelist[pos];
	while(cur) {
		usleep(10); __sync_synchronize();
		cur = queue->allocator.freelist[pos];
	}
	queue->allocator.freelist[pos] = message;
	__sync_fetch_and_add(&queue->allocator.free_blocks, 1);
}

void message_queue_write(struct message_queue *queue, void *message) {
	unsigned int pos = __sync_fetch_and_add(&queue->queue.writepos, 1) % queue->max_depth;
	void *cur = queue->queue.queue[pos];
	while(cur) {
		usleep(10); __sync_synchronize();
		cur = queue->queue.queue[pos];
	}
	queue->queue.queue[pos] = message;
	__sync_fetch_and_add(&queue->queue.entries, 1);
}

void *message_queue_tryread(struct message_queue *queue) {
	if(__sync_fetch_and_add(&queue->queue.entries, -1)) {
		unsigned int pos = __sync_fetch_and_add(&queue->queue.readpos, 1) % queue->max_depth;
		void *rv = queue->queue.queue[pos];
		while(!rv) {
			usleep(10); __sync_synchronize();
			rv = queue->queue.queue[pos];
		}
		queue->queue.queue[pos] = NULL;
		return rv;
	}
	__sync_fetch_and_add(&queue->queue.entries, 1);
	return NULL;
}

void message_queue_destroy(struct message_queue *queue) {
	free(queue->allocator.freelist);
	free(queue->memory);
}
