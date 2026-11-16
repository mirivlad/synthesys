# Synthesis / «Синтезис» — coder work document

Version: hardware bring-up draft  
Target board: ESP32-S3 N16R8 DevKit shown in the supplied photos  
Goal: create firmware and test scaffolding for the first real hardware bring-up.

---

## 0. Core architecture

Synthesis is a modular ESP32-S3 computer.

Modules:

- `CPU` — default bus owner, BIOS/monitor, FAT32, boot logic.
- `Storage` — SD card via SDMMC, block device only.
- `VGA` — video generator, local PSRAM/VRAM.
- `Audio` — I2S audio output, local audio buffer.
- `HID` — keyboard/mouse input.

The CPU controls the system, but large payloads should not permanently require CPU copying. The target architecture allows CPU-sanctioned data sessions where a peripheral temporarily becomes HESS bus master.

---

## 1. HESS bus model

HESS is **not** ordinary SPI with one CS per module.

HESS is a shared addressed half-duplex 4-bit synchronous bus.

Physical HESS lines:

```text
HESS_CLK
HESS_D0
HESS_D1
HESS_D2
HESS_D3
HESS_FRAME
HESS_IRQ
HESS_RST
```

All modules share the same HESS lines. Addressing is inside the frame:

```text
src_id
dst_id
seq
type
cmd/status
flags
payload_len
payload
crc8
```

A module listens to every frame, but executes only if `dst_id == my_module_id` or `dst_id == BROADCAST`.

### Important hardware rule

`HESS_RST` should be wired to the module reset/EN line where possible. It is not a normal data GPIO on peripheral modules.

On the CPU module, use a GPIO-driven open-drain/reset-buffer circuit if software reset of other modules is needed.

---

## 2. Common pin map

Use this common HESS pin map for all modules:

```text
HESS_CLK     GPIO4
HESS_D0      GPIO5
HESS_D1      GPIO6
HESS_D2      GPIO7
HESS_D3      GPIO8
HESS_FRAME   GPIO9
HESS_IRQ     GPIO16
HESS_RST     physical RST/EN pin
```

CPU extra:

```text
HESS_RST_OUT GPIO17  # drive backplane HESS_RST through open-drain transistor/buffer
```

Avoid for normal Synthesis signals unless explicitly verified:

```text
GPIO0       boot/strapping
GPIO3       strapping/JTAG-related
GPIO45      strapping
GPIO46      strapping
GPIO19/20   USB-JTAG / native USB
GPIO35..37  avoid on N16R8/R8-like boards unless verified safe
GPIO48      likely onboard RGB LED on many dev boards; avoid for VGA v0
```

---

## 3. Module IDs

Hard-code IDs in firmware for v0.

```text
0x00 CPU
0x01 Storage
0x02 VGA
0x03 Audio
0x04 HID
0xFF Broadcast
```

Do not implement dynamic enumeration yet.

---

## 4. HESS frame v0

Use a concrete v0 frame.

```text
Offset  Size  Field
0       1     SOF = 0xAA
1       1     version = 0x01
2       1     src_id
3       1     dst_id
4       1     seq
5       1     type
6       1     cmd_or_status
7       1     flags
8       2     payload_len_le
10      N     payload
10+N    1     crc8
```

No EOF in v0. `payload_len` and CRC are enough.

Frame types:

```text
0x01 CMD
0x02 RESPONSE
0x03 DATA
0x04 ACK
0x05 NACK
```

CRC:

```text
CRC-8/ATM polynomial 0x07
```

Endian:

```text
little-endian for all multi-byte fields
```

Payload size v0:

```text
max_payload = 128 bytes
```

Keep it small until HESS is reliable.

---

## 5. HESS PHY v0

Do not start with 80 MHz.

Start with GPIO/bit-banged PHY:

```text
FRAME=1 during the whole frame
CLK idle low
D0..D3 valid before rising edge
receiver samples D0..D3 on rising edge
byte transfer: low nibble first, then high nibble
FRAME=0 after frame
```

Initial speed:

```text
100 kHz
```

Then test:

```text
1 MHz
5 MHz
```

Only after stable tests should hardware SPI/DMA acceleration be considered.

---

## 6. Response window

