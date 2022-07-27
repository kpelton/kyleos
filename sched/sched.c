#include <output/output.h>
#include <sched/sched.h>
#include <init/tables.h>
#include <timer/timer.h>
#include <mm/paging.h>
#include <mm/mm.h>
#include <mm/pmem.h>

#define USER_STACK_VADDR 0x60000000
#define USER_STACK_SIZE 32

static struct ktask ktasks[SCHED_MAX_TASKS];
static uint32_t next_task = 0;
static int prev_task = -1;
static int pid = 0;

void switch_to(uint64_t *rsp, uint64_t *addr);
void resume_p(uint64_t *rsp, uint64_t *rbp);
static const char process_types_str[PROCESS_TYPES_LEN][20] = {"Kernel", "User"};
static const char task_type_str[TASK_STATE_NUM][20] = {"RUNNING", "READY", "NEW", "BLOCKED", "DONE"};

void ksleepm(uint32_t ms)
{
	struct ktask *process = get_current_process();
	process->timer = new_timer(ms);
	//yield to another process
	schedule();
	//asm("sti");
}

void sched_init()
{
	int i;
	for (i = 0; i < SCHED_MAX_TASKS; i++)
		ktasks[i].pid = -1;
}

static int find_free_task()
{

	int i = 0;
	int found_task = ktasks[i].pid;
	int trys = 0;
	while (found_task != -1)
	{
		i = (i + 1) % SCHED_MAX_TASKS;
		found_task = ktasks[i].pid;
		trys++;
		if (trys == SCHED_MAX_TASKS * 2)
			panic("No free tasks");
	//kprintf("t:%d %d\n",found_task,i);


	}
	//kprintf("f:%d\n",i);
	return i;
}

void kthread_add(void (*fptr)(), char *name)
{
	struct ktask *t;

	t = &ktasks[find_free_task()];
	t->mm = NULL;
	t->stack_alloc = (uint64_t *)kmalloc(KTHREAD_STACK_SIZE);
	t->state = TASK_NEW;
	t->start_addr = (uint64_t *)fptr;
	t->start_stack = (uint64_t *)((uint64_t)t->stack_alloc + KTHREAD_STACK_SIZE);
	t->user_stack_alloc = NULL;
	t->user_start_stack = NULL;
	t->pid = pid;
	t->type = KERNEL_PROCESS;
	t->timer.state = TIMER_UNUSED;
	t->mem_list = NULL;
	kstrcpy(t->name, name);
	t->context_switches = 0;
	pid += 1;
}

struct ktask *get_current_process()
{
	return &ktasks[prev_task];
}

int user_process_add(void (*fptr)(), char *name)
{
	struct ktask *t;
	t = &ktasks[find_free_task()];
	int pid_ret;

	//kprintf("Allocating Stack\n");
	t->stack_alloc = (uint64_t *)kmalloc(KTHREAD_STACK_SIZE);
	t->user_stack_alloc = (uint64_t *)KERN_PHYS_TO_VIRT(pmem_alloc_block(32));
	t->mm = (struct pg_tbl *)kmalloc(sizeof(struct pg_tbl));
	t->mm->pml4 = (uint64_t *)KERN_PHYS_TO_PVIRT(pmem_alloc_zero_page());
	kprintf("sched pml4 %x\n",t->mm->pml4);
	paging_map_user_range(t->mm,(uint64_t) KERN_VIRT_TO_PHYS((uint64_t)fptr), 0, 1,USER_PAGE_RO);
	paging_map_user_range(t->mm,(uint64_t) KERN_VIRT_TO_PHYS(t->user_stack_alloc), USER_STACK_VADDR,USER_STACK_SIZE,USER_PAGE);

	t->state = TASK_NEW;
	t->start_addr = (uint64_t *)(((uint64_t)fptr - addr_start) &0xfff);
	kprintf("start add 0x%x\n",t->start_addr);
	t->start_stack = (uint64_t *)((uint64_t)t->stack_alloc + KTHREAD_STACK_SIZE) -16;
	t->user_start_stack = (uint64_t *)(USER_STACK_VADDR + (4096*USER_STACK_SIZE) -16 );
	t->pid = pid;
	t->type = USER_PROCESS;
	t->timer.state = TIMER_UNUSED;
	t->mem_list = NULL;
	kstrcpy(t->name, name);
	t->context_switches = 0;
	pid_ret = pid;
	pid += 1;
	return pid_ret;

}

int user_process_add_exec(uint64_t startaddr, char *name,struct pg_tbl *tbl,struct p_memblock *head)
{
	struct ktask *t;
	t = &ktasks[find_free_task()];
	int pid_ret;

	//kprintf("Allocating Stack\n");
	t->stack_alloc = (uint64_t *)kmalloc(KTHREAD_STACK_SIZE);
	t->user_stack_alloc = (uint64_t *)KERN_PHYS_TO_VIRT(pmem_alloc_block(32));
	t->mm = tbl;
	paging_map_user_range(t->mm,(uint64_t) KERN_VIRT_TO_PHYS(t->user_stack_alloc), USER_STACK_VADDR,USER_STACK_SIZE,USER_PAGE);
	t->state = TASK_NEW;
	t->start_addr =  (uint64_t *) startaddr;
	t->start_stack = (uint64_t *)((uint64_t)t->stack_alloc + KTHREAD_STACK_SIZE);
	t->user_start_stack = (uint64_t *)(USER_STACK_VADDR + (4096*USER_STACK_SIZE) -16);
	t->pid = pid;
	t->type = USER_PROCESS;
	t->timer.state = TIMER_UNUSED;
	t->mem_list = head;
	kstrcpy(t->name, name);
	t->context_switches = 0;
	pid_ret = pid;
	pid += 1;
	return pid_ret;

}

