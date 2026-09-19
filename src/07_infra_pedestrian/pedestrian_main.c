#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/neutrino.h>
#include <sys/dispatch.h>
#include "../../Includes/common/ipc_messages.h"
#include "../../Includes/common/system_topology.h"
#include "../../Includes/common/proc_monitor.h"

int main() {
    printf("[PEDESTRIAN] Crosswalk demand generator active...\n");
    pm_set_priority("PEDESTRIAN", PRIO_SENSOR);

    int arbiter_coid = -1;
    /* BUG FIX #26: arbiter registers as "corridor_arbiter", not "infra_arbiter" */
    while ((arbiter_coid = name_open("corridor_arbiter", 0)) == -1) usleep(500000);
    pm_start_reporting("infra_pedestrian", PRIO_SENSOR);

    for (;;) {
        // A pedestrian presses the crossing button every ~15 seconds
        sleep(15);

        msg_sensor_t msg;
        msg.type = MSG_SENSOR_UPDATE;
        msg.junction_id = 0;
        msg.arm_id = 2; // Crosswalk id
        msg.vehicle_count = 1; // Treat pedestrian as 1 "demand unit"
        msg.is_faulty = 0;

        if (MsgSend(arbiter_coid, &msg, sizeof(msg), NULL, 0) == 0) {
            printf("[PEDESTRIAN] Crosswalk button pressed on Arm %d. Demand sent.\n", msg.arm_id);
        }
    }
    return 0;
}
