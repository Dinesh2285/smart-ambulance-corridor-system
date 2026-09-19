#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <sys/mman.h>
#include <sys/neutrino.h>
#include <sys/dispatch.h>
#include "../../Includes/common/ipc_messages.h"
#include "../../Includes/common/system_topology.h"
#include "../../Includes/common/proc_monitor.h"

#define GPIO_PHYS   0xFE200000u
#define GPIO_SIZE   0x1000u
#define GPFSEL0     (0x00 / 4)
#define GPSET0      (0x1C / 4)
#define GPCLR0      (0x28 / 4)

volatile uint32_t *regs;

// Map Arm ID to physical Pi Header pins
const int head_pins[4][3] = {
    { 17, 27, 22 }, // Arm 0 (J1)
    {  5,  6, 13 }, // Arm 1 (J2)
    { 19, 26, 21 }, // Arm 2 (J3)
    { 16, 20, 12 }  // Arm 3 (J4)
};

void init_pins_as_outputs() {
    for (int arm = 0; arm < 4; arm++) {
        for (int color = 0; color < 3; color++) {
            int pin = head_pins[arm][color];
            int fsel_reg = pin / 10;
            int shift = (pin % 10) * 3;
            // Clear 3 bits, then set to 1 (Output)
            regs[GPFSEL0 + fsel_reg] = (regs[GPFSEL0 + fsel_reg] & ~(7u << shift)) | (1u << shift);
        }
    }
}

void set_lamp(int arm, int state) {
    if (!regs) return;
    // Turn off Red, Amber, and Green
    regs[GPCLR0 + head_pins[arm][0] / 32] = 1u << (head_pins[arm][0] % 32);
    regs[GPCLR0 + head_pins[arm][1] / 32] = 1u << (head_pins[arm][1] % 32);
    regs[GPCLR0 + head_pins[arm][2] / 32] = 1u << (head_pins[arm][2] % 32);

    // Turn on requested color (0=Red, 1=Amber, 2=Green)
    if (state >= 0 && state <= 2) {
        regs[GPSET0 + head_pins[arm][state] / 32] = 1u << (head_pins[arm][state] % 32);
    }
}

#define LAMP_OFF (-1)   /* sentinel for "all LEDs off" — used in ripple test */

int main() {
    pm_set_priority("GPIO_DRIVER", PRIO_HARDWARE);

    if (ThreadCtl(_NTO_TCTL_IO, 0) == -1) {
        perror("[GPIO_DRIVER] FATAL: I/O privilege denied.");
        return 1;
    }

    regs = mmap_device_memory(NULL, GPIO_SIZE, PROT_READ | PROT_WRITE | PROT_NOCACHE, 0, GPIO_PHYS);
    /* BUG FIX #39: MAP_FAILED is (void*)-1, which is truthy — must check explicitly */
    if (regs == MAP_FAILED) {
        printf("[GPIO_DRIVER] mmap_device_memory failed — running in simulation mode.\n");
        regs = NULL;
    } else {
        printf("[GPIO_DRIVER] Memory map successful. Initializing physical LEDs...\n");
        init_pins_as_outputs();

        /* BUG FIX #37: Use LAMP_OFF constant instead of magic -1 */
        printf("[GPIO_DRIVER] Running LED Ripple Test...\n");
        for (int i = 0; i < 4; i++) {
            set_lamp(i, 2);       /* Green On  */
            usleep(250000);
            set_lamp(i, LAMP_OFF); /* All Off   */
        }
        printf("[GPIO_DRIVER] Hardware Test Complete. Waiting for IPC Commands.\n");
    }

    /* BUG FIX #40: check name_attach for NULL before using attach->chid */
    name_attach_t *attach = name_attach(NULL, "infra_gpio", 0);
    if (attach == NULL) {
        perror("[GPIO_DRIVER] FATAL: name_attach failed");
        return 1;
    }
    pm_start_reporting("gpio_driver", PRIO_HARDWARE);

    for (;;) {
        msg_gpio_t cmd;
        int rcvid = MsgReceive(attach->chid, &cmd, sizeof(cmd), NULL);
        if (rcvid <= 0) continue;   /* system pulse or error — skip */

        if (cmd.type == MSG_GPIO_CMD) {
            set_lamp(cmd.arm_id, cmd.lamp_state);
            MsgReply(rcvid, 0, NULL, 0);
        } else {
            /* BUG FIX #38: unhandled message type — must reply or sender deadlocks */
            MsgError(rcvid, ENOSYS);
        }
    }
    return 0;
}
