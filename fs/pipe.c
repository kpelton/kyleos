#include <fs/pipe.h>
#include <mm/mm.h>
#include <sched/sched.h>

int pipe_read(struct pipe_data *pipe, void *buf, int count)
{
    int copied = 0;
    if (pipe == NULL || buf == NULL || count < 0)
        return -1;
    if (count == 0)
        return 0;
    while (copied == 0) {
        bool wake_writers = false;

        acquire_spinlock(&pipe->lock);
        while (copied < count && pipe->used > 0) {
            ((uint8_t *)buf)[copied++] = pipe->buffer[pipe->read_pos];
            pipe->read_pos = (pipe->read_pos + 1) % PIPE_BUFFER_SIZE;
            pipe->used--;
        }
        if (copied > 0) {
            pipe->write_wait = 0;
            wake_writers = true;
        }
        if (copied > 0 || pipe->writers == 0) {
            release_spinlock(&pipe->lock);
            if (wake_writers)
                sched_wakeup(&pipe->write_wait);
            if (copied > 0)
                sched_io_notify();
            return copied;
        }
        pipe->read_wait = 1;
        release_spinlock(&pipe->lock);
        sched_sleep_on_if(&pipe->read_wait, &pipe->read_wait);
    }
    return copied;
}

int pipe_write(struct pipe_data *pipe, const void *buf, int count)
{
    int copied = 0;
    if (pipe == NULL || buf == NULL || count < 0)
        return -1;
    while (copied < count) {
        int before = copied;

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
        if (copied > before)
            pipe->read_wait = 0;
        if (copied < count)
            pipe->write_wait = 1;
        release_spinlock(&pipe->lock);
        if (copied > before)
            sched_wakeup(&pipe->read_wait);
        if (copied > before)
            sched_io_notify();
        if (copied < count)
            sched_sleep_on_if(&pipe->write_wait, &pipe->write_wait);
    }
    return copied;
}

void pipe_close(struct pipe_data *pipe, bool writer)
{
    bool free_pipe = false;
    bool wake_readers = false;
    bool wake_writers = false;

    if (pipe == NULL)
        return;
    acquire_spinlock(&pipe->lock);
    if (writer) {
        pipe->writers--;
        if (pipe->writers == 0) {
            pipe->read_wait = 0;
            wake_readers = true;
        }
    } else {
        pipe->readers--;
        if (pipe->readers == 0) {
            pipe->write_wait = 0;
            wake_writers = true;
        }
    }
    if (pipe->readers == 0 && pipe->writers == 0)
        free_pipe = true;
    release_spinlock(&pipe->lock);
    if (wake_readers)
        sched_wakeup(&pipe->read_wait);
    if (wake_writers)
        sched_wakeup(&pipe->write_wait);
    sched_io_notify();
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

void pipe_status(struct pipe_data *pipe, bool writer,
                 uint32_t *readable, uint32_t *writable, uint32_t *hung_up)
{
    if (readable != NULL) *readable = 0;
    if (writable != NULL) *writable = 0;
    if (hung_up != NULL) *hung_up = 1;
    if (pipe == NULL)
        return;
    acquire_spinlock(&pipe->lock);
    if (writer) {
        if (writable != NULL)
            *writable = pipe->readers > 0 && pipe->used < PIPE_BUFFER_SIZE;
        if (hung_up != NULL) *hung_up = pipe->readers == 0;
    } else {
        if (readable != NULL) *readable = pipe->used;
        if (hung_up != NULL) *hung_up = pipe->writers == 0;
    }
    release_spinlock(&pipe->lock);
}
