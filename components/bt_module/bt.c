#include "bt.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "driver/uart.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "defines.h"
#include "ws_server.h"
#include "app_state.h"
#include <stdlib.h>
#include <string.h>

static const char *TAG = "BT_MODULE";

#define BT_UART_BUF_SIZE        1024

static void bt_uart_rx_task(void *arg);
void bt_send_raw(const char *cmd);

static bool s_bt_enabled = false;
static int s_bt_volume = 15;

void bt_init(void)
{
    // esp_log_level_set(TAG, ESP_LOG_NONE);
    
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << BT_MOULE_PIN),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE
    };

    gpio_config(&io_conf);

    // default LOW (ESP mode)
    gpio_set_level(BT_MOULE_PIN, 0);
    s_bt_enabled = false;

        uart_config_t uart_config = {
        .baud_rate = BT_UART_BAUD,
        .data_bits = UART_DATA_8_BITS,
        .parity    = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_DEFAULT,
    };

    uart_driver_install(BT_UART_NUM, BT_UART_BUF_SIZE * 2, 0, 0, NULL, 0);
    uart_param_config(BT_UART_NUM, &uart_config);
    uart_set_pin(BT_UART_NUM, BT_MODULE_TX_PIN, BT_MODULE_RX_PIN, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);

    xTaskCreate(bt_uart_rx_task, "bt_uart_rx_task", 4096, NULL, 5, NULL);

    ESP_LOGI(TAG, "UART initialized (TX=%d RX=%d)", BT_MODULE_TX_PIN, BT_MODULE_RX_PIN);

    ESP_LOGI(TAG, "BT MODULE initialized (GPIO %d) = LOW", BT_MOULE_PIN);

    // uint8_t test[] = { 0x02, 0x0b, 0x00 };
    // uart_write_bytes(UART_NUM_1, test, sizeof(test));
}

void bt_set_enabled(bool enabled)
{
    s_bt_enabled = enabled;

    gpio_set_level(BT_MOULE_PIN, enabled ? 1 : 0);

    // uart_write_bytes(BT_UART_NUM, "AT+STATE\r\n", 10);
    // uart_write_bytes(BT_UART_NUM, "AT+GRBM\r\n", 9);

    ESP_LOGI(TAG, "AUDIO MODE set to %s", enabled ? "BT" : "ESP");
}

bool bt_get_enabled(void)
{
    return s_bt_enabled;
}

void bt_set_volume(int volume)
{
    if(volume < 0)
        volume = 0;
    else if(volume > 100)
        volume = 100;

    s_bt_volume = volume;

    int at_vol = (volume * BT_AT_SVOL_MAX + 50) / 100;

    char cmd[32];
    snprintf(cmd, sizeof(cmd), "AT+SVOL=%d", at_vol);
    ESP_LOGI(TAG, "bt_set_volume %d%% -> AT+SVOL=%d", volume, at_vol);
    bt_send_raw(cmd);
}

int bt_get_volume()
{
    return s_bt_volume;
}

void bt_check_connection() {
    ESP_LOGI(TAG, "bt_check_connection AT+STATE");
    bt_send_raw("AT+STATE");
}

void bt_send_raw(const char *cmd)
{
    if (!cmd) return;

    uart_write_bytes(BT_UART_NUM, cmd, strlen(cmd));
    uart_write_bytes(BT_UART_NUM, "\r\n", 2);
}

// Extract value following `key` up to CR/LF or end of buffer.
// Returns true if the key was found.
static bool bt_extract_field(const char *buf, const char *key, char *out, size_t out_size)
{
    const char *p = strstr(buf, key);
    if (!p) return false;
    p += strlen(key);

    size_t i = 0;
    while (*p && *p != '\r' && *p != '\n' && i + 1 < out_size) {
        out[i++] = *p++;
    }
    out[i] = 0;
    return true;
}

void bt_parse_cmd(const char *cmd) {
    if (strstr(cmd, "BT_CN")) {
        app_state_update(&(app_state_patch_t){
            .has_bt_state  = true,
            .bt_state      = BT_CONNECTED
        });
    }
    else if (strstr(cmd, "Connected")) {
        app_state_update(&(app_state_patch_t){
            .has_bt_state   = true,
            .bt_state       = BT_CONNECTED
        });
    }
    else if (strstr(cmd, "BT_DC")) {
        app_state_update(&(app_state_patch_t){
            .has_bt_state   = true,
            .bt_state       = BT_DISCONNECTED
        });
    }
    else if (strstr(cmd, "ConnDiscoverable")) {
        app_state_update(&(app_state_patch_t){
            .has_bt_state   = true,
            .bt_state       = BT_DISCOVERABLE
        });
    }

    if (strstr(cmd, "+SRC=NONE")) {
        app_state_update(&(app_state_patch_t){
            .has_bt_title       = true, .bt_title       = "",
            .has_bt_artist      = true, .bt_artist      = "",
            .has_bt_duration_ms = true, .bt_duration_ms = 0,
            .has_bt_position_s  = true, .bt_position_s  = 0,
        });
    }

    if (strstr(cmd, "BT_PA")) {
        bt_send_raw("AT+GMETA");
        bt_send_raw("AT+SMTIMEON");
    }

    char buf[128];

    if (bt_extract_field(cmd, "+TITL=", buf, sizeof(buf))) {
        app_state_update(&(app_state_patch_t){
            .has_bt_title = true,
            .bt_title     = buf
        });
    }
    if (bt_extract_field(cmd, "+ARTS=", buf, sizeof(buf))) {
        app_state_update(&(app_state_patch_t){
            .has_bt_artist = true,
            .bt_artist     = buf
        });
    }
    if (bt_extract_field(cmd, "+PYTM=", buf, sizeof(buf))) {
        int ms = atoi(buf);
        app_state_update(&(app_state_patch_t){
            .has_bt_duration_ms = true,
            .bt_duration_ms     = ms
        });
    }
    if (bt_extract_field(cmd, "+PYPS=", buf, sizeof(buf))) {
        int s = atoi(buf);
        app_state_update(&(app_state_patch_t){
            .has_bt_position_s = true,
            .bt_position_s     = s
        });
    }
    // else if (strstr(cmd, "+VOL1=")) {
    //     int vol;
    //     if (sscanf(cmd, "+VOL1=%d", &vol) == 1) {
    //         app_state_update(&(app_state_patch_t){
    //             .has_bt_volume   = true,
    //             .bt_volume       = vol
    //         });
    //     }
    // }
}

static void bt_uart_rx_task(void *arg)
{
    uint8_t *data = (uint8_t *) malloc(BT_UART_BUF_SIZE);

    while (1) {
        int len = uart_read_bytes(BT_UART_NUM, data, BT_UART_BUF_SIZE - 1, pdMS_TO_TICKS(1000));

        if (len > 0) {
            data[len] = 0; // null terminate (for logs)

            ESP_LOGI(TAG, "UART RX (%d bytes): %s", len, (char *)data);
            ws_send_bt_log((char*)data);
            bt_parse_cmd((char*)data);
        }
    }

    free(data);
    vTaskDelete(NULL);
}