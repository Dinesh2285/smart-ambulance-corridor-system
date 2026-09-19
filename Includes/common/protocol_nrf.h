#ifndef PROTOCOL_NRF_H
#define PROTOCOL_NRF_H

#include <stdint.h>

/*
 * protocol_nrf.h
 *
 * Wire format for the simulated nRF24L01 link between the ambulance's
 * RF_TX process (04) and the infrastructure's RF_RX process (05). Both
 * files reference claim_packet_t and PKT_MAGIC but the header never
 * shipped, so those two processes could not compile.
 */

/* Marks a valid Greenlane RF frame ("GLN1" in ASCII), so the receiver can
 * reject noise/garbage before trusting a payload. */
#define PKT_MAGIC 0x474C4E31u

#pragma pack(push, 1)
typedef struct {
    uint32_t magic;             /* PKT_MAGIC */
    char     amb_id[8];         /* e.g. "AMB-A", NUL-padded by strncpy */
    float    lat;
    float    lon;
    float    heading_deg;
    float    speed_kmh;
    float    ahead_m;
    uint16_t target_junction;
    /* checksum MUST stay the last field: calculate_checksum() in
     * rf_tx_main.c / rf_rx_main.c sums every byte of the struct up to
     * (sizeof(claim_packet_t) - sizeof(checksum)), i.e. "everything
     * before this field". Moving checksum, or inserting a field after
     * it, silently breaks the integrity check. */
    uint16_t checksum;
} claim_packet_t;
#pragma pack(pop)

#endif /* PROTOCOL_NRF_H */
