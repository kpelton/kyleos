#include <mm/paging.h>
#include <mm/vmm.h>
#include <sched/ps.h>
#include <sched/sched.h>
#include <sched/signal.h>

#define SIGNAL_FRAME_MAGIC 0x31474953454c594bULL
#define USER_CODE_SELECTOR ((3 * 8) | 3)
#define USER_DATA_SELECTOR ((4 * 8) | 3)
#define USER_RFLAGS_ALLOWED 0x0000000000240fd5ULL

struct signal_user_frame {
    uint64_t magic;
    uint64_t saved_result;
    uint64_t saved_mask;
    struct signal_kernel_frame saved_frame;
} __attribute__((packed));

static uint64_t signal_bit(int signal)
{
    return 1ULL << signal;
}

static bool signal_valid(int signal)
{
    return signal > 0 && signal < KYLEOS_NSIG;
}

static bool signal_default_ignored(int signal)
{
    return signal == KYLEOS_SIGCHLD || signal == KYLEOS_SIGCONT ||
           signal == KYLEOS_SIGWINCH;
}

static bool signal_default_stops(int signal)
{
    return signal == KYLEOS_SIGSTOP || signal == KYLEOS_SIGTSTP ||
           signal == KYLEOS_SIGTTIN || signal == KYLEOS_SIGTTOU;
}

static uint64_t signal_allowed_mask(uint64_t mask)
{
    return mask & ~KYLEOS_SIGNAL_UNMASKABLE;
}

static void signal_terminate(struct ktask *task, int signal)
{
    task->exit_code = 0;
    task->exit_signal = signal;
    sched_process_kill(task->pid, false, true);
    schedule();
}

void signal_task_init(struct ktask *task)
{
    if (task == NULL)
        return;
    task->signal_pending = 0;
    task->signal_mask = 0;
    task->exit_signal = 0;
    task->stop_signal = 0;
    task->stop_reported = 0;
    for (int signal = 0; signal < KYLEOS_NSIG; signal++) {
        task->signal_actions[signal].handler = KYLEOS_SIG_DFL;
        task->signal_actions[signal].mask = 0;
        task->signal_actions[signal].restorer = 0;
        task->signal_actions[signal].flags = 0;
        task->signal_actions[signal].reserved = 0;
    }
}

void signal_task_fork(struct ktask *child, const struct ktask *parent)
{
    if (child == NULL || parent == NULL)
        return;
    child->signal_pending = 0;
    child->signal_mask = parent->signal_mask;
    child->exit_signal = 0;
    child->stop_signal = 0;
    child->stop_reported = 0;
    for (int signal = 0; signal < KYLEOS_NSIG; signal++)
        child->signal_actions[signal] = parent->signal_actions[signal];
}

void signal_task_exec(struct ktask *task)
{
    if (task == NULL)
        return;
    for (int signal = 1; signal < KYLEOS_NSIG; signal++) {
        struct kyleos_sigaction *action = &task->signal_actions[signal];

        if (action->handler != KYLEOS_SIG_IGN) {
            action->handler = KYLEOS_SIG_DFL;
            action->mask = 0;
            action->restorer = 0;
            action->flags = 0;
            action->reserved = 0;
        }
    }
    task->signal_mask = signal_allowed_mask(task->signal_mask);
}

bool signal_has_unmasked_pending(const struct ktask *task)
{
    uint64_t deliverable;

    if (task == NULL)
        return false;
    deliverable = task->signal_pending & ~task->signal_mask;
    for (int signal = 1; signal < KYLEOS_NSIG; signal++) {
        uint64_t handler;

        if ((deliverable & signal_bit(signal)) == 0)
            continue;
        handler = task->signal_actions[signal].handler;
        if (handler == KYLEOS_SIG_IGN ||
            (handler == KYLEOS_SIG_DFL && signal_default_ignored(signal)))
            continue;
        return true;
    }
    return false;
}

