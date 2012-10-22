#ifndef MESSAGE_QUEUE_H
#define MESSAGE_QUEUE_H
#include <stdint.h>

/**
 * \brief Message queue structure
 *
 * This structure is passed to all message_queue API calls
 */
struct message_queue {
	struct queue_ent *freelist;
	int_fast8_t freelist_lock;
	int message_size;
	struct queue_ent *queue_head;
	struct queue_ent **queue_tail;
	int_fast8_t queue_lock;
};

#ifdef __cplusplus
extern "C" {
#endif

/**
 * \brief Initialize a message queue structure
 *
 * This function must be called before any other message_queue API calls on a
 * message queue structure.
 *
 * \param queue pointer to the message queue structure to initialize
 * \param message_size size in bytes of the largest message that will be sent
 *        on this queue
 *
 * \return 0 if successful, or nonzero if an error occured
 */
int message_queue_init(struct message_queue *queue, int message_size);

/**
 * \brief Allocate a new message
 *
 * This allocates message_size bytes to be used with this queue. Messages
 * passed to the queue MUST be allocated with this function.
 *
 * \param queue pointer to the message queue to which the message will be
 *        written
 * \return pointer to the allocated message, or NULL if no memory is available
 */
void *message_queue_message_alloc(struct message_queue *queue);

/**
 * \brief Free a message
 *
 * This returns the message to the queue's freelist to be reused to satisfy
 * future allocations. This function MUST be used to free messages--they
 * cannot be passed to free().
 *
 * \param queue pointer to the message queue from which the message was
 *        allocated
 * \param message pointer to the message to be freed
 */
void message_queue_message_free(struct message_queue *queue, void *message);

/**
 * \brief Write a message to the queue
 *
 * Messages must have been allocated from the same queue by
 * message_queue_message_alloc to be passed to this function.
 *
 * \param queue pointer to the queue to which to write
 * \param message pointer to the message to write to the queue
 */
void message_queue_write(struct message_queue *queue, void *message);

/**
 * \brief Read a message from the queue if one is available
 *
 * \param queue pointer to the queue from which to read
 * \return pointer to the next message on the queue, or NULL if no messages
 *         are available.
 */
void *message_queue_tryread(struct message_queue *queue);

/**
 * \brief Destroy a message queue structure
 *
 * This frees any resources associated with the message queue.
 *
 * \param queue pointer to the message queue to destroy
 */
void message_queue_destroy(struct message_queue *queue);

#ifdef __cplusplus
}
#endif

#endif
