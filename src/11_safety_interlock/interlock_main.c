#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <sys/neutrino.h>
#include <sys/dispatch.h>
#include "../../Includes/common/ipc_messages.h"
#include "../../Includes/common/system_topology.h"
#include "../../Includes/common/proc_monitor.h"

int current_states[MAX_APPROACHES] = {0}; // Track states: 0=Red, 1=Amber, 2=Green

int main() {
    printf("[INTERLOCK] Safety Interlock active. Enforcing collision prevention.\n");
    pm_set_priority("INTERLOCK", PRIO_HARDWARE);

    // 1. Setup listening channel for the Scheduler
    name_attach_t *attach = name_attach(NULL, "safety_interlock", 0);
    if (attach == NULL) {
        perror("[INTERLOCK] FATAL: name_attach failed");
        return 1;
    }

    // 2. Connect to the physical GPIO driver
    int gpio_coid = -1;
    while ((gpio_coid = name_open("infra_gpio", 0)) == -1) usleep(500000);
    pm_start_reporting("safety_interlock", PRIO_HARDWARE);

    for (;;) {
        msg_gpio_t req;
        int rcvid = MsgReceive(attach->chid, &req, sizeof(req), NULL);
        if (rcvid <= 0) continue;

        if (req.type == MSG_GPIO_CMD) {
            // THE AUDIT LOGIC: Check if this command creates a double-green fault
            int green_count = (req.lamp_state == 2) ? 1 : 0;

            for (int i = 0; i < MAX_APPROACHES; i++) {
                if (i != req.arm_id && current_states[i] == 2) green_count++;
            }

            if (green_count > 1) {
                printf("[INTERLOCK-FATAL] SCHEDULER REQUESTED CONFLICTING GREENS! VETOING COMMAND.\n");
                /* BUG FIX: Force ALL arms to Red, not just arm 0.
                 * Previous code left arms 1-3 in their existing state,
                 * which could still be Green — defeating the safety veto. */
                for (int arm = 0; arm < MAX_APPROACHES; arm++) {
                    msg_gpio_t override = { MSG_GPIO_CMD, arm, 0 }; /* Red */
                    MsgSend(gpio_coid, &override, sizeof(override), NULL, 0);
                    current_states[arm] = 0;
                }
                MsgError(rcvid, EPERM); /* Deny the IPC request from Scheduler */
            } else {
                // Command is safe. Pass it through to the hardware.
                current_states[req.arm_id] = req.lamp_state;
                MsgSend(gpio_coid, &req, sizeof(req), NULL, 0);
                MsgReply(rcvid, 0, NULL, 0);
            }
        }
    }
    return 0;
}
