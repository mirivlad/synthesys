#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "driver/gpio.h"
#include "driver/uart.h"
#include "esp_chip_info.h"
#include "esp_err.h"
#include "esp_heap_caps.h"
#include "esp_system.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

/* Synthesys CPU P0 monitor.
 * Deliberately NO SPI / HESS bus driver: it has not passed hardware bring-up.
 * GPIO13 BUS_ENABLE must also have a physical pull-down on the backplane.
 */
#define HESS_BUS_ENABLE_GPIO GPIO_NUM_13
#define CONSOLE_BUF 192
#define KEY_QUEUE_SIZE 64
#define USB_RX_POLL_MS 25

typedef struct {
    uint32_t sequence;
    TickType_t tick;
    char verb[8];   /* press or release */
    char key[24];   /* name, not yet mapped to a HID usage code */
} virtual_key_t;

static virtual_key_t key_events[KEY_QUEUE_SIZE];
static size_t key_head;
static size_t key_count;
static uint32_t key_seq;
static uint32_t key_overflows;

static void term_send(const char *text)
{
    size_t len = strlen(text);
    while (len > 0) {
        int n = uart_write_bytes(UART_NUM_0, text, len);
        if (n <= 0) {
            break; /* disconnected/full TX: do not deadlock CPU */
        }
        text += n;
        len -= (size_t)n;
    }
}

static void term_printf_line(const char *label, const char *value)
{
    char tmp[200];
    snprintf(tmp, sizeof(tmp), "%s: %s\r\n", label, value);
    term_send(tmp);
}

static void print_prompt(void) { term_send("synthesys> "); }

