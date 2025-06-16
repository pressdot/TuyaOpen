/**
 * @file tdd_disp_st7305.h
 * @version 0.1
 * @date 2025-03-12
 */

#ifndef __TDD_DISP_ST7305_H__
#define __TDD_DISP_ST7305_H__

#include "tuya_cloud_types.h"
#include "tdd_disp_type.h"

#ifdef __cplusplus
extern "C" {
#endif

/***********************************************************
************************macro define************************
***********************************************************/
/* ST7305 commands */
#define ST7305_NOP        0x00    // No Operation
#define ST7305_SWRESET    0x01    // Software Reset
#define ST7305_RDDID      0x04    // Read Display ID
#define ST7305_RDDST      0x09    // Read Display Status
#define ST7305_SLPIN      0x10    // Sleep In
#define ST7305_SLPOUT     0x11    // Sleep Out
#define ST7305_PTLON      0x12    // Partial Display Mode On
#define ST7305_NORON      0x13    // Normal Display Mode On
#define ST7305_INVOFF     0x20    // Display Inversion Off
#define ST7305_INVON      0x21    // Display Inversion On
#define ST7305_DISPOFF    0x28    // Display Off
#define ST7305_DISPON     0x29    // Display On
#define ST7305_CASET      0x2A    // Column Address Set
#define ST7305_RASET      0x2B    // Row Address Set
#define ST7305_RAMWR      0x2C    // Memory Write
#define ST7305_RAMRD      0x2E    // Memory Read
#define ST7305_COLMOD     0x3A    // Interface Pixel Format
#define ST7305_TE         0x35    // TE Tear Effect Control
#define ST7305_MADCTL     0x36    // Memory Data Access Control
#define ST7305_RDID1      0xDA    // Read ID1
#define ST7305_RDID2      0xDB    // Read ID2
#define ST7305_RDID3      0xDC    // Read ID3
#define ST7305_RDID4      0xDD    // Read ID4
#define ST7305_GMCTRP1    0xE0    // Positive Gamma Correction
#define ST7305_GMCTRN1    0xE1    // Negative Gamma Correction
#define ST7305_PWCTR6     0xFC    // Power Control 6
#define ST7305_BOOSTER_EN 0xD1    // Booster Enable
#define ST7305_NVM_LOAD   0xD6    // NVM Load Control
#define ST7305_GATE_VOLTAGE 0xC0  // Gate Voltage Setting
#define ST7305_VSHP_SETTING 0xC1  // VSHP Setting
#define ST7305_VSLP_SETTING 0xC2  // VSLP Setting
#define ST7305_VSHN_SETTING 0xC4  // VSHN Setting
#define ST7305_VSLN_SETTING 0xC5  // VSLN Setting
#define ST7305_HPM        0x38    // High Power Mode
#define ST7305_LPM        0x39    // Low Power Mode
#define ST7305_OSC_SET        0xD8    // OSC Setting
#define ST7305_FRAME_RATE     0xB2    // Frame Rate Control
#define ST7305_GATE_EQ_HPM    0xB3    // Gate EQ Control HPM
#define ST7305_GATE_EQ_LPM    0xB4    // Gate EQ Control LPM
#define ST7305_GATE_TIMING    0x62    // Gate Timing Control
#define ST7305_SOURCE_EQ      0xB7    // Source EQ Enable
#define ST7305_GATE_LINE      0xB0    // Gate Line Setting
#define ST7305_SOURCE_VOLT    0xC9    // Source Voltage Select
#define ST7305_GAMMA_MODE     0xB9    // Gamma Mode Setting
#define ST7305_PANEL_SET      0xB8    // Panel Setting
#define ST7305_AUTO_POWER     0xD0    // Auto Power Down
#define ST7305_CLEAR_RAM      0xBB    // Enable Clear RAM



/***********************************************************
***********************typedef define***********************
***********************************************************/

/***********************************************************
********************function declaration********************
***********************************************************/
OPERATE_RET tdd_disp_spi_st7309_register(char *name, DISP_SPI_DEVICE_CFG_T *dev_cfg);


#ifdef __cplusplus
}
#endif

#endif /* __LCD_ST7305_H__ */
