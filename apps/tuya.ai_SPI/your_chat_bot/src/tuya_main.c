/**
 * @file tuya_main.c
 * @brief Implements main audio functionality for IoT device
 *
 * This source file provides the implementation of the main audio functionalities
 * required for an IoT device. It includes functionality for audio processing,
 * device initialization, event handling, and network communication. The
 * implementation supports audio volume control, data point processing, and
 * interaction with the Tuya IoT platform. This file is essential for developers
 * working on IoT applications that require audio capabilities and integration
 * with the Tuya IoT ecosystem.
 *
 * @copyright Copyright (c) 2021-2025 Tuya Inc. All Rights Reserved.
 *
 */

#include "tuya_cloud_types.h"

#include <assert.h>
#include <stdbool.h>
#include "cJSON.h"
#include "tal_api.h"
#include "tuya_config.h"
#include "tuya_iot.h"
#include "tuya_iot_dp.h"
#include "netmgr.h"
#include "tkl_output.h"
#include "tal_cli.h"
#include "tuya_authorize.h"
#if defined(ENABLE_WIFI) && (ENABLE_WIFI == 1)
#include "netconn_wifi.h"
#endif
#if defined(ENABLE_WIRED) && (ENABLE_WIRED == 1)
#include "netconn_wired.h"
#endif
#if defined(ENABLE_LIBLWIP) && (ENABLE_LIBLWIP == 1)
#include "lwip_init.h"
#endif

#if defined(ENABLE_CHAT_DISPLAY) && (ENABLE_CHAT_DISPLAY == 1)
#include "app_display.h"
#endif

#include "board_com_api.h"
#include "tkl_spi.h"
#include "app_chat_bot.h"
#include "ai_audio.h"
#include "reset_netcfg.h"
#include "app_system_info.h"
#include "tal_network.h"
#include "tal_queue.h"
#include "loragw_hal.h"
#include "loragw_reg.h"
#include "loragw_aux.h"
#define COM_TYPE_DEFAULT LGW_COM_SPI
#define DEFAULT_CLK_SRC     0
#define DEFAULT_FREQ_HZ     506500000U //系统默认发送频率
#define CONFIG_SPI_SLAVE 0 
#define MAX_SIZE 1024
#define MAX_NODE 57//最大节点数（可增加）
#define MSG_STOP "ENDMSG"
//故障标志，0x00为离线或故障，0x01为正常工作
#define WORK 0x01
#define FAULT 0x00

// 测试功能开关宏定义
#define ENABLE_SPI_LOOPBACK_TEST 0// 是否启用SPI回环测试
#define ENABLE_SPI_TCP_COMM_TEST 0    // 是否启用SPI-TCP通信测试
#define AUTO_RUN_TESTS_ON_STARTUP 0   // 是否在启动时自动运行测试

/* 存储接收到的数据包缓冲区*/
uint8_t max_rx_pkt = 16; //设置接收数据包的最大数量（若全开/全关会有反馈数据，若设置为16则会遗漏一些数据，因为反馈数据不做处理，在此设置为16
struct lgw_pkt_rx_s rxpkt[16];//接收数据包个数最大为16
/* Tuya device handle */
tuya_iot_client_t ai_client;

#ifndef PROJECT_VERSION
#define PROJECT_VERSION "1.0.0"
#endif
#define DPID_VOLUME 3
#define SPI_FREQ 1000
#define TCP_SERVER_IP "192.168.50.239"
#define TCP_SERVER_PORT 1234
#define TY_IPADDR_ANY ((uint32_t)0x00000000UL)
static uint8_t _need_reset = 0;
char wlan_state=0; 

// 队列和线程相关定义
#define QUEUE_MSG_SIZE 256          // 队列消息大小
#define QUEUE_DEPTH 10              // 队列深度

typedef struct {
    uint8_t data[QUEUE_MSG_SIZE];   // 数据内容
    uint16_t length;                // 数据长度
} queue_msg_t;

// 队列句柄
static QUEUE_HANDLE tcp_to_spi_queue = NULL;  // TCP到SPI的数据队列
static QUEUE_HANDLE spi_to_tcp_queue = NULL;  // SPI到TCP的数据队列

// 线程句柄
static THREAD_HANDLE spi_thread_handle = NULL; // SPI处理线程

// 线程控制标志
static volatile bool spi_thread_running = false; 

static void wifi_event_callback(WF_EVENT_E event, void *arg)
{
    OPERATE_RET op_ret = OPRT_OK;
    NW_IP_S sta_info;
    memset(&sta_info, 0, SIZEOF(NW_IP_S));

    PR_DEBUG("-------------event callback-------------");
    switch (event) {
        case WFE_CONNECTED:{
            PR_DEBUG("connection succeeded!");
            wlan_state = 1; // connected
            /* output ip information */
            op_ret = tal_wifi_get_ip(WF_STATION, &sta_info);
            if (OPRT_OK != op_ret) {
                PR_ERR("get station ip error");
                            wlan_state = 0; // disconnected

                return;
            }

#ifdef nwipstr                
            if(IS_NW_IPV4_ADDR(&sta_info)) {
                TAL_PR_NOTICE("gw: %s", sta_info.nwgwstr);
                TAL_PR_NOTICE("ip: %s", sta_info.nwipstr);
                TAL_PR_NOTICE("mask: %s", sta_info.nwmaskstr);
            }else {
                TAL_PR_NOTICE("ip: %s", sta_info.addr.ip6.ip);
                TAL_PR_NOTICE("islinklocal: %d", sta_info.addr.ip6.islinklocal);
            }
#else 
            PR_NOTICE("gw: %s", sta_info.gw);
            PR_NOTICE("ip: %s", sta_info.ip);
            PR_NOTICE("mask: %s", sta_info.mask);
            UI_WIFI_STATUS_E wifi_status;
            wifi_status = UI_WIFI_STATUS_GOOD;
            app_display_send_msg(TY_DISPLAY_TP_NETWORK, (uint8_t *)&wifi_status, sizeof(UI_WIFI_STATUS_E));
            app_display_send_msg(TY_DISPLAY_STM32_IP, (uint8_t *)&sta_info.ip, sizeof(sta_info.ip));

#endif
            break;
            wlan_state = 1; // connected
        }

        case WFE_CONNECT_FAILED: {
            PR_DEBUG("connection fail!");
            wlan_state = 0; // disconnected
            break;
        }

        case WFE_DISCONNECTED: {
            PR_DEBUG("disconnect!");
            wlan_state = 0; // disconnected

            break;        
        }
       
    }
}
/**
 * @brief user defined log output api, in this demo, it will use uart0 as log-tx
 *
 * @param str log string
 * @return void
 */
