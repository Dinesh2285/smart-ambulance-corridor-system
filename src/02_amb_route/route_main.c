#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>

#define SHM_ROUTE_FILE "/dev/shmem/amb_route.dat"
#define FALLBACK_POINTS 50

typedef struct {
    double lat, lon;
} waypoint_t;

int main(int argc, char **argv) {
    printf("[ROUTE_PLANNER] Starting route generation on Core 1...\n");

    // Default coordinates if none provided
    double start_lat = 12.97, start_lon = 80.22;
    double end_lat = 13.01, end_lon = 80.25;

    waypoint_t poly[FALLBACK_POINTS];

    // Generate simulated straight-line polyline[cite: 1]
    for (int i = 0; i < FALLBACK_POINTS; i++) {
        double f = (double)i / (double)(FALLBACK_POINTS - 1);
        poly[i].lat = start_lat + (end_lat - start_lat) * f;
        poly[i].lon = start_lon + (end_lon - start_lon) * f;
    }

    // Write to QNX Shared Memory for the Kinematics engine to read
    int fd = open(SHM_ROUTE_FILE, O_CREAT | O_RDWR | O_TRUNC, 0666);
    if (fd == -1) {
        perror("[ROUTE_PLANNER] Failed to open shared memory");
        return 1;
    }
    write(fd, poly, sizeof(waypoint_t) * FALLBACK_POINTS);
    close(fd);

    printf("[ROUTE_PLANNER] Generated %d waypoints to %s. Exiting cleanly.\n", FALLBACK_POINTS, SHM_ROUTE_FILE);
    return 0;
}
