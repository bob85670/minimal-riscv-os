/*
 * (C) 2026, Cornell University
 * All rights reserved.
 *
 * Description: helper functions for process management
 */

#include "process.h"

extern struct process proc_set[MAX_NPROCESS + 1];

static void proc_set_status(int pid, enum proc_status status) {
    for (uint i = 0; i < MAX_NPROCESS; i++)
        if (proc_set[i].pid == pid) proc_set[i].status = status;
}

void proc_set_ready(int pid) { proc_set_status(pid, PROC_READY); }
void proc_set_running(int pid) { proc_set_status(pid, PROC_RUNNING); }
void proc_set_runnable(int pid) { proc_set_status(pid, PROC_RUNNABLE); }
void proc_set_pending(int pid) { proc_set_status(pid, PROC_PENDING_SYSCALL); }

int proc_alloc() {
    static uint curr_pid = 0;
    for (uint i = 1; i <= MAX_NPROCESS; i++)
        if (proc_set[i].status == PROC_UNUSED) {
            proc_set[i].pid    = ++curr_pid;
            proc_set[i].status = PROC_LOADING;
            /* Student's code goes here (Preemptive Scheduler | System Call). */

            /* Initialization of lifecycle statistics, MLFQ or process sleep. */
            proc_set[i].creation_time = mtime_get();
            proc_set[i].first_schedule_time = 0;
            proc_set[i].last_schedule_time = 0;
            proc_set[i].total_cpu_time = 0;
            proc_set[i].timer_interrupts = 0;
            proc_set[i].scheduled_before = 0;
            proc_set[i].mlfq_level = 0;
            proc_set[i].mlfq_level_remaining = MLFQ_LEVEL_RUNTIME(0);
            proc_set[i].sleep_until = 0;

            /* Student's code ends here. */
            return curr_pid;
        }

    FATAL("proc_alloc: reach the limit of %d processes", MAX_NPROCESS);
}

void proc_free(int pid) {
    /* Student's code goes here (Preemptive Scheduler). */

    /* Print the lifecycle statistics of the terminated process or processes. */
    if (pid != GPID_ALL) {
        /* Find and free a single process */
        for (uint i = 0; i < MAX_NPROCESS; i++) {
            if (proc_set[i].pid == pid && proc_set[i].status != PROC_UNUSED) {
                ulonglong current_time = mtime_get();
                ulonglong turnaround_time = current_time - proc_set[i].creation_time;
                ulonglong response_time =
                    (proc_set[i].first_schedule_time == 0)
                        ? 0
                        : (proc_set[i].first_schedule_time - proc_set[i].creation_time);
                
                /* Convert to milliseconds (10^-7 seconds to ms: divide by 10000) */
                int turnaround_ms = turnaround_time / 10000;
                int response_ms = response_time / 10000;
                int cpu_ms = proc_set[i].total_cpu_time / 10000;
                
                INFO("process %d terminated after %d timer interrupts, "
                     "turnaround time: %dms, response time: %dms, CPU time: %dms",
                     pid, proc_set[i].timer_interrupts, 
                     turnaround_ms, response_ms, cpu_ms);
                
                earth->mmu_free(pid);
                proc_set[i].sleep_until = 0;
                proc_set[i].status      = PROC_UNUSED;
                break;
            }
        }
    } else {
        /* Free all user processes */
        for (uint i = 0; i < MAX_NPROCESS; i++) {
            if (proc_set[i].pid >= GPID_USER_START &&
                proc_set[i].status != PROC_UNUSED) {
                
                ulonglong current_time = mtime_get();
                ulonglong turnaround_time = current_time - proc_set[i].creation_time;
                ulonglong response_time =
                    (proc_set[i].first_schedule_time == 0)
                        ? 0
                        : (proc_set[i].first_schedule_time - proc_set[i].creation_time);
                
                int turnaround_ms = turnaround_time / 10000;
                int response_ms = response_time / 10000;
                int cpu_ms = proc_set[i].total_cpu_time / 10000;
                
                INFO("process %d terminated after %d timer interrupts, "
                     "turnaround time: %dms, response time: %dms, CPU time: %dms",
                     proc_set[i].pid, proc_set[i].timer_interrupts,
                     turnaround_ms, response_ms, cpu_ms);
                
                earth->mmu_free(proc_set[i].pid);
                proc_set[i].sleep_until = 0;
                proc_set[i].status      = PROC_UNUSED;
            }
        }
    }
    /* Student's code ends here. */
}

