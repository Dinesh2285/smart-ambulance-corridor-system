s# smart-ambulance-corridor-system
Greenlane V2I is a real-time emergency traffic arbitration engine built on QNX Neutrino RTOS for Raspberry Pi 4. It uses a 16-process microkernel architecture featuring IPC multicore processing, bare-metal GPIO safety interlocks, FDIR self-healing, and an embedded HTTP server powering a live Leaflet.js GPS map dashboard."


# Greenlane V21: Emergency Corridor Arbitration System

A real-time, fault-tolerant Vehicle-to-Infrastructure (V2I) emergency corridor arbitration system built on the **QNX Neutrino RTOS** for the **Raspberry Pi 4B (BCM2711)**.

Greenlane V21 grants approaching emergency vehicles right-of-way through signalled intersections by safely pre-empting standard traffic-light cycles. Rather than a monolithic binary, the system runs as **15 independent, communicating user-space processes** leveraging native QNX synchronous message-passing IPC, POSIX real-time scheduling (`SCHED_RR`), and continuous Fault Detection, Isolation, and Recovery (FDIR).

---

## Key Features

* **Microkernel Architecture**: 15 single-responsibility processes; device drivers, arbiters, and sensors run entirely in user space to guarantee fault containment.
* **Synchronous Message-Passing IPC**: Clean client-server interfaces via QNX native channels (`name_attach`, `name_open`, `MsgSend`, `MsgReceive`, `MsgReply`) with lazy-reconnect support.
* **Tiered POSIX Real-Time Scheduling**: All critical threads run under `SCHED_RR` with strict priority separation (Supervisor > Hardware > Control > Sensors > Observability).
* **Self-Healing Supervision (FDIR)**: A dedicated Watchdog supervisor detects non-zero exits or fatal signals, respawns crashed processes up to 3 times, and isolates repeated failures.
* **Hardware Safety Interlock**: An independent hardware arbitration process sits between the phase scheduler and GPIO drivers, physically enforcing that conflicting approach arms never show green concurrently[cite: 1].
* **Register-Level BCM2711 GPIO Driver**: Maps physical SoC peripherals using `mmap_device_memory()` with `ThreadCtl(_NTO_TCTL_IO, 0)` privileges to directly drive `GPFSEL`, `GPSET`, and `GPCLR`[cite: 1].
* **Live In-Terminal Dashboard**: Displays real, measured per-process CPU utilization (`CLOCK_PROCESS_CPUTIME_ID` vs. `CLOCK_MONOTONIC`), SCHED_RR priorities, restart counts, and junction states[cite: 1].

---
---

## Process Breakdown & Priority Tiers

Processes raise their calling thread scheduling policy using `pthread_setschedparam()`[cite: 1]. Priorities are assigned according to safety criticality[cite: 1]:

