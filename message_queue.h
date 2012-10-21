#ifndef MESSAGE_QUEUE_H
#define MESSAGE_QUEUE_H
#include <stdint.h>

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
int message_queue_destroy(struct message_queue *queue);

#ifdef __cplusplus
}
#endif

#endif
