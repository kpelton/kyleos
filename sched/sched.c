#include <output/output.h>
#include <sched/sched.h>
#include <init/tables.h>
#include <sched/exec.h>
#include <timer/timer.h>
#include <mm/paging.h>
#include <mm/mm.h>
#include <mm/pmem.h>
#include <asm/asm.h>
#include <locks/mutex.h>
#include <locks/spinlock.h>


static struct ktask ktasks[SCHED_MAX_TASKS];
static uint32_t next_task = 0;
static int prev_task = -1;
static int pid = 0;
static struct mutex sched_mutex;
static struct spinlock sched_spinlock;


void switch_to(uint64_t *rsp, uint64_t *addr);
extern void resume_p(uint64_t *rsp, uint64_t *rbp);
extern void resume_p_userspace(uint64_t *rsp, uint64_t *rbp);

static const char process_types_str[PROCESS_TYPES_LEN][20] = {"Kernel", "User"};
const char task_type_str[TASK_STATE_NUM][20] = {"RUNNING", "READY", "NEW", "BLOCKED", "DONE"};
//#define SCHED_DEBUG
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
    init_mutex(&sched_mutex);
    init_spinlock(&sched_spinlock);
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
        // kprintf("t:%d %d\n",found_task,i);
    }
    // kprintf("f:%d\n",i);
    return i;
}
static void clear_fd_table(struct ktask *t)
{
    for (int j = 0; j < MAX_TASK_OPEN_FILES; j++)
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
    kstrcpy(t->name, name);
    t->context_switches = 0;
    t->parent = -1;
    pid += 1;
}

struct ktask *get_current_process()
{
    struct ktask *val;
    val = &ktasks[prev_task];
    return val;
}

struct ktask *sched_get_process(int pid)
{
    acquire_spinlock(&sched_spinlock);
    for (int i = 0; i < SCHED_MAX_TASKS; i++)
    {
        if (ktasks[i].pid == pid)
        {
            release_spinlock(&sched_spinlock);
            return &ktasks[i];
        }
    }
    release_spinlock(&sched_spinlock);

    return NULL;
}

void sched_save_context(uint64_t rip, uint64_t rsp)
{
    struct ktask *c = get_current_process();
    c->save_rip = (uint64_t *)rip;
    c->save_rsp = (uint64_t *)rsp;
    fpu_save_context(c->fxsave_region);
}

int user_process_fork()
{
    // TODO:Change below to copy on write
    acquire_spinlock(&sched_spinlock);
    struct ktask *curr = get_current_process();
    struct ktask *t;

    t = &ktasks[find_free_task()];
    clear_fd_table(t);
    t->stack_alloc = (uint64_t *)kmalloc(KTHREAD_STACK_SIZE);
    t->mm = vmm_map_new();

    vmm_copy_section(curr->mm,t->mm,VMM_STACK);
    vmm_copy_section(curr->mm,t->mm,VMM_TEXT);
    vmm_copy_section(curr->mm,t->mm,VMM_DATA);


    t->state = TASK_READY;
    t->type = USER_PROCESS;
    t->timer.state = TIMER_UNUSED;
    t->start_stack = (uint64_t *)((uint64_t)t->stack_alloc + KTHREAD_STACK_SIZE) - 8;
    t->user_start_stack = curr->user_start_stack;
    t->pid = pid;
    t->context_switches = 0;
    t->parent = curr->pid;
    t->user_start_heap = (uint64_t *) USER_HEAP_VADDR;
    t->user_heap_loc = curr->user_heap_loc;
    t->heap_size = curr->heap_size;
    kstrcpy(t->name, curr->name);

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

    for (int j = 0; j < MAX_TASK_OPEN_FILES; j++)
    {
        t->open_fds[j] = curr->open_fds[j];
        if (t->open_fds[j] != NULL)
            t->open_fds[j]->refcount++;
    }

    pid += 1;
    // Return will not apply since eax will be popped off stack
    release_spinlock(&sched_spinlock);
    return -1;
}


int user_process_replace_exec(struct ktask *t, uint64_t startaddr, char *name, struct vmm_map *mm,char *argv[])
{
    asm("cli");
    int c_pid = t->pid;
    kprintf("%d\n",c_pid);
    // save old pid and parent pid since kill will clear them
    sched_process_kill(t->pid,false);
    user_process_add_exec(startaddr,name,mm,true,argv,t);
    t->pid = c_pid;
    int i=0;
    //Free kernel allocated argv passed in from userspace
    for(i=0; i<MAX_ARGS && argv[i]; i++);

    if (i > 0) {
            for (int j=0; j<i; j++){
                kfree(argv[j]);
            }
            kfree(argv);
    }
    schedule();
    // will never return
    return 0;
}