int signal_send(int pid, int signal)
{
    struct ktask *task;
    uint64_t bit;

    if (signal < 0 || signal >= KYLEOS_NSIG)
        return -1;
    task = sched_get_process(pid);
    if (task == NULL || task->type != USER_PROCESS ||
        task->state == TASK_DONE)
        return -1;
    if (signal == 0)
        return 0;

    if (signal == KYLEOS_SIGCONT) {
        __sync_fetch_and_and(&task->signal_pending,
                             ~(signal_bit(KYLEOS_SIGSTOP) |
                               signal_bit(KYLEOS_SIGTSTP) |
                               signal_bit(KYLEOS_SIGTTIN) |
                               signal_bit(KYLEOS_SIGTTOU)));
        if (task->state == TASK_STOPPED)
            task->state = TASK_READY;
        task->stop_signal = 0;
        task->stop_reported = 0;
    }
    bit = signal_bit(signal);
    if ((task->signal_actions[signal].handler == KYLEOS_SIG_IGN ||
         (task->signal_actions[signal].handler == KYLEOS_SIG_DFL &&
          signal_default_ignored(signal))) &&
        signal != KYLEOS_SIGKILL && signal != KYLEOS_SIGSTOP)
        return 0;

    __sync_fetch_and_or(&task->signal_pending, bit);
    if (task->state == TASK_BLOCKED ||
        (task->state == TASK_STOPPED && signal == KYLEOS_SIGCONT) ||
        signal == KYLEOS_SIGKILL) {
        task->wait_channel = NULL;
        task->timer.state = TIMER_UNUSED;
        task->state = TASK_READY;
    }
    return 0;
}

int signal_action(int signal, const struct kyleos_sigaction *action,
                  struct kyleos_sigaction *old_action)
{
    struct ktask *task = get_current_process();

    if (!signal_valid(signal) || task == NULL ||
        (action != NULL &&
         (signal == KYLEOS_SIGKILL || signal == KYLEOS_SIGSTOP)))
        return -1;
    if (old_action != NULL)
        *old_action = task->signal_actions[signal];
    if (action != NULL) {
        struct kyleos_sigaction replacement = *action;

        if (replacement.handler > KYLEOS_SIG_IGN &&
            (replacement.handler >= (uint64_t)KERN_SPACE_BOUNDRY ||
             replacement.restorer == 0 ||
             replacement.restorer >= (uint64_t)KERN_SPACE_BOUNDRY))
            return -1;
        replacement.mask = signal_allowed_mask(replacement.mask);
        replacement.reserved = 0;
        task->signal_actions[signal] = replacement;
        if (replacement.handler == KYLEOS_SIG_IGN ||
            (replacement.handler == KYLEOS_SIG_DFL &&
             signal_default_ignored(signal)))
            __sync_fetch_and_and(&task->signal_pending,
                                 ~signal_bit(signal));
    }
    return 0;
}

int signal_set_mask(int how, const uint64_t *set, uint64_t *old_set)
{
    struct ktask *task = get_current_process();
    uint64_t replacement;

    if (task == NULL)
        return -1;
    if (old_set != NULL)
        *old_set = task->signal_mask;
    if (set == NULL)
        return 0;
    if (how == KYLEOS_SIG_BLOCK)
        replacement = task->signal_mask | *set;
    else if (how == KYLEOS_SIG_UNBLOCK)
        replacement = task->signal_mask & ~*set;
    else if (how == KYLEOS_SIG_SETMASK)
        replacement = *set;
    else
        return -1;
    task->signal_mask = signal_allowed_mask(replacement);
    return 0;
}

int signal_get_pending(uint64_t *set)
{
    struct ktask *task = get_current_process();

    if (task == NULL || set == NULL)
        return -1;
    *set = task->signal_pending;
    return 0;
}

static int signal_next(struct ktask *task)
{
    uint64_t deliverable = task->signal_pending & ~task->signal_mask;

    for (int signal = 1; signal < KYLEOS_NSIG; signal++)
        if ((deliverable & signal_bit(signal)) != 0)
            return signal;
    return 0;
}

