#include <output/output.h>
#include <sched/sched.h>
#include <init/tables.h>
#include <mm/mm.h>

static struct ktask ktasks[SCHED_MAX_TASKS];
static unsigned int max_task = 0;
static unsigned int next_task = 0;
static int  prev_task = -1;
static int pid = 1;

void switch_to(unsigned long rsp,unsigned long addr);
void resume_p(unsigned long rsp,unsigned long rbp);
const char process_types_str [PROCESS_TYPES_LEN][100] = {"Kernel","User"};
const char task_type_str [TASK_STATE_NUM][100] = {"RUNNING","READY","NEW","BLOCKED","DONE"};

void ksleepm(unsigned int ms) {
    struct ktask *process = get_current_process();
    process->timer = new_timer(ms); 
    //yield to another process
    schedule();
	//asm("sti");
}

void kthread_add(unsigned long *fptr,char * name)
{
	struct ktask *t;
	unsigned long stack_low;
	t = &ktasks[max_task];

	//kprintf("Allocating Stack\n");
	stack_low = (unsigned long) kmalloc(KTHREAD_STACK_SIZE);
	max_task += 1;

	//kprint_hex("Stack addr 0x",stack_low);
	//kprint_hex("Stack ptr  0x",stack_low+ KTHREAD_STACK_SIZE);
	//kprint_hex("start ptr  0x",fptr);
	t->state = TASK_NEW;
	t->start_addr = fptr;
	t->start_stack = stack_low+ KTHREAD_STACK_SIZE;
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

void user_process_add(unsigned long *fptr,char * name)
{
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
	t->start_addr = fptr;
	t->start_stack = stack_low + KTHREAD_STACK_SIZE;
	t->user_start_stack = user_stack_low + KTHREAD_STACK_SIZE;
	t->pid = pid;
	t->type = USER_PROCESS;
    t->timer.state = TIMER_UNUSED;
	kstrcpy(t->name,name);
	t->context_switches = 0;
	pid+=1;
}

void sched_stats()
{
	int i;

	for (i=0; i<max_task; i++) {
		kprint_hex("PID ",ktasks[i].pid);
		kprintf("  name:");
		kprintf(ktasks[i].name);
		kprintf("\n");
		kprint_hex("  stack:           0x",ktasks[i].start_stack);
		kprint_hex("  stack end:       0x",ktasks[i].start_stack-KTHREAD_STACK_SIZE);
		kprintf("  Process State:   ");
		kprintf(task_type_str[ktasks[i].state]);
		kprintf("\n");
		kprint_hex("  context switches:0x",ktasks[i].context_switches);
		kprintf("  Process Type:    ");
		kprintf(process_types_str[ktasks[i].type]);
		kprintf("\n");
		kprint_hex("  Sleep state:    ",ktasks[i].timer.state);
	}
}

void schedule()
{
    int i = next_task;
    int x = pid +i;
    asm("cli");   
    for (i = next_task; i < max_task; i++) {
        next_task = (next_task +1) %max_task;
        //kprint_hex("next ",next_task);
        if (update_timer(&ktasks[i].timer)) {
            ktasks[i].state = TASK_READY;
        }
        if (ktasks[i].timer.state == TIMER_RUNNING) {
            ktasks[i].state = TASK_BLOCKED;
            return;
        }

        if (prev_task != -1 && ktasks[prev_task].state != TASK_BLOCKED) {
            ktasks[prev_task].state = TASK_READY;
            //save old stack
            asm volatile("movq %%rsp ,%0" : "=g"(ktasks[prev_task].s_rsp));
            asm volatile("movq %%rbp ,%0" : "=g"(ktasks[prev_task].s_rbp));
            //kprint_hex("saved stack ",ktasks[prev_task].s_rsp);
        }
        if (ktasks[i].state == TASK_NEW && ktasks[i].type == KERNEL_PROCESS) {
            ktasks[i].state = TASK_RUNNING;
            prev_task=i;
            switch_to(ktasks[i].start_stack,ktasks[i].start_addr);
            return;
        }
        else if (ktasks[i].state == TASK_NEW && ktasks[i].type == USER_PROCESS) {
            ktasks[i].state = TASK_RUNNING;
            set_tss_rsp(ktasks[i].start_stack); // Set the kernel stack pointer.
            prev_task=i;
            jump_usermode(ktasks[i].start_addr,ktasks[i].user_start_stack);
            return;
        }
        else if (ktasks[i].state == TASK_READY){
            ktasks[i].state = TASK_RUNNING;
            ktasks[i].context_switches += 1;
            set_tss_rsp(ktasks[i].start_stack); // Set the kernel stack pointer.
            prev_task=i;
            //kprintf("Resuming:");
			//kprintf(ktasks[i].name);
			//kprintf("\n");
			resume_p(ktasks[i].s_rsp,ktasks[i].s_rbp);
			
            return;
        }	
    }
}
