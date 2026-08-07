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
    struct spinlock lock;
};

int pipe_read(struct pipe_data *pipe, void *buf, int count);
int pipe_write(struct pipe_data *pipe, const void *buf, int count);
void pipe_close(struct pipe_data *pipe, bool writer);

#endif
