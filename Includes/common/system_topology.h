#ifndef SYSTEM_TOPOLOGY_H
#define SYSTEM_TOPOLOGY_H

/*
 * system_topology.h
 *
 * Shared physical/timing/scheduling constants for the Greenlane V2I
 * corridor. Every process that needs "how many arms/vehicles" or "how
 * long is amber" or "what priority do I run at" includes this file.
 */

/* Physical intersection: 4 arms (J1..J4), matching the 4 GPIO LED heads
 * wired in Process 10 (10_gpio_driver/gpio_main.c: head_pins[4][3]). */
#define MAX_APPROACHES   4

/* Simultaneous ambulances the corridor_arbiter's priority queue tracks. */
#define MAX_VEHICLES     8

/* Preemption geometry, metres. TRIGGER_M is how far out a vehicle "claims"
 * a junction; CLEAR_M is how far past it the junction is considered clear
 * again and the claim is dropped. */
#define TRIGGER_M        150.0
#define CLEAR_M          45.0

/* Phase timing, milliseconds. */
#define GREEN_MS         8000
#define AMBER_MS         2500
#define ALLRED_MS        1200

/* How many processes cli_dashboard can show in its live table at once.
 * There are 13 long-running processes under the Watchdog (see
 * 01_supervisor/watchdog_main.c's procs[]); this has headroom above that
 * on purpose so adding a 14th process doesn't silently truncate. */
#define MAX_TRACKED_PROCS  16

/*
 * ---- Real-time scheduling tiers -----------------------------------
 *
 * QNX Neutrino's scheduler only does what you tell it: leave every
 * process at the default priority and "16-process microkernel with SMP
 * scheduling" is just prose. Every long-running process calls
 * pm_set_priority() (see Includes/common/proc_monitor.h) once at start-up
 * with one of these tiers, so the process actually closest to the
 * physical LEDs genuinely preempts best-effort diagnostics under load —
 * this is real sched_setscheduler(SCHED_RR, ...), not cosmetic.
 *
 * Raising a process above the default priority needs root (or the
 * PROCMGR_AID_PRIORITY ability) on the target, the same requirement
 * 10_gpio_driver/gpio_main.c already has for ThreadCtl(_NTO_TCTL_IO,...).
 * Run 01_supervisor as root and every spawned child inherits it.
 */
#define PRIO_SUPERVISOR  30   /* 01: must stay responsive to heal a crash fast */
#define PRIO_HARDWARE    25   /* 10/11: nearest the physical LEDs */
#define PRIO_CONTROL     20   /* 08/09: the preemption control loop */
#define PRIO_SENSOR      15   /* 03/04/05/06/07: RF + kinematics + sensors */
#define PRIO_MONITOR     10   /* 12/13/14: diagnostics — never allowed to
                                * outrank the corridor they're watching */

#endif /* SYSTEM_TOPOLOGY_H */
