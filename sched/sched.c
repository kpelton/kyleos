#include <output/output.h>
#include <sched/sched.h>
#include <init/tables.h>
#include <timer/timer.h>
#include <mm/paging.h>
#include <mm/mm.h>
#include <mm/pmem.h>

#define USER_STACK_VADDR 0x600000000
#define USER_STACK_SIZE 32
#define IDLE_PID 0

static struct ktask ktasks[SCHED_MAX_TASKS];
static uint32_t next_task = 0;
static int prev_task = -1;
static int pid = 0;


void switch_to(uint64_t *rsp, uint64_t *addr);
void resume_p(uint64_t *rsp, uint64_t *rbp);
static const char process_types_str[PROCESS_TYPES_LEN][20] = {"Kernel", "User"};
const char task_type_str[TASK_STATE_NUM][20] = {"RUNNING", "READY", "NEW", "BLOCKED", "DONE"};

void ksleepm(uint32_t ms)
{
    struct ktask *process = get_current_process();
    process->timer = new_timer(ms);
    // yield to another process
    schedule();
    // asm("sti");
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
    while (found_task != -1 && ktasks[i].state != TASK_DONE)
    {
        i = (i + 1) % SCHED_MAX_TASKS;
        found_task = ktasks[i].pid;
        trys++;
        if (trys == SCHED_MAX_TASKS * 2)
            panic("No free tasks");
        // kprintf("t:%d %d\n",found_task,i);
    }
    // kprintf("f:%d\n",i);
    return i;
}
static void clear_fd_table(struct ktask *t)
{
    for (int j; j < MAX_TASK_OPEN_FILES; j++)
        t->open_fds[j] = NULL;
}

void kthread_add(void (*fptr)(), char *name)
{
    struct ktask *t;

    t = &ktasks[find_free_task()];
    clear_fd_table(t);

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
    t->parent = -1;
    pid += 1;
}

struct ktask *get_current_process()
{
    return &ktasks[prev_task];
}

struct ktask *sched_get_process(int pid)
{
    for (int i = 0; i < SCHED_MAX_TASKS; i++)
        if (ktasks[i].pid == pid)
            return &ktasks[i];

    return NULL;
}

void sched_save_context(uint64_t rip, uint64_t rsp)
{
    struct ktask *c = get_current_process();
    c->save_rip = (uint64_t *)rip;
    c->save_rsp = (uint64_t *)rsp;
}