//Free list of used blocks allocated for program image
static void free_memblock_list(struct p_memblock *head) {
	struct p_memblock *p = head;
	struct p_memblock *curr = NULL;
	uint32_t j = 0;

	while(p != NULL) {
		curr = p;
		p = p->next;
		for(j =0; j<curr->count; j++) {
			kprintf("Freeing 0x%x\n",(uint64_t)curr->block+(PAGE_SIZE*j));
			pmem_free_block((uint64_t)curr->block+(PAGE_SIZE*j));	
			}
		kfree(curr);
	}
}

bool sched_process_kill(int pid)
{
	int i;
	int j;
	kprintf("Killing %d\n", pid);
	for (i = 0; i < SCHED_MAX_TASKS; i++)
	{

		if (ktasks[i].pid != -1 && ktasks[i].pid == pid)
		{

			struct ktask *t;
			t = &ktasks[i];

			if (t->type == USER_PROCESS)
			{
				paging_free_pg_tbl(t->mm);
				kfree(t->mm);
				t->mm = NULL;
				for( j = 0; j<USER_STACK_SIZE; j++)
					pmem_free_block(KERN_VIRT_TO_PHYS(t->user_stack_alloc+(PAGE_SIZE*j)));

				free_memblock_list(t->mem_list);

			}
			kfree(t->stack_alloc);
			kprintf("Killed %d\n", t->pid);
			t->state = TASK_DONE;
			t->pid = -1;
			t->mem_list = NULL;

			return true;
		}
	}
	return false;
}

void sched_stats()
{
	uint32_t i;

	for (i = 0; i < SCHED_MAX_TASKS; i++)
	{
		if (ktasks[i].pid != -1)
		{

			kprintf("PID %d\n", ktasks[i].pid);
			kprintf("  name:%s\n", ktasks[i].name);
			kprintf("  stack:           0x%x\n", ktasks[i].start_stack);
			kprintf("  stack end:       0x%x\n", ktasks[i].start_stack - KTHREAD_STACK_SIZE);
			kprintf("  Process State:   %s\n", task_type_str[ktasks[i].state]);
			kprintf("  context switches:0x%x\n", ktasks[i].context_switches);
			kprintf("  Process Type:    %s\n", process_types_str[ktasks[i].type]);
			kprintf("  Sleep state:     %s\n", str_timer_states[ktasks[i].timer.state]);
		}
	}
}



static int find_next_task(int current_task)
{

	int i = current_task;
	int found_task = -1;
	int trys = 0;
	while (found_task == -1)
	{

		i = (i + 1) % SCHED_MAX_TASKS;
		if (ktasks[i].pid != -1)
		{
			found_task = i;
		}
		trys++;
		if (trys == SCHED_MAX_TASKS * 2)
			panic("Could not find a task to run!");
	}

	return found_task;
}

static void update_timers()
{
	int i;
	for (i = 0; i < SCHED_MAX_TASKS; i++)
	{
		if (update_timer(&ktasks[i].timer))
		{
			ktasks[i].state = TASK_READY;
		}
		else if (ktasks[i].timer.state == TIMER_RUNNING)
		{
			ktasks[i].state = TASK_BLOCKED;
		}
	}
}

void schedule()
{
	asm("cli");
	//kprintf("schedule\n");
	bool success = false;
	if (prev_task != -1 && ktasks[prev_task].state != TASK_BLOCKED && ktasks[prev_task].state != TASK_DONE)
	{
		ktasks[prev_task].state = TASK_READY;
		//save old stack
		asm volatile("movq %%rsp ,%0"
					 : "=g"(ktasks[prev_task].s_rsp));
		asm volatile("movq %%rbp ,%0"
					 : "=g"(ktasks[prev_task].s_rbp));
	}

	while (success == false)
	{
		int i = next_task;

		next_task = find_next_task(i);
		update_timers();

		prev_task = i;
		if (ktasks[i].state == TASK_NEW && ktasks[i].type == KERNEL_PROCESS)
		{
			ktasks[i].state = TASK_RUNNING;
			switch_to(ktasks[i].start_stack, ktasks[i].start_addr);
			success = true;
		}
		else if (ktasks[i].state == TASK_NEW && ktasks[i].type == USER_PROCESS)
		{
			ktasks[i].state = TASK_RUNNING;
			set_tss_rsp(ktasks[i].start_stack); // Set the kernel stack pointer.
			user_switch_paging(ktasks[i].mm);

			jump_usermode((uint64_t)ktasks[i].start_addr, ktasks[i].user_start_stack);
			success = true;
		}
		else if (ktasks[i].state == TASK_READY)
		{
			ktasks[i].state = TASK_RUNNING;
			ktasks[i].context_switches += 1;
			set_tss_rsp(ktasks[i].start_stack); // Set the kernel stack pointer.

			if (ktasks[i].type == USER_PROCESS)
			{
				user_switch_paging(ktasks[i].mm);
			}
			resume_p(ktasks[i].s_rsp, ktasks[i].s_rbp);
			success = true;
		}
	}
}
