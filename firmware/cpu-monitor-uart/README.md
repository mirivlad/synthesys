# CPU P0 UART0 recovery monitor — с питанием через HESS

**Исходники подготовлены, но аппаратно не тестировались и в текущей среде ESP-IDF не установлен.**

БП +5 В → backplane → HESS-40 (+5 В contacts 1/2/5/6) → защита F2 → ESP32-S3 DevKitC-1 **U1.J1.21 5V**. U1.J1.22 — общая GND. Буфер U2=74HC125D всегда в Hi-Z на P0, CPU GPIO13 BUS_ENABLE LOW.

Для терминала/прошивки ПК нужен **отдельный USB–UART TTL с выходами 3,3 В**, не соединять +5 V и 3V3 адаптера с CPU:

| CPU ESP32-S3 DevKitC-1 | USB–UART adapter |
|---|---|
| GPIO43 TX0 / J3.2 | RX |
| GPIO44 RX0 / J3.3 | TX |
| GND / J1.22 | GND |
| +5 V / 3V3 | **НЕ подключать** |

Перед UART-подключением убедитесь, что CPU питается от HESS, а TX USB–UART действительно 3,3 В. Сначала включить backplane БП с ограничением тока, затем подключить адаптер. Не питать отключённую ESP32 через UART GPIO.

```sh
cd firmware/cpu-monitor-uart
idf.py set-target esp32s3
idf.py build
# Перевести ESP32-S3 в ROM bootloader: держать BOOT, нажать RESET, отпустить BOOT.
# Указать последовательный USB–UART порт (пример /dev/ttyUSB0)
idf.py -p /dev/ttyUSB0 flash
picocom -b 115200 /dev/ttyUSB0
```

Команды аналогичны [первоначальному USB-монитору](../cpu-monitor/README.md): `help`, `status`, `mem`, `uptime`, `key tap up`, `input`, `boot`. Начальные ESP-IDF ROM-логи на UART0 могут появляться отдельно. USB Serial/JTAG и внешний UART0 — **разные** интерфейсы.

**Не считать это уже испытанной прошивкой.** Схема питания [09-backplane-powered-p0.md](../../docs/hardware/09-backplane-powered-p0.md) обязательна к чтению до пайки и подключения.
