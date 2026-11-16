# GPIO ESP32-S3 N16R8: предварительные карты

**Ниже — проектная разводка P1, не окончательная распиновка.** Для конкретной платы сравнить BOM devkit, маркировку WROOM, схему PSRAM, pin labels, boot straps и внешний USB.

## Нельзя считать свободными

- GPIO33–37 у ESP32-S3 с Octal PSRAM 8 МБ используются PSRAM и не выделяются на HESS/видео.
- GPIO26–32 обычно задействованы интерфейсом Flash/PSRAM.
- GPIO0, 3, 45, 46 — особые boot/strapping; не вешать на них bus gating без исследования.
- GPIO19,20 — native USB/JTAG; сохраните для прошивки/диагностики.
- GPIO43,44 — на ряде модулей UART0; GPIO48 на многих devkit занят RGB LED.
- GPIO 5V tolerant **нет** — всё логическое 3,3 В, клавиатура PS/2 требует согласования.

Источник: https://docs.espressif.com/projects/esp-idf/en/stable/esp32s3/api-reference/peripherals/gpio.html

## Предлагаемый набор на периферийном модуле

| Назначение | GPIO (кандидат) | Условие |
|---|---:|---|
| HESS_CLK через OE / также SPI-slave CLK | 4 | проверить SPI2/3 GPIO Matrix |
| HESS_D0 (MOSI) | 5 | CPU/peripheral: master TX или slave RX |
| HESS_D1 (MISO) | 6 | slave TX только под CS |
| HESS_D2 (QPI резерв) | 7 | пока Hi-Z |
| HESS_D3 (QPI резерв) | 8 | пока Hi-Z |
| HESS_XFER_OUT_N | 9 | master-путь gated, НЕ общий push-pull |
| HESS_SLOT_CS_N | 16 | вход от decoder |
| HESS_GRANT_N | 17 | вход от decoder в аппаратную схему OE |
| HESS_ATTN_N | 18 | только open-drain |
| HESS_RST_N | EN/reset input | не GPIO данных |
| SLOT_ID | 1–3 свободных входа или ID EEPROM | по PCB |

Эти GPIO недостаточны для утверждения совмещённой конфигурации: одновременно нужны SPI2 slave и SPI3 master, Video LCD_CAM, SDMMC, Audio I2S, USB, debug. До M0 **полная таблица занятости выводов составляется отдельно для каждой карты**, и только затем замораживается.

## Storage (дополнительные GPIO, отдельный проект)

Прежний вариант: SDMMC CLK=10, CMD=11, D0=12, D1=13, D2=14, D3=15. На ESP32-S3 SDMMC может использовать GPIO Matrix, но это не готовая разводка: проверить speed, pull-ups, картридер и соответствие аппаратному интерфейсу. SDMMC 1-bit допустим для первого запуска, 4-bit позже.

Источник: https://docs.espressif.com/projects/esp-idf/en/latest/esp32s3/api-reference/peripherals/sdmmc_host.html

## Video

LCD_CAM / параллельный RGB с отдельными HSYNC, VSYNC и RGB выходами требует выделения большой группы GPIO. Историческая идея 6-bit R2/G2/B2, HSYNC, VSYNC — только пример. Перед заморозкой проверить, что видеогенератор и SPI master/slave могут одновременно жить в ESP32-S3 и передавать данные без артефактов.

## Audio

I2S BCLK, LRCLK, DATA к DAC PCM5102A (возможно MCLK); pin candidates 38–41. Подключение и MCLK зависят от конкретного DAC, не от универсальной легенды.

## HID

PS/2 CLK/DATA условно 38/39; с уровнями 5 В использовать входные буферы или согласование на линии open-collector. В дальнейшем USB Host/другие устройства — отдельный расчёт GPIO/USB возможностей.

## Контроль перед пайкой

1. Проверить, что на всех пяти конкретных devkit есть маркировка ESP32-S3 N16R8.
2. Получить разводку платы; зафиксировать USB, UART, LED, strap, PSRAM.
3. Составить таблицу функций **каждого** GPIO с пометкой направления и OE.
4. Проверить, что boot/reset не включает bus-drivers.
5. Отдельно протестировать SPI slave, master и SDMMC/I2S/LCD_CAM.
6. Зафиксировать распиновку P1 только после испытаний; убрать слово «кандидат».