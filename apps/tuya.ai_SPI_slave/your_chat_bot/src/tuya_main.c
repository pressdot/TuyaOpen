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
#include "tal_uart.h"
#include "app_chat_bot.h"
#include "ai_audio.h"
#include "reset_netcfg.h"
#include "app_system_info.h"
#include "tal_network.h"

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

// UART回环测试开关
#define ENABLE_UART_LOOPBACK_TEST 0

// 全局变量用于UART任务
static int g_remote_fd = -1;
static uint8_t _need_reset = 0;
char wlan_state=0; 

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
 * @brief UART接收回调任务函数
 * 
 * @param args 任务参数
 */
void uart_rx_callback_task(void *args) {
    char buff[256] = {0};
    int read_uart_len;
    
    PR_NOTICE("UART RX callback task started");
    
    while(1) {
        read_uart_len = tal_uart_get_rx_data_size(TUYA_UART_NUM_0);
        if (read_uart_len > 0 && read_uart_len <= 256) {
            // 串口收数据
            int actual_read = tal_uart_read(TUYA_UART_NUM_0, (uint8_t *)buff, read_uart_len);
            if (actual_read > 0) {
                // 打印接收到的UART数据
                PR_DEBUG("UART RX task received: %d bytes", actual_read);
                
                // 打印十六进制格式
                PR_DEBUG("UART data (hex): ");
                for (int i = 0; i < actual_read; i++) {
                    PR_DEBUG_RAW("%02x ", (uint8_t)buff[i]);
                }
                PR_DEBUG_RAW("\n");
                
                // 如果是可打印字符，也打印ASCII格式
                bool is_printable = true;
                for (int i = 0; i < actual_read; i++) {
                    if (buff[i] < 32 || buff[i] > 126) {
                        is_printable = false;
                        break;
                    }
                }
                if (is_printable) {
                    char ascii_str[257] = {0};
                    memcpy(ascii_str, buff, actual_read);
                    ascii_str[actual_read] = '\0';
                    PR_DEBUG("UART data (ASCII): %s", ascii_str);
                }
                
                // 如果TCP连接有效，转发数据到TCP服务器
                if (g_remote_fd >= 0) {
                    if (tal_net_send(g_remote_fd, (uint8_t *)buff, actual_read) >= 0) {
                        PR_DEBUG("UART data forwarded to TCP server: %d bytes", actual_read);
                    } else {
                        PR_ERR("Failed to forward UART data to TCP server");
                    }
                }
                
                // 清空缓冲区
                memset(buff, 0, sizeof(buff));
            }
        }
       // PR_NOTICE("UART RX callback task called");
        tal_system_sleep(20); // 20ms检查一次
    }
}

/**
 * @brief 创建UART接收任务
 */
