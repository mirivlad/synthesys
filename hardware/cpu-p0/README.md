# CPU P0 - wiring

[Functional schematic](../../assets/cpu-module-p0.svg) · [40-pin keyed connector pinout](../../assets/hess-40-pinout.svg) · [HESS-40 specification](../../docs/hardware/04-hess-40pin-connector.md)

The pin-nets CSV describes conceptual nets, not pin numbers of specific IC *packages* or a validated multi-master circuit. The hardware grant/OE gate and VBUS isolation MUST be designed and reviewed before using HESS under power. The first firmware keeps HESS disconnected.

Next KiCad task requires selecting the exact ESP32-S3 N16R8 DevKit and buffer/transceiver package, then capturing GPIO+headers and package pins in a full schematic and running ERC.

## Конкретная DevKitC-1 и U2 DIP-14

[Контакты обоих 22-pin header ESP32 и все ноги SN74AHC125N DIP-14](../../docs/hardware/07-devkitc1-dip14-wiring.md). Это карта для **Espressif ESP32-S3-DevKitC-1 v1.1**, а не произвольной 38-pin S3 платы. На CPU P0 выходы U2 остаются Hi-Z; USB right/OTG/Serial-JTAG обеспечивает монитор.
