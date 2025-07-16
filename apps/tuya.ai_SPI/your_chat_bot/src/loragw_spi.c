/*
 / _____)             _              | |
( (____  _____ ____ _| |_ _____  ____| |__
 \____ \| ___ |    (_   _) ___ |/ ___)  _ \
 _____) ) ____| | | || |_| ____( (___| | | |
(______/|_____)_|_|_| \__)_____)\____)_| |_|
  (C)2019 Semtech

Description:
    Host specific functions to address the LoRa concentrator registers through
    a SPI interface.
    Single-byte read/write and burst read/write.
    Could be used with multiple SPI ports in parallel (explicit file descriptor)

License: Revised BSD License, see LICENSE.TXT file include in the project
*/


/* -------------------------------------------------------------------------- */
/* --- DEPENDANCIES --------------------------------------------------------- */

#include <stdint.h>     /* C99 types */
#include <stdio.h>      /* printf fprintf */
#include <stdlib.h>     /* malloc free */
#include <unistd.h>     /* lseek, close */
#include <fcntl.h>      /* open */
#include <string.h>     /* memset */

#include "tuya_cloud_types.h"
#include "tkl_spi.h"

#include "loragw_spi.h"
#include "loragw_aux.h"

/* -------------------------------------------------------------------------- */
/* --- PRIVATE MACROS ------------------------------------------------------- */

#define ARRAY_SIZE(a) (sizeof(a) / sizeof((a)[0]))
#if DEBUG_COM == 1
    #define DEBUG_MSG(str)                fprintf(stdout, str)
    #define DEBUG_PRINTF(fmt, args...)    fprintf(stdout,"%s:%d: "fmt, __FUNCTION__, __LINE__, args)
    #define CHECK_NULL(a)                if(a==NULL){fprintf(stderr,"%s:%d: ERROR: NULL POINTER AS ARGUMENT\n", __FUNCTION__, __LINE__);return LGW_SPI_ERROR;}
#else
    #define DEBUG_MSG(str)
    #define DEBUG_PRINTF(fmt, args...)
    #define CHECK_NULL(a)                if(a==NULL){return LGW_SPI_ERROR;}
#endif

/* -------------------------------------------------------------------------- */
/* --- PRIVATE CONSTANTS ---------------------------------------------------- */

#define READ_ACCESS     0x00
#define WRITE_ACCESS    0x80

#define LGW_BURST_CHUNK     1024

/* -------------------------------------------------------------------------- */
/* --- PUBLIC FUNCTIONS DEFINITION ------------------------------------------ */

/* SPI initialization and configuration */
int lgw_spi_open(const char * com_path, void **com_target_ptr) {
    TUYA_SPI_NUM_E *spi_port = NULL;
    OPERATE_RET ret;

    /* check input variables */
    CHECK_NULL(com_path);
    CHECK_NULL(com_target_ptr);

    /* allocate memory for the device descriptor */
    spi_port = malloc(sizeof(TUYA_SPI_NUM_E));
    if (spi_port == NULL) {
        DEBUG_MSG("ERROR: MALLOC FAIL\n");
        return LGW_SPI_ERROR;
    }

    /* Initialize SPI using Tuya API */
    *spi_port = TUYA_SPI_NUM_1; // Use SPI port 1 as default
    
    TUYA_SPI_BASE_CFG_T spi_cfg = {
        .mode = TUYA_SPI_MODE0,
        .freq_hz = SPI_SPEED,
        .databits = TUYA_SPI_DATA_BIT8,
        .bitorder = TUYA_SPI_ORDER_MSB2LSB,
        .role = TUYA_SPI_ROLE_MASTER,
        .type = TUYA_SPI_AUTO_TYPE,
        .spi_dma_flags = 1
    };

    ret = tkl_spi_init(*spi_port, &spi_cfg);
    if (ret != OPRT_OK) {
        DEBUG_MSG("ERROR: SPI PORT INITIALIZATION FAILED\n");
        free(spi_port);
        return LGW_SPI_ERROR;
    }

    *com_target_ptr = (void *)spi_port;
    DEBUG_MSG("Note: SPI port opened and configured ok\n");
    return LGW_SPI_SUCCESS;
}

/* ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */

/* SPI release */
int lgw_spi_close(void *com_target) {
    TUYA_SPI_NUM_E spi_port;
    OPERATE_RET ret;

    /* check input variables */
    CHECK_NULL(com_target);

    /* get SPI port and deinitialize */
    spi_port = *(TUYA_SPI_NUM_E *)com_target;
    ret = tkl_spi_deinit(spi_port);
    free(com_target);

    /* determine return code */
    if (ret != OPRT_OK) {
        DEBUG_MSG("ERROR: SPI PORT FAILED TO CLOSE\n");
        return LGW_SPI_ERROR;
    } else {
        DEBUG_MSG("Note: SPI port closed\n");
        return LGW_SPI_SUCCESS;
    }
}

/* ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */

/* Simple write */
int lgw_spi_w(void *com_target, uint8_t spi_mux_target, uint16_t address, uint8_t data) {
    TUYA_SPI_NUM_E spi_port;
    uint8_t out_buf[4];
    uint8_t command_size;
    OPERATE_RET ret;

    /* check input variables */
    CHECK_NULL(com_target);

    spi_port = *(TUYA_SPI_NUM_E *)com_target;

    /* prepare frame to be sent */
    out_buf[0] = spi_mux_target;
    out_buf[1] = WRITE_ACCESS | ((address >> 8) & 0x7F);
    out_buf[2] =                ((address >> 0) & 0xFF);
    out_buf[3] = data;
    command_size = 4;

    /* I/O transaction using Tuya SPI API */
    ret = tkl_spi_send(spi_port, out_buf, command_size);

    /* determine return code */
    if (ret != OPRT_OK) {
        DEBUG_MSG("ERROR: SPI WRITE FAILURE\n");
        return LGW_SPI_ERROR;
    } else {
        DEBUG_MSG("Note: SPI write success\n");
        return LGW_SPI_SUCCESS;
    }
}

/* ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */

/* Simple read */
int lgw_spi_r(void *com_target, uint8_t spi_mux_target, uint16_t address, uint8_t *data) {
    TUYA_SPI_NUM_E spi_port;
    uint8_t out_buf[5];
    uint8_t command_size;
    uint8_t in_buf[5];
    OPERATE_RET ret;

    /* check input variables */
    CHECK_NULL(com_target);
    CHECK_NULL(data);

    spi_port = *(TUYA_SPI_NUM_E *)com_target;

    /* prepare frame to be sent */
    out_buf[0] = spi_mux_target;
    out_buf[1] = READ_ACCESS | ((address >> 8) & 0x7F);
    out_buf[2] =               ((address >> 0) & 0xFF);
    out_buf[3] = 0x00;
    out_buf[4] = 0x00;
    command_size = 5;

    /* I/O transaction using Tuya SPI API */
    ret = tkl_spi_transfer(spi_port, out_buf, in_buf, command_size);

    /* determine return code */
    if (ret != OPRT_OK) {
        DEBUG_MSG("ERROR: SPI READ FAILURE\n");
        return LGW_SPI_ERROR;
    } else {
        DEBUG_MSG("Note: SPI read success\n");
        *data = in_buf[command_size - 1];
        return LGW_SPI_SUCCESS;
    }
}

/* ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */

/* Single Byte Read-Modify-Write */
int lgw_spi_rmw(void *com_target, uint8_t spi_mux_target, uint16_t address, uint8_t offs, uint8_t leng, uint8_t data) {
    int spi_stat = LGW_SPI_SUCCESS;
    uint8_t buf[4] = "\x00\x00\x00\x00";

    /* Read */
    spi_stat += lgw_spi_r(com_target, spi_mux_target, address, &buf[0]);

    /* Modify */
    buf[1] = ((1 << leng) - 1) << offs; /* bit mask */
    buf[2] = ((uint8_t)data) << offs; /* new data offsetted */
    buf[3] = (~buf[1] & buf[0]) | (buf[1] & buf[2]); /* mixing old & new data */

    /* Write */
    spi_stat += lgw_spi_w(com_target, spi_mux_target, address, buf[3]);

    return spi_stat;
}

/* ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */

