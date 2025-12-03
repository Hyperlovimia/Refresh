/**
 * @file wifi_manager.c
 * @brief WiFi 连接管理模块 - 实现
 */

#include "wifi_manager.h"
#include "esp_log.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "nvs_flash.h"
#include "esp_smartconfig.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "freertos/timers.h"
#include <string.h>

static const char *TAG = "WIFI_MGR";

// EventGroup 位定义
#define WIFI_CONNECTED_BIT  BIT0
#define WIFI_FAIL_BIT       BIT1
#define SC_DONE_BIT         BIT2

// WiFi 重试配置
#define WIFI_INIT_RETRY_INTERVAL_MS  5000   // 初始化阶段重试间隔 5 秒
#define WIFI_INIT_MAX_RETRY_COUNT    3      // 初始化阶段最多重试 3 次
#define WIFI_RUNTIME_RETRY_DELAY_MS  10000  // 运行时重连间隔 10 秒

// SmartConfig 配置
#define SMARTCONFIG_TIMEOUT_SEC      60     // SmartConfig 超时 60 秒

// NVS 存储键值
#define NVS_NAMESPACE   "wifi_config"
#define NVS_KEY_SSID    "ssid"
#define NVS_KEY_PASS    "password"

// 全局变量
static EventGroupHandle_t s_wifi_event_group = NULL;
static TimerHandle_t s_reconnect_timer = NULL;
static bool s_initialized = false;
static int s_retry_count = 0;
static bool s_is_runtime = false;  // 标记是否已进入运行时（初始化完成后）

/**
 * @brief WiFi 重连定时器回调（在定时器任务上下文中执行，不阻塞事件循环）
 */
static void reconnect_timer_callback(TimerHandle_t xTimer)
{
    ESP_LOGI(TAG, "执行 WiFi 重连...");
    esp_wifi_connect();
}

/**
 * @brief WiFi 和 IP 事件处理器
 */
static void wifi_event_handler(void* arg, esp_event_base_t event_base,
                               int32_t event_id, void* event_data)
{
    if (event_base == WIFI_EVENT) {
        switch (event_id) {
            case WIFI_EVENT_STA_START:
                ESP_LOGI(TAG, "WiFi 驱动启动完成");
                esp_wifi_connect();
                break;

            case WIFI_EVENT_STA_CONNECTED:
                ESP_LOGI(TAG, "WiFi 连接成功");
                s_retry_count = 0;  // 重置重试计数
                break;

            case WIFI_EVENT_STA_DISCONNECTED: {
                wifi_event_sta_disconnected_t *event = (wifi_event_sta_disconnected_t*) event_data;
                ESP_LOGW(TAG, "WiFi 连接断开，原因: %d", event->reason);

                xEventGroupClearBits(s_wifi_event_group, WIFI_CONNECTED_BIT);

                if (s_is_runtime) {
                    // 运行时：无限重试，使用定时器延迟 10 秒后重连
                    ESP_LOGI(TAG, "运行时断线，将在 %d 秒后重连...", WIFI_RUNTIME_RETRY_DELAY_MS / 1000);
                    if (s_reconnect_timer != NULL) {
                        xTimerChangePeriod(s_reconnect_timer, pdMS_TO_TICKS(WIFI_RUNTIME_RETRY_DELAY_MS), 0);
                        xTimerStart(s_reconnect_timer, 0);
                    } else {
                        // 定时器未创建，直接重连（兜底）
                        ESP_LOGW(TAG, "重连定时器未创建，立即重连");
                        esp_wifi_connect();
                    }
                } else {
                    // 初始化阶段：最多重试 3 次，使用定时器延迟 5 秒后重连
                    if (s_retry_count < WIFI_INIT_MAX_RETRY_COUNT) {
                        s_retry_count++;
                        ESP_LOGI(TAG, "初始化阶段连接失败，重试 %d/%d（%d 秒后）",
                                s_retry_count, WIFI_INIT_MAX_RETRY_COUNT,
                                WIFI_INIT_RETRY_INTERVAL_MS / 1000);
                        if (s_reconnect_timer != NULL) {
                            xTimerChangePeriod(s_reconnect_timer, pdMS_TO_TICKS(WIFI_INIT_RETRY_INTERVAL_MS), 0);
                            xTimerStart(s_reconnect_timer, 0);
                        } else {
                            // 定时器未创建，直接重连（兜底）
                            ESP_LOGW(TAG, "重连定时器未创建，立即重连");
                            esp_wifi_connect();
                        }
                    } else {
                        ESP_LOGE(TAG, "初始化阶段连接失败，已达最大重试次数");
                        xEventGroupSetBits(s_wifi_event_group, WIFI_FAIL_BIT);
                    }
                }
                break;
            }

            default:
                break;
        }
    } else if (event_base == IP_EVENT) {
        if (event_id == IP_EVENT_STA_GOT_IP) {
            ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
            ESP_LOGI(TAG, "获得 IP 地址: " IPSTR, IP2STR(&event->ip_info.ip));
            xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
        }
    } else if (event_base == SC_EVENT) {
        switch (event_id) {
            case SC_EVENT_GOT_SSID_PSWD: {
                ESP_LOGI(TAG, "SmartConfig 接收到 SSID 和密码");
                smartconfig_event_got_ssid_pswd_t *evt = (smartconfig_event_got_ssid_pswd_t *)event_data;

                // 保存 SSID 和密码到 NVS
                nvs_handle_t nvs_handle;
                esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &nvs_handle);
                if (err == ESP_OK) {
                    nvs_set_str(nvs_handle, NVS_KEY_SSID, (char*)evt->ssid);
                    nvs_set_str(nvs_handle, NVS_KEY_PASS, (char*)evt->password);
                    nvs_commit(nvs_handle);
                    nvs_close(nvs_handle);
                    ESP_LOGI(TAG, "WiFi 配置已保存到 NVS");
                } else {
                    ESP_LOGE(TAG, "无法打开 NVS 保存配置: %s", esp_err_to_name(err));
                }

                // 配置 WiFi 并连接
                wifi_config_t wifi_config = {0};
                memcpy(wifi_config.sta.ssid, evt->ssid, sizeof(wifi_config.sta.ssid));
                memcpy(wifi_config.sta.password, evt->password, sizeof(wifi_config.sta.password));

                esp_wifi_disconnect();
                esp_wifi_set_config(WIFI_IF_STA, &wifi_config);
                esp_wifi_connect();
                break;
            }

            case SC_EVENT_SEND_ACK_DONE:
                ESP_LOGI(TAG, "SmartConfig 配网完成");
                xEventGroupSetBits(s_wifi_event_group, SC_DONE_BIT);
                break;

            default:
                break;
        }
    }
}

