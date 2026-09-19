#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>
#include <time.h>
#include <string.h>
#include <spawn.h>
#include <pthread.h>
#include <sys/neutrino.h>
#include <sys/dispatch.h>
#include "../../Includes/common/ipc_messages.h"
#include "../../Includes/common/system_topology.h"
#include "../../Includes/common/proc_monitor.h"

#define MAX_RESTARTS 3
#define HEAL_WINDOW_SEC 10

typedef struct {
    char *path;
    pid_t pid;
    int restart_count;
    time_t last_crash_time;
    int isolated;
} process_t;

/* The 12 long-running processes the Watchdog boots and self-heals.
 * amb_route (one-shot, writes waypoints then exits) and chaos_injector
 * (manual, judge-triggered) are their own binaries but aren't part of
 * this managed set — see the clean-exit handling below for amb_route,
 * and 15_fault_injector/chaos_main.c for the standalone tool. */
#define MAX_PROCESSES 12

process_t procs[MAX_PROCESSES] = {
    {"./amb_kinematics",   0, 0, 0, 0},
    {"./amb_rf_tx",        0, 0, 0, 0},
    {"./infra_rf_rx",      0, 0, 0, 0},
    {"./infra_radar",      0, 0, 0, 0},
    {"./infra_pedestrian", 0, 0, 0, 0},
    {"./corridor_arbiter", 0, 0, 0, 0},
    {"./phase_scheduler",  0, 0, 0, 0},
    {"./gpio_driver",      0, 0, 0, 0},
    {"./safety_interlock", 0, 0, 0, 0},
    {"./cpu_monitor",      0, 0, 0, 0},
    {"./logic_analyzer",   0, 0, 0, 0},
    {"./cli_dashboard",    0, 0, 0, 0},
};

/* Also spawned at boot but not health-tracked above: amb_route is a
 * one-shot task, not a long-running service (see the clean-exit branch
 * in the FDIR loop below). */
static const char *ONE_SHOT_PATH = "./amb_route";

int num_procs = sizeof(procs) / sizeof(procs[0]);

/* Guards procs[] between the FDIR loop (main thread) and the status
 * server thread below, which cli_dashboard polls for live PID/restart/
 * isolation state. */
static pthread_mutex_t procs_lock = PTHREAD_MUTEX_INITIALIZER;

void spawn_process(int i) {
    if (procs[i].isolated) return;

    pid_t pid = spawnl(P_NOWAIT, procs[i].path, procs[i].path, NULL);
    if (pid > 0) {
        procs[i].pid = pid;
        printf("[WATCHDOG] Spawned %s (PID: %d)\n", procs[i].path, pid);
    }
}

/* ------------------------------------------------------------------ */
/*  Status server — a separate thread so it never competes with the     */
/*  main thread's wait()-based FDIR loop for CPU or blocks it. Answers  */
/*  MSG_SNAPSHOT with the live PID/restart-count/isolated state of      */
/*  every managed process, for cli_dashboard (14) to display.           */
/* ------------------------------------------------------------------ */
static void *status_server_thread(void *unused) {
    (void)unused;

    name_attach_t *attach = name_attach(NULL, "supervisor", 0);
    if (attach == NULL) {
        perror("[WATCHDOG] status server: name_attach failed");
        return NULL;
    }

    for (;;) {
        msg_snapshot_req_t req;
        int rcvid = MsgReceive(attach->chid, &req, sizeof(req), NULL);
        if (rcvid <= 0) continue;

        if (req.type == MSG_SNAPSHOT) {
            msg_supervisor_snap_t reply;
            memset(&reply, 0, sizeof(reply));

            pthread_mutex_lock(&procs_lock);
            reply.count = num_procs;
            for (int i = 0; i < num_procs && i < MAX_TRACKED_PROCS; i++) {
                /* procs[i].path is "./name" - strip the leading "./" so
                 * it matches the names processes self-report under
                 * proc_monitor.h's pm_start_reporting(). */
                const char *name = procs[i].path;
                if (name[0] == '.' && name[1] == '/') name += 2;
                strncpy(reply.procs[i].name, name, sizeof(reply.procs[i].name) - 1);
                reply.procs[i].pid            = procs[i].pid;
                reply.procs[i].restart_count  = procs[i].restart_count;
                reply.procs[i].isolated       = procs[i].isolated;
            }
            pthread_mutex_unlock(&procs_lock);

            MsgReply(rcvid, 0, &reply, sizeof(reply));
        } else {
            MsgReply(rcvid, 0, NULL, 0);
        }
    }
    return NULL;
}

