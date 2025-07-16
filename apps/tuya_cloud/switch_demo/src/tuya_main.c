/**
 * @file tuya_main.c
 *
 * @brief a simple switch demo show how to use tuya-open-sdk-for-device to
 * develop a simple switch. 1, download, compile, run in ubuntu according the
 * README.md 2, binding the device use tuya APP accoring scan QRCode 3, on/off
 * the switch in tuya APP 4, "switch on/off" use cli
 * @copyright Copyright (c) 2021-2024 Tuya Inc. All Rights Reserved.
 */

#include "cJSON.h"
#include "netmgr.h"
#include "tal_api.h"
#include "tkl_output.h"
#include "tuya_config.h"
#include "tuya_iot.h"
#include "tuya_iot_dp.h"
#include "tal_cli.h"
#include "tuya_authorize.h"
#include <assert.h>
#if defined(ENABLE_WIFI) && (ENABLE_WIFI == 1)
#include "netconn_wifi.h"
#endif
#if defined(ENABLE_WIRED) && (ENABLE_WIRED == 1)
#include "netconn_wired.h"
#endif
#if defined(ENABLE_LIBLWIP) && (ENABLE_LIBLWIP == 1)
#include "lwip_init.h"
#endif

#include "reset_netcfg.h"
#include "tkl_spi.h"
#ifndef PROJECT_VERSION
#define PROJECT_VERSION "1.0.0"
#endif

#define SPI_FREQ 1000

void user_main()
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
