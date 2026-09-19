/*
 * console_main.c  -  Process 14: CLI Dashboard
 *
 * The only dashboard in this build (no web server / no HTTP) — a single
 * live table in the QNX terminal, refreshed once a second, showing:
 *
 *   - every managed process's real PID, real SCHED_RR priority and a
 *     genuinely measured CPU% (see Includes/common/proc_monitor.h —
 *     clock_gettime against CLOCK_PROCESS_CPUTIME_ID, not rand())
 *   - how the Watchdog (01) is actually managing them: restart counts
 *     and isolation state, pulled live from its own status channel
 *   - the corridor's real state: which arm is green, whether a
 *     preemption is in progress, and measured RF->GPIO latency
 *
 * Every number on screen comes from a live MsgSend/MsgReceive round trip
 * to the process that owns it — nothing here is fabricated.
 */

#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/neutrino.h>
#include <sys/dispatch.h>
#include "../../Includes/common/ipc_messages.h"
#include "../../Includes/common/system_topology.h"
#include "../../Includes/common/proc_monitor.h"

/* ---- lazily-connected, self-healing channels to every data source ---- */
static int proc_coid  = -1;   /* 12_cpu_monitor:   per-process priority/CPU%   */
static int super_coid = -1;   /* 01_supervisor:    restart-count/isolation    */
static int sched_coid = -1;   /* 09_phase_scheduler: current green arm/preempt */
static int lat_coid   = -1;   /* 13_logic_analyzer: RF->GPIO latency           */

static int query(int *coid, const char *service, void *reply, size_t reply_len) {
    if (*coid == -1) *coid = name_open(service, 0);
    if (*coid == -1) return -1;

    msg_snapshot_req_t req = { MSG_SNAPSHOT };
    if (MsgSend(*coid, &req, sizeof(req), reply, reply_len) == -1) {
        *coid = -1;   /* stale connection (service isolated/restarted) - retry next tick */
        return -1;
    }
    return 0;
}

/* ---- one merged row per process, joining the two live tables --------- */
typedef struct {
    char   name[16];
    int    pid;
    int    priority;
    double cpu_pct;
    int    has_cpu;         /* did 12_cpu_monitor have a report for this name? */
    int    restart_count;
    int    isolated;
    int    has_health;      /* did 01_supervisor know this name? */
} row_t;

static row_t rows[MAX_TRACKED_PROCS];
static int   row_count;

static row_t *find_or_add_row(const char *name) {
    for (int i = 0; i < row_count; i++) {
        if (strncmp(rows[i].name, name, sizeof(rows[i].name)) == 0) return &rows[i];
    }
    if (row_count >= MAX_TRACKED_PROCS) return NULL;
    row_t *r = &rows[row_count++];
    memset(r, 0, sizeof(*r));
    strncpy(r->name, name, sizeof(r->name) - 1);
    return r;
}

static void merge_tables(const msg_cpu_snap_t *cpu, const msg_supervisor_snap_t *sup) {
    row_count = 0;

    for (int i = 0; i < sup->count && i < MAX_TRACKED_PROCS; i++) {
        row_t *r = find_or_add_row(sup->procs[i].name);
        if (!r) continue;
        r->pid           = sup->procs[i].pid;
        r->restart_count = sup->procs[i].restart_count;
        r->isolated      = sup->procs[i].isolated;
        r->has_health    = 1;
    }
    for (int i = 0; i < cpu->count && i < MAX_TRACKED_PROCS; i++) {
        if (!cpu->procs[i].valid) continue;
        row_t *r = find_or_add_row(cpu->procs[i].name);
        if (!r) continue;
        r->priority = cpu->procs[i].priority;
        r->cpu_pct  = cpu->procs[i].cpu_pct;
        r->has_cpu  = 1;
        if (!r->has_health) r->pid = cpu->procs[i].pid; /* e.g. supervisor unreachable */
    }
}