OPERATE_RET create_uart_rx_task(void) {
    OPERATE_RET ret = OPRT_OK;
    THREAD_HANDLE uart_task_handle;
    THREAD_CFG_T thread_cfg = {
        .thrdname = "uart_rx_task",
        .priority = THREAD_PRIO_3,  // 较高优先级
        .stackDepth = 4096
    };
    
    ret = tal_thread_create_and_start(&uart_task_handle, NULL, NULL, uart_rx_callback_task, NULL, &thread_cfg);
    if (ret != OPRT_OK) {
        PR_ERR("Failed to create UART RX task: %d", ret);
        return ret;
    }
    
    PR_NOTICE("UART RX task created successfully");
    return OPRT_OK;
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

void user_main(void)
{
    int ret = OPRT_OK;
    int remote_fd = -1;


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
    //tal_cli_init();
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
    
    // 初始化UART0用于数据通信
    TAL_UART_CFG_T uart_cfg;
    memset(&uart_cfg, 0, sizeof(TAL_UART_CFG_T));
    uart_cfg.base_cfg.baudrate = 115200;
    uart_cfg.base_cfg.databits = TUYA_UART_DATA_LEN_8BIT;
    uart_cfg.base_cfg.parity = TUYA_UART_PARITY_TYPE_NONE;
    uart_cfg.base_cfg.stopbits = TUYA_UART_STOP_LEN_1BIT;
    uart_cfg.rx_buffer_size = 256;
    uart_cfg.open_mode = O_BLOCK;
    
    ret = tal_uart_init(TUYA_UART_NUM_0, &uart_cfg);
    if (ret != OPRT_OK) {
        PR_ERR("uart init failed: %d", ret);
        goto __EXIT;
    }
    PR_NOTICE("UART0 initialized successfully");

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
            PR_ERR("connect to remote server fail");
            goto __EXIT;
        }
        PR_NOTICE("Connected to remote server.");
        ai_audio_player_play_alert(AI_AUDIO_ALERT_NETWORK_CONNECTED);

        // 设置socket为非阻塞模式，避免在没有数据时阻塞
        tal_net_set_block(remote_fd, FALSE);
        
        // 设置全局TCP socket句柄，供UART任务使用
        g_remote_fd = remote_fd;
        
        // 创建UART接收任务
        ret = create_uart_rx_task();
        if (ret != OPRT_OK) {
            PR_ERR("Failed to create UART RX task");
            goto __EXIT;
        }

#if ENABLE_UART_LOOPBACK_TEST
        // UART回环测试
        PR_NOTICE("Starting UART loopback test...");
        
        // 发送测试数据到UART
        const char test_message[] = "UART Loopback Test: Hello TuyaOpen!";
        int test_msg_len = strlen(test_message);
        uint8_t loopback_buf[256] = {0};
        PR_NOTICE("Sending test message to UART: %s", test_message);
        int sent_bytes = tal_uart_write(TUYA_UART_NUM_0, (const uint8_t *)test_message, test_msg_len);
        if (sent_bytes > 0) {
            
            PR_NOTICE("UART test message sent: %d bytes", sent_bytes);
            
            // 等待一小段时间让数据通过回环
           // tal_system_sleep(100);
            
            // 尝试读取回环数据
         
            int loopback_len = tal_uart_get_rx_data_size(TUYA_UART_NUM_0);
            
            if (loopback_len > 0) {
                int read_bytes = tal_uart_read(TUYA_UART_NUM_0, loopback_buf, loopback_len);
                if (read_bytes > 0) {
                    PR_NOTICE("UART loopback received: %d bytes", read_bytes);
                    
                    // 将回环测试数据发送到TCP服务器
                    if (tal_net_send(remote_fd, loopback_buf, read_bytes) >= 0) {
                        PR_NOTICE("Loopback test data sent to TCP server successfully");
                    } else {
                        PR_ERR("Failed to send loopback test data to TCP server");
                    }
                    
                    // 打印回环数据的十六进制格式
                    PR_NOTICE("Loopback data (hex): ");
                    for (int i = 0; i < read_bytes; i++) {
                        PR_DEBUG_RAW("%02x ", loopback_buf[i]);
                    }
                    PR_DEBUG_RAW("\n");
                } else {
                    PR_WARN("Failed to read UART loopback data");
                }
            } else {
                PR_WARN("No UART loopback data received (check physical loopback connection)");
            }
        } else {
            PR_NOTICE("Failed to send UART test message: %s", loopback_buf);
            PR_ERR("Failed to send UART test message");
        }
        
        PR_NOTICE("UART loopback test completed");
#endif

        for (;;) {
            // UART数据接收现在由专门的任务处理
            // 这里只处理从TCP服务器接收数据并发送到UART0
            
            uint8_t tcp_rec_buf[256] = {0};
            int tcp_recv_len = tal_net_recv(remote_fd, tcp_rec_buf, sizeof(tcp_rec_buf));
            
            if (tcp_recv_len > 0) {
                PR_DEBUG("Received from TCP server: %d bytes", tcp_recv_len);
                
                // 将TCP数据转发到UART
                int uart_sent_len = tal_uart_write(TUYA_UART_NUM_0, tcp_rec_buf, tcp_recv_len);
                if (uart_sent_len > 0) {
                    PR_DEBUG("TCP data forwarded to UART0: %d bytes", uart_sent_len);
                    
                    // 打印接收到的TCP数据（十六进制格式）
                    PR_DEBUG("TCP data (hex): ");
                    for (int i = 0; i < tcp_recv_len; i++) {
                        PR_DEBUG_RAW("%02x ", tcp_rec_buf[i]);
                    }
                    PR_DEBUG_RAW("\n");
                    
                    // 如果是可打印字符，也打印ASCII格式
                    bool is_printable = true;
                    for (int i = 0; i < tcp_recv_len; i++) {
                        if (tcp_rec_buf[i] < 32 || tcp_rec_buf[i] > 126) {
                            is_printable = false;
                            break;
                        }
                    }
                    if (is_printable) {
                        char ascii_str[257] = {0};
                        memcpy(ascii_str, tcp_rec_buf, tcp_recv_len);
                        ascii_str[tcp_recv_len] = '\0';
                        PR_DEBUG("TCP data (ASCII): %s", ascii_str);
                    }
                } else {
                    PR_ERR("Failed to forward TCP data to UART0");
                }
            } else if (tcp_recv_len < 0) {
                // 对于非阻塞socket，返回负值可能是正常的（没有数据可读）
                // 只有在真正的连接错误时才退出循环
            }
            // tcp_recv_len == 0 通常表示连接被对方关闭
            
            tal_system_sleep(10); // 避免CPU占用过高
        }
        __EXIT:
         PR_ERR("failed");
        // 清理全局TCP句柄
        g_remote_fd = -1;
        if (remote_fd >= 0) tal_net_close(remote_fd);
       // tal_uart_deinit(TUYA_UART_NUM_0);
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