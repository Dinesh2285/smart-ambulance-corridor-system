#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

int main(int argc, char **argv) {
    printf("==========================================\n");
    printf(" QNX CHAOS MONKEY - FAULT INJECTOR\n");
    printf("==========================================\n");

    if (argc < 2) {
        printf("Usage: ./chaos_injector <process_name>\n");
        printf("Example: ./chaos_injector infra_radar\n\n");
        return 1;
    }

    char command[256];
    // Uses the QNX 'slay' command to send a fatal SIGKILL (Signal 9)
    snprintf(command, sizeof(command), "slay -9 %s", argv[1]);

    printf("[CHAOS] Injecting fatal SIGKILL (Signal 9) into '%s'...\n", argv[1]);

    // Execute the kill command
    int result = system(command);

    if (result == 0) {
        printf("[CHAOS] Process '%s' successfully terminated.\n", argv[1]);
        printf("[CHAOS] Watch the Watchdog (Process 01) terminal to see the Self-Healing recovery!\n");
    } else {
        printf("[CHAOS] Failed to kill '%s'. Is it currently running?\n", argv[1]);
    }

    return 0;
}