/**
 * @brief 从 NVS 读取 WiFi 配置
 * @param ssid 输出 SSID
 * @param password 输出密码
 * @return ESP_OK 成功，ESP_FAIL 失败（无配置）
 */
static esp_err_t read_wifi_config_from_nvs(char *ssid, char *password)
{
    nvs_handle_t nvs_handle;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READONLY, &nvs_handle);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "未找到 WiFi 配置（NVS namespace 不存在）");
        return ESP_FAIL;
    }

    size_t ssid_len = 32;
    size_t pass_len = 64;

    err = nvs_get_str(nvs_handle, NVS_KEY_SSID, ssid, &ssid_len);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "NVS 中无 SSID 配置");
        nvs_close(nvs_handle);
        return ESP_FAIL;
    }

    err = nvs_get_str(nvs_handle, NVS_KEY_PASS, password, &pass_len);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "NVS 中无密码配置");
        nvs_close(nvs_handle);
        return ESP_FAIL;
    }

    nvs_close(nvs_handle);
    ESP_LOGI(TAG, "从 NVS 读取 WiFi 配置: SSID=%s", ssid);
    return ESP_OK;
}

esp_err_t wifi_manager_init(void)
{
    if (s_initialized) {
        ESP_LOGW(TAG, "WiFi 管理器已初始化，跳过");
        return ESP_OK;
    }

    ESP_LOGI(TAG, "初始化 WiFi 管理器");

    // 1. 初始化 NVS
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_LOGW(TAG, "NVS 分区需要擦除，正在重新初始化...");
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    // 2. 初始化 netif
    ESP_ERROR_CHECK(esp_netif_init());

    // 3. 创建默认事件循环
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    // 4. 创建默认 WiFi STA netif
    esp_netif_create_default_wifi_sta();

    // 5. 初始化 WiFi 驱动
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    // 6. 创建 EventGroup
    s_wifi_event_group = xEventGroupCreate();
    if (s_wifi_event_group == NULL) {
        ESP_LOGE(TAG, "创建 EventGroup 失败");
        return ESP_FAIL;
    }

    // 7. 注册事件处理器
    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &wifi_event_handler, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(SC_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL));

    // 8. 配置 WiFi 为 STA 模式
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));

    // 9. 创建重连定时器（单次触发）
    s_reconnect_timer = xTimerCreate("wifi_reconnect",
                                     pdMS_TO_TICKS(WIFI_INIT_RETRY_INTERVAL_MS),
                                     pdFALSE,  // 单次触发
                                     NULL,
                                     reconnect_timer_callback);
    if (s_reconnect_timer == NULL) {
        ESP_LOGE(TAG, "创建重连定时器失败");
        return ESP_FAIL;
    }

    // 10. 尝试从 NVS 读取配置
    char ssid[32] = {0};
    char password[64] = {0};
    wifi_config_t wifi_config = {0};
    bool has_config = false;

    if (read_wifi_config_from_nvs(ssid, password) == ESP_OK) {
        // 有保存的配置，使用它连接
        ESP_LOGI(TAG, "使用 NVS 中的 WiFi 配置");
        strlcpy((char*)wifi_config.sta.ssid, ssid, sizeof(wifi_config.sta.ssid));
        strlcpy((char*)wifi_config.sta.password, password, sizeof(wifi_config.sta.password));
        has_config = true;
    } else {
        // NVS 无配置，尝试从 menuconfig 读取
#ifdef CONFIG_WIFI_SSID
        const char *config_ssid = CONFIG_WIFI_SSID;
        const char *config_password = CONFIG_WIFI_PASSWORD;

        if (strlen(config_ssid) > 0) {
            ESP_LOGI(TAG, "使用 menuconfig 中的 WiFi 配置");
            strlcpy((char*)wifi_config.sta.ssid, config_ssid, sizeof(wifi_config.sta.ssid));
            strlcpy((char*)wifi_config.sta.password, config_password, sizeof(wifi_config.sta.password));
            has_config = true;
        } else {
            ESP_LOGW(TAG, "menuconfig 中 SSID 为空");
        }
#else
        ESP_LOGW(TAG, "menuconfig 未配置 WiFi SSID");
#endif
    }

    if (has_config) {
        // 有配置（NVS 或 menuconfig），尝试连接
        ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
        ESP_ERROR_CHECK(esp_wifi_start());

        // 等待连接结果（最多 WIFI_INIT_MAX_RETRY_COUNT * WIFI_INIT_RETRY_INTERVAL_MS）
        EventBits_t bits = xEventGroupWaitBits(s_wifi_event_group,
                                               WIFI_CONNECTED_BIT | WIFI_FAIL_BIT,
                                               pdFALSE,
                                               pdFALSE,
                                               portMAX_DELAY);

        if (bits & WIFI_CONNECTED_BIT) {
            ESP_LOGI(TAG, "WiFi 连接成功");
            s_is_runtime = true;  // 进入运行时模式
            s_initialized = true;
            return ESP_OK;
        } else if (bits & WIFI_FAIL_BIT) {
            ESP_LOGE(TAG, "WiFi 连接失败");
            s_is_runtime = false;
            s_initialized = true;  // 标记为已初始化，但连接失败
            return ESP_FAIL;
        }
    } else {
        // 无任何配置，提示用户配网
        ESP_LOGW(TAG, "未找到 WiFi 配置（NVS 和 menuconfig 均无），请调用 wifi_manager_start_provisioning() 进行配网");
        ESP_ERROR_CHECK(esp_wifi_start());
        s_initialized = true;
        return ESP_OK;  // 返回 OK，但未连接
    }

    return ESP_OK;
}