void user_log_output_cb(const char *str)
{
    tal_uart_write(TUYA_UART_NUM_0, (const uint8_t *)str, strlen(str));
}

/**
 * @brief user defined upgrade notify callback, it will notify device a OTA request received
 *
 * @param client device info
 * @param upgrade the upgrade request info
 * @return void
 */
void user_upgrade_notify_on(tuya_iot_client_t *client, cJSON *upgrade)
{
    PR_INFO("----- Upgrade information -----");
    PR_INFO("OTA Channel: %d", cJSON_GetObjectItem(upgrade, "type")->valueint);
    PR_INFO("Version: %s", cJSON_GetObjectItem(upgrade, "version")->valuestring);
    PR_INFO("Size: %s", cJSON_GetObjectItem(upgrade, "size")->valuestring);
    PR_INFO("MD5: %s", cJSON_GetObjectItem(upgrade, "md5")->valuestring);
    PR_INFO("HMAC: %s", cJSON_GetObjectItem(upgrade, "hmac")->valuestring);
    PR_INFO("URL: %s", cJSON_GetObjectItem(upgrade, "url")->valuestring);
    PR_INFO("HTTPS URL: %s", cJSON_GetObjectItem(upgrade, "httpsUrl")->valuestring);
}

OPERATE_RET audio_dp_obj_proc(dp_obj_recv_t *dpobj)
{
    uint32_t index = 0;
    for (index = 0; index < dpobj->dpscnt; index++) {
        dp_obj_t *dp = dpobj->dps + index;
        PR_DEBUG("idx:%d dpid:%d type:%d ts:%u", index, dp->id, dp->type, dp->time_stamp);

        switch (dp->id) {
        case DPID_VOLUME: {
            uint8_t volume = dp->value.dp_value;
            PR_DEBUG("volume:%d", volume);
            ai_audio_set_volume(volume);
            char volume_str[20] = {0};
#if defined(ENABLE_CHAT_DISPLAY) && (ENABLE_CHAT_DISPLAY == 1)
            snprintf(volume_str, sizeof(volume_str), "%s%d", VOLUME, volume);
            app_display_send_msg(TY_DISPLAY_TP_NOTIFICATION, (uint8_t *)volume_str, strlen(volume_str));
#endif
            break;
        }
        default:
            break;
        }
    }

    return OPRT_OK;
}

OPERATE_RET ai_audio_volume_upload(void)
{
    tuya_iot_client_t *client = tuya_iot_client_get();
    dp_obj_t dp_obj = {0};

    uint8_t volume = ai_audio_get_volume();

    dp_obj.id = DPID_VOLUME;
    dp_obj.type = PROP_VALUE;
    dp_obj.value.dp_value = volume;

    PR_DEBUG("DP upload volume:%d", volume);

    return tuya_iot_dp_obj_report(client, client->activate.devid, &dp_obj, 1, 0);
}

/**
 * @brief user defined event handler
 *
 * @param client device info
 * @param event the event info
 * @return void
 */
void user_event_handler_on(tuya_iot_client_t *client, tuya_event_msg_t *event)
{
    PR_DEBUG("Tuya Event ID:%d(%s)", event->id, EVENT_ID2STR(event->id));
    PR_INFO("Device Free heap %d", tal_system_get_free_heap_size());

    switch (event->id) {
    case TUYA_EVENT_BIND_START:
        PR_INFO("Device Bind Start!");
        if (_need_reset == 1) {
            PR_INFO("Device Reset!");
            tal_system_reset();
        }

        ai_audio_player_play_alert(AI_AUDIO_ALERT_NETWORK_CFG);
        break;

    case TUYA_EVENT_BIND_TOKEN_ON:
        break;

    /* MQTT with tuya cloud is connected, device online */
    case TUYA_EVENT_MQTT_CONNECTED:
        PR_INFO("Device MQTT Connected!");
        tal_event_publish(EVENT_MQTT_CONNECTED, NULL);

        static uint8_t first = 1;
        if (first) {
            first = 0;

#if defined(ENABLE_CHAT_DISPLAY) && (ENABLE_CHAT_DISPLAY == 1)
            UI_WIFI_STATUS_E wifi_status = UI_WIFI_STATUS_GOOD;
            app_display_send_msg(TY_DISPLAY_TP_NETWORK, (uint8_t *)&wifi_status, sizeof(UI_WIFI_STATUS_E));
#endif

            ai_audio_player_play_alert(AI_AUDIO_ALERT_NETWORK_CONNECTED);
            ai_audio_volume_upload();
        }
        break;

    /* MQTT with tuya cloud is disconnected, device offline */
    case TUYA_EVENT_MQTT_DISCONNECT:
        PR_INFO("Device MQTT DisConnected!");
        tal_event_publish(EVENT_MQTT_DISCONNECTED, NULL);
        break;

    /* RECV upgrade request */
    case TUYA_EVENT_UPGRADE_NOTIFY:
        user_upgrade_notify_on(client, event->value.asJSON);
        break;

    /* Sync time with tuya Cloud */
    case TUYA_EVENT_TIMESTAMP_SYNC:
        PR_INFO("Sync timestamp:%d", event->value.asInteger);
        tal_time_set_posix(event->value.asInteger, 1);
        break;

    case TUYA_EVENT_RESET:
        PR_INFO("Device Reset:%d", event->value.asInteger);

        _need_reset = 1;
        break;

    /* RECV OBJ DP */
    case TUYA_EVENT_DP_RECEIVE_OBJ: {
        dp_obj_recv_t *dpobj = event->value.dpobj;
        PR_DEBUG("SOC Rev DP Cmd t1:%d t2:%d CNT:%u", dpobj->cmd_tp, dpobj->dtt_tp, dpobj->dpscnt);
        if (dpobj->devid != NULL) {
            PR_DEBUG("devid.%s", dpobj->devid);
        }

        audio_dp_obj_proc(dpobj);

        tuya_iot_dp_obj_report(client, dpobj->devid, dpobj->dps, dpobj->dpscnt, 0);

    } break;

    /* RECV RAW DP */
    case TUYA_EVENT_DP_RECEIVE_RAW: {
        dp_raw_recv_t *dpraw = event->value.dpraw;
        PR_DEBUG("SOC Rev DP Cmd t1:%d t2:%d", dpraw->cmd_tp, dpraw->dtt_tp);
        if (dpraw->devid != NULL) {
            PR_DEBUG("devid.%s", dpraw->devid);
        }

        uint32_t index = 0;
        dp_raw_t *dp = &dpraw->dp;
        PR_DEBUG("dpid:%d type:RAW len:%d data:", dp->id, dp->len);
        for (index = 0; index < dp->len; index++) {
            PR_DEBUG_RAW("%02x", dp->data[index]);
        }

        tuya_iot_dp_raw_report(client, dpraw->devid, &dpraw->dp, 3);

    } break;

    default:
        break;
    }
}

