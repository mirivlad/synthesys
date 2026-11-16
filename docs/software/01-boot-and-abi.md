# Загрузка ОС, файловая система, ELF и ABI

## Последовательность

```text
power → ROM boot → CPU firmware/recovery → HESS init → module scan
      → Storage raw LBA → FAT32 on CPU → /boot.ini → shell/ELF
      → prepare graphics/sound/resources → grant direct transfers
      → READY → start app
```

CPU сначала проверяет Storage и FAT32. Storage не монтирует FAT32 — он сообщает геометрию и читает/пишет блоки.

### Файловые пути

Пути типа `/apps/demo.elf`, `/assets/font.bin` существуют только в VFS CPU. При загрузке ресурсов CPU разрезает файл на список экстентов (LBA/count) и знает последнюю неполную границу байтов. Нельзя без проверки передать LBA другого файла. При изменении файлов и записи на SD требуется блокировка VFS/extent lease.

## ELF

Запуск исполняемых ELF без перепрошивки — цель проекта. В ESP32-S3 нет полноценной виртуальной памяти и изоляции пользовательских процессов как у Linux; **исполняемый код доверенный**. ABI должен фиксировать calling convention toolchain Xtensa, память исполнения/данных, системные вызовы, версионирование, жизненный цикл и освобождение ресурсов.

Можно исследовать `elf_loader` Espressif как компонент, но не объявлять его уже встроенным и проверенным. До реализации loader системный shell и программы могут быть статически встроенными.

## Synthesis ABI v0 (предлагаемый)

- `sys_console_write`, `sys_open/read/close`, `sys_sleep`.
- `sys_video_cmd`, `sys_video_load_resource`, `sys_audio_cmd`, `sys_hid_read_event`.
- `sys_hess_status`, `sys_wait_transfer`, `sys_poll_transfer` (в будущем).
- App handles являются opaque 32-bit идентификаторами; адреса памяти и GPIO устройств не доступны как общесистемные.
- Системные API проверяют размер/версию структур, не передают сырые C-pointer через HESS.
- Процесс может быть выгружен без перезагрузки всех модулей, но гарантии fault isolation на первой версии нет.

## Пример /boot.ini

См. [examples/boot.ini](../../examples/boot.ini). Параметры обсуждаемые, парсер ещё не реализован.

## Восстановление

При отказе SD/Video можно войти в rescue monitor через USB Serial; провал одного модуля не должен приводить к бесконечной блокировке главного CPU. `boot.ini` и будущий ABI должны иметь версии.