#ifndef SCHED_H
#define SCHED_H
#include <timer/timer.h>
#define SCHED_MAX_TASKS 100
#define SCHED_MAX_NAME 32
#define KTHREAD_STACK_SIZE 0x8000

void kthread_add(unsigned long *fptr,char * name);
void user_process_add(unsigned long *fptr,char * name);
void schedule();
void sched_stats();
void ksleepm(unsigned int ms);
struct ktask* get_current_process();
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
	unsigned char state;
	unsigned char type;
	unsigned long *start_stack;
	unsigned long *user_start_stack;
	unsigned long *start_addr;
	unsigned long *s_rsp;
	unsigned long *s_rbp;
	unsigned long context_switches;
    struct basic_timer timer;
};



#endif
