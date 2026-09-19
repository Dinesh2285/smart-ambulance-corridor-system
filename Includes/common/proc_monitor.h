#ifndef PROC_MONITOR_H
#define PROC_MONITOR_H

#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <sched.h>
#include <time.h>
#include <sys/neutrino.h>
#include <sys/dispatch.h>
#include "ipc_messages.h"

/*
 * proc_monitor.h
 *
 * Two small, reusable pieces every long-running Greenlane process calls
 * once at start-up:
 *
 *   pm_set_priority(tag, priority)   - actually raise this process's own
 *                                       real-time priority (SCHED_RR),
 *                                       instead of leaving every process
 *                                       at the same default.
 *
 *   pm_start_reporting(name, prio)   - spawn a background thread that
 *                                       reports this process's genuine
 *                                       CPU usage to Process 12
 *                                       (cpu_monitor) once a second, so
 *                                       cli_dashboard has real numbers
 *                                       instead of rand().
 *
 * Both are plain POSIX (pthread_setschedparam, pthread_create,
 * clock_gettime with CLOCK_PROCESS_CPUTIME_ID / CLOCK_MONOTONIC) plus
 * this project's own QNX MsgSend — nothing QNX-internal or guessed, so
 * it needs no extra link libraries on QNX (pthreads live in libc there).
 * pthread_setschedparam(pthread_self(), ...) is used rather than
 * sched_setscheduler(): QNX Neutrino schedules per-THREAD, so setting
 * the calling (main) thread's own policy is the more direct match for
 * "make this process's real work run at this priority".
 */

static inline void pm_set_priority(const char *tag, int priority) {
    struct sched_param sp;
    memset(&sp, 0, sizeof(sp));
    sp.sched_priority = priority;
    if (pthread_setschedparam(pthread_self(), SCHED_RR, &sp) != 0) {
        printf("[%s] WARNING: could not raise priority to %d (run as root "
               "to enable) — continuing at the default priority.\n",
               tag, priority);
    } else {
        printf("[%s] Running under SCHED_RR at priority %d.\n", tag, priority);
    }
}

typedef struct {
    char name[16];
    int  priority;
} pm_reporter_args_t;

static inline void *pm_reporter_thread(void *arg) {
    pm_reporter_args_t *a = (pm_reporter_args_t *)arg;
    int mon_coid = -1;

    struct timespec cpu_prev, cpu_now, wall_prev, wall_now;
    clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &cpu_prev);
    clock_gettime(CLOCK_MONOTONIC, &wall_prev);

    for (;;) {
        sleep(1);

        clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &cpu_now);
        clock_gettime(CLOCK_MONOTONIC, &wall_now);

        double cpu_delta  = (double)(cpu_now.tv_sec  - cpu_prev.tv_sec)
                           + (double)(cpu_now.tv_nsec  - cpu_prev.tv_nsec)  / 1e9;
        double wall_delta = (double)(wall_now.tv_sec - wall_prev.tv_sec)
                           + (double)(wall_now.tv_nsec - wall_prev.tv_nsec) / 1e9;
        double pct = (wall_delta > 0.0) ? (100.0 * cpu_delta / wall_delta) : 0.0;
        if (pct < 0.0)   pct = 0.0;
        if (pct > 100.0) pct = 100.0;

        cpu_prev  = cpu_now;
        wall_prev = wall_now;

        if (mon_coid == -1) mon_coid = name_open("cpu_monitor", 0);
        if (mon_coid != -1) {
            msg_proc_stats_t rep;
            memset(&rep, 0, sizeof(rep));
            rep.type     = MSG_PROC_STATS;
            rep.pid      = getpid();
            rep.priority = a->priority;
            rep.cpu_pct  = pct;
            strncpy(rep.name, a->name, sizeof(rep.name) - 1);
            /* fire-and-forget: cpu_monitor always replies (rbytes=0 is
             * fine), but if the connection itself is gone, drop it and
             * reconnect lazily next tick instead of blocking forever. */
            if (MsgSend(mon_coid, &rep, sizeof(rep), NULL, 0) == -1) {
                mon_coid = -1;
            }
        }
    }
    return NULL;
}

/* Call once from main(), after pm_set_priority(). Returns immediately —
 * the calling process's normal MsgReceive loop is completely untouched. */
static inline void pm_start_reporting(const char *name, int priority) {
    /* static: must outlive this call, since the thread reads it forever */
    static pm_reporter_args_t args;
    memset(&args, 0, sizeof(args));
    strncpy(args.name, name, sizeof(args.name) - 1);
    args.priority = priority;

    pthread_t tid;
    if (pthread_create(&tid, NULL, pm_reporter_thread, &args) == 0) {
        pthread_detach(tid);
    } else {
        printf("[%s] WARNING: could not start CPU-reporting thread.\n", name);
    }
}

#endif /* PROC_MONITOR_H */