void mlfq_update_level(struct process* p, ulonglong runtime) {
    /* Student's code goes here (Preemptive Scheduler). */

    /* Update the MLFQ-related fields in struct process* p after this
     * process has run on the CPU for another runtime microseconds. */
    if (p == 0 || runtime == 0) return;
    if (p->mlfq_level < 0) 
        p->mlfq_level = 0;
    if (p->mlfq_level >= MLFQ_NLEVELS)
        p->mlfq_level = MLFQ_NLEVELS - 1;
    if (p->mlfq_level_remaining == 0)
        p->mlfq_level_remaining = MLFQ_LEVEL_RUNTIME(p->mlfq_level);

    while (runtime > 0) {
        if (p->mlfq_level >= MLFQ_NLEVELS - 1) {
            if (runtime >= p->mlfq_level_remaining)
                p->mlfq_level_remaining = 0;
            else
                p->mlfq_level_remaining -= runtime;
            return;
        }

        if (runtime < p->mlfq_level_remaining) {
            p->mlfq_level_remaining -= runtime;
            return;
        }

        runtime -= p->mlfq_level_remaining;
        p->mlfq_level++;
        p->mlfq_level_remaining = MLFQ_LEVEL_RUNTIME(p->mlfq_level);
    }

    /* Student's code ends here. */
}

void mlfq_reset_level() {
    /* Student's code goes here (Preemptive Scheduler). */
    if (!earth->tty_input_empty()) {
        /* Reset the level of GPID_SHELL if there is pending keyboard input. */
        for (uint i = 1; i <= MAX_NPROCESS; i++) {
            if (proc_set[i].status != PROC_UNUSED && proc_set[i].pid == GPID_SHELL) {
                proc_set[i].mlfq_level = 0;
                proc_set[i].mlfq_level_remaining = MLFQ_LEVEL_RUNTIME(0);
                break;
            }
        }
    }

    static ulonglong MLFQ_last_reset_time = 0;
    /* Reset the level of all processes every MLFQ_RESET_PERIOD microseconds. */
    ulonglong now = mtime_get();
    if (MLFQ_last_reset_time == 0) 
        MLFQ_last_reset_time = now;
    if (now - MLFQ_last_reset_time >= MLFQ_RESET_PERIOD) {
        for (uint i = 1; i <= MAX_NPROCESS; i++) {
            if (proc_set[i].status == PROC_UNUSED) continue;
            proc_set[i].mlfq_level = 0;
            proc_set[i].mlfq_level_remaining = MLFQ_LEVEL_RUNTIME(0);
        }
        MLFQ_last_reset_time = now;
    }

    /* Student's code ends here. */
}

void proc_sleep(int pid, uint usec) {
    /* Student's code goes here (System Call & Protection). */

    /* Update the sleep-related fields in the struct process for process pid. */
    for (uint i = 0; i < MAX_NPROCESS; i++) {
        if (proc_set[i].pid != pid || proc_set[i].status == PROC_UNUSED) continue;
        ulonglong until = mtime_get() + (ulonglong)usec * 10ULL;
        if (proc_set[i].status == PROC_SLEEPING && proc_set[i].sleep_until > mtime_get())
            return;
        proc_set[i].sleep_until = until;
        proc_set[i].status      = PROC_SLEEPING;
        return;
    }

    /* Student's code ends here. */
}

void proc_coresinfo() {
    /* Student's code goes here (Multicore & Locks). */

    /* Print out the pid of the process running on each CPU core. */

    /* Student's code ends here. */
}