int main() {
    printf("==========================================\n");
    printf(" QNX SUPERVISOR: SYSTEM BOOT SEQUENCE\n");
    printf("==========================================\n");

    /* Highest priority in the system: if the Watchdog itself is starved,
     * self-healing can't happen when it's needed most. */
    pm_set_priority("WATCHDOG", PRIO_SUPERVISOR);

    pthread_t stid;
    if (pthread_create(&stid, NULL, status_server_thread, NULL) == 0) {
        pthread_detach(stid);
    } else {
        printf("[WATCHDOG] WARNING: could not start status server thread — "
               "cli_dashboard will show this process's table as unreachable.\n");
    }

    // Boot the one-shot route generator, then every long-running process.
    spawnl(P_NOWAIT, ONE_SHOT_PATH, ONE_SHOT_PATH, NULL);
    for (int i = 0; i < num_procs; i++) {
        spawn_process(i);
    }

    int status;
    pid_t crashed_pid;

    // The infinite monitoring loop
    while (1) {
        crashed_pid = wait(&status); // Wait blocks until a child process dies
        if (crashed_pid > 0) {
            time_t now = time(NULL);
            int tracked = 0;

            pthread_mutex_lock(&procs_lock);
            for (int i = 0; i < num_procs; i++) {
                if (procs[i].pid == crashed_pid) {
                    tracked = 1;
                    /* A plain exit(0) is not a fault — only a non-zero
                     * exit or a fatal signal (e.g. the Chaos Injector's
                     * SIGKILL) is a real fault that needs FDIR. */
                    if (WIFEXITED(status) && WEXITSTATUS(status) == 0) {
                        printf("\n[EXIT] %s finished normally (exit 0) — not a fault, not restarted.\n",
                               procs[i].path);
                        procs[i].pid = 0;
                        break;
                    }

                    if (WIFSIGNALED(status)) {
                        printf("\n[FAULT DETECTED] Process %s killed by signal %d.\n",
                               procs[i].path, WTERMSIG(status));
                    } else {
                        printf("\n[FAULT DETECTED] Process %s crashed (exit code %d).\n",
                               procs[i].path, WIFEXITED(status) ? WEXITSTATUS(status) : -1);
                    }

                    if (now - procs[i].last_crash_time < HEAL_WINDOW_SEC) {
                        procs[i].restart_count++;
                    } else {
                        procs[i].restart_count = 1; // Reset count if it was stable for a while
                    }
                    procs[i].last_crash_time = now;

                    if (procs[i].restart_count > MAX_RESTARTS) {
                        printf("[ISOLATION] %s failed self-healing 3 times. Isolating subsystem.\n", procs[i].path);
                        procs[i].isolated = 1;
                        // In the future, we will send MSG_SENSOR_FAULT to the Arbiter here
                    } else {
                        printf("[SELF-HEALING] Restarting %s (Attempt %d/3)\n", procs[i].path, procs[i].restart_count);
                        spawn_process(i); // Heal by respawning
                    }
                    break;
                }
            }
            pthread_mutex_unlock(&procs_lock);

            if (!tracked) {
                /* Not in procs[] -> this is amb_route (the one-shot
                 * waypoint generator, spawned once above but never
                 * health-tracked or restarted). */
                printf("\n[EXIT] %s (one-shot) finished, PID %d.\n", ONE_SHOT_PATH, (int)crashed_pid);
            }
        }
    }
    return 0;
}
