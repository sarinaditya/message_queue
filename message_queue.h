#ifndef MESSAGE_QUEUE_H
#define MESSAGE_QUEUE_H
#include <stdint.h>

struct message_queue {
	struct queue_ent *freelist;
	int_fast8_t freelist_lock;
	int message_size;
};

#ifdef __cplusplus
extern "C" {
#endif

int message_queue_init(struct message_queue *queue, int message_size);
void *message_queue_message_alloc(struct message_queue *queue);
void message_queue_message_free(struct message_queue *queue, void *message);
void message_queue_destroy(struct message_queue *queue);

#ifdef __cplusplus
}
#endif

#endif