Only one device may drive `HESS_D0..D3`.

Rules:

- CPU is default bus owner.
- Non-owner modules keep D0..D3 as input/Hi-Z.
- A peripheral may answer only during a response window opened by CPU.
- In v1, a peripheral may drive the bus only after `GRANT_BUS`.

v0 response flow:

```text
CPU sends CMD to module.
CPU releases D0..D3.
Addressed module sends RESPONSE.
CPU closes response window.
```

---

## 7. Bus ownership v0/v1

### v0

No temporary bus master. CPU remains owner.

Data path:

```text
Storage -> CPU -> VGA
```

This is allowed only as bring-up fallback.

### v1

Add bus ownership commands:

```text
GRANT_BUS
RELEASE_BUS
BUS_ABORT
GET_BUS_OWNER
```

Target direct load:

```text
CPU -> VGA: LOAD_FROM_STORAGE(lba, len, local_addr)
CPU -> VGA: GRANT_BUS(target=Storage, timeout=...)
VGA -> Storage: READ_BLOCKS(...)
Storage -> VGA: DATA...
VGA -> CPU: IRQ_READY
```

Do not implement v1 before v0 is stable.

---

## 8. 20-pin HESS slot

Proposed physical slot:

```text
1   5V
2   3V3
3   GND
4   GND
5   HESS_CLK
6   HESS_D0
7   HESS_D1
8   HESS_D2
9   HESS_D3
10  HESS_FRAME
11  HESS_IRQ
12  HESS_RST
13  EN/ID0 optional
14  RES0
15  RES1
16  RES2
17  RES3
18  RES4
19  GND
20  GND
```

Electrical rules:

- all logic is 3.3V;
- IRQ must be open-drain/open-collector with pull-up;
- add 22–47 ohm series resistors on CLK, FRAME, D0..D3 near the current driver where practical;
- all boards must share ground;
- keep several GND pins in the slot;
- do not use reserved pins until explicitly assigned.

---

## 9. Module-specific pin maps

### CPU module

```text
HESS_CLK        GPIO4
HESS_D0         GPIO5
HESS_D1         GPIO6
HESS_D2         GPIO7
HESS_D3         GPIO8
HESS_FRAME      GPIO9
HESS_IRQ        GPIO16
HESS_RST_OUT    GPIO17   # via open-drain/reset buffer to slot HESS_RST
UART0_TX/RX     board TX/RX or USB debug
```

### Storage module

```text
HESS_CLK        GPIO4
HESS_D0         GPIO5
HESS_D1         GPIO6
HESS_D2         GPIO7
HESS_D3         GPIO8
HESS_FRAME      GPIO9
HESS_IRQ        GPIO16
HESS_RST        physical RST/EN

SDMMC_CLK       GPIO10
SDMMC_CMD       GPIO11
SDMMC_D0        GPIO12
SDMMC_D1        GPIO13
SDMMC_D2        GPIO14
SDMMC_D3        GPIO15

SD_DETECT       GPIO17 optional
SD_POWER_EN     GPIO18 optional
```

### VGA module

v0: 6-bit RGB + sync.

```text
HESS_CLK        GPIO4
HESS_D0         GPIO5
HESS_D1         GPIO6
HESS_D2         GPIO7
HESS_D3         GPIO8
HESS_FRAME      GPIO9
HESS_IRQ        GPIO16
HESS_RST        physical RST/EN

VGA_R0          GPIO38
VGA_R1          GPIO39
VGA_G0          GPIO40
VGA_G1          GPIO41
VGA_B0          GPIO42
VGA_B1          GPIO47
VGA_HSYNC       GPIO21
VGA_VSYNC       GPIO18
```

Do not use GPIO48 for VGA v0 because it is often connected to onboard RGB LED.

### Audio module

```text
HESS_CLK        GPIO4
HESS_D0         GPIO5
HESS_D1         GPIO6
HESS_D2         GPIO7
HESS_D3         GPIO8
HESS_FRAME      GPIO9
HESS_IRQ        GPIO16
HESS_RST        physical RST/EN

I2S_BCLK        GPIO38
I2S_LRCLK       GPIO39
I2S_DOUT        GPIO40
I2S_MCLK        GPIO41 optional
AUDIO_MUTE      GPIO42 optional
```

