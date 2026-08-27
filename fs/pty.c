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

static void queue_byte(struct pty_queue *queue, uint8_t byte)
{
    if (queue->used >= PTY_BUFFER_SIZE)
        return;
    queue->buffer[queue->write_pos] = byte;
    queue->write_pos = (queue->write_pos + 1) % PTY_BUFFER_SIZE;
    queue->used++;
    queue->read_wait = 0;
}

static void clear_queue(struct pty_queue *queue)
{
    queue->read_pos = 0;
    queue->write_pos = 0;
    queue->used = 0;
    queue->write_wait = 0;
}

static void echo_bytes(struct pty_data *pty, const char *bytes, int count)
{
    for (int index = 0; index < count; index++)
        queue_byte(&pty->master_input, (uint8_t)bytes[index]);
}

static void flush_canonical(struct pty_data *pty)
{
    for (uint32_t index = 0; index < pty->canonical_length; index++)
        queue_byte(&pty->slave_input, pty->canonical[index]);
    pty->canonical_length = 0;
}

int pty_read(struct pty_data *pty, bool master, void *buf, int count)
{
    struct pty_queue *queue;
    int copied = 0;

    if (pty == NULL || buf == NULL || count < 0)
        return -1;
    if (count == 0)
        return 0;
    if (!master) {
        struct ktask *current = get_current_process();
        int foreground;

        acquire_spinlock(&pty->lock);
        foreground = pty->foreground_group;
        release_spinlock(&pty->lock);
        if (current != NULL && foreground > 0 &&
            current->process_group != foreground) {
            (void)signal_send(current->pid, KYLEOS_SIGTTIN);
            return -1;
        }
    }
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
    if (!master) {
        struct ktask *current = get_current_process();
        int foreground;
        bool stop_background;

        acquire_spinlock(&pty->lock);
        foreground = pty->foreground_group;
        stop_background =
            (pty->termios.local_flags & KYLEOS_TOSTOP) != 0;
        release_spinlock(&pty->lock);
        if (current != NULL && stop_background && foreground > 0 &&
            current->process_group != foreground) {
            (void)signal_send(current->pid, KYLEOS_SIGTTOU);
            return -1;
        }
    }
    queue = outgoing_queue(pty, master);
    while (copied < count) {
        int before = copied;
        int generated_signal = 0;
        int foreground_group = 0;
        bool wake_master = false;

        acquire_spinlock(&pty->lock);
        if (peer_refs(pty, master) == 0) {
            release_spinlock(&pty->lock);
            signal_send(get_current_process()->pid, KYLEOS_SIGPIPE);
            return copied ? copied : -1;
        }
        while (copied < count && queue->used < PTY_BUFFER_SIZE) {
            uint8_t byte = ((const uint8_t *)buf)[copied++];

            if (master) {
                uint16_t local_flags = pty->termios.local_flags;
                uint8_t *controls = pty->termios.control_characters;

                if ((pty->termios.input_flags & KYLEOS_ICRNL) != 0 &&
                    byte == '\r')
                    byte = '\n';
                if ((local_flags & KYLEOS_ISIG) != 0 &&
                    (byte == controls[KYLEOS_VINTR] ||
                     byte == controls[KYLEOS_VQUIT] ||
                     byte == controls[KYLEOS_VSUSP])) {
                    generated_signal =
                        byte == controls[KYLEOS_VINTR] ? KYLEOS_SIGINT :
                        byte == controls[KYLEOS_VQUIT] ? KYLEOS_SIGQUIT :
                                                        KYLEOS_SIGTSTP;
                    foreground_group = pty->foreground_group;
                    if ((local_flags & KYLEOS_NOFLSH) == 0) {
                        clear_queue(&pty->slave_input);
                        clear_queue(&pty->master_input);
                        pty->canonical_length = 0;
                    }
                    if ((local_flags & KYLEOS_ECHO) != 0) {
                        char echo[3] = {'^',
                            generated_signal == KYLEOS_SIGINT ? 'C' :
                            generated_signal == KYLEOS_SIGQUIT ? '\\' : 'Z',
                            '\n'};
                        echo_bytes(pty, echo, 3);
                        wake_master = true;
                    }
                    break;
                }
                if ((local_flags & KYLEOS_ICANON) != 0) {
                    if (byte == controls[KYLEOS_VERASE]) {
                        if (pty->canonical_length > 0) {
                            pty->canonical_length--;
                            if ((local_flags & KYLEOS_ECHOE) != 0) {
                                echo_bytes(pty, "\b \b", 3);
                                wake_master = true;
                            }
                        }
                        continue;
                    }
                    if (byte == controls[KYLEOS_VKILL]) {
                        pty->canonical_length = 0;
                        if ((local_flags & KYLEOS_ECHOK) != 0) {
                            echo_bytes(pty, "\n", 1);
                            wake_master = true;
                        }
                        continue;
                    }
                    if (byte == controls[KYLEOS_VEOF]) {
                        if (pty->canonical_length == 0)
                            pty->slave_eof_pending = 1;
                        else
                            flush_canonical(pty);
                        continue;
                    }
                    if (pty->canonical_length + pty->slave_input.used >=
                        PTY_BUFFER_SIZE) {
                        copied--;
                        break;
                    }
                    pty->canonical[pty->canonical_length++] = byte;
                    if ((local_flags & KYLEOS_ECHO) != 0) {
                        queue_byte(&pty->master_input, byte);
                        wake_master = true;
                    }
                    if (byte == '\n' ||
                        (controls[KYLEOS_VEOL] != 0 &&
                         byte == controls[KYLEOS_VEOL]))
                        flush_canonical(pty);
                    continue;
                }
            }
            queue_byte(queue, byte);
        }
        if (copied > before)
            queue->read_wait = 0;
        if (copied < count && generated_signal == 0)
            queue->write_wait = 1;
        release_spinlock(&pty->lock);
        if (copied > before)
            sched_wakeup(&queue->read_wait);
        if (wake_master)
            sched_wakeup(&pty->master_input.read_wait);
        if (copied > before)
            sched_io_notify();
        if (generated_signal != 0 && foreground_group > 0)
            (void)signal_send_group(foreground_group, generated_signal);
        if (generated_signal != 0)
            continue;
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
        *writable = peers > 0 && outgoing->used +
                    (master &&
                     (pty->termios.local_flags & KYLEOS_ICANON) != 0
                         ? pty->canonical_length : 0) < PTY_BUFFER_SIZE;
    if (hung_up != NULL)
        *hung_up = peers == 0;
    release_spinlock(&pty->lock);
}

int pty_get_foreground_group(struct pty_data *pty)
{
    int group;

    if (pty == NULL)
        return -1;
    acquire_spinlock(&pty->lock);
    group = pty->foreground_group;
    release_spinlock(&pty->lock);
    return group;
}

int pty_set_foreground_group(struct pty_data *pty, int session,
                             int process_group)
{
    if (pty == NULL || session <= 0 || process_group <= 0)
        return -1;
    acquire_spinlock(&pty->lock);
    if (pty->session != 0 && pty->session != session) {
        release_spinlock(&pty->lock);
        return -1;
    }
    pty->session = session;
    pty->foreground_group = process_group;
    release_spinlock(&pty->lock);
    return 0;
}

int pty_get_termios(struct pty_data *pty, struct kyleos_termios *termios)
{
    if (pty == NULL || termios == NULL)
        return -1;
    acquire_spinlock(&pty->lock);
    *termios = pty->termios;
    release_spinlock(&pty->lock);
    return 0;
}

int pty_set_termios(struct pty_data *pty, int action,
                    const struct kyleos_termios *termios)
{
    if (pty == NULL || termios == NULL ||
        (action != KYLEOS_TCSANOW && action != KYLEOS_TCSADRAIN &&
         action != KYLEOS_TCSAFLUSH))
        return -1;
    acquire_spinlock(&pty->lock);
    if (action == KYLEOS_TCSAFLUSH) {
        clear_queue(&pty->slave_input);
        pty->canonical_length = 0;
        pty->slave_eof_pending = 0;
    }
    pty->termios = *termios;
    release_spinlock(&pty->lock);
    sched_io_notify();
    return 0;
}
