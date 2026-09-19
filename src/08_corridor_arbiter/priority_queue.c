#include <stdio.h>
#include <string.h>
#include "priority_queue.h"
#include "../../Includes/common/system_topology.h"

typedef struct {
    int active;
    char vehicle_id[16];
    double eta_s;
    double ahead_m;
    int target_arm;
} claim_t;

static claim_t fleet[MAX_VEHICLES];

void pq_init(void) {
    memset(fleet, 0, sizeof(fleet));
}

int pq_update_and_get_top(const char* vehicle_id, double eta_s, double ahead_m, int target_arm) {
    int slot = -1;

    // 1. Find existing slot for this vehicle or the first empty slot
    for (int i = 0; i < MAX_VEHICLES; i++) {
        if (fleet[i].active && strcmp(fleet[i].vehicle_id, vehicle_id) == 0) {
            slot = i;
            break;
        }
        if (!fleet[i].active && slot < 0) {
            slot = i;
        }
    }

    // 2. Update the vehicle's telemetry
    if (slot >= 0) {
        fleet[slot].active = 1;
        strncpy(fleet[slot].vehicle_id, vehicle_id, 16);
        fleet[slot].eta_s = eta_s;
        fleet[slot].ahead_m = ahead_m;
        fleet[slot].target_arm = target_arm;
    }

    // 3. Clear out vehicles that have cleared the intersection
    for(int i = 0; i < MAX_VEHICLES; i++) {
        if(fleet[i].active && fleet[i].ahead_m <= -CLEAR_M) {
            printf("[PRIORITY_QUEUE] %s cleared the intersection. Removing claim.\n", fleet[i].vehicle_id);
            fleet[i].active = 0;
        }
    }

    // 4. Find the vehicle with the lowest ETA inside the trigger zone
    int best_idx = -1;
    double min_eta = 999999.0;

    for (int i = 0; i < MAX_VEHICLES; i++) {
        if (fleet[i].active && fleet[i].ahead_m > -CLEAR_M && fleet[i].ahead_m <= TRIGGER_M) {
            if (fleet[i].eta_s < min_eta) {
                min_eta = fleet[i].eta_s;
                best_idx = i;
            }
        }
    }

    // Return the arm of the highest priority vehicle, or -1 if the zone is empty
    if (best_idx >= 0) {
        return fleet[best_idx].target_arm;
    }
    return -1;
}
