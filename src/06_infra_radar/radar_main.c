#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>
#include <sys/neutrino.h>
#include <sys/dispatch.h>
#include "../../Includes/common/ipc_messages.h"
#include "../../Includes/common/system_topology.h"
#include "../../Includes/common/proc_monitor.h"

int hardware_glitch_counter = 0;
int consecutive_failures = 0;

// FDIR TIER 1: Self-Healing Logic
int self_heal_sensor() {
    printf("[RADAR-FDIR] Glitch detected! Attempting I2C bus reset... (Attempt %d/3)\n", consecutive_failures + 1);
    usleep(200000); // Simulate reset time

    // 50% chance the software reset actually fixes the hardware
    if (rand() % 2 == 0) {
        printf("[RADAR-FDIR] Self-healing SUCCESSFUL. Sensor stabilized.\n");
        consecutive_failures = 0;
        return 1;
    }
    printf("[RADAR-FDIR] Self-healing FAILED.\n");
    consecutive_failures++;
    return 0;
}

int main() {
    srand(time(NULL) ^ getpid());
    printf("[RADAR_SENSOR] Stopline vehicle detection active...\n");
    pm_set_priority("RADAR_SENSOR", PRIO_SENSOR);

    int arbiter_coid = -1;
    /* BUG FIX #22: arbiter registers as "corridor_arbiter", not "infra_arbiter" */
    while ((arbiter_coid = name_open("corridor_arbiter", 0)) == -1) usleep(500000);
    pm_start_reporting("infra_radar", PRIO_SENSOR);

    for (;;) {
        sleep(2); // Poll sensor every 2 seconds

        // Randomly simulate a hardware register glitch (1 in 10 chance)
        if (rand() % 10 == 0) {
            if (!self_heal_sensor()) {
                if (consecutive_failures >= 3) {
                    // FDIR TIER 2 TRIGGER: Cannot self-heal.
                    // Crash the process intentionally so the Watchdog isolates it.
                    printf("[RADAR-FATAL] Unrecoverable hardware state! Requesting Watchdog Isolation!\n");
                    exit(1);
                }
                continue; // Skip this read and try again
            }
        }

        // Normal Operation: Send civilian vehicle count to Arbiter
        msg_sensor_t msg;
        msg.type = MSG_SENSOR_UPDATE;
        msg.junction_id = 0;
        msg.arm_id = 1; // Side street
        msg.vehicle_count = rand() % 5;
        msg.is_faulty = 0;

        MsgSend(arbiter_coid, &msg, sizeof(msg), NULL, 0);
        printf("[RADAR_SENSOR] Arm %d reports %d vehicles waiting.\n", msg.arm_id, msg.vehicle_count);
    }
    return 0;
}
