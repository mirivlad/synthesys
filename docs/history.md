# Происхождение и изменение архитектуры

Оригинальные исторические файлы сохранены **без редактирования по смыслу** в каталоге [legacy/SPEC-2025.md](../legacy/SPEC-2025.md) и не являются текущими инструкциями по пайке:

- `legacy/SPEC-2025.md` — первоначальная спецификация с CPU master, кадром `0xAA`, CRC8, QPI до 80 МГц (не подтверждено).
- `legacy/README-early.md` — ранняя концепция CPU, Storage, Video, HID, Audio.
- `legacy/SYNTHESIS_CODER_WORKDOC-2026.md` — рабочие тесты ESP32-S3, GPIO4–9, HESS_FRAME и переход от CPU-only к peer mastering.
- `legacy/README-v0.2-2026.md` — перечень восстановленного комплекта документации v0.2, который прежде содержал более 40 Markdown-файлов. Полный оригинальный архив не восстановлен.

## Как менялась HESS

1. **v1.0 (2025):** CPU — единственный master, периферия slave; противоречие с идеей прямых передач.
2. **v0 / рабочее ТЗ:** общая 4-битная адресная шина и возможность grant, ранние спекулятивные GPIO и пакеты.
3. **v0.3 (16.11.2026, отвергнут):** две отдельные SPI шины, HESS-C / HESS-D, Storage постоянно master данных; оказалось слишком узким для будущих модулей.
4. **Текущий HESS v1 draft:** одна общая адресная SPI/QPI-шина, CPU — арбитр, любой совместимый модуль — временный master, любой выбранный другой модуль — slave; автономные Video/Audio и другие сопроцессоры.

## Инварианты

- Каждая плата имеет собственную RAM и CPU, никаких общих кадров/сэмплов по шине.
- Файловая система у CPU, Storage raw LBA, CPU контролирует прямые передачи ресурсов.
- Один физический владелец CLK в каждый момент, аппаратный Hi-Z; раздельный дешифратор владельца и адресата.
- Витые пары в backplane, МГТФ внутри модулей, сначала низкие частоты и измерения.

## Источники по технике

- [ESP32-S3 GPIO](https://docs.espressif.com/projects/esp-idf/en/stable/esp32s3/api-reference/peripherals/gpio.html)
- [ESP32-S3 SPI Slave](https://docs.espressif.com/projects/esp-idf/en/stable/esp32s3/api-reference/peripherals/spi_slave.html)
- [ESP32-S3 SPI Master](https://docs.espressif.com/projects/esp-idf/en/stable/esp32s3/api-reference/peripherals/spi_master.html)
- [ESP32-S3 SPI Slave Half-Duplex](https://docs.espressif.com/projects/esp-idf/en/stable/esp32s3/api-reference/peripherals/spi_slave_hd.html)
- [ESP32-S3 SDMMC Host](https://docs.espressif.com/projects/esp-idf/en/stable/esp32s3/api-reference/peripherals/sdmmc_host.html)
- [Espressif Hardware Design Guidelines](https://docs.espressif.com/projects/esp-hardware-design-guidelines/en/latest/esp32s3/schematic-checklist.html)

**Замечание:** в документации Espressif версий для ESP32 vs ESP32-S3 есть существенные различия pin map SDMMC. Используйте именно раздел ESP32-S3 выбранной ESP-IDF версии, а не найденный pinout для ESP32.