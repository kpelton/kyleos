#include <output/output.h>
#include <sched/sched.h>
#include <init/tables.h>
#include <timer/timer.h>
#include <mm/paging.h>
#include <mm/mm.h>

static struct ktask ktasks[SCHED_MAX_TASKS];
static uint32_t max_task = 0;
static uint32_t next_task = 0;
static int  prev_task = -1;
static int pid = 1;

void switch_to(uint64_t *rsp,uint64_t *addr);
void resume_p(uint64_t *rsp,uint64_t *rbp);
static const char process_types_str [PROCESS_TYPES_LEN][20] = {"Kernel","User"};
static const char task_type_str [TASK_STATE_NUM][20] = {"RUNNING","READY","NEW","BLOCKED","DONE"};

void ksleepm(uint32_t ms) {
    struct ktask *process = get_current_process();
    process->timer = new_timer(ms); 
    //yield to another process
    schedule();
	//asm("sti");
}

void kthread_add(void (*fptr)(),char * name) {
	struct ktask *t;
	uint64_t stack_low;
	t = &ktasks[max_task];
	t->pg_tbl = NULL;
	stack_low = (uint64_t) kmalloc(KTHREAD_STACK_SIZE);
	max_task += 1;
	t->state = TASK_NEW;
	t->start_addr = (uint64_t *) fptr;
	t->start_stack =(uint64_t *) ( stack_low + KTHREAD_STACK_SIZE);
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

void user_process_add(void (*fptr)(),char *name) {
	struct ktask *t;
	uint64_t stack_low;
	uint64_t user_stack_low;
	t = &ktasks[max_task];

	//kprintf("Allocating Stack\n");
	stack_low = (uint64_t) kmalloc(KTHREAD_STACK_SIZE);
	user_stack_low = (uint64_t) kmalloc(KTHREAD_STACK_SIZE);
	t->pg_tbl = (uint64_t *) kmalloc(sizeof(struct pg_tbl));
	user_setup_paging(t->pg_tbl,fptr,0,100);
	paging_map_user_range(t->pg_tbl,user_stack_low,0x8000,8);
	max_task += 1;

	t->state = TASK_NEW;
	t->start_addr = (uint64_t)fptr-addr_start;
	t->start_stack = (uint64_t *) (stack_low + KTHREAD_STACK_SIZE);
	t->user_start_stack = (uint64_t *) (0x8000 + KTHREAD_STACK_SIZE);
	t->pid = pid;
	t->type = USER_PROCESS;
    t->timer.state = TIMER_UNUSED;
	kstrcpy(t->name,name);
	t->context_switches = 0;
	pid+=1;
}

void sched_stats() {
	uint32_t i;

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
    uint32_t i = next_task;
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
			//kernel_switch_paging();
            switch_to(ktasks[i].start_stack,ktasks[i].start_addr);
        }
        else if (ktasks[i].state == TASK_NEW && ktasks[i].type == USER_PROCESS) {
            ktasks[i].state = TASK_RUNNING;
            set_tss_rsp(ktasks[i].start_stack); // Set the kernel stack pointer.
			user_switch_paging(ktasks[i].pg_tbl);
			
            jump_usermode((uint64_t)ktasks[i].start_addr &0xfff,ktasks[i].user_start_stack);
        }
        else if (ktasks[i].state == TASK_READY){
            ktasks[i].state = TASK_RUNNING;
            ktasks[i].context_switches += 1;
            set_tss_rsp(ktasks[i].start_stack); // Set the kernel stack pointer.
			
			if (ktasks[i].type == USER_PROCESS){
				user_switch_paging(ktasks[i].pg_tbl);
			}
			resume_p(ktasks[i].s_rsp,ktasks[i].s_rbp);

        }	
        return;
    }
}
