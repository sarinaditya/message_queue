#ifndef MESSAGE_QUEUE_H
#define MESSAGE_QUEUE_H

#ifndef CACHE_LINE_SIZE
#define CACHE_LINE_SIZE 64
#endif

/**
 * \brief Message queue structure
 *
 * This structure is passed to all message_queue API calls
 */
struct message_queue {
	unsigned int message_size;
	unsigned int max_depth;
	void *memory;
	struct {
		void **freelist;
		unsigned int freelist_size;
		unsigned int free_blocks;
		unsigned int allocpos;
		unsigned int freepos;
	} allocator;
};

/**
 * \brief Initialize a message queue structure
 *
 * This function must be called before any other message_queue API calls on a
 * message queue structure.
 *
 * \param queue pointer to the message queue structure to initialize
 * \param message_size size in bytes of the largest message that will be sent
 *        on this queue
 * \param max_depth the maximum number of message to allow in the queue at
 *        once
 *
 * \return 0 if successful, or nonzero if an error occured
 */
int message_queue_init(struct message_queue *queue, int message_size, int max_depth);

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
 * \brief Destroy a message queue structure
 *
 * This frees any resources associated with the message queue.
 *
 * \param queue pointer to the message queue to destroy
 */
void message_queue_destroy(struct message_queue *queue);

#endif
