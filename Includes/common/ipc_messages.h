#ifndef IPC_MESSAGES_H
#define IPC_MESSAGES_H

#include <stdint.h>
#include "system_topology.h"

/*
 * ipc_messages.h
 *
 * Shared QNX message-passing structs for every process in the Greenlane
 * V2I corridor.
 *
 * Every message struct below starts with `uint16_t type` at offset 0 on
 * purpose: several processes (corridor_arbiter, phase_scheduler) receive
 * into a `union { uint16_t type; msg_x_t x; ... }` and dispatch on
 * `msg.type` before touching the specific member — that only works if
 * `type` is the first field of every struct that can appear in the
 * union, so keep it that way if you add a new message type.
 */

/* ---- message type tags ------------------------------------------------ */
#define MSG_NAV_UPDATE      1   /* ambulance telemetry: 03/05 -> 04, 05 -> 08 */
#define MSG_GPIO_CMD        2   /* lamp command: 09 -> 11 -> 10 */
#define MSG_PREEMPT_CMD     3   /* preemption request: 08 -> 09 */
#define MSG_SENSOR_UPDATE   4   /* side/pedestrian demand: 06/07 -> 08 */
#define MSG_SENSOR_FAULT    5   /* reserved: 01 -> 08 isolation notice */
#define MSG_SNAPSHOT        6   /* state pull: 14 -> 01/08/09/12/13 */
#define MSG_PROC_STATS      7   /* periodic self-report: any -> 12 */

/* ---- 03/05 -> 04, 05 -> 08: ambulance nav telemetry -------------------- */
typedef struct {
    uint16_t type;              /* MSG_NAV_UPDATE */
    double   lat;
    double   lon;
    double   speed_kmh;
    double   heading_deg;
    double   ahead_m;           /* metres to target junction; negative once past it */
    int      target_junction;
} msg_nav_t;

/* ---- 09 -> 11 -> 10: traffic-light lamp command ------------------------ */
typedef struct {
    uint16_t type;              /* MSG_GPIO_CMD */
    int      arm_id;            /* 0..3 == J1..J4, see system_topology.h MAX_APPROACHES */
    int      lamp_state;        /* 0=Red 1=Amber 2=Green */
} msg_gpio_t;

/* ---- 08 -> 09: preemption request -------------------------------------- */
typedef struct {
    uint16_t type;              /* MSG_PREEMPT_CMD */
    int      target_arm;
    int      hold_green;        /* 1 = request/hold green, 0 = release */
} msg_preempt_t;

/* ---- 06/07 -> 08: side-street / crosswalk demand ----------------------- */
typedef struct {
    uint16_t type;              /* MSG_SENSOR_UPDATE */
    int      junction_id;
    int      arm_id;
    int      vehicle_count;
    int      is_faulty;
} msg_sensor_t;

/* ---- generic "pull my state" request, reused against several services -- */
typedef struct {
    uint16_t type;              /* MSG_SNAPSHOT */
} msg_snapshot_req_t;

/*
 * Reply payload shared by corridor_arbiter (08) and phase_scheduler (09).
 * Each service only owns a subset of these fields and zeroes the rest
 * before replying, so process 14 (cli_dashboard) can poll both with one
 * struct type:
 *   - corridor_arbiter fills amb_lat / amb_lon
 *   - phase_scheduler  fills current_arm_green / is_preempted / latency_ms
 */
typedef struct {
    double amb_lat;
    double amb_lon;
    int    current_arm_green;
    int    is_preempted;
    double latency_ms;
} msg_sys_snap_t;

/* ---- any long-running process -> 12: periodic self-report -------------
 * Sent once a second by a background thread (see
 * Includes/common/proc_monitor.h's pm_start_reporting()). cpu_pct is a
 * genuine measurement — clock_gettime(CLOCK_PROCESS_CPUTIME_ID, ...)
 * against wall-clock time — not a random number. */
typedef struct {
    uint16_t type;               /* MSG_PROC_STATS */
    char     name[16];
    int      pid;
    int      priority;
    double   cpu_pct;
} msg_proc_stats_t;

/* One process's live entry in the table 12_cpu_monitor hands back on
 * MSG_SNAPSHOT. valid==0 means "never reported" (e.g. a one-shot process
 * like amb_route that finished before anyone asked). */
typedef struct {
    char   name[16];
    int    pid;
    int    priority;
    double cpu_pct;
    int    valid;
} proc_stat_t;

/* ---- 12 -> 14: live process/scheduling table --------------------------- */
typedef struct {
    proc_stat_t procs[MAX_TRACKED_PROCS];
    int         count;
} msg_cpu_snap_t;

/* One process's health entry as tracked by the Watchdog's own FDIR loop. */
typedef struct {
    char name[16];
    int  pid;
    int  restart_count;
    int  isolated;
} proc_health_t;

/* ---- 01 -> 14: live supervisor / FDIR table ----------------------------
 * Served from a background thread in 01_supervisor so the main wait()
 * loop that actually does the healing is never touched by this. */
typedef struct {
    proc_health_t procs[MAX_TRACKED_PROCS];
    int           count;
} msg_supervisor_snap_t;

#endif /* IPC_MESSAGES_H */
