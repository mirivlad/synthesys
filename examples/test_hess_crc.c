#include <stdio.h>
#include <stdint.h>
#include "../include/hess_wire_v1.h"
int main(void) {
  const uint8_t sample[]={'1','2','3','4','5','6','7','8','9'};
  const uint16_t crc=hess_crc16_ccitt_false(sample,9);
  printf("CRC16/CCITT-FALSE: 0x%04X (expected 0x29B1)\n",crc);
  return crc==0x29B1 ? 0 : 1;
}
