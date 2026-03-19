/*
 * (C) 2026, Cornell University
 * All rights reserved.
 *
 * Description: cooperative multithreading and synchronization
 */

#include <sys/queue.h>
#include "print.c"
#include "thread.h"

/* Student's code goes here (Cooperative Threads). */
/* Define the TCB and helper functions (if needed) for multi-threading. */
int current_idx;
struct thread TCB[32];

static int find_next_ready_thread() {
    int start = (current_idx + 1) % 32;
    for (int i = 0; i < 32; i++) {
        int idx = (start + i) % 32;
        if (TCB[idx].status == THREAD_RUNNING) {
            return idx;
        }
    }
    return -1;
}

static void cleanup_terminated_threads() {

}

/* Student's code ends here. */

void ctx_entry() {
    /* Student's code goes here (Cooperative Threads). */
    struct thread *current_thread = &TCB[current_idx];
    current_thread->status = THREAD_RUNNING;
    current_thread->start_func(current_thread->start_arg);
    thread_exit();
    /* Student's code ends here. */
}

void thread_init() {
    /* Student's code goes here (Cooperative Threads). */
    for (int i = 0; i < 32; i++) {
        TCB[i].id = i;
        TCB[i].sp = NULL;
        TCB[i].status = THREAD_TERMINATED;
        TCB[i].start_func = NULL;
        TCB[i].start_arg = NULL;
    }

    current_idx = 0;
    TCB[current_idx].id = 0;
    TCB[current_idx].status = THREAD_RUNNING;
    TCB[current_idx].start_func = NULL;
    TCB[current_idx].start_arg = NULL;
    /* Student's code ends here. */
}

void thread_create(void (*entry)(void *arg), void *arg) {
    /* Student's code goes here (Cooperative Threads). */
    int new_idx = -1;
    for (int i = 0; i < 32; i++) {
        if (TCB[i].status == THREAD_TERMINATED) {
            new_idx = i;
            break;
        }
    }

    if (new_idx == -1) {
        return;
    }

    char* child_stack = malloc(STACK_SIZE);
    TCB[new_idx].id = new_idx;
    TCB[new_idx].status = THREAD_READY;
    TCB[new_idx].start_func = entry;
    TCB[new_idx].start_arg = arg;

    void* sp = child_stack + STACK_SIZE;
    ctx_start(&TCB[current_idx].sp, sp);

    cleanup_terminated_threads();
    /* Student's code ends here. */
}

void thread_yield() {
    /* Student's code goes here (Cooperative Threads). */
    int next_idx = find_next_ready_thread();
    if (next_idx == -1) {
        return;
    }

    if (TCB[current_idx].status == THREAD_RUNNING) {
        TCB[current_idx].status = THREAD_READY;
    }

    int prev_idx = current_idx;
    current_idx = next_idx;
    TCB[current_idx].status = THREAD_RUNNING;

    ctx_switch(&TCB[prev_idx].sp, TCB[current_idx].sp);

    cleanup_terminated_threads(); // When we return here, we're back in the original thread
    /* Student's code ends here. */
}

void thread_exit() {
    /* Student's code goes here (Cooperative Threads). */
    TCB[current_idx].status = THREAD_TERMINATED;

    int next_idx = find_next_ready_thread();
    if (next_idx == -1) {
        _end();
    }

    int prev_idx = current_idx;
    current_idx = next_idx;
    TCB[current_idx].status = THREAD_RUNNING;

    ctx_switch(&TCB[prev_idx].sp, TCB[current_idx].sp);
    // Should never return here
    /* Student's code ends here. */
}

/* Student's code goes here (Cooperative Threads). */
/* Define helper functions (if needed) for conditional variables. */

/* Student's code ends here. */

void cv_init(struct cv *condition) {
    /* Student's code goes here (Cooperative Threads). */

    /* Student's code ends here. */
}

void cv_wait(struct cv *condition) {
    /* Student's code goes here (Cooperative Threads). */

    /* Student's code ends here. */
}

void cv_signal(struct cv *condition) {
    /* Student's code goes here (Cooperative Threads). */

    /* Student's code ends here. */
}

#define BUF_SIZE 3
void* buffer[BUF_SIZE];
int count = 0;
int head = 0, tail = 0;
struct cv nonempty, nonfull;

void produce(void* arg) {
    while (1) {
        while (count == BUF_SIZE) cv_wait(&nonfull);
        /* At this point, the buffer is not full. */

        /* Student's code goes here (Cooperative Threads). */
        /* Print out the producer ID with the arg pointer. */

        /* Student's code ends here. */
        buffer[tail] = arg;
        tail = (tail + 1) % BUF_SIZE;
        count += 1;
        cv_signal(&nonempty);
    }
}

void consume(void *arg) {
    while (1) {
        while (count == 0) cv_wait(&nonempty);
        /* At this point, the buffer is not empty. */

        /* Student's code goes here (Cooperative Threads). */
        /* Print out the consumer ID with the arg pointer. */

        /* Student's code ends here. */
        void* result = buffer[head];
        head = (head + 1) % BUF_SIZE;
        count -= 1;
        cv_signal(&nonfull);
    }
}

int main() {
    thread_init();

    int ID[500];
    for (int i = 0; i < 500; i++) ID[i] = i;

    for (int i = 0; i < 500; i++)
        thread_create(consume, ID + i);

    for (int i = 0; i < 500; i++)
        thread_create(produce, ID + i);

    printf("main thread exits\n\r");
    thread_exit();

    /* The control flow should NEVER get here. If the main thread is the last to
     * call thread_exit(), thread_exit() should terminate the program by calling
     * the _end() in thread.s.
     * If the main thread is not the last, thread_exit() will switch the context
     * to another thread. Later, when all the threads have called thread_exit(),
     * the last one calling it should then call _end() within thread_exit(). */
}
