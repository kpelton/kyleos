#ifndef SCHED_H
#define SCHED_H
#include <timer/timer.h>
#include <include/types.h>
#include <mm/paging.h>
#define SCHED_MAX_TASKS 1024
#define SCHED_MAX_NAME 32
#define KTHREAD_STACK_SIZE 0x8000

void kthread_add(void (*fptr)(),char * name);
void user_process_add(void (*fptr)(),char * name);
void schedule();
void sched_stats();
void ksleepm(uint32_t ms);
struct ktask* get_current_process();
bool sched_process_kill(int pid);
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
	struct pg_tbl *mm;
    struct basic_timer timer;
};



#endif
