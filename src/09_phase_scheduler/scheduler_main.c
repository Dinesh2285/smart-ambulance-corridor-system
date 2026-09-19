#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/neutrino.h>
#include <sys/dispatch.h>
#include "../../Includes/common/ipc_messages.h"
#include "../../Includes/common/system_topology.h"
#include "../../Includes/common/proc_monitor.h"

int main() {
    printf("[SCHEDULER] Phase Timing Engine active.\n");
    pm_set_priority("SCHEDULER", PRIO_CONTROL);

    // 1. Register this process so the Arbiter can find it
    name_attach_t *attach = name_attach(NULL, "infra_scheduler", 0);
    if (attach == NULL) {
        perror("[SCHEDULER] Failed to attach name");
        return 1;
    }

    // 2. Connect to the Safety Interlock (We NEVER talk directly to GPIO)
    int interlock_coid = -1;
    while ((interlock_coid = name_open("safety_interlock", 0)) == -1) {
        usleep(500000); // Wait 0.5s if Interlock isn't booted yet
    }
    pm_start_reporting("phase_scheduler", PRIO_CONTROL);

    int preemption_active = 0;
    int current_green_arm = 0; // Assume Arm 0 starts green

    for (;;) {
        // Use a union to catch either a Preempt Command OR a Snapshot Request
        union {
            uint16_t type;
            msg_preempt_t preempt;
            msg_snapshot_req_t snap;
        } msg;

        int rcvid = MsgReceive(attach->chid, &msg, sizeof(msg), NULL);
        if (rcvid <= 0) continue;

        if (msg.type == MSG_PREEMPT_CMD) {
            if (msg.preempt.hold_green && !preemption_active) {
                preemption_active = 1;
                if (current_green_arm != msg.preempt.target_arm) {
                    msg_gpio_t amber_cmd = { MSG_GPIO_CMD, current_green_arm, 1 };
                    MsgSend(interlock_coid, &amber_cmd, sizeof(amber_cmd), NULL, 0);
                    usleep(AMBER_MS * 1000);

                    msg_gpio_t red_cmd = { MSG_GPIO_CMD, current_green_arm, 0 };
                    MsgSend(interlock_coid, &red_cmd, sizeof(red_cmd), NULL, 0);
                    usleep(ALLRED_MS * 1000);
                }

                msg_gpio_t green_cmd = { MSG_GPIO_CMD, msg.preempt.target_arm, 2 };
                MsgSend(interlock_coid, &green_cmd, sizeof(green_cmd), NULL, 0);
                current_green_arm = msg.preempt.target_arm;
            }
            else if (!msg.preempt.hold_green && preemption_active) {
                preemption_active = 0;
            }
            MsgReply(rcvid, 0, NULL, 0);
        }
        else if (msg.type == MSG_SNAPSHOT) {
            /* BUG FIX #36: zero the reply struct — amb_lat/amb_lon must not be garbage */
            msg_sys_snap_t reply;
            memset(&reply, 0, sizeof(reply));
            reply.current_arm_green = current_green_arm;
            reply.is_preempted      = preemption_active;
            reply.latency_ms        = 0; /* Handled by logic analyzer */
            MsgReply(rcvid, 0, &reply, sizeof(reply));
        }
    }
    return 0;
}
