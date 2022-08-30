#ifndef SCHED_H
#define SCHED_H
#include <timer/timer.h>
#include <include/types.h>
#include <mm/paging.h>
#include <fs/vfs.h>
#define MAX_TASK_OPEN_FILES 8
#define SCHED_MAX_TASKS 1024
#define SCHED_MAX_NAME 32
#define KTHREAD_STACK_SIZE 4096*8

struct p_memblock {
    void *block;
    uint32_t count;
            struct p_memblock *next;

    uint32_t pg_opts;
    uint64_t vaddr;

};


void kthread_add(void (*fptr)(),char * name);
void schedule();
void sched_stats();
void ksleepm(uint32_t ms);
struct ktask* get_current_process();
struct ktask *sched_get_process(int pid);
bool sched_process_kill(int pid,bool cleanup);
int user_process_fork();
void sched_save_context();
int user_process_add_exec(uint64_t startaddr, char *name,struct pg_tbl *tbl,struct p_memblock *head);
int user_process_replace_exec(struct ktask *t, uint64_t startaddr,char *name,struct pg_tbl *tbl, struct p_memblock *head);
void sched_init();
enum sched_states {
    TASK_RUNNING,
    TASK_READY,
    TASK_NEW,
    TASK_BLOCKED,
    TASK_DONE,
    TASK_STATE_NUM
};

enum process_types {
    KERNEL_PROCESS,
    USER_PROCESS,
    PROCESS_TYPES_LEN
};

struct ktask{
    int pid;
    int parent;
    int exit_code;
    char name[SCHED_MAX_NAME];
    uint8_t state;
    uint8_t type;
    uint64_t *stack_alloc;
    uint64_t *start_stack;
    uint64_t *user_stack_alloc;
    uint64_t *user_start_stack;
    uint64_t *start_addr;
    uint64_t *s_rsp;
    uint64_t *s_rbp;
    uint64_t context_switches;
    uint64_t *save_rsp;
    uint64_t *save_rip;
    struct pg_tbl *mm;
    struct basic_timer timer;
    struct p_memblock *mem_list;
    struct file *open_fds[MAX_TASK_OPEN_FILES];
};

struct reg_state 
{
    uint64_t flags;
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
    uint64_t rax;
    uint64_t user_ret_addr;
    uint64_t user_cs;
    uint64_t user_flags;
    uint64_t user_rsp;
    uint64_t user_ss;
} __attribute__((packed));

#endif