/**
 * @brief user defined network check callback, it will check the network every 1sec,
 *        in this demo it alwasy return ture due to it's a wired demo
 *
 * @return true
 * @return false
 */
bool user_network_check(void)
{
    netmgr_status_e status = NETMGR_LINK_DOWN;
    netmgr_conn_get(NETCONN_AUTO, NETCONN_CMD_STATUS, &status);
    return status == NETMGR_LINK_DOWN ? false : true;
}

/**
 * @brief 初始化队列
 */
OPERATE_RET init_communication_queues(void)
{
    OPERATE_RET ret = OPRT_OK;
    
    // 创建TCP到SPI的数据队列
    if (tcp_to_spi_queue == NULL) {
        ret = tal_queue_create_init(&tcp_to_spi_queue, sizeof(queue_msg_t), QUEUE_DEPTH);
        if (ret != OPRT_OK) {
            PR_ERR("Failed to create tcp_to_spi_queue: %d", ret);
            return ret;
        }
        PR_DEBUG("tcp_to_spi_queue created successfully");
    }
    
    // 创建SPI到TCP的数据队列
    if (spi_to_tcp_queue == NULL) {
        ret = tal_queue_create_init(&spi_to_tcp_queue, sizeof(queue_msg_t), QUEUE_DEPTH);
        if (ret != OPRT_OK) {
            PR_ERR("Failed to create spi_to_tcp_queue: %d", ret);
            // 清理已创建的队列
            if (tcp_to_spi_queue) {
                tal_queue_free(tcp_to_spi_queue);
                tcp_to_spi_queue = NULL;
            }
            return ret;
        }
        PR_DEBUG("spi_to_tcp_queue created successfully");
    }
    
    PR_NOTICE("Communication queues initialized successfully");
    return OPRT_OK;
}

/**
 * @brief 释放队列资源
 */
void cleanup_communication_queues(void)
{
    if (tcp_to_spi_queue) {
        tal_queue_free(tcp_to_spi_queue);
        tcp_to_spi_queue = NULL;
        PR_DEBUG("tcp_to_spi_queue freed");
    }
    
    if (spi_to_tcp_queue) {
        tal_queue_free(spi_to_tcp_queue);
        spi_to_tcp_queue = NULL;
        PR_DEBUG("spi_to_tcp_queue freed");
    }
}

/**
 * @brief SPI处理线程
 * 负责：
 * 1. 从tcp_to_spi_queue读取数据并发送到SPI
 * 2. 从SPI读取数据并放入spi_to_tcp_queue
 */
static void spi_handler_thread(void *args)
{
    queue_msg_t msg;
    uint8_t spi_snd_buf[] = {"Hello Tuya"};
    uint8_t spi_rec_buf[256];
    int spi_len = 0;
    
    PR_NOTICE("SPI handler thread started");
    spi_thread_running = true;
    
    while (spi_thread_running) {
        // 1. 检查是否有TCP数据需要发送到SPI
        if (tal_queue_fetch(tcp_to_spi_queue, &msg, 10) == OPRT_OK) {
            PR_DEBUG("Received %d bytes from TCP queue, forwarding to SPI", msg.length);
            
            // 发送数据到SPI
            if (tkl_spi_send(TUYA_SPI_NUM_1, msg.data, msg.length) == OPRT_OK) {
                PR_DEBUG("Successfully sent %d bytes to SPI", msg.length);
            } else {
                PR_ERR("Failed to send data to SPI");
            }
        }
        
        // 2. 从SPI读取数据
        memset(spi_rec_buf, 0, sizeof(spi_rec_buf));
        if (tkl_spi_transfer(TUYA_SPI_NUM_1, spi_snd_buf, spi_rec_buf, CNTSOF(spi_snd_buf)) == OPRT_OK) {
            spi_len = strlen((char*)spi_rec_buf);
            
            // 只有当SPI返回有效数据且不是默认的"Hello Tuya"时才处理
            if (spi_len > 0 && strcmp((char*)spi_rec_buf, "Hello Tuya") != 0) {
                PR_DEBUG("Read %d bytes from SPI: %s", spi_len, spi_rec_buf);
                
                // 将SPI数据放入队列
                memset(&msg, 0, sizeof(msg));
                memcpy(msg.data, spi_rec_buf, spi_len);
                msg.length = spi_len;
                
                if (tal_queue_post(spi_to_tcp_queue, &msg, 0) == OPRT_OK) {
                    PR_DEBUG("Successfully queued %d bytes from SPI", spi_len);
                } else {
                    PR_ERR("Failed to queue SPI data - queue may be full");
                }
            }
        }
        
        // 适当延时，避免CPU占用过高
        tal_system_sleep(50);
    }
    
    PR_NOTICE("SPI handler thread exiting");
    spi_thread_handle = NULL;
}