int user_process_fork()
{
    // TODO:Change below to copy on write

    struct ktask *curr = get_current_process();
    struct ktask *t;
    struct p_memblock *c_mem;
    struct p_memblock *track = NULL;
    struct p_memblock *head = NULL;
    t = &ktasks[find_free_task()];
    clear_fd_table(t);
    t->stack_alloc = (uint64_t *)kmalloc(KTHREAD_STACK_SIZE);
    t->user_stack_alloc = (uint64_t *)KERN_PHYS_TO_VIRT(pmem_alloc_block(USER_STACK_SIZE));
    t->mm = (struct pg_tbl *)kmalloc(sizeof(struct pg_tbl));
    t->mm->pml4 = (uint64_t *)KERN_PHYS_TO_PVIRT(pmem_alloc_zero_page());
    paging_map_user_range(t->mm, (uint64_t)KERN_VIRT_TO_PHYS(t->user_stack_alloc), USER_STACK_VADDR, USER_STACK_SIZE, USER_PAGE);
    t->state = TASK_READY;
    t->type = USER_PROCESS;
    t->timer.state = TIMER_UNUSED;
    t->start_stack = (uint64_t *)((uint64_t)t->stack_alloc + KTHREAD_STACK_SIZE) - 16;
    t->user_start_stack = curr->user_start_stack;
    t->pid = pid;
    t->mem_list = NULL;
    t->context_switches = 0;
    t->parent = curr->pid;

    kstrcpy(t->name, curr->name);
    // Copy over parent data using brute force;

    memcpy8((uint8_t *)KERN_PHYS_TO_PVIRT(KERN_VIRT_TO_PHYS((uint64_t)t->user_stack_alloc)),
            (uint8_t *)KERN_PHYS_TO_PVIRT(KERN_VIRT_TO_PHYS((uint64_t)curr->user_stack_alloc)),
            USER_STACK_SIZE * PAGE_SIZE);

    t->s_rbp = (uint64_t *)curr->s_rbp;
    t->s_rsp = (uint64_t *)t->start_stack;
    // Context stack frame is 1 past the return address
    struct reg_state *rptr = (struct reg_state *)(curr->save_rsp + 1);
    *(--t->s_rsp) = rptr->user_ss;
    *(--t->s_rsp) = rptr->user_rsp;
    *(--t->s_rsp) = rptr->user_flags;
    *(--t->s_rsp) = rptr->user_cs;
    *(--t->s_rsp) = rptr->user_ret_addr;
    // set return val RAX = 0 on child PID
    *(--t->s_rsp) = 0;
    *(--t->s_rsp) = rptr->rbx;
    *(--t->s_rsp) = rptr->rcx;
    *(--t->s_rsp) = rptr->rdx;
    *(--t->s_rsp) = rptr->rsi;
    *(--t->s_rsp) = rptr->rdi;
    *(--t->s_rsp) = rptr->rbp;
    *(--t->s_rsp) = rptr->r8;
    *(--t->s_rsp) = rptr->r9;
    *(--t->s_rsp) = rptr->r10;
    *(--t->s_rsp) = rptr->r11;
    *(--t->s_rsp) = rptr->r12;
    *(--t->s_rsp) = rptr->r13;
    *(--t->s_rsp) = rptr->r14;
    *(--t->s_rsp) = rptr->r15;
    *(--t->s_rsp) = rptr->flags;
    // Set return addr
    *(t->s_rsp - 1) = (uint64_t)curr->save_rip;
    // Set parent return value
    rptr->rax = pid;

    for (int j; j < MAX_TASK_OPEN_FILES; j++)
    {
        t->open_fds[j] = curr->open_fds[j];
        if (t->open_fds[j] != NULL)
            t->open_fds[j]->refcount++;
    }

    // copy over allocated memory pages to process
    c_mem = curr->mem_list;
    while (c_mem != NULL)
    {
        // track pages allocated for program image in stack linked list
        track = kmalloc(sizeof(struct p_memblock));
        track->block = pmem_alloc_block(c_mem->count);
        track->vaddr = c_mem->vaddr;
        track->pg_opts = c_mem->pg_opts;
        track->count = c_mem->count;
        if (head == NULL)
        {
            head = track;
        }
        else
        {
            track->next = head;
            head = track;
        }
        memcpy8((uint8_t *)KERN_PHYS_TO_PVIRT(track->block), (uint8_t *)KERN_PHYS_TO_PVIRT(c_mem->block), c_mem->count * PAGE_SIZE);
        paging_map_user_range(t->mm, (uint64_t)track->block, track->vaddr, track->count, track->pg_opts);

        c_mem = c_mem->next;
    }
    t->mem_list = head;

    pid += 1;
    // Return will not apply since eax will be popped off stack
    return -1;
}

