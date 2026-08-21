#ifndef PIPE_H
#define PIPE_H

#include <include/types.h>
#include <locks/spinlock.h>

#define PIPE_BUFFER_SIZE 4096

struct pipe_data {
    uint8_t buffer[PIPE_BUFFER_SIZE];
    uint32_t read_pos;
    uint32_t write_pos;
    uint32_t used;
    int readers;
    int writers;
    /* A zero condition cancels a pending scheduler sleep after readiness or
     * endpoint closure, closing the unlock-to-sleep lost-wakeup window. */
    int read_wait;
    int write_wait;
    struct spinlock lock;
};

int pipe_read(struct pipe_data *pipe, void *buf, int count);
int pipe_write(struct pipe_data *pipe, const void *buf, int count);
void pipe_close(struct pipe_data *pipe, bool writer);
void pipe_add_ref(struct pipe_data *pipe, bool writer);
void pipe_status(struct pipe_data *pipe, bool writer,
                 uint32_t *readable, uint32_t *writable, uint32_t *hung_up);

#endif
