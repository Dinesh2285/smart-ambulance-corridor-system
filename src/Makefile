# Top-level build for the Greenlane V2I Emergency Corridor.
#
# Terminal-only build: the supervisor (01) self-heals 12 long-running
# processes (03-14), plus amb_route (02, one-shot) and chaos_injector
# (15, manual/judge-triggered) = 15 process directories total. No web
# server/HTTP — everything observable lives in the QNX console via
# cli_dashboard (see 14_cli_dashboard/console_main.c).
#
# `make` (or `make all`) builds every process into ../Binaries/; `make run`
# also launches the supervisor, which spawns everything else (see
# 01_supervisor/watchdog_main.c).
#
# Requires the QNX qcc cross-toolchain (aarch64le) on PATH, per every
# subdirectory's own Makefile (CC = qcc, -Vgcc_ntoaarch64le).
#
# NOTE ON THREADS: every process now links Includes/common/proc_monitor.h,
# which uses pthread_create/pthread_setschedparam for real-time priority
# and CPU-usage reporting. QNX Neutrino's libc includes pthread support
# natively (no separate -lpthread needed). If your SDP setup differs and
# the linker reports "undefined reference to pthread_create" or similar,
# add "-lpthread" to CFLAGS/LDFLAGS in the affected subdirectory's own
# Makefile.

PROCESSES = \
	01_supervisor \
	02_amb_route \
	03_amb_kinematics \
	04_amb_rf_tx \
	05_infra_rf_rx \
	06_infra_radar \
	07_infra_pedestrian \
	08_corridor_arbiter \
	09_phase_scheduler \
	10_gpio_driver \
	11_safety_interlock \
	12_cpu_monitor \
	13_logic_analyzer \
	14_cli_dashboard \
	15_fault_injector

.PHONY: all clean run $(PROCESSES)

all: $(PROCESSES)

$(PROCESSES):
	$(MAKE) -C $@

clean:
	@for d in $(PROCESSES); do $(MAKE) -C $$d clean; done
	rm -rf ../Binaries

# Run on the target Pi (as root — the GPIO driver's mmap_device_memory()
# needs I/O privilege, see 10_gpio_driver/gpio_main.c). The supervisor
# spawns every other process with relative paths ("./amb_route" etc), so
# it must be started with Binaries/ as the working directory.
run: all
	cd ../Binaries && ./supervisor
