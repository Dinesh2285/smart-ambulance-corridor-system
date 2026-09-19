#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/neutrino.h>
#include <sys/dispatch.h>
#include "../../Includes/common/ipc_messages.h"
#include "../../Includes/common/protocol_nrf.h"
#include "../../Includes/common/system_topology.h"
#include "../../Includes/common/proc_monitor.h"

uint16_t calculate_checksum(claim_packet_t *p) {
    uint8_t *b = (uint8_t *)p;
    size_t n = sizeof(*p) - sizeof(p->checksum);
    uint16_t sum = 0;
    for (size_t i = 0; i < n; i++) sum += b[i];
    return sum;
}

int main() {
    printf("[RF_RX] Starting Infrastructure NRF24 Receiver...\n");
    pm_set_priority("RF_RX", PRIO_SENSOR);

    int arbiter_coid = -1;
    printf("[RF_RX] Waiting for Arbiter Service...\n");
    /* BUG FIX #18: arbiter registers as "corridor_arbiter", not "infra_arbiter" */
    while ((arbiter_coid = name_open("corridor_arbiter", 0)) == -1) {
        usleep(500000);
    }
    pm_start_reporting("infra_rf_rx", PRIO_SENSOR);

    claim_packet_t pkt;
    long packets_rx = 0, invalid_rx = 0;

    for (;;) {
        usleep(100000); /* 100ms simulated SPI poll interval */

        /* Build simulated incoming RF packet */
        memset(&pkt, 0, sizeof(pkt));
        pkt.magic       = PKT_MAGIC;
        pkt.lat         = 12.9801f;
        pkt.lon         = 80.2245f;
        /* BUG FIX #20: ahead_m decrements toward 0, then re-triggers at 100m */
        pkt.ahead_m     = 100.0f - (float)(packets_rx % 101);
        /* BUG FIX #21: speed and heading were never set — arbiter divided by zero */
        pkt.speed_kmh   = 50.0f;
        pkt.heading_deg = 45.0f;
        pkt.checksum    = calculate_checksum(&pkt);

        packets_rx++;

        /* BUG FIX #19: save expected checksum BEFORE any possible corruption
         * in real hardware; in simulation this check is always true but the
         * structure is correct for real SPI reads.                           */
        uint16_t expected = pkt.checksum;
        if (pkt.magic != PKT_MAGIC || expected != calculate_checksum(&pkt)) {
            invalid_rx++;
            printf("[RF_RX] Corrupted packet dropped. (Total invalid: %ld)\n", invalid_rx);
            continue;
        }

        /* Convert to IPC format and forward to arbiter */
        msg_nav_t msg;
        memset(&msg, 0, sizeof(msg));
        msg.type            = MSG_NAV_UPDATE;
        msg.lat             = (double)pkt.lat;
        msg.lon             = (double)pkt.lon;
        msg.speed_kmh       = (double)pkt.speed_kmh;
        msg.heading_deg     = (double)pkt.heading_deg;
        msg.ahead_m         = (double)pkt.ahead_m;
        msg.target_junction = (int)pkt.target_junction;

        if (MsgSend(arbiter_coid, &msg, sizeof(msg), NULL, 0) == -1) {
            printf("[RF_RX-FAULT] Connection to Arbiter lost. Reconnecting...\n");
            name_close(arbiter_coid);
            while ((arbiter_coid = name_open("corridor_arbiter", 0)) == -1) usleep(500000);
            printf("[RF_RX-HEALED] Reconnected to Arbiter.\n");
        }
    }
    return 0;
}
