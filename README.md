# Introduction

I've always longed for a good, _fast_ way to relay information between
threads. So, I built one!

What's a message? It's anything you want it to be: a built-in data type, a
character string, a function pointer, or a complicated data structure. The
queue won't copy or move your structure, so internal pointers remain intact. A
message is anything your application wants to send between threads.

# How does it work?

The library uses a slab allocator protected by a spinlock`to allocate memory
for messages. Then, your application can construct whatever it needs to send
in-place. The library never copies it--instead, it reserves a few bytes before
the pointer it returns to track the message internally. When you write the
message to the queue, it's added to the end of a linked list, protected by a
separate spinlock.

# Why should I use this?

* It's fast. Crazy fast. My three-year-old laptop can push over 6,000,000
  messages per second between threads, _including_ the overhead of allocating
  the messages.
* It's easy. There are only 7 functions to learn, and you probably only need 6
  of them. Really, there are only 3 concepts to worry about:
  * initialization/teardown,
  * allocation/deallocation, and
  * writing/reading.

  If you're a C programmer, you've dealt with all of these already.

# Why shouldn't I use this?

* It's new and so not widely tested. In fact, it's only been tested at all on
  two x86_64 machines, running Mac OS X and Linux.
* I have no clue how well it scales past two CPUs. Anyone want to try it on a
  bigger, beefier machine?
* You have to know how big the largest message you want to send on a given
  queue is in advance. This is the cost of using a trivial slab allocator to
  make allocation faster.

# How do I use this?

First, set up a message queue somewhere:

    struct message_queue queue;

Before using it, you have to initialize it:

    message_queue_init(&queue, 512); /* The biggest message we'll send with
                                      * this queue is 512 bytes */

To send a message:

    struct my_message *message = message_queue_message_alloc(&queue);
    /* Construct the message here */
    message_queue_write(&queue, message);

To read a message:

	/* Blocks until a message is available */
    struct my_message *message = message_queue_read(&queue);
    /* Do something with the message here */
    message_queue_message_free(&queue, message);

If you'd rather not block to wait for a new message:

    /* Returns NULL if no message is available */
    struct my_message *message = message_queue_tryread(&queue);
    if(message) {
        /* Do something with the message here */
        message_queue_message_free(&queue, message);
    }

Whenever you're done with the queue (and no other threads are accessing it
anymore):

    message_queue_destroy(&queue);

So give it a shot and let me know what you think!