int user_process_replace_exec(struct ktask *t, uint64_t startaddr,char *name,struct pg_tbl *tbl, struct p_memblock *head)
{
    int pid = t->pid;
    int parent = t->parent;
    sched_process_kill(t->pid,true);
    clear_fd_table(t);
    t->stack_alloc = (uint64_t *)kmalloc(KTHREAD_STACK_SIZE);
    t->user_stack_alloc = (uint64_t *)KERN_PHYS_TO_VIRT(pmem_alloc_block(USER_STACK_SIZE));
    t->mm = tbl;
    paging_map_user_range(t->mm, (uint64_t)KERN_VIRT_TO_PHYS(t->user_stack_alloc), USER_STACK_VADDR, USER_STACK_SIZE, USER_PAGE);
    t->state = TASK_NEW;
    t->start_addr = (uint64_t *)startaddr;
    t->start_stack = (uint64_t *)((uint64_t)t->stack_alloc + KTHREAD_STACK_SIZE) - 16;
    t->s_rbp = t->user_start_stack;
    t->user_start_stack = (uint64_t *)(USER_STACK_VADDR + (PAGE_SIZE * USER_STACK_SIZE) - 16);
    t->pid = pid;
    t->parent = parent;
    t->type = USER_PROCESS;
    t->timer.state = TIMER_UNUSED;
    t->mem_list = head;
    kstrcpy(t->name, name);
    t->context_switches = 0;
    kprintf("All done!\n");
    schedule();

    //will never return
    return 0;
}



