#include <fs/pty.h>
#include <mm/mm.h>
#include <sched/sched.h>
#include <sched/signal.h>

static struct pty_queue *incoming_queue(struct pty_data *pty, bool master)
{
    return master ? &pty->master_input : &pty->slave_input;
}

static struct pty_queue *outgoing_queue(struct pty_data *pty, bool master)
{
    return master ? &pty->slave_input : &pty->master_input;
}

static int peer_refs(struct pty_data *pty, bool master)
{
    return master ? pty->slave_refs : pty->master_refs;
}

int pty_read(struct pty_data *pty, bool master, void *buf, int count)
{
    struct pty_queue *queue;
    int copied = 0;

    if (pty == NULL || buf == NULL || count < 0)
        return -1;
    if (count == 0)
        return 0;
    queue = incoming_queue(pty, master);
    while (copied == 0) {
        bool wake_writers = false;

        acquire_spinlock(&pty->lock);
        while (copied < count && queue->used > 0) {
            ((uint8_t *)buf)[copied++] = queue->buffer[queue->read_pos];
            queue->read_pos = (queue->read_pos + 1) % PTY_BUFFER_SIZE;
            queue->used--;
        }
        if (copied > 0) {
            queue->write_wait = 0;
            wake_writers = true;
        }
        if (!master && copied == 0 && pty->slave_eof_pending) {
            pty->slave_eof_pending = 0;
            release_spinlock(&pty->lock);
            return 0;
        }
        if (copied > 0 || peer_refs(pty, master) == 0) {
            release_spinlock(&pty->lock);
            if (wake_writers)
                sched_wakeup(&queue->write_wait);
            if (copied > 0)
                sched_io_notify();
            return copied;
        }
        if (signal_has_unmasked_pending(get_current_process())) {
            release_spinlock(&pty->lock);
            return -1;
        }
        queue->read_wait = 1;
        release_spinlock(&pty->lock);
        sched_sleep_on_if(&queue->read_wait, &queue->read_wait);
    }
    return copied;
}

int pty_write(struct pty_data *pty, bool master, const void *buf, int count)
{
    struct pty_queue *queue;
    int copied = 0;

    if (pty == NULL || buf == NULL || count < 0)
        return -1;
    queue = outgoing_queue(pty, master);
    while (copied < count) {
        int before = copied;

        acquire_spinlock(&pty->lock);
        if (peer_refs(pty, master) == 0) {
            release_spinlock(&pty->lock);
            signal_send(get_current_process()->pid, KYLEOS_SIGPIPE);
            return copied ? copied : -1;
        }
        while (copied < count && queue->used < PTY_BUFFER_SIZE) {
            uint8_t byte = ((const uint8_t *)buf)[copied++];

            if (master && byte == PTY_EOF_CHARACTER) {
                pty->slave_eof_pending = 1;
                continue;
            }
            queue->buffer[queue->write_pos] = byte;
            queue->write_pos = (queue->write_pos + 1) % PTY_BUFFER_SIZE;
            queue->used++;
        }
        if (copied > before)
            queue->read_wait = 0;
        if (copied < count)
            queue->write_wait = 1;
        release_spinlock(&pty->lock);
        if (copied > before)
            sched_wakeup(&queue->read_wait);
        if (copied > before)
            sched_io_notify();
        if (copied < count &&
            signal_has_unmasked_pending(get_current_process()))
            return copied ? copied : -1;
        if (copied < count)
            sched_sleep_on_if(&queue->write_wait, &queue->write_wait);
    }
    return copied;
}

void pty_close(struct pty_data *pty, bool master)
{
    struct pty_queue *peer_input;
    struct pty_queue *own_input;
    bool free_pty = false;

    if (pty == NULL)
        return;
    peer_input = outgoing_queue(pty, master);
    own_input = incoming_queue(pty, master);
    acquire_spinlock(&pty->lock);
    if (master)
        pty->master_refs--;
    else
        pty->slave_refs--;
    if ((master ? pty->master_refs : pty->slave_refs) == 0) {
        peer_input->read_wait = 0;
        own_input->write_wait = 0;
    }
    if (pty->master_refs == 0 && pty->slave_refs == 0)
        free_pty = true;
    release_spinlock(&pty->lock);
    sched_wakeup(&peer_input->read_wait);
    sched_wakeup(&own_input->write_wait);
    sched_io_notify();
    if (free_pty)
        kfree(pty);
}

void pty_add_ref(struct pty_data *pty, bool master)
{
    if (pty == NULL)
        return;
    acquire_spinlock(&pty->lock);
    if (master)
        pty->master_refs++;
    else
        pty->slave_refs++;
    release_spinlock(&pty->lock);
}

void pty_status(struct pty_data *pty, bool master,
                uint32_t *readable, uint32_t *writable,
                uint32_t *hung_up)
{
    struct pty_queue *incoming;
    struct pty_queue *outgoing;
    int peers;

    if (readable != NULL) *readable = 0;
    if (writable != NULL) *writable = 0;
    if (hung_up != NULL) *hung_up = 1;
    if (pty == NULL)
        return;
    incoming = incoming_queue(pty, master);
    outgoing = outgoing_queue(pty, master);
    acquire_spinlock(&pty->lock);
    peers = peer_refs(pty, master);
    if (readable != NULL)
        *readable = incoming->used +
                    (!master && pty->slave_eof_pending ? 1U : 0U);
    if (writable != NULL)
        *writable = peers > 0 && outgoing->used < PTY_BUFFER_SIZE;
    if (hung_up != NULL)
        *hung_up = peers == 0;
    release_spinlock(&pty->lock);
}