static bool signal_prepare_user_handler(struct ktask *task, int signal,
                                        struct signal_kernel_frame *frame,
                                        uint64_t result)
{
    struct kyleos_sigaction *action = &task->signal_actions[signal];
    struct signal_user_frame *user_frame;
    uint64_t handler_rsp;
    uint64_t frame_address;
    uint64_t frame_end;

    handler_rsp = (frame->user_rsp - sizeof(*user_frame) - 8) & ~0xfULL;
    handler_rsp -= 8;
    frame_address = handler_rsp + 8;
    frame_end = frame_address + sizeof(*user_frame);
    if (handler_rsp < (uint64_t)USER_STACK_VADDR ||
        frame_end < frame_address ||
        frame_end > (uint64_t)USER_STACK_VADDR +
                    USER_STACK_SIZE * PAGE_SIZE)
        return false;
    if (!vmm_populate_page(task->mm, handler_rsp) ||
        !vmm_populate_page(task->mm, frame_end - 1))
        return false;

    *(uint64_t *)handler_rsp = action->restorer;
    user_frame = (struct signal_user_frame *)frame_address;
    user_frame->magic = SIGNAL_FRAME_MAGIC;
    user_frame->saved_result = result;
    user_frame->saved_mask = task->signal_mask;
    user_frame->saved_frame = *frame;

    task->signal_mask = signal_allowed_mask(task->signal_mask |
                                             action->mask |
                                             signal_bit(signal));
    frame->rdi = (uint64_t)signal;
    frame->user_rip = action->handler;
    frame->user_rsp = handler_rsp;
    return true;
}

uint64_t signal_deliver(struct signal_kernel_frame *frame, uint64_t result)
{
    struct ktask *task = get_current_process();

    if (task == NULL || task->type != USER_PROCESS || frame == NULL)
        return result;
    for (;;) {
        int signal = signal_next(task);
        struct kyleos_sigaction *action;

        if (signal == 0)
            return result;
        __sync_fetch_and_and(&task->signal_pending, ~signal_bit(signal));
        action = &task->signal_actions[signal];
        if (action->handler == KYLEOS_SIG_IGN ||
            (action->handler == KYLEOS_SIG_DFL &&
             signal_default_ignored(signal)))
            continue;
        if (action->handler == KYLEOS_SIG_DFL &&
            signal_default_stops(signal)) {
            task->state = TASK_STOPPED;
            sched_process_stopped(task, signal);
            schedule();
            continue;
        }
        if (action->handler == KYLEOS_SIG_DFL) {
            signal_terminate(task, signal);
            return result;
        }
        if (!signal_prepare_user_handler(task, signal, frame, result)) {
            signal_terminate(task, KYLEOS_SIGSEGV);
            return result;
        }
        return result;
    }
}

uint64_t signal_sigreturn(void *user_frame_pointer,
                          struct signal_kernel_frame *kernel_frame)
{
    struct ktask *task = get_current_process();
    struct signal_user_frame *user_frame = user_frame_pointer;
    uint64_t address = (uint64_t)user_frame_pointer;
    uint64_t stack_end = (uint64_t)USER_STACK_VADDR +
                         USER_STACK_SIZE * PAGE_SIZE;
    uint64_t kernel_flags;
    uint64_t result;

    if (task == NULL || kernel_frame == NULL || user_frame == NULL ||
        address < (uint64_t)USER_STACK_VADDR ||
        address > stack_end - sizeof(*user_frame) ||
        user_frame->magic != SIGNAL_FRAME_MAGIC ||
        user_frame->saved_frame.user_rip >=
            (uint64_t)KERN_SPACE_BOUNDRY ||
        user_frame->saved_frame.user_rsp >=
            (uint64_t)KERN_SPACE_BOUNDRY) {
        if (task != NULL)
            signal_terminate(task, KYLEOS_SIGSEGV);
        return (uint64_t)-1;
    }

    kernel_flags = kernel_frame->kernel_flags;
    result = user_frame->saved_result;
    task->signal_mask = signal_allowed_mask(user_frame->saved_mask);
    *kernel_frame = user_frame->saved_frame;
    kernel_frame->kernel_flags = kernel_flags;
    kernel_frame->user_cs = USER_CODE_SELECTOR;
    kernel_frame->user_ss = USER_DATA_SELECTOR;
    kernel_frame->user_flags =
        (kernel_frame->user_flags & USER_RFLAGS_ALLOWED) | 0x202;
    user_frame->magic = 0;
    return result;
}
