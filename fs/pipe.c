#include <fs/pipe.h>
#include <mm/mm.h>
#include <sched/sched.h>

int pipe_read(struct pipe_data *pipe, void *buf, int count)
{
    int copied = 0;
    if (pipe == NULL || buf == NULL || count < 0)
        return -1;
    while (copied == 0) {
        acquire_spinlock(&pipe->lock);
        while (copied < count && pipe->used > 0) {
            ((uint8_t *)buf)[copied++] = pipe->buffer[pipe->read_pos];
            pipe->read_pos = (pipe->read_pos + 1) % PIPE_BUFFER_SIZE;
            pipe->used--;
        }
        if (copied > 0 || pipe->writers == 0) {
            release_spinlock(&pipe->lock);
            return copied;
        }
        release_spinlock(&pipe->lock);
        ksleepm(10);
    }
    return copied;
}

int pipe_write(struct pipe_data *pipe, const void *buf, int count)
{
    int copied = 0;
    if (pipe == NULL || buf == NULL || count < 0)
        return -1;
    while (copied < count) {
        acquire_spinlock(&pipe->lock);
        if (pipe->readers == 0) {
            release_spinlock(&pipe->lock);
            return copied ? copied : -1;
        }
        while (copied < count && pipe->used < PIPE_BUFFER_SIZE) {
            pipe->buffer[pipe->write_pos] = ((const uint8_t *)buf)[copied++];
            pipe->write_pos = (pipe->write_pos + 1) % PIPE_BUFFER_SIZE;
            pipe->used++;
        }
        release_spinlock(&pipe->lock);
        if (copied < count)
            ksleepm(10);
    }
    return copied;
}

void pipe_close(struct pipe_data *pipe, bool writer)
{
    bool free_pipe = false;
    if (pipe == NULL)
        return;
    acquire_spinlock(&pipe->lock);
    if (writer)
        pipe->writers--;
    else
        pipe->readers--;
    if (pipe->readers == 0 && pipe->writers == 0)
        free_pipe = true;
    release_spinlock(&pipe->lock);
    if (free_pipe)
        kfree(pipe);
}

void pipe_add_ref(struct pipe_data *pipe, bool writer)
{
    if (pipe == NULL)
        return;
    acquire_spinlock(&pipe->lock);
    if (writer)
        pipe->writers++;
    else
        pipe->readers++;
    release_spinlock(&pipe->lock);
}