/* Burst (multiple-byte) write */
int lgw_spi_wb(void *com_target, uint8_t spi_mux_target, uint16_t address, const uint8_t *data, uint16_t size) {
    TUYA_SPI_NUM_E spi_port;
    uint8_t command[3];
    uint8_t command_size;
    int size_to_do, chunk_size, offset;
    int byte_transfered = 0;
    int i;
    OPERATE_RET ret;
    uint16_t max_chunk_size;

    /* check input parameters */
    CHECK_NULL(com_target);
    CHECK_NULL(data);
    if (size == 0) {
        DEBUG_MSG("ERROR: BURST OF NULL LENGTH\n");
        return LGW_SPI_ERROR;
    }

    spi_port = *(TUYA_SPI_NUM_E *)com_target;

    /* prepare command byte */
    command[0] = spi_mux_target;
    command[1] = WRITE_ACCESS | ((address >> 8) & 0x7F);
    command[2] =                ((address >> 0) & 0xFF);
    command_size = 3;
    size_to_do = size;

    /* Determine max chunk size based on tkl_spi implementation constraints */
    max_chunk_size = 4096 - command_size; /* Reserve space for command bytes */
    if (max_chunk_size > LGW_BURST_CHUNK) {
        max_chunk_size = LGW_BURST_CHUNK;
    }

    /* I/O transaction using Tuya SPI API */
    for (i=0; size_to_do > 0; ++i) {
        chunk_size = (size_to_do < max_chunk_size) ? size_to_do : max_chunk_size;
        offset = i * max_chunk_size;
        
        uint8_t tx_buf[command_size + chunk_size];
        memcpy(tx_buf, command, command_size);
        memcpy(tx_buf + command_size, data + offset, chunk_size);
        
        ret = tkl_spi_send(spi_port, tx_buf, command_size + chunk_size);
        if (ret != OPRT_OK) {
            DEBUG_MSG("ERROR: SPI BURST WRITE FAILURE\n");
            return LGW_SPI_ERROR;
        }
        
        byte_transfered += chunk_size;
        DEBUG_PRINTF("BURST WRITE: to trans %d # chunk %d # transferred %d \n", size_to_do, chunk_size, byte_transfered);
        size_to_do -= chunk_size; /* subtract the quantity of data already transferred */
    }

    /* determine return code */
    if (byte_transfered != size) {
        DEBUG_MSG("ERROR: SPI BURST WRITE FAILURE\n");
        return LGW_SPI_ERROR;
    } else {
        DEBUG_MSG("Note: SPI burst write success\n");
        return LGW_SPI_SUCCESS;
    }
}

/* ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */

/* Burst (multiple-byte) read */
int lgw_spi_rb(void *com_target, uint8_t spi_mux_target, uint16_t address, uint8_t *data, uint16_t size) {
    TUYA_SPI_NUM_E spi_port;
    uint8_t command[4];
    uint8_t command_size;
    int size_to_do, chunk_size, offset;
    int byte_transfered = 0;
    int i;
    OPERATE_RET ret;
    uint16_t max_chunk_size;

    /* check input parameters */
    CHECK_NULL(com_target);
    CHECK_NULL(data);
    if (size == 0) {
        DEBUG_MSG("ERROR: BURST OF NULL LENGTH\n");
        return LGW_SPI_ERROR;
    }

    spi_port = *(TUYA_SPI_NUM_E *)com_target;

    /* prepare command byte */
    command[0] = spi_mux_target;
    command[1] = READ_ACCESS | ((address >> 8) & 0x7F);
    command[2] =               ((address >> 0) & 0xFF);
    command[3] = 0x00;
    command_size = 4;
    size_to_do = size;

    /* Determine max chunk size based on tkl_spi implementation constraints */
    max_chunk_size = 4096 - command_size; /* Reserve space for command bytes */
    if (max_chunk_size > LGW_BURST_CHUNK) {
        max_chunk_size = LGW_BURST_CHUNK;
    }

    /* I/O transaction using Tuya SPI API */
    for (i=0; size_to_do > 0; ++i) {
        chunk_size = (size_to_do < max_chunk_size) ? size_to_do : max_chunk_size;
        offset = i * max_chunk_size;
        
        uint8_t tx_buf[command_size + chunk_size];
        uint8_t rx_buf[command_size + chunk_size];
        memcpy(tx_buf, command, command_size);
        memset(tx_buf + command_size, 0, chunk_size);
        
        ret = tkl_spi_transfer(spi_port, tx_buf, rx_buf, command_size + chunk_size);
        if (ret != OPRT_OK) {
            DEBUG_MSG("ERROR: SPI BURST READ FAILURE\n");
            return LGW_SPI_ERROR;
        }
        
        memcpy(data + offset, rx_buf + command_size, chunk_size);
        byte_transfered += chunk_size;
        DEBUG_PRINTF("BURST READ: to trans %d # chunk %d # transferred %d \n", size_to_do, chunk_size, byte_transfered);
        size_to_do -= chunk_size;  /* subtract the quantity of data already transferred */
    }

    /* determine return code */
    if (byte_transfered != size) {
        DEBUG_MSG("ERROR: SPI BURST READ FAILURE\n");
        return LGW_SPI_ERROR;
    } else {
        DEBUG_MSG("Note: SPI burst read success\n");
        return LGW_SPI_SUCCESS;
    }
}

/* ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */

uint16_t lgw_spi_chunk_size(void) {
    /* Return the effective chunk size considering tkl_spi constraints */
    uint16_t max_chunk_size = 4096 - 4; /* 4096 limit minus max command bytes (4 for read) */
    return (max_chunk_size < LGW_BURST_CHUNK) ? max_chunk_size : LGW_BURST_CHUNK;
}

/* --- EOF ------------------------------------------------------------------ */
