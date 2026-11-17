# Архитектура прошивок и API устройств

## Общие требования

- ESP-IDF (конкретную версию **зафиксировать** для M1), SDK и pinmap раздельно по платам.
- Общий HESS serializer/parser, CRC16 golden vectors и тесты битых пакетов.
- Один thread/task владеет каждой SPI-периферией; очередь запросов.
- На модуле: `transport`, `commands`, `resources`, `events`, `health`, `watchdog`.
- Возврат `BUSY/NOT_READY` вместо старта SPI-пакета до готовности target DMA.
- Тайм-ауты, счётчики ошибок, reset reason, просадка питания, power-on self-test.
- При reset GPIO/буферы безопасны; не включать master OE программно в обход grant.

## Унифицированный интерфейс устройства

```text
PING()
GET_DESCRIPTOR() -> (type, protocol_range, features, limits, slots)
GET_STATUS() -> state, errors, active_transfer
POLL_EVENTS(since_seq) -> events[]
PREPARE_TRANSFER(id, peer, direction, limit, descriptor)
TRANSFER_STATUS(id) -> NEW/PREPARED/ACTIVE/DONE/FAILED
CANCEL_TRANSFER(id)
```

Video, Audio, Storage, HID добавляют свои подкоманды и локальные очереди. Объекты адресуются по handle, а не указателям. CPU может без перезагрузки приложения узнавать возможности устройства, но горячая замена физической платы не поддерживается.

## Пример использования из приложения

```c
// Высокоуровневый интерфейс ОС, не физический wire format.
sprite_t hero = video_load_sprite("/game/hero.spr"); // CPU организует transfer
video_wait_ready(hero);
video_draw_sprite(hero, 100, 80);
audio_play(sound_jump);
video_present();
```

## Структура будущего кода

```text
firmware/
  common/hess/             # serializers, CRC, timeouts
  cpu/                     # arbitration, VFS, shell, ELF
  storage/                 # raw SDMMC, extent transfer
  video/                   # scanout, command queue, renderer
  audio/                   # I2S, mixer, player
  hid/                     # PS/2 events
  tests/                   # loopback, fault injection
```

Здесь описана **планируемая структура**, не притворяйтесь, что исходники уже готовы.

## Пять одновременно активных модулей

До отметки FULL PASS системный scheduler должен обслуживать CPU, Storage, Video, Sound и Input одновременно, вести счётчики HESS requests/owner, CRC errors, Input FIFO overflow/seq, Video frame latency/drop, Sound underrun и device resets. Не выполнять `spi_slave_transmit()` с предположением, что peer всегда готов: применять заранее queued DMA transaction и ready-handshake по официальному драйверу Espressif.

CPU должен **пакетировать команды одного видеокадра**, ввод должен читаться пачками, peer grants — быть ограниченными по времени. Логика приложения может синхронно ждать ресурс до старта игрового процесса, но игровая демонстрация с фоновой загрузкой обязана регулярно возвращать bus CPU. [Критерии](../acceptance-five-modules.md).