/**
 * @brief 启动SPI处理线程
 */
OPERATE_RET start_spi_thread(void)
{
    if (spi_thread_handle != NULL) {
        PR_WARN("SPI thread is already running");
        return OPRT_OK;
    }
    
    THREAD_CFG_T thread_cfg = {
        .thrdname = "spi_handler",
        .stackDepth = 4096,
        .priority = THREAD_PRIO_2,
    };
    
    OPERATE_RET ret = tal_thread_create_and_start(&spi_thread_handle, NULL, NULL, 
                                                  spi_handler_thread, NULL, &thread_cfg);
    if (ret != OPRT_OK) {
        PR_ERR("Failed to create SPI thread: %d", ret);
        return ret;
    }
    
    PR_NOTICE("SPI handler thread started successfully");
    return OPRT_OK;
}

/**
 * @brief 停止SPI处理线程
 */
void stop_spi_thread(void)
{
    if (spi_thread_handle != NULL) {
        spi_thread_running = false;
        
        // 等待线程结束
        while (spi_thread_handle != NULL) {
            tal_system_sleep(100);
        }
        
        PR_NOTICE("SPI handler thread stopped");
    }
}

#if ENABLE_SPI_TCP_COMM_TEST || ENABLE_SPI_LOOPBACK_TEST
/**
 * @brief 完整TCP-SPI回环通信测试函数
 * 
 * 测试完整数据流路径：TCP → SPI → (硬件回环) → SPI → TCP
 * 
 * 测试流程：
 * 1. 发送数据到TCP-to-SPI队列
 * 2. SPI线程从队列读取数据并发送到SPI硬件
 * 3. SPI硬件通过回环连接返回数据
 * 4. SPI线程接收回环数据并放入SPI-to-TCP队列
 * 5. 从SPI-to-TCP队列读取数据验证完整性
 */
