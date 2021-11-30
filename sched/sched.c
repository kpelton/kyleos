#include <output/output.h>
#include <sched/sched.h>
#include <init/tables.h>
#include <timer/timer.h>
#include <mm/mm.h>

static struct ktask ktasks[SCHED_MAX_TASKS];
static unsigned int max_task = 0;
static unsigned int next_task = 0;
static int  prev_task = -1;
static int pid = 1;

void switch_to(unsigned long *rsp,unsigned long *addr);
void resume_p(unsigned long *rsp,unsigned long *rbp);
static const char process_types_str [PROCESS_TYPES_LEN][20] = {"Kernel","User"};
static const char task_type_str [TASK_STATE_NUM][20] = {"RUNNING","READY","NEW","BLOCKED","DONE"};

void ksleepm(unsigned int ms) {
    struct ktask *process = get_current_process();
    process->timer = new_timer(ms); 
    //yield to another process
    schedule();
	//asm("sti");
}

void kthread_add(void (*fptr)(),char * name) {
	struct ktask *t;
	unsigned long stack_low;
	t = &ktasks[max_task];

	stack_low = (unsigned long) kmalloc(KTHREAD_STACK_SIZE);
	max_task += 1;
	t->state = TASK_NEW;
	t->start_addr = (unsigned long *) fptr;
	t->start_stack =(unsigned long *) ( stack_low + KTHREAD_STACK_SIZE);
	t->pid = pid;
	t->type = KERNEL_PROCESS;
    t->timer.state = TIMER_UNUSED;
	kstrcpy(t->name,name);
	t->context_switches = 0;
	pid+=1;
}

struct ktask* get_current_process() {
    return &ktasks[prev_task];
}

void user_process_add(void (*fptr)(),char * name) {
	struct ktask *t;
	unsigned long stack_low;
	unsigned long user_stack_low;
	t = &ktasks[max_task];

	//kprintf("Allocating Stack\n");
	stack_low = (unsigned long) kmalloc(KTHREAD_STACK_SIZE);
	user_stack_low = (unsigned long) kmalloc(KTHREAD_STACK_SIZE);
	max_task += 1;

	//kprint_hex("Stack addr 0x",stack_low);
	//kprint_hex("Stack ptr  0x",stack_low+ KTHREAD_STACK_SIZE);
	//kprint_hex("start ptr  0x",fptr);
	t->state = TASK_NEW;
	t->start_addr = (unsigned long *) fptr;
	t->start_stack = (unsigned long *) (stack_low + KTHREAD_STACK_SIZE);
	t->user_start_stack = (unsigned long *) (user_stack_low + KTHREAD_STACK_SIZE);
	t->pid = pid;
	t->type = USER_PROCESS;
    t->timer.state = TIMER_UNUSED;
	kstrcpy(t->name,name);
	t->context_switches = 0;
	pid+=1;
}

void sched_stats() {
	unsigned int i;

	for (i=0; i<max_task; i++) {
		kprintf("PID %d\n",ktasks[i].pid);
		kprintf("  name:%s\n",ktasks[i].name);
		kprintf("  stack:           0x%x\n",ktasks[i].start_stack);
		kprintf("  stack end:       0x%x\n",ktasks[i].start_stack-KTHREAD_STACK_SIZE);
		kprintf("  Process State:   %s\n",task_type_str[ktasks[i].state]);
		kprintf("  context switches:0x%x\n",ktasks[i].context_switches);
		kprintf("  Process Type:    %s\n",process_types_str[ktasks[i].type]);
		kprintf("  Sleep state:     %s\n",str_timer_states[ktasks[i].timer.state]);
	}
}

void schedule() {
    unsigned int i = next_task;
    asm("cli");   
    if (prev_task != -1 && ktasks[prev_task].state != TASK_BLOCKED) {
        ktasks[prev_task].state = TASK_READY;
        //save old stack
        asm volatile("movq %%rsp ,%0" : "=g"(ktasks[prev_task].s_rsp));
        asm volatile("movq %%rbp ,%0" : "=g"(ktasks[prev_task].s_rbp));
    }
 
    for (i = next_task; i < max_task; i++) {
        next_task = (next_task +1) %max_task;
        
        if (update_timer(&ktasks[i].timer)) {
            ktasks[i].state = TASK_READY;
        }
        else if (ktasks[i].timer.state == TIMER_RUNNING) {
            ktasks[i].state = TASK_BLOCKED;
            return;
        }

        prev_task=i;

        if (ktasks[i].state == TASK_NEW && ktasks[i].type == KERNEL_PROCESS) {
            ktasks[i].state = TASK_RUNNING;
            switch_to(ktasks[i].start_stack,ktasks[i].start_addr);
        }
        else if (ktasks[i].state == TASK_NEW && ktasks[i].type == USER_PROCESS) {
            ktasks[i].state = TASK_RUNNING;
            set_tss_rsp(ktasks[i].start_stack); // Set the kernel stack pointer.
            jump_usermode(ktasks[i].start_addr,ktasks[i].user_start_stack);
        }
        else if (ktasks[i].state == TASK_READY){
            ktasks[i].state = TASK_RUNNING;
            ktasks[i].context_switches += 1;
            set_tss_rsp(ktasks[i].start_stack); // Set the kernel stack pointer.
			resume_p(ktasks[i].s_rsp,ktasks[i].s_rbp);
        }	
        return;
    }
}
