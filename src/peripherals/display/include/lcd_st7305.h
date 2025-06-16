/**
 * @file lcd_st7305.h
 * @version 0.1
 * @date 2025-03-12
 */

#ifndef __LCD_ST7305_H__
#define __LCD_ST7305_H__

#include "tuya_cloud_types.h"

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
#define ST7305_MADCTL     0x36    // Memory Data Access Control
#define ST7305_FRMCTR1    0xB1    // Frame Rate Control (In normal mode)
#define ST7305_FRMCTR2    0xB2    // Frame Rate Control (In idle mode)
#define ST7305_FRMCTR3    0xB3    // Frame Rate Control (In partial mode)
#define ST7305_INVCTR     0xB4    // Display Inversion Control
#define ST7305_PWCTR1     0xC0    // Power Control 1
#define ST7305_PWCTR2     0xC1    // Power Control 2
#define ST7305_PWCTR3     0xC2    // Power Control 3
#define ST7305_PWCTR4     0xC3    // Power Control 4
#define ST7305_PWCTR5     0xC4    // Power Control 5
#define ST7305_VMCTR1     0xC5    // VCOM Voltage Setting
#define ST7305_RDID1      0xDA    // Read ID1
#define ST7305_RDID2      0xDB    // Read ID2
#define ST7305_RDID3      0xDC    // Read ID3
#define ST7305_RDID4      0xDD    // Read ID4
#define ST7305_GMCTRP1    0xE0    // Positive Gamma Correction
#define ST7305_GMCTRN1    0xE1    // Negative Gamma Correction
#define ST7305_PWCTR6     0xFC    // Power Control 6
#define ST7305_BOOSTER_EN 0xF7    // Booster Enable
#define ST7305_NVM_LOAD   0xF8    // NVM Load Control
#define ST7305_GATE_VOLTAGE 0xF9  // Gate Voltage Setting
#define ST7305_VSHP_SETTING 0xFA  // VSHP Setting
#define ST7305_VSLP_SETTING 0xFB  // VSLP Setting
#define ST7305_VSHN_SETTING 0xFC  // VSHN Setting
#define ST7305_VSLN_SETTING 0xFD  // VSLN Setting
#define ST7305_HPM        0xFE    // High Power Mode
#define ST7305_LPM        0xFF    // Low Power Mode


/***********************************************************
***********************typedef define***********************
***********************************************************/

/***********************************************************
********************function declaration********************
***********************************************************/

#ifdef __cplusplus
}
#endif

#endif /* __LCD_ST7305_H__ */