OPERATE_RET test_spi_tcp_communication(void)
{
    OPERATE_RET ret = OPRT_OK;
    queue_msg_t send_msg, recv_msg;
    uint8_t test_count = 0;
    uint8_t max_tests = 5;
    uint8_t successful_loops = 0;
    uint32_t start_time, end_time;
    
    PR_NOTICE("=== Starting TCP-SPI-TCP Loop Communication Test ===");
    PR_NOTICE("Test Path: TCP -> Queue -> SPI -> Hardware Loop -> SPI -> Queue -> TCP");
    
    // 检查前提条件
    if (tcp_to_spi_queue == NULL || spi_to_tcp_queue == NULL) {
        PR_ERR("Queues not initialized! Cannot run test.");
        return OPRT_COM_ERROR;
    }
    
    if (!spi_thread_running) {
        PR_ERR("SPI thread not running! Cannot run test.");
        return OPRT_COM_ERROR;
    }
    
    PR_NOTICE("Prerequisites check passed. Starting loop tests...");
    
    // 清空队列，确保测试环境干净
    while (tal_queue_fetch(spi_to_tcp_queue, &recv_msg, 0) == OPRT_OK) {
        // 清空SPI到TCP队列中的旧数据
    }
    
    start_time = tal_time_get_posix();
    
    // 执行TCP-SPI-TCP回环测试
    for (test_count = 1; test_count <= max_tests; test_count++) {
        PR_NOTICE("\n--- Loop Test %d/%d ---", test_count, max_tests);
        
        // 第1步：准备测试数据并发送到TCP-to-SPI队列
        memset(&send_msg, 0, sizeof(send_msg));
        snprintf((char*)send_msg.data, QUEUE_MSG_SIZE, "LOOP_TEST_%02d_TIME_%u", 
                test_count, (unsigned int)tal_time_get_posix());
        send_msg.length = strlen((char*)send_msg.data);
        
        PR_DEBUG("Step 1: Sending to TCP-to-SPI queue: '%s' (%d bytes)", 
                send_msg.data, send_msg.length);
        
        // 发送数据到TCP-to-SPI队列
        ret = tal_queue_post(tcp_to_spi_queue, &send_msg, 1000); // 1秒超时
        if (ret != OPRT_OK) {
            PR_ERR("Failed to post test data to tcp_to_spi_queue: %d", ret);
            continue;
        }
        
        PR_DEBUG("Step 2: Data posted to queue, waiting for SPI processing...");
        
        // 第2步：等待SPI线程处理（TCP -> SPI -> 硬件回环 -> SPI -> Queue）
        // 给SPI线程足够时间完成整个回环过程
        tal_system_sleep(1000); // 等待1秒让SPI线程完成处理
        
        // 第3步：从SPI-to-TCP队列读取回环数据
        PR_DEBUG("Step 3: Checking SPI-to-TCP queue for looped data...");
        
        uint32_t wait_time = 0;
        uint32_t max_wait_time = 3000; // 最多等待3秒
        bool data_received = false;
        
        while (wait_time < max_wait_time && !data_received) {
            ret = tal_queue_fetch(spi_to_tcp_queue, &recv_msg, 100); // 100ms超时
            if (ret == OPRT_OK) {
                data_received = true;
                recv_msg.data[recv_msg.length] = '\0'; // 确保字符串结束
                
                PR_NOTICE("Step 4: Received looped data: '%s' (%d bytes)", 
                         recv_msg.data, recv_msg.length);
                
                // 第4步：验证数据完整性
                if (recv_msg.length == send_msg.length && 
                    memcmp(send_msg.data, recv_msg.data, send_msg.length) == 0) {
                    successful_loops++;
                    PR_NOTICE("✓ Loop Test %d: SUCCESS - Data integrity verified!", test_count);
                    PR_DEBUG("  Sent:     '%s'", send_msg.data);
                    PR_DEBUG("  Received: '%s'", recv_msg.data);
                } else {
                    PR_ERR("✗ Loop Test %d: FAILED - Data mismatch!", test_count);
                    PR_ERR("  Sent:     '%s' (%d bytes)", send_msg.data, send_msg.length);
                    PR_ERR("  Received: '%s' (%d bytes)", recv_msg.data, recv_msg.length);
                    
                    // 详细的十六进制比较
                    PR_DEBUG("Sent data (hex):");
                    for (int i = 0; i < send_msg.length; i++) {
                        PR_DEBUG_RAW("%02X ", send_msg.data[i]);
                    }
                    PR_DEBUG("\nReceived data (hex):");
                    for (int i = 0; i < recv_msg.length; i++) {
                        PR_DEBUG_RAW("%02X ", recv_msg.data[i]);
                    }
                    PR_DEBUG("\n");
                }
            } else {
                tal_system_sleep(100);
                wait_time += 100;
            }
        }
        
        if (!data_received) {
            PR_ERR("✗ Loop Test %d: TIMEOUT - No data received after %u ms", 
                  test_count, max_wait_time);
        }
        
        // 测试间隔
        tal_system_sleep(500);
    }
    
    end_time = tal_time_get_posix();
    
    // 额外的队列状态测试
    PR_NOTICE("\n--- Queue Performance Test ---");
    uint8_t queue_stress_count = 3;
    
    for (int i = 1; i <= queue_stress_count; i++) {
        memset(&send_msg, 0, sizeof(send_msg));
        snprintf((char*)send_msg.data, QUEUE_MSG_SIZE, "STRESS_TEST_%02d", i);
        send_msg.length = strlen((char*)send_msg.data);
        
        ret = tal_queue_post(tcp_to_spi_queue, &send_msg, 0); // 非阻塞
        if (ret == OPRT_OK) {
            PR_DEBUG("Stress test message %d queued successfully", i);
        } else {
            PR_WARN("Stress test message %d failed to queue (queue may be full)", i);
        }
    }
    
    // 测试总结
    PR_NOTICE("\n");
    PR_NOTICE("========================================");
    PR_NOTICE("    TCP-SPI-TCP Loop Test Summary");
    PR_NOTICE("========================================");
    PR_NOTICE("Total tests executed:     %d", max_tests);
    PR_NOTICE("Successful loops:         %d", successful_loops);
    PR_NOTICE("Failed loops:             %d", max_tests - successful_loops);
    PR_NOTICE("Success rate:             %.1f%%", (float)successful_loops * 100.0 / max_tests);
    PR_NOTICE("Total test time:          %u seconds", end_time - start_time);
    PR_NOTICE("Average time per loop:    %.2f seconds", (float)(end_time - start_time) / max_tests);
    
    if (successful_loops >= (max_tests * 3 / 4)) { // 75%成功率认为测试通过
        PR_NOTICE("========================================");
        PR_NOTICE("✓ TCP-SPI-TCP Loop Test: PASSED");
        PR_NOTICE("========================================");
        return OPRT_OK;
    } else {
        PR_NOTICE("========================================");
        PR_ERR("✗ TCP-SPI-TCP Loop Test: FAILED");
        PR_ERR("Expected success rate >= 75%%, got %.1f%%", 
              (float)successful_loops * 100.0 / max_tests);
        PR_NOTICE("========================================");
        return OPRT_COM_ERROR;
    }
}
#endif // ENABLE_SPI_TCP_COMM_TEST

#if ENABLE_SPI_LOOPBACK_TEST
/**
 * @brief SPI回环测试函数
 * 
 * 测试SPI MISO和MOSI的连接，发送数据并验证接收
 */
OPERATE_RET test_spi_loopback(void)
{
    OPERATE_RET ret = OPRT_OK;
    uint8_t test_data[] = "SPI_LOOPBACK_TEST";
    uint8_t recv_data[32];
    uint8_t test_count = 5;
    uint8_t success_count = 0;
    
    PR_NOTICE("=== Starting SPI Loopback Test ===");
    
    for (int i = 1; i <= test_count; i++) {
        // 准备测试数据
        memset(recv_data, 0, sizeof(recv_data));
        snprintf((char*)test_data, sizeof(test_data), "LOOP_TEST_%02d", i);
        
        PR_DEBUG("SPI Loopback Test %d: Sending '%s'", i, test_data);
        
        // 执行SPI传输
        ret = tkl_spi_transfer(TUYA_SPI_NUM_1, test_data, recv_data, strlen((char*)test_data));
        if (ret == OPRT_OK) {
            // 验证接收数据
            recv_data[strlen((char*)test_data)] = '\0'; // 确保字符串结束
            
            if (strcmp((char*)test_data, (char*)recv_data) == 0) {
                success_count++;
                PR_NOTICE("SPI Loopback Test %d: SUCCESS - Sent: '%s', Received: '%s'", 
                         i, test_data, recv_data);
            } else {
                PR_ERR("SPI Loopback Test %d: MISMATCH - Sent: '%s', Received: '%s'", 
                      i, test_data, recv_data);
                
                // 打印十六进制数据用于调试
                PR_DEBUG("Sent data (hex):");
                for (int j = 0; j < strlen((char*)test_data); j++) {
                    PR_DEBUG_RAW("%02X ", test_data[j]);
                }
                PR_DEBUG("Received data (hex):");
                for (int j = 0; j < strlen((char*)test_data); j++) {
                    PR_DEBUG_RAW("%02X ", recv_data[j]);
                }
            }
        } else {
            PR_ERR("SPI Loopback Test %d: SPI transfer failed with error %d", i, ret);
        }
        
        tal_system_sleep(200); // 延时200ms
    }
    
    PR_NOTICE("=== SPI Loopback Test Summary ===");
    PR_NOTICE("Total tests: %d, Successful: %d, Failed: %d", 
             test_count, success_count, test_count - success_count);
    
    if (success_count == test_count) {
        PR_NOTICE("=== SPI Loopback Test PASSED ===");
        return OPRT_OK;
    } else {
        PR_ERR("=== SPI Loopback Test FAILED ===");
        return OPRT_COM_ERROR;
    }
}
#endif // ENABLE_SPI_LOOPBACK_TEST

