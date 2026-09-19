#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/neutrino.h>
#include <sys/dispatch.h>
#include "../../Includes/common/ipc_messages.h"
#include "../../Includes/common/system_topology.h"
#include "../../Includes/common/proc_monitor.h"
#include "priority_queue.h"

/* Track the latest known ambulance position, for the console/CLI dashboard. */
#define DEFAULT_LAT 13.01000
#define DEFAULT_LON 80.25000
double current_lat = DEFAULT_LAT;
double current_lon = DEFAULT_LON;

int main() {
    printf("[ARBITER] Corridor Arbiter active.\n");
    pm_set_priority("ARBITER", PRIO_CONTROL);

    name_attach_t *attach = name_attach(NULL, "corridor_arbiter", 0);
    int sched_coid = -1;
    while ((sched_coid = name_open("infra_scheduler", 0)) == -1) usleep(500000);

    pq_init();
    pm_start_reporting("corridor_arbiter", PRIO_CONTROL);

    for (;;) {
        union {
            uint16_t type;
            msg_nav_t nav;
            msg_snapshot_req_t snap;
        } msg;

        int rcvid = MsgReceive(attach->chid, &msg, sizeof(msg), NULL);
        if (rcvid <= 0) continue;

        if (msg.type == MSG_NAV_UPDATE) {
            // Update live GPS state
            current_lat = msg.nav.lat;
            current_lon = msg.nav.lon;

            double eta = (msg.nav.speed_kmh > 0.1)
                         ? msg.nav.ahead_m / (msg.nav.speed_kmh * 0.2778)
                         : 999.0; /* BUG FIX #29: guard div-by-zero when speed==0 */
            printf("[ARBITER] Ambulance ETA: %.1fs (Ahead: %.1fm)\n", eta, msg.nav.ahead_m);

            int arm = pq_update_and_get_top("AMB1", eta, msg.nav.ahead_m, msg.nav.target_junction);

            if (arm >= 0 && msg.nav.ahead_m < 100.0) {
                printf("[ARBITER] Trigger condition met. Sent Preemption Command!\n");
                msg_preempt_t preempt = { MSG_PREEMPT_CMD, arm, 1 };
                MsgSend(sched_coid, &preempt, sizeof(preempt), NULL, 0);
            } else if (msg.nav.ahead_m < -5.0) {
                msg_preempt_t preempt = { MSG_PREEMPT_CMD, 0, 0 };
                MsgSend(sched_coid, &preempt, sizeof(preempt), NULL, 0);
            }
            MsgReply(rcvid, 0, NULL, 0);
        }
        else if (msg.type == MSG_SNAPSHOT) {
            // Serve the live ambulance position to the console dashboard.
            msg_sys_snap_t reply = {0};
            reply.amb_lat = current_lat;
            reply.amb_lon = current_lon;
            MsgReply(rcvid, 0, &reply, sizeof(reply));
        } else {
            MsgReply(rcvid, 0, NULL, 0);
        }
    }
    return 0;
}