int user_process_add_exec(uint64_t startaddr, char *name,struct vmm_map *mm,bool update_pid,char *argv[],struct ktask *ta)
{

    int pid_ret = -1;
    int argc = 0;
    struct vmm_block *stack_block = NULL;
    uint64_t *sp = NULL; 
    uint64_t *ustack[MAX_ARGS];
    kprintf("Allocating Stack\n");
    struct ktask *t;

    
    if (ta == NULL) {
        t = &ktasks[find_free_task()];
    } else {
        t= ta;
    }

        clear_fd_table(t);

    t->stack_alloc = (uint64_t *)kmalloc(KTHREAD_STACK_SIZE);
    stack_block = vmm_add_new_mapping(mm,VMM_STACK,USER_STACK_VADDR,USER_STACK_SIZE,READ_WRITE | SUPERVISOR | PAGE_PRESENT,true,true);
    vmm_add_new_mapping(mm,VMM_DATA,USER_HEAP_VADDR,USER_HEAP_SIZE,READ_WRITE | SUPERVISOR,true,true);
    user_switch_paging(&(mm->pagetable));


    t->heap_size = USER_HEAP_SIZE;
    t->user_start_heap = (uint64_t *)USER_HEAP_VADDR;
    t->user_heap_loc = (uint64_t *)((uint64_t)t->user_start_heap+t->heap_size*PAGE_SIZE);
    t->mm = mm;
    t->state = TASK_NEW;
    t->start_addr = (uint64_t *)startaddr;
    t->start_stack = (uint64_t *)((uint64_t)t->stack_alloc + KTHREAD_STACK_SIZE) - 8;
    t->s_rbp = t->user_start_stack;
    t->type = USER_PROCESS;
    t->timer.state = TIMER_UNUSED;
    kstrcpy(t->name, name);
    if (update_pid)
    {
        t->context_switches = 0;
        t->pid = pid;
        pid_ret = pid;
        pid += 1;
    }
    //TODO: Add more checks on arguments
    // max argument length and total size
    // Switch to new page able in order to copy in arguments into stack pages
    sp = (uint64_t *)((uint64_t)USER_STACK_VADDR + (PAGE_SIZE * USER_STACK_SIZE) - 8);
    if( argv) {

        //Copy name of prog in argv[0]
        /*
        int len = kstrlen(name);
        sp = (uint64_t)(((char*)sp) - len) & 0xffffffffffffff00;
        ustack[argc] = sp;
        kstrncpy(sp,name,len);
*/
        for(argc = 0; argv[argc] && argc < MAX_ARGS; argc++) 
        {
            //kprintf(" %d %s\n",argc, argv[argc]);
            int len = kstrlen(argv[argc]);
            sp = (uint64_t)(((char*)sp) - len) & 0xffffffffffffff00;
            // track where argument is placed for later use
            ustack[argc] = sp;
            kstrncpy(sp,argv[argc],len);
            //kprintf("ustack %x %x\n", argc,sp);
        }
    }
    
    //Fix up stack for arguments
    sp-=argc+8;

    for(int i=0; i<=argc; i++)
        sp[i+3] = (uint64_t) ustack[i];

    sp[2] = sp+3;  
    sp[1] = argc;
    sp[0] = 0xdeadbeefdeadbeef;
    t->user_start_stack = sp;

    //kprintf("Fin stack 0x%x\n",sp);
    //kernel_switch_paging();
    //kprintf("Add exec done");
    return pid_ret;
}

bool sched_process_kill(int pid, bool cleanup)
{
    int i;
    int j;
    acquire_spinlock(&sched_spinlock);
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
                vmm_free(ktasks[i].mm);
            }
            else
            {
                // Kernel threads do not need to be "reaped"
                t->pid = -1;
            }
#ifdef SCHED_DEBUG
            kprintf("Killed %d\n", t->pid);
#endif
            // Close any opened files
            for (j = 0; j < MAX_TASK_OPEN_FILES; j++)
                if (t->open_fds[j] != NULL)
                {
                    vfs_close_file(t->open_fds[j]);
                    t->open_fds[j] = NULL;
                }

            t->state = TASK_DONE;
            // TODO: have init process handle reaping all pids and update children PIDs
            if (cleanup == true || t->parent <= 0)
                t->pid = -1;
            kfree(t->stack_alloc);
            t->stack_alloc = NULL;
            goto done;
            // manually
        }
        else if (ktasks[i].pid == pid && ktasks[i].state == TASK_DONE)
        {
            ktasks[i].pid = -1;
            goto done;
        }
    }
    release_spinlock(&sched_spinlock);
    return false;

