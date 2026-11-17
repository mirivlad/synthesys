# Испытания, трассировка, условия приемки

Пока **нет опубликованных измерений** — здесь тест-план. Отмечать PASS только после выполнения. **Обязательный финальный критерий — пять одновременно активных карт**, см. [Synthesys-5 приёмка](acceptance-five-modules.md).

## Оборудование

Пара/тройка ESP32-S3 N16R8, токограниченный PSU 5V, мультиметр, осциллограф (желательно ≥100 МГц), логический анализатор со SPI декодером, тестовая SD-карта, доступный модуль-переходник.

## Проверка слоёв

| ID | Тест | Что записать |
|---|---|---|
| PWR-01 | 3,3 В и 5 В при reset/SD/audio/video | токи, просадки, температура |
| PWR-02 | USB VBUS при общем питании | обратный ток / отсутствие |
| ELEC-01 | Hi-Z после reset, off module, watchdog | осциллограммы CLK/D0/MISO/XFER |
| ELEC-02 | Грант CPU → peer и обратно | OE non-overlap/guard time |
| ELEC-03 | OWNER==TARGET | отказ арбитра, CS не активен |
| PHY-01 | SPI1 100 кГц / 1 / 4 / 8 МГц | скорость, CRC, error rate |
| PHY-02 | 2→3→5→8 слотов, витая пара/МГТФ | ringing, setup/hold, packet loss |
| PROTO-01 | CRC-16/CCITT-FALSE golden `123456789` | 0x29B1 |
| PROTO-02 | length/seq/CRC corruption | NACK и отсутствие out-of-bounds |
| FS-01 | FAT32 fragmented file | экстенты, корректные байты/последний сектор |
| XFER-01 | Storage → test-peer (CPU data copy отсутствует) | 64 КиБ, CRC readback |
| XFER-02 | Peer timeout/power reset | recovery без повреждений |
| VIDEO-01 | автономная RGB/VGA генерация | синхронизация, изображение |
| VIDEO-02 | рендер при HESS transfer | нет dropouts, контролируемый FPS |
| AUDIO-01 | I2S воспроизведение и микширование | underruns, шум/джиттер |
| SYS-01 | SD отсутствует, Video отсутствует | rescue shell работоспособен |
| ABI-01 | ELF sample, handles, resource release | проверка ABI/памяти |

## Для M1

Между двумя платами `10 000 PING` с 0 ошибками CRC и reset, но **нулевая ошибка в тесте не доказывает надёжность на любом времени**. Отправить и специально ошибочные длины, CRC, seq, burst back-to-back с готовностью slave.

## Для M3

Установить физические шунты/пробники на выходах буферов. Проверить, что:
1. CPU CLK OE выключен **до** включения Video CLK OE.
2. У остальных модулей выходы Hi-Z.
3. При assert `XFER` выбирается ровно один slave.
4. После отмены/неисправности CLK и data не застревают в активном состоянии.
5. Отдельно тестируется сценарий CPU reset *в момент peer transfer*.

## Отчёт

Ввести таблицу `docs/test-results/` с датой, ревизией платы, длинами жгута, схемой OE, версией ESP-IDF, логикой DMA, частотой, raw логом, захватами анализатора, уровнем питания и выводом PASS/FAIL. Никаких вымышленных результатов.

## Системная приёмка: пять модулей

Только конфигурация **CPU + Storage + Video + Sound + Input** допускает статус `FULL_PASS`:

| Test ID | Нагрузочный сценарий | Условие |
|---|---|---|
| SYS5-01 | Все пять обнаружены, boot SD/FAT32, игра активна | все READY, корректный reset/recovery |
| SYS5-02 | 256×192, 16 движущихся спрайтов, scroll + HUD | ≥50 логических кадров/с; p99 ≤25 мс; ≤0,1% пропусков/30 мин |
| SYS5-03 | Video scanout при Storage→Video transfer | стабильная синхронизация ≥50 Гц, 0 corruption |
| SYS5-04 | Sound четыре голоса и Storage→Sound transfer | 22,05 kHz 16 bit mono (или лучше), 0 I2S underrun |
| SYS5-05 | Input PS/2 или USB; keydown/keyup при bulk transfer | p95 реакция ≤50 мс, p99 ≤80 мс, 0 неучтённых потерь |
| SYS5-06 | 5 модулей, массовые командные burst и фоновые блоки | 0 электрических конфликтов OE, 0 неразрешённых bus deadlock; command p99 wait-to-service ≤5 мс |
| SYS5-07 | Continuous direct transfer 64KiB на Video и Sound | CRC совпадает, полезная пропускная способность ≥100KiB/s без конкуренции |
| SYS5-08 | 60 мин одновременно gameplay, Sound, Input, Storage loads | 0 reset, 0 underrun, 0 lost input, 0 CRC corruption |
| SYS5-09 | 24 часа burn-in с повторением полной нагрузки | 0 irrecoverable deadlock, 0 corruption/overheating, стабильное питание |

В тесте SYS5-06 обязательно фиксировать waveform: один grant — один владелец, break-before-make, безопасный reset, `ATTN_N` и CPU reclaim. Hardware fault injection: peer завис, Input flood, Video продолжает scanout, Storage missing SD. Не считать `ATTN_N` равнозначным мгновенной доступности HESS: для пяти плат scheduler отдаёт её CPU между короткими сегментами.

**Расчёт raw SPI без накладных расходов:** 256 bytes × 8 / 4MHz ≈0,512ms; при 1MHz ≈2,048ms. Для проектного лимита ≤2ms выбирать меньшие сегменты на малой частоте и учитывать CRC, cs, DMA setup. Проверить *реальные*, а не математические p95/p99.

Первый FULL_PASS нельзя присваивать при невыполнении любой обязательной SYS5-проверки. Для остальных уровней можно отмечать COMPONENT_PASS с перечнем пройденных тестов.