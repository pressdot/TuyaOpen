/**
 * @file disp_spi_st7305.c
 * @brief Implementation of ST7305 TFT LCD driver with SPI interface. This file
 *        provides hardware-specific control functions for ST7305 series TFT
 *        displays, including initialization sequence, pixel data transfer,
 *        and display control commands through SPI communication.
 *
 * @copyright Copyright (c) 2021-2025 Tuya Inc. All Rights Reserved.
 *
 */
#include "tuya_cloud_types.h"
#include "lcd_st7305.h"
#include "tkl_disp_drv_lcd.h"
#include "tal_log.h"


/***********************************************************
***********************const define**********************
***********************************************************/
const uint8_t cST7305_INIT_SEQ[] = {
    3,    0,    ST7305_NVM_LOAD,   0x13, 0x02,                  // NVM Load Control
    2,    0,    ST7305_BOOSTER_EN, 0x01,                        // Booster Enable
    3,    0,    ST7305_GATE_VOLTAGE, 0x12, 0x0a,                // Gate Voltage Setting
    5,    0,    ST7305_VSHP_SETTING, 0x73, 0x3e, 0x3c, 0x3c,    // VSHP Setting
    5,    0,    ST7305_VSLP_SETTING, 0x00, 0x21, 0x23, 0x23,    // VSLP Setting
    5,    0,    ST7305_VSHN_SETTING, 0x32, 0x5c, 0x5a, 0x5a,    // VSHN Setting
    5,    0,    ST7305_VSLN_SETTING, 0x32, 0x35, 0x37, 0x37,    // VSLN Setting
    3,    0,    0xD8, 0x80, 0xe9,                               // OSC Setting
    2,    0,    ST7305_FRMCTR2, 0x12,                           // Frame Rate Control
    11,   0,    0xB3, 0xe5, 0xf6, 0x17, 0x77, 0x77, 0x77, 0x77, 0x77, 0x77, 0x71, // Update Period Gate EQ Control in HPM
    8,    0,    0xB4, 0x05, 0x46, 0x77, 0x77, 0x77, 0x77, 0x76, 0x45, // Update Period Gate EQ Control in LPM
    4,    0,    0x62, 0x32, 0x03, 0x1f,                         // Gate Timing Control
    2,    0,    0xB7, 0x13,                                     // Source EQ Enable
    2,    0,    0xB0, 0x60,                                     // Gate Line Setting
    1,    120,  ST7305_SLPOUT,                                  // Sleep out
    2,    0,    0xC9, 0x00,                                     // Source Voltage Select
    2,    0,    ST7305_MADCTL, 0x48,                            // Memory Data Access Control
    2,    0,    ST7305_COLMOD, 0x11,                            // Data Format Select
    2,    0,    0xB9, 0x20,                                     // Gamma Mode Setting
    2,    0,    0xB8, 0x29,                                     // Panel Setting
    3,    0,    ST7305_CASET, 0x17, 0x24,                       // Column Address Setting
    3,    0,    ST7305_RASET, 0x00, 0xbf,                       // Row Address Setting
    2,    0,    0x35, 0x00,                                     // TE
    2,    0,    0xD0, 0xff,                                     // Auto power down
    1,    0,    0x38,                                           // HPM: high Power Mode ON
    1,    0,    ST7305_DISPON,                                  // DISPLAY ON
    1,    0,    ST7305_INVOFF,                                  // Display Inversion Off
    2,    0,    0xBB, 0x4f,                                     // Enable Clear RAM
    0                                                           // Terminate list
};

const TUYA_LCD_SPI_CFG_T cST7305_CFG = {
    .rst_pin   = LCD_SPI_RST_PIN,
    .cs_pin    = LCD_SPI_CS_PIN,
    .dc_pin    = LCD_SPI_DC_PIN,
    .spi_port  = LCD_SPI_PORT,
    .spi_clk   = LCD_SPI_CLK,

    .cmd_caset = ST7305_CASET,
    .cmd_raset = ST7305_RASET,
    .cmd_ramwr = ST7305_RAMWR,

    .init_seq  = cST7305_INIT_SEQ,
};

/***********************************************************
***********************function define**********************
***********************************************************/
/**
 * @brief Register an LCD device.
 * 
 * This function initializes and registers an LCD device based on the provided device ID.
 * It configures the device's basic properties such as resolution, pixel format, rotation,
 * and backlight mode. Depending on the backlight mode, it sets up either GPIO or PWM parameters.
 * Finally, the function registers the LCD device using the configured parameters.
 * 
 * @param dev_id The unique identifier for the LCD device to be registered.
 * @return OPERATE_RET Returns 0 (OPRT_OK) on success, or a non-zero error code on failure.
 */
OPERATE_RET tuya_lcd_device_register(int dev_id)
{
    OPERATE_RET ret = 0;
    TUYA_LCD_CFG_T lcd_device;

    memset(&lcd_device, 0x00, sizeof(TUYA_LCD_CFG_T));

    lcd_device.id       = dev_id;
    lcd_device.width    = DISPLAY_LCD_WIDTH;
    lcd_device.height   = DISPLAY_LCD_HEIGHT;
    lcd_device.fmt      = TKL_DISP_PIXEL_FMT_MONO;
    lcd_device.rotation = TKL_DISP_ROTATION_0;

    lcd_device.lcd_tp  = TUYA_LCD_TYPE_SPI;
    lcd_device.p_spi   = &cST7305_CFG;

#if defined(ENABLE_LCD_POWER_CTRL) && (ENABLE_LCD_POWER_CTRL == 1)
    lcd_device.power_io        = DISPLAY_LCD_POWER_PIN;
    lcd_device.power_active_lv = DISPLAY_LCD_POWER_POLARITY_LEVEL;
#else
    lcd_device.power_io = INVALID_GPIO_PIN;
#endif

#if defined(ENABLE_LCD_BL_MODE_GPIO) && (ENABLE_LCD_BL_MODE_GPIO == 1)
    lcd_device.bl.mode           = TUYA_DISP_BL_GPIO;
    lcd_device.bl.gpio.io        = DISPLAY_LCD_BL_PIN;
    lcd_device.bl.gpio.active_lv = DISPLAY_LCD_BL_POLARITY_LEVEL;
#elif defined(ENABLE_LCD_BL_MODE_PWM) && (ENABLE_LCD_BL_MODE_PWM == 1)
    lcd_device.bl.mode              = TUYA_DISP_BL_PWM;
    lcd_device.bl.pwm.id            = DISPLAY_LCD_BL_PWM_ID;
    lcd_device.bl.pwm.cfg.polarity  = DISPLAY_LCD_BL_POLARITY_LEVEL;
    lcd_device.bl.pwm.cfg.frequency = DISPLAY_LCD_BL_PWM_FREQ;
#else 
    lcd_device.bl.mode = TUYA_DISP_BL_NOT_EXIST;
#endif
    ret = tkl_disp_register_lcd_dev(&lcd_device);
    if(ret != OPRT_OK) {
         PR_ERR("tkl_disp_register_lcd_dev error:%d", ret);
         return ret;
    }
    
    return OPRT_OK;
}