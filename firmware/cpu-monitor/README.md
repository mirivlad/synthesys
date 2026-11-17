# CPU P0 — минимальная USB-прошивка монитора

**Экспериментальная прошивка:** исходники подготовлены для ESP-IDF **5.5.x**, но **не собраны и не прошиты на реальном устройстве в рамках этой редакции** (локальный `idf.py` недоступен). В частности, необходимо проверить реальную конфигурацию USB Serial/JTAG на выбранном ESP32-S3 N16R8 DevKit. Никакой HESS трафик пока не испускается.

## Назначение

- Двусторонний USB Serial/JTAG терминал с ПК, help/version/status/mem/uptime.
- Простая очередь **виртуальных** клавиш, пересылаемых из USB-терминала командами `key tap up`, `key down left`, `key up left`, `input`. Это не USB HID-устройство и не полноценная Input-плата.
- HESS **преднамеренно выключена**: GPIO13 BUS_ENABLE = 0, CLK/D0/XFER переведены в input. На backplane также обязателен **аппаратный pulldown BUS_ENABLE**, даже когда ESP32 перезагружается. `hess`, `modules`, `boot` честно сообщают, что транспорт и загрузчик не реализованы.
- Текстовый монитор для rescue/bring-up, позже — загрузка из Storage.

## Физическое подключение P0

CPU без остальных модулей; HESS JP_PWR разомкнут и все peripheral drivers отключены. DevKit подключить собственным USB кабелем к ПК через USB Serial/JTAG (GPIO19 D−, GPIO20 D+). На платах с двумя USB-портами найти именно порт USB Serial/JTAG. **Не** подключать одновременно HESS +5V и PC USB VBUS без проверенной развязки.

## Сборка ESP-IDF

```sh
cd firmware/cpu-monitor
idf.py set-target esp32s3
idf.py build
idf.py -p /dev/ttyACM0 flash
idf.py -p /dev/ttyACM0 monitor
```

Порт `/dev/ttyACM0` — **пример** для Linux, определите фактический через `ls -l /dev/serial/by-id/`. Если `ESP_CONSOLE_NONE` отключает начальные boot-логи ESP-IDF, это осознанно: USB Serial/JTAG обрабатывает непосредственно один драйвер прошивки, без конкуренции с VFS console. Для fallback-логов можно позднее включить UART0 на GPIO43/44 через 3,3 В USB-UART.

Установка драйвера и работа USB Serial/JTAG: [Espressif ESP-IDF](https://docs.espressif.com/projects/esp-idf/en/v5.5.1/esp32s3/api-guides/usb-serial-jtag-console.html). Низкоуровневые [API ESP-IDF](https://github.com/espressif/esp-idf/blob/master/components/esp_driver_usb_serial_jtag/include/driver/usb_serial_jtag.h).

### Пример сессии

```text
Synthesys CPU P0 recovery monitor
HESS output gating disabled. Type help.
synthesys> help
synthesys> status
CPU: ESP32-S3, 2 core(s), revision ...
HESS: off; Storage: absent/not detected; OS: flash recovery only
synthesys> key left
OK: virtual key tap queued
synthesys> input
seq=1 tick=... press left
seq=2 tick=... release left
synthesys> boot
Storage bootloader not implemented; recovery monitor remains active.
```

## Ограничения P0

Это **не** готовая CPU↔Storage прошивка. Пока нет HESS bus master, SDMMC, FAT32, OS image loader, реального графического вывода и физических клавиатур. После P0 следующим этапом будет M1/P1: аппаратная безопасная HESS, CPU+Storage ping, SD LBA, загрузка boot.ini и OS. Счётчики и CLI команды являются интерфейсной заготовкой.

## Безопасность и наблюдаемость

- Проверить реальную схему devkit перед питанием.
- Включение `BUS_ENABLE` только после утверждения interlock на backplane.
- При сбое USB/отсутствии PC CPU не должен зависать в цикле записи: отправка в TX ограничена тайм-аутом.
- Для первой проверки не подключать другие карты, не подавать 5 В с двух источников.
- `reboot` намеренно перезапускает только CPU; `hess` не имеет команды включения шины.

План этапов: [docs/hardware/06-boot-stages.md](../../docs/hardware/06-boot-stages.md).