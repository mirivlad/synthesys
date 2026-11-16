/* Synthesys HESS v1 PROPOSAL (not hardware/firmware verified).
 * Frames serialized explicitly; do not memcpy packed structs to wire.
 * SPI P0/P1: MOSI=D0, MISO=D1; QPI needs separate electrical drivers.
 */
#ifndef SYNTHESYS_HESS_WIRE_V1_H
#define SYNTHESYS_HESS_WIRE_V1_H
#include <stdint.h>
#define HESS_VERSION 1u
#define HESS_MAGIC 0xA5u
#define HESS_HEADER_BYTES 18u
#define HESS_TRAILER_BYTES 2u
#define HESS_CPU_PHYS_ADDR 0u
#define HESS_FIRST_SLOT 1u
#define HESS_LAST_SLOT 8u
#define HESS_V1_INITIAL_MAX_PAYLOAD 128u

enum hess_frame_type { HESS_REQUEST=1, HESS_RESPONSE=2, HESS_EVENT=3,
                       HESS_DATA=4, HESS_ACK=5, HESS_NACK=6 };
enum hess_status {
  HESS_OK=0, HESS_INVALID_CMD=1, HESS_BAD_ARGUMENT=2, HESS_BUSY=3,
  HESS_NOT_READY=4, HESS_CRC_ERROR=5, HESS_IO_ERROR=6,
  HESS_TIMEOUT=7, HESS_VERSION_MISMATCH=8, HESS_NO_DEVICE=9,
  HESS_NO_SPACE=10, HESS_ABORTED=11, HESS_UNCERTAIN=12
};
/* Proposed header offsets, see docs/hess/04-protocol.md */
enum hess_field_offset {
  HESS_OFF_MAGIC=0, HESS_OFF_VERSION=1, HESS_OFF_TYPE=2,
  HESS_OFF_FLAGS=3, HESS_OFF_SEQ=4, HESS_OFF_COMMAND=6,
  HESS_OFF_LENGTH=8, HESS_OFF_SRC=10, HESS_OFF_DST=12,
  HESS_OFF_TRANSFER_ID=14
};
static inline uint16_t hess_crc16_ccitt_false(const uint8_t *d, unsigned n) {
  uint32_t crc = 0xFFFFu;
  for (unsigned i = 0; i < n; ++i) {
    crc ^= (uint32_t)d[i] << 8;
    for (unsigned b = 0; b < 8; ++b)
      crc = ((crc << 1) ^ ((crc & 0x8000u) ? 0x1021u : 0u)) & 0xFFFFu;
  }
  return (uint16_t)crc;
}
#endif
