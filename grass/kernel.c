/*
 * (C) 2026, Cornell University
 * All rights reserved.
 *
 * Description: kernel ≈ 2 handlers
 *   intr_entry() handles timer and device interrupts.
 *   excp_entry() handles system calls and faults (e.g., invalid memory access).
 */

#include "process.h"
#include <string.h>

uint core_in_kernel;
uint core_to_proc_idx[NCORES];
struct process proc_set[MAX_NPROCESS + 1];
/* proc_set[0] is a place holder for idle cores. */

#define curr_proc_idx core_to_proc_idx[core_in_kernel]
#define curr_pid      proc_set[curr_proc_idx].pid
#define curr_status   proc_set[curr_proc_idx].status
#define curr_saved    proc_set[curr_proc_idx].saved_registers

static void intr_entry(uint);
static void excp_entry(uint);

void kernel_entry() {
    /* With the kernel lock, only one core can enter this point at any time. */
    asm("csrr %0, mhartid" : "=r"(core_in_kernel));

    /* Save the process context. */
    asm("csrr %0, mepc" : "=r"(proc_set[curr_proc_idx].mepc));
    memcpy(curr_saved, (void*)(EGOS_STACK_TOP - 32 * 4), 32 * 4);

    uint mcause;
    asm("csrr %0, mcause" : "=r"(mcause));
    (mcause & (1 << 31)) ? intr_entry(mcause & 0x3FF) : excp_entry(mcause);

    /* Restore the process context. */
    asm("csrw mepc, %0" ::"r"(proc_set[curr_proc_idx].mepc));
    memcpy((void*)(EGOS_STACK_TOP - 32 * 4), curr_saved, 32 * 4);
}

#define INTR_ID_TIMER   7
#define EXCP_ID_ECALL_U 8
#define EXCP_ID_ECALL_M 11
static void proc_yield();
static void proc_try_syscall(struct process* proc);
static void wake_sleepers(void);
static int pick_next_idx(void);

static void excp_entry(uint id) {
    if (id >= EXCP_ID_ECALL_U && id <= EXCP_ID_ECALL_M) {
        /* Copy the system call arguments from user space to the kernel. */
        uint syscall_paddr = earth->mmu_translate(curr_pid, SYSCALL_ARG);
        memcpy(&proc_set[curr_proc_idx].syscall, (void*)syscall_paddr,
               sizeof(struct syscall));
        proc_set[curr_proc_idx].syscall.status = PENDING;

        proc_set_pending(curr_pid);
        proc_set[curr_proc_idx].mepc += 4;
        proc_try_syscall(&proc_set[curr_proc_idx]);
        proc_yield();
        return;
    }
    /* Student's code goes here (System Call & Protection | Virtual Memory). */

    /* Kill the current process if curr_pid is a user application. */
    if (curr_pid >= GPID_USER_START) {
        for (uint i = 0; i < MAX_NPROCESS; i++) {
            if (proc_set[i].pid != curr_pid) continue;
            earth->mmu_free(curr_pid);
            proc_set[i].sleep_until = 0;
            proc_set[i].status      = PROC_UNUSED;
            break;
        }
        INFO("process %d terminated with exception %d", curr_pid, id);
        proc_yield();
        return;
    }

    /* Student's code ends here. */
    FATAL("excp_entry: kernel got exception %d", id);
}

static void intr_entry(uint id) {
    /* Student's code goes here (Preemptive Scheduler). */
    /* Update the process lifecycle statistics. */
    if (id == INTR_ID_TIMER && curr_status == PROC_RUNNING &&
        curr_pid >= GPID_USER_START)
        proc_set[curr_proc_idx].timer_interrupts++;

    /* Student's code ends here. */

    if (id == INTR_ID_TIMER) return proc_yield();

    /* Student's code goes here (Ethernet & TCP/IP). */

    /* Handle an external interrupt from the Intel Gigabit Ethernet Controller.
     * Specifically, you need to (1) Claim the PLIC interrupt; (2) Check if the
     * interrupt is for receiving an Ethernet frame; (3) Read the received frame
     * from an RX buffer and print the content; (4) Complete the PLIC interrupt,
     * so PLIC can fire the next interrupt; (5) Call proc_yield() and return. */

    /* Student's code ends here. */
}

static void wake_sleepers(void) {
    for (uint i = 1; i <= MAX_NPROCESS; i++) {
        if (proc_set[i].status == PROC_SLEEPING &&
            mtime_get() >= proc_set[i].sleep_until) {
            proc_set[i].sleep_until = 0;
            proc_set_runnable(proc_set[i].pid);
        }
    }
}

static int pick_next_idx(void) {
    int next_idx = MAX_NPROCESS;
    int best_level = MLFQ_NLEVELS;
    for (uint i = 1; i <= MAX_NPROCESS; i++) {
        struct process* p = &proc_set[(curr_proc_idx + i) % MAX_NPROCESS];
        if (p->status == PROC_PENDING_SYSCALL) proc_try_syscall(p);

        if (p->status == PROC_READY || p->status == PROC_RUNNABLE) {
            int lvl = p->mlfq_level;
            if (lvl < 0) lvl = 0;
            if (lvl >= MLFQ_NLEVELS) lvl = MLFQ_NLEVELS - 1;
            if (lvl < best_level) {
                best_level = lvl;
                next_idx = (curr_proc_idx + i) % MAX_NPROCESS;
                if (best_level == 0) break;
            }
        }
    }
    return next_idx;
}

