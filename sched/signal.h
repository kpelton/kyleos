#ifndef KYLEOS_SCHED_SIGNAL_H
#define KYLEOS_SCHED_SIGNAL_H

#include <include/types.h>
#include <include/uapi/signal.h>

struct ktask;

/* Stack image saved by usermode_int immediately before returning to ring 3. */
struct signal_kernel_frame {
    uint64_t kernel_flags;
    uint64_t r15;
    uint64_t r14;
    uint64_t r13;
    uint64_t r12;
    uint64_t r11;
    uint64_t r10;
    uint64_t r9;
    uint64_t r8;
    uint64_t rbp;
    uint64_t rdi;
    uint64_t rsi;
    uint64_t rdx;
    uint64_t rcx;
    uint64_t rbx;
    uint64_t user_rip;
    uint64_t user_cs;
    uint64_t user_flags;
    uint64_t user_rsp;
    uint64_t user_ss;
} __attribute__((packed));

void signal_task_init(struct ktask *task);
void signal_task_fork(struct ktask *child, const struct ktask *parent);
void signal_task_exec(struct ktask *task);
int signal_send(int pid, int signal);
int signal_action(int signal, const struct kyleos_sigaction *action,
                  struct kyleos_sigaction *old_action);
int signal_set_mask(int how, const uint64_t *set, uint64_t *old_set);
int signal_get_pending(uint64_t *set);
bool signal_has_unmasked_pending(const struct ktask *task);
uint64_t signal_deliver(struct signal_kernel_frame *frame, uint64_t result);
uint64_t signal_sigreturn(void *user_frame,
                          struct signal_kernel_frame *kernel_frame);

#endif