static void print_bar(double pct, int width) {
    int filled = (int)((pct / 100.0) * width);
    if (filled > width) filled = width;
    if (filled < 0) filled = 0;
    putchar('[');
    for (int i = 0; i < width; i++) putchar(i < filled ? '#' : '.');
    printf("] %5.1f%%", pct);
}

int main() {
    printf("[CLI_DASHBOARD] Terminal dashboard starting...\n");
    pm_set_priority("CLI_DASHBOARD", PRIO_MONITOR);
    pm_start_reporting("cli_dashboard", PRIO_MONITOR);

    for (;;) {
        msg_cpu_snap_t        cpu_snap;    memset(&cpu_snap, 0, sizeof(cpu_snap));
        msg_supervisor_snap_t super_snap;  memset(&super_snap, 0, sizeof(super_snap));
        msg_sys_snap_t        sched_snap;  memset(&sched_snap, 0, sizeof(sched_snap));
        msg_sys_snap_t        lat_snap;    memset(&lat_snap, 0, sizeof(lat_snap));

        int have_cpu   = (query(&proc_coid,  "cpu_monitor",      &cpu_snap,   sizeof(cpu_snap))   == 0);
        int have_super = (query(&super_coid, "supervisor",       &super_snap, sizeof(super_snap)) == 0);
        int have_sched = (query(&sched_coid, "infra_scheduler",  &sched_snap, sizeof(sched_snap)) == 0);
        int have_lat   = (query(&lat_coid,   "logic_analyzer",   &lat_snap,   sizeof(lat_snap))   == 0);

        merge_tables(&cpu_snap, &super_snap);

        printf("\033[2J\033[H");
        printf("================================================================================\n");
        printf(" GREENLANE V2I CORRIDOR - QNX NEUTRINO PROCESS & SCHEDULING MONITOR\n");
        printf("================================================================================\n");
        printf(" %-18s %7s %5s %-24s %5s %s\n",
               "PROCESS", "PID", "PRIO", "CPU (SCHED_RR)", "RST", "STATUS");
        printf("--------------------------------------------------------------------------------\n");

        for (int i = 0; i < row_count; i++) {
            row_t *r = &rows[i];
            char pid_buf[8];
            if (r->pid > 0) snprintf(pid_buf, sizeof(pid_buf), "%d", r->pid);
            else            snprintf(pid_buf, sizeof(pid_buf), "-");

            char prio_buf[8];
            if (r->has_cpu) snprintf(prio_buf, sizeof(prio_buf), "%d", r->priority);
            else            snprintf(prio_buf, sizeof(prio_buf), "-");

            printf(" %-18s %7s %5s ", r->name, pid_buf, prio_buf);
            if (r->has_cpu) print_bar(r->cpu_pct, 15);
            else            printf("%-24s", "(no report yet)");

            const char *status = r->isolated ? "ISOLATED"
                                : (r->pid > 0 ? "RUNNING" : "EXITED");
            printf(" %5d %s\n", r->restart_count, status);
        }
        if (row_count == 0) {
            printf(" (no process/supervisor data yet - is the Watchdog running?)\n");
        }

        printf("--------------------------------------------------------------------------------\n");
        if (have_sched) {
            printf(" CORRIDOR: arm %d green%s\n",
                   sched_snap.current_arm_green,
                   sched_snap.is_preempted ? "  >>> EMERGENCY PREEMPTION ACTIVE <<<" : " (normal cycle)");
        } else {
            printf(" CORRIDOR: phase_scheduler unreachable\n");
        }
        if (have_lat) {
            printf(" LATENCY (RF -> GPIO): %.2f ms (budget: 250 ms)\n", lat_snap.latency_ms);
        } else {
            printf(" LATENCY: logic_analyzer unreachable\n");
        }
        if (!have_cpu)   printf(" [!] cpu_monitor unreachable — priority/CPU%% columns are stale.\n");
        if (!have_super) printf(" [!] supervisor status server unreachable — PID/restart columns are stale.\n");
        printf("================================================================================\n");

        sleep(1);
    }
    return 0;
}
