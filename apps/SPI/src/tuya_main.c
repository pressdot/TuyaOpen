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
#include "tkl_output.h"
#include "tkl_spi.h"
#include "board_com_api.h"

#include "app_system_info.h"

/* Tuya device handle */
#define SPI_FREQ 1000


#ifndef PROJECT_VERSION
#define PROJECT_VERSION "1.0.0"
#endif
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


void user_main(void)
{
 uint8_t spi_snd_buf[] = {"Hello Tuya"};
    uint8_t spi_rec_buf[256] = {"Hello Tuya"};
    int rt = OPRT_OK;
    TUYA_SPI_BASE_CFG_T spi_cfg = {.mode = TUYA_SPI_MODE0,
                                   .freq_hz = SPI_FREQ,
                                   .databits = TUYA_SPI_DATA_BIT8,
                                   .bitorder = TUYA_SPI_ORDER_LSB2MSB,
                                   .role = TUYA_SPI_ROLE_MASTER,
                                   .type = TUYA_SPI_AUTO_TYPE,
                                   .spi_dma_flags = 1
                                };
    TUYA_CALL_ERR_LOG(tkl_spi_init(TUYA_SPI_NUM_1, &spi_cfg));


    PR_NOTICE("Application information:");
    PR_NOTICE("Project name:        %s", PROJECT_NAME);
    PR_NOTICE("App version:         %s", PROJECT_VERSION);
    PR_NOTICE("Compile time:        %s", __DATE__);
    PR_NOTICE("TuyaOpen version:    %s", OPEN_VERSION);
    PR_NOTICE("TuyaOpen commit-id:  %s", OPEN_COMMIT);
    PR_NOTICE("Platform chip:       %s", PLATFORM_CHIP);
    PR_NOTICE("Platform board:      %s", PLATFORM_BOARD);
    PR_NOTICE("Platform commit-id:  %s", PLATFORM_COMMIT);

    // 初始化LWIP
    for (;;) {
        /* Loop to receive packets, and handles client keepalive */
        TUYA_CALL_ERR_LOG(tkl_spi_transfer(TUYA_SPI_NUM_1, spi_snd_buf,spi_rec_buf, CNTSOF(spi_snd_buf)));
        PR_NOTICE("spi send  \"%s\" rec \"%s\" finish",spi_snd_buf, spi_rec_buf);
        
    }
}

/**
 * @brief main
 *
 * @param argc
 * @param argv
 * @return void
 */


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