done:
    release_spinlock(&sched_spinlock);

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
            kprintf("  stack:           0x%x\n", ktasks[i].stack_alloc);
            kprintf("  Process State:   %s\n", task_type_str[ktasks[i].state]);
            kprintf("  context switches:0x%x\n", ktasks[i].context_switches);
            kprintf("  Process Type:    %s\n", process_types_str[ktasks[i].type]);
            kprintf("  Sleep state:     %s\n", str_timer_states[ktasks[i].timer.state]);
            kprintf("  User Heap size   %d\n", ktasks[i].heap_size);
            if(ktasks[i].type == USER_PROCESS) {
                kprintf("  Total pages      %d\n", ktasks[i].mm->total_pages);
                kprintf("      Stack pages      %d\n", ktasks[i].mm->vmm_areas[VMM_STACK]->count);
                kprintf("      Text pages      %d\n", ktasks[i].mm->vmm_areas[VMM_TEXT]->count);
                kprintf("      Data pages      %d\n", ktasks[i].mm->vmm_areas[VMM_DATA]->count);
            }
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
    acquire_spinlock(&sched_spinlock);
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
    release_spinlock(&sched_spinlock);
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
    bool skip_idle = false;
    uint64_t resume_func;

    if (prev_task != -1 && ktasks[prev_task].state != TASK_BLOCKED && ktasks[prev_task].state != TASK_NEW && ktasks[prev_task].state != TASK_DONE)
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
        if (prev_task != -1 && ktasks[prev_task].state != TASK_BLOCKED && ktasks[prev_task].state != TASK_NEW && ktasks[prev_task].state != TASK_DONE)
        {
            ktasks[prev_task].state = TASK_READY;
            // save old stack
        }

        int i = next_task;
        update_timers();
        next_task = find_next_task(i);

        // If we are on the idle PID skip it if there is something else to run
        if (i == IDLE_PID)
        {
            skip_idle = false;

            for (int j = IDLE_PID + 1; j < SCHED_MAX_TASKS; j++)
                if (ktasks[j].pid != -1 && (ktasks[j].state == TASK_NEW || ktasks[j].state == TASK_READY))
                {
                    skip_idle = true;
                    break;
                }
            if (skip_idle == true)
            {
                continue; // go to next task
            }
            else
            {
                // kprintf("idle\n");
            }
        }

        prev_task = i;
        if (ktasks[i].state == TASK_NEW && ktasks[i].type == KERNEL_PROCESS)
        {

            set_tss_rsp(ktasks[i].start_stack); // Set the kernel stack pointer.
            ktasks[i].state = TASK_RUNNING;
            kernel_switch_paging();
            switch_to(ktasks[i].start_stack, ktasks[i].start_addr);
            success = true;

        }
        else if (ktasks[i].state == TASK_NEW && ktasks[i].type == USER_PROCESS)
        {
            ktasks[i].state = TASK_RUNNING;
            set_tss_rsp(ktasks[i].start_stack); // Set the kernel stack pointer.
            user_switch_paging(&(ktasks[i].mm->pagetable));

            asm volatile("movq %0,%%rdi\n\t"
                         "movq %1,%%rsi\n\t"
                         "movq %2, %%rax\n\t"
                         "callq *%%rax" ::"g"(ktasks[i].start_addr),
                         "g"(ktasks[i].user_start_stack), "g"(&jump_usermode)
                         : "rdi", "rsi", "rax");

            success = true;
        }
        else if (ktasks[i].state == TASK_READY)
        {
            ktasks[i].state = TASK_RUNNING;
            ktasks[i].context_switches += 1;
            set_tss_rsp(ktasks[i].start_stack); // Set the kernel stack pointer.

            if (ktasks[i].type == USER_PROCESS)
            {

                fpu_restore_context(ktasks[i].fxsave_region);
                user_switch_paging(&(ktasks[i].mm->pagetable));
                resume_func = (uint64_t) resume_p_userspace;
            }
            else
            {
                kernel_switch_paging();
                resume_func = (uint64_t) resume_p;
            }
            // resume_p(ktasks[i].s_rsp, ktasks[i].s_rbp);
            /// Need to call fucntion with inline asm  due to issues with -O2

            asm volatile("movq %0,%%rdi\n\t"
                         "movq %1, %%rax\n\t"
                         "callq *%%rax" ::"g"(ktasks[i].s_rsp),
                         "g"(resume_func)
                         : "rdi", "rax");

            success = true;
        }
    }
}
