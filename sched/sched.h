#ifndef SCHED_H
#define SCHED_H
#include <timer/timer.h>
#include <include/types.h>
#include <mm/paging.h>
#include <fs/vfs.h>
#define MAX_TASK_OPEN_FILES 8
#define SCHED_MAX_TASKS 1024
#define SCHED_MAX_NAME 32
#define KTHREAD_STACK_SIZE 0x8000

struct p_memblock {
	void *block;
	uint32_t count;
	uint32_t pg_opts;
	uint64_t vaddr;
	struct p_memblock *next;
};


void kthread_add(void (*fptr)(),char * name);
int user_process_add(void (*fptr)(),char * name);
void schedule();
void sched_stats();
void ksleepm(uint32_t ms);
struct ktask* get_current_process();
bool sched_process_kill(int pid);
int user_process_fork();
void sched_save_context();
int user_process_add_exec(uint64_t startaddr, char *name,struct pg_tbl *tbl,struct p_memblock *head);
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



#endif