#if ENABLE_SPI_LOOPBACK_TEST || ENABLE_SPI_TCP_COMM_TEST
/**
 * @brief 综合测试函数 - 运行所有通信测试
 * 
 * 测试包括：
 * 1. SPI硬件回环测试 - 验证SPI MISO/MOSI连接
 * 2. TCP-SPI-TCP完整回环测试 - 验证完整通信链路
 */
OPERATE_RET run_communication_tests(void)
{
    OPERATE_RET ret = OPRT_OK;
    OPERATE_RET overall_result = OPRT_OK;
    
    PR_NOTICE("\n");
    PR_NOTICE("########################################");
    PR_NOTICE("#     COMMUNICATION SYSTEM TESTS      #");
    PR_NOTICE("#                                      #");
    PR_NOTICE("# Test 1: SPI Hardware Loopback       #");
    PR_NOTICE("# Test 2: TCP-SPI-TCP Full Loop       #");
    PR_NOTICE("########################################");
    
#if ENABLE_SPI_LOOPBACK_TEST
    // 测试1: SPI硬件回环测试
    PR_NOTICE("\n[1/2] Running SPI Hardware Loopback Test...");
    PR_NOTICE("This test verifies SPI MISO-MOSI hardware connection");
    ret = test_spi_loopback();
    if (ret != OPRT_OK) {
        PR_ERR("SPI Hardware Loopback Test failed!");
        overall_result = ret;
    }
    
    // 等待一段时间
    tal_system_sleep(1000);
#else
    PR_NOTICE("\n[1/2] SPI Hardware Loopback Test: DISABLED");
#endif

#if ENABLE_SPI_TCP_COMM_TEST
    // 测试2: TCP-SPI-TCP完整回环测试
    PR_NOTICE("\n[2/2] Running TCP-SPI-TCP Full Loop Test...");
    PR_NOTICE("This test verifies: TCP → Queue → SPI → Hardware Loop → SPI → Queue → TCP");
    ret = test_spi_tcp_communication();
    if (ret != OPRT_OK) {
        PR_ERR("TCP-SPI-TCP Full Loop Test failed!");
        overall_result = ret;
    }
#else
    PR_NOTICE("\n[2/2] TCP-SPI-TCP Full Loop Test: DISABLED");
#endif
    
    // 总结
    PR_NOTICE("\n");
    PR_NOTICE("########################################");
    if (overall_result == OPRT_OK) {
        PR_NOTICE("#         ALL TESTS PASSED!           #");
    } else {
        PR_NOTICE("#        SOME TESTS FAILED!           #");
    }
    PR_NOTICE("########################################");
    PR_NOTICE("\n");
    
    return overall_result;
}
#endif // ENABLE_SPI_TCP_COMM_TEST || ENABLE_SPI_LOOPBACK_TEST