| # | Process Binary | Priority Tier | Description |
|---|---|---|---|
| **01** | `supervisor` | **Tier 30** (Supervisor) | System bootstrapper and FDIR watchdog; monitors child terminations[cite: 1]. |
| **02** | `amb_route` | *Transient* | One-shot binary: writes a 50-point waypoint polyline into shared memory and exits[cite: 1]. |
| **03** | `amb_kinematics` | **Tier 15** (Sensor) | Dead-reckons position every 100 ms via POSIX timer pulses, computing bearing and distance[cite: 1]. |
| **04** | `amb_rf_tx` | **Tier 15** (Sensor) | Simulates NRF24 OTA transmission; packages kinematics into checksummed packets[cite: 1]. |
| **05** | `infra_rf_rx` | **Tier 15** (Sensor) | Validates incoming packet checksums and forwards claims to the Arbiter[cite: 1]. |
| **06** | `infra_radar` | **Tier 15** (Sensor) | Simulates approach-radar tracking data[cite: 1]. |
| **07** | `infra_pedestrian`| **Tier 15** (Sensor) | Simulates pedestrian crosswalk button presses and crossing demand[cite: 1]. |
| **08** | `corridor_arbiter`| **Tier 20** (Control) | Manages vehicle priority queue (150 m trigger / 45 m clear); triggers pre-emptions[cite: 1]. |
| **09** | `phase_scheduler` | **Tier 20** (Control) | State machine executing phase intervals (Amber: 2.5s, All-Red: 1.2s, Green)[cite: 1]. |
| **10** | `gpio_driver` | **Tier 25** (Hardware) | Direct register I/O (`GPFSEL`, `GPSET`, `GPCLR`) to drive physical LEDs[cite: 1]. |
| **11** | `safety_interlock`| **Tier 25** (Hardware) | Intermediate gatekeeper preventing conflicting approaches from ever showing green simultaneously[cite: 1]. |
| **12** | `cpu_monitor` | **Tier 10** (Monitor) | Aggregates self-reported CPU metrics and priorities from all processes[cite: 1]. |
| **13** | `logic_analyzer` | **Tier 10** (Monitor) | Latency tracking daemon checking end-to-end RF-to-GPIO time against a 250 ms budget[cite: 1]. |
| **14** | `cli_dashboard` | **Tier 10** (Monitor) | Full-screen ncurses/terminal monitor refreshed at 1 Hz[cite: 1]. |
| **15** | `chaos_injector` | *Manual Tool* | Diagnostic tool allowing manual `SIGKILL` injection to test FDIR live[cite: 1]. |

---

## Fault Detection, Isolation & Recovery (FDIR)

The supervisor manages stability through an automated lifecycle[cite: 1]:

1. **Detection**: The supervisor blocks on POSIX `wait()`[cite: 1]. When any child process terminates, it evaluates `WIFEXITED()` and `WEXITSTATUS()`[cite: 1].
2. **Evaluation**:
   * Exit code `0` is treated as an expected, clean shutdown (e.g., `amb_route`) and is not restarted[cite: 1].
   * Non-zero exits or terminations caused by signals (e.g., `SIGSEGV`, `SIGKILL`) trigger the recovery routine[cite: 1].
3. **Recovery Window**:
   * If a process crashes fewer than 3 times within a rolling 10-second stability window, it is instantly respawned[cite: 1].
   * If a process fails 4 times consecutively without sustaining 10 seconds of uptime, it is tagged as faulty and permanently isolated to prevent CPU thrashing[cite: 1].

---

## Directory Structure

```text
├── Makefile                   # Multi-binary master build configuration
├── Includes/
│   └── common/
│       ├── ipc_messages.h     # Shared C structs and union definitions
│       ├── system_topology.h  # Junction layouts and timing configurations
│       ├── protocol_nrf.h     # RF packet definitions and checksum layouts
│       └── proc_monitor.h     # Shared CPU metrics and SCHED_RR helper
└── src/
    ├── supervisor/            # Process 01 (FDIR watchdog)
    ├── amb_route/             # Process 02 (Waypoints)
    ├── amb_kinematics/        # Process 03 (Dead-reckoning)
    ├── amb_rf_tx/             # Process 04 (Simulated TX)
    ├── infra_rf_rx/           # Process 05 (Simulated RX)
    ├── infra_radar/           # Process 06 (Approach radar)
    ├── infra_pedestrian/      # Process 07 (Crosswalk logic)
    ├── corridor_arbiter/      # Process 08 (Priority queue & arbitration)
    ├── phase_scheduler/       # Process 09 (Traffic sequencing state machine)
    ├── gpio_driver/           # Process 10 (BCM2711 register-level driver)
    ├── safety_interlock/      # Process 11 (Hardware validation proxy)
    ├── cpu_monitor/           # Process 12 (CPU measurement aggregator)
    ├── logic_analyzer/        # Process 13 (Latency profiler)
    ├── cli_dashboard/         # Process 14 (Terminal UI)
    └── chaos_injector/        # Process 15 (Fault injection tool)