### HID module

```text
HESS_CLK          GPIO4
HESS_D0           GPIO5
HESS_D1           GPIO6
HESS_D2           GPIO7
HESS_D3           GPIO8
HESS_FRAME        GPIO9
HESS_IRQ          GPIO16
HESS_RST          physical RST/EN

PS2_KBD_CLK       GPIO38
PS2_KBD_DATA      GPIO39
PS2_MOUSE_CLK     GPIO40 optional
PS2_MOUSE_DATA    GPIO41 optional
```

---

## 10. Bring-up milestones

### M0 — Docs and pinout freeze

- Freeze HESS frame v0.
- Freeze common HESS GPIO map.
- Freeze slot pinout.
- Print pinout diagrams.
- Prepare wiring harness.

### M1 — CPU ↔ Storage HESS ping

Hardware: two ESP32-S3 boards only.

Implement:

```text
PING
GET_INFO
GET_STATUS
```

Success:

```text
10,000 PING/GET_INFO cycles with zero CRC errors at 100 kHz
then repeat at 1 MHz
```

### M2 — Storage standalone SDMMC

Implement Storage local test over UART:

```text
sd_info
read_lba 0
read_lba N
write_test
verify
```

Success:

```text
SD card reads/writes reliably in 4-bit SDMMC mode
```

### M3 — CPU reads Storage over HESS

Implement:

```text
STORAGE_GET_INFO
STORAGE_READ_BLOCKS
```

Split 512-byte block into DATA frames of <=128 bytes.

Success:

```text
CPU reads LBA 0 via HESS and validates expected content/CRC
```

### M4 — VGA standalone

No HESS yet.

Implement:

```text
color bars
grid
text buffer in PSRAM
```

Success:

```text
monitor shows stable signal
```

### M5 — CPU writes to VGA over HESS

Implement:

```text
VGA_GET_INFO
VGA_SET_MODE
VGA_WRITE_MEM
VGA_FILL_MEM
VGA_COMMIT_REGION
```

Success:

```text
CPU displays "SYNTHESIS READY" on VGA
```

### M6 — Storage -> CPU -> VGA

Implement v0 fallback transfer:

```text
CPU reads raw screen/tile from Storage
CPU writes it to VGA memory
VGA displays it
```

Success:

```text
image/resource from SD appears on display
```

### M7 — HID PS/2

Implement HID standalone, then HESS event polling.

Success:

```text
keyboard input appears on VGA through CPU
```

### M8 — BIOS monitor

Implement CPU monitor:

```text
modules
storage info
read lba
vga mode
vga fill
vga write
hid poll
```

### M9 — FAT32 on CPU

CPU mounts FAT32 using Storage block reads.

Success:

```text
CPU reads /boot.ini and /gfx/test.bin
```

### M10 — GRANT_BUS prototype

Only after M1-M9.

Implement:

```text
GRANT_BUS
RELEASE_BUS
BUS_TIMEOUT
BUS_RECOVERY
```

First success:

```text
CPU grants bus to VGA
VGA pings Storage
VGA releases bus
CPU regains control
```

Second success:

```text
VGA directly reads blocks from Storage and loads VRAM
```

### M11 — Audio

Standalone tone first, then HESS write buffer, then Storage load.

### M12 — shell and ELF

Do not start ELF before HESS, Storage, VGA, HID and FAT32 are stable.

---

## 11. What not to do yet

Do not:

- implement dynamic module enumeration in v0;
- implement ELF loader before hardware bring-up;
- start at 80 MHz;
- use GPIO35..37 without explicit board-level verification;
- use strapping pins for critical signals;
- use GPIO48 for VGA v0;
- assume HESS is just normal SPI;
- allow two modules to drive D0..D3 at the same time;
- skip standalone Storage and VGA tests.

---

## 12. Deliverables expected from coder

For each milestone:

1. Code changes.
2. Wiring assumptions.
3. Build instructions.
4. Flash instructions.
5. Serial monitor expected output.
6. Test command.
7. Pass/fail result.
8. Known limitations.
9. Next recommended step.

No milestone is complete without a real hardware test or a clear statement that it was not tested on hardware.