int user_process_add_exec(uint64_t startaddr, char *name, struct pg_tbl *tbl, struct p_memblock *head)
{
    struct ktask *t;
    t = &ktasks[find_free_task()];
    int pid_ret;

    clear_fd_table(t);

    // kprintf("Allocating Stack\n");
    t->stack_alloc = (uint64_t *)kmalloc(KTHREAD_STACK_SIZE);
    t->user_stack_alloc = (uint64_t *)KERN_PHYS_TO_VIRT(pmem_alloc_block(USER_STACK_SIZE));
    t->mm = tbl;
    paging_map_user_range(t->mm, (uint64_t)KERN_VIRT_TO_PHYS(t->user_stack_alloc), USER_STACK_VADDR, USER_STACK_SIZE, USER_PAGE);
    t->state = TASK_NEW;
    t->start_addr = (uint64_t *)startaddr;
    t->start_stack = (uint64_t *)((uint64_t)t->stack_alloc + KTHREAD_STACK_SIZE) - 16;
    t->s_rbp = t->user_start_stack;
    t->user_start_stack = (uint64_t *)(USER_STACK_VADDR + (PAGE_SIZE * USER_STACK_SIZE) - 16);
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

// Free list of used blocks allocated for program image
static void free_memblock_list(struct p_memblock *head)
{
    struct p_memblock *p = head;
    struct p_memblock *curr = NULL;
    uint32_t j = 0;

    while (p != NULL)
    {
        curr = p;
        p = p->next;
        for (j = 0; j < curr->count; j++)
        {
#ifdef SCHED_DEBUG
            kprintf("Freeing 0x%x\n", (uint64_t)curr->block + (PAGE_SIZE * j));
#endif
            pmem_free_block((uint64_t)curr->block + (PAGE_SIZE * j));
        }
        kfree(curr);
    }
}

bool sched_process_kill(int pid, bool cleanup)
{
    int i;
    int j;
    asm("cli");
#ifdef SCHED_DEBUG
    kprintf("Killing %d\n", pid);
#endif
    for (i = 0; i < SCHED_MAX_TASKS; i++)
    {

        if (ktasks[i].pid != -1 && ktasks[i].pid == pid && ktasks[i].state != TASK_DONE)
        {

            struct ktask *t;
            t = &ktasks[i];

            if (t->type == USER_PROCESS)
            {
                paging_free_pg_tbl(t->mm);
                kfree(t->mm);
                t->mm = NULL;
                for (j = 0; j < USER_STACK_SIZE; j++)
                    pmem_free_block(KERN_VIRT_TO_PHYS(t->user_stack_alloc + (PAGE_SIZE * j)));

                free_memblock_list(t->mem_list);
            }
            else
            {
                // Kernel threads do not need to be "reaped"
                t->pid = -1;
            }
            kfree(t->stack_alloc);
#ifdef SCHED_DEBUG
            kprintf("Killed %d\n", t->pid);
#endif
            // Close any opened files
            for (int j; j < MAX_TASK_OPEN_FILES; j++)
                if (t->open_fds[j] != NULL)
                {
                    vfs_close_file(t->open_fds[j]);
                    t->open_fds[j] = NULL;
                }

            t->state = TASK_DONE;
            // TODO: have init process handle reaping all pids and update children PIDs
            if (cleanup == true || t->parent <= 0)
                t->pid = -1;
            t->mem_list = NULL;

            goto done;
            // manually
        }
        else if (ktasks[i].pid == pid && ktasks[i].state == TASK_DONE)
        {
            ktasks[i].pid = -1;
            goto done;
        }
    }
    asm("sti");
    return false;

done:
    asm("sti");
    return true;
}

void sched_stats()
{
    uint32_t i;
    uint32_t j;
    for (i = 0; i < SCHED_MAX_TASKS; i++)
    {
        if (ktasks[i].pid != -1)
        {

            kprintf("PID:%d\n", ktasks[i].pid);
            kprintf("  parent:%d\n", ktasks[i].parent);
            kprintf("  name:%s\n", ktasks[i].name);
            kprintf("  stack:           0x%x\n", ktasks[i].start_stack);
            kprintf("  stack end:       0x%x\n", ktasks[i].start_stack - KTHREAD_STACK_SIZE);
            kprintf("  Process State:   %s\n", task_type_str[ktasks[i].state]);
            kprintf("  context switches:0x%x\n", ktasks[i].context_switches);
            kprintf("  Process Type:    %s\n", process_types_str[ktasks[i].type]);
            kprintf("  Sleep state:     %s\n", str_timer_states[ktasks[i].timer.state]);
            kprintf("  File Table\n");
            for (j = 0; j < MAX_TASK_OPEN_FILES; j++)
            {
                kprintf("    %d -> %x\n", j, ktasks[i].open_fds[j]);
                if (ktasks[i].open_fds[j] != NULL)
                {

                    kprintf("         refcount -> %d\n", ktasks[i].open_fds[j]->refcount);
                    kprintf("         flags -> 0x%x\n", ktasks[i].open_fds[j]->flags);
                }
            }
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
        if (ktasks[i].pid != -1 && ktasks[i].state != TASK_DONE)
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
    // kprintf("schedule\n");
    bool success = false;
    if (prev_task != -1 && ktasks[prev_task].state != TASK_BLOCKED && ktasks[prev_task].state != TASK_NEW && ktasks[prev_task].state != TASK_DONE )
    {
        ktasks[prev_task].state = TASK_READY;
        // save old stack
        asm volatile("movq %%rsp ,%0"
                     : "=g"(ktasks[prev_task].s_rsp));
        asm volatile("movq %%rbp ,%0"
                     : "=g"(ktasks[prev_task].s_rbp));
    }
    while (success == false)
    {
        // only set to ready if someone else has not changed the state
        if (prev_task != -1 && ktasks[prev_task].state != TASK_BLOCKED && ktasks[prev_task].state != TASK_NEW && ktasks[prev_task].state != TASK_DONE )
        {
            ktasks[prev_task].state = TASK_READY;
            // save old stack
        }

        int i = next_task;
        bool skip_idle = false;
        update_timers();
        next_task = find_next_task(i);

        //If we are on the idle PID skip it if there is something else to run
        if (i == IDLE_PID) {
            for (int j = IDLE_PID+1; j < SCHED_MAX_TASKS; j++)
                if(ktasks[j].pid != -1 && (ktasks[j].state == TASK_NEW || ktasks[j].state == TASK_READY)) {
                    skip_idle = true;
                    break;
                }
            if(skip_idle == true){
                continue; // go to next task
            }else{
                //kprintf("idle\n");
            }
        }

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
            // kprintf("%d %x\n",i,ktasks[i].s_rsp);
            resume_p(ktasks[i].s_rsp, ktasks[i].s_rbp);
            success = true;
        }
    }
}
