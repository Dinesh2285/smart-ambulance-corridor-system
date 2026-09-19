#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/neutrino.h>
#include <sys/dispatch.h>
#include "../../Includes/common/ipc_messages.h"
#include "../../Includes/common/system_topology.h"
#include "../../Includes/common/proc_monitor.h"

#define SHM_ROUTE_FILE "/dev/shmem/amb_route.dat"
#define FALLBACK_POINTS 50
#define PULSE_TICK _PULSE_CODE_MINAVAIL

// Haversine and Bearing Math[cite: 1]
#define EARTH_R 6371000.0
#define DEG (M_PI / 180.0)

double bearing_deg(double lat1, double lon1, double lat2, double lon2) {
    double y = sin((lon2 - lon1) * DEG) * cos(lat2 * DEG);
    double x = cos(lat1 * DEG) * sin(lat2 * DEG) - sin(lat1 * DEG) * cos(lat2 * DEG) * cos((lon2 - lon1) * DEG);
    double b = atan2(y, x) / DEG;
    return b < 0.0 ? b + 360.0 : b;
}

int main() {
    printf("[KINEMATICS] Engine starting...\n");
    pm_set_priority("KINEMATICS", PRIO_SENSOR);

    struct { double lat, lon; } poly[FALLBACK_POINTS];

    // 1. Wait for Route Planner to finish
    int fd = -1;
    while((fd = open(SHM_ROUTE_FILE, O_RDONLY)) == -1) {
        usleep(100000); // 100ms
    }
    read(fd, poly, sizeof(poly));
    close(fd);

    // 2. Locate the RF Transmission Service (IPC)
    int rf_coid = -1;
    printf("[KINEMATICS] Waiting for RF TX Service...\n");
    while ((rf_coid = name_open("amb_tx_service", 0)) == -1) {
        sleep(1); // Wait for Watchdog to start RF process
    }
    pm_start_reporting("amb_kinematics", PRIO_SENSOR);

    // 3. Setup Timer Pulse[cite: 1]
    int chid = ChannelCreate(0);
    int timer_coid = ConnectAttach(ND_LOCAL_NODE, 0, chid, _NTO_SIDE_CHANNEL, 0);
    struct sigevent ev;
    timer_t tid;
    SIGEV_PULSE_INIT(&ev, timer_coid, SIGEV_PULSE_PRIO_INHERIT, PULSE_TICK, 0);
    timer_create(CLOCK_MONOTONIC, &ev, &tid);

    struct itimerspec its;
    its.it_value.tv_sec = 0;
    its.it_value.tv_nsec = 100000000; // 100ms tick
    its.it_interval = its.it_value;
    timer_settime(tid, 0, &its, NULL);

    int cur_idx = 0;
    double total_dist = 0.0;

    /* Pre-compute total route distance for ahead_m calculation */
    for (int i = 0; i < FALLBACK_POINTS - 1; i++) {
        double dlat = (poly[i+1].lat - poly[i].lat) * M_PI / 180.0;
        double dlon = (poly[i+1].lon - poly[i].lon) * M_PI / 180.0;
        double a = sin(dlat/2)*sin(dlat/2) +
                   cos(poly[i].lat*M_PI/180.0) * cos(poly[i+1].lat*M_PI/180.0) *
                   sin(dlon/2)*sin(dlon/2);
        total_dist += 2.0 * EARTH_R * asin(sqrt(a));
    }

    double travelled = 0.0;

    for (;;) {
        struct _pulse pulse;
        MsgReceivePulse(chid, &pulse, sizeof(pulse), NULL);

        if (pulse.code == PULSE_TICK) {
            /* BUG FIX #8: cap at FALLBACK_POINTS-2 so poly[cur_idx+1] is always valid */
            if (cur_idx < FALLBACK_POINTS - 2) {
                double seg_dlat = (poly[cur_idx+1].lat - poly[cur_idx].lat) * M_PI / 180.0;
                double seg_dlon = (poly[cur_idx+1].lon - poly[cur_idx].lon) * M_PI / 180.0;
                double seg_a = sin(seg_dlat/2)*sin(seg_dlat/2) +
                               cos(poly[cur_idx].lat*M_PI/180.0) *
                               cos(poly[cur_idx+1].lat*M_PI/180.0) *
                               sin(seg_dlon/2)*sin(seg_dlon/2);
                double seg_m = 2.0 * EARTH_R * asin(sqrt(seg_a));
                travelled += seg_m;
                cur_idx++;
            }

            /* BUG FIX #9: compute real remaining distance to end of route */
            double ahead_m = total_dist - travelled;
            if (ahead_m < 0.0) ahead_m = 0.0;

            msg_nav_t msg;
            msg.type        = MSG_NAV_UPDATE;
            msg.lat         = poly[cur_idx].lat;
            msg.lon         = poly[cur_idx].lon;
            msg.speed_kmh   = 50.0;
            /* BUG FIX #8: safe — cur_idx is always <= FALLBACK_POINTS-2 here */
            msg.heading_deg = bearing_deg(poly[cur_idx].lat, poly[cur_idx].lon,
                                          poly[cur_idx+1].lat, poly[cur_idx+1].lon);
            msg.ahead_m     = ahead_m;
            /* BUG FIX #10: target_junction 0 is correct for single-junction demo;
             * extend to multi-junction by computing proximity to waypoints.    */
            msg.target_junction = 0;

            if (MsgSend(rf_coid, &msg, sizeof(msg), NULL, 0) == -1) {
                printf("[KINEMATICS-FAULT] Lost connection to RF TX. Reconnecting...\n");
                name_close(rf_coid);
                rf_coid = -1;
                while ((rf_coid = name_open("amb_tx_service", 0)) == -1) usleep(500000);
                printf("[KINEMATICS-HEALED] Reconnected to RF TX!\n");
            } else {
                printf("[KINEMATICS] Pos: %.5f, %.5f | ahead: %.1fm -> Sent to RF\n",
                       msg.lat, msg.lon, msg.ahead_m);
            }
        }
    }
    return 0;
}