void user_main(void)
{
    int ret = OPRT_OK;
    int rt = OPRT_OK;
    int  remote_fd = -1;


    //! open iot development kit runtim init
    cJSON_InitHooks(&(cJSON_Hooks){.malloc_fn = tal_malloc, .free_fn = tal_free});
    tal_log_init(TAL_LOG_LEVEL_DEBUG, 1024, (TAL_LOG_OUTPUT_CB)tkl_log_output);

    PR_NOTICE("Application information:");
    PR_NOTICE("Project name:        %s", PROJECT_NAME);
    PR_NOTICE("App version:         %s", PROJECT_VERSION);
    PR_NOTICE("Compile time:        %s", __DATE__);
    PR_NOTICE("TuyaOpen version:    %s", OPEN_VERSION);
    PR_NOTICE("TuyaOpen commit-id:  %s", OPEN_COMMIT);
    PR_NOTICE("Platform chip:       %s", PLATFORM_CHIP);
    PR_NOTICE("Platform board:      %s", PLATFORM_BOARD);
    PR_NOTICE("Platform commit-id:  %s", PLATFORM_COMMIT);

    tal_kv_init(&(tal_kv_cfg_t){
        .seed = "vmlkasdh93dlvlcy",
        .key = "dflfuap134ddlduq",
    });
    tal_sw_timer_init();
    tal_workq_init();
    tal_cli_init();
    tuya_authorize_init();

    reset_netconfig_start();

    tuya_iot_license_t license;

    if (OPRT_OK != tuya_authorize_read(&license)) {
        license.uuid = TUYA_OPENSDK_UUID;
        license.authkey = TUYA_OPENSDK_AUTHKEY;
        PR_WARN("Replace the TUYA_OPENSDK_UUID and TUYA_OPENSDK_AUTHKEY contents, otherwise the demo cannot work.\n \
                Visit https://platform.tuya.com/purchase/index?type=6 to get the open-sdk uuid and authkey.");
    }

    /* Initialize Tuya device configuration */
    ret = tuya_iot_init(&ai_client, &(const tuya_iot_config_t){
                                        .software_ver = PROJECT_VERSION,
                                        .productkey = TUYA_PRODUCT_ID,
                                        .uuid = license.uuid,
                                        .authkey = license.authkey,
                                        // .firmware_key      = TUYA_DEVICE_FIRMWAREKEY,
                                        .event_handler = user_event_handler_on,
                                        .network_check = user_network_check,
                                    });
    assert(ret == OPRT_OK);

    // 初始化LWIP
#if defined(ENABLE_LIBLWIP) && (ENABLE_LIBLWIP == 1)
    TUYA_LwIP_Init();
#endif

    // network init
    netmgr_type_e type = 0;
#if defined(ENABLE_WIFI) && (ENABLE_WIFI == 1)
    type |= NETCONN_WIFI;
#endif
#if defined(ENABLE_WIRED) && (ENABLE_WIRED == 1)
    type |= NETCONN_WIRED;
#endif
    netmgr_init(type);
#if defined(ENABLE_WIFI) && (ENABLE_WIFI == 1)
    netmgr_conn_set(NETCONN_WIFI, NETCONN_CMD_NETCFG, &(netcfg_args_t){.type = NETCFG_TUYA_BLE});
#endif

    PR_DEBUG("tuya_iot_init success");

    ret = board_register_hardware();
    if (ret != OPRT_OK) {
        PR_ERR("board_register_hardware failed");
    }

    ret = app_chat_bot_init();
    if (ret != OPRT_OK) {
        PR_ERR("tuya_audio_recorde_init failed");
    }

    app_system_info();

    /* Start tuya iot task */
    tuya_iot_start(&ai_client);

    tkl_wifi_set_lp_mode(0, 0);

    reset_netconfig_check();
    TUYA_SPI_BASE_CFG_T spi_cfg = {.mode = TUYA_SPI_MODE0,
                                   .freq_hz = SPI_FREQ,
                                   .databits = TUYA_SPI_DATA_BIT8,
                                   .bitorder = TUYA_SPI_ORDER_LSB2MSB,
                                   .role = TUYA_SPI_ROLE_MASTER,
                                   .type = TUYA_SPI_AUTO_TYPE,
                                   .spi_dma_flags = 1
                                };
    TUYA_CALL_ERR_GOTO(tkl_spi_init(TUYA_SPI_NUM_1, &spi_cfg), __EXIT);
    
    // 初始化通信队列
    ret = init_communication_queues();
    if (ret != OPRT_OK) {
        PR_ERR("Failed to initialize communication queues");
        goto __EXIT;
    }
    
    // 启动SPI处理线程
    ret = start_spi_thread();
    if (ret != OPRT_OK) {
        PR_ERR("Failed to start SPI thread");
        cleanup_communication_queues();
        goto __EXIT;
    }

while (1)
{
  if (wlan_state == 0) {
        OPERATE_RET rt = OPRT_OK;   
        PR_NOTICE("wlan_state is 0, connect to the wifi first");
           /* Initialize the TuyaOS WiFi service */
        AP_IF_S *ap_info;
        uint32_t ap_info_nums;
        int i = 0;
        char info_ssid[50];

        PR_NOTICE("------ wifi scan example start ------");

        /*Scan WiFi information in the current environment*/
        TUYA_CALL_ERR_GOTO(tal_wifi_all_ap_scan(&ap_info, &ap_info_nums), __EXIT);
        PR_DEBUG("Scanf to %d wifi signals", ap_info_nums);
        for(i = 0; i < 2; i++) {
            strcpy((char *)info_ssid, (const char *)ap_info[i].ssid);
            PR_DEBUG("channel:%d, ssid:%s", ap_info[i].channel, info_ssid);  
        }

        /*Release the acquired WiFi information in the current environment*/
        TUYA_CALL_ERR_LOG(tal_wifi_release_ap(ap_info));
        PR_NOTICE("------ wifi scan example end ------");   
                // 派发“开始配网”事件，应用层可以根据此事件来闪灯
        ai_audio_player_play_alert(AI_AUDIO_ALERT_NETWORK_CFG);

        char connect_ssid[] = "Press@New";    // connect wifi ssid
        char connect_passwd[] = "anu1@163.com";   // connect wifi password


        PR_NOTICE("------ wifi station example start ------");

        /*WiFi init*/
        TUYA_CALL_ERR_GOTO(tal_wifi_init(wifi_event_callback), __EXIT);

        /*Set WiFi mode to station*/
        TUYA_CALL_ERR_GOTO(tal_wifi_set_work_mode(WWM_STATION), __EXIT);

        /*STA mode, connect to WiFi*/
        PR_NOTICE("\r\nconnect wifi ssid: %s, password: %s\r\n", connect_ssid, connect_passwd);
        TUYA_CALL_ERR_LOG(tal_wifi_station_connect((int8_t *)connect_ssid, (int8_t *)connect_passwd));
        tal_system_sleep(1000);  // 1s
        continue;
        }
       else
       {
        TUYA_IP_ADDR_T remote_ip;
        TUYA_ERRNO net_errno = 0;
        remote_fd = tal_net_socket_create(PROTOCOL_TCP);
        if (remote_fd < 0) {
            PR_ERR("create remote socket fail");
            goto __EXIT;
        }
        remote_ip = tal_net_str2addr(TCP_SERVER_IP);
        PR_NOTICE("Connecting to remote server %s:%d...", TCP_SERVER_IP, TCP_SERVER_PORT);
        app_display_send_msg(TY_DISPLAY_REMOTE_IP, (uint8_t *)&TCP_SERVER_IP, sizeof(TCP_SERVER_IP));

        net_errno = tal_net_connect(remote_fd, remote_ip, TCP_SERVER_PORT);
        if (net_errno < 0) {
            PR_ERR("connect to remote server fail, error: %d", tal_net_get_errno());
            goto __EXIT;
        }
        PR_NOTICE("Connected to remote server.");
        ai_audio_player_play_alert(AI_AUDIO_ALERT_NETWORK_CONNECTED);

        // 设置socket为非阻塞模式
        if (tal_net_set_block(remote_fd, FALSE) != OPRT_OK) {
            PR_ERR("Failed to set socket to non-blocking mode");
            goto __EXIT;
        }
        ret = init_communication_queues();
        if (ret != OPRT_OK) {
        PR_ERR("Failed to initialize communication queues");
        goto __EXIT;
    }
        
    // 等待SPI线程稳定运行
    tal_system_sleep(1000);
    
    #if AUTO_RUN_TESTS_ON_STARTUP
        // 运行通信系统测试
        PR_NOTICE("Starting communication system tests...");
        #if ENABLE_SPI_LOOPBACK_TEST || ENABLE_SPI_TCP_COMM_TEST
        ret = run_communication_tests();
        if (ret == OPRT_OK) {
            PR_NOTICE("All communication tests passed successfully!");
        } else {
            PR_WARN("Some communication tests failed, but continuing with normal operation...");
        }
        #else
        PR_NOTICE("All test functions disabled - no tests to run");
        #endif
    #else
        PR_NOTICE("Communication system tests: DISABLED (AUTO_RUN_TESTS_ON_STARTUP = 0)");
    #endif
    
        for (;;) {
            TUYA_FD_SET_T readfds;
            int max_fd = remote_fd + 1;
            int select_ret;
            uint8_t server_recv_buf[256];
            int recv_len;
            queue_msg_t queue_msg;
            
            // 初始化文件描述符集合，用于检测服务器是否有数据发送
            tal_net_fd_zero(&readfds);
            tal_net_fd_set(remote_fd, &readfds);
            
            // 使用select检查是否有数据可读，超时时间设为10ms
            // 这样既能及时响应服务器数据，又不会阻塞队列操作
            select_ret = tal_net_select(max_fd, &readfds, NULL, NULL, 10);
            
            if (select_ret > 0) {
                // 检查服务器是否有数据发送过来
                if (tal_net_fd_isset(remote_fd, &readfds)) {
                    recv_len = tal_net_recv(remote_fd, server_recv_buf, sizeof(server_recv_buf) - 1);
                    if (recv_len > 0) {
                        server_recv_buf[recv_len] = '\0'; // 确保字符串结束
                        PR_NOTICE("Received from server: %d bytes: %s", recv_len, server_recv_buf);
                        
                        // 将服务器接收的数据放入TCP到SPI的队列
                        memset(&queue_msg, 0, sizeof(queue_msg));
                        memcpy(queue_msg.data, server_recv_buf, recv_len);
                        queue_msg.length = recv_len;
                        
                        if (tal_queue_post(tcp_to_spi_queue, &queue_msg, 0) == OPRT_OK) {
                            PR_DEBUG("Successfully queued %d bytes from server to SPI", recv_len);
                        } else {
                            PR_ERR("Failed to queue server data to TCP-to-SPI queue");
                        }
                    } else if (recv_len == 0) {
                        PR_NOTICE("Server disconnected gracefully");
                        break;
                    } else {
                        // recv_len < 0，检查是否是EAGAIN错误（非阻塞模式下正常）
                        TUYA_ERRNO errno_val = tal_net_get_errno();
                        if (errno_val != UNW_EAGAIN && errno_val != UNW_EWOULDBLOCK) {
                            PR_ERR("recv error: %d", errno_val);
                            break;
                        }
                        // EAGAIN/EWOULDBLOCK表示暂时没有数据，继续处理队列
                    }
                }
            } else if (select_ret < 0) {
                PR_ERR("select error: %d", tal_net_get_errno());
                break;
            }
            // select_ret == 0 表示超时，正常情况，继续处理队列
            
            // 从SPI到TCP队列中获取数据并发送到服务器
            if (tal_queue_fetch(spi_to_tcp_queue, &queue_msg, 0) == OPRT_OK) {
                PR_DEBUG("Got %d bytes from SPI queue, sending to server: %s", queue_msg.length, queue_msg.data);
                
                // 发送SPI数据到远程服务器
                if (tal_net_send(remote_fd, queue_msg.data, queue_msg.length) < 0) {
                    TUYA_ERRNO errno_val = tal_net_get_errno();
                    if (errno_val != UNW_EAGAIN && errno_val != UNW_EWOULDBLOCK) {
                        PR_ERR("send to remote server fail: %d", errno_val);
                        break;
                    }
                    // 如果是EAGAIN/EWOULDBLOCK，表示发送缓冲区满，数据已经丢失，下次再试
                    PR_WARN("TCP send buffer full, SPI data lost");
                }
            }
            
            tal_system_sleep(10); // 减少延时，提高响应速度
        }
        __EXIT:
         PR_ERR("TCP connection failed or disconnected");
         
         // 停止SPI线程和清理资源
         //stop_spi_thread();
         cleanup_communication_queues();
         
        if (remote_fd >= 0) tal_net_close(remote_fd);
    }

    }
}

/**
 * @brief main
 *
 * @param argc
 * @param argv
 * @return void
 */
#if OPERATING_SYSTEM == SYSTEM_LINUX
void main(int argc, char *argv[])
{
    user_main();
}
#else

/* Tuya thread handle */
static THREAD_HANDLE ty_app_thread = NULL;

/**
 * @brief  task thread
 *
 * @param[in] arg:Parameters when creating a task
 * @return none
 */
static void tuya_app_thread(void *arg)
{
    user_main();

    tal_thread_delete(ty_app_thread);
    ty_app_thread = NULL;
}

void tuya_app_main(void)
{
    THREAD_CFG_T thrd_param = {4096, 4, "tuya_app_main"};
    tal_thread_create_and_start(&ty_app_thread, NULL, NULL, tuya_app_thread, NULL, &thrd_param);
}
#endif