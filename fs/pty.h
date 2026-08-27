#ifndef PTY_H
#define PTY_H

#include <include/types.h>
#include <include/uapi/termios.h>
#include <locks/spinlock.h>

#define PTY_BUFFER_SIZE 4096
#define PTY_EOF_CHARACTER 4

struct pty_queue {
    uint8_t buffer[PTY_BUFFER_SIZE];
    uint32_t read_pos;
    uint32_t write_pos;
    uint32_t used;
    int read_wait;
    int write_wait;
};

struct pty_data {
    struct pty_queue master_input;
    struct pty_queue slave_input;
    int master_refs;
    int slave_refs;
    int slave_eof_pending;
    uint8_t canonical[PTY_BUFFER_SIZE];
    uint32_t canonical_length;
    int foreground_group;
    int session;
    struct kyleos_termios termios;
    struct spinlock lock;
};

int pty_read(struct pty_data *pty, bool master, void *buf, int count);
int pty_write(struct pty_data *pty, bool master, const void *buf, int count);
void pty_close(struct pty_data *pty, bool master);
void pty_add_ref(struct pty_data *pty, bool master);
void pty_status(struct pty_data *pty, bool master,
                uint32_t *readable, uint32_t *writable,
                uint32_t *hung_up);
int pty_get_foreground_group(struct pty_data *pty);
int pty_set_foreground_group(struct pty_data *pty, int session,
                             int process_group);
int pty_get_termios(struct pty_data *pty, struct kyleos_termios *termios);
int pty_set_termios(struct pty_data *pty, int action,
                    const struct kyleos_termios *termios);

#endif
