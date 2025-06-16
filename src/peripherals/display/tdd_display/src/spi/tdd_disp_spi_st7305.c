/**
 * @file tdd_disp_spi_st7305.c
 * @brief Implementation of ST7305 TFT LCD driver with SPI interface. This file
 *        provides hardware-specific control functions for ST7305 series TFT
 *        displays, including initialization sequence, pixel data transfer,
 *        and display control commands through SPI communication.
 *
 * @copyright Copyright (c) 2021-2025 Tuya Inc. All Rights Reserved.
 *
 */
#include "tuya_cloud_types.h"
#include "tdd_disp_st7305.h"
#include "tdl_display_driver.h"
#include "tal_log.h"

/***********************************************************
***********************const define**********************
***********************************************************/
const uint8_t cST7305_INIT_SEQ[] = {
    2,    0,    ST7305_NVM_LOAD,   0x13, 0x02,                 // NVM Load Control
    2,    0,    ST7305_BOOSTER_EN, 0x01,                        // Booster Enable
    3,    0,    ST7305_GATE_VOLTAGE, 0x12, 0x0A,               // Gate Voltage Setting (VGH=12V, VGL=-10V)
    5,    0,    ST7305_VSHP_SETTING, 0x3C, 0x3C, 0x3C, 0x3C,   // VSHP Setting (4.8V)
    5,    0,    ST7305_VSLP_SETTING, 0x23, 0x23, 0x23, 0x23,   // VSLP Setting (0.98V)
    5,    0,    ST7305_VSHN_SETTING, 0x5A, 0x5A, 0x5A, 0x5A,   // VSHN Setting (-3.6V)
    5,    0,    ST7305_VSLN_SETTING, 0x37, 0x37, 0x37, 0x37,   // VSLN Setting (0.22V)
    3,    0,    ST7305_OSC_SET,    0xA6, 0xE9,                 // OSC Setting (HPM=32Hz)
    2,    0,    ST7305_FRAME_RATE, 0x12,                        // Frame Rate Control (HPM=32Hz, LPM=1Hz)
    11,   0,    ST7305_GATE_EQ_HPM, 0xE5, 0xF6, 0x05, 0x46, 0x77, 0x77, 0x77, 0x77, 0x76, 0x45, // Gate EQ Control HPM
    8,    0,    ST7305_GATE_EQ_LPM, 0x05, 0x46, 0x77, 0x77, 0x77, 0x77, 0x76, 0x45, // Gate EQ Control LPM
    4,    0,    ST7305_GATE_TIMING, 0x32, 0x03, 0x1F,          // Gate Timing Control
    2,    0,    ST7305_SOURCE_EQ,  0x13,                        // Source EQ Enable
    2,    0,    ST7305_GATE_LINE,  0x60,                        // Gate Line Setting (384 line)
    1,    255,  ST7305_SLPOUT,                                  // Sleep out
    2,    0,    ST7305_SOURCE_VOLT, 0x00,                       // Source Voltage Select (VSHP1, VSLP1, VSHN1, VSLN1)
    2,    0,    ST7305_MADCTL,    0x48,                        // Memory Data Access Control (MX=1, DO=1)
    2,    0,    ST7305_COLMOD,    0x10,                        // Data Format Select (4write for 24bit, 3write for 18bit) 
    2,    0,    ST7305_GAMMA_MODE, 0x20,                       // Gamma Mode Setting (Mono)
    2,    0,    ST7305_PANEL_SET,  0x09,                       // Panel Setting (Frame inversion, column)
    3,    0,    ST7305_CASET,     0x17, 0x24,                  // Column Address Setting (168*384)
    3,    0,    ST7305_RASET,     0x00, 0xBF,                  // Row Address Setting
    2,    0,    ST7305_TE,        0x00,                        // TE
    2,    0,    ST7305_AUTO_POWER, 0xFF,                       // Auto power down ON
    1,    0,    ST7305_LPM,                                     // Low Power Mode ON
    1,    0,    ST7305_DISPON,                                 // Display ON
    0                                                          // Terminate list
};

static TDD_DISP_SPI_CFG_T sg_disp_spi_cfg = {
    .cfg = {
        .cmd_caset = ST7305_CASET,
        .cmd_raset = ST7305_RASET,
        .cmd_ramwr = ST7305_RAMWR,
    },
    
    .init_seq = cST7305_INIT_SEQ,
};

/***********************************************************
***********************function define**********************
***********************************************************/
OPERATE_RET tdd_disp_spi_st7305_register(char *name, DISP_SPI_DEVICE_CFG_T *dev_cfg)
{
    if (NULL == name || NULL == dev_cfg) {
        return OPRT_INVALID_PARM;
    }

    PR_NOTICE("tdd_disp_spi_st7305_register: %s", name);

    sg_disp_spi_cfg.cfg.width     = dev_cfg->width;
    sg_disp_spi_cfg.cfg.height    = dev_cfg->height;
    sg_disp_spi_cfg.cfg.pixel_fmt = dev_cfg->pixel_fmt;
    sg_disp_spi_cfg.cfg.port      = dev_cfg->port;
    sg_disp_spi_cfg.cfg.spi_clk   = dev_cfg->spi_clk;
    sg_disp_spi_cfg.cfg.cs_pin    = dev_cfg->cs_pin;
    sg_disp_spi_cfg.cfg.dc_pin    = dev_cfg->dc_pin;
    sg_disp_spi_cfg.cfg.rst_pin   = dev_cfg->rst_pin;
    sg_disp_spi_cfg.rotation      = dev_cfg->rotation;

    memcpy(&sg_disp_spi_cfg.power, &dev_cfg->power, sizeof(TUYA_DISPLAY_IO_CTRL_T));
    memcpy(&sg_disp_spi_cfg.bl, &dev_cfg->bl, sizeof(TUYA_DISPLAY_BL_CTRL_T));

    return tdl_disp_spi_device_register(name, &sg_disp_spi_cfg);
}
