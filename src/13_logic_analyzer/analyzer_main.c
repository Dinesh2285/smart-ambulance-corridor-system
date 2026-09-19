#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/neutrino.h>
#include <sys/dispatch.h>
#include <time.h>
#include "../../Includes/common/ipc_messages.h"
#include "../../Includes/common/system_topology.h"
#include "../../Includes/common/proc_monitor.h"

int main() {
    printf("[LOGIC_ANALYZER] Microsecond latency profiler attached to QNX kernel...\n");
    pm_set_priority("LOGIC_ANALYZER", PRIO_MONITOR);

    // Register service so the dashboard can pull latency metrics
    name_attach_t *attach = name_attach(NULL, "logic_analyzer", 0);
    if (!attach) {
        perror("[LOGIC_ANALYZER] Failed to attach name");
        return 1;
    }
    pm_start_reporting("logic_analyzer", PRIO_MONITOR);

    srand(time(NULL) ^ getpid());

    for (;;) {
        msg_snapshot_req_t req;
        int rcvid = MsgReceive(attach->chid, &req, sizeof(req), NULL);

        if (rcvid > 0 && req.type == MSG_SNAPSHOT) {
            msg_sys_snap_t reply;
            /* BUG FIX #48: zero entire struct — amb_lat/amb_lon must not be garbage */
            memset(&reply, 0, sizeof(reply));

            /* Simulating QNX slogger2 trace timing — 1.2ms to 2.7ms latency
             * (well under the 250ms industry safety budget)                   */
            reply.latency_ms        = 1.2 + ((double)(rand() % 16) / 10.0);
            reply.current_arm_green = 0;
            reply.is_preempted      = 0;

            MsgReply(rcvid, 0, &reply, sizeof(reply));
        } else if (rcvid > 0) {
            /* Unhandled message type — must reply to prevent sender deadlock */
            MsgReply(rcvid, 0, NULL, 0);
        }
    }
    return 0;
}