static void proc_yield() {
    /* Student's code goes here (Multiple Projects). */

    /* [Preemptive Scheduler]
     * Measure and record lifecycle statistics for the *current* process. */
    if (curr_status == PROC_RUNNING) {
        /* Update CPU time for the current process before it's preempted */
        if (curr_pid >= GPID_USER_START &&
            proc_set[curr_proc_idx].last_schedule_time != 0) {
            ulonglong current_time = mtime_get();
            ulonglong runtime = current_time - proc_set[curr_proc_idx].last_schedule_time;
            proc_set[curr_proc_idx].total_cpu_time += runtime;
            mlfq_update_level(&proc_set[curr_proc_idx], runtime);
        }
        proc_set[curr_proc_idx].last_schedule_time = 0;
        proc_set_runnable(curr_pid);
    }

    mlfq_reset_level();

    /* [System Call & Protection]
     * Do not schedule a process that should still be sleeping at this time. */

    int next_idx;
    do {
        wake_sleepers();
        next_idx = pick_next_idx();
        if (next_idx < MAX_NPROCESS) break;

        curr_proc_idx = 0;
        earth->timer_reset(core_in_kernel);
        asm("csrs mstatus, %0" ::"r"(8));
        asm("wfi");
    } while (1);

    /* [Preemptive Scheduler]
     * Measure and record lifecycle statistics for the *next* process. */
    struct process* next_proc = &proc_set[next_idx];
    if (!next_proc->scheduled_before && next_proc->status == PROC_READY) {
        next_proc->first_schedule_time = mtime_get();
        next_proc->scheduled_before = 1;
    }

    /* [System Call & Protection | Multicore & Locks]
     * Modify mstatus.MPP to enter machine or user mode after mret. */
    {
        uint mstatus;
        uint mpp = (uint)(next_proc->pid < GPID_USER_START ? 3 : 0);
        asm("csrr %0, mstatus" : "=r"(mstatus));
        mstatus = (mstatus & ~(3U << 11)) | (mpp << 11);
        asm("csrw mstatus, %0" : : "r"(mstatus));
    }

    /* Student's code ends here. */

    curr_proc_idx = next_idx;
    earth->mmu_switch(curr_pid);
    earth->mmu_flush_cache();
    if (curr_status == PROC_READY) {
        /* Setup argc, argv and program counter for a newly created process. */
        curr_saved[0]                = APPS_ARG;
        curr_saved[1]                = APPS_ARG + 4;
        proc_set[curr_proc_idx].mepc = APPS_ENTRY;
    }
    proc_set_running(curr_pid);
    if (curr_pid >= GPID_USER_START)
        proc_set[curr_proc_idx].last_schedule_time = mtime_get();
    earth->timer_reset(core_in_kernel);
}

static void proc_try_send(struct process* sender) {
    for (uint i = 0; i < MAX_NPROCESS; i++) {
        struct process* dst = &proc_set[i];
        if (dst->pid == sender->syscall.receiver &&
            dst->status != PROC_UNUSED) {
            /* Return if dst is not receiving or not taking msg from sender. */
            if (!(dst->syscall.type == SYS_RECV &&
                  dst->syscall.status == PENDING))
                return;
            if (!(dst->syscall.sender == GPID_ALL ||
                  dst->syscall.sender == sender->pid))
                return;

            dst->syscall.status = DONE;
            dst->syscall.sender = sender->pid;
            /* Copy the system call arguments within the kernel PCB. */
            memcpy(dst->syscall.content, sender->syscall.content,
                   SYSCALL_MSG_LEN);
            struct proc_request* req = (void*)sender->syscall.content;
            if (dst->pid == GPID_PROCESS && req->type == PROC_SLEEP)
                proc_sleep(sender->pid, req->usec);
            return;
        }
    }
    FATAL("proc_try_send: unknown receiver pid=%d", sender->syscall.receiver);
}

static void proc_try_recv(struct process* receiver) {
    if (receiver->syscall.status == PENDING) return;

    /* Copy the system call struct from the kernel back to user space. */
    uint syscall_paddr = earth->mmu_translate(receiver->pid, SYSCALL_ARG);
    memcpy((void*)syscall_paddr, &receiver->syscall, sizeof(struct syscall));

    struct proc_request* req = (void*)receiver->syscall.content;
    if (receiver->pid == GPID_PROCESS && req->type == PROC_SLEEP) {
        proc_set_runnable(receiver->pid);
        return;
    }

    /* Set the receiver and sender back to RUNNABLE. */
    proc_set_runnable(receiver->pid);
    proc_set_runnable(receiver->syscall.sender);
}

static void proc_try_syscall(struct process* proc) {
    switch (proc->syscall.type) {
    case SYS_RECV:
        proc_try_recv(proc);
        break;
    case SYS_SEND:
        proc_try_send(proc);
        break;
    default:
        FATAL("proc_try_syscall: unknown syscall type=%d", proc->syscall.type);
    }
}
