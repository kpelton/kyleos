#ifndef SCHED_H
#define SCHED_H
#include <timer/timer.h>
#include <include/types.h>
#include <mm/paging.h>
#include <mm/vmm.h>
#include <fs/vfs.h>
#define MAX_TASK_OPEN_FILES 8
#define SCHED_MAX_TASKS 1024
#define SCHED_MAX_NAME 32
#define KTHREAD_STACK_SIZE 4096*8

#define USER_STACK_VADDR (uint64_t *) 0x700000000
#define USER_STACK_SIZE 32
#define USER_HEAP_VADDR (uint64_t *) 0x600000000
#define USER_HEAP_SIZE 1 // in pages
#define IDLE_PID 0
void kthread_add(void (*fptr)(),char * name);
void schedule();
void sched_stats();
void ksleepm(uint32_t ms);
struct ktask* get_current_process();
struct ktask *sched_get_process(int pid);
bool sched_process_kill(int pid,bool cleanup);
int user_process_fork();
void sched_save_context();
int user_process_replace_exec(struct ktask *t, uint64_t startaddr, char *name, struct vmm_map *mm, char *argv[]);
int user_process_add_exec(uint64_t startaddr, char *name,struct vmm_map *mm,bool update_pid,char *argv[]);
void sched_init();
#define FXSAVE_SIZE 512
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
    uint64_t *user_start_heap;
    uint64_t *end_heap;
    uint64_t *user_heap_loc;
    uint64_t *start_addr;
    uint64_t *s_rsp;
    uint64_t *s_rbp;
    uint64_t context_switches;
    uint64_t *save_rsp;
    uint64_t *save_rip;
    uint64_t heap_size; //in pages
    struct vmm_map *mm;
    struct basic_timer timer;
    struct file *open_fds[MAX_TASK_OPEN_FILES];
    uint64_t fxsave_region[FXSAVE_SIZE/sizeof(uint64_t)] __attribute__((aligned(16)));
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
