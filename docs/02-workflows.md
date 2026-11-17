# Сценарии обмена

## A. Загрузка программы

1. CPU обнаруживает Storage, Video, Audio, HID; проверяет версии и capabilities.
2. CPU считывает с Storage raw-секторы, монтирует FAT32 и читает `boot.ini`.
3. CPU загружает ELF в собственную доступную память, проверяет ABI/размеры/сегменты.
4. CPU разбирает список ресурсов приложения: графика, звук, другие assets.
5. Для каждого ресурса CPU строит список экстентов FAT32 (LBA + count), передаёт длину и дескриптор операции Storage и модулю-получателю.
6. CPU организует допустимую передачу блоков, при необходимости ожидает сигнал готовности.
7. После `RESOURCE_READY` запускает пользовательский код; выдает ему handles, а не физические адреса чужой PSRAM.

## B. Video загружает спрайт напрямую из Storage

```text
CPU → Video:  ALLOC_RESOURCE(kind=sprite, size=N, transfer_id=42)
CPU → Storage: PREPARE_READ(42, extents[], bytes=N)
CPU → Video:  PREPARE_RECEIVE(42, target_handle)
CPU:          проверяет READY обеих сторон
CPU:          OWNER=Video, TARGET=Storage; GRANT(42)
Video [master] → Storage [slave]: READ(42, offset, len)
Storage → Video: DATA сегментами с CRC/счётчиками
Video:         помещает содержимое в PSRAM, проверяет формат и размер
Video:         переводит интерфейс в Hi-Z, освобождает шину, ATTN_N
CPU:           отзывает грант, опрашивает обоих, фиксирует COMPLETE
CPU → Video:   DRAW_SPRITE(handle, x, y)
```

Здесь **Video — SPI-master, Storage — SPI-slave**, но направление полезных данных противоположно направлению тактирования; в режиме 1-bit данные Storage идут по MISO. Video должен формировать такты чтения. Storage предварительно поставил в очередь транзакции.

## C. Audio

CPU аналогично инициирует загрузку сэмпла или композиции в локальные буферы Audio. После готовности достаточно `PLAY(sample_id)`, `STOP`, `VOLUME`, `MUSIC_SET`. ESP32-S3 Audio выдаёт I2S самостоятельно. При проигрывании старых ресурсов новая загрузка не обязана останавливать Audio, если есть буферный резерв и CPU не перезапускает модуль.

## D. Произвольные будущие модули

Compute хочет записать результат в Storage: CPU проверяет направление и полномочия, резервирует диапазон/операцию, подготавливает Storage, выдаёт грант Compute → Storage. Все операции имеют идентификатор и безопасные границы данных. Запретить произвольную запись в LBA без явного разрешения CPU.

## E. Ошибка / выключенная карта / зависший peer

CPU следит за тайм-аутом и ATTN_N вне зависимости от владения HESS. При превышении дедлайна отключает мастерские драйверы через аппаратный grant gate, возвращает HESS в idle, маркирует `transfer_id` как `ABORTED/UNCERTAIN`, запрашивает состояние. Не повторять запись без контроля commit/idempotency. При исчезновении SD Storage сообщает IO_ERROR, а не зависает.

## F. Fallback двух плат

CPU ↔ Storage без bus mastering: CPU мастер, Storage slave. Для теста чтения FAT32 достаточно стандартного SPI, фреймов и CRC. Это единственное на M1 допущение передачи raw-блоков через CPU; для нормальной прямой загрузки требуется M3.

## F. Полная работа пяти модулей

```text
CPU:   game loop 50 Hz; command buffer → Video; music events → Sound
Video: independent scanout + renderer using local PSRAM; no stream of frames from CPU
Sound: independent mixer/I2S using local buffers
Input: keyboard PS/2 OR USB host → timestamped event FIFO → ATTN_N
Storage: direct read of new tiles/sounds only after CPU prepares extents
HESS: CPU gives Video/Storage or Sound/Storage short grants, then reclaims
      bus to poll Input and issue frame/sound commands
```

A single seconds-long transfer grant is **not compatible** with low-latency Input/CPU commands. The baseline requires bounded chunks and fair service windows. [Acceptance profile](acceptance-five-modules.md).

Исторический Fallback тест из двух плат описан ниже как этап bring-up, не как полный ПК.