static void safe_hess_init(void)
{
    /* Set the output latch LOW first; external 10k pull-down on pin 40
     * is mandatory regardless of firmware/reset state. */
    gpio_set_level(HESS_BUS_ENABLE_GPIO, 0);
    gpio_config_t out = {
        .pin_bit_mask = 1ULL << HESS_BUS_ENABLE_GPIO,
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    ESP_ERROR_CHECK(gpio_config(&out));
    gpio_set_level(HESS_BUS_ENABLE_GPIO, 0);

    /* Never energize CLK, MOSI, XFER on incomplete multidrop hardware. */
    const gpio_num_t disabled_lines[] = {
        GPIO_NUM_4, GPIO_NUM_5, GPIO_NUM_7,
        GPIO_NUM_8, GPIO_NUM_9, GPIO_NUM_10,
        GPIO_NUM_11, GPIO_NUM_12,
    };
    for (size_t i = 0; i < sizeof(disabled_lines) / sizeof(disabled_lines[0]); i++) {
        ESP_ERROR_CHECK(gpio_set_direction(disabled_lines[i], GPIO_MODE_INPUT));
    }
}

static void show_help(void)
{
    term_send(
        "\r\nSynthesys CPU P0 - UART0 recovery monitor\r\n"
        "  help                 this screen\r\n"
        "  version              firmware and architecture\r\n"
        "  status               CPU, HESS state and virtual input\r\n"
        "  mem                  internal RAM and PSRAM statistics\r\n"
        "  uptime               elapsed time in ticks\r\n"
        "  hess                 bus safety state (not initialized)\r\n"
        "  modules              discovery not available yet\r\n"
        "  boot                 Storage OS loader not available yet\r\n"
        "  key <name>           emulate a short press from host keyboard\r\n"
        "  key tap <name>      tap a key (including arrow up/down)\r\n"
        "  key down|up <name>   emulate a key transition\r\n"
        "  input                list & drain virtual input event queue\r\n"
        "  echo <text>          echo text for transport test\r\n"
        "  clear                ANSI terminal clear\r\n"
        "  reboot               restart CPU board\r\n"
        "Ctrl+C does not abort firmware; close monitor to disconnect.\r\n\r\n"
    );
}

static void key_push(const char *verb, const char *key)
{
    if (!key || !*key) {
        term_send("ERR: missing key name\r\n");
        return;
    }
    if (key_count >= KEY_QUEUE_SIZE) {
        key_overflows++;
        term_send("ERR: virtual input queue full (overflow counted)\r\n");
        return;
    }
    size_t dst = (key_head + key_count) % KEY_QUEUE_SIZE;
    key_events[dst].sequence = ++key_seq;
    key_events[dst].tick = xTaskGetTickCount();
    snprintf(key_events[dst].verb, sizeof(key_events[dst].verb), "%s", verb);
    snprintf(key_events[dst].key, sizeof(key_events[dst].key), "%s", key);
    key_count++;
}

static void show_input(void)
{
    char buf[140];
    snprintf(buf, sizeof(buf), "Input queue=%u overflows=%lu\r\n",
             (unsigned)key_count, (unsigned long)key_overflows);
    term_send(buf);
    while (key_count) {
        virtual_key_t *event = &key_events[key_head];
        snprintf(buf, sizeof(buf), "seq=%lu tick=%lu %s %s\r\n",
                 (unsigned long)event->sequence,
                 (unsigned long)event->tick, event->verb, event->key);
        term_send(buf);
        key_head = (key_head + 1U) % KEY_QUEUE_SIZE;
        key_count--;
    }
}

static void handle_command(char *line)
{
    char *saveptr = NULL;
    char *command = strtok_r(line, " \t", &saveptr);
    if (!command) return;

    if (!strcmp(command, "help") || !strcmp(command, "?")) {
        show_help();
    } else if (!strcmp(command, "version")) {
        term_send("Synthesys CPU P0 monitor 0.1.0; ESP-IDF UART0 115200\r\n"
                 "HESS-40 proposed; no SPI implementation yet.\r\n");
    } else if (!strcmp(command, "hess")) {
        term_send("HESS BUS_ENABLE=0 (GPIO13). All bus master drivers DISABLED.\r\n"
                 "Owner/target grant and SPI transport: NOT IMPLEMENTED.\r\n");
    } else if (!strcmp(command, "status")) {
        char buf[160];
        esp_chip_info_t chip;
        esp_chip_info(&chip);
        snprintf(buf, sizeof(buf), "CPU: ESP32-S3, %d core(s), revision %d\r\n",
                 chip.cores, chip.revision);
        term_send(buf);
        snprintf(buf, sizeof(buf),
                 "HESS: off; Storage: absent/not detected; OS: flash recovery only\r\n"
                 "Virtual key events=%u overflows=%lu\r\n",
                 (unsigned)key_count, (unsigned long)key_overflows);
        term_send(buf);
    } else if (!strcmp(command, "mem")) {
        char buf[140];
        snprintf(buf, sizeof(buf), "RAM free=%lu bytes; PSRAM free=%lu bytes\r\n",
                 (unsigned long)heap_caps_get_free_size(MALLOC_CAP_INTERNAL),
                 (unsigned long)heap_caps_get_free_size(MALLOC_CAP_SPIRAM));
        term_send(buf);
    } else if (!strcmp(command, "uptime")) {
        char buf[96];
        snprintf(buf, sizeof(buf), "uptime ticks=%lu (tick rate=%u Hz)\r\n",
                 (unsigned long)xTaskGetTickCount(), (unsigned)configTICK_RATE_HZ);
        term_send(buf);
    } else if (!strcmp(command, "modules")) {
        term_send("CPU present; HESS enumeration not implemented in P0.\r\n");
    } else if (!strcmp(command, "boot")) {
        term_send("Storage bootloader not implemented; recovery monitor remains active.\r\n");
    } else if (!strcmp(command, "input")) {
        show_input();
    } else if (!strcmp(command, "key")) {
        char *first = strtok_r(NULL, " \t", &saveptr);
        if (!first) {
            term_send("Usage: key <name> | key down|up <name>\r\n");
        } else if (!strcmp(first, "down") || !strcmp(first, "up")) {
            char *name = strtok_r(NULL, " \t", &saveptr);
            if (name) {
                key_push(!strcmp(first, "down") ? "press" : "release", name);
                term_send("OK: virtual key event queued\r\n");
            } else {
                term_send("ERR: missing key name\r\n");
            }
        } else {
            char *name = first;
            if (!strcmp(first, "tap")) name = strtok_r(NULL, " \t", &saveptr);
            if (!name || !*name) {
                term_send("ERR: missing key name\r\n");
            } else if (key_count + 2 > KEY_QUEUE_SIZE) {
                key_overflows++;
                term_send("ERR: virtual input queue full (cannot tap)\r\n");
            } else {
                key_push("press", name);
                key_push("release", name);
                term_send("OK: virtual key tap queued\r\n");
            }
        }
    } else if (!strcmp(command, "echo")) {
        const char *rest = saveptr ? saveptr : "";
        term_printf_line("echo", rest);
    } else if (!strcmp(command, "clear")) {
        term_send("\x1b[2J\x1b[H");
    } else if (!strcmp(command, "reboot")) {
        term_send("Rebooting...\r\n");
        uart_wait_tx_done(UART_NUM_0, pdMS_TO_TICKS(150));
        esp_restart();
    } else {
        term_send("ERR: unknown command; type help\r\n");
    }
}

void app_main(void)
{
    safe_hess_init();
    /* CPU is powered from HESS +5V. UART0 TX=GPIO43 RX=GPIO44;
     * 3.3V USB-UART adapter: TX/RX/GND only, never connect VCC/5V. */
    uart_config_t cfg = {
        .baud_rate = 115200,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_DEFAULT,
    };
    ESP_ERROR_CHECK(uart_driver_install(UART_NUM_0, 1024, 2048, 0, NULL, 0));
    ESP_ERROR_CHECK(uart_param_config(UART_NUM_0, &cfg));
    ESP_ERROR_CHECK(uart_set_pin(UART_NUM_0, GPIO_NUM_43, GPIO_NUM_44, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE));
    term_send("\r\nSynthesys CPU P0 recovery monitor\r\n");
    term_send("HESS output gating disabled. Type help.\r\n");
    print_prompt();

    char line[CONSOLE_BUF];
    size_t used = 0;
    for (;;) {
        char rx[64];
        int n = uart_read_bytes(UART_NUM_0, (uint8_t *)rx, sizeof(rx), pdMS_TO_TICKS(USB_RX_POLL_MS));
        for (int i = 0; i < n; i++) {
            char ch = rx[i];
            if (ch == '\r' || ch == '\n') {
                if (used) {
                    line[used] = '\0';
                    term_send("\r\n");
                    handle_command(line);
                    used = 0;
                    print_prompt();
                }
            } else if (ch == '\b' || ch == 127) {
                if (used) { used--; term_send("\b \b"); }
            } else if ((unsigned char)ch >= 32 && used + 1 < CONSOLE_BUF) {
                line[used++] = ch;
                char echo[2] = {ch, '\0'};
                term_send(echo);
            } else if (used + 1 >= CONSOLE_BUF) {
                term_send("\r\nERR: input line too long\r\n");
                used = 0;
                print_prompt();
            }
        }
    }
}
