#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/neutrino.h>
#include <sys/dispatch.h>
#include "../../Includes/common/ipc_messages.h"
#include "../../Includes/common/protocol_nrf.h"
#include "../../Includes/common/system_topology.h"
#include "../../Includes/common/proc_monitor.h"

// Checksum logic from protocol.h[cite: 6]
uint16_t calculate_checksum(claim_packet_t *p) {
    uint8_t *b = (uint8_t *)p;
    size_t n = sizeof(*p) - sizeof(p->checksum);
    uint16_t sum = 0;
    for (size_t i = 0; i < n; i++) sum += b[i];
    return sum;
}

int main() {
    printf("[RF_TX] Starting NRF24 Transmission Service...\n");
    pm_set_priority("RF_TX", PRIO_SENSOR);

    // Create the IPC Service Name so Kinematics can find us
    name_attach_t *attach = name_attach(NULL, "amb_tx_service", 0);
    if (attach == NULL) {
        perror("[RF_TX] Failed to attach name");
        return 1;
    }
    pm_start_reporting("amb_rf_tx", PRIO_SENSOR);

    // In a real build, we would open /dev/spi0 here[cite: 1, 7]
    int spi_simulated = 1;
    printf("[RF_TX] SPI hardware simulating...\n");

    msg_nav_t rx_msg;
    claim_packet_t pkt;
    long packets_sent = 0;

    for (;;) {
        // Block and wait for a message from Kinematics (No CPU spin)
        int rcvid = MsgReceive(attach->chid, &rx_msg, sizeof(rx_msg), NULL);
        if (rcvid == -1) continue;

        // Handle QNX system pulses (like disconnect events)
        if (rcvid == 0) continue;

        if (rx_msg.type == MSG_NAV_UPDATE) {
            memset(&pkt, 0, sizeof(pkt));
            pkt.magic = PKT_MAGIC;
            strncpy(pkt.amb_id, "AMB-A", sizeof(pkt.amb_id));
            pkt.lat = (float)rx_msg.lat;
            pkt.lon = (float)rx_msg.lon;
            pkt.heading_deg = (float)rx_msg.heading_deg;
            pkt.speed_kmh = (float)rx_msg.speed_kmh;
            pkt.ahead_m = (float)rx_msg.ahead_m;
            pkt.target_junction = (uint16_t)rx_msg.target_junction;
            pkt.checksum = calculate_checksum(&pkt);

            // Transmit to hardware
            if (spi_simulated) {
                packets_sent++;
                printf("[RF_TX] TX SIMULATED (32 bytes) - total sent: %ld\n", packets_sent);
            }

            // Immediately unblock the Kinematics process
            MsgReply(rcvid, 0, NULL, 0);
        } else {
            MsgError(rcvid, ENOSYS);
        }
    }
    return 0;
}
