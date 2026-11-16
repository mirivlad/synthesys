# HESS v1 — протокол кадров (DRAFT)

Документ описывает новый формат с единым адресным транспортом. Он **не бинарно совместим** с историческим `[0xAA][Dest][CMD]...` или HESS-C v3.

## Логический пакет

```text
offset size  field
0      1     magic = 0xA5
1      1     version = 0x01
2      1     type
3      1     flags
4      2     seq_le
6      2     command_le
8      2     payload_len_le
10     2     source_id_le
12     2     dest_id_le
14     4     transfer_id_le
18     N     payload
18+N   2     crc16_ccitt_false_le
```

Все multibyte-поля little-endian. CRC-16/CCITT-FALSE: poly 0x1021, init 0xFFFF, refin/refout=false, xorout=0, проверочный вектор `123456789` → `0x29B1`. CRC покрывает все байты от magic до конца payload. Размер заголовка = 18 байт, хвост = 2. Нельзя отправлять `struct` без явного сериализатора.

## Типы

| type | Семантика |
|---|---|
| 1 | REQUEST |
| 2 | RESPONSE |
| 3 | EVENT |
| 4 | DATA |
| 5 | ACK |
| 6 | NACK |

`seq` идентифицирует транзакцию в пределах сессии, `transfer_id` — долгую операцию. `status` в первом `u16` ответа: OK=0, INVALID_CMD=1, BAD_ARGUMENT=2, BUSY=3, NOT_READY=4, CRC_ERROR=5, IO_ERROR=6, TIMEOUT=7, VERSION_MISMATCH=8, NO_DEVICE=9, NO_SPACE=10, ABORTED=11, UNCERTAIN=12.

## Размеры и целостность

- `max_payload` объявляет модуль; для M1 рекомендовано 128 байт, для direct data M4 — сначала 512 байт/сегмент (с DMA-выравниванием).
- Каждый сегмент имеет `transfer_id`, `offset`, `length`, `CRC16`; целостность **всего ресурса** подтверждается CRC32/хэшем или метаданными приложения; не считать CRC16 защитой от злонамеренного кода.
- `GET_CAPABILITIES` сообщает supported commands, max frame, PHY speed, возможности master/slave и версии.
- Дубликаты с одинаковыми `seq/transfer_id` не должны дважды выполнять PLAY, WRITE или START.
- Если CPU не владеет шиной, `EVENT` физически приходит после возврата владельца; до этого `ATTN_N` лишь сигнализирует необходимость обслуживания.

## Команды (проектные пространства)

| Код | Имя |
|---|---|
| 0x0001 | PING |
| 0x0002 | GET_DESCRIPTOR |
| 0x0003 | GET_STATUS |
| 0x0004 | POLL_EVENTS |
| 0x0010 | PREPARE_TRANSFER |
| 0x0011 | TRANSFER_STATUS |
| 0x0012 | CANCEL_TRANSFER |
| 0x0013 | ABORT_TRANSFER |
| 0x1001 | STORAGE_GET_GEOMETRY |
| 0x1002 | STORAGE_READ_BLOCKS |
| 0x1003 | STORAGE_WRITE_BLOCKS |
| 0x1004 | STORAGE_PREPARE_EXTENTS |
| 0x2001 | VIDEO_SET_MODE |
| 0x2002 | VIDEO_DRAW_TEXT |
| 0x2003 | VIDEO_DRAW_PRIMITIVE |
| 0x2004 | VIDEO_SPRITE |
| 0x2005 | VIDEO_PRESENT |
| 0x3001 | AUDIO_PLAY |
| 0x3002 | AUDIO_STOP |
| 0x3003 | AUDIO_SET_VOLUME |
| 0x4001 | HID_GET_EVENTS |

## Почему отдельные source_id и dest_id

Дешифратор выбирает **физический** TARGET; логический `dest_id` может означать одну из нескольких функций внутри него. CPU сопоставляет logical device ID ↔ slot ID и проверяет, что пакет не адресован другому физическому слоту. Протокол не предоставляет аутентификации для внешнего недоверенного доступа.

## Выравнивание

При SPI Slave DMA длины RX/TX выравнивать по требованиям выбранной версии ESP-IDF; padding байты не включать в `payload_len` и CRC. Нельзя отправлять следующий блок, не удостоверившись, что target успел поставить DMA buffer в очередь.