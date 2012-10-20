#ifndef MESSAGE_QUEUE_H
#define MESSAGE_QUEUE_H
#include <stdint.h>
#include <semaphore.h>

struct message_queue {
	struct queue_ent *freelist;
	int_fast8_t freelist_lock;
	int message_size;
	struct queue_ent *queue_head;
	struct queue_ent **queue_tail;
	int_fast8_t queue_lock;
	int blocked_readers;
	sem_t *sem;
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
int message_queue_destroy(struct message_queue *queue);

#ifdef __cplusplus
}
#endif

#endif