esp_err_t wifi_manager_start_provisioning(void)
{
    if (!s_initialized) {
        ESP_LOGE(TAG, "WiFi 管理器未初始化");
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "启动 SmartConfig 配网流程（超时 %d 秒）", SMARTCONFIG_TIMEOUT_SEC);

    // 清除 SC_DONE_BIT
    xEventGroupClearBits(s_wifi_event_group, SC_DONE_BIT);

    // 启动 SmartConfig
    ESP_ERROR_CHECK(esp_smartconfig_set_type(SC_TYPE_ESPTOUCH));
    smartconfig_start_config_t cfg = SMARTCONFIG_START_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_smartconfig_start(&cfg));

    // 等待配网完成（阻塞模式，60 秒超时）
    EventBits_t bits = xEventGroupWaitBits(s_wifi_event_group,
                                           SC_DONE_BIT,
                                           pdTRUE,  // 清除位
                                           pdFALSE,
                                           pdMS_TO_TICKS(SMARTCONFIG_TIMEOUT_SEC * 1000));

    // 停止 SmartConfig
    esp_smartconfig_stop();

    if (bits & SC_DONE_BIT) {
        ESP_LOGI(TAG, "SmartConfig 配网成功");

        // 等待连接完成
        bits = xEventGroupWaitBits(s_wifi_event_group,
                                   WIFI_CONNECTED_BIT,
                                   pdFALSE,
                                   pdFALSE,
                                   pdMS_TO_TICKS(30000));  // 30 秒超时

        if (bits & WIFI_CONNECTED_BIT) {
            ESP_LOGI(TAG, "WiFi 连接成功");
            s_is_runtime = true;  // 进入运行时模式
            return ESP_OK;
        } else {
            ESP_LOGW(TAG, "SmartConfig 完成，但 WiFi 连接失败");
            return ESP_FAIL;
        }
    } else {
        ESP_LOGW(TAG, "SmartConfig 超时");
        return ESP_FAIL;
    }
}

bool wifi_manager_is_connected(void)
{
    if (s_wifi_event_group == NULL) {
        return false;
    }

    EventBits_t bits = xEventGroupGetBits(s_wifi_event_group);
    return (bits & WIFI_CONNECTED_BIT) != 0;
}
