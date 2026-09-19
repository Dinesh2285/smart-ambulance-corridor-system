/*
 * cpu_monitor_main.c  -  Process 12: Process & Scheduling Monitor
 *
 * Collects the periodic MSG_PROC_STATS self-reports every long-running
 * process sends (see Includes/common/proc_monitor.h) — real PID, real
 * SCHED_RR priority, and a genuinely measured CPU% (clock_gettime against
 * CLOCK_PROCESS_CPUTIME_ID, not a random number) — and hands the latest
 * snapshot to cli_dashboard (14) on MSG_SNAPSHOT.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/neutrino.h>
#include <sys/dispatch.h>
#include "../../Includes/common/ipc_messages.h"
#include "../../Includes/common/system_topology.h"
#include "../../Includes/common/proc_monitor.h"

static proc_stat_t table[MAX_TRACKED_PROCS];
static int table_count = 0;

static void record_report(const msg_proc_stats_t *rep) {
    for (int i = 0; i < table_count; i++) {
        if (strncmp(table[i].name, rep->name, sizeof(table[i].name)) == 0) {
            table[i].pid      = rep->pid;
            table[i].priority = rep->priority;
            table[i].cpu_pct  = rep->cpu_pct;
            table[i].valid    = 1;
            return;
        }
    }
    if (table_count < MAX_TRACKED_PROCS) {
        proc_stat_t *slot = &table[table_count++];
        memset(slot, 0, sizeof(*slot));
        strncpy(slot->name, rep->name, sizeof(slot->name) - 1);
        slot->pid      = rep->pid;
        slot->priority = rep->priority;
        slot->cpu_pct  = rep->cpu_pct;
        slot->valid    = 1;
    }
}

int main() {
    printf("[PROC_MONITOR] Process & Scheduling Monitor active...\n");
    pm_set_priority("PROC_MONITOR", PRIO_MONITOR);

    name_attach_t *attach = name_attach(NULL, "cpu_monitor", 0);
    if (!attach) {
        perror("[PROC_MONITOR] Failed to attach name");
        return 1;
    }

    /* Report on itself too, same as every other tracked process. */
    pm_start_reporting("cpu_monitor", PRIO_MONITOR);

    for (;;) {
        union {
            uint16_t         type;
            msg_snapshot_req_t snap;
            msg_proc_stats_t   stat;
        } msg;

        int rcvid = MsgReceive(attach->chid, &msg, sizeof(msg), NULL);
        if (rcvid <= 0) continue;

        if (msg.type == MSG_PROC_STATS) {
            record_report(&msg.stat);
            MsgReply(rcvid, 0, NULL, 0);
        } else if (msg.type == MSG_SNAPSHOT) {
            msg_cpu_snap_t reply;
            memset(&reply, 0, sizeof(reply));
            memcpy(reply.procs, table, sizeof(table));
            reply.count = table_count;
            MsgReply(rcvid, 0, &reply, sizeof(reply));
        } else {
            /* Unhandled message type — must reply so the sender doesn't
             * block forever waiting for us. */
            MsgReply(rcvid, 0, NULL, 0);
        }
    }
    return 0;